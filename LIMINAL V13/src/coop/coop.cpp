#include "coop/coop.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>

#include "core/log.hpp"
#include "gameplay/content.hpp"
#include "world/map_def.hpp"

namespace lim::coop {

namespace nc = net::coop;
using game::GameEvent;
using game::GameSession;
using game::Pawn;
using game::Presence;

namespace {
constexpr double kSnapInterval = 0.05;     // 20 Momentaufnahmen je Sekunde
constexpr double kInputInterval = 1.0 / 60.0;
constexpr double kRespawnDelay = 20.0;     // Wiedereinstieg nach dem Tod (s)
constexpr double kGroupPull = 60.0;        // danach zieht der Host die Uebrigen mit
constexpr double kReconnectWindow = 120.0;
constexpr int kMaxInvalid = 8;
const char kB32[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

vec3 posOf(const game::Player& p) { return {(float)p.x, (float)p.y, (float)p.z}; }
vec3 mmVec(i32 x, i32 y, i32 z) { return {(float)nc::fromMm(x), (float)nc::fromMm(y), (float)nc::fromMm(z)}; }
u8 pct(double v) { return (u8)std::clamp((int)std::lround(v), 0, 100); }
u8 q01(float v) { return (u8)std::clamp((int)std::lround(v * 255.0f), 0, 255); }
u8 qs(float v) { return (u8)(i8)std::clamp((int)std::lround(v * 127.0f), -127, 127); }
float dq01(u8 v) { return v / 255.0f; }
float dqs(u8 v) { return (float)(i8)v / 127.0f; }
std::array<i8, 3> qv(vec3 v) { return {(i8)std::clamp((int)std::lround(v.x * 127), -127, 127), (i8)std::clamp((int)std::lround(v.y * 127), -127, 127), (i8)std::clamp((int)std::lround(v.z * 127), -127, 127)}; }
vec3 dqv(const std::array<i8, 3>& a) {
    vec3 v{a[0] / 127.0f, a[1] / 127.0f, a[2] / 127.0f};
    const float l = length(v);
    return l > 0.05f ? v / l : vec3{0, 0, 1};
}

// Steuerung aus einer Eingabe (Achsen, Tasten) - dieselbe Abbildung bei Host und Gast
game::PlayerControl controlOf(const nc::InputFrame& f) {
    game::PlayerControl c;
    c.forward = f.forward / 127.0f;
    c.side = f.side / 127.0f;
    c.sprint = (f.buttons & 1) != 0;
    c.jump = (f.buttons & 2) != 0;
    c.jumpHeld = (f.buttons & 4) != 0;
    c.sneak = (f.buttons & 8) != 0;
    return c;
}
}  // namespace

// --- Schluessel ------------------------------------------------------------------------------------
std::string JoinKey::text() const {
    std::string raw;
    u32 acc = 0;
    int bits = 0;
    for (u8 v : b) {
        acc = (acc << 8) | v, bits += 8;
        while (bits >= 5) raw += kB32[(acc >> (bits - 5)) & 31], bits -= 5;
    }
    if (bits > 0) raw += kB32[(acc << (5 - bits)) & 31];
    std::string out;
    for (size_t i = 0; i < raw.size(); ++i) {
        if (i && i % 5 == 0) out += '-';
        out += raw[i];
    }
    return out;
}

bool JoinKey::parse(const std::string& s, JoinKey& out) {
    std::vector<int> sym;
    for (char c : s) {
        if (c == '-' || c == ' ') continue;
        const char u = (char)std::toupper((unsigned char)c);
        const char* p = std::strchr(kB32, u);
        if (!p || !u) return false;
        sym.push_back((int)(p - kB32));
    }
    if (sym.size() != 26) return false;
    u32 acc = 0;
    int bits = 0;
    size_t k = 0;
    for (int v : sym) {
        acc = (acc << 5) | (u32)v, bits += 5;
        if (bits >= 8 && k < 16) out.b[k++] = (u8)(acc >> (bits - 8)), bits -= 8;
    }
    return k == 16 && (acc & ((1u << bits) - 1)) == 0;  // Rest muss null sein (kanonisch)
}

bool Bucket::take(double now) {
    if (last < 0) last = now;
    tokens = std::min(burst, tokens + (now - last) * rate);
    last = now;
    if (tokens < 1.0) return false;
    tokens -= 1.0;
    return true;
}

// =================================================================================================
// Host
// =================================================================================================
struct CoopHost::Guest {
    int peer = -1;
    int id = 0;  // 1..3, 0: noch nicht beigetreten
    std::string name;
    net::ConnState st = net::ConnState::Handshake;
    std::unique_ptr<Pawn> pawn;
    std::array<u8, 16> token{};
    Bucket inB{75.0, 20.0}, actB{10.0, 6.0}, pingB{2.0, 4.0}, joinB{1.0, 2.0};
    int invalid = 0;
    u32 lastAction = 0;
    bool anyAction = false;
    double snapAcc = 0.0, checkAcc = 0.0;
    u32 invHash = 0;
    double goneT = -1.0;  // getrennt seit (Wiederverbinden)
    bool heard = false;   // erste Eingabe da: der Gast spielt (erst dann Momentaufnahmen - sonst kaemen sie vor dem Willkommen)
    u32 eventSeq = 0;
    bool wasAlive = true;
    int spectate = 0;
    bool connected() const { return peer >= 0; }
};

CoopHost::CoopHost(NetApi api, std::string hostName, u32 buildHash) : api_(api), hostName_(std::move(hostName)), build_(buildHash) {
    if (!nc::nameOk(hostName_)) hostName_ = "Host";
}
CoopHost::~CoopHost() { stop(); }

bool CoopHost::start(u16 port, int maxPlayers, std::string& error) {
    if (!api_.ok()) {
        error = "Netz nicht verfuegbar";
        return false;
    }
    if (api_.random(key_.b.data(), 16) != 1) {
        error = "kein Zufall verfuegbar";
        return false;
    }
    maxPlayers_ = std::clamp(maxPlayers, 2, nc::kMaxPlayers);
    LnLimits lim{maxPlayers_ - 1, 8, 120.0, 60.0, 8, 5000, 10000, 5, 60000};
    ctx_ = api_.host(port, key_.b.data(), &lim);
    if (!ctx_) {
        error = std::format("Port {} ist belegt oder gesperrt", port);
        return false;
    }
    log::info("Koop: Host auf Port {} (hoechstens {} Spieler)", api_.port(ctx_), maxPlayers_);
    return true;
}

void CoopHost::stop() {
    if (!ctx_) return;
    nc::Msg m;
    m.type = nc::Type::Bye;
    m.bye.reason = 7;
    sendAll(m, true);
    for (auto& g : guests_)
        if (g->connected()) api_.drop(ctx_, g->peer, LN_R_NORMAL, 0);
    for (int i = 0; i < 20; ++i) {  // Abschied zustellen
        LnEvent ev;
        while (api_.poll(ctx_, &ev)) {}
    }
    api_.close(ctx_);
    ctx_ = nullptr;
    if (s_) s_->pawns.clear();
    guests_.clear();
}

u16 CoopHost::port() const { return ctx_ ? api_.port(ctx_) : 0; }

CoopHost::Guest* CoopHost::guestByPeer(int peer) {
    for (auto& g : guests_)
        if (g->peer == peer) return g.get();
    return nullptr;
}
CoopHost::Guest* CoopHost::guestById(int id) {
    for (auto& g : guests_)
        if (g->id == id) return g.get();
    return nullptr;
}
Pawn* CoopHost::pawnOf(int id) {
    Guest* g = guestById(id);
    return g ? g->pawn.get() : nullptr;
}

void CoopHost::rebuildPawnList() {
    if (!s_) return;
    s_->pawns.clear();
    for (auto& g : guests_)
        if (g->pawn && g->connected() && g->st == net::ConnState::Connected) s_->pawns.push_back(g->pawn.get());
}

void CoopHost::attach(GameSession& s, const std::string& anchor, int difficultyIndex) {
    s_ = &s;
    anchor_ = anchor;
    difficulty_ = difficultyIndex;
    s.netRole = GameSession::NetRole::Host;
    s.netSelfId = 0;
    group_ = Group{};
    selfReady_ = false;
    int k = 0;
    for (auto& g : guests_)
        if (g->pawn) {
            g->pawn->ready = false;
            placeAtAnchor(*g->pawn, ++k);
        }
    rebuildPawnList();
    // die Gaeste folgen in die Ebene
    const std::string text = s.levelId + "|" + anchor;
    eventAll(nc::EventKind::Level, (i32)(u32)s.seed, (i32)std::lround(s.supplyScale() * 100.0), {}, text);
    rosterDirty_ = true;
}

void CoopHost::placeAtAnchor(Pawn& p, int k) {
    // neben dem eigenen Spieler (am Anker oder Startpunkt der Ebene), etwas versetzt
    const game::Player& me = s_->player();
    const double a = me.angle + 2.4 * k;
    p.player = game::Player(me.x + std::cos(a) * 0.9, me.y + std::sin(a) * 0.9, me.z, me.angle);
    if (!p.alive) s_->pawnRespawn(p);
    else p.body.reset();
    p.anySeq = false;
}

void CoopHost::beforeUpdate(double dt) {
    clock_ += dt;
    if (!ctx_) return;
    // Zeitbudget der Eingaben waechst mit der Zeit des Hosts (gegen zu schnelle Uhren der Gaeste; 0,5 s Spielraum
    // fuer Schwankungen im Netz)
    for (auto& g : guests_)
        if (g->pawn) g->pawn->simBudget = std::min(0.5, g->pawn->simBudget + dt);
    LnEvent ev;
    for (int guard = 0; guard < 2000 && api_.poll(ctx_, &ev); ++guard) {
        if (ev.type == LN_EV_CONNECTED) onConnected(ev.peer, ev.name);
        else if (ev.type == LN_EV_DISCONNECTED) onDisconnected(ev.peer, ev.reason);
        else if (ev.type == LN_EV_MESSAGE) onMessage(ev.peer, ev.channel, ev.data, ev.len);
    }
    // Wiederverbinden verfallen lassen
    for (size_t i = 0; i < guests_.size();) {
        Guest& g = *guests_[i];
        if (!g.connected() && g.goneT >= 0.0 && clock_ - g.goneT > kReconnectWindow) {
            log::info("Koop: {} kommt nicht zurueck - Platz frei", g.name);
            remember(g);
            guests_.erase(guests_.begin() + (std::ptrdiff_t)i);
            rosterDirty_ = true;
            continue;
        }
        ++i;
    }
    rebuildPawnList();
}

void CoopHost::onConnected(int peer, const char* name) {
    if (guestByPeer(peer)) onDisconnected(peer, LN_R_TIMEOUT);  // Platz neu vergeben: der alte Eintrag ist getrennt
    auto g = std::make_unique<Guest>();
    g->peer = peer;
    g->name = nc::nameOk(name) ? name : "Gast";
    guests_.push_back(std::move(g));
    log::info("Koop: Verbindung von '{}' (Platz {}) - wartet auf Beitritt", name, peer);
}

void CoopHost::onDisconnected(int peer, int reason) {
    Guest* g = guestByPeer(peer);
    if (!g) return;
    g->peer = -1;
    if (g->id == 0) {  // nie beigetreten
        std::erase_if(guests_, [&](const auto& q) { return q.get() == g; });
        return;
    }
    g->goneT = clock_;
    g->st = net::ConnState::Handshake;
    if (g->pawn) g->pawn->connected = false;
    log::info("Koop: {} getrennt (Grund {})", g->name, reason);
    toastSelf(std::format("{} hat die Verbindung verloren.", g->name));
    eventAll(nc::EventKind::Left, g->id, reason, {}, "");
    rosterDirty_ = true;
}

void CoopHost::onMessage(int peer, int channel, const u8* data, int len) {
    (void)channel;
    Guest* g = guestByPeer(peer);
    if (!g) return;
    nc::Msg m;
    const nc::Reject r = nc::decode(net::Role::Guest, g->st, data, (size_t)len, m);
    if (r != nc::Reject::None) {
        ++counters.rejected;
        if (++g->invalid > kMaxInvalid) {
            log::warn("Koop: {} sendet ungueltige Nachrichten ({}) - getrennt", g->name, nc::rejectName(r));
            api_.drop(ctx_, peer, LN_R_PROTOCOL, 0);
            onDisconnected(peer, LN_R_PROTOCOL);  // ln_drop meldet selbst keine Trennung
            ++counters.kicked;
        }
        return;
    }
    switch (m.type) {
        case nc::Type::Join:
            if (g->joinB.take(clock_)) handleJoin(*g, m.join);
            break;
        case nc::Type::Input:
            if (g->inB.take(clock_)) handleInput(*g, m.input);
            else ++counters.dropped;
            break;
        case nc::Type::Action:
            if (g->actB.take(clock_)) handleAction(*g, m.action);
            else ++counters.dropped;
            break;
        case nc::Type::Ping:
            if (g->pingB.take(clock_)) {
                nc::Msg p;
                p.type = nc::Type::Pong;
                p.ping = m.ping;
                send(*g, p, false);
            }
            break;
        case nc::Type::Bye:
            api_.drop(ctx_, peer, LN_R_NORMAL, 0);
            onDisconnected(peer, LN_R_NORMAL);
            break;
        default: break;  // vom Gast nicht erlaubt - decode lehnt es ab
    }
}

void CoopHost::handleJoin(Guest& g, const nc::Join& j) {
    nc::Msg bye;
    bye.type = nc::Type::Bye;
    if (j.build != build_) {
        bye.bye.reason = 6;
        log::warn("Koop: {} hat andere Spieldaten (Pruefsumme {:08x} statt {:08x})", g.name, j.build, build_);
        send(g, bye, true);
        api_.drop(ctx_, g.peer, LN_R_VERSION, 0);
        onDisconnected(g.peer, LN_R_VERSION);  // g ist danach fort
        return;
    }
    // Wiederverbinden: derselbe Token, innerhalb von 120 s, Platz frei
    bool zero = true;
    for (u8 b : j.token) zero = zero && b == 0;
    if (!zero)
        for (auto& q : guests_)
            if (q.get() != &g && !q->connected() && q->id && q->token == j.token && q->goneT >= 0.0) {
                q->peer = g.peer;
                q->goneT = -1.0;
                q->heard = false;
                q->st = net::ConnState::Connected;
                q->pawn->connected = true;
                q->invHash = 0;
                q->name = g.name;
                Guest* back = q.get();
                std::erase_if(guests_, [&](const auto& x) { return x.get() == &g; });
                sendWelcome(*back, true);
                toastSelf(std::format("{} ist zurück.", back->name));
                eventAll(nc::EventKind::Joined, back->id, 1, {}, back->name, back->id);
                rosterDirty_ = true;
                log::info("Koop: {} wieder verbunden (Spieler {})", back->name, back->id);
                return;
            }
    // neu: kleinste freie Nummer
    int id = 0;
    for (int k = 1; k < maxPlayers_ && !id; ++k)
        if (!guestById(k)) id = k;
    if (!id || !s_) {
        bye.bye.reason = 3;
        send(g, bye, true);
        api_.drop(ctx_, g.peer, LN_R_FULL, 0);
        onDisconnected(g.peer, LN_R_FULL);
        return;
    }
    // gleicher Name wie ein anderer: Nummer anhaengen
    for (const auto& q : guests_)
        if (q.get() != &g && q->name == g.name && g.name.size() < 15) g.name += std::to_string(id);
    if (g.name == hostName_ && g.name.size() < 15) g.name += std::to_string(id);
    g.id = id;
    g.st = net::ConnState::Connected;
    api_.random(g.token.data(), 16);
    g.pawn = std::make_unique<Pawn>(id, g.name, s_->content().difficulty(s_->difficulty), &s_->content());
    g.pawn->flash = game::Flashlight{};
    if (auto it = memory_.find(g.name); it != memory_.end()) {  // aus dem Koop-Stand
        g.pawn->stats.fromJson(it->second["stats"]);
        g.pawn->stats.dead = false;
        g.pawn->stats.health = std::max(g.pawn->stats.health, 30.0);
        g.pawn->inventory.fromJson(it->second["inventory"]);
        g.pawn->flash.fromJson(it->second["flash"]);
        g.pawn->flash.on = false;
        log::info("Koop: {} bekommt Werte und Inventar aus dem Koop-Stand", g.name);
    }
    placeAtAnchor(*g.pawn, id);
    sendWelcome(g, false);
    toastSelf(std::format("{} ist beigetreten.", g.name));
    eventAll(nc::EventKind::Joined, id, 0, {}, g.name, id);
    rosterDirty_ = true;
    log::info("Koop: {} beigetreten als Spieler {}", g.name, id);
}

void CoopHost::sendWelcome(Guest& g, bool reconnected) {
    nc::Msg m;
    m.type = nc::Type::Welcome;
    nc::Welcome& w = m.welcome;
    w.playerId = (u8)g.id;
    w.token = g.token;
    w.runSeed = (i64)s_->runSeedOr();
    w.seed = (i64)s_->seed;
    w.difficulty = (u8)std::clamp(difficulty_, 0, 7);
    w.level = s_->levelId;
    w.anchor = nc::levelOk(anchor_) ? anchor_ : "";
    w.worldHash = build_;
    w.tick = tick_;
    w.maxPlayers = (u8)maxPlayers_;
    w.flags = reconnected ? 1 : 0;
    w.supply = (u16)std::clamp((int)std::lround(s_->supplyScale() * 100.0), 100, 300);
    send(g, m, true);
    g.invHash = 0;
}

void CoopHost::handleInput(Guest& g, const nc::Input& in) {
    Pawn* p = g.pawn.get();
    if (!p || !s_) return;
    g.heard = true;
    for (int i = 0; i < in.count; ++i) {
        const nc::InputFrame& f = in.f[(size_t)i];
        if (p->anySeq && (f.seq <= p->lastSeq || f.seq > p->lastSeq + 4000)) continue;  // alt oder unplausibel
        p->anySeq = true;
        p->lastSeq = f.seq;
        const double dt = f.dtMs / 1000.0;
        if (p->simBudget < dt * 0.5) {  // schneller als die echte Zeit: verwerfen (bestaetigt trotzdem die Nummer)
            ++counters.inputsRefused;
            continue;
        }
        p->simBudget -= dt;
        ++counters.inputs;
        p->player.angle = nc::fromMrad(f.yaw);
        p->player.pitch = std::clamp(nc::fromMrad(f.pitch), -game::Player::kPitchMax, game::Player::kPitchMax);
        const game::PlayerControl c = controlOf(f);
        p->control = c;
        s_->pawnInput(*p, c, dt);
    }
}

void CoopHost::handleAction(Guest& g, const nc::Action& a) {
    Pawn* p = g.pawn.get();
    if (!p || !s_) return;
    if (g.anyAction && a.seq <= g.lastAction) return;  // wiederholt
    g.anyAction = true;
    g.lastAction = a.seq;
    ++counters.actions;
    std::string msg, sound;
    bool ok = false;
    switch ((nc::ActionKind)a.kind) {
        case nc::ActionKind::Interact: {
            int note = -1;
            ok = s_->pawnInteract(*p, a.arg, msg, note);
            if (ok && note >= 0) sendEvent(g, nc::EventKind::Note, note, 0, {}, "");
            break;
        }
        case nc::ActionKind::UseSlot:
            ok = s_->pawnUse(*p, (int)a.arg, msg, sound);
            if (ok && nc::kindOk(sound)) sendEvent(g, nc::EventKind::Sound, 0, 70, p->eye(), sound);
            break;
        case nc::ActionKind::Drop: ok = s_->pawnDrop(*p, (int)a.arg, false); break;
        case nc::ActionKind::DropAll: ok = s_->pawnDrop(*p, (int)a.arg, true); break;
        case nc::ActionKind::Lamp:
            ok = p->alive;
            if (ok) p->flash.on = a.arg == 1 && p->flash.charge > 0.0 && s_->world().level().flashlight;
            break;
        case nc::ActionKind::Select:
            ok = true;
            p->inventory.selected = (int)a.arg;
            break;
        case nc::ActionKind::Spectate:
            ok = !p->alive;
            if (ok) g.spectate = (int)a.arg;
            break;
    }
    if (!ok) ++counters.actionsRefused;
    if (!msg.empty() && nc::textOk(msg)) sendEvent(g, nc::EventKind::Toast, 0, 26, {}, msg);
}

void CoopHost::send(Guest& g, const nc::Msg& m, bool reliable) {
    if (!ctx_ || !g.connected()) return;
    u8 buf[nc::kMaxMsg];
    const size_t n = nc::encode(m, buf, sizeof(buf));
    if (!n) {
        log::warn("Koop: Nachricht {} nicht kodierbar", (int)m.type);
        return;
    }
    api_.send(ctx_, g.peer, reliable ? LN_RELIABLE : LN_UNRELIABLE, buf, (int)n);
}

void CoopHost::sendAll(const nc::Msg& m, bool reliable, int except) {
    for (auto& g : guests_)
        if (g->connected() && g->st == net::ConnState::Connected && g->id != except) send(*g, m, reliable);
}

void CoopHost::sendEvent(Guest& g, nc::EventKind k, i32 a, i32 b, vec3 pos, const std::string& text) {
    nc::Msg m;
    m.type = nc::Type::Event;
    m.event.seq = ++g.eventSeq;
    m.event.kind = (u8)k;
    m.event.a = a, m.event.b = b;
    m.event.x = nc::toMm(pos.x), m.event.y = nc::toMm(pos.y), m.event.z = nc::toMm(pos.z);
    m.event.text = text;
    send(g, m, true);
}

void CoopHost::eventAll(nc::EventKind k, i32 a, i32 b, vec3 pos, const std::string& text, int except) {
    for (auto& g : guests_)
        if (g->connected() && g->st == net::ConnState::Connected && g->id != except) sendEvent(*g, k, a, b, pos, text);
}

void CoopHost::toastSelf(const std::string& text, float dur) {
    if (s_) s_->events().push_back({GameEvent::Toast, text, dur});
}

void CoopHost::afterUpdate(double dt) {
    if (!ctx_ || !s_) return;
    ++tick_;
    flushWorld();
    runDeath(dt);
    runGroup(dt);
    // Momentaufnahmen, Inventare, Pruefsummen
    for (auto& g : guests_) {
        if (!g->connected() || g->st != net::ConnState::Connected || !g->pawn || !g->heard) continue;
        g->snapAcc += dt;
        if (g->snapAcc >= kSnapInterval) {
            g->snapAcc = std::fmod(g->snapAcc, kSnapInterval);
            sendSnapshot(*g);
        }
        sendInventory(*g, false);
        g->checkAcc += dt;
        if (g->checkAcc >= 2.0) {
            g->checkAcc = 0.0;
            nc::Msg m;
            m.type = nc::Type::Check;
            m.check.tick = tick_;
            m.check.hash = s_->sharedHash();
            send(*g, m, true);
        }
    }
    rosterT_ += dt;
    if (rosterDirty_ || rosterT_ > 1.0) sendRoster();
}

void CoopHost::flushWorld() {
    for (const auto& w : s_->netWorldOut) {
        if (w.kind == GameSession::NetWorldEvent::Picked) eventAll(nc::EventKind::Picked, (i32)w.hash, w.by, w.pos, "");
        else if (nc::kindOk(w.itemKind)) eventAll(nc::EventKind::Dropped, (i32)w.hash, 1, w.pos, w.itemKind);
    }
    s_->netWorldOut.clear();
    for (const auto& [pos, dur] : s_->netBlackoutOut) eventAll(nc::EventKind::Blackout, 0, (i32)std::lround(dur * 10.0f), pos, "");
    s_->netBlackoutOut.clear();
    using PE = Presence::Event;
    for (const PE& e : s_->netFigureOut) {
        if (e.type == PE::Step) continue;  // Schritte erzeugen die Puppen beim Gast selbst
        const bool world = e.type == PE::Growl || e.type == PE::Bark || e.type == PE::Whine || e.type == PE::TakeOff || e.type == PE::Knock ||
                           e.type == PE::Drop || e.type == PE::Drag || e.type == PE::Breath || e.type == PE::Rustle;
        if (e.type == PE::Flicker) {
            eventAll(nc::EventKind::Flicker, 0, 0, e.pos, "");
            continue;
        }
        for (auto& g : guests_) {
            if (!g->connected() || g->st != net::ConnState::Connected) continue;
            if (g->id == e.target || (world && e.target != g->id)) sendEvent(*g, nc::EventKind::Figure, (i32)e.type, (i32)e.role, e.pos, "");
        }
    }
    s_->netFigureOut.clear();
    for (const auto& pm : s_->netPersonalOut)
        if (Guest* g = guestById(pm.to); g && nc::textOk(pm.text)) sendEvent(*g, nc::EventKind::Toast, 0, 30, {}, pm.text);
    s_->netPersonalOut.clear();
}

void CoopHost::sendSnapshot(Guest& g) {
    Pawn& p = *g.pawn;
    nc::Msg m;
    m.type = nc::Type::Snapshot;
    nc::Snapshot& sn = m.snapshot;
    sn.tick = tick_;
    nc::SelfNet& me = sn.self;
    me.ack = p.lastSeq;
    me.x = nc::toMm(p.player.x), me.y = nc::toMm(p.player.y), me.z = nc::toMm(p.player.z);
    me.vx = (i16)std::clamp(std::lround(p.player.vx * 100.0), -32000L, 32000L);
    me.vy = (i16)std::clamp(std::lround(p.player.vy * 100.0), -32000L, 32000L);
    me.vz = (i16)std::clamp(std::lround(p.player.vz * 100.0), -32000L, 32000L);
    me.yaw = nc::toMrad(p.player.angle);
    me.pitch = (i16)std::clamp(std::lround(p.player.pitch * 1000.0), -1450L, 1450L);
    me.health = pct(p.stats.health), me.sanity = pct(p.stats.sanity), me.stamina = pct(p.stats.stamina), me.battery = pct(p.flash.charge);
    me.flags = (u16)((p.flash.on && p.flash.charge > 0.0 ? 1 : 0) | (!p.alive ? 2 : 0) | (p.stats.exhausted ? 4 : 0) | (p.player.onGround ? 8 : 0) |
                     (!p.alive ? 16 : 0) | (p.flash.charge <= 0.0 ? 32 : 0));
    me.respawnIn = (u8)std::clamp((int)std::ceil(kRespawnDelay - p.deadT), 0, 255);
    me.spectate = (u8)std::clamp(g.spectate, 0, nc::kMaxPlayers - 1);
    me.boost = (u8)(p.stats.speedLeft > 0.0 ? std::clamp((int)std::lround(p.stats.speedBonus * 100.0), 0, 100) : 0);
    // die anderen Spieler: Host und Gaeste
    auto addPlayer = [&](int id, const game::Player& pl, bool lamp, bool alive, bool ready) {
        if (sn.nPlayers >= nc::kMaxPlayers - 1) return;
        nc::PlayerNet& o = sn.players[sn.nPlayers++];
        o.id = (u8)id;
        o.x = nc::toMm(pl.x), o.y = nc::toMm(pl.y), o.z = nc::toMm(pl.z);
        o.yaw = nc::toMrad(pl.angle);
        o.pitch = (i16)std::clamp(std::lround(pl.pitch * 1000.0), -1450L, 1450L);
        o.speed = (u8)std::clamp((int)std::lround(pl.groundSpeed * 100.0 / 4.0), 0, 255);
        o.flags = (u8)((lamp ? 1 : 0) | (!alive ? 2 : 0) | (pl.sprinting ? 4 : 0) | (pl.sneaking ? 8 : 0) | (pl.onGround ? 16 : 0) |
                       (pl.swimming ? 32 : 0) | (ready ? 64 : 0));
    };
    const game::FlashBeam fb = s_->flashBeam();
    addPlayer(0, s_->player(), fb.on, !s_->stats().dead, selfReady_);
    for (auto& q : guests_)
        if (q.get() != &g && q->pawn && q->connected() && q->st == net::ConnState::Connected)
            addPlayer(q->id, q->pawn->player, q->pawn->flash.on && q->pawn->flash.charge > 0.0, q->pawn->alive, q->pawn->ready);
    // die Wesen nahe diesem Gast
    std::vector<Presence::NetState> cs;
    s_->presence().netStates(cs, p.eye(), nc::kMaxCreatures);
    for (const Presence::NetState& c : cs) {
        nc::CreatureNet& o = sn.creatures[sn.nCreatures++];
        o.netId = c.netId;
        o.role = (u8)std::clamp(c.role, 0, 5);
        o.x = nc::toMm(c.pos.x), o.y = nc::toMm(c.pos.y), o.z = nc::toMm(c.pos.z);
        o.yaw = nc::toMrad(c.yaw);
        o.state = (u8)std::clamp(c.state, 0, 15);
        o.target = c.target >= 0 && c.target < nc::kMaxPlayers ? (u8)c.target : 255;
        o.flags = (u8)((c.look ? 1 : 0) | (c.lit ? 2 : 0) | (c.frozen ? 4 : 0));
        o.anim = {q01(c.jaw), q01(c.alert), q01(c.crouch), q01(c.growl), q01(c.reach), q01(c.fly), q01(c.shiver),
                  q01(c.hang), q01(c.bait), q01(c.light), q01(c.perceive), q01(c.glow), qs(c.tilt), qs(c.lean)};
        o.up = qv(c.up), o.fwd = qv(c.fwd);
    }
    ++counters.snapshots;
    send(g, m, false);
}

void CoopHost::sendInventory(Guest& g, bool force) {
    const game::Inventory& inv = g.pawn->inventory;
    u32 h = 2166136261u ^ (u32)inv.selected;
    for (int i = 0; i < game::Inventory::kSize; ++i)
        if (const auto& sl = inv.slots[(size_t)i]) {
            h = (h ^ (u32)i) * 16777619u ^ game::worldItemHash(sl->kind);
            h = (h ^ (u32)sl->count) * 16777619u;
        }
    if (!force && h == g.invHash) return;
    g.invHash = h;
    nc::Msg m;
    m.type = nc::Type::Inventory;
    m.inventory.selected = (u8)std::clamp(inv.selected, 0, nc::kInvSlots - 1);
    for (int i = 0; i < game::Inventory::kSize && i < nc::kInvSlots; ++i)
        if (const auto& sl = inv.slots[(size_t)i]; sl && nc::kindOk(sl->kind)) {
            nc::InvEntry& e = m.inventory.e[m.inventory.n++];
            e.slot = (u8)i;
            e.kind = sl->kind;
            e.count = (u8)std::clamp(sl->count, 1, 99);
        }
    send(g, m, true);
}

void CoopHost::sendRoster() {
    rosterDirty_ = false;
    rosterT_ = 0.0;
    nc::Msg m;
    m.type = nc::Type::Roster;
    auto add = [&](int id, const std::string& name, bool alive, bool ready, bool conn) {
        if (m.roster.n >= nc::kMaxPlayers) return;
        nc::RosterEntry& e = m.roster.e[m.roster.n++];
        e.id = (u8)id;
        e.flags = (u8)((alive ? 1 : 0) | (ready ? 2 : 0) | (conn ? 4 : 0));
        e.name = nc::nameOk(name) ? name : "Spieler";
    };
    add(0, hostName_, s_ && !s_->stats().dead, selfReady_, true);
    for (auto& g : guests_)
        if (g->id && g->pawn) add(g->id, g->name, g->pawn->alive, g->pawn->ready, g->connected());
    sendAll(m, true);
}

// --- Gruppe: Ausgang -------------------------------------------------------------------------------
void CoopHost::runGroup(double dt) {
    struct Who {
        const game::Player* pl;
        game::Inventory* inv;
        bool* ready;
        std::string name;
    };
    std::vector<Who> alive;
    if (!s_->stats().dead) alive.push_back({&s_->player(), &s_->inventory(), &selfReady_, hostName_});
    else selfReady_ = false;
    for (Pawn* p : s_->pawns) {
        if (p->alive) alive.push_back({&p->player, &p->inventory, &p->ready, p->name});
        else p->ready = false;
    }
    int readyN = 0;
    bool needsHave = false;
    for (Who& w : alive) {
        bool needsOk = true;
        const world::MapTransition* t = s_->transitionAt(*w.pl, *w.inv, needsOk);
        if (t && t->kind == "end") {  // das Ende der Reise: fuer alle
            pendingEscape_ = true;
            eventAll(nc::EventKind::Level, 0, 0, {}, "end|");
            return;
        }
        bool r = t && !t->target.empty();
        if (r && !group_.active) {
            group_ = Group{};
            group_.active = true;
            group_.level = t->target, group_.anchor = t->id, group_.needs = t->needs;
            toastSelf(std::format("{} hat einen Ausgang gefunden.", w.name), 3.0f);
            eventAll(nc::EventKind::Toast, 0, 30, {}, std::format("{} hat einen Ausgang gefunden.", w.name));
        }
        r = r && t->target == group_.level;
        if (r != *w.ready) rosterDirty_ = true;
        *w.ready = r;
        if (r) {
            ++readyN;
            needsHave = needsHave || needsOk;
        }
    }
    if (!group_.active) return;
    group_.t += dt;
    group_.emptyT = readyN == 0 ? group_.emptyT + dt : 0.0;
    if (group_.emptyT > 6.0) {  // alle haben den Ausgang wieder verlassen
        group_ = Group{};
        rosterDirty_ = true;
        return;
    }
    const int aliveN = (int)alive.size();
    const double left = std::max(0.0, kGroupPull - group_.t);
    group_.announceT -= dt;
    if (group_.announceT <= 0.0) {
        group_.announceT = 1.0;
        const std::string st = std::format("Ausgang gefunden – {}/{} bereit", readyN, aliveN);
        eventAll(nc::EventKind::Ready, readyN, aliveN, vec3{0, 0, (float)(left <= 15.0 ? left / 1000.0 : -0.001)}, "");
        (void)st;
    }
    const bool go = aliveN > 0 && readyN > 0 && (readyN >= aliveN || left <= 0.0);
    if (!go) return;
    if (!group_.needs.empty()) {  // der Aufzug faehrt erst mit der Sicherung - einer der Bereiten muss sie haben
        if (!needsHave) {
            group_.needsT -= dt;
            if (group_.needsT <= 0.0) {
                group_.needsT = 4.0;
                const game::ItemDef* d = s_->content().item(group_.needs);
                const std::string t = std::format("Kein Strom. Es fehlt: {}.", d ? d->name : group_.needs);
                toastSelf(t);
                eventAll(nc::EventKind::Toast, 0, 30, {}, t);
            }
            return;
        }
        for (Who& w : alive)
            if (*w.ready && w.inv->count(group_.needs) > 0) {
                for (int i = 0; i < game::Inventory::kSize; ++i)
                    if (w.inv->slots[(size_t)i] && w.inv->slots[(size_t)i]->kind == group_.needs) {
                        w.inv->take(i);
                        break;
                    }
                break;
            }
    }
    pendingLevel_ = group_.level, pendingAnchor_ = group_.anchor;
    pendingChange_ = true;
    log::info("Koop: die Gruppe wechselt nach {} ({}/{} bereit nach {:.0f} s)", group_.level, readyN, aliveN, group_.t);
    group_ = Group{};
}

bool CoopHost::takeLevelChange(std::string& level, std::string& anchor) {
    if (!pendingChange_) return false;
    pendingChange_ = false;
    level = pendingLevel_, anchor = pendingAnchor_;
    return true;
}
bool CoopHost::takeEscape() {
    const bool e = pendingEscape_;
    pendingEscape_ = false;
    return e;
}

// --- Tod, Zuschauen, Wiedereinstieg ------------------------------------------------------------------
void CoopHost::runDeath(double dt) {
    bool anyAlive = !s_->stats().dead;
    for (Pawn* p : s_->pawns) anyAlive = anyAlive || p->alive;
    // Tote Gaeste melden; nach 20 s zurueck, solange jemand lebt
    for (auto& g : guests_) {
        Pawn* p = g->pawn.get();
        if (!p || !g->connected()) continue;
        if (g->wasAlive && !p->alive) {
            eventAll(nc::EventKind::Died, g->id, p->killRole, posOf(p->player), "");
            toastSelf(std::format("{} ist tot.", g->name));
            rosterDirty_ = true;
            g->spectate = 0;
        }
        g->wasAlive = p->alive;
        if (!p->alive && p->deadT >= kRespawnDelay && anyAlive) {
            s_->pawnRespawn(*p);
            g->wasAlive = true;
            eventAll(nc::EventKind::Respawn, g->id, nc::toMrad(p->player.angle), posOf(p->player), "");
            rosterDirty_ = true;
        }
    }
    // der Host selbst
    if (s_->stats().dead) {
        if (selfDeadT_ < 0.0) {
            selfDeadT_ = 0.0;
            eventAll(nc::EventKind::Died, 0, s_->killRole(), posOf(s_->player()), "");
            rosterDirty_ = true;
            spectate_ = -1;
        }
        selfDeadT_ += dt;
        if (spectate_ < 0 || !pawnOf(spectate_) || !pawnOf(spectate_)->alive) {
            spectate_ = -1;
            for (Pawn* p : s_->pawns)
                if (p->alive && spectate_ < 0) spectate_ = p->id;
        }
        if (selfDeadT_ >= kRespawnDelay && anyAlive) {
            s_->respawnCoop();
            selfDeadT_ = -1.0;
            spectate_ = -1;
            eventAll(nc::EventKind::Respawn, 0, nc::toMrad(s_->player().angle), posOf(s_->player()), "");
            rosterDirty_ = true;
        }
    } else {
        selfDeadT_ = -1.0;
    }
    // alle tot: die Gruppe beginnt am Sicherungspunkt neu
    if (!anyAlive && (s_->stats().dead || !s_->pawns.empty())) {
        if (allDeadT_ < 0.0) allDeadT_ = 0.0;
        allDeadT_ += dt;
        if (allDeadT_ > 6.0) {
            allDeadT_ = -1.0;
            s_->respawnCoop();
            selfDeadT_ = -1.0;
            eventAll(nc::EventKind::Respawn, 0, nc::toMrad(s_->player().angle), posOf(s_->player()), "");
            int k = 0;
            for (auto& g : guests_)
                if (g->pawn) {
                    s_->pawnRespawn(*g->pawn);
                    placeAtAnchor(*g->pawn, ++k);
                    g->wasAlive = true;
                    eventAll(nc::EventKind::Respawn, g->id, nc::toMrad(g->pawn->player.angle), posOf(g->pawn->player), "");
                }
            toastSelf("Alle sind tot. Die Gruppe beginnt am letzten Sicherungspunkt neu.", 4.0f);
            eventAll(nc::EventKind::Toast, 0, 40, {}, "Alle sind tot. Die Gruppe beginnt am letzten Sicherungspunkt neu.");
            rosterDirty_ = true;
        }
    } else {
        allDeadT_ = -1.0;
    }
}

bool CoopHost::selfDead() const { return s_ && s_->stats().dead; }
double CoopHost::selfRespawnIn() const { return selfDeadT_ < 0.0 ? 0.0 : std::max(0.0, kRespawnDelay - selfDeadT_); }

void CoopHost::cycleSpectate() {
    if (!s_) return;
    std::vector<int> ids;
    for (Pawn* p : s_->pawns)
        if (p->alive) ids.push_back(p->id);
    if (ids.empty()) return;
    auto it = std::find(ids.begin(), ids.end(), spectate_);
    spectate_ = it == ids.end() || it + 1 == ids.end() ? ids.front() : *(it + 1);
}

std::vector<PlayerInfo> CoopHost::players() const {
    std::vector<PlayerInfo> out;
    out.push_back({0, hostName_, s_ && !s_->stats().dead, selfReady_, true, true, 0});
    for (const auto& g : guests_) {
        if (!g->id || !g->pawn) continue;
        PlayerInfo i{g->id, g->name, g->pawn->alive, g->pawn->ready, g->connected(), false, 0};
        LnStats st{};
        if (g->connected() && api_.stats(ctx_, g->peer, &st)) i.rttMs = (int)st.rttMs;
        out.push_back(i);
    }
    return out;
}

void CoopHost::remember(const Guest& g) {
    if (!g.pawn || !g.id) return;
    Json j = Json::object();
    j.set("stats", g.pawn->stats.toJson());
    j.set("inventory", g.pawn->inventory.toJson());
    j.set("flash", g.pawn->flash.toJson());
    memory_[g.name] = j;
}

Json CoopHost::saveGuests() {
    for (const auto& g : guests_) remember(*g);
    Json out = Json::object();
    for (const auto& [name, j] : memory_) out.set(name, j);
    return out;
}

void CoopHost::loadGuests(const Json& j) {
    memory_.clear();
    if (!j.isObject()) return;
    for (const auto& [name, v] : j.members())
        if (nc::nameOk(name) && v.isObject()) memory_[name] = v;
}

bool CoopHost::kick(int id, bool ban) {
    Guest* g = guestById(id);
    if (!g || !g->connected()) return false;
    nc::Msg m;
    m.type = nc::Type::Bye;
    m.bye.reason = ban ? 5 : 4;
    send(*g, m, true);
    api_.drop(ctx_, g->peer, LN_R_KICKED, ban ? 1 : 0);
    log::info("Koop: {} {}", g->name, ban ? "gesperrt" : "entfernt");
    toastSelf(std::format("{} wurde {}.", g->name, ban ? "gesperrt" : "entfernt"));
    const int gid = g->id;
    remember(*g);
    std::erase_if(guests_, [&](const auto& q) { return q.get() == g; });
    eventAll(nc::EventKind::Left, gid, ban ? 5 : 4, {}, "");
    rosterDirty_ = true;
    ++counters.kicked;
    rebuildPawnList();
    return true;
}

std::string CoopHost::status() const {
    if (selfDead()) return std::format("Tot – Wiedereinstieg in {:.0f} s", selfRespawnIn());
    if (group_.active) {
        int ready = selfReady_ ? 1 : 0, alive = s_ && !s_->stats().dead ? 1 : 0;
        if (s_)
            for (Pawn* p : s_->pawns) ready += p->ready ? 1 : 0, alive += p->alive ? 1 : 0;
        const double left = std::max(0.0, kGroupPull - group_.t);
        return left <= 15.0 ? std::format("Ausgang gefunden – {}/{} bereit · die Gruppe geht in {:.0f} s", ready, alive, left)
                            : std::format("Ausgang gefunden – {}/{} bereit", ready, alive);
    }
    return "";
}

// =================================================================================================
// Gast
// =================================================================================================
CoopClient::CoopClient(NetApi api, u32 buildHash) : api_(api), build_(buildHash) {}
CoopClient::~CoopClient() { stop(); }

bool CoopClient::connect(const std::string& host, u16 port, const JoinKey& key, const std::string& name, std::string& error) {
    if (!api_.ok()) {
        error = "Netz nicht verfuegbar";
        return false;
    }
    if (!nc::nameOk(name)) {
        error = "Name: 1-16 Zeichen (Buchstaben, Ziffern, Leerzeichen, _ -)";
        return false;
    }
    name_ = name;
    ctx_ = api_.connect(host.c_str(), port, key.b.data(), name.c_str());
    if (!ctx_) {
        error = "Adresse nicht aufloesbar";
        return false;
    }
    state_ = State::Connecting;
    reason_.clear();
    return true;
}

void CoopClient::stop() {
    if (!ctx_) return;
    if (state_ != State::Closed) {
        nc::Msg m;
        m.type = nc::Type::Bye;
        m.bye.reason = 0;
        sendMsg(m, true);
        for (int i = 0; i < 10; ++i) {
            LnEvent ev;
            while (api_.poll(ctx_, &ev)) {}
        }
    }
    api_.close(ctx_);
    ctx_ = nullptr;
    state_ = State::Closed;
    if (s_) {
        s_->pawns.clear();
        s_->netAction = nullptr;
    }
}

void CoopClient::sendMsg(const nc::Msg& m, bool reliable) {
    if (!ctx_) return;
    u8 buf[nc::kMaxMsg];
    const size_t n = nc::encode(m, buf, sizeof(buf));
    if (n) api_.send(ctx_, 0, reliable ? LN_RELIABLE : LN_UNRELIABLE, buf, (int)n);
}

void CoopClient::pump() {
    if (!ctx_) return;
    LnEvent ev;
    for (int guard = 0; guard < 2000 && api_.poll(ctx_, &ev); ++guard) {
        if (ev.type == LN_EV_CONNECTED) {
            state_ = State::Joining;
            nc::Msg m;
            m.type = nc::Type::Join;
            m.join.build = build_;
            m.join.token = token_;  // beim Wiederverbinden der alte
            sendMsg(m, true);
        } else if (ev.type == LN_EV_DISCONNECTED) {
            static const char* why[] = {"", "getrennt", "Zeitlimit", "falscher Schluessel", "Host ist voll", "andere Version", "Protokollfehler",
                                        "gesperrt", "entfernt", "zu viele Nachrichten", "Host nicht erreichbar"};
            if (reason_.empty()) reason_ = ev.reason >= 0 && ev.reason <= 10 ? why[ev.reason] : "getrennt";
            state_ = State::Closed;
        } else if (ev.type == LN_EV_MESSAGE) {
            onMessage(ev.channel, ev.data, ev.len);
        }
    }
}

void CoopClient::onMessage(int channel, const u8* data, int len) {
    (void)channel;
    nc::Msg m;
    const net::ConnState st = state_ == State::Welcomed || state_ == State::Playing ? net::ConnState::Connected : net::ConnState::Handshake;
    const nc::Reject r = nc::decode(net::Role::Host, st, data, (size_t)len, m);
    if (r != nc::Reject::None) {
        ++counters.rejected;
        log::warn("Koop: ungueltige Nachricht vom Host ({})", nc::rejectName(r));
        return;
    }
    switch (m.type) {
        case nc::Type::Welcome:
            welcome_ = m.welcome;
            if (welcome_.worldHash != build_) {
                reason_ = "andere Spieldaten als der Host";
                state_ = State::Closed;
                return;
            }
            selfId_ = welcome_.playerId;
            token_ = welcome_.token;
            welcomeNew_ = true;
            state_ = State::Welcomed;
            log::info("Koop: willkommen als Spieler {} - Ebene {}, Seed {}", selfId_, welcome_.level, welcome_.seed);
            break;
        case nc::Type::Snapshot:
            if (s_) applySnapshot(m.snapshot);
            break;
        case nc::Type::Event:
            applyEvent(m.event);
            break;
        case nc::Type::Inventory:
            if (s_) applyInventory(m.inventory);
            break;
        case nc::Type::Roster: {
            roster_.clear();
            for (int i = 0; i < m.roster.n; ++i) {
                const nc::RosterEntry& e = m.roster.e[(size_t)i];
                roster_.push_back({e.id, e.name, (e.flags & 1) != 0, (e.flags & 2) != 0, (e.flags & 4) != 0, e.id == selfId_, 0});
            }
            break;
        }
        case nc::Type::Check:
            ++counters.checks;
            if (s_ && state_ == State::Playing && m.check.hash != s_->sharedHash()) {
                ++counters.desync;
                log::warn("Koop: geteilter Zustand weicht ab (Host {:08x}, hier {:08x})", m.check.hash, s_->sharedHash());
            }
            break;
        case nc::Type::Bye: {
            static const char* why[] = {"Host hat beendet", "Zeitlimit", "Protokollfehler", "Host ist voll", "vom Host entfernt", "vom Host gesperrt",
                                        "andere Version", "Host hat das Spiel beendet"};
            reason_ = why[std::min<int>(m.bye.reason, 7)];
            state_ = State::Closed;
            break;
        }
        default: break;
    }
}

bool CoopClient::takeWelcome(nc::Welcome& w) {
    if (!welcomeNew_) return false;
    welcomeNew_ = false;
    w = welcome_;
    return true;
}

void CoopClient::attach(GameSession& s) {
    s_ = &s;
    s.netRole = GameSession::NetRole::Guest;
    s.netSelfId = selfId_;
    s.netAction = [this](int kind, u32 arg) { action(kind, arg); };
    s.player().walkSpeed = game::Player().walkSpeed;  // wie die Figur beim Host (keine eigene Laufgeschwindigkeit)
    history_.clear();
    anySnap_ = false;
    state_ = State::Playing;
    s.pawns.clear();
    for (auto& o : others_) s.pawns.push_back(o.get());
}

void CoopClient::action(int kind, u32 arg) {
    nc::Msg m;
    m.type = nc::Type::Action;
    m.action.seq = ++actionSeq_;
    m.action.kind = (u8)kind;
    m.action.arg = arg;
    if (kind == (int)nc::ActionKind::Lamp) lampToggleT_ = clock_;
    sendMsg(m, true);
}

void CoopClient::beforeUpdate(double dt) {
    clock_ += dt;
    pump();
    if (!s_) return;
    // Mitspieler: 100 ms zurueck zwischen zwei Proben
    for (auto& o : others_) o->interpolate(clock_ - 0.1, dt);
    pingT_ -= dt;
    if (pingT_ <= 0.0 && ctx_) {
        pingT_ = 1.0;
        nc::Msg m;
        m.type = nc::Type::Ping;
        m.ping.nonce = (u32)(clock_ * 1000.0);
        m.ping.time = m.ping.nonce;
        sendMsg(m, false);
    }
}

void CoopClient::afterUpdate(double dt, const game::PlayerControl& c) {
    if (!s_ || state_ != State::Playing) return;
    // Eingaben sammeln und mit hoechstens 60 je Sekunde senden (die letzten vier, gegen Verlust)
    accDt_ += dt;
    accC_ = c;
    accJump_ = accJump_ || c.jump;
    if (accDt_ < kInputInterval && accDt_ < 0.1) return;
    const game::Player& pl = s_->player();
    nc::InputFrame f;
    f.seq = ++seq_;
    f.dtMs = (u8)std::clamp((int)std::lround(accDt_ * 1000.0), 1, 100);
    f.forward = (i8)std::clamp((int)std::lround(accC_.forward * 127.0f), -127, 127);
    f.side = (i8)std::clamp((int)std::lround(accC_.side * 127.0f), -127, 127);
    f.yaw = nc::toMrad(pl.angle);
    f.pitch = (i16)std::clamp(std::lround(pl.pitch * 1000.0), -1450L, 1450L);
    f.buttons = (u8)((accC_.sprint ? 1 : 0) | (accJump_ ? 2 : 0) | (accC_.jumpHeld ? 4 : 0) | (accC_.sneak ? 8 : 0));
    history_.push_back({f.seq, f.dtMs / 1000.0, controlOf(f), (float)nc::fromMrad(f.yaw), (float)nc::fromMrad(f.pitch)});
    while (history_.size() > 240) history_.pop_front();
    accDt_ = 0.0;
    accJump_ = false;
    nc::Msg m;
    m.type = nc::Type::Input;
    const int n = (int)std::min<size_t>(history_.size(), nc::kMaxInputs);
    m.input.count = (u8)n;
    for (int i = 0; i < n; ++i) {
        const Sent& h = history_[history_.size() - (size_t)n + (size_t)i];
        nc::InputFrame& o = m.input.f[(size_t)i];
        o.seq = h.seq;
        o.dtMs = (u8)std::clamp((int)std::lround(h.dt * 1000.0), 1, 100);
        o.forward = (i8)std::clamp((int)std::lround(h.c.forward * 127.0f), -127, 127);
        o.side = (i8)std::clamp((int)std::lround(h.c.side * 127.0f), -127, 127);
        o.yaw = nc::toMrad(h.yaw);
        o.pitch = (i16)std::clamp(std::lround(h.pitch * 1000.0), -1450L, 1450L);
        o.buttons = (u8)((h.c.sprint ? 1 : 0) | (h.c.jump ? 2 : 0) | (h.c.jumpHeld ? 4 : 0) | (h.c.sneak ? 8 : 0));
    }
    ++counters.inputsSent;
    sendMsg(m, false);
}

void CoopClient::applySnapshot(const nc::Snapshot& sn) {
    if (anySnap_ && (i32)(sn.tick - lastSnapTick_) <= 0) return;  // aelter als die letzte (unzuverlaessiger Kanal)
    anySnap_ = true;
    lastSnapTick_ = sn.tick;
    ++counters.snapshots;
    GameSession& s = *s_;
    const nc::SelfNet& me = sn.self;
    // Werte vom Host
    game::Stats& st = s.stats();
    st.health = me.health, st.sanity = me.sanity, st.stamina = me.stamina;
    st.exhausted = (me.flags & 4) != 0;
    st.speedBonus = me.boost / 100.0, st.speedLeft = me.boost > 0 ? 1.0 : 0.0;  // Energy Bar: gleiches Tempo wie beim Host
    s.flashlight().charge = me.battery;
    const bool lamp = (me.flags & 1) != 0;
    if (lamp != s.flashlight().on && clock_ - lampToggleT_ > 0.6) s.flashlight().on = lamp;
    const bool deadNow = (me.flags & 2) != 0;
    dead_ = deadNow;
    respawnIn_ = me.respawnIn;
    spectate_ = deadNow ? me.spectate : -1;
    // eigene Bewegung: Stand des Hosts nach Eingabe ack, Rest nachrechnen
    while (!history_.empty() && (i32)(history_.front().seq - me.ack) <= 0) history_.pop_front();
    if (!deadNow) {
        std::vector<GameSession::NetReplay> replay;
        replay.reserve(history_.size());
        for (const Sent& h : history_) replay.push_back({h.c, h.dt, h.yaw, h.pitch});
        const double err = s.netReconcile(mmVec(me.x, me.y, me.z), vec3{me.vx / 100.0f, me.vy / 100.0f, me.vz / 100.0f}, replay, false);
        counters.maxError = std::max(counters.maxError, err);
        counters.sumError += err, ++counters.errSamples, counters.over10cm += err > 0.1 ? 1 : 0;
        if (err > 2.0) ++counters.snaps;
        else if (err > 0.01) ++counters.corrections;
    }
    // Mitspieler
    for (int i = 0; i < sn.nPlayers; ++i) {
        const nc::PlayerNet& p = sn.players[(size_t)i];
        if (p.id == selfId_) continue;
        Pawn* o = pawnOf(p.id);
        if (!o) {
            std::string nm = "Spieler";
            for (const PlayerInfo& r : roster_)
                if (r.id == p.id) nm = r.name;
            auto np = std::make_unique<Pawn>(p.id, nm, s.content().difficulty(s.difficulty), &s.content());
            np->player = game::Player(nc::fromMm(p.x), nc::fromMm(p.y), nc::fromMm(p.z), nc::fromMrad(p.yaw));
            o = np.get();
            others_.push_back(std::move(np));
            s.pawns.clear();
            for (auto& q : others_) s.pawns.push_back(q.get());
        }
        Pawn::Sample sm;
        sm.t = clock_;
        sm.p = mmVec(p.x, p.y, p.z);
        sm.yaw = (float)nc::fromMrad(p.yaw), sm.pitch = p.pitch / 1000.0f;
        sm.speed = p.speed * 4.0f / 100.0f;
        sm.flags = p.flags;
        o->addSample(sm);
    }
    // wer fehlt, ist fort
    std::erase_if(others_, [&](const auto& o) {
        for (int i = 0; i < sn.nPlayers; ++i)
            if (sn.players[(size_t)i].id == o->id) return false;
        return true;
    });
    s.pawns.clear();
    for (auto& q : others_) s.pawns.push_back(q.get());
    // Wesen
    s.netCreatures.clear();
    for (int i = 0; i < sn.nCreatures; ++i) {
        const nc::CreatureNet& c = sn.creatures[(size_t)i];
        Presence::NetState n;
        n.netId = c.netId;
        n.role = c.role;
        n.pos = mmVec(c.x, c.y, c.z);
        n.yaw = (float)nc::fromMrad(c.yaw);
        n.state = c.state;
        n.target = c.target == 255 ? -1 : c.target;
        n.look = (c.flags & 1) != 0, n.lit = (c.flags & 2) != 0, n.frozen = (c.flags & 4) != 0;
        n.jaw = dq01(c.anim[0]), n.alert = dq01(c.anim[1]), n.crouch = dq01(c.anim[2]), n.growl = dq01(c.anim[3]);
        n.reach = dq01(c.anim[4]), n.fly = dq01(c.anim[5]), n.shiver = dq01(c.anim[6]), n.hang = dq01(c.anim[7]);
        n.bait = dq01(c.anim[8]), n.light = dq01(c.anim[9]), n.perceive = dq01(c.anim[10]), n.glow = dq01(c.anim[11]);
        n.tilt = dqs(c.anim[12]), n.lean = dqs(c.anim[13]);
        n.up = dqv(c.up), n.fwd = dqv(c.fwd);
        s.netCreatures.push_back(n);
    }
    s.netCreaturesFresh = true;
}

void CoopClient::applyInventory(const nc::Inventory& v) {
    game::Inventory& inv = s_->inventory();
    for (auto& sl : inv.slots) sl.reset();
    for (int i = 0; i < v.n; ++i) {
        const nc::InvEntry& e = v.e[(size_t)i];
        if (!s_->content().item(e.kind)) continue;  // unbekannte Art: verwerfen
        inv.slots[e.slot] = game::Stack{e.kind, e.count};
    }
}

void CoopClient::applyEvent(const nc::Event& e) {
    const vec3 pos = mmVec(e.x, e.y, e.z);
    if (e.kind == (u8)nc::EventKind::Level) {
        const size_t bar = e.text.find('|');
        pendingLevel_ = e.text.substr(0, bar), pendingAnchor_ = e.text.substr(bar + 1);
        pendingSupply_ = e.b ? e.b / 100.0 : 1.0;
        if (pendingLevel_ == "end") pendingEscape_ = true;
        else if (!s_ || pendingLevel_ != s_->levelId) pendingChange_ = true;
        return;
    }
    if (e.kind == (u8)nc::EventKind::Joined || e.kind == (u8)nc::EventKind::Left) {
        if (s_)
            s_->events().push_back({GameEvent::Toast,
                                    e.kind == (u8)nc::EventKind::Joined ? std::format("{} ist {}.", e.text, e.b ? "zurück" : "beigetreten")
                                                                       : std::string("Ein Mitspieler hat das Spiel verlassen."),
                                    3.0f});
        return;
    }
    if (!s_) return;
    GameSession& s = *s_;
    switch ((nc::EventKind)e.kind) {
        case nc::EventKind::Picked: s.netRemoveItem((u32)e.a); break;
        case nc::EventKind::Dropped: s.netAddDropped((u32)e.a, e.text, pos); break;
        case nc::EventKind::Blackout: s.netPowerCut(pos, e.b / 10.0f); break;
        case nc::EventKind::Died:
            if (e.a == selfId_) {
                s.netDie(e.b);
                dead_ = true;
            } else {
                s.events().push_back({GameEvent::Toast, std::format("{} ist tot.", [&] {
                                          for (const PlayerInfo& r : roster_)
                                              if (r.id == e.a) return r.name;
                                          return std::string("Ein Mitspieler");
                                      }()), 3.0f});
            }
            break;
        case nc::EventKind::Respawn:
            if (e.a == selfId_) {
                s.respawnCoop();
                s.player().x = pos.x, s.player().y = pos.y, s.player().z = pos.z;
                s.player().angle = nc::fromMrad((i16)e.b);
                history_.clear();
                dead_ = false;
            }
            break;
        case nc::EventKind::Toast:
            s.events().push_back({GameEvent::Toast, e.text, std::clamp(e.b / 10.0f, 1.0f, 6.0f)});
            break;
        case nc::EventKind::Ready:
            status_ = std::format("Ausgang gefunden – {}/{} bereit", e.a, e.b);
            if (pos.z > -0.0005f) status_ += std::format(" · die Gruppe geht in {:.0f} s", pos.z * 1000.0f);
            break;
        case nc::EventKind::Note: {
            GameEvent ev{GameEvent::NoteRead};
            ev.index = e.a;
            s.events().push_back(ev);
            break;
        }
        case nc::EventKind::Sound: {
            GameEvent ev{GameEvent::SoundAt};
            ev.sound = e.text, ev.pos = pos, ev.intensity = std::clamp(e.b / 100.0f, 0.0f, 1.0f);
            s.events().push_back(ev);
            break;
        }
        case nc::EventKind::Flicker: s.netFlicker(pos); break;
        case nc::EventKind::Figure: {
            GameEvent ev;
            ev.type = GameEvent::Figure;
            ev.index = std::clamp(e.a, 0, 30);
            ev.role = std::clamp(e.b, 0, 5);
            ev.pos = pos;
            s.events().push_back(ev);
            break;
        }
        default: break;
    }
}

bool CoopClient::takeLevelChange(std::string& level, std::string& anchor) {
    if (!pendingChange_) return false;
    pendingChange_ = false;
    level = pendingLevel_, anchor = pendingAnchor_;
    return true;
}

// Vorraete fuer die naechste Ebene: 1 + 0,6 x (n - 1), n = verbundene Spieler (MULTIPLAYER_ARCHITEKTUR.md 9)
double CoopHost::supplyScaleNext() const {
    int n = 1;
    for (const auto& g : guests_) n += g->id && g->connected() ? 1 : 0;
    return 1.0 + 0.6 * (n - 1);
}
bool CoopClient::takeEscape() {
    const bool e = pendingEscape_;
    pendingEscape_ = false;
    return e;
}

Pawn* CoopClient::pawnOf(int id) {
    for (auto& o : others_)
        if (o->id == id) return o.get();
    return nullptr;
}

std::vector<PlayerInfo> CoopClient::players() const { return roster_; }

void CoopClient::cycleSpectate() {
    std::vector<int> ids;
    for (const PlayerInfo& r : roster_)
        if (r.alive && r.id != selfId_) ids.push_back(r.id);
    if (ids.empty()) return;
    auto it = std::find(ids.begin(), ids.end(), spectate_);
    const int next = it == ids.end() || it + 1 == ids.end() ? ids.front() : *(it + 1);
    action((int)nc::ActionKind::Spectate, (u32)next);
}

int CoopClient::rttMs() const {
    LnStats st{};
    return ctx_ && api_.stats(ctx_, 0, &st) ? (int)st.rttMs : 0;
}

}  // namespace lim::coop
