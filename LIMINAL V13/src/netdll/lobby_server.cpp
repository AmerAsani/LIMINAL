#include "netdll/lobby_server.hpp"

#include <juice/juice.h>
#include <rtc/rtc.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <thread>

#include "netdll/joincode.hpp"

namespace lim::netc {

namespace {

double nowSec() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

struct Bucket {
    double rate, burst, tokens, last = -1;
    Bucket(double r, double b) : rate(r), burst(b), tokens(b) {}
    bool take(double now) {
        if (last < 0) last = now;
        tokens = std::min(burst, tokens + (now - last) * rate);
        last = now;
        if (tokens < 1.0) return false;
        tokens -= 1.0;
        return true;
    }
};

std::string ipOf(const std::shared_ptr<rtc::WebSocket>& ws) {
    std::optional<std::string> a;
    try {
        a = ws->remoteAddress();
    } catch (...) {
    }
    if (!a) return "?";
    const size_t p = a->rfind(':');
    return p == std::string::npos ? *a : a->substr(0, p);
}
bool loopback(const std::string& ip) { return ip == "::1" || ip.rfind("127.", 0) == 0; }

}  // namespace

struct LobbyServer::Impl {
    LobbyConfig cfg;
    std::unique_ptr<rtc::WebSocketServer> server;
    juice_server_t* turn = nullptr;
    uint16_t turnPortV = 0, portV = 0;
    std::thread th;
    std::atomic<bool> stopFlag{false};
    // aus den Rueckrufen von libdatachannel
    std::mutex mx;
    std::condition_variable cv;
    bool wake = false;
    std::vector<std::shared_ptr<rtc::WebSocket>> pending;
    mutable std::mutex statsMx;
    Stats st;

    // nur im Lobby-Thread
    struct Client {
        uint64_t id = 0;
        std::shared_ptr<rtc::WebSocket> ws;
        std::string ip;
        enum Role { None, Host, Guest } role = None;
        std::string room;
        int guestId = 0;
        Bucket bucket{30.0, 60.0};
        int excess = 0, msgs = 0;
        double opened = 0, joined = 0;
        bool closing = false;
    };
    struct Room {
        std::string id;
        uint64_t host = 0;
        int max = 3;
        std::map<int, uint64_t> guests;  // Nummer -> Verbindung (nur solange in der Lobby)
        int nextGuest = 1;
        std::set<std::string> banned;    // IPs, nur im Speicher
        std::map<std::string, std::string> tokens;  // IP -> Kennung fuer den Host
        std::vector<IceEntry> ice;
        double created = 0;
        std::string hostIp;
    };
    struct IpInfo {
        int conns = 0, rooms = 0;
        std::deque<double> joins, fails;
        double blockedUntil = 0;
    };
    std::map<uint64_t, Client> clients;
    std::map<std::string, Room> rooms;
    std::map<std::string, IpInfo> ips;
    uint64_t nextId = 1;

    void notify() {
        {
            std::lock_guard<std::mutex> l(mx);
            wake = true;
        }
        cv.notify_one();
    }
    void log(const std::string& s) {
        if (cfg.verbose) std::fprintf(stderr, "[lobby] %s\n", s.c_str());
    }
    void count(uint64_t Stats::*f) {
        std::lock_guard<std::mutex> l(statsMx);
        ++(st.*f);
    }

    void send(Client& c, const LMsg& m) {
        const std::string s = build(m);
        if (s.empty() || c.closing) return;
        try {
            if (c.ws->isOpen()) c.ws->send(s);
        } catch (...) {
        }
    }
    void sendErr(Client& c, const char* e) {
        LMsg m;
        m.t = LType::Err;
        m.err = e;
        send(c, m);
    }
    void closeClient(Client& c) {
        if (c.closing) return;
        c.closing = true;
        try {
            c.ws->close();
        } catch (...) {
        }
    }
    void refuse(Client& c, const char* e) {
        sendErr(c, e);
        closeClient(c);
    }
    Client* clientById(uint64_t id) {
        auto it = clients.find(id);
        return it == clients.end() || it->second.closing ? nullptr : &it->second;
    }

