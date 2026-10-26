#include "net/coop_proto.hpp"

#include <cstring>

namespace lim::net::coop {
namespace {

struct Writer {
    u8* p;
    size_t cap, n = 0;
    bool ok = true;
    void put(const void* v, size_t k) {
        if (!ok || cap - n < k) {
            ok = false;
            return;
        }
        if (k) std::memcpy(p + n, v, k);
        n += k;
    }
    void b(u8 v) { put(&v, 1); }
    void w(u16 v) {
        const u8 x[2] = {(u8)v, (u8)(v >> 8)};
        put(x, 2);
    }
    void d(u32 v) {
        const u8 x[4] = {(u8)v, (u8)(v >> 8), (u8)(v >> 16), (u8)(v >> 24)};
        put(x, 4);
    }
    void q(u64 v) {
        d((u32)v);
        d((u32)(v >> 32));
    }
    void s(const std::string& t) {  // Laenge (1 Byte) + Inhalt
        b((u8)t.size());
        put(t.data(), t.size());
    }
};

struct Reader {
    const u8* p;
    size_t n, i = 0;
    bool bad = false;
    bool has(size_t k) {
        if (n - i < k) bad = true;
        return !bad;
    }
    u8 b() { return has(1) ? p[i++] : 0; }
    u16 w() {
        if (!has(2)) return 0;
        const u16 v = (u16)(p[i] | (p[i + 1] << 8));
        i += 2;
        return v;
    }
    u32 d() {
        if (!has(4)) return 0;
        const u32 v = (u32)p[i] | ((u32)p[i + 1] << 8) | ((u32)p[i + 2] << 16) | ((u32)p[i + 3] << 24);
        i += 4;
        return v;
    }
    u64 q() {
        const u64 lo = d();
        return lo | ((u64)d() << 32);
    }
    std::string s(size_t max) {
        const size_t k = b();
        if (bad || k > max || !has(k)) {
            bad = true;
            return {};
        }
        std::string out((const char*)p + i, k);
        i += k;
        return out;
    }
};

bool knownType(u8 t) { return (t >= 1 && t <= 5) || (t >= 16 && t <= 22); }

// Erlaubnisliste: wer darf was in welchem Zustand senden
bool allowedFor(Role from, Type t) {
    const u8 v = (u8)t;
    if (t == Type::Bye) return true;
    return from == Role::Guest ? v < 16 : v >= 16;
}
bool allowedIn(Role from, ConnState st, Type t) {
    if (st == ConnState::Closed) return false;
    if (st == ConnState::Handshake) return from == Role::Guest ? (t == Type::Join || t == Type::Bye) : (t == Type::Welcome || t == Type::Bye);
    return t != Type::Join && t != Type::Welcome;  // verbunden: alles ausser dem Handshake (Rolle oben)
}

bool inRange(i32 v, i32 lo, i32 hi) { return v >= lo && v <= hi; }
bool coordOk(i32 v) { return inRange(v, -kCoordMax, kCoordMax); }

Reject validate(const Msg& m) {
    switch (m.type) {
        case Type::Join: return m.join.version == kProtoVersion ? Reject::None : Reject::BadVersion;
        case Type::Input: {
            const Input& in = m.input;
            if (in.count < 1 || in.count > kMaxInputs) return Reject::BadRange;
            for (int i = 0; i < in.count; ++i) {
                const InputFrame& f = in.f[(size_t)i];
                if (f.dtMs < 1 || f.dtMs > 100) return Reject::BadRange;
                if (f.forward < -127 || f.side < -127) return Reject::BadRange;
                if (!inRange(f.yaw, -3142, 3142) || !inRange(f.pitch, -1450, 1450)) return Reject::BadRange;
                if (f.buttons & ~kButtonMask) return Reject::BadButtons;
                if (i > 0 && f.seq != in.f[(size_t)i - 1].seq + 1) return Reject::BadRange;  // lueckenlos aufsteigend
            }
            return Reject::None;
        }
        case Type::Action: {
            const Action& a = m.action;
            switch ((ActionKind)a.kind) {
                case ActionKind::Interact: return Reject::None;
                case ActionKind::UseSlot:
                case ActionKind::Drop:
                case ActionKind::DropAll:
                case ActionKind::Select: return a.arg < (u32)kInvSlots ? Reject::None : Reject::BadRange;
                case ActionKind::Lamp: return a.arg <= 1 ? Reject::None : Reject::BadRange;
                case ActionKind::Spectate: return a.arg < (u32)kMaxPlayers ? Reject::None : Reject::BadRange;
            }
            return Reject::BadRange;
        }
        case Type::Ping: return Reject::None;
        case Type::Bye: return m.bye.reason <= 7 ? Reject::None : Reject::BadRange;
        case Type::Welcome: {
            const Welcome& w = m.welcome;
            if (w.playerId < 1 || w.playerId >= kMaxPlayers || w.difficulty > 7 || w.maxPlayers < 2 || w.maxPlayers > kMaxPlayers || w.flags > 1)
                return Reject::BadRange;
            if (w.level.empty() || !levelOk(w.level) || !levelOk(w.anchor)) return Reject::BadText;
            if (w.supply < 100 || w.supply > 300) return Reject::BadRange;
            return Reject::None;
        }
        case Type::Snapshot: {
            const Snapshot& s = m.snapshot;
            const SelfNet& me = s.self;
            if (!coordOk(me.x) || !coordOk(me.y) || !coordOk(me.z)) return Reject::BadRange;
            if (!inRange(me.yaw, -3142, 3142) || !inRange(me.pitch, -1450, 1450)) return Reject::BadRange;
            if (me.health > 100 || me.sanity > 100 || me.stamina > 100 || me.battery > 100 || (me.flags & ~0x3Fu) || me.spectate >= kMaxPlayers ||
                me.boost > 100)
                return Reject::BadRange;
            if (s.nPlayers > kMaxPlayers - 1 || s.nCreatures > kMaxCreatures) return Reject::BadRange;
            u32 seen = 0;
            for (int i = 0; i < s.nPlayers; ++i) {
                const PlayerNet& p = s.players[(size_t)i];
                if (p.id >= kMaxPlayers || (seen & (1u << p.id))) return Reject::Duplicate;
                seen |= 1u << p.id;
                if (!coordOk(p.x) || !coordOk(p.y) || !coordOk(p.z) || !inRange(p.yaw, -3142, 3142) || !inRange(p.pitch, -1450, 1450) ||
                    (p.flags & 0x80))
                    return Reject::BadRange;
            }
            for (int i = 0; i < s.nCreatures; ++i) {
                const CreatureNet& c = s.creatures[(size_t)i];
                if (c.role > 5 || !coordOk(c.x) || !coordOk(c.y) || !coordOk(c.z) || !inRange(c.yaw, -3142, 3142) || (c.flags & ~7u) ||
                    (c.target != 255 && c.target >= kMaxPlayers) || c.state > 15)
                    return Reject::BadRange;
                for (int j = 0; j < i; ++j)
                    if (s.creatures[(size_t)j].netId == c.netId) return Reject::Duplicate;
            }
            return Reject::None;
        }
        case Type::Event: {
            const Event& e = m.event;
            if (e.kind < 1 || e.kind > 15) return Reject::BadRange;
            if (!coordOk(e.x) || !coordOk(e.y) || !coordOk(e.z)) return Reject::BadRange;
            if (!textOk(e.text)) return Reject::BadText;
            if (e.kind == (u8)EventKind::Dropped && (!kindOk(e.text) || e.b < 1 || e.b > 99)) return Reject::BadKind;
            if (e.kind == (u8)EventKind::Level) {
                if (e.b != 0 && (e.b < 100 || e.b > 300)) return Reject::BadRange;  // Vorraete der neuen Ebene in %
                const size_t bar = e.text.find('|');
                if (bar == std::string::npos || !levelOk(e.text.substr(0, bar)) || !levelOk(e.text.substr(bar + 1)) || bar == 0) return Reject::BadText;
            }
            if ((e.kind == (u8)EventKind::Died || e.kind == (u8)EventKind::Respawn || e.kind == (u8)EventKind::Joined ||
                 e.kind == (u8)EventKind::Left) &&
                !inRange(e.a, 0, kMaxPlayers - 1))
                return Reject::BadRange;
            if (e.kind == (u8)EventKind::Joined && !nameOk(e.text)) return Reject::BadText;
            if (e.kind == (u8)EventKind::Sound && !kindOk(e.text)) return Reject::BadKind;
            return Reject::None;
        }
        case Type::Inventory: {
            const Inventory& v = m.inventory;
            if (v.selected >= kInvSlots || v.n > kInvSlots) return Reject::BadRange;
            u64 seen = 0;
            for (int i = 0; i < v.n; ++i) {
                const InvEntry& e = v.e[(size_t)i];
                if (e.slot >= kInvSlots || (seen & (1ull << e.slot))) return Reject::Duplicate;
                seen |= 1ull << e.slot;
                if (!kindOk(e.kind)) return Reject::BadKind;
                if (e.count < 1 || e.count > 99) return Reject::BadRange;
            }
            return Reject::None;
        }
        case Type::Roster: {
            const Roster& r = m.roster;
            if (r.n < 1 || r.n > kMaxPlayers) return Reject::BadRange;
            u32 seen = 0;
            for (int i = 0; i < r.n; ++i) {
                const RosterEntry& e = r.e[(size_t)i];
                if (e.id >= kMaxPlayers || (seen & (1u << e.id))) return Reject::Duplicate;
                seen |= 1u << e.id;
                if (e.flags & ~7u) return Reject::BadRange;
                if (!nameOk(e.name)) return Reject::BadText;
            }
            return Reject::None;
        }
        case Type::Pong:
        case Type::Check: return Reject::None;
    }
    return Reject::UnknownType;
}

void putCreature(Writer& o, const CreatureNet& c) {
    o.w(c.netId);
    o.b(c.role);
    o.d((u32)c.x), o.d((u32)c.y), o.d((u32)c.z);
    o.w((u16)c.yaw);
    o.b(c.state);
    o.b(c.target);
    o.b(c.flags);
    o.put(c.anim.data(), kAnimN);
    for (i8 v : c.up) o.b((u8)v);
    for (i8 v : c.fwd) o.b((u8)v);
}

}  // namespace

const char* rejectName(Reject r) {
    static const char* const names[] = {"ok", "leer", "zu lang", "unbekannter Typ", "falsche Rolle", "falscher Zustand", "falsche Laenge",
                                        "falsche Version", "ausser Bereich", "ungueltiger Text", "ungueltige Art", "ungueltige Tasten",
                                        "doppelt"};
    const size_t i = (size_t)r;
    return i < sizeof(names) / sizeof(names[0]) ? names[i] : "?";
}

bool kindOk(const std::string& s) {
    if (s.empty() || s.size() > (size_t)kKindMax) return false;
    for (char c : s)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    return true;
}
bool levelOk(const std::string& s) {
    if (s.size() > (size_t)kLevelMax) return false;
    for (char c : s)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-')) return false;
    return true;
}
bool nameOk(const std::string& s) {
    if (s.empty() || s.size() > (size_t)kNameMax) return false;
    for (char c : s)
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == ' ' || c == '_' || c == '-')) return false;
    return true;
}
bool textOk(const std::string& s) {
    if (s.size() > (size_t)kTextMax) return false;
    for (size_t i = 0; i < s.size();) {
        const u8 c = (u8)s[i];
        int k = 0;
        u32 cp = 0;
        if (c < 0x80) k = 0, cp = c;
        else if ((c & 0xE0) == 0xC0) k = 1, cp = c & 0x1F;
        else if ((c & 0xF0) == 0xE0) k = 2, cp = c & 0x0F;
        else if ((c & 0xF8) == 0xF0) k = 3, cp = c & 0x07;
        else return false;
        for (int j = 1; j <= k; ++j) {
            if (i + (size_t)j >= s.size()) return false;
            const u8 cc = (u8)s[i + (size_t)j];
            if ((cc & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (cc & 0x3F);
        }
        // keine Steuerzeichen, keine ueberlangen Formen, keine Ersatzzeichen-Haelften
        if (cp < 0x20 || cp == 0x7F || (cp >= 0x80 && cp < 0xA0) || (k == 1 && cp < 0x80) || (k == 2 && cp < 0x800) ||
            (k == 3 && cp < 0x10000) || (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF || cp == 0x202E || cp == 0x202D)
            return false;
        i += (size_t)k + 1;
    }
    return true;
}
i16 toMrad(double a) {
    const double r = std::remainder(a, 6.283185307179586);
    return (i16)std::clamp((long)std::lround(r * 1000.0), -3142L, 3142L);
}

size_t encode(const Msg& m, u8* buf, size_t cap) {
    if (!buf || validate(m) != Reject::None) return 0;
    Writer o{buf, std::min(cap, kMaxMsg)};
    o.b((u8)m.type);
    switch (m.type) {
        case Type::Join:
            o.w(m.join.version);
            o.d(m.join.build);
            o.put(m.join.token.data(), 16);
            break;
        case Type::Input:
            o.b(m.input.count);
            for (int i = 0; i < m.input.count; ++i) {
                const InputFrame& f = m.input.f[(size_t)i];
                o.d(f.seq);
                o.b(f.dtMs);
                o.b((u8)f.forward), o.b((u8)f.side);
                o.w((u16)f.yaw), o.w((u16)f.pitch);
                o.b(f.buttons);
            }
            break;
        case Type::Action:
            o.d(m.action.seq);
            o.b(m.action.kind);
            o.d(m.action.arg);
            break;
        case Type::Ping:
        case Type::Pong:
            o.d(m.ping.nonce);
            o.d(m.ping.time);
            break;
        case Type::Bye: o.b(m.bye.reason); break;
        case Type::Welcome: {
            const Welcome& w = m.welcome;
            o.b(w.playerId);
            o.put(w.token.data(), 16);
            o.q((u64)w.runSeed), o.q((u64)w.seed);
            o.b(w.difficulty);
            o.s(w.level), o.s(w.anchor);
            o.d(w.worldHash), o.d(w.tick);
            o.b(w.maxPlayers), o.b(w.flags);
            o.w(w.supply);
            break;
        }
        case Type::Snapshot: {
            const Snapshot& s = m.snapshot;
            const SelfNet& me = s.self;
            o.d(s.tick);
            o.d(me.ack);
            o.d((u32)me.x), o.d((u32)me.y), o.d((u32)me.z);
            o.w((u16)me.vx), o.w((u16)me.vy), o.w((u16)me.vz);
            o.w((u16)me.yaw), o.w((u16)me.pitch);
            o.b(me.health), o.b(me.sanity), o.b(me.stamina), o.b(me.battery);
            o.w(me.flags);
            o.b(me.respawnIn), o.b(me.spectate), o.b(me.boost);
            o.b(s.nPlayers);
            for (int i = 0; i < s.nPlayers; ++i) {
                const PlayerNet& p = s.players[(size_t)i];
                o.b(p.id);
                o.d((u32)p.x), o.d((u32)p.y), o.d((u32)p.z);
                o.w((u16)p.yaw), o.w((u16)p.pitch);
                o.b(p.speed), o.b(p.flags);
            }
            o.b(s.nCreatures);
            for (int i = 0; i < s.nCreatures; ++i) putCreature(o, s.creatures[(size_t)i]);
            break;
        }
        case Type::Event: {
            const Event& e = m.event;
            o.d(e.seq);
            o.b(e.kind);
            o.d((u32)e.a), o.d((u32)e.b);
            o.d((u32)e.x), o.d((u32)e.y), o.d((u32)e.z);
            o.s(e.text);
            break;
        }
        case Type::Inventory:
            o.b(m.inventory.selected);
            o.b(m.inventory.n);
            for (int i = 0; i < m.inventory.n; ++i) {
                const InvEntry& e = m.inventory.e[(size_t)i];
                o.b(e.slot);
                o.s(e.kind);
                o.b(e.count);
            }
            break;
        case Type::Roster:
            o.b(m.roster.n);
            for (int i = 0; i < m.roster.n; ++i) {
                const RosterEntry& e = m.roster.e[(size_t)i];
                o.b(e.id), o.b(e.flags);
                o.s(e.name);
            }
            break;
        case Type::Check:
            o.d(m.check.tick);
            o.d(m.check.hash);
            break;
    }
    return o.ok ? o.n : 0;
}

Reject decode(Role from, ConnState st, const u8* data, size_t n, Msg& out) {
    if (!data || n == 0) return Reject::Empty;
    if (n > kMaxMsg) return Reject::TooLong;
    Reader r{data, n};
    const u8 t = r.b();
    if (!knownType(t)) return Reject::UnknownType;
    const Type type = (Type)t;
    if (!allowedFor(from, type)) return Reject::WrongRole;
    if (!allowedIn(from, st, type)) return Reject::WrongState;
    Msg m;
    m.type = type;
    switch (type) {
        case Type::Join:
            m.join.version = r.w();
            m.join.build = r.d();
            if (r.has(16)) std::memcpy(m.join.token.data(), r.p + r.i, 16), r.i += 16;
            break;
        case Type::Input:
            m.input.count = r.b();
            if (m.input.count < 1 || m.input.count > kMaxInputs) return Reject::BadRange;
            for (int i = 0; i < m.input.count; ++i) {
                InputFrame& f = m.input.f[(size_t)i];
                f.seq = r.d();
                f.dtMs = r.b();
                f.forward = (i8)r.b(), f.side = (i8)r.b();
                f.yaw = (i16)r.w(), f.pitch = (i16)r.w();
                f.buttons = r.b();
            }
            break;
        case Type::Action:
            m.action.seq = r.d();
            m.action.kind = r.b();
            m.action.arg = r.d();
            break;
        case Type::Ping:
        case Type::Pong:
            m.ping.nonce = r.d();
            m.ping.time = r.d();
            break;
        case Type::Bye: m.bye.reason = r.b(); break;
        case Type::Welcome: {
            Welcome& w = m.welcome;
            w.playerId = r.b();
            if (r.has(16)) std::memcpy(w.token.data(), r.p + r.i, 16), r.i += 16;
            w.runSeed = (i64)r.q(), w.seed = (i64)r.q();
            w.difficulty = r.b();
            w.level = r.s(kLevelMax), w.anchor = r.s(kLevelMax);
            w.worldHash = r.d(), w.tick = r.d();
            w.maxPlayers = r.b(), w.flags = r.b();
            w.supply = r.w();
            break;
        }
        case Type::Snapshot: {
            Snapshot& s = m.snapshot;
            SelfNet& me = s.self;
            s.tick = r.d();
            me.ack = r.d();
            me.x = (i32)r.d(), me.y = (i32)r.d(), me.z = (i32)r.d();
            me.vx = (i16)r.w(), me.vy = (i16)r.w(), me.vz = (i16)r.w();
            me.yaw = (i16)r.w(), me.pitch = (i16)r.w();
            me.health = r.b(), me.sanity = r.b(), me.stamina = r.b(), me.battery = r.b();
            me.flags = r.w();
            me.respawnIn = r.b(), me.spectate = r.b(), me.boost = r.b();
            s.nPlayers = r.b();
            if (s.nPlayers > kMaxPlayers - 1) return Reject::BadRange;
            for (int i = 0; i < s.nPlayers; ++i) {
                PlayerNet& p = s.players[(size_t)i];
                p.id = r.b();
                p.x = (i32)r.d(), p.y = (i32)r.d(), p.z = (i32)r.d();
                p.yaw = (i16)r.w(), p.pitch = (i16)r.w();
                p.speed = r.b(), p.flags = r.b();
            }
            s.nCreatures = r.b();
            if (s.nCreatures > kMaxCreatures) return Reject::BadRange;
            for (int i = 0; i < s.nCreatures; ++i) {
                CreatureNet& c = s.creatures[(size_t)i];
                c.netId = r.w();
                c.role = r.b();
                c.x = (i32)r.d(), c.y = (i32)r.d(), c.z = (i32)r.d();
                c.yaw = (i16)r.w();
                c.state = r.b(), c.target = r.b(), c.flags = r.b();
                if (r.has(kAnimN)) std::memcpy(c.anim.data(), r.p + r.i, kAnimN), r.i += kAnimN;
                for (i8& v : c.up) v = (i8)r.b();
                for (i8& v : c.fwd) v = (i8)r.b();
            }
            break;
        }
        case Type::Event: {
            Event& e = m.event;
            e.seq = r.d();
            e.kind = r.b();
            e.a = (i32)r.d(), e.b = (i32)r.d();
            e.x = (i32)r.d(), e.y = (i32)r.d(), e.z = (i32)r.d();
            e.text = r.s(kTextMax);
            break;
        }
        case Type::Inventory: {
            Inventory& v = m.inventory;
            v.selected = r.b();
            v.n = r.b();
            if (v.n > kInvSlots) return Reject::BadRange;
            for (int i = 0; i < v.n; ++i) {
                InvEntry& e = v.e[(size_t)i];
                e.slot = r.b();
                e.kind = r.s(kKindMax);
                e.count = r.b();
            }
            break;
        }
        case Type::Roster: {
            Roster& ro = m.roster;
            ro.n = r.b();
            if (ro.n > kMaxPlayers) return Reject::BadRange;
            for (int i = 0; i < ro.n; ++i) {
                RosterEntry& e = ro.e[(size_t)i];
                e.id = r.b(), e.flags = r.b();
                e.name = r.s(kNameMax);
            }
            break;
        }
        case Type::Check:
            m.check.tick = r.d();
            m.check.hash = r.d();
            break;
    }
    if (r.bad || r.i != n) return Reject::BadLength;  // genau so lang wie der Inhalt, kein Rest
    if (const Reject v = validate(m); v != Reject::None) return v;
    out = std::move(m);
    return Reject::None;
}

}  // namespace lim::net::coop
