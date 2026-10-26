#include "net/protocol.hpp"

#include <algorithm>
#include <cstring>

namespace lim::net {
namespace {

constexpr size_t kHeader = 5;  // Kennung (4) + Typ (1)
constexpr size_t kPlayerBytes = 16;

struct Writer {
    u8* p;
    size_t cap, n = 0;
    bool ok = true;
    void put(const u8* v, size_t k) {
        if (!ok || cap - n < k) {
            ok = false;
            return;
        }
        std::memcpy(p + n, v, k);
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
};

// Liest nur innerhalb der vorher genau geprueften Laenge; has() schuetzt trotzdem jeden Zugriff
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
};

bool nameChar(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == ' ' || c == '_' || c == '-';
}

bool knownType(u8 t) { return t >= (u8)MsgType::Hello && t <= (u8)MsgType::Ping; }

// Erlaubnisliste je Absender und Zustand
bool allowedFor(Role from, MsgType t) {
    if (from == Role::Guest) return t == MsgType::Hello || t == MsgType::Input || t == MsgType::Ping || t == MsgType::Bye;
    return t == MsgType::Welcome || t == MsgType::Snapshot || t == MsgType::Ping || t == MsgType::Bye;
}
bool allowedIn(Role from, ConnState st, MsgType t) {
    if (st == ConnState::Closed) return false;
    if (st == ConnState::Handshake) return from == Role::Guest ? t == MsgType::Hello : (t == MsgType::Welcome || t == MsgType::Bye);
    return from == Role::Guest ? (t == MsgType::Input || t == MsgType::Ping || t == MsgType::Bye)
                               : (t == MsgType::Snapshot || t == MsgType::Ping || t == MsgType::Bye);
}

size_t fixedSize(MsgType t) {
    switch (t) {
        case MsgType::Hello: return kHeader + 2 + 16 + 1 + kNameMax;
        case MsgType::Welcome: return kHeader + 1 + 4;
        case MsgType::Input: return kHeader + 4 + 1 + 1 + 2 + 2 + 2;
        case MsgType::Bye: return kHeader + 1;
        case MsgType::Ping: return kHeader + 4;
        case MsgType::Snapshot: return kHeader + 4 + 1;  // ohne Eintraege
    }
    return 0;
}

// Inhaltliche Pruefung (gemeinsam fuer Lesen und Schreiben: nur kanonische Nachrichten existieren)
Reject validate(const Message& m) {
    switch (m.type) {
        case MsgType::Hello: {
            if (m.hello.version != kVersion) return Reject::BadVersion;
            if (m.hello.nameLen < 1 || m.hello.nameLen > kNameMax) return Reject::BadName;
            for (int i = 0; i < kNameMax; ++i) {
                const char c = m.hello.name[(size_t)i];
                if (i < m.hello.nameLen ? !nameChar(c) : c != 0) return Reject::BadName;
            }
            return Reject::None;
        }
        case MsgType::Welcome:
            return m.welcome.playerId >= 1 && m.welcome.playerId < kMaxSnapshotPlayers ? Reject::None : Reject::BadRange;
        case MsgType::Input: {
            const Input& in = m.input;
            if (in.moveX < -127 || in.moveY < -127) return Reject::BadRange;
            if (in.yawMilli < -kMaxLookMilli || in.yawMilli > kMaxLookMilli) return Reject::BadRange;
            if (in.pitchMilli < -kMaxLookMilli || in.pitchMilli > kMaxLookMilli) return Reject::BadRange;
            if ((in.buttons & ~kButtonMask) != 0) return Reject::BadButtons;
            return Reject::None;
        }
        case MsgType::Snapshot: {
            const Snapshot& s = m.snapshot;
            if (s.count > kMaxSnapshotPlayers) return Reject::BadRange;
            u32 seen = 0;
            for (int i = 0; i < s.count; ++i) {
                const PlayerState& p = s.players[(size_t)i];
                if (p.id >= kMaxSnapshotPlayers || (seen & (1u << p.id))) return Reject::BadRange;
                seen |= 1u << p.id;
                for (i32 c : {p.xMm, p.yMm, p.zMm})
                    if (c < -kMaxCoordMm || c > kMaxCoordMm) return Reject::BadRange;
                if (p.yawMilli < -3142 || p.yawMilli > 3142) return Reject::BadRange;
                if ((p.flags & ~7u) != 0) return Reject::BadRange;
            }
            return Reject::None;
        }
        case MsgType::Bye: return m.bye.reason <= 3 ? Reject::None : Reject::BadRange;
        case MsgType::Ping: return Reject::None;
    }
    return Reject::UnknownType;
}

}  // namespace

const char* rejectName(Reject r) {
    static const char* const names[] = {"ok", "zu kurz", "zu lang", "falsche Kennung", "unbekannter Typ", "falsche Rolle", "falscher Zustand",
                                        "falsche Laenge", "falsche Version", "ausser Bereich", "ungueltiger Name", "ungueltige Tasten",
                                        "falscher Sitzungscode", "alte Folgenummer", "Rate ueberschritten"};
    const size_t i = (size_t)r;
    return i < sizeof(names) / sizeof(names[0]) ? names[i] : "?";
}

size_t wireSize(const Message& m) {
    size_t s = fixedSize(m.type);
    if (m.type == MsgType::Snapshot) s += (size_t)std::min<int>(m.snapshot.count, kMaxSnapshotPlayers) * kPlayerBytes;
    return s;
}

size_t encode(const Message& m, u8* buf, size_t cap) {
    if (!buf || validate(m) != Reject::None) return 0;
    Writer o{buf, cap};
    o.d(kMagic);
    o.b((u8)m.type);
    switch (m.type) {
        case MsgType::Hello:
            o.w(m.hello.version);
            o.put(m.hello.code.data(), 16);
            o.b(m.hello.nameLen);
            o.put((const u8*)m.hello.name.data(), kNameMax);
            break;
        case MsgType::Welcome:
            o.b(m.welcome.playerId);
            o.d(m.welcome.cookie);
            break;
        case MsgType::Input:
            o.d(m.input.seq);
            o.b((u8)m.input.moveX);
            o.b((u8)m.input.moveY);
            o.w((u16)m.input.yawMilli);
            o.w((u16)m.input.pitchMilli);
            o.w(m.input.buttons);
            break;
        case MsgType::Snapshot:
            o.d(m.snapshot.tick);
            o.b(m.snapshot.count);
            for (int i = 0; i < m.snapshot.count; ++i) {
                const PlayerState& p = m.snapshot.players[(size_t)i];
                o.b(p.id);
                o.d((u32)p.xMm);
                o.d((u32)p.yMm);
                o.d((u32)p.zMm);
                o.w((u16)p.yawMilli);
                o.b(p.flags);
            }
            break;
        case MsgType::Bye: o.b(m.bye.reason); break;
        case MsgType::Ping: o.d(m.ping.nonce); break;
    }
    return o.ok && o.n == wireSize(m) ? o.n : 0;
}

Reject decode(Role from, ConnState st, const u8* data, size_t n, Message& out) {
    if (!data || n < kHeader) return Reject::TooShort;
    if (n > kMaxPacket) return Reject::TooLong;
    Reader r{data, n};
    if (r.d() != kMagic) return Reject::BadMagic;
    const u8 t = r.b();
    if (!knownType(t)) return Reject::UnknownType;
    const MsgType type = (MsgType)t;
    if (!allowedFor(from, type)) return Reject::WrongRole;
    if (!allowedIn(from, st, type)) return Reject::WrongState;
    // Laenge genau: nichts fehlt, nichts haengt an (keine versteckten Daten)
    size_t want = fixedSize(type);
    if (type == MsgType::Snapshot) {
        if (n < want) return Reject::BadLength;
        const u8 count = data[kHeader + 4];
        if (count > kMaxSnapshotPlayers) return Reject::BadRange;
        want += (size_t)count * kPlayerBytes;
    }
    if (n != want) return Reject::BadLength;

    Message m;
    m.type = type;
    switch (type) {
        case MsgType::Hello:
            m.hello.version = r.w();
            for (auto& c : m.hello.code) c = r.b();
            m.hello.nameLen = r.b();
            for (auto& c : m.hello.name) c = (char)r.b();
            break;
        case MsgType::Welcome:
            m.welcome.playerId = r.b();
            m.welcome.cookie = r.d();
            break;
        case MsgType::Input:
            m.input.seq = r.d();
            m.input.moveX = (i8)r.b();
            m.input.moveY = (i8)r.b();
            m.input.yawMilli = (i16)r.w();
            m.input.pitchMilli = (i16)r.w();
            m.input.buttons = r.w();
            break;
        case MsgType::Snapshot:
            m.snapshot.tick = r.d();
            m.snapshot.count = r.b();
            for (int i = 0; i < m.snapshot.count; ++i) {
                PlayerState& p = m.snapshot.players[(size_t)i];
                p.id = r.b();
                p.xMm = (i32)r.d();
                p.yMm = (i32)r.d();
                p.zMm = (i32)r.d();
                p.yawMilli = (i16)r.w();
                p.flags = r.b();
            }
            break;
        case MsgType::Bye: m.bye.reason = r.b(); break;
        case MsgType::Ping: m.ping.nonce = r.d(); break;
    }
    if (r.bad || r.i != n) return Reject::BadLength;
    const Reject v = validate(m);
    if (v != Reject::None) return v;
    out = m;
    return Reject::None;
}

bool TokenBucket::take(double now) {
    if (last_ >= 0.0 && now > last_) tokens_ = std::min(burst_, tokens_ + (now - last_) * rate_);
    if (last_ < 0.0 || now > last_) last_ = now;  // Uhr laeuft nie rueckwaerts
    if (tokens_ < 1.0) return false;
    tokens_ -= 1.0;
    return true;
}

bool SeqWindow::accept(u32 seq) {
    if (!any_) {
        any_ = true;
        last_ = seq;
        return true;
    }
    const i32 d = (i32)(seq - last_);
    if (d <= 0 || d > (1 << 20)) return false;
    last_ = seq;
    return true;
}

Peer::Peer(Role remote, double now, const std::array<u8, 16>& sessionCode, Limits lim)
    : remote_(remote), code_(sessionCode), lim_(lim), bucket_(lim.rate, lim.burst), opened_(now), heard_(now) {}

Peer::Verdict Peer::fail(Reject r) {
    last_ = r;
    if (++invalid_ >= lim_.maxInvalid) {
        state_ = ConnState::Closed;
        return Verdict::Disconnect;
    }
    return Verdict::Drop;
}

Peer::Verdict Peer::onPacket(double now, const u8* data, size_t n, Message& out) {
    if (state_ == ConnState::Closed) return Verdict::Disconnect;
    if (!bucket_.take(now)) {
        last_ = Reject::RateLimited;
        if (++droppedInRow_ > lim_.maxDroppedInRow) {
            state_ = ConnState::Closed;
            return Verdict::Disconnect;
        }
        return Verdict::Drop;
    }
    droppedInRow_ = 0;
    Message m;
    const Reject r = decode(remote_, state_, data, n, m);
    if (r != Reject::None) return fail(r);
    if (m.type == MsgType::Hello) {
        u8 diff = 0;  // Vergleich in fester Zeit
        for (size_t i = 0; i < code_.size(); ++i) diff |= (u8)(m.hello.code[i] ^ code_[i]);
        if (diff != 0) return fail(Reject::BadCode);
        state_ = ConnState::Connected;
    } else if (m.type == MsgType::Welcome) {
        state_ = ConnState::Connected;
    } else if (m.type == MsgType::Input && !seq_.accept(m.input.seq)) {
        last_ = Reject::OldSequence;  // vertauschte Pakete sind normal: verwerfen, nicht zaehlen
        return Verdict::Drop;
    } else if (m.type == MsgType::Bye) {
        state_ = ConnState::Closed;
    }
    heard_ = now;
    last_ = Reject::None;
    out = m;
    return Verdict::Accept;
}

bool Peer::timedOut(double now) const {
    if (state_ == ConnState::Handshake) return now - opened_ > lim_.handshakeTimeout;
    if (state_ == ConnState::Connected) return now - heard_ > lim_.silenceTimeout;
    return false;
}

}  // namespace lim::net