    // Kandidaten (einzeln und in der Beschreibung) durch den Testhaken
    bool hookCand(std::string& c, bool fromHost) { return !cfg.candidateHook || cfg.candidateHook(c, fromHost); }
    void hookSdp(std::string& sdp, bool fromHost) {
        if (!cfg.candidateHook) return;
        std::string out;
        size_t i = 0;
        while (i < sdp.size()) {
            size_t e = sdp.find('\n', i);
            if (e == std::string::npos) e = sdp.size() - 1;
            std::string line = sdp.substr(i, e - i + 1);
            i = e + 1;
            if (line.rfind("a=candidate:", 0) == 0) {
                std::string body = line.substr(2);
                while (!body.empty() && (body.back() == '\n' || body.back() == '\r')) body.pop_back();
                if (!hookCand(body, fromHost)) continue;
                line = "a=" + body + "\r\n";
            }
            out += line;
        }
        sdp = out;
    }

    void closeRoom(const std::string& id) {
        auto it = rooms.find(id);
        if (it == rooms.end()) return;
        for (auto& [gid, cid] : it->second.guests)
            if (Client* g = clientById(cid)) refuse(*g, "closed");
        if (auto ip = ips.find(it->second.hostIp); ip != ips.end()) ip->second.rooms = std::max(0, ip->second.rooms - 1);
        rooms.erase(it);
        log("Raum geschlossen");
    }

    void onHost(Client& c, const LMsg& m) {
        if (c.role != Client::None) return refuse(c, "bad");
        if (m.v != kLobbyVersion) return refuse(c, "version");
        IpInfo& ip = ips[c.ip];
        if (cfg.singleRoom && (!loopback(c.ip) || !rooms.empty())) return refuse(c, "denied");
        if ((int)rooms.size() >= cfg.maxRooms || ip.rooms >= cfg.maxRoomsPerIp) return refuse(c, "busy");
        Room r;
        for (int tries = 0; tries < 20; ++tries) {
            if (!randomChars(kRoomChars, r.id)) return refuse(c, "busy");
            if (!rooms.count(r.id)) break;
            r.id.clear();
        }
        if (r.id.empty()) return refuse(c, "busy");
        r.host = c.id;
        r.max = std::clamp(m.max, 1, 7);
        r.created = nowSec();
        r.hostIp = c.ip;
        const int ttl = (int)(cfg.roomHours * 3600.0);
        if (turn) {
            std::string user, pass, tail;
            if (!randomChars(6, tail) || !randomChars(24, pass)) return refuse(c, "busy");
            user = "l" + r.id + tail;
            juice_server_credentials_t cr{user.c_str(), pass.c_str(), 4 * (r.max + 1)};
            if (juice_server_add_credentials(turn, &cr, (unsigned long)ttl * 1000ul) < 0) return refuse(c, "busy");
            const uint16_t tp = cfg.publicTurnPort ? cfg.publicTurnPort : turnPortV;
            r.ice.push_back({false, cfg.publicHost, tp, "", ""});
            r.ice.push_back({true, cfg.publicHost, tp, user, pass});
        }
        for (const IceEntry& e : cfg.extraIce)
            if ((int)r.ice.size() < kMaxIce) r.ice.push_back(e);
        c.role = Client::Host;
        c.room = r.id;
        ++ip.rooms;
        LMsg out;
        out.t = LType::Room;
        out.room = r.id;
        out.ice = r.ice;
        out.ttl = ttl;
        rooms[r.id] = std::move(r);
        send(c, out);
        log("Raum geoeffnet");
    }

