#include "netdll/lobby_proto.hpp"

#include <nlohmann/json.hpp>

#include <cstring>

#include "netdll/joincode.hpp"

namespace lim::netc {

using nlohmann::json;

namespace {

const char* kErrs[] = {"noroom", "full", "banned", "rate", "version", "bad", "busy", "closed", "timeout", "badkey", "denied"};

bool getStr(const json& j, const char* key, std::string& out, size_t maxLen) {
    auto it = j.find(key);
    if (it == j.end() || !it->is_string()) return false;
    const std::string& s = it->get_ref<const std::string&>();
    if (s.size() > maxLen) return false;
    out = s;
    return true;
}
bool getInt(const json& j, const char* key, int lo, int hi, int& out) {
    auto it = j.find(key);
    if (it == j.end() || !it->is_number_integer()) return false;
    const long long v = it->get<long long>();
    if (v < lo || v > hi) return false;
    out = (int)v;
    return true;
}
bool hasKey(const json& j, const char* key) { return j.find(key) != j.end(); }

LType typeOf(const std::string& t) {
    static const struct {
        const char* n;
        LType t;
    } map[] = {{"host", LType::Host},     {"join", LType::Join},   {"sig", LType::Sig},     {"cand", LType::Cand},
               {"refuse", LType::Refuse}, {"ban", LType::Ban},     {"done", LType::Done},   {"room", LType::Room},
               {"joined", LType::Joined}, {"guest", LType::Guest}, {"gone", LType::Gone},   {"err", LType::Err}};
    for (const auto& m : map)
        if (t == m.n) return m.t;
    return LType::None;
}
const char* nameOf(LType t) {
    switch (t) {
        case LType::Host: return "host";
        case LType::Join: return "join";
        case LType::Sig: return "sig";
        case LType::Cand: return "cand";
        case LType::Refuse: return "refuse";
        case LType::Ban: return "ban";
        case LType::Done: return "done";
        case LType::Room: return "room";
        case LType::Joined: return "joined";
        case LType::Guest: return "guest";
        case LType::Gone: return "gone";
        case LType::Err: return "err";
        default: return nullptr;
    }
}

bool parseIce(const json& j, std::vector<IceEntry>& out) {
    auto it = j.find("ice");
    if (it == j.end()) return true;  // ohne: keine Server (direkt)
    if (!it->is_array() || it->size() > (size_t)kMaxIce) return false;
    for (const json& e : *it) {
        if (!e.is_object()) return false;
        IceEntry ie;
        std::string type;
        int port = 0;
        if (!getStr(e, "type", type, 8) || (type != "stun" && type != "turn")) return false;
        ie.turn = type == "turn";
        if (!getStr(e, "host", ie.host, 253) || !hostOk(ie.host)) return false;
        if (!getInt(e, "port", 1, 65535, port)) return false;
        ie.port = (uint16_t)port;
        if (ie.turn && (!getStr(e, "user", ie.user, 64) || !credOk(ie.user) || !getStr(e, "pass", ie.pass, 64) || !credOk(ie.pass))) return false;
        out.push_back(ie);
    }
    return true;
}

// gemeinsame Felder von sig und cand
bool parseSigCand(const json& j, LMsg& m) {
    if (m.t == LType::Sig) {
        if (!getStr(j, "kind", m.kind, 8) || (m.kind != "offer" && m.kind != "answer")) return false;
        return getStr(j, "sdp", m.sdp, kMaxSdp) && sdpOk(m.sdp);
    }
    if (!getStr(j, "c", m.cand, kMaxCand) || !candOk(m.cand)) return false;
    return getStr(j, "mid", m.mid, 32) && midOk(m.mid);
}

bool parseCommon(const std::string& text, json& j, LMsg& m) {
    if (text.empty() || text.size() > kMaxLobbyMsg || !depthOk(text, 4)) return false;
    j = json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return false;
    std::string t;
    if (!getStr(j, "t", t, 8)) return false;
    m = LMsg{};
    m.t = typeOf(t);
    return m.t != LType::None;
}

}  // namespace

bool depthOk(const std::string& text, int maxDepth) {
    int d = 0;
    bool inStr = false, esc = false;
    for (char c : text) {
        if (inStr) {
            if (esc) esc = false;
            else if (c == '\\') esc = true;
            else if (c == '"') inStr = false;
            continue;
        }
        if (c == '"') inStr = true;
        else if (c == '{' || c == '[') {
            if (++d > maxDepth) return false;
        } else if (c == '}' || c == ']') --d;
    }
    return true;
}

bool nameOk(const std::string& s) {
    if (s.empty() || s.size() > 16 || s.front() == ' ' || s.back() == ' ') return false;
    for (unsigned char c : s)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '_' || c == '.')) return false;
    return true;
}
bool sdpOk(const std::string& s) {
    if (s.size() < 10 || s.size() > kMaxSdp) return false;
    for (unsigned char c : s)
        if (!(c == '\r' || c == '\n' || (c >= 0x20 && c < 0x7F))) return false;
    return s.compare(0, 4, "v=0\r") == 0 || s.compare(0, 4, "v=0\n") == 0;
}
bool candOk(const std::string& s) {
    if (s.size() < 12 || s.size() > kMaxCand) return false;
    if (s.compare(0, 10, "candidate:") != 0 && s.compare(0, 12, "a=candidate:") != 0) return false;
    for (unsigned char c : s)
        if (c < 0x20 || c >= 0x7F) return false;
    return true;
}
bool midOk(const std::string& s) {
    if (s.empty() || s.size() > 32) return false;
    for (unsigned char c : s)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    return true;
}
bool errOk(const std::string& s) {
    for (const char* e : kErrs)
        if (s == e) return true;
    return false;
}
bool hostOk(const std::string& s) {
    if (s.size() > 253) return false;
    for (unsigned char c : s)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == ':')) return false;
    return true;
}
bool credOk(const std::string& s) {
    if (s.empty() || s.size() > 64) return false;
    for (unsigned char c : s)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    return true;
}

