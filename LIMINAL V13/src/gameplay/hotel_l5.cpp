// V13.3 Teil 4 Level 5: Wesen des Hotels ohne eigene Rolle - Kolonien, Woodlins, Watchers, der Gastgeber, Fleisch
// (gameplay/hotel_l5.hpp; KREATUR_Ants.md, KREATUR_Woodlins.md, KREATUR_Watchers.md, KREATUR_Gastgeber.md).
#include "gameplay/hotel_l5.hpp"

#include <algorithm>
#include <cmath>
#include <format>

#include "anim/skeleton.hpp"
#include "core/log.hpp"
#include "gameplay/kin_l3.hpp"
#include "world/level_def.hpp"
#include "world/world.hpp"

namespace lim::game {

namespace {
constexpr float kPi = 3.14159265f, kTau = 6.2831853f;
mat4 rotX(float a) {
    mat4 m = mat4::identity();
    const float c = std::cos(a), s = std::sin(a);
    m.c[1] = {0, c, s, 0};
    m.c[2] = {0, -s, c, 0};
    return m;
}
mat4 rotY(float a) {
    mat4 m = mat4::identity();
    const float c = std::cos(a), s = std::sin(a);
    m.c[0] = {c, 0, -s, 0};
    m.c[2] = {s, 0, c, 0};
    return m;
}
mat4 basis(vec3 x, vec3 y, vec3 z, vec3 o) {
    mat4 m = mat4::identity();
    m.c[0] = {x.x, x.y, x.z, 0};
    m.c[1] = {y.x, y.y, y.z, 0};
    m.c[2] = {z.x, z.y, z.z, 0};
    m.c[3] = {o.x, o.y, o.z, 1};
    return m;
}
// Teil entlang d (x), z moeglichst "up"
mat4 along(vec3 o, vec3 d, vec3 up) {
    d = normalize(d);
    vec3 y = cross(up, d);
    if (length(y) < 1e-4f) y = cross(vec3{1, 0, 0}, d);
    if (length(y) < 1e-4f) y = cross(vec3{0, 1, 0}, d);
    y = normalize(y);
    const vec3 z = cross(d, y);
    return basis(d, y, z, o);
}
vec3 apply(const mat4& m, vec3 p) {
    const vec4 r = m * vec4{p.x, p.y, p.z, 1.0f};
    return {r.x, r.y, r.z};
}
vec3 dirOf(float yaw) { return {std::cos(yaw), std::sin(yaw), 0.0f}; }
float sm01(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
float hd(vec3 a, vec3 b) { return std::hypot(a.x - b.x, a.y - b.y); }
vec3 nrm(vec3 v, vec3 fb) {
    const float l = length(v);
    return l > 1e-5f ? v / l : fb;
}
float h01(u32 i) {
    i ^= i >> 16, i *= 0x7feb352du, i ^= i >> 15, i *= 0x846ca68bu, i ^= i >> 16;
    return (float)(i & 0xFFFFFF) / 16777216.0f;
}
void push(std::vector<VisibleMesh>& out, HotelL5::Part& p, const mat4& w, bool dynamic = true, float cullR = 0.0f, int material = -1) {
    VisibleMesh v;
    v.model = &p.model;
    v.meshId = &p.id;
    v.world = w;
    v.dynamic = dynamic;
    v.material = material;
    if (cullR > 0.0f) v.cullCenter = apply(w, vec3{0, 0, 0}), v.cullRadius = cullR;
    out.push_back(v);
}
u32 code(u32 kind, u32 a, u32 b, u32 c) { return (kind << 24) | ((a & 0xFF) << 16) | ((b & 0xFF) << 8) | (c & 0xFF); }
bool seenBy(const HotelL5::PlayerIn& p, world::World& w, vec3 feet, float h) {
    return p.alive && perceive(p.view, w, feet, h).level > 0.25f;
}
template <class PL>
bool seenAny(const PL& pl, world::World& w, vec3 feet, float h) {
    for (const auto& p : pl)
        if (seenBy(p, w, feet, h)) return true;
    return false;
}
// Saetze des Gastgebers (eigene Worte; die Seite: hoeflicher Gastgeber, sein Hotel, ein Geschaeft, nie wieder Angst)
struct HostLine {
    const char* sound;
    const char* sub;
    float secs;
};
constexpr HostLine kHostLines[] = {
    {"e18_line_0", "Eine ruhige Männerstimme, ganz nah: „Ah. Da sind Sie ja. Ich habe Sie erwartet.“", 4.0f},
    {"e18_line_1", "„Mein Hotel macht Ihnen Angst, nicht wahr? Das muss nicht sein.“", 4.0f},
    {"e18_line_2", "„Kommen Sie in mein Büro, wir plaudern ein wenig. Sie müssen nicht weitergehen.“", 4.5f},
    {"e18_line_3", "„Setzen Sie sich. Ein kleines Geschäft unter uns: Ich helfe Ihnen – Sie überlassen mir ein paar Dinge.“", 5.0f},
    {"e18_line_4", "„Geben Sie mir die Hand. Und Sie werden nie wieder Angst haben.“", 4.0f},
    {"e18_line_5", "„Sie gehen? Wie schade. Mein Hotel ist groß – wir sehen uns.“", 4.0f},
    {"e18_line_6", "„Abgemacht.“", 2.5f},
    {"e18_line_7", "Ganz nah an deinem Ohr: „Wir sind noch nicht fertig miteinander.“", 4.5f},
    {"e18_line_8", "„Sehen Sie nicht so genau hin. Die Bilder lügen.“", 3.5f},
};
}  // namespace

namespace hotel {
// Saetze und Laute der Samantha (Klang: synth_hotel.cpp). Eigene Worte; im Spiel heisst sie nur "die Katze" (E-039).
// Sie spricht mehrere Sprachen (Wiki): vor Sicht murmelt sie franzoesisch, italienisch, englisch - den Spieler spricht sie
// in seiner Sprache an (sie liest Gedanken - Auslegung).
const KinLine* samLine(int cue) {
    static const KinLine k[SAM_COUNT] = {
        {"sam_purr", nullptr, 0.55f, 8.0f},
        {"sam_murmur_0", "(Eine weiche Frauenstimme murmelt etwas auf Französisch.)", 0.6f, 18.0f, 2.6f},
        {"sam_murmur_1", "(Eine weiche Frauenstimme murmelt etwas auf Italienisch.)", 0.6f, 18.0f, 2.6f},
        {"sam_murmur_2", "(Eine weiche Frauenstimme, Englisch, fast ein Lied.)", 0.6f, 18.0f, 2.6f},
        {"sam_ask_0", "Die Katze spricht – ruhig, mit einer Frauenstimme: „Du riechst nach Angst. Hast du mir etwas Gutes mitgebracht?“", 0.9f, 16.0f, 5.0f,
         "Rat der Reisenden: Wer ihr begegnet, bringe gutes Fleisch mit. Höflich bleiben, sie nicht reizen."},
        {"sam_ask_1", "„Komm näher. Ich beiße nur die, die nichts mitbringen.“", 0.9f, 16.0f, 4.0f},
        {"sam_ask_2", "„Nichts? Gar nichts? Das ist … unhöflich.“", 0.9f, 16.0f, 3.5f},
        {"sam_thank_good", "„Oh. Das ist gutes Fleisch. Bleib. Hör zu.“", 0.9f, 14.0f, 3.5f},
        {"sam_thank_poor", "„Dosenfleisch? … Na schön.“", 0.9f, 14.0f, 3.0f},
        {"sam_read_good", nullptr, 0.9f, 14.0f},
        {"sam_read_poor", nullptr, 0.9f, 14.0f},
        {"sam_warn", "Sie faucht, das Maul reißt auf bis hinter die Augen: „Mit leeren Händen?“", 1.0f, 20.0f, 2.4f,
         "Ohne Fleisch: Abstand – schnell und weit."},
        {"sam_rude", "„Nimm das Licht aus meinem Gesicht. Das ist unhöflich.“", 0.9f, 14.0f, 3.0f},
        {"sam_scream", nullptr, 1.0f, 28.0f},
        {"sam_bite", nullptr, 1.0f, 10.0f},
        {"sam_eat", nullptr, 0.75f, 10.0f},
        {"sam_bye", "„Wir sehen uns. Ich finde dich – ich finde jeden.“", 0.85f, 16.0f, 3.5f},
    };
    return cue >= 0 && cue < SAM_COUNT ? &k[cue] : nullptr;
}
const KinLine* samLineBySound(const std::string& s) {
    for (int i = 0; i < SAM_COUNT; ++i)
        if (const KinLine* L = samLine(i); L && L->sound && s == L->sound) return L;
    return nullptr;
}

float meatQuality(const std::string& item) {
    if (item == "steak") return 1.0f;
    if (item == "dosenfleisch") return 0.3f;
    return 0.0f;
}
}  // namespace hotel

const std::vector<HotelL5::Spot>& HotelL5::spots(const std::string& set) const {
    if (set == "ants") return sAnts_;
    if (set == "woodlin") return sWood_;
    if (set == "watcher") return sWatch_;
    return sNone_;
}

// --- Aufbau ---------------------------------------------------------------------------------------------------------
void HotelL5::setup(const world::LevelDef& level, u64 seed) {
    cfg_ = Config{};
    st_ = Stats{};
    sAnts_.clear(), sWood_.clear(), sWatch_.clear(), col_.clear(), wl_.clear(), wa_.clear(), meat_.clear(), portraits_.clear();
    host_ = Host{};
    hasOffice_ = false;
    clock_ = 0.0, woodT_ = 60.0, woodCool_ = 0.0, hold_ = false;
    slowT_.fill(0.0f);
    rng_ = Rng(hash64((i64)seed, "hotel_l5", level.id.c_str()));
    const Json& root = level.apparitions;
    if (!root.isObject() || !level.mapped()) return;
    const Json& j = root["hotel"];
    if (!j.isObject()) return;
    cfg_.ants = j["ants"].asBool(false);
    cfg_.woodlins = j["woodlins"].asBool(false);
    cfg_.watchers = j["watchers"].asBool(false);
    cfg_.host = j["host"].asBool(false);
    cfg_.meat = j["meat"].asBool(false);
    cfg_.colonyChance = j["colony_chance"].asNumber(cfg_.colonyChance);
    cfg_.woodlinGroup = (int)j["woodlin_group"].asInt(cfg_.woodlinGroup);
    cfg_.woodlinEvery = j["woodlin_every"].asNumber(cfg_.woodlinEvery);
    cfg_.watcherCount = (int)j["watcher_count"].asInt(cfg_.watcherCount);
    cfg_.hostAfter = j["host_after"].asNumber(cfg_.hostAfter);
    cfg_.hostSanity = j["host_sanity"].asNumber(cfg_.hostSanity);
    cfg_.fearless = j["fearless"].asNumber(cfg_.fearless);
    const auto& sp = level.map->spawns;
    for (size_t i = 0; i < sp.size(); ++i) {
        const world::MapSpawn& s = sp[i];
        if (s.kind != "trace") continue;
        Spot p;
        p.pos = {(float)s.x, (float)s.y, (float)s.z};
        p.yaw = (float)s.yaw;
        p.tags = s.tags;
        for (const std::string& t : s.tags) {
            if (t == "side-1") p.side = -1;
            if (t.size() > 1 && t[0] == 'o')  // der Punkt liegt davor (in einem Raum der Karte): so weit zurueck liegt der Ort
                p.pos = p.pos - dirOf(p.yaw) * (float)std::atof(t.c_str() + 1);
        }
        if (s.set == "ants") sAnts_.push_back(p);
        else if (s.set == "woodlin") sWood_.push_back(p);
        else if (s.set == "watcher") sWatch_.push_back(p);
        else if (s.set == "office") office_ = p, hasOffice_ = true;
        else if (s.set == "meat" && cfg_.meat) {
            Meat m;
            m.pos = p.pos, m.yaw = p.yaw;
            m.item = s.hasTag("poor") ? "dosenfleisch" : "steak";
            m.id = std::format("l5meat{}", i);
            meat_.push_back(m);
        }
    }
    for (const auto& o : level.map->objects)
        if (o.model == "painting_portrait") portraits_.push_back({{(float)o.x, (float)o.y, (float)o.z}, (float)o.yaw});
    // Kolonien: sie wandern (Wiki) - je Lauf wohnt nur in einem Teil der dunklen Ecken eine
    if (cfg_.ants)
        for (size_t k = 0; k < sAnts_.size(); ++k) {
            if (rng_.random() > cfg_.colonyChance) continue;
            Colony c;
            c.pos = sAnts_[k].pos, c.yaw = sAnts_[k].yaw, c.spot = (int)k;
            const int n = (int)rng_.randint(120, 170);  // Runde 4: dichter
            for (int i = 0; i < n; ++i) {
                Ball b;
                const float a = c.yaw + (float)rng_.uniform(-0.8, 0.8);
                // Runde 2: der Haufen war zu klein und licht (einzelne graue Kugeln) - breiter, hoeher, dicht gedraengt
                const float R = 0.42f, rr = R * std::pow((float)rng_.random(), 0.7f) * (0.25f + 0.75f * (float)rng_.random());
                b.r = 0.012f + 0.018f * (float)std::pow(rng_.random(), 1.3);  // 2-6 cm
                const float dome = 0.38f * (1.0f - (rr / R) * (rr / R));
                b.home = c.pos + dirOf(a) * rr + vec3{0, 0, b.r + dome * (float)rng_.random()};
                b.p = b.home;
                b.ph = (float)rng_.uniform(0.0, kTau);
                b.hMax = 0.2f + 1.15f * (float)std::pow(rng_.random(), 1.4);
                b.model = (int)rng_.randint(0, 3);
                c.balls.push_back(b);
            }
            col_.push_back(std::move(c));
        }
    if (cfg_.woodlins) wl_.resize((size_t)std::max(0, cfg_.woodlinGroup));
    if (cfg_.watchers && !sWatch_.empty()) {  // feste Plaetze je Lauf (Host und Gaeste gleich), mindestens 12 m auseinander
        std::vector<int> idx((size_t)sWatch_.size());
        for (size_t i = 0; i < idx.size(); ++i) idx[i] = (int)i;
        for (size_t i = idx.size(); i > 1; --i) std::swap(idx[i - 1], idx[(size_t)rng_.randint(0, (i64)i - 1)]);
        for (int s : idx) {
            if ((int)wa_.size() >= cfg_.watcherCount) break;
            bool ok = true;
            for (const Watcher& o : wa_) ok = ok && hd(sWatch_[(size_t)o.spot].pos, sWatch_[(size_t)s].pos) > 12.0f;
            if (!ok) continue;
            Watcher a;
            placeWatcher(a, s);
            wa_.push_back(a);
        }
    }
    setupParts();
    log::info("Hotel: {} Kolonien ({} Ecken), {} Woodlins ({} Moebel), {} Watchers ({} Tueren), Buero {}, {} Fleisch, {} Bildnisse",
              col_.size(), sAnts_.size(), wl_.size(), sWood_.size(), wa_.size(), sWatch_.size(), hasOffice_ ? "ja" : "nein", meat_.size(),
              portraits_.size());
}

void HotelL5::placeWatcher(Watcher& a, int spot) {
    a.spot = spot;
    a.side = spot >= 0 && spot < (int)sWatch_.size() ? sWatch_[(size_t)spot].side : 1;
    a.state = 0, a.t = 0.0, a.peek = 0.0f, a.arm = 0.0f, a.grip = 0.0f, a.cool = 0.0, a.hiddenT = 0.0, a.told = false;
    a.tilt = 1.0f + 0.35f * h01((u32)spot * 977u + 5u);
}

void HotelL5::setupParts() {
    for (int k = 0; k < 4; ++k) antPart_[k].model = std::format("prop:ant_ball@{}", k);
    for (Woodlin& w : wl_) {
        w.body.model = "prop:wl_body", w.jaw_.model = "prop:wl_jaw";
        for (int l = 0; l < 4; ++l) w.legA[l].model = "prop:wl_leg_a", w.legB[l].model = "prop:wl_leg_b";
    }
    for (Watcher& a : wa_) {
        a.head.model = "prop:wa_head", a.face.model = "prop:wa_face";
        for (Part& e : a.eye) e.model = "prop:wa_eye";
        a.hand[0].model = a.side > 0 ? "prop:wa_hand@0" : "prop:wa_hand@1";
        a.hand[1].model = a.side > 0 ? "prop:wa_hand@0" : "prop:wa_hand@1";
        a.armA.model = "prop:wa_arm_a", a.armB.model = "prop:wa_arm_b", a.claw.model = "prop:wa_claw", a.body.model = "prop:wa_body";
    }
    host_.armA.model = "prop:e18_arm_a", host_.armB.model = "prop:e18_arm_b", host_.armC.model = "prop:e18_arm_c", host_.hand.model = "prop:e18_hand";
    for (auto& pe : host_.eyes) pe[0].model = pe[1].model = "prop:e18_eye";
}

// --- Bild fuer Bild -------------------------------------------------------------------------------------------------
void HotelL5::update(double dt, const std::array<PlayerIn, kPlayers>& pl, bool auth, world::World& w, std::vector<Event>& out) {
    if (!any()) return;
    clock_ += dt;
    lastPl_ = pl;
    lastW_ = &w;
    for (float& s : slowT_) s = std::max(0.0f, s - (float)dt);
    if (cfg_.ants) updateAnts(dt, pl, auth, out);
    if (cfg_.woodlins) updateWood(dt, pl, auth, w, out);
    if (cfg_.watchers) updateWatch(dt, pl, auth, w, out);
    if (cfg_.host && hasOffice_) updateHost(dt, pl, auth, w, out);
}

// --- Kolonien (Entity 89) --------------------------------------------------------------------------------------------
void HotelL5::triggerColony(Colony& c, int k, int player, std::vector<Event>& out, bool net) {
    c.state = 2, c.t = 0.0, c.target = player, ++c.triggers;
    ++st_.antTriggers;
    // gut acht von zehn kommen (der Rest bleibt als Haufen in der Ecke)
    for (size_t i = 0; i < c.balls.size(); ++i) {
        Ball& b = c.balls[i];
        if (b.state != 0) continue;
        if (h01((u32)i * 7919u + (u32)c.triggers * 131u) < 0.17f) continue;
        b.state = 1, b.target = player;
        b.ang = h01((u32)i * 31u + (u32)c.triggers) * kTau;
    }
    Event s{Event::SoundAt};
    s.text = "ant_swarm", s.pos = c.pos + vec3{0, 0, 0.1f}, s.value = 0.9f;
    out.push_back(s);
    if (net) {
        Event n{Event::Net};
        n.code = code(1, (u32)k, (u32)player, 0), n.pos = c.pos;
        out.push_back(n);
    }
    log::info("Hotel: Kolonie {} klettert an Spieler {} hoch", k, player);
}

void HotelL5::detach(Colony& c, int player, int n) {
    for (Ball& b : c.balls) {
        if (n <= 0) break;
        if (b.state == 2 && b.target == player) b.state = 3, b.v = {0, 0, 0}, --n;
    }
    for (Ball& b : c.balls)  // was noch unterwegs ist, kehrt um, wenn nichts mehr am Spieler haengt
        if (b.state == 1 && b.target == player && n > 0) b.state = 3, --n;
}

int HotelL5::antsOn(int player) const {
    int n = 0;
    for (const Colony& c : col_)
        for (const Ball& b : c.balls) n += b.state == 2 && b.target == player ? 1 : 0;
    return n;
}

float HotelL5::antsCrawl(int player) const { return std::clamp((float)antsOn(player) / 60.0f, 0.0f, 1.0f); }

bool HotelL5::pickAnts(int player, std::vector<Event>& out) {
    for (size_t k = 0; k < col_.size(); ++k) {
        Colony& c = col_[k];
        int on = 0;
        for (const Ball& b : c.balls) on += b.state == 2 && b.target == player ? 1 : 0;
        if (!on) continue;
        const int n = std::min(on, 14);
        detach(c, player, n);
        ++st_.antPicks;
        Event n2{Event::Net};
        n2.code = code(3, (u32)k, (u32)player, (u32)n), n2.pos = c.pos;
        out.push_back(n2);
        Event s{Event::Sound2d};
        s.text = "ant_pick", s.player = player, s.value = 0.8f;
        out.push_back(s);
        if (on - n <= 0) {
            Event t{Event::Toast};
            t.player = player, t.text = "Das letzte Büschel ist ab. Rote Flecken bleiben auf der Haut.", t.value = 2.6f;
            out.push_back(t);
        }
        return true;
    }
    return false;
}

void HotelL5::updateAnts(double dt, const std::array<PlayerIn, kPlayers>& pl, bool auth, std::vector<Event>& out) {
    const float fdt = (float)dt;
    static std::array<int, kPlayers> told{};
    for (size_t k = 0; k < col_.size(); ++k) {
        Colony& c = col_[k];
        c.t += dt;
        c.cool = std::max(0.0, c.cool - dt);
        // naechster Spieler
        int near = -1;
        float nd = 1e9f;
        for (int i = 0; i < kPlayers; ++i) {
            if (!pl[(size_t)i].alive) continue;
            const float d = hd(pl[(size_t)i].feet, c.pos);
            if (d < nd && std::fabs(pl[(size_t)i].feet.z - c.pos.z) < 1.2f) nd = d, near = i;
        }
        const float rouse = nd < 2.4f ? 1.0f : 0.0f;
        if (rouse > c.rouse + 0.5f && c.state == 0) {
            c.rustleT -= dt;
            if (c.rustleT <= 0.0) {
                c.rustleT = 6.0;
                Event s{Event::SoundAt};
                s.text = "ant_rustle", s.pos = c.pos, s.value = 0.5f;
                out.push_back(s);
            }
        }
        c.rouse += (rouse - c.rouse) * std::min(1.0f, fdt * 2.0f);
        // Ausloeser: naeher als etwa 0,9 m an den Haufen (3 Fuss, Wiki) - der Haufen reicht 0,35 m in den Raum
        if (auth && !hold_ && c.state != 2 && c.cool <= 0.0 && near >= 0 && nd < 0.9f + 0.25f) triggerColony(c, (int)k, near, out, true);
        int moving = 0;
        for (size_t i = 0; i < c.balls.size(); ++i) {
            Ball& b = c.balls[i];
            b.ph += fdt * (1.5f + 3.0f * c.rouse);
            switch (b.state) {
                case 0: {  // im Haufen: Gedraenge, unruhig, wenn jemand nah ist
                    const float j = 0.0025f + 0.006f * c.rouse;
                    b.p = b.home + vec3{std::sin(b.ph * 1.3f) * j, std::cos(b.ph * 1.1f + 1.0f) * j, std::max(-0.3f * j, std::sin(b.ph * 0.9f) * j)};
                    break;
                }
                case 1: {  // stroemt ueber den Boden zum Ziel
                    ++moving;
                    const PlayerIn* P = b.target >= 0 && b.target < kPlayers && pl[(size_t)b.target].alive ? &pl[(size_t)b.target] : nullptr;
                    if (!P || hd(P->feet, c.pos) > 3.4f) {
                        b.state = 3;
                        break;
                    }
                    const vec3 goal = P->feet + dirOf(b.ang) * 0.13f;
                    vec3 d = goal - b.p;
                    d.z = 0.0f;
                    const float L = length(d), sp = (2.0f + 0.8f * h01((u32)i * 13u)) * fdt;
                    if (L < 0.06f) {
                        b.state = 2, b.h = b.r + 0.01f;
                        break;
                    }
                    b.p = b.p + d / L * std::min(sp, L);
                    b.p.z = P->feet.z + b.r + 0.012f * std::fabs(std::sin(b.ph * 4.0f));  // rollen, huepfen
                    break;
                }
                case 2: {  // am Koerper: klettern, krabbeln, bleiben
                    ++moving;
                    const PlayerIn* P = b.target >= 0 && b.target < kPlayers ? &pl[(size_t)b.target] : nullptr;
                    if (!P || !P->alive) {
                        b.state = 3;
                        break;
                    }
                    b.h = std::min(b.hMax, b.h + fdt * (0.18f + 0.18f * h01((u32)i * 3u + 1u)));
                    b.ang += fdt * 0.9f * std::sin(b.ph * 0.37f + (float)i);
                    // Koerper als Saeulen: zwei Beine bis 0,85 m, darueber der Rumpf
                    const vec3 fw = nrm(vec3{P->look.x, P->look.y, 0.0f}, {1, 0, 0}), lf{-fw.y, fw.x, 0.0f};
                    vec3 axis = P->feet;
                    float rad = 0.17f;
                    if (b.h < 0.85f) axis = axis + lf * ((i % 2) ? 0.09f : -0.09f), rad = 0.065f + 0.02f * (b.h / 0.85f);
                    b.p = axis + vec3{std::cos(b.ang), std::sin(b.ang), 0.0f} * (rad + b.r * 0.6f) + vec3{0, 0, b.h};
                    break;
                }
                case 3: {  // abgezupft/zurueck: faellt, rollt in die Ecke
                    vec3 d = b.home - b.p;
                    if (b.p.z > c.pos.z + b.r + 0.01f && hd(b.p, b.home) > 0.5f) {
                        b.v.z -= 9.8f * fdt;
                        b.p.z = std::max(c.pos.z + b.r, b.p.z + b.v.z * fdt);
                        break;
                    }
                    const float L = length(d);
                    if (L < 0.03f) {
                        b.state = 0, b.target = -1, b.p = b.home;
                        break;
                    }
                    ++moving;
                    b.p = b.p + d / L * std::min(L, 0.9f * fdt);
                    break;
                }
                default: break;
            }
        }
        if (c.state == 2 && moving == 0 && c.t > 3.0) c.state = 0, c.cool = 12.0, c.target = -1;
    }
    // Wirkung (Host): leichter Dauerschaden wie Blutegel, nie toedlich; das Krabbeln zehrt am Verstand
    if (!auth) return;
    for (int i = 0; i < kPlayers; ++i) {
        const int n = antsOn(i);
        if (!n || !pl[(size_t)i].alive) {
            if (!n) told[(size_t)i] = 0;
            continue;
        }
        if (pl[(size_t)i].health > cfg_.antFloor) {
            Event h{Event::Hurt};
            h.player = i, h.value = (float)(dt * cfg_.antBite * std::min(n, 80)), h.text = "ants";  // hoechstens 80 beissen zugleich
            out.push_back(h);
        }
        Event s{Event::Sanity};
        s.player = i, s.value = (float)(dt * 0.006 * n);
        out.push_back(s);
        if (!told[(size_t)i]) {
            told[(size_t)i] = 1;
            Event t{Event::Toast};
            t.player = i, t.value = 4.0f;
            t.text = "Etwas Schwarzes, Flauschiges krabbelt an dir hoch und beißt sich fest – abzupfen (Benutzen-Taste)!";
            out.push_back(t);
        }
    }
}

// --- Woodlins (nur Name - Auslegung) ---------------------------------------------------------------------------------
void HotelL5::woodGroup(const std::array<PlayerIn, kPlayers>& pl, world::World& w, std::vector<Event>& out, bool net, int forceTarget) {
    // Ziel: ein Spieler in einem Gang der Haupthalle (oder erzwungen)
    int tgt = forceTarget;
    if (tgt < 0)
        for (int i = 0; i < kPlayers && tgt < 0; ++i)
            if (pl[(size_t)i].alive && pl[(size_t)i].corridor) tgt = i;
    if (tgt < 0 || sWood_.empty()) return;
    const PlayerIn& P = pl[(size_t)tgt];
    std::vector<int> cand;
    for (size_t s = 0; s < sWood_.size(); ++s) {
        const float d = hd(sWood_[s].pos, P.feet);
        if (d < (forceTarget >= 0 ? 3.0f : 7.0f) || d > 20.0f || std::fabs(sWood_[s].pos.z - P.feet.z) > 1.5f) continue;
        if (forceTarget < 0 && seenAny(pl, w, sWood_[s].pos, 0.45f)) continue;
        cand.push_back((int)s);
    }
    for (size_t i = cand.size(); i > 1; --i) std::swap(cand[i - 1], cand[(size_t)rng_.randint(0, (i64)i - 1)]);
    int made = 0;
    for (size_t k = 0; k < wl_.size() && made < (int)cand.size(); ++k) {
        Woodlin& a = wl_[k];
        const Spot& s = sWood_[(size_t)cand[(size_t)made]];
        a.state = 1, a.spot = cand[(size_t)made], a.target = tgt, a.pos = s.pos, a.from = a.to = s.pos;
        a.yaw = s.yaw, a.upright = 1.0f, a.t = 0.0, a.unseen = 0.0, a.seenT = 0.0, a.clickT = rng_.uniform(1.0, 4.0);
        ++made;
        if (net) {
            Event n{Event::Net};
            n.code = code(4, (u32)k, 1, (u32)tgt), n.pos = a.pos;
            out.push_back(n);
        }
    }
    if (made) log::info("Hotel: {} Woodlins in den Moebeln (Ziel Spieler {})", made, tgt);
}

void HotelL5::updateWood(double dt, const std::array<PlayerIn, kPlayers>& pl, bool auth, world::World& w, std::vector<Event>& out) {
    if (auth && !hold_) {
        bool active = false;
        for (const Woodlin& a : wl_) active = active || a.state != 0;
        woodCool_ = std::max(0.0, woodCool_ - dt);
        if (!active && woodCool_ <= 0.0) {
            woodT_ -= dt;
            if (woodT_ <= 0.0) {
                woodT_ = cfg_.woodlinEvery * rng_.uniform(0.6, 1.4);
                woodGroup(pl, w, out, true);
            }
        }
    }
    auto netState = [&](size_t k, int state) {
        if (!auth) return;
        Event n{Event::Net};
        n.code = code(4, (u32)k, (u32)state, (u32)std::max(0, wl_[k].target)), n.pos = state == 2 || state == 5 ? wl_[k].to : wl_[k].pos;
        out.push_back(n);
    };
    for (size_t k = 0; k < wl_.size(); ++k) {
        Woodlin& a = wl_[k];
        if (a.state == 0) continue;
        a.t += dt;
        const PlayerIn* P = a.target >= 0 && a.target < kPlayers && pl[(size_t)a.target].alive ? &pl[(size_t)a.target] : nullptr;
        const bool seen = seenAny(pl, w, a.pos, 0.42f);
        if (seen) a.seenT += dt, a.unseen = 0.0;
        else a.unseen += dt, a.seenT = 0.0;
        // Bewegung (alle Rechner): huschen, fliehen
        if (a.state == 2 || a.state == 5) {
            const float u = (float)std::min(1.0, a.t / std::max(0.05, a.dur));
            a.pos = a.from + (a.to - a.from) * u;
            a.gait += (float)dt * (a.state == 5 ? 26.0f : 18.0f);
            const vec3 d = a.to - a.from;
            if (length(d) > 0.05f) a.yaw = std::atan2(d.y, d.x);
            a.upright += (0.0f - a.upright) * std::min(1.0f, (float)dt * 10.0f);
            if (u >= 1.0f) {
                if (a.state == 5) a.state = 0;
                else a.state = 1, a.t = 0.0;
            }
        } else {
            a.upright += ((a.state == 1 ? 1.0f : 0.25f) - a.upright) * std::min(1.0f, (float)dt * 6.0f);
        }
        a.jaw += ((a.state == 3 || a.state == 4 ? 1.0f : 0.0f) - a.jaw) * std::min(1.0f, (float)dt * 12.0f);
        // Klacken im Holz vor Sicht
        a.clickT -= dt;
        if (a.clickT <= 0.0 && !seen) {
            a.clickT = rng_.uniform(2.5, 6.0);
            Event s{Event::SoundAt};
            s.text = std::format("wl_click_{}", rng_.randint(0, 2)), s.pos = a.pos + vec3{0, 0, 0.2f}, s.value = 0.55f;
            out.push_back(s);
        }
        // reglos: ungesehen dreht es sich langsam zum Ziel - beim naechsten Blick sieht es einen an (alle Rechner)
        if (a.state == 1 && !seen && P && !hold_) {
            const float want = std::atan2(P->feet.y - a.pos.y, P->feet.x - a.pos.x);
            a.yaw += std::clamp(std::remainder(want - a.yaw, kTau), -(float)dt * 2.5f, (float)dt * 2.5f);
        }
        if (!auth || hold_) continue;
        if (!P) {
            if (!seen) a.state = 0, netState(k, 0);
            continue;
        }
        const float dP = hd(a.pos, P->feet);
        switch (a.state) {
            case 1: {
                if (dP > 28.0f && !seen) {
                    a.state = 0, netState(k, 0);
                    break;
                }
                // ins Gesicht gesehen und ganz nah: verschwindet ins Holz
                const vec3 toA = nrm(a.pos + vec3{0, 0, 0.3f} - P->eye, {1, 0, 0});
                if (dP < 0.9f && dot(toA, P->look) > 0.75f) {
                    a.from = a.pos, a.to = sWood_[(size_t)std::max(0, a.spot)].pos, a.t = 0.0;
                    if (hd(a.to, P->feet) < 2.0f) a.to = a.pos - nrm(P->feet - a.pos, {1, 0, 0}) * 2.5f;
                    a.dur = std::max(0.25, (double)length(a.to - a.from) / 3.2), a.state = 5;
                    netState(k, 5);
                    break;
                }
                if (dP < 1.4f && a.unseen > 0.5) {  // Ankuendigung: Holz knarrt hinter dem Spieler
                    a.state = 3, a.t = 0.0;
                    Event s{Event::SoundAt};
                    s.text = "wl_creak", s.pos = a.pos + vec3{0, 0, 0.2f}, s.value = 1.0f;
                    out.push_back(s);
                    Event t{Event::Toast};
                    t.player = a.target, t.text = "Dicht hinter dir knarrt Holz.", t.value = 1.6f;
                    out.push_back(t);
                    netState(k, 3);
                    break;
                }
                if (a.unseen > 0.9 + 0.9 * h01((u32)k * 17u + (u32)(clock_ * 3.0))) {
                    // ein Stueck naeher - hinter den Spieler, nie naeher als 1 m; hoechstens 4 m je Satz
                    const vec3 back = nrm(vec3{-P->look.x, -P->look.y, 0.0f}, {1, 0, 0});
                    vec3 goal = P->feet + back * 1.15f + vec3{back.y, -back.x, 0.0f} * ((k % 2) ? 0.5f : -0.5f);
                    vec3 d = goal - a.pos;
                    d.z = 0.0f;
                    const float L = length(d);
                    if (L < 0.3f) break;
                    vec3 to = a.pos + d / L * std::min(L, 4.0f);
                    const world::Tile* t = w.tileIfLoaded(ifloor(to.x), ifloor(to.y));
                    if (!t || !t->open() || !clearSight(w, a.pos + vec3{0, 0, 0.25f}, to + vec3{0, 0, 0.25f})) {
                        a.unseen = 0.0;
                        break;
                    }
                    to.z = t->floor;
                    a.from = a.pos, a.to = to, a.t = 0.0, a.dur = std::max(0.2, (double)length(to - a.pos) / 1.9), a.state = 2;
                    ++st_.woodMoves;
                    netState(k, 2);
                }
                break;
            }
            case 2:
                if (seen && a.t > 0.12) {  // erwischt: erstarrt mitten im Lauf (auf dem Teppich)
                    a.state = 1, a.t = 0.0, a.to = a.pos;
                    netState(k, 1);
                }
                break;
            case 3:
                if (seen && a.seenT > 0.2) {  // umgedreht: erstarrt
                    a.state = 1, a.t = 0.0;
                    netState(k, 1);
                    break;
                }
                if (a.t > 0.7) {
                    a.state = 4, a.t = 0.0;
                    a.yaw = std::atan2(P->feet.y - a.pos.y, P->feet.x - a.pos.x);
                    if (dP < 1.9f) {
                        ++st_.woodBites;
                        Event h{Event::Hurt};
                        h.player = a.target, h.value = (float)cfg_.woodlinBite, h.text = "woodlin";
                        out.push_back(h);
                        Event sl{Event::Slow};
                        sl.player = a.target, sl.value = 2.2f;
                        out.push_back(sl);
                        Event s{Event::SoundAt};
                        s.text = "wl_bite", s.pos = P->feet + vec3{0, 0, 0.15f}, s.value = 1.0f;
                        out.push_back(s);
                        Event t{Event::Toast};
                        t.player = a.target, t.text = "Etwas beißt dir in den Knöchel – Holzsplitter stecken in der Haut.", t.value = 2.6f;
                        out.push_back(t);
                    }
                    netState(k, 4);
                }
                break;
            case 4:
                if (a.t > 0.35) {  // alle verschwinden ins Holz
                    for (size_t j = 0; j < wl_.size(); ++j) {
                        Woodlin& b = wl_[j];
                        if (b.state == 0) continue;
                        int best = -1;
                        float bd = 1e9f;
                        for (size_t s = 0; s < sWood_.size(); ++s) {
                            const float dd = hd(sWood_[s].pos, b.pos);
                            if (dd < bd && hd(sWood_[s].pos, P->feet) > 2.5f) bd = dd, best = (int)s;
                        }
                        b.from = b.pos, b.to = best >= 0 ? sWood_[(size_t)best].pos : b.pos + nrm(b.pos - P->feet, {1, 0, 0}) * 3.0f;
                        b.t = 0.0, b.dur = std::max(0.25, (double)length(b.to - b.from) / 3.4), b.state = 5;
                        netState(j, 5);
                    }
                    woodCool_ = rng_.uniform(120.0, 200.0);
                }
                break;
            default: break;
        }
    }
}

// --- Watchers (nur Name; Level 43: selten, feindselig - Auslegung) ----------------------------------------------------
void HotelL5::updateWatch(double dt, const std::array<PlayerIn, kPlayers>& pl, bool auth, world::World& w, std::vector<Event>& out) {
    for (size_t k = 0; k < wa_.size(); ++k) {
        Watcher& a = wa_[k];
        if (a.spot < 0) continue;
        const Spot& S = sWatch_[(size_t)a.spot];
        const vec3 out_ = dirOf(S.yaw), along{-out_.y, out_.x, 0.0f};
        const vec3 headAt = S.pos + along * ((float)a.side * 0.3f) - out_ * 0.1f + vec3{0, 0, 1.85f};  // Kopf, wenn er lugt
        a.t += dt;
        a.cool = std::max(0.0, a.cool - dt);
        // naechster Spieler mit Sicht auf die Tueroeffnung
        int tgt = -1;
        float td = 1e9f;
        for (int i = 0; i < kPlayers; ++i) {
            const PlayerIn& P = pl[(size_t)i];
            if (!P.alive) continue;
            const float d = length(P.eye - headAt);
            if (d < td && d < 16.0f && dot(P.eye - S.pos, out_) > 0.2f && clearSight(w, headAt + out_ * 0.15f, P.eye)) td = d, tgt = i;
        }
        // Augen folgen dem Ziel, blinzeln nacheinander (rollen nach oben)
        if (tgt >= 0) a.look = nrm(pl[(size_t)tgt].eye - headAt, a.look);
        a.blinkT -= dt;
        if (a.blinkT <= 0.0) a.blinkT = 0.18, a.blinkEye = a.blinkEye + 1 >= 5 ? -1 : a.blinkEye + 1, a.blink = 1.0f;
        if (a.blinkEye < 0 && a.blinkT > 0.17) a.blinkT = rng_.uniform(2.5, 6.0);
        a.blink = std::max(0.0f, a.blink - (float)dt * 6.0f);
        // Ansehen: der Spieler blickt direkt in die Oeffnung (und sieht ihn: Licht oder nah)
        bool looked = false;
        for (const PlayerIn& P : pl) {
            if (!P.alive) continue;
            const vec3 d = headAt - P.eye;
            const float L = length(d);
            if (L < 14.0f && dot(d / L, P.look) > 0.93f && seenBy(P, w, headAt - vec3{0, 0, 0.4f}, 0.5f)) looked = true;
        }
        a.seenT = looked ? a.seenT + dt : std::max(0.0, a.seenT - dt);
        const float targetPeek = a.state == 1 ? 1.0f : (a.state == 3 || a.state == 4 ? 0.8f : 0.0f);
        a.peek += (targetPeek - a.peek) * std::min(1.0f, (float)dt * (a.state == 2 ? 0.7f : 1.6f));
        a.grip += ((a.state == 3 ? 1.0f : 0.0f) - a.grip) * std::min(1.0f, (float)dt * 8.0f);
        a.arm += ((a.state == 4 ? 1.0f : 0.0f) - a.arm) * std::min(1.0f, (float)dt * (a.state == 4 ? 14.0f : 5.0f));
        // Sanft zehren, solange er lugt (das Gefuehl, beobachtet zu werden)
        if (auth && tgt >= 0 && a.peek > 0.5f && td < 12.0f) {
            Event s{Event::Sanity};
            s.player = tgt, s.value = (float)(dt * 0.25);
            out.push_back(s);
            if (!a.told) {
                a.told = true;
                ++st_.waPeeks;
                Event t{Event::Toast};
                t.player = tgt, t.text = "Du fühlst dich beobachtet.", t.value = 2.2f;
                out.push_back(t);
            }
        }
        if (!auth || hold_) continue;
        auto netState = [&](int st) {
            Event n{Event::Net};
            n.code = code(5, (u32)k, (u32)st, (u32)a.spot), n.pos = S.pos;
            out.push_back(n);
        };
        // wer in die Tuer tritt: er zieht sich zurueck und sucht sich spaeter eine andere Tuer
        bool inDoor = false;
        int nearP = -1;
        float nearD = 1e9f;
        for (int i = 0; i < kPlayers; ++i) {
            const PlayerIn& P = pl[(size_t)i];
            if (!P.alive) continue;
            const float d = hd(P.feet, S.pos);
            inDoor = inDoor || d < 0.75f;
            if (d < nearD) nearD = d, nearP = i;
        }
        switch (a.state) {
            case 0:
                if (inDoor || hd(S.pos, nest_) < 2.5f) {
                    a.state = 5, a.hiddenT = 0.0, netState(5);
                    break;
                }
                if (tgt >= 0 && !looked && td < 14.0f && a.t > 1.0) a.state = 1, a.t = 0.0, netState(1);
                break;
            case 1:
                if (inDoor) {
                    a.state = 5, a.hiddenT = 0.0, netState(5);
                    break;
                }
                if (a.seenT > 1.2 || tgt < 0) {
                    a.state = 2, a.t = 0.0, netState(2);
                    break;
                }
                // Hieb: wer dicht vorbeigeht und ihm den Ruecken zukehrt
                if (nearP >= 0 && nearD < 1.6f && a.cool <= 0.0) {
                    const PlayerIn& P = pl[(size_t)nearP];
                    const vec3 toW = nrm(headAt - P.eye, out_);
                    if (dot(toW, P.look) < 0.3f) {
                        a.state = 3, a.t = 0.0, a.target = nearP;
                        Event s{Event::SoundAt};
                        s.text = "wa_crack", s.pos = headAt, s.value = 1.0f;
                        out.push_back(s);
                        netState(3);
                    }
                }
                break;
            case 2:
                if (a.t > 3.0 && !looked) a.state = 0, a.t = 0.0, netState(0);
                break;
            case 3:
                if (a.t > 0.8) {
                    a.state = 4, a.t = 0.0;
                    const PlayerIn* P = a.target >= 0 && a.target < kPlayers && pl[(size_t)a.target].alive ? &pl[(size_t)a.target] : nullptr;
                    if (P) a.armTo = P->eye - vec3{0, 0, 0.45f};
                    if (P && hd(P->feet, S.pos) < 2.1f) {
                        ++st_.waStrikes;
                        Event h{Event::Hurt};
                        h.player = a.target, h.value = (float)cfg_.watcherStrike, h.text = "watcher";
                        out.push_back(h);
                        Event s{Event::SoundAt};
                        s.text = "wa_strike", s.pos = P->eye - vec3{0, 0, 0.4f}, s.value = 1.0f;
                        out.push_back(s);
                        Event t{Event::Toast};
                        t.player = a.target, t.text = "Lange Finger reißen dir über den Rücken.", t.value = 2.4f;
                        out.push_back(t);
                    }
                    netState(4);
                }
                break;
            case 4:
                if (a.t > 0.6) a.state = 2, a.t = 0.0, a.cool = 40.0, netState(2);
                break;
            case 5: {  // fort - spaeter an einer anderen Tuer
                a.hiddenT += dt;
                if (a.hiddenT < 30.0 || sWatch_.size() < 2) break;
                for (int tries = 0; tries < 12; ++tries) {
                    const int s = (int)rng_.randint(0, (i64)sWatch_.size() - 1);
                    bool ok = s != a.spot && hd(sWatch_[(size_t)s].pos, nest_) > 2.5f && !seenAny(pl, w, sWatch_[(size_t)s].pos, 2.0f);
                    for (const Watcher& o : wa_) ok = ok && (&o == &a || o.spot != s);
                    if (!ok) continue;
                    placeWatcher(a, s);
                    a.hand[0].model = a.hand[1].model = a.side > 0 ? "prop:wa_hand@0" : "prop:wa_hand@1";
                    a.hand[0].id = a.hand[1].id = -2;
                    netState(0);
                    break;
                }
                break;
            }
            default: break;
        }
    }
}

// --- Der Gastgeber (Entity 18) ---------------------------------------------------------------------------------------
void HotelL5::hostLine(int line, const std::array<PlayerIn, kPlayers>& pl, std::vector<Event>& out) {
    if (line < 0 || line >= (int)std::size(kHostLines) || host_.chosen < 0) return;
    const PlayerIn& P = pl[(size_t)std::clamp(host_.chosen, 0, kPlayers - 1)];
    // aus dem naechsten Bildnis (er benutzt die Bilder des Hotels), sonst ganz nah
    vec3 at = P.eye + P.look * 1.5f;
    float best = 14.0f;
    for (const Portrait& pt : portraits_)
        if (const float d = length(pt.pos + vec3{0, 0, 0.13f} - P.eye); d < best) best = d, at = pt.pos + dirOf(pt.yaw) * 0.05f + vec3{0, 0, 0.13f};
    if (host_.phase >= 2) at = office_.pos + vec3{0, 0, 1.1f} - dirOf(office_.yaw) * 0.4f;  // im Buero: aus dem Sessel
    Event s{Event::SoundAt};
    s.text = kHostLines[line].sound, s.pos = at, s.value = 1.0f, s.player = host_.chosen;
    out.push_back(s);
    Event t{Event::Toast};
    t.player = host_.chosen, t.text = kHostLines[line].sub, t.value = kHostLines[line].secs;
    out.push_back(t);
    ++st_.hostLines;
}

void HotelL5::hostPhaseSet(int phase, std::vector<Event>& out, bool net) {
    host_.phase = phase, host_.t = 0.0, host_.lineT = phase == 2 ? 0.8 : 0.5, host_.line = 0, host_.outT = 0.0;
    if (phase == 3 && host_.fearUntil < 0.0) host_.fearUntil = clock_ + cfg_.fearless;
    // Saetze beim Wechsel - nur der Gewaehlte hoert ihn (auf seinem eigenen Rechner: Platz 0)
    if (host_.chosen == 0) {
        if (phase == 3) hostLine(6, lastPl_, out);
        if (phase == 4) hostLine(7, lastPl_, out);
        if (phase == 5) hostLine(5, lastPl_, out);
    }
    if (net) {
        Event n{Event::Net};
        n.code = code(6, (u32)std::max(0, host_.chosen), (u32)phase, 0), n.pos = office_.pos;
        out.push_back(n);
    }
    log::info("Hotel: Gastgeber - Phase {} (Spieler {})", phase, host_.chosen);
}

bool HotelL5::handAt(vec3& pos) const {
    if (!hasOffice_ || host_.phase != 2 || host_.arm < 0.6f) return false;
    pos = office_.pos + dirOf(office_.yaw) * (0.15f + 0.55f * host_.arm) + vec3{0, 0, 0.95f};
    return true;
}

double HotelL5::fearless(int player) const {
    if (host_.chosen != player || host_.fearUntil < 0.0) return 0.0;
    return std::max(0.0, host_.fearUntil - clock_);
}

bool HotelL5::shake(int player, std::vector<Event>& out) {
    if (host_.chosen != player || host_.phase != 2) return false;
    host_.dealt = true;
    ++st_.deals;
    host_.fearUntil = clock_ + cfg_.fearless;
    Event d{Event::Deal};
    d.player = player, d.value = 2.0f;  // die Sitzung nimmt "ein paar Dinge" (zwei) und gibt den Verstand zurueck
    out.push_back(d);
    hostPhaseSet(3, out, true);
    return true;
}

void HotelL5::updateHost(double dt, const std::array<PlayerIn, kPlayers>& pl, bool auth, world::World& w, std::vector<Event>& out) {
    (void)w;
    Host& H = host_;
    H.onLevel += dt;
    H.t += dt;
    const bool active = H.phase == 1 || H.phase == 2;
    H.lamp += ((active ? 1.0f : 0.0f) - H.lamp) * std::min(1.0f, (float)dt * (active ? 0.8f : 3.0f));
    H.arm += ((H.phase == 2 && H.t > 2.0 ? 1.0f : 0.0f) - H.arm) * std::min(1.0f, (float)dt * (H.phase == 2 ? 0.45f : 2.5f));
    const PlayerIn* P = H.chosen >= 0 && H.chosen < kPlayers ? &pl[(size_t)H.chosen] : nullptr;
    // Bildnisse mit Augen: die drei naechsten beim Gewaehlten (waehrend der Einladung und nach dem Handel)
    if (P && P->alive && (active || H.phase == 3 || H.phase == 4)) {
        std::vector<std::pair<float, int>> d;
        for (size_t i = 0; i < portraits_.size(); ++i) d.push_back({length(portraits_[i].pos - P->eye), (int)i});
        std::sort(d.begin(), d.end());
        H.portraits.clear();
        for (size_t i = 0; i < d.size() && i < 3; ++i)
            if (d[i].first < 16.0f) H.portraits.push_back(d[i].second);
    } else {
        H.portraits.clear();
    }
    // Rat der Seite: die Bildnisse nicht ansehen - wer es tut, dem zehrt es am Verstand
    if (auth && P && P->alive && !H.portraits.empty()) {
        for (int i : H.portraits) {
            const vec3 e = portraits_[(size_t)i].pos + vec3{0, 0, 0.13f};
            const vec3 d = e - P->eye;
            const float L = length(d);
            if (L < 7.0f && dot(d / L, P->look) > 0.95f) {
                Event s{Event::Sanity};
                s.player = H.chosen, s.value = (float)(dt * 1.0);
                out.push_back(s);
                H.lookDrain += dt;
                if (H.lookDrain > 2.0 && H.chosen == 0) {
                    H.lookDrain = -30.0;
                    hostLine(8, pl, out);
                }
            }
        }
    }
    // Saetze: nur auf dem Rechner des Gewaehlten (Platz 0) - die anderen hoeren und sehen nichts (er erscheint nur ihm)
    if (H.chosen == 0 && (H.phase == 1 || H.phase == 2) && !hold_) {
        H.lineT -= dt;
        if (H.phase == 1 && H.lineT <= 0.0 && H.line < 3) hostLine(H.line++, pl, out), H.lineT = 5.0;
        if (H.phase == 2 && H.lineT <= 0.0 && H.line < 2) hostLine(3 + H.line++, pl, out), H.lineT = 6.0;
    }
    if (!auth || hold_) return;
    switch (H.phase) {
        case 0: {
            if (H.onLevel < cfg_.hostAfter || H.chosen >= 0) break;
            // er waehlt: wer die meiste Angst hat (er riecht sie), in einem Gang nicht zu weit vom Buero
            int best = -1;
            double bs = 1e9;
            for (int i = 0; i < kPlayers; ++i) {
                const PlayerIn& Q = pl[(size_t)i];
                if (!Q.alive || !Q.corridor || hd(Q.feet, office_.pos) > 45.0f) continue;
                if (Q.sanity < bs) bs = Q.sanity, best = i;
            }
            if (best < 0 || (bs > cfg_.hostSanity && H.onLevel < cfg_.hostAfter * 2.0)) break;
            H.chosen = best;
            hostPhaseSet(1, out, true);
            break;
        }
        case 1:
            if (!P || !P->alive) {
                hostPhaseSet(5, out, true);
                break;
            }
            if (hd(P->feet, office_.pos) < 2.4f) {
                hostPhaseSet(2, out, true);
                break;
            }
            if (H.t > 90.0 || hd(P->feet, office_.pos) > 50.0f) {
                H.refused = true;
                Event s{Event::Sanity};
                s.player = H.chosen, s.value = 8.0f;
                out.push_back(s);
                hostPhaseSet(5, out, true);
            }
            break;
        case 2:
            if (!P || !P->alive) {
                hostPhaseSet(5, out, true);
                break;
            }
            H.outT = hd(P->feet, office_.pos) > 4.0f ? H.outT + dt : 0.0;
            if (H.outT > 6.0 || H.t > 120.0) {
                H.refused = true;
                Event s{Event::Sanity};
                s.player = H.chosen, s.value = 8.0f;
                out.push_back(s);
                hostPhaseSet(5, out, true);
            }
            break;
        case 3:
            if (H.fearUntil >= 0.0 && clock_ >= H.fearUntil) {  // die Rechnung: der Handel laesst einen nicht los
                hostPhaseSet(4, out, true);
                Event s{Event::Sanity};
                s.player = H.chosen, s.value = 80.0f;  // die Sitzung setzt hoechstens auf 20
                out.push_back(s);
            }
            break;
        default: break;
    }
}

// --- Koop (Gast) -----------------------------------------------------------------------------------------------------
void HotelL5::applyNet(u32 c, vec3 pos, int selfId, std::vector<Event>& out) {
    const u32 kind = netKind(c), a = (c >> 16) & 0xFF, b = (c >> 8) & 0xFF, d = c & 0xFF;
    // Spieler-Nummern des Hosts: 0 = Host; beim Gast ist 0 dieser Rechner - umrechnen
    auto local = [&](int id) { return id == selfId ? 0 : (id == 0 ? selfId : id); };  // Gast: Platz 0 ist er selbst, der Host auf seinem Platz
    switch (kind) {
        case 1:
            if (a < col_.size()) triggerColony(col_[a], (int)a, local((int)b), out, false);
            break;
        case 3:
            if (a < col_.size()) detach(col_[a], local((int)b), (int)d);
            break;
        case 4:
            if (a < wl_.size()) {
                Woodlin& w = wl_[a];
                const int st = (int)b;
                if (st == 2 || st == 5) {
                    if (w.state == 0) w.pos = pos;
                    w.from = w.pos, w.to = pos, w.t = 0.0, w.dur = std::max(0.2, (double)length(pos - w.pos) / (st == 5 ? 3.3 : 1.9));
                } else if (st == 1 && w.state == 0) {
                    w.pos = w.from = w.to = pos, w.upright = 1.0f;
                }
                w.state = st, w.t = 0.0, w.target = local((int)d);
                if (st == 3) {
                    Event s{Event::SoundAt};
                    s.text = "wl_creak", s.pos = w.pos + vec3{0, 0, 0.2f}, s.value = 1.0f;
                    out.push_back(s);
                }
            }
            break;
        case 5:
            if (a < wa_.size()) {
                Watcher& w = wa_[a];
                if ((int)d != w.spot && d < sWatch_.size()) {
                    placeWatcher(w, (int)d);
                    w.hand[0].model = w.hand[1].model = w.side > 0 ? "prop:wa_hand@0" : "prop:wa_hand@1";
                    w.hand[0].id = w.hand[1].id = -2;
                }
                w.state = (int)b, w.t = 0.0;
                if (w.state == 3) {
                    Event s{Event::SoundAt};
                    s.text = "wa_crack", s.pos = pos + vec3{0, 0, 1.9f}, s.value = 1.0f;
                    out.push_back(s);
                }
            }
            break;
        case 6:
            host_.chosen = local((int)a);
            hostPhaseSet((int)b, out, false);
            break;
        default: break;
    }
}

// --- Skript/Test ----------------------------------------------------------------------------------------------------
bool HotelL5::force(const std::string& what, int idx, int player, std::vector<Event>& out) {
    if (what == "woodlins" && lastW_) {
        for (Woodlin& a : wl_) a.state = 0;
        woodGroup(lastPl_, *lastW_, out, true, player);
        if (idx >= 0 && idx < (int)sWood_.size() && !wl_.empty()) {  // der erste genau an diesem Moebel (Bilder)
            Woodlin& a = wl_[0];
            a.state = 1, a.spot = idx, a.pos = a.from = a.to = sWood_[(size_t)idx].pos, a.yaw = sWood_[(size_t)idx].yaw, a.upright = 1.0f;
            a.target = player;
            if (player >= 0 && player < kPlayers && lastPl_[(size_t)player].alive)  // sieht den Spieler an (Bilder)
                a.yaw = std::atan2(lastPl_[(size_t)player].feet.y - a.pos.y, lastPl_[(size_t)player].feet.x - a.pos.x);
        }
        for (const Woodlin& a : wl_)
            if (a.state != 0) return true;
        return false;
    }
    if (what == "colony" && idx >= 0 && idx < (int)col_.size()) {
        triggerColony(col_[(size_t)idx], idx, player, out, true);
        return true;
    }
    if (what == "host" && hasOffice_) {
        host_.chosen = player;
        hostPhaseSet(std::clamp(idx, 1, 4), out, true);
        if (idx == 2) host_.arm = 1.0f, host_.lamp = 1.0f, host_.t = 3.0;
        return true;
    }
    if (what == "watcher" && idx >= 0 && idx < (int)wa_.size()) {
        wa_[(size_t)idx].state = 1, wa_[(size_t)idx].t = 0.0;
        return true;
    }
    return false;
}

bool HotelL5::portraitNear(vec3 p, vec3& at, float& yaw) const {
    float best = 1e9f;
    for (const Portrait& pt : portraits_)
        if (const float d = hd(pt.pos, p); d < best) best = d, at = pt.pos, yaw = pt.yaw;
    return best < 1e8f;
}

void HotelL5::setState(const std::string& f, double v) {
    if (f == "hold") hold_ = v > 0.5;
    else if (f == "wood_t") woodT_ = v, woodCool_ = 0.0;
    else if (f == "host_after") cfg_.hostAfter = v;
    else if (f == "host_on") host_.onLevel = v;
    else if (f == "wa_peek")
        for (Watcher& a : wa_) a.peek = (float)v, a.state = v > 0.5 ? 1 : 0;
    else if (f == "wa_arm")
        for (Watcher& a : wa_) a.arm = (float)v;
    else if (f == "host_arm") host_.arm = (float)v;
    else if (f == "host_lamp") host_.lamp = (float)v;
    else if (f == "fear_end" && host_.fearUntil >= 0.0) host_.fearUntil = clock_ + v;  // Tests: die Rechnung frueher
}

// --- Bild ------------------------------------------------------------------------------------------------------------
void HotelL5::collect(std::vector<VisibleMesh>& out, vec3 eye) {
    // Kolonien: jedes Tierchen ein Ball (nah); weit weg nur die groesseren
    for (Colony& c : col_) {
        const float dc = length(c.pos - eye);
        if (dc > 40.0f && c.state == 0) continue;
        for (size_t i = 0; i < c.balls.size(); ++i) {
            Ball& b = c.balls[i];
            if (dc > 18.0f && b.state == 0 && b.r < 0.018f) continue;
            const float s = b.r / kAntR;
            const mat4 M = mat4::translation(b.p) * mat4::rotationZ(b.ph * 0.7f + (float)i) * rotX(b.ph * 0.3f) * mat4::scale(vec3{s, s, s * (b.state == 1 ? 0.85f : 1.0f)});
            // Runde 2: der Haufen las sich grau (Glanz im Lampenlicht) - festes Material: schwarz, stumpf, schluckt das Licht
            push(out, antPart_[b.model], M, b.state != 0 || c.rouse > 0.05f, 0.0f, kUmbraLook);
        }
    }
    // Woodlins: Rumpf mit Kopf (gedrechselt), Kiefer, vier Beine aus zwei Gliedern mit Klauen-und-Kugel-Fuss
    for (Woodlin& a : wl_) {
        if (a.state == 0 || length(a.pos - eye) > 45.0f) continue;
        const vec3 f = dirOf(a.yaw), l{-f.y, f.x, 0.0f}, U{0, 0, 1};
        const float up = std::clamp(a.upright, 0.0f, 1.0f);
        constexpr float s = 1.45f;  // Runde 2: kaum zu bemerken (40 cm) - so gross wie ein kleines Kind (gut 0,55 m)
        const mat4 Sc = mat4::scale(vec3{s, s, s});
        // Rumpf: aufrecht (Schnitzfigur) .. waagrecht (huschend), dann haengt der Kopf vorn
        const float pitch = (1.0f - up) * 1.35f;
        const float bob = (1.0f - up) * 0.012f * s * std::sin(a.gait * 2.0f);
        const vec3 hip = a.pos + U * ((0.06f + 0.10f * (1.0f - up)) * s + bob) - f * (0.08f * s * (1.0f - up));
        const mat4 B = mat4::translation(hip) * mat4::rotationZ(a.yaw) * rotY(pitch) * rotY(-0.25f * a.jaw) * Sc;
        push(out, a.body, B, true, 0.5f);
        push(out, a.jaw_, B * mat4::translation(vec3{kWlJaw[0], kWlJaw[1], kWlJaw[2]}) * rotY(0.75f * a.jaw) * mat4::scale(vec3{1.4f, 1.4f, 1.4f}), true, 0.25f);  // Runde 4: Kopf 1,4-fach
        for (int g = 0; g < 4; ++g) {
            const bool front = g < 2;
            const float sd = (g % 2 == 0) ? 1.0f : -1.0f;
            const vec3 att = apply(B, vec3{0.0f, sd * 0.045f, front ? kWlShoulder : kWlHip + 0.02f});
            // Fuesse: aufrecht - Hinterbeine gefaltet am Boden, Vorderbeine vor der Brust (gefaltete Haende einer Figur);
            // huschend - weit seitlich gespreizt, im Takt (diagonal versetzt)
            const float ph = a.gait + (front == (sd > 0.0f) ? 0.0f : kPi);
            const vec3 footRun = a.pos + (f * ((front ? 0.16f : -0.12f) + 0.07f * std::sin(ph)) + l * (sd * 0.2f) + U * (0.03f * std::max(0.0f, std::cos(ph)))) * s;
            const vec3 footUp = front ? apply(B, vec3{0.09f, sd * 0.03f, kWlShoulder - 0.07f}) : a.pos + (f * 0.06f + l * (sd * 0.07f)) * s;
            vec3 foot = footUp + (footRun - footUp) * (1.0f - up);
            if (a.state == 4 && front) foot = a.pos + (f * 0.35f + U * 0.08f + l * (sd * 0.05f)) * s;  // Biss: nach vorn gestossen
            const vec3 bend = front ? nrm(-f * 0.4f + U + l * sd * 0.6f * (1.0f - up), U) : nrm(f * 0.3f + U + l * sd * 0.8f, U);
            const vec3 knee = [&] {
                vec3 m, e;
                anim::ik2(att, foot, kWlLegA * s, kWlLegB * s, bend, m, e);
                return m;
            }();
            push(out, a.legA[g], along(att, knee - att, U) * Sc, true, 0.35f);
            push(out, a.legB[g], along(knee, foot - knee, f) * Sc, true, 0.35f);
        }
    }
    // Watchers: Finger um den Rahmen (immer), Kopf mit Augensaeule lugt um die Kante, Arm beim Hieb
    for (Watcher& a : wa_) {
        if (a.spot < 0 || a.state == 5) continue;
        const Spot& S = sWatch_[(size_t)a.spot];
        if (length(S.pos - eye) > 40.0f) continue;
        const vec3 o = dirOf(S.yaw), al{-o.y, o.x, 0.0f}, U{0, 0, 1};
        const float sd = (float)a.side;
        const vec3 edge = S.pos + al * (sd * 0.5f);  // Kante der Tueroeffnung (Wandflucht zum Gang)
        // zwei Haende: hoch neben dem Kopf, viel zu tief unten (Auslegung: falsche Hoehe); greifen fester vor dem Hieb
        const float gz[2] = {1.86f, 0.52f};
        for (int h = 0; h < 2; ++h) {
            const float creep = 0.012f * std::sin((float)clock_ * (0.7f + 0.3f * (float)h)) - 0.01f * a.grip;
            const mat4 M = basis(o, al, U, edge + U * gz[h] + o * creep) * mat4::scale(vec3{1.0f, 1.0f, 1.0f + 0.04f * a.grip});
            push(out, a.hand[h], M, true, 0.4f, kRotSkinLook);
        }
        {  // Runde 2: nur Kopf und Finger lasen sich wie ein Teller an der Wand - der ausgezehrte Leib steht im Dunkel der
           // Oeffnung, vorgebeugt unter dem Sturz (Ursprung am Halsansatz, der Rumpf haengt nach unten)
            const float pk = sm01(a.peek);
            const vec3 nb = edge - o * (0.78f - 0.34f * pk) - al * (sd * 0.24f) + U * 1.6f;
            push(out, a.body, mat4::translation(nb) * mat4::rotationZ(S.yaw) * rotY(0.32f) * rotX(sd * 0.12f * pk), true, 1.2f, kDarkSkinLook);
        }
        if (a.peek > 0.02f) {
            // Kopf: steht tief in der dunklen Oeffnung an der Laibung und kommt nach vorn an die Kante, schief gelegt (fast
            // waagrecht) - er ist groesser als die Tuer und muss sich unter den Sturz ducken
            const float pk = sm01(a.peek);
            const vec3 neck = edge - o * (0.6f - 0.52f * pk) - al * (sd * (0.12f + 0.1f * pk)) + U * (1.66f - 0.04f * pk);
            // Runde 4: der Kopf dreht sich mit dem Blick zum Ziel (das Gesicht, nicht die Seite); weniger schief
            const float turnTo = std::clamp(std::remainder(std::atan2(a.look.y, a.look.x) - S.yaw, kTau), -0.7f, 0.7f) * pk;
            const mat4 Hm = mat4::translation(neck) * mat4::rotationZ(S.yaw + turnTo) * rotX(sd * 0.55f * a.tilt * pk) *
                            rotY(-0.15f + 0.08f * std::sin((float)clock_ * 0.5f)) * mat4::scale(vec3{1.2f, 1.2f, 1.2f});  // Runde 2: groesser
            push(out, a.head, Hm, true, 0.5f, kRotSkinLook);
            push(out, a.face, Hm, true, 0.5f);
            // Augen: Saeule aus fuenf Augen verschiedener Groesse; jedes folgt dem Ziel, blinzeln nacheinander (rollen hoch)
            static const vec3 kEye[5] = {{0.085f, 0.0f, 0.36f}, {0.098f, 0.008f, 0.30f}, {0.104f, -0.006f, 0.235f}, {0.1f, 0.01f, 0.17f}, {0.088f, -0.004f, 0.11f}};
            static const float kEyeS[5] = {0.7f, 1.0f, 1.25f, 0.85f, 0.6f};
            for (int e = 0; e < 5; ++e) {
                const vec3 p = apply(Hm, kEye[e]);
                vec3 d = a.look;
                if (e == a.blinkEye && a.blink > 0.0f) d = nrm(d + U * 2.5f * a.blink, d);  // rollt nach oben
                d = nrm(d + vec3{0.04f * std::sin((float)clock_ * 3.0f + (float)e), 0.0f, 0.0f}, d);
                const float es = kEyeS[e] * 1.2f;
                push(out, a.eye[e], along(p, d, U) * mat4::scale(vec3{es, es, es}), true, 0.05f);
            }
        }
        if (a.arm > 0.02f) {  // Hieb: der Arm schiesst aus der Oeffnung zum Ziel
            const vec3 sh = edge - o * 0.3f - al * (sd * 0.15f) + U * 1.5f;
            const vec3 tgt = a.armTo.x != 0.0f || a.armTo.y != 0.0f ? a.armTo : sh + o * 1.5f;
            const vec3 reach = sh + (tgt - sh) * (0.15f + 0.85f * sm01(a.arm));
            vec3 el, wr;
            anim::ik2(sh, reach, kWaArmA, kWaArmB, U + o * 0.3f, el, wr);
            push(out, a.armA, along(sh, el - sh, U), true, 0.8f, kRotSkinLook);
            push(out, a.armB, along(el, wr - el, U), true, 0.8f, kRotSkinLook);
            push(out, a.claw, along(wr, nrm(wr - el, o), U), true, 0.4f, kRotSkinLook);
        }
    }
    // Gastgeber: Arm im Anzugaermel aus dem abgewandten Sessel, viel zu lang (drei Glieder), die Hand zum Handschlag;
    // die Augen in den Bildnissen
    Host& H = host_;
    if (hasOffice_ && H.chosen == 0 && H.arm > 0.01f && length(office_.pos - eye) < 30.0f) {  // nur der Gewaehlte sieht ihn
        const vec3 f = dirOf(office_.yaw), l{-f.y, f.x, 0.0f}, U{0, 0, 1};
        const float u = sm01(H.arm);
        // Runde 2: der Arm las sich wie ein Ofenrohr - jetzt ueber die Lehne gehoben, mit zwei Ellbogen auf den Tisch
        // gelegt (zu lang: drei Glieder), die Hand hebt sich zum Handschlag ueber die Besucherseite
        const vec3 sh = office_.pos - f * 0.66f + l * 0.24f + U * 1.02f;  // hinter der hohen Lehne des abgewandten Sessels
        const vec3 handP = office_.pos + f * (0.12f + 0.55f * u) + l * 0.02f + U * (0.93f + 0.025f * std::sin((float)clock_ * 1.3f));
        const vec3 e1 = sh + (f * 0.3f + U * 0.26f + l * 0.04f) * (0.4f + 0.6f * u);
        const vec3 e2 = sh + (office_.pos - f * 0.04f + l * 0.14f + U * 0.86f - sh) * u + U * 0.1f * (1.0f - u);
        auto seg = [&](Part& p, vec3 a0, vec3 b0) {  // Glied mit etwas Ueberstand an den Ellbogen (keine Luecke)
            const float L = length(b0 - a0) + 0.05f;
            push(out, p, along(a0 - nrm(b0 - a0, f) * 0.025f, b0 - a0, U) * mat4::scale(vec3{L / kE18Arm, 1.0f, 1.0f}), true, 1.0f);
        };
        seg(H.armA, sh, e1);
        seg(H.armB, e1, e2);
        seg(H.armC, e2, handP);
        const float beckon = H.phase == 2 ? 0.25f * std::max(0.0f, std::sin((float)clock_ * 2.2f)) : 0.0f;
        push(out, H.hand, along(handP, nrm(handP - e2, f), U) * rotX(-0.15f) * rotY(beckon) * mat4::scale(vec3{1.5f, 1.5f, 1.5f}), true, 0.4f);
    }
    for (size_t k = 0; H.chosen == 0 && k < H.portraits.size() && k < 6; ++k) {
        const Portrait& pt = portraits_[(size_t)H.portraits[k]];
        const vec3 f = dirOf(pt.yaw), l{-f.y, f.x, 0.0f}, U{0, 0, 1};
        for (int s = 0; s < 2; ++s) {
            const vec3 p = pt.pos + f * 0.017f + l * ((s ? -1.0f : 1.0f) * 0.032f) + U * 0.126f;
            vec3 d = nrm(eye - p, f);
            if (dot(d, f) < 0.2f) d = nrm(d + f, f);
            push(out, H.eyes[k][s], along(p, d, U), true, 0.03f);
        }
    }
}

void HotelL5::lights(std::vector<Light>& out, vec3 eye) const {
    if (hasOffice_ && host_.chosen == 0 && host_.lamp > 0.02f && length(office_.pos - eye) < 30.0f) {  // Bankerlampe mit gruenem Schirm
        const vec3 f = dirOf(office_.yaw), l{-f.y, f.x, 0.0f};
        Light L;
        const float fl = 0.92f + 0.08f * std::sin((float)clock_ * 17.0f) * std::sin((float)clock_ * 3.1f);
        L.pos = office_.pos - f * 0.05f + l * 0.38f + vec3{0, 0, 1.02f};
        L.color = vec3{1.0f, 0.86f, 0.55f} * (2.2f * host_.lamp * fl);
        L.range = 5.0f;
        L.dir = vec3{0, 0, -1};
        out.push_back(L);
        Light G;  // gruener Schein durch das Glas des Schirms
        G.pos = L.pos + vec3{0, 0, 0.12f};
        G.color = vec3{0.25f, 0.9f, 0.45f} * (0.5f * host_.lamp * fl);
        G.range = 2.5f;
        G.dir = vec3{0, 0, 1};
        out.push_back(G);
    }
}

}  // namespace lim::game