    void onJoin(Client& c, const LMsg& m, double now) {
        if (c.role != Client::None) return refuse(c, "bad");
        if (m.v != kLobbyVersion) return refuse(c, "version");
        IpInfo& ip = ips[c.ip];
        while (!ip.joins.empty() && now - ip.joins.front() > 60.0) ip.joins.pop_front();
        while (!ip.fails.empty() && now - ip.fails.front() > 600.0) ip.fails.pop_front();
        if (ip.blockedUntil > now || (int)ip.joins.size() >= cfg.joinsPerMinute) {
            count(&Stats::rateLimited);
            return refuse(c, "rate");
        }
        ip.joins.push_back(now);
        auto it = rooms.find(m.room);
        if (it == rooms.end()) {
            ip.fails.push_back(now);
            if ((int)ip.fails.size() >= cfg.failsPerTenMinutes) ip.blockedUntil = now + 600.0;
            count(&Stats::refusedJoins);
            return refuse(c, "noroom");
        }
        Room& r = it->second;
        if (r.banned.count(c.ip)) {
            count(&Stats::refusedJoins);
            return refuse(c, "banned");
        }
        if ((int)r.guests.size() >= 4) {  // gleichzeitig in der Lobby; die Plaetze im Spiel zaehlt der Host
            count(&Stats::refusedJoins);
            return refuse(c, "busy");
        }
        Client* host = clientById(r.host);
        if (!host) return refuse(c, "closed");
        const int gid = r.nextGuest++;
        if (r.nextGuest > kMaxGuestId) r.nextGuest = 1;
        r.guests[gid] = c.id;
        std::string& tok = r.tokens[c.ip];
        if (tok.empty() && !randomChars(10, tok)) tok = "a" + std::to_string(r.tokens.size());
        c.role = Client::Guest;
        c.room = r.id;
        c.guestId = gid;
        c.joined = now;
        LMsg j;
        j.t = LType::Joined;
        j.id = gid;
        j.ice = r.ice;
        send(c, j);
        LMsg g;
        g.t = LType::Guest;
        g.id = gid;
        g.name = m.name;
        g.addr = tok;
        send(*host, g);
        log("Gast wartet");
    }

    void onSignal(Client& c, LMsg m) {
        auto it = rooms.find(c.room);
        if (it == rooms.end()) return refuse(c, "closed");
        Room& r = it->second;
        if (c.role == Client::Host) {
            auto g = r.guests.find(m.to);
            Client* gc = g == r.guests.end() ? nullptr : clientById(g->second);
            if (!gc) return;  // Gast schon fort: still verwerfen
            if (m.t == LType::Sig) hookSdp(m.sdp, true);
            else if (!hookCand(m.cand, true)) return;
            m.to = 0;
            send(*gc, m);
        } else if (c.role == Client::Guest) {
            Client* hc = clientById(r.host);
            if (!hc) return refuse(c, "closed");
            if (m.t == LType::Sig) hookSdp(m.sdp, false);
            else if (!hookCand(m.cand, false)) return;
            m.from = c.guestId;
            send(*hc, m);
        } else {
            refuse(c, "bad");
        }
    }

    void onHostCommand(Client& c, const LMsg& m) {
        if (c.role != Client::Host) return refuse(c, "bad");
        auto it = rooms.find(c.room);
        if (it == rooms.end()) return;
        Room& r = it->second;
        auto g = r.guests.find(m.id);
        Client* gc = g == r.guests.end() ? nullptr : clientById(g->second);
        if (m.t == LType::Ban) {
            // Adresse des Gasts sperren - auch wenn er die Lobby schon verlassen hat (ueber die Kennung)
            std::string ip = gc ? gc->ip : std::string();
            if (ip.empty())
                for (const auto& [addr, tok] : r.tokens)
                    if (tok == m.addr) ip = addr;
            if (!ip.empty()) r.banned.insert(ip);
            if (gc) refuse(*gc, "banned");
            log("Adresse im Raum gesperrt");
        } else if (gc) {
            refuse(*gc, m.err.c_str());
        }
    }

    void handle(Client& c, const std::string& text, double now) {
        count(&Stats::messages);
        ++c.msgs;
        if (!c.bucket.take(now)) {
            count(&Stats::rateLimited);
            if (++c.excess > 100) {
                count(&Stats::closedForAbuse);
                refuse(c, "rate");
            }
            return;
        }
        LMsg m;
        if (!parseFromClient(text, c.role == Client::Host, m) || (c.role == Client::None && c.msgs > 2) ||
            (c.role == Client::Guest && c.msgs > 400)) {
            count(&Stats::rejected);
            count(&Stats::closedForAbuse);
            return refuse(c, "bad");
        }
        switch (m.t) {
            case LType::Host: return onHost(c, m);
            case LType::Join: return onJoin(c, m, now);
            case LType::Sig:
            case LType::Cand: return onSignal(c, m);
            case LType::Refuse:
            case LType::Ban: return onHostCommand(c, m);
            case LType::Done: return closeClient(c);
            default: return refuse(c, "bad");
        }
    }