bool parseFromClient(const std::string& text, bool fromHost, LMsg& m) {
    json j;
    if (!parseCommon(text, j, m)) return false;
    switch (m.t) {
        case LType::Host:
            return getInt(j, "v", 0, 1000, m.v) && getInt(j, "max", 1, 7, m.max);
        case LType::Join:
            return getInt(j, "v", 0, 1000, m.v) && getStr(j, "room", m.room, 6) && roomOk(m.room) && getStr(j, "name", m.name, 16) && nameOk(m.name);
        case LType::Sig:
        case LType::Cand:
            if (fromHost) {
                if (!getInt(j, "to", 1, kMaxGuestId, m.to)) return false;
            } else if (hasKey(j, "to")) {
                return false;  // ein Gast spricht nur mit dem Host
            }
            return parseSigCand(j, m);
        case LType::Refuse:
            return fromHost && getInt(j, "id", 1, kMaxGuestId, m.id) && getStr(j, "e", m.err, 16) && errOk(m.err);
        case LType::Ban:
            return fromHost && getInt(j, "id", 1, kMaxGuestId, m.id) && getStr(j, "addr", m.addr, 32) && credOk(m.addr);
        case LType::Done:
            return !fromHost;
        default:
            return false;  // Nachrichten der Lobby nimmt die Lobby nicht an
    }
}

bool parseFromLobby(const std::string& text, LMsg& m) {
    json j;
    if (!parseCommon(text, j, m)) return false;
    switch (m.t) {
        case LType::Room:
            return getStr(j, "room", m.room, 6) && roomOk(m.room) && parseIce(j, m.ice) && getInt(j, "ttl", 0, 7 * 86400, m.ttl);
        case LType::Joined:
            return getInt(j, "id", 1, kMaxGuestId, m.id) && parseIce(j, m.ice);
        case LType::Guest:
            return getInt(j, "id", 1, kMaxGuestId, m.id) && getStr(j, "name", m.name, 16) && nameOk(m.name) && getStr(j, "addr", m.addr, 32) &&
                   credOk(m.addr);
        case LType::Sig:
        case LType::Cand:
            if (hasKey(j, "from") && !getInt(j, "from", 1, kMaxGuestId, m.from)) return false;
            return parseSigCand(j, m);
        case LType::Gone:
            return getInt(j, "id", 1, kMaxGuestId, m.id);
        case LType::Err:
            return getStr(j, "e", m.err, 16) && errOk(m.err);
        default:
            return false;
    }
}

std::string build(const LMsg& m) {
    const char* t = nameOf(m.t);
    if (!t) return {};
    json j = json::object();
    j["t"] = t;
    auto ice = [&] {
        json a = json::array();
        for (const IceEntry& e : m.ice) {
            json o = {{"type", e.turn ? "turn" : "stun"}, {"host", e.host}, {"port", e.port}};
            if (e.turn) o["user"] = e.user, o["pass"] = e.pass;
            a.push_back(o);
        }
        j["ice"] = a;
    };
    switch (m.t) {
        case LType::Host: j["v"] = m.v, j["max"] = m.max; break;
        case LType::Join: j["v"] = m.v, j["room"] = m.room, j["name"] = m.name; break;
        case LType::Sig:
        case LType::Cand:
            if (m.to) j["to"] = m.to;
            if (m.from) j["from"] = m.from;
            if (m.t == LType::Sig) j["kind"] = m.kind, j["sdp"] = m.sdp;
            else j["c"] = m.cand, j["mid"] = m.mid;
            break;
        case LType::Refuse: j["id"] = m.id, j["e"] = m.err; break;
        case LType::Ban: j["id"] = m.id, j["addr"] = m.addr; break;
        case LType::Done: break;
        case LType::Room: j["room"] = m.room, j["ttl"] = m.ttl, ice(); break;
        case LType::Joined: j["id"] = m.id, ice(); break;
        case LType::Guest: j["id"] = m.id, j["name"] = m.name, j["addr"] = m.addr; break;
        case LType::Gone: j["id"] = m.id; break;
        case LType::Err: j["e"] = m.err; break;
        default: return {};
    }
    std::string out = j.dump(-1, ' ', true, json::error_handler_t::replace);
    return out.size() <= kMaxLobbyMsg ? out : std::string();
}

}  // namespace lim::netc