    void drop(uint64_t id) {
        auto it = clients.find(id);
        if (it == clients.end()) return;
        Client& c = it->second;
        if (c.role == Client::Host) {
            closeRoom(c.room);
        } else if (c.role == Client::Guest) {
            if (auto r = rooms.find(c.room); r != rooms.end()) {
                r->second.guests.erase(c.guestId);
                if (Client* h = clientById(r->second.host)) {
                    LMsg g;
                    g.t = LType::Gone;
                    g.id = c.guestId;
                    send(*h, g);
                }
            }
        }
        if (auto ip = ips.find(c.ip); ip != ips.end()) ip->second.conns = std::max(0, ip->second.conns - 1);
        try {
            c.ws->resetCallbacks();
            c.ws->close();
        } catch (...) {
        }
        clients.erase(it);
    }

    void accept(std::shared_ptr<rtc::WebSocket> ws, double now) {
        const std::string ip = ipOf(ws);
        IpInfo& info = ips[ip];
        if ((int)clients.size() >= cfg.maxConnections || info.conns >= cfg.maxPerIp || info.blockedUntil > now) {
            count(&Stats::refusedJoins);
            try {
                ws->close();
            } catch (...) {
            }
            return;
        }
        ++info.conns;
        Client c;
        c.id = nextId++;
        c.ws = ws;
        c.ip = ip;
        c.opened = now;
        ws->onAvailable([this] { notify(); });
        ws->onClosed([this] { notify(); });
        clients.emplace(c.id, std::move(c));
    }

    void run() {
        double lastSweep = 0;
        while (!stopFlag) {
            {
                std::unique_lock<std::mutex> l(mx);
                cv.wait_for(l, std::chrono::milliseconds(50), [this] { return wake || stopFlag.load(); });
                wake = false;
            }
            const double now = nowSec();
            std::vector<std::shared_ptr<rtc::WebSocket>> fresh;
            {
                std::lock_guard<std::mutex> l(mx);
                fresh.swap(pending);
            }
            for (auto& ws : fresh) accept(std::move(ws), now);
            std::vector<uint64_t> gone;
            for (auto& [id, c] : clients) {
                for (int k = 0; k < 32 && !c.closing; ++k) {
                    std::optional<rtc::message_variant> m;
                    try {
                        m = c.ws->receive();
                    } catch (...) {
                    }
                    if (!m) break;
                    if (!std::holds_alternative<std::string>(*m)) {
                        count(&Stats::rejected);
                        refuse(c, "bad");
                        break;
                    }
                    handle(c, std::get<std::string>(*m), now);
                }
                bool closed = false;
                try {
                    closed = c.ws->isClosed();
                } catch (...) {
                    closed = true;
                }
                if (closed) gone.push_back(id);
            }
            if (now - lastSweep > 1.0) {
                lastSweep = now;
                for (auto& [id, c] : clients) {
                    if (c.closing) {
                        if (now - c.opened > 5.0) gone.push_back(id);
                        continue;
                    }
                    const bool idle = c.role == Client::None && now - c.opened > 10.0;
                    const bool stale = c.role == Client::Guest && now - c.joined > 60.0;
                    if (idle || stale) refuse(c, "timeout");
                }
                std::vector<std::string> old;
                for (auto& [rid, r] : rooms)
                    if (now - r.created > cfg.roomHours * 3600.0) old.push_back(rid);
                for (const std::string& rid : old) {
                    if (Client* h = clientById(rooms[rid].host)) refuse(*h, "timeout");
                    closeRoom(rid);
                }
                for (auto it = ips.begin(); it != ips.end();) {  // Speicher: leere Eintraege vergessen
                    const IpInfo& i = it->second;
                    if (i.conns == 0 && i.rooms == 0 && i.blockedUntil < now && (i.joins.empty() || now - i.joins.back() > 60.0) &&
                        (i.fails.empty() || now - i.fails.back() > 600.0))
                        it = ips.erase(it);
                    else
                        ++it;
                }
            }
            std::sort(gone.begin(), gone.end());
            gone.erase(std::unique(gone.begin(), gone.end()), gone.end());
            for (uint64_t id : gone) drop(id);
            {
                std::lock_guard<std::mutex> l(statsMx);
                st.connections = (int)clients.size();
                st.rooms = (int)rooms.size();
                int g = 0;
                for (auto& [rid, r] : rooms) g += (int)r.guests.size();
                st.guests = g;
            }
        }
        // Ende: alle trennen
        std::vector<uint64_t> all;
        for (auto& [id, c] : clients) all.push_back(id);
        for (uint64_t id : all) drop(id);
    }
};

LobbyServer::LobbyServer() : d_(std::make_unique<Impl>()) {}
LobbyServer::~LobbyServer() { stop(); }

bool LobbyServer::start(const LobbyConfig& cfg, std::string& error) {
    if (d_->server) {
        error = "laeuft schon";
        return false;
    }
    d_->cfg = cfg;
    d_->stopFlag = false;
    if (cfg.turn) {
        juice_server_config_t sc{};
        sc.max_allocations = 1000;
        sc.max_peers = 16;
        sc.bind_address = cfg.bind.empty() ? nullptr : cfg.bind.c_str();
        sc.external_address = cfg.externalAddress.empty() ? nullptr : cfg.externalAddress.c_str();
        sc.port = cfg.turnPort;
        sc.relay_port_range_begin = cfg.relayPortBegin;
        sc.relay_port_range_end = cfg.relayPortEnd;
        sc.realm = "liminal";
        d_->turn = juice_server_create(&sc);
        if (!d_->turn) {
            error = "STUN/TURN-Server startet nicht (UDP-Port " + std::to_string(cfg.turnPort) + " belegt?)";
            return false;
        }
        d_->turnPortV = juice_server_get_port(d_->turn);
    }
    try {
        rtc::WebSocketServer::Configuration wc;
        wc.port = cfg.port;
        if (!cfg.bind.empty()) wc.bindAddress = cfg.bind;
        wc.maxMessageSize = kMaxLobbyMsg;
        wc.connectionTimeout = std::chrono::milliseconds(8000);
        if (!cfg.tlsCert.empty() || !cfg.tlsKey.empty()) {
            wc.enableTls = true;
            wc.certificatePemFile = cfg.tlsCert;
            wc.keyPemFile = cfg.tlsKey;
        }
        d_->server = std::make_unique<rtc::WebSocketServer>(wc);
        d_->portV = d_->server->port();
        Impl* d = d_.get();
        d_->server->onClient([d](std::shared_ptr<rtc::WebSocket> ws) {
            {
                std::lock_guard<std::mutex> l(d->mx);
                if (d->pending.size() < 256) d->pending.push_back(std::move(ws));
                else ws->close();
            }
            d->notify();
        });
    } catch (const std::exception& e) {
        error = std::string("Lobby startet nicht: ") + e.what();
        if (d_->turn) juice_server_destroy(d_->turn), d_->turn = nullptr;
        d_->server.reset();
        return false;
    }
    d_->th = std::thread([d = d_.get()] { d->run(); });
    return true;
}

void LobbyServer::stop() {
    if (!d_->server && !d_->turn) return;
    d_->stopFlag = true;
    d_->notify();
    if (d_->th.joinable()) d_->th.join();
    if (d_->server) {
        try {
            d_->server->stop();
        } catch (...) {
        }
        d_->server.reset();
    }
    {
        std::lock_guard<std::mutex> l(d_->mx);
        d_->pending.clear();
    }
    if (d_->turn) {
        juice_server_destroy(d_->turn);
        d_->turn = nullptr;
    }
    d_->clients.clear();
    d_->rooms.clear();
    d_->ips.clear();
}

uint16_t LobbyServer::port() const { return d_->portV; }
uint16_t LobbyServer::turnPort() const { return d_->turnPortV; }
LobbyServer::Stats LobbyServer::stats() const {
    std::lock_guard<std::mutex> l(d_->statsMx);
    return d_->st;
}

}  // namespace lim::netc
