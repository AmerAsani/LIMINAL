// V13.3 Teil 4: Randwesen von Level 2 ohne eigene Rolle - Cannibal Cuisine, Swindlebird, Photoshop, Macchina
// (gameplay/apparitions_l2.hpp; KREATUR_CannibalCuisine.md, KREATUR_Swindlebird.md, KREATUR_Photoshop.md,
// KREATUR_Macchina.md). Alle beweglichen Teile sind starre Modelle aus models_l2_rand.py, die hier in Ketten gedreht
// werden (Ursprung jedes Teils = sein Gelenk, das Teil liegt entlang +x).
#include "gameplay/apparitions_l2.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <format>

#include "core/log.hpp"
#include "gameplay/navfield.hpp"
#include "world/level_def.hpp"
#include "world/map_def.hpp"
#include "world/world.hpp"

namespace lim::game {

namespace {

constexpr float kPi = 3.14159265f;

// --- Gelenke und Masse (gleich in models_l2_rand.py) ----------------------------------------------------------------
constexpr float kCcFront = Apparitions::kCcD * 0.5f;     // Vorderseite (lokal x)
constexpr vec3 kCcFlapTop{0.36f, 0.0f, 1.81f};            // Gelenk der Kopfklappe
constexpr vec3 kCcFlapBot{0.36f, 0.0f, 0.08f};            // Gelenk der Sockelklappe
constexpr float kCcArm[3] = {0.55f, 0.52f, 0.30f};        // Oberarm, Unterarm, Hand
constexpr vec3 kCcShoulderUp{kCcFront - 0.16f, 0.36f, 1.72f}, kCcShoulderLow{kCcFront - 0.18f, 0.36f, 0.20f};
constexpr float kCcGy0 = -Apparitions::kCcW * 0.5f + 0.04f, kCcGz0 = 0.58f;  // Fach: linke Kante, Unterkante
constexpr vec3 kSbNeck{0.085f, 0.0f, 0.055f}, kSbShoulder{0.045f, 0.052f, 0.04f}, kSbTail{-0.115f, 0.0f, 0.012f};
constexpr float kSbWrist = 0.11f;
constexpr float kSbLeg[3] = {0.075f, 0.07f, 0.06f};
constexpr float kPsArm[2] = {0.62f, 0.56f};
constexpr float kPsHand = 0.19f;
const float kMonRot[3] = {0.0f, -0.10f, 0.22f};          // Drehung des Monitors je Schreibtisch (models_l1_code.py)

enum NetKind : u32 { NCcVend = 1, NCcAnger = 2, NSbAppear = 3, NSbFlee = 4, NSbGone = 5, NPsBox = 6, NPsFed = 7, NPsGone = 8, NPsKill = 9 };
u32 netCode(u32 kind, u32 a, u32 b) { return (kind << 24) | ((a & 0xFFu) << 16) | (b & 0xFFFFu); }

mat4 rotY(float a) {  // +x neigt sich fuer a > 0 nach unten
    mat4 m = mat4::identity();
    const float c = std::cos(a), s = std::sin(a);
    m.c[0] = {c, 0, -s, 0};
    m.c[2] = {s, 0, c, 0};
    return m;
}
mat4 pitchUp(float a) { return rotY(-a); }
mat4 rotX(float a) {
    mat4 m = mat4::identity();
    const float c = std::cos(a), s = std::sin(a);
    m.c[1] = {0, c, s, 0};
    m.c[2] = {0, -s, c, 0};
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
// Teil entlang d (x), Seitenachse moeglichst "side"
mat4 along(vec3 o, vec3 d, vec3 side) {
    d = normalize(d);
    vec3 z = cross(d, side);
    if (length(z) < 1e-4f) z = cross(d, vec3{0, 0, 1});
    if (length(z) < 1e-4f) z = cross(d, vec3{1, 0, 0});
    z = normalize(z);
    const vec3 y = cross(z, d);
    return basis(d, y, z, o);
}
vec3 apply(const mat4& m, vec3 p) {
    const vec4 r = m * vec4{p.x, p.y, p.z, 1.0f};
    return {r.x, r.y, r.z};
}
vec3 flat(vec3 v) { return {v.x, v.y, 0.0f}; }
float smooth01(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
float hdist(vec3 a, vec3 b) { return std::hypot(a.x - b.x, a.y - b.y); }

void push(std::vector<VisibleMesh>& out, Apparitions::Part& p, const mat4& w, bool dynamic = true, float cullR = 0.0f) {
    VisibleMesh v;
    v.model = &p.model;
    v.meshId = &p.id;
    v.world = w;
    v.dynamic = dynamic;
    if (cullR > 0.0f) v.cullCenter = apply(w, vec3{0, 0, 0}), v.cullRadius = cullR;
    out.push_back(v);
}

// zwei Glieder: Gelenkpunkt zwischen a und b (Laengen l1, l2), Knick in Richtung "bend"
vec3 elbowOf(vec3 a, vec3 b, float l1, float l2, vec3 bend) {
    const vec3 ab = b - a;
    const float d = std::clamp(length(ab), 1e-3f, l1 + l2 - 1e-3f);
    const vec3 dir = normalize(ab);
    const float x = (d * d + l1 * l1 - l2 * l2) / (2.0f * d);
    const float h = std::sqrt(std::max(0.0f, l1 * l1 - x * x));
    vec3 n = bend - dir * dot(bend, dir);
    n = length(n) < 1e-4f ? vec3{0, 0, 1} : normalize(n);
    return a + dir * x + n * h;
}

const char* const kMacchinaText =
    "Ein Anzug aus Rohrblech, an die Wand gesunken, die Brustplatte steht offen – innen ist er leer und blank gescheuert, "
    "als hätte nie etwas anderes darin gesteckt. Auf das Visier ist ein Menschengesicht geschweißt. Daneben ein kleines "
    "Gestell, nie fertig geworden. Ohne Anzug, heißt es, galten sie nicht als lebendig.";

}  // namespace

// --- Aufbau ---------------------------------------------------------------------------------------------------------

void Apparitions::setup(const world::LevelDef& level, u64 seed) {
    cfg_ = Config{};
    cc_.clear(), sbSpots_.clear(), sbYaw_.clear(), psSpots_.clear(), relics_.clear();
    bird_ = Bird{};
    shop_ = Shop{};
    clock_ = 0.0, sbDue_ = -1.0;
    rng_ = Rng(hash64((i64)seed, "apparitions", level.id.c_str()));
    const Json& j = level.apparitions;
    if (!j.isObject() || !level.mapped()) return;
    cfg_.cuisine = j["cuisine"].asBool(false);
    cfg_.swindle = j["swindle"].asBool(false);
    cfg_.photoshop = j["photoshop"].asBool(false);
    cfg_.macchina = j["macchina"].asBool(false);
    cfg_.stock = (int)j["stock"].asInt(cfg_.stock);
    cfg_.strike = j["strike"].asNumber(cfg_.strike);
    cfg_.withdraw = j["withdraw"].asNumber(cfg_.withdraw);
    cfg_.photoshopChance = j["photoshop_chance"].asNumber(cfg_.photoshopChance);
    cfg_.boxRange = j["box_range"].asNumber(cfg_.boxRange);
    cfg_.boxTime = j["box_time"].asNumber(cfg_.boxTime);
    cfg_.swindleChance = j["swindle_chance"].asNumber(cfg_.swindleChance);
    cfg_.swindleAfter = j["swindle_after"].asNumber(cfg_.swindleAfter);
    cfg_.swindleSpan = j["swindle_span"].asNumber(cfg_.swindleSpan);
    const auto& sp = level.map->spawns;
    for (size_t i = 0; i < sp.size(); ++i) {
        const world::MapSpawn& s = sp[i];
        if (s.kind != "trace") continue;
        const vec3 p{(float)s.x, (float)s.y, (float)s.z};
        if (s.set == "cuisine" && cfg_.cuisine) {
            Cuisine c;
            c.pos = p, c.yaw = (float)s.yaw;
            c.variant = (int)(cc_.size() % 3);
            for (const std::string& t : s.tags) {
                if (t.size() == 2 && t[0] == 'v' && t[1] >= '0' && t[1] <= '2') c.variant = t[1] - '0';
                if (t.size() > 1 && t[0] == 'o')  // der Spawnpunkt liegt vor dem Block: so weit dahinter steht der Automat
                    c.pos = c.pos - vec3{std::cos(c.yaw), std::sin(c.yaw), 0.0f} * (float)std::atof(t.c_str() + 1);
            }
            c.stock = cfg_.stock;
            c.id = std::format("cc{}", i);
            // das leere Fach dieser Lackierung (models_l2_rand.py: (r * 7 + c * 3 + v) % 11 == 4)
            bool found = false;
            for (int r = 0; r < 5 && !found; ++r)
                for (int k = 0; k < 4 && !found; ++k)
                    if ((r * 7 + k * 3 + c.variant) % 11 == 4) c.slotRow = r, c.slotCol = k, found = true;
            c.whisperT = rng_.uniform(3.0, 9.0);
            cc_.push_back(std::move(c));
        } else if (s.set == "swindle" && cfg_.swindle) {
            sbSpots_.push_back(p), sbYaw_.push_back((float)s.yaw);
        } else if (s.set == "photoshop" && cfg_.photoshop) {
            PsSpot ps;
            ps.pos = p, ps.yaw = (float)s.yaw;
            ps.screen = p + vec3{0, 0, 0.96f}, ps.screenYaw = ps.yaw;
            for (const std::string& t : s.tags) {  // "pc<x>,<y>,<Drehung>,<Variante>": Schreibtisch mit Roehrenmonitor
                if (t.rfind("pc", 0) != 0) continue;
                float dx = 0, dy = 0, dyaw = 0;
                int var = 0;
                if (std::sscanf(t.c_str() + 2, "%f,%f,%f,%d", &dx, &dy, &dyaw, &var) != 4) continue;
                const float mr = kMonRot[std::clamp(var, 0, 2)];
                // Abnahme Level 2: Blender-y ist im Spiel gespiegelt (props/glb_import) - die Drehung des Monitors wirkt im
                // Spiel andersherum; vorher sass das Bild bis 9 cm neben der Scheibe und das dunkle Glas schnitt ein Oval hinein
                const vec3 local{-0.02f + 0.2035f * std::cos(mr), -0.2035f * std::sin(mr), 0.958f};
                ps.screen = apply(mat4::translation(vec3{dx, dy, p.z}) * mat4::rotationZ(dyaw), local);
                ps.screenYaw = dyaw - mr;
            }
            psSpots_.push_back(ps);
        } else if (s.set == "macchina" && cfg_.macchina) {
            relics_.push_back({p, (float)s.yaw, std::format("mc{}", i), rng_.uniform(14.0, 30.0)});
        }
    }
    if (!psSpots_.empty() && rng_.random() < cfg_.photoshopChance) {
        shop_.present = true;
        shop_.spot = (int)rng_.randint(0, (i64)psSpots_.size() - 1);
        const PsSpot& ps = psSpots_[(size_t)shop_.spot];
        shop_.pos = ps.pos, shop_.yaw = ps.yaw, shop_.screen = ps.screen, shop_.screenYaw = ps.screenYaw;
    }
    if (!sbSpots_.empty() && rng_.random() < cfg_.swindleChance) sbDue_ = cfg_.swindleAfter + rng_.random() * cfg_.swindleSpan;
    setupParts();
    log::info("Randwesen ({}): Automaten {}, Swindlebird {} Stellen (Sichtung {}), Photoshop {} Plaetze ({}), Macchina-Reste {}",
              level.id, cc_.size(), sbSpots_.size(), sbDue_ >= 0.0 ? std::format("nach {:.0f} s", sbDue_) : std::string("keine"),
              psSpots_.size(), shop_.present ? "da" : "nicht da", relics_.size());
}

void Apparitions::setupParts() {
    for (Cuisine& c : cc_) {
        c.body.model = std::format("prop:cc_body@{}", c.variant);
        c.flapTop.model = std::format("prop:cc_flap_top@{}", c.variant);
        c.flapBot.model = "prop:cc_flap_bot";
        for (int a = 0; a < 4; ++a) {
            c.arm[a][0].model = "prop:cc_humerus";
            c.arm[a][1].model = "prop:cc_forearm";
            c.arm[a][2].model = a < 2 ? "prop:cc_hand@0" : "prop:cc_hand@1";  // oben Krallen, unten gespreizt (Fuesse)
        }
        c.vendHand.model = "prop:cc_hand@1";
        c.ware.model = "prop:cc_ware";
    }
    Bird& b = bird_;
    b.body.model = "prop:sb_body", b.head.model = "prop:sb_head", b.tail.model = "prop:sb_tail";
    for (int s = 0; s < 2; ++s) {
        b.wingIn[s].model = std::format("prop:sb_wing_in@{}", s);
        b.wingOut[s].model = std::format("prop:sb_wing_out@{}", s);
        b.shin[s].model = "prop:sb_shin", b.tars[s].model = "prop:sb_tars", b.foot[s].model = "prop:sb_foot";
    }
    Shop& p = shop_;
    for (int k = 0; k < 4; ++k) p.block[k].model = std::format("prop:ps_block@{}", k + 1);
    p.screenImg.model = "prop:ps_screen";
    p.armA.model = "prop:ps_arm_a", p.armB.model = "prop:ps_arm_b", p.hand.model = "prop:ps_hand@0";
    p.edge.model = "prop:ps_edge", p.handle.model = "prop:ps_handle";
}

// --- Bild fuer Bild -------------------------------------------------------------------------------------------------

void Apparitions::update(double dt, const std::array<PlayerIn, kPlayers>& pl, const std::vector<Viewer>& viewers, bool auth,
                         world::World& w, std::vector<Event>& out) {
    clock_ += dt;
    auth = auth && !hold_;
    // Sucht: Entzug fern der Automaten, Schluchzen in ihrer Naehe (nur der eigene Spieler hoert es)
    for (int id = 0; id < kPlayers; ++id) {
        Addict& a = add_[(size_t)id];
        if (!a.on || !pl[(size_t)id].alive) continue;
        float best = 1e9f;
        vec3 at{};
        for (const Cuisine& c : cc_)
            if (const float d = hdist(c.pos, pl[(size_t)id].feet); d < best) best = d, at = c.pos;
        a.withdrawT = best > 40.0f ? a.withdrawT + dt : std::max(0.0, a.withdrawT - dt * 3.0);
        if (id == 0 && best < 9.0f && (a.sobT -= dt) <= 0.0) {
            out.push_back({Event::SoundAt, 0, "cc_sob", at + vec3{0, 0, 0.3f}, 0.55f});
            a.sobT = rng_.uniform(7.0, 13.0);
        }
        if (id == 0 && a.withdrawT > 60.0 && std::fmod(a.withdrawT, 90.0) < dt)
            out.push_back({Event::Toast, 0, "Der Hunger nach dem Automaten. Nichts anderes schmeckt, nichts anderes hilft.", {}, 4.0f});
    }
    updateCuisine(dt, pl, auth, out);
    updateBird(dt, pl, viewers, auth, w, out);
    updateShop(dt, pl, auth, out);
    updateRelics(dt, pl, out);
}

// --- Cannibal Cuisine -----------------------------------------------------------------------------------------------

int Apparitions::cuisineAt(vec3 eye, vec3 look, float range) const {
    int best = -1;
    float bd = range;
    for (size_t i = 0; i < cc_.size(); ++i) {
        const Cuisine& c = cc_[i];
        const vec3 front = c.pos + vec3{std::cos(c.yaw), std::sin(c.yaw), 0.0f} * kCcFront + vec3{0, 0, 1.0f};
        const vec3 d = front - eye;
        const float len = length(d);
        if (len > bd || len < 1e-3f || dot(d / len, look) < 0.55f) continue;
        bd = len, best = (int)i;
    }
    return best;
}

bool Apparitions::canVend(int idx, std::string& msg) const {
    if (idx < 0 || idx >= (int)cc_.size()) return false;
    const Cuisine& c = cc_[(size_t)idx];
    if (c.angerT >= 0.0) {
        msg = "Die Klappen schlagen. Weg da!";
        return false;
    }
    if (c.vendT >= 0.0 && c.vendT < 1.6) {
        msg = "Die Hand im Fach schiebt noch.";
        return false;
    }
    if (c.stock <= 0) {
        msg = "Die Fächer sind leer. Dahinter flüstert es.";
        return false;
    }
    return true;
}

bool Apparitions::vend(int idx, int player, std::string& msg, std::vector<Event>& out) {
    if (!canVend(idx, msg)) {
        if (idx >= 0 && idx < (int)cc_.size() && cc_[(size_t)idx].stock <= 0)
            out.push_back({Event::SoundAt, player, "cc_whisper", cc_[(size_t)idx].pos + vec3{0, 0, 0.3f}, 0.8f});
        return false;
    }
    Cuisine& c = cc_[(size_t)idx];
    const vec3 f{std::cos(c.yaw), std::sin(c.yaw), 0.0f}, l{-f.y, f.x, 0.0f};
    --c.stock;
    c.vendT = 0.0;
    out.push_back({Event::SoundAt, player, "cc_keys", c.pos + f * kCcFront + l * 0.45f + vec3{0, 0, 1.2f}, 0.7f});
    out.push_back({Event::Net, player, "", c.pos, 1.0f, netCode(NCcVend, (u32)idx, (u32)player)});
    msg = "Ware aus dem Automaten – gratis. Kein Preis, keine Münze.";
    return true;
}

std::string Apparitions::eat(int id) {
    Addict& a = add_[(size_t)id];
    const bool first = !a.on;
    a.on = true, ++a.eaten, a.withdrawT = 0.0;
    return first ? "Süß, salzig, warm – ein Rausch wie nie zuvor. Und sofort der Gedanke: mehr."
                 : "Wieder dieser Rausch. Woher hast du das? Egal. Mehr.";
}

bool Apparitions::refuses(int id, const std::string& key) const {
    return add_[(size_t)id].on && (key == "almond" || key == "almond_open" || key == "energy");
}

double Apparitions::sanityDrain(int id) const {
    const Addict& a = add_[(size_t)id];
    return a.on && a.withdrawT > 0.0 ? cfg_.withdraw * std::min(1.0, a.withdrawT / 30.0) : 0.0;
}

void Apparitions::startAnger(Cuisine& c, int target, std::vector<Event>& out) {
    const vec3 f{std::cos(c.yaw), std::sin(c.yaw), 0.0f};
    if (c.angerT >= 0.0) {
        c.angerLen = std::max(c.angerLen, c.angerT + 4.0);  // weiter gereizt: laenger
        c.target = target;
        return;
    }
    c.angerT = 0.0, c.angerLen = 5.6, c.strikeT = 1.15, c.target = target, c.strikeArm = 0;
    out.push_back({Event::SoundAt, 0, "cc_rattle", c.pos + f * kCcFront + vec3{0, 0, 1.0f}, 0.9f});
}

void Apparitions::updateCuisine(double dt, const std::array<PlayerIn, kPlayers>& pl, bool auth, std::vector<Event>& out) {
    for (size_t i = 0; i < cc_.size(); ++i) {
        Cuisine& c = cc_[i];
        const vec3 f{std::cos(c.yaw), std::sin(c.yaw), 0.0f};
        // Fluestern der Leichen im Inneren (leise, unverstaendlich) - fuer den eigenen Spieler, nicht fuer Suechtige
        const float d0 = pl[0].alive ? hdist(c.pos, pl[0].feet) : 1e9f;
        if (d0 < 6.0f && !add_[0].on && (c.whisperT -= dt) <= 0.0) {
            out.push_back({Event::SoundAt, 0, "cc_whisper", c.pos + f * 0.1f + vec3{0, 0, 0.25f}, 0.45f});
            c.whisperT = rng_.uniform(7.0, 15.0);
        }
        // Ausgabe: Tasten, die Hand schiebt, die Ware kippt und faellt
        if (c.vendT >= 0.0) {
            const double t0 = c.vendT;
            c.vendT += dt;
            const vec3 slot = c.pos + f * (kCcFront - 0.06f) + vec3{0, 0, kCcGz0 + 0.2f * (float)c.slotRow};
            if (t0 < 0.38 && c.vendT >= 0.38) out.push_back({Event::SoundAt, 0, "cc_hand", slot, 0.75f});
            if (t0 < 1.32 && c.vendT >= 1.32) out.push_back({Event::SoundAt, 0, "cc_drop", c.pos + f * (kCcFront - 0.08f) + vec3{0, 0, 0.42f}, 0.8f});
            if (c.vendT > 2.6) c.vendT = -1.0;
        }
        // Zorn: Klappen klappern, Kopfschild flackert, Klappen auf, Arme heraus, Hiebe, zurueck
        c.flicker = 1.0f;
        if (c.angerT < 0.0) {
            c.lurch = std::max(0.0f, c.lurch - (float)dt * 0.8f);
            c.lift = std::max(0.0f, c.lift - (float)dt * 0.5f);
            continue;
        }
        const double t0 = c.angerT;
        c.angerT += dt;
        if (c.angerT < 0.6) c.flicker = std::sin((float)c.angerT * 61.0f) > 0.0f ? 1.0f : 0.25f;
        if (t0 < 0.6 && c.angerT >= 0.6) out.push_back({Event::SoundAt, 0, "cc_flap", c.pos + f * kCcFront + vec3{0, 0, 1.0f}, 1.0f});
        if (t0 < 0.95 && c.angerT >= 0.95) out.push_back({Event::SoundAt, 0, "cc_creak", c.pos + f * kCcFront + vec3{0, 0, 0.25f}, 0.8f});
        // Ziel (fuer Ausfall und Hiebe)
        vec3 tp = c.pos + f * 2.0f;
        if (c.target >= 0 && c.target < kPlayers && pl[(size_t)c.target].alive) tp = pl[(size_t)c.target].feet;
        const float tdist = hdist(c.pos, tp);
        const bool out_ = c.angerT > 0.75 && c.angerT < c.angerLen - 0.7;
        const float wantLurch = out_ ? std::clamp(tdist - kCcFront - 1.25f, 0.0f, 0.6f) : 0.0f;
        c.lurch += std::clamp(wantLurch - c.lurch, -(float)dt * 0.9f, (float)dt * 1.4f);
        const float wantLift = out_ ? 0.12f : 0.0f;
        c.lift += std::clamp(wantLift - c.lift, -(float)dt * 0.4f, (float)dt * 0.6f);
        if (out_ && (c.strikeT -= dt) <= 0.0) {
            c.strikeT = 0.85, c.strikeArm ^= 1;
            out.push_back({Event::SoundAt, 0, "cc_strike", c.pos + f * (kCcFront + 0.6f + c.lurch) + vec3{0, 0, 1.3f}, 0.9f});
            if (auth)  // wer in Reichweite der Arme steht (1,37 m ab der Vorderseite, Wiki), wird getroffen
                for (int id = 0; id < kPlayers; ++id) {
                    const PlayerIn& p = pl[(size_t)id];
                    if (!p.alive) continue;
                    const vec3 rel = p.feet - (c.pos + f * c.lurch);
                    const float ahead = dot(flat(rel), f), side = std::fabs(dot(flat(rel), vec3{-f.y, f.x, 0.0f}));
                    if (ahead > 0.0f && ahead < kCcFront + 1.6f && side < 1.0f)
                        out.push_back({Event::Hurt, id, "automat", p.feet + vec3{0, 0, 1.3f}, (float)cfg_.strike});
                }
        }
        if (c.angerT > c.angerLen) c.angerT = -1.0, c.target = -1;
    }
}

int Apparitions::swing(int player, vec3 eye, vec3 look, std::vector<Event>& out) {
    // Automat: wer ihn angreift, den schlagen die Arme (Wiki entity-36)
    if (const int i = cuisineAt(eye, look, 2.0f); i >= 0) {
        Cuisine& c = cc_[(size_t)i];
        out.push_back({Event::Net, player, "", eye, 1.0f, netCode(NCcAnger, (u32)i, (u32)player)});
        startAnger(c, player, out);
        return 2;
    }
    // Photoshop: wer ihn angreift, wird ausgeschnitten (Wiki entity-104)
    if (shop_.present && (shop_.state == 0)) {
        const vec3 c = shop_.pos + vec3{0, 0, 1.2f};
        const vec3 d = c - eye;
        if (length(d) < 2.3f && dot(normalize(d), look) > 0.5f) {
            startBox(player, eye - vec3{0, 0, 1.6f}, out);
            return 2;
        }
    }
    return 0;
}

// --- Swindlebird ----------------------------------------------------------------------------------------------------

bool Apparitions::birdSeen(const std::vector<Viewer>& viewers, world::World& w) const {
    const vec3 c = bird_.pos + vec3{0, 0, 0.25f};  // Mitte des Vogels (Ara-Groesse)
    for (const Viewer& v : viewers) {
        const vec3 d = c - v.eye;
        const float len = length(d);
        if (len < 3.0f) return true;
        if (len > 45.0f || dot(d / len, v.look) < std::cos(0.8f)) continue;
        if (clearSight(w, v.eye, c)) return true;
    }
    return false;
}

void Apparitions::forceBird(int spot) {
    if (spot < 0 || spot >= (int)sbSpots_.size()) return;
    bird_.state = 1, bird_.spot = spot, bird_.t = 0.0, bird_.seen = bird_.unseen = 0.0;
    bird_.wasSeen = bird_.told = false;
    bird_.pos = sbSpots_[(size_t)spot];
    bird_.yaw = sbYaw_[(size_t)spot] + kPi * 0.5f;
    bird_.vel = {};
    bird_.seenNeed = 1.4;
    sbDue_ = -1.0;
}

void Apparitions::birdGone(std::vector<Event>& out) {
    if (bird_.wasSeen && !bird_.told) {
        bird_.told = true;
        out.push_back({Event::Toast, 0, "War da ein blauer Vogel? Da ist nichts. Keine Feder, kein Laut, nichts, was es beweist.", {}, 4.5f});
    }
    bird_.state = 3;
}

void Apparitions::updateBird(double dt, const std::array<PlayerIn, kPlayers>& pl, const std::vector<Viewer>& viewers, bool auth,
                             world::World& w, std::vector<Event>& out) {
    Bird& b = bird_;
    // Sichtung ansetzen (Host): eine Stelle in freier Sicht vom Spieler, die gerade niemand ansieht
    if (auth && b.state == 0 && sbDue_ >= 0.0 && clock_ >= sbDue_ && std::fmod(clock_, 0.5) < dt) {
        for (int id = 0; id < kPlayers && b.state == 0; ++id) {
            const PlayerIn& p = pl[(size_t)id];
            if (!p.alive) continue;
            for (size_t k = 0; k < sbSpots_.size(); ++k) {
                const vec3 s = sbSpots_[k] + vec3{0, 0, 0.16f};
                const float d = length(s - p.eye);
                if (d < 11.0f || d > 28.0f || !clearSight(w, p.eye, s)) continue;
                bool watched = false;
                for (const Viewer& v : viewers) {
                    const vec3 dv = s - v.eye;
                    watched = watched || dot(normalize(dv), v.look) > std::cos(1.15f) || length(dv) < 9.0f;
                }
                if (watched) continue;
                forceBird((int)k);
                b.yaw = std::atan2(p.feet.y - s.y, p.feet.x - s.x) + (rng_.random() < 0.5 ? 1.35f : -1.35f);  // im Profil
                b.seenNeed = rng_.uniform(0.9, 1.8);
                out.push_back({Event::Net, 0, "", b.pos, 1.0f, netCode(NSbAppear, 0, (u32)k)});
                break;
            }
        }
    }
    if (b.state != 1 && b.state != 2) return;
    b.t += dt;
    const bool seen = birdSeen(viewers, w);
    if (seen && !b.wasSeen) out.push_back({Event::SoundAt, 0, "sb_coo", b.pos + vec3{0, 0, 0.2f}, 0.4f});
    if (seen) b.wasSeen = true;
    b.seen = seen ? b.seen + dt : 0.0;
    b.unseen = seen ? 0.0 : b.unseen + dt;
    if (b.state == 1) {
        // fort, sobald man ihn zu genau ansehen oder naeher kommen will (Host entscheidet)
        if (!auth) return;
        bool close = false, lit = false;
        float nearD = 1e9f;
        vec3 from{};
        for (int id = 0; id < kPlayers; ++id) {
            const PlayerIn& p = pl[(size_t)id];
            if (!p.alive) continue;
            const float d = hdist(p.feet, b.pos);
            if (d < nearD) nearD = d, from = p.feet;
            close = close || d < 8.0f;
        }
        for (const Viewer& v : viewers) {
            if (!v.flashOn) continue;
            const vec3 d = b.pos + vec3{0, 0, 0.16f} - v.flashPos;
            lit = lit || (length(d) < 20.0f && dot(normalize(d), v.flashDir) > 0.95f && clearSight(w, v.flashPos, b.pos + vec3{0, 0, 0.16f}));
        }
        if (nearD < 0.7f)  // beruehrt: das Mittel der aeusseren Federn (leichtes Delirium) - so nah laesst er niemanden
            out.push_back({Event::Toast, 0, "Etwas Feuchtes, Blaues streift dich. Die Wände atmen.", {}, 3.0f});
        if (b.wasSeen && b.unseen > 0.35) {  // weggesehen: fort, ohne Spur
            birdGone(out);
            out.push_back({Event::Net, 0, "", b.pos, 1.0f, netCode(NSbGone, 0, 0)});
            return;
        }
        if (b.seen > b.seenNeed || close || lit) {
            const vec3 away = nearD < 1e8f ? normalize(flat(b.pos - from)) : vec3{std::cos(b.yaw), std::sin(b.yaw), 0.0f};
            b.state = 2, b.t = 0.0;
            b.yaw = std::atan2(away.y, away.x);
            b.vel = away * 2.5f + vec3{0, 0, 2.2f};
            out.push_back({Event::SoundAt, 0, "sb_flutter", b.pos + vec3{0, 0, 0.2f}, 0.8f});
            const u32 qy = (u32)std::lround((std::remainder(b.yaw, 2.0f * kPi) + kPi) / (2.0f * kPi) * 65535.0f);
            out.push_back({Event::Net, 0, "", b.pos, 1.0f, netCode(NSbFlee, 0, qy)});
        } else if (!b.wasSeen && b.t > 40.0) {  // nie gesehen: still fort, eine spaetere Sichtung bleibt moeglich
            b.state = 0;
            sbDue_ = clock_ + rng_.uniform(120.0, 300.0);
            out.push_back({Event::Net, 0, "", b.pos, 1.0f, netCode(NSbGone, 1, 0)});
        }
        return;
    }
    // im Flug: schneller, steigt auf Kopfhoehe, fort durch den Gang; weg, sobald ihn niemand mehr sieht
    const vec3 f{std::cos(b.yaw), std::sin(b.yaw), 0.0f};
    b.vel = b.vel + (f * 7.5f - flat(b.vel)) * std::min(1.0f, (float)dt * 2.5f);
    b.vel.z = b.pos.z - sbSpots_[(size_t)std::max(0, b.spot)].z < 1.5f ? b.vel.z : b.vel.z * 0.85f;
    const vec3 next = b.pos + b.vel * (float)dt;
    const bool blocked = !clearSight(w, b.pos + vec3{0, 0, 0.16f}, next + f * 0.4f + vec3{0, 0, 0.16f});
    b.pos = next;
    if ((b.unseen > 0.2 || blocked || b.t > 3.0) && (auth || b.t > 4.0)) {
        birdGone(out);
        if (auth) out.push_back({Event::Net, 0, "", b.pos, 1.0f, netCode(NSbGone, 0, 0)});
    }
}

// --- Photoshop ------------------------------------------------------------------------------------------------------

void Apparitions::forceShop(int spot) {
    if (spot < 0 || spot >= (int)psSpots_.size()) return;
    shop_ = Shop{};
    shop_.present = true, shop_.spot = spot;
    const PsSpot& ps = psSpots_[(size_t)spot];
    shop_.pos = ps.pos, shop_.yaw = ps.yaw, shop_.screen = ps.screen, shop_.screenYaw = ps.screenYaw;
    setupParts();
}

void Apparitions::startBox(int victim, vec3 at, std::vector<Event>& out) {
    Shop& s = shop_;
    s.state = 1, s.t = 0.0, s.victim = victim, s.boxC = at, s.half = 1.0f, s.killed = false;
    out.push_back({Event::SoundAt, 0, "ps_box", at + vec3{0, 0, 1.0f}, 1.0f});
    out.push_back({Event::Toast, victim,
                   "Eine durchsichtige Box mit schwarzen Kanten schließt dich ein – und schrumpft. Nicht hindurchgehen. "
                   "Gib ihm etwas: hinwerfen (Q)!", {}, 4.5f});
    out.push_back({Event::Net, victim, "", at, 1.0f, netCode(NPsBox, (u32)victim, 0)});
}

bool Apparitions::wantsOffering(vec3& center, float& radius) const {
    if (!shop_.present || shop_.state > 1) return false;
    center = shop_.pos, radius = 2.3f;
    return true;
}

void Apparitions::offered(vec3 itemPos, const std::string& itemModel, std::vector<Event>& out) {
    Shop& s = shop_;
    if (!s.present || s.state > 1) return;
    s.state = 2, s.t = 0.0, s.item = itemPos;
    s.offer.model = itemModel, s.offer.id = -2;
    out.push_back({Event::SoundAt, 0, "ps_arm", s.screen, 1.0f});
}

void Apparitions::updateShop(double dt, const std::array<PlayerIn, kPlayers>& pl, bool auth, std::vector<Event>& out) {
    Shop& s = shop_;
    if (!s.present || s.state == 3) return;
    s.t += dt;
    s.cool = std::max(0.0, s.cool - dt);
    const vec3 f{std::cos(s.yaw), std::sin(s.yaw), 0.0f}, l{-f.y, f.x, 0.0f};
    // Glitch: die Scheiben des Klotzes verrutschen in Stoessen
    if ((s.glitchT -= dt) <= 0.0) {
        s.burst = s.state == 2 && s.t > 1.5 ? 1.2 : rng_.uniform(0.12, 0.45);
        s.glitchT = s.state == 1 ? rng_.uniform(0.25, 0.7) : rng_.uniform(1.4, 4.0);
        if (pl[0].alive && hdist(pl[0].feet, s.pos) < 14.0f) out.push_back({Event::SoundAt, 0, "ps_glitch", s.pos + vec3{0, 0, 1.6f}, 0.55f});
    }
    s.burst -= dt;
    for (int k = 0; k < 4; ++k) s.slice[k] = s.burst > 0.0 ? (float)rng_.uniform(-0.11, 0.11) * (s.state == 1 ? 1.4f : 1.0f) : 0.0f;
    // Annaeherung (eigener Spieler): Fiepen des Monitors, beim ersten Blick ein Hinweis
    if (pl[0].alive) {
        const float d = hdist(pl[0].feet, s.pos);
        if (d < 5.0f && (s.hintT -= dt) <= 0.0) {
            out.push_back({Event::SoundAt, 0, "ps_whine", s.screen, 0.45f});
            s.hintT = 2.2;
        }
        const vec3 toS = s.pos + vec3{0, 0, 1.5f} - pl[0].eye;
        if (!s.hinted && d < 7.5f && dot(normalize(toS), pl[0].look) > 0.75f) {
            s.hinted = true;
            out.push_back({Event::Toast, 0, "Neben dem Rechner ein Klotz, über zwei Meter hoch – Fotos von Augen, ein aufgerissenes "
                                            "Hundemaul, Fleisch auf allen Seiten. Er flackert. Abstand halten.", {}, 5.0f});
        }
    }
    if (s.state == 0) {
        if (!auth || s.cool > 0.0) return;
        for (int id = 0; id < kPlayers; ++id) {  // naeher als 5 Fuss an den Klotz (Abstand zu seiner Grundflaeche)
            const PlayerIn& p = pl[(size_t)id];
            if (!p.alive) continue;
            const vec3 rel = flat(p.feet - s.pos);
            const float ax = std::max(0.0f, std::fabs(dot(rel, f)) - kPsW * 0.5f), ay = std::max(0.0f, std::fabs(dot(rel, l)) - kPsW * 0.5f);
            if (std::hypot(ax, ay) < (float)cfg_.boxRange && std::fabs(p.feet.z - s.pos.z) < 1.5f) {
                startBox(id, p.feet, out);
                break;
            }
        }
        return;
    }
    if (s.state == 1) {
        if (s.killed) {  // zugeklappt: noch einen Augenblick, dann lauert er wieder
            s.half = std::max(0.0f, s.half - (float)dt * 3.0f);
            if (s.t > 0.4) s.state = 0, s.cool = 8.0, s.victim = -1, s.killed = false;
            return;
        }
        s.half = std::max(0.2f, 1.0f - (float)(s.t / cfg_.boxTime) * 0.8f);
        if (!auth) return;
        const PlayerIn* v = s.victim >= 0 && s.victim < kPlayers ? &pl[(size_t)s.victim] : nullptr;
        bool kill = s.t >= cfg_.boxTime, why = false;
        if (v && v->alive) {
            const vec3 rel = flat(v->feet - s.boxC);
            if (std::fabs(dot(rel, f)) > s.half + 0.02f || std::fabs(dot(rel, l)) > s.half + 0.02f) kill = true, why = true;
        } else {
            s.state = 0, s.cool = 4.0;  // das Opfer ist fort (anders gestorben, getrennt)
            return;
        }
        if (kill) {
            out.push_back({Event::SoundAt, 0, "ps_crop", v->feet + vec3{0, 0, 1.0f}, 1.0f});
            out.push_back({Event::Died, s.victim, "photoshop", v->feet, why ? 1.0f : 0.0f});
            out.push_back({Event::Net, s.victim, "", s.boxC, 1.0f, netCode(NPsKill, (u32)s.victim, 0)});
            s.killed = true, s.t = 0.0;
        }
        return;
    }
    // Gabe: der Arm greift aus dem Bildschirm, holt den Gegenstand, das Bild zeigt ihn; dann ist Photoshop fort
    if (s.t > 0.7 && s.t - dt <= 0.7) out.push_back({Event::SoundAt, 0, "ps_release", s.boxC + vec3{0, 0, 1.0f}, 0.8f});
    if (s.t > 2.7) {
        s.state = 3;
        out.push_back({Event::SoundAt, 0, "ps_glitch", s.pos + vec3{0, 0, 1.4f}, 0.9f});
        if (auth) out.push_back({Event::Net, 0, "", s.pos, 1.0f, netCode(NPsGone, 0, 0)});
    }
}

// --- Macchina -------------------------------------------------------------------------------------------------------

void Apparitions::updateRelics(double dt, const std::array<PlayerIn, kPlayers>& pl, std::vector<Event>& out) {
    if (!pl[0].alive) return;
    for (Relic& r : relics_) {
        const float d = hdist(r.pos, pl[0].feet);
        if (d < 2.6f && !add_[0].macchinaSeen) {
            const vec3 to = r.pos + vec3{0, 0, 0.7f} - pl[0].eye;
            if (dot(normalize(to), pl[0].look) > 0.6f) {
                add_[0].macchinaSeen = true;
                out.push_back({Event::Toast, 0, kMacchinaText, {}, 7.0f});
                out.push_back({Event::SoundAt, 0, "mc_creak", r.pos + vec3{0, 0, 0.8f}, 0.7f});
            }
        }
        if (d < 10.0f && (r.creakT -= dt) <= 0.0) {  // das Blech arbeitet im Luftzug
            out.push_back({Event::SoundAt, 0, "mc_creak", r.pos + vec3{0, 0, 0.9f}, 0.35f});
            r.creakT = rng_.uniform(18.0, 40.0);
        }
    }
}

// --- Koop -----------------------------------------------------------------------------------------------------------

void Apparitions::applyNet(u32 code, vec3 pos, const std::string& itemModel, std::vector<Event>& out) {
    const u32 kind = code >> 24, a = (code >> 16) & 0xFFu, b = code & 0xFFFFu;
    switch (kind) {
        case NCcVend:
            if (a < cc_.size()) {
                Cuisine& c = cc_[a];
                c.stock = std::max(0, c.stock - 1), c.vendT = 0.0;
                out.push_back({Event::SoundAt, 0, "cc_keys", c.pos + vec3{std::cos(c.yaw), std::sin(c.yaw), 0} * kCcFront + vec3{0, 0, 1.2f}, 0.7f});
            }
            break;
        case NCcAnger:
            if (a < cc_.size()) startAnger(cc_[a], -1, out);
            break;
        case NSbAppear:
            if (b < sbSpots_.size()) {
                forceBird((int)b);
                bird_.pos = pos;
            }
            break;
        case NSbFlee:
            if (bird_.state == 1) {
                bird_.state = 2, bird_.t = 0.0, bird_.pos = pos;
                bird_.yaw = (float)b / 65535.0f * 2.0f * kPi - kPi;
                bird_.vel = vec3{std::cos(bird_.yaw), std::sin(bird_.yaw), 0.0f} * 2.5f + vec3{0, 0, 2.2f};
                out.push_back({Event::SoundAt, 0, "sb_flutter", pos + vec3{0, 0, 0.2f}, 0.8f});
            }
            break;
        case NSbGone:
            if (bird_.state == 1 || bird_.state == 2) {
                if (a == 1) bird_.state = 0;
                else birdGone(out);
            }
            break;
        case NPsBox:
            if (shop_.present) startBox((int)a, pos, out);
            break;
        case NPsFed:
            if (shop_.present) offered(pos, itemModel, out);
            break;
        case NPsGone: shop_.state = 3; break;
        case NPsKill:
            if (shop_.present && shop_.state == 1) {
                out.push_back({Event::SoundAt, 0, "ps_crop", pos + vec3{0, 0, 1.0f}, 1.0f});
                shop_.killed = true, shop_.t = 0.0;
            }
            break;
        default: break;
    }
}

// --- Skript und Test ------------------------------------------------------------------------------------------------

void Apparitions::setState(const std::string& field, double v) {
    std::vector<Event> sink;
    if (field == "hold")
        hold_ = v > 0.5;
    else if (field == "cc_vend")
        for (Cuisine& c : cc_) c.vendT = v;
    else if (field == "cc_anger")
        for (Cuisine& c : cc_) {
            c.angerT = v, c.angerLen = std::max(5.6, v + 1.0), c.strikeT = 10.0;
            const bool o = v > 0.75 && v < c.angerLen - 0.7;
            c.lift = o ? 0.12f : 0.0f;
        }
    else if (field == "cc_lurch")
        for (Cuisine& c : cc_) c.lurch = (float)v;
    else if (field == "cc_arm")
        for (Cuisine& c : cc_) c.strikeArm = (int)v, c.strikeT = 0.425;
    else if (field == "sb_flee" && bird_.state == 1) {
        bird_.state = 2, bird_.t = v;
        const vec3 f{std::cos(bird_.yaw), std::sin(bird_.yaw), 0.0f};
        bird_.pos = bird_.pos + f * (float)(v * 4.0) + vec3{0, 0, (float)std::min(1.3, v * 2.2)};
        bird_.vel = f * 6.0f;
    } else if (field == "sb_t")
        bird_.t = v;
    else if (field == "sb_clear")
        bird_.state = 0;
    else if (field == "ps_box" && shop_.present) {
        const vec3 f{std::cos(shop_.yaw), std::sin(shop_.yaw), 0.0f};
        startBox(0, shop_.pos + f * 1.5f, sink);
        shop_.t = v * cfg_.boxTime, shop_.half = std::max(0.2f, 1.0f - (float)v * 0.8f);
    } else if (field == "ps_fed" && shop_.present) {
        const vec3 f{std::cos(shop_.yaw), std::sin(shop_.yaw), 0.0f};
        offered(shop_.pos + f * 1.2f + vec3{std::sin(shop_.yaw), -std::cos(shop_.yaw), 0} * 0.3f, "almond", sink);
        shop_.t = v;
    } else if (field == "ps_glitch")
        shop_.burst = v, shop_.glitchT = 10.0;
}

// --- Bild -----------------------------------------------------------------------------------------------------------

void Apparitions::collect(std::vector<VisibleMesh>& out, vec3 eye) {
    // Cannibal Cuisine
    for (Cuisine& c : cc_) {
        if (length(c.pos - eye) > 70.0f) continue;
        const vec3 f{std::cos(c.yaw), std::sin(c.yaw), 0.0f};
        const mat4 M = mat4::translation(c.pos + f * c.lurch + vec3{0, 0, c.lift}) * mat4::rotationZ(c.yaw);
        push(out, c.body, M, c.angerT >= 0.0 || c.lurch > 0.0f || c.lift > 0.0f, 1.3f);
        // Klappen: zu, beim Zorn auf (oben hoch, unten herunter)
        float open = 0.0f;
        if (c.angerT >= 0.0) {
            const float t = (float)c.angerT, L = (float)c.angerLen;
            open = t < 0.6f ? 0.06f * std::fabs(std::sin(t * 47.0f)) : smooth01((t - 0.6f) / 0.25f) * (1.0f - smooth01((t - (L - 0.45f)) / 0.4f));
        }
        push(out, c.flapTop, M * mat4::translation(kCcFlapTop) * rotY(-open * 1.75f), open > 0.0f);
        push(out, c.flapBot, M * mat4::translation(kCcFlapBot) * rotY(open * 1.62f), open > 0.0f);
        // Ware: die Hand schiebt sie durch das leere Fach vor, sie kippt ueber die Kante und faellt ins Ausgabefach
        if (c.vendT >= 0.0) {
            const float t = (float)c.vendT;
            const float yy = kCcGy0 + 0.1f + 0.19f * (float)c.slotCol, z = kCcGz0 + 0.005f + 0.2f * (float)c.slotRow;
            const float push01 = smooth01((t - 0.38f) / 0.7f);
            float wx = -0.17f + (0.31f - -0.17f) * push01, wz = z, tilt = 0.0f;
            if (t > 1.08f) {  // kippt und faellt
                const float ft = t - 1.08f;
                tilt = std::min(1.0f, ft / 0.12f) * 1.45f;
                wx = 0.31f - std::min(0.06f, ft * 0.4f);
                wz = std::max(0.375f, z - 0.5f * 9.81f * ft * ft);
                if (wz <= 0.375f) tilt = 0.12f;  // liegt quer im Fach
            }
            if (t < 2.4f) push(out, c.ware, M * mat4::translation(vec3{wx, yy, wz}) * rotY(tilt));
            const float hx = -0.17f + (0.25f - -0.17f) * push01 - (t > 1.3f ? smooth01((t - 1.3f) / 0.8f) * 0.18f : 0.0f);
            if (t < 2.2f)
                push(out, c.vendHand, M * mat4::translation(vec3{hx - 0.075f, yy, z + 0.03f}) * mat4::scale(vec3{0.4f, 0.4f, 0.4f}));
        }
        if (c.angerT < 0.0) continue;
        // Arme: zwei oben (Hiebe), zwei unten (stemmen den Automaten hoch, Fuesse am Boden)
        const float t = (float)c.angerT, L = (float)c.angerLen;
        const float ext = smooth01((t - 0.7f) / 0.4f) * (1.0f - smooth01((t - (L - 0.7f)) / 0.55f));
        if (ext <= 0.001f) continue;
        const mat4 Mi = M;
        for (int a = 0; a < 4; ++a) {
            const bool upper = a < 2;
            const float side = (a % 2 == 0) ? 1.0f : -1.0f;
            vec3 sh = upper ? kCcShoulderUp : kCcShoulderLow;
            sh.y *= side;
            const vec3 S = apply(Mi, sh);
            vec3 target, bend;
            if (upper) {
                // Hieb: ausholen (hoch) und niederschlagen, die Arme abwechselnd
                // Runde 3: der ruhende Arm lauert auf Brusthoehe vor dem Automaten (vorher hoch wie ein Fuehler)
                const float phase = (c.strikeArm == (a % 2)) ? (float)std::clamp(1.0 - c.strikeT / 0.85, 0.0, 1.0) : 0.36f;
                const float swingT = phase < 0.55f ? phase / 0.55f : 1.0f - (phase - 0.55f) / 0.45f;
                // Runde 2: nach vorn zum Ziel statt seitlich hoch (wirkte wie Fuehler)
                const vec3 reach = vec3{kCcFront + 0.75f + 0.6f * swingT, side * (0.38f - 0.2f * swingT), 2.05f - 0.95f * swingT};
                target = apply(Mi, reach);
                bend = apply(Mi, vec3{0.1f, side * 0.45f, 1.0f}) - apply(Mi, vec3{0, 0, 0});
            } else {
                // Beine: die gespreizten Haende stehen vor dem Automaten auf dem Boden
                target = c.pos + f * (kCcFront + 0.72f + c.lurch * 0.6f) + vec3{-f.y, f.x, 0.0f} * (side * 0.62f) + vec3{0, 0, 0.02f};
                bend = vec3{0, 0, 1.0f} + vec3{-f.y, f.x, 0.0f} * side * 0.6f;
            }
            // ausfahren: vom eingeklappten Platz in der Klappe zur Zielhaltung
            const vec3 rest = S + (apply(Mi, vec3{-0.3f, sh.y, sh.z}) - S) * 1.0f;
            const vec3 T = rest + (target - rest) * ext;
            const float l1 = kCcArm[0], l2 = kCcArm[1] + kCcArm[2] * 0.6f;
            const vec3 E = elbowOf(S, T, l1, l2, bend);
            const vec3 sideAxis = apply(Mi, vec3{0, 1, 0}) - apply(Mi, vec3{0, 0, 0});
            push(out, c.arm[a][0], along(S, E - S, sideAxis));
            const vec3 W = E + normalize(T - E) * kCcArm[1];
            push(out, c.arm[a][1], along(E, W - E, sideAxis));
            vec3 hd = normalize(T - E);
            if (!upper) hd = normalize(hd + vec3{0, 0, -1.2f} + f * 0.5f);  // Hand flach auf den Boden
            push(out, c.arm[a][2], along(W, hd, sideAxis));
        }
    }
    // Swindlebird
    Bird& b = bird_;
    if ((b.state == 1 || b.state == 2) && length(b.pos - eye) < 60.0f) {
        const vec3 f{std::cos(b.yaw), std::sin(b.yaw), 0.0f}, l{-f.y, f.x, 0.0f};
        const float t = (float)b.t;
        const bool fly = b.state == 2;
        // Haltung: aufrecht wie ein Papagei auf dem Boden, Kopf kippt und dreht, putzt sich ab und zu
        // Runde 2: aufrechter, die Beine eingeknickt (vorher stelzig) - das dritte Gelenk bleibt sichtbar
        const float bodyPitch = fly ? 0.12f : 0.78f + 0.03f * std::sin(t * 1.7f);
        const float a1 = 0.5f, a2 = 2.3f;  // Unterschenkel nach vorn unten, Lauf steil nach hinten unten, letztes Glied senkrecht
        const mat4 R = mat4::rotationZ(b.yaw) * pitchUp(bodyPitch);
        const vec3 hipL{0.01f, 0.0f, -0.012f};
        const vec3 hipW = apply(R, hipL);
        const float drop = kSbLeg[0] * std::sin(a1) + kSbLeg[1] * std::sin(a2) + kSbLeg[2] + 0.004f;
        // Runde 3: "ungewoehnlich maechtig" (Level-2-Meldung) - groesser als ein gewoehnlicher (Ara-Groesse)
        constexpr float kSc = 1.6f;
        const vec3 root = fly ? b.pos : b.pos + vec3{0, 0, (drop - hipW.z) * kSc};
        const mat4 Sc = mat4::scale(vec3{kSc, kSc, kSc});
        const mat4 B = mat4::translation(root) * Sc * R;
        push(out, b.body, B);
        float hTurn = 0.6f * std::sin(t * 0.9f) * std::sin(t * 0.37f + 1.0f), hTilt = 0.35f * std::sin(t * 1.3f + 0.5f), hPitch = -bodyPitch + 0.1f;
        const float preen = fly ? 0.0f : std::max(0.0f, std::sin(t * 0.55f - 1.2f));
        if (preen > 0.6f) hTurn = 2.3f * (preen - 0.6f) / 0.4f, hPitch -= 0.9f * (preen - 0.6f) / 0.4f;  // Schnabel ins Gefieder
        if (b.wasSeen && !fly) hTurn = hTurn * 0.3f + 0.9f, hTilt = 0.5f;  // sieht einen an - mit einem Auge
        push(out, b.head, B * mat4::translation(kSbNeck) * pitchUp(hPitch) * mat4::rotationZ(hTurn) * rotX(hTilt));
        // Fluegel: angelegt; im Flug weit ausgebreitet und schlagend
        for (int s = 0; s < 2; ++s) {
            const float sg = s == 0 ? 1.0f : -1.0f;
            vec3 sh = kSbShoulder;
            sh.y *= sg;
            float spread = fly ? 1.45f : 0.0f, flap = fly ? 0.9f * std::sin(t * 26.0f) : 0.0f;
            if (!fly && t < 0.6f) spread = 0.0f;
            const mat4 Wi = B * mat4::translation(sh) * mat4::rotationZ(sg * spread) * rotX(sg * flap);
            push(out, b.wingIn[s], Wi);
            push(out, b.wingOut[s], Wi * mat4::translation(vec3{-kSbWrist, sg * 0.01f, -0.006f}) * mat4::rotationZ(sg * (fly ? -0.35f : 0.0f)) *
                                        rotX(sg * flap * 0.5f));
        }
        push(out, b.tail, B * mat4::translation(kSbTail) * pitchUp(fly ? 0.2f : -0.25f + 0.08f * std::sin(t * 2.3f)));
        // Beine: drei Glieder je Bein (die Gattung teilt den Knochenbau der roten Art); im Flug angezogen
        for (int s = 0; s < 2; ++s) {
            const float sg = s == 0 ? 1.0f : -1.0f;
            const vec3 hip = apply(B, hipL + vec3{0, sg * 0.028f, 0});
            vec3 d1, d2, d3;
            if (fly) {
                d1 = normalize(-f * 0.8f + vec3{0, 0, -0.6f}), d2 = normalize(-f + vec3{0, 0, -0.2f}), d3 = normalize(-f + vec3{0, 0, 0.1f});
            } else {
                d1 = f * std::cos(a1) + vec3{0, 0, -std::sin(a1)};
                d2 = f * std::cos(a2) + vec3{0, 0, -std::sin(a2)};
                d3 = vec3{0, 0, -1.0f};
            }
            const vec3 k1 = hip + d1 * (kSbLeg[0] * kSc), k2 = k1 + d2 * (kSbLeg[1] * kSc);
            push(out, b.shin[s], along(hip, d1, l) * Sc);
            push(out, b.tars[s], along(k1, d2, l) * Sc);
            if (fly) push(out, b.foot[s], along(k2, d3, l) * Sc);
            else push(out, b.foot[s], basis(vec3{0, 0, -1}, -l, -f, k2) * Sc);  // Zehen: zwei vor, zwei zurueck
        }
    }
    // Photoshop
    Shop& s = shop_;
    if (!s.present || length(s.pos - eye) > 60.0f) return;
    const vec3 f{std::cos(s.yaw), std::sin(s.yaw), 0.0f}, l{-f.y, f.x, 0.0f};
    const mat4 P = mat4::translation(s.pos) * mat4::rotationZ(s.yaw);
    if (s.state != 3)
        for (int k = 0; k < 4; ++k) push(out, s.block[k], P * mat4::translation(vec3{s.slice[k] * 0.3f, s.slice[k], 0.0f}), s.burst > 0.0, 1.0f);
    // Bild auf dem Monitor: sein Bild; nach der Gabe der genommene Gegenstand (flach wie ein Foto)
    const mat4 S = mat4::translation(s.screen) * mat4::rotationZ(s.screenYaw);
    const bool showItem = s.state == 3 || (s.state == 2 && s.t > 1.5);
    if (!showItem) push(out, s.screenImg, S * mat4::translation(vec3{0.003f, 0.0f, 0.0f}) * mat4::scale(vec3{1.0f, 0.95f, 0.95f}), s.burst > 0.0);
    else if (!s.offer.model.empty())
        push(out, s.offer, S * mat4::translation(vec3{0.004f, 0.0f, -0.1f}) * mat4::scale(vec3{0.05f, 0.8f, 0.8f}), false);
    // Box: zwoelf schwarze Kanten, Anfasser an den Ecken (wie das Freistellwerkzeug)
    if (s.state == 1 && s.half > 0.01f) {
        const float h = s.half, H = std::max(0.05f, 2.15f * h);
        const mat4 Bx = mat4::translation(s.boxC) * mat4::rotationZ(s.yaw);
        auto edge = [&](vec3 a, vec3 d, float len) {
            push(out, s.edge, Bx * mat4::translation(a) * along(vec3{0, 0, 0}, d, vec3{0, 1, 0}) * mat4::scale(vec3{len, 1.5f, 1.5f}));
        };
        for (float sy : {-1.0f, 1.0f})
            for (float z : {0.0f, H}) edge(vec3{-h, sy * h, z}, vec3{1, 0, 0}, 2.0f * h);
        for (float sx : {-1.0f, 1.0f})
            for (float z : {0.0f, H}) edge(vec3{sx * h, -h, z}, vec3{0, 1, 0}, 2.0f * h);
        for (float sx : {-1.0f, 1.0f})
            for (float sy : {-1.0f, 1.0f}) edge(vec3{sx * h, sy * h, 0.0f}, vec3{0, 0, 1}, H);
        for (float sx : {-1.0f, 1.0f})
            for (float sy : {-1.0f, 1.0f})
                for (float z : {0.0f, H}) push(out, s.handle, Bx * mat4::translation(vec3{sx * h, sy * h, z}) * mat4::scale(vec3{1.4f, 1.4f, 1.4f}));
        for (float sx : {-1.0f, 1.0f}) push(out, s.handle, Bx * mat4::translation(vec3{sx * h, 0.0f, H}));
        for (float sy : {-1.0f, 1.0f}) push(out, s.handle, Bx * mat4::translation(vec3{0.0f, sy * h, H}));
    }
    // Arm aus dem Bildschirm: greift den Gegenstand und zieht ihn hinein
    if (s.state == 2) {
        const float t = (float)s.t;
        const float e = t < 0.7f ? smooth01(t / 0.7f) : 1.0f - smooth01((t - 0.7f) / 0.8f);
        const vec3 sd{std::cos(s.screenYaw), std::sin(s.screenYaw), 0.0f};
        const vec3 S0 = s.screen - sd * 0.05f;
        const vec3 grab = s.item + vec3{0, 0, 0.08f};
        const vec3 T = S0 + sd * 0.15f + (grab - (S0 + sd * 0.15f)) * e;
        const float dist = length(T - S0);
        // Runde 2: der Arm waechst aus dem Bildschirm (Laenge folgt der Reichweite, fast gestreckt); weiter als er lang
        // ist, dehnt er sich - vorher faltete er sich beim Zurueckziehen nach oben aus dem Monitor
        const float st = dist / ((kPsArm[0] + kPsArm[1] + kPsHand * 0.6f) * 0.96f);
        const float l1 = kPsArm[0] * st, l2 = (kPsArm[1] + kPsHand * 0.6f) * st;
        if (dist > 0.12f && e > 0.1f) {  // Runde 3: ganz zurueckgezogen ist er im Bildschirm (kein Stummel mit Hand)
            const vec3 E = elbowOf(S0, T, l1, l2, vec3{0, 0, 1} + sd * 0.3f);
            const vec3 side{-sd.y, sd.x, 0.0f};
            push(out, s.armA, along(S0, E - S0, side) * mat4::scale(vec3{st, 1.0f, 1.0f}));
            const vec3 W = E + normalize(T - E) * kPsArm[1] * st;
            push(out, s.armB, along(E, W - E, side) * mat4::scale(vec3{st, 1.0f, 1.0f}));
            s.hand.model = t < 0.7f ? "prop:ps_hand@0" : "prop:ps_hand@1";
            if (t >= 0.68f && t < 0.72f) s.hand.id = -2;
            const float hs = std::clamp(st * 1.15f, 0.35f, 1.0f);  // die Hand waechst mit dem Arm aus dem Bild
            push(out, s.hand, along(W, T - W + vec3{0, 0, -0.05f}, side) * mat4::scale(vec3{hs, hs, hs}));
            if (t >= 0.7f && t < 1.42f && !s.offer.model.empty()) push(out, s.offer, mat4::translation(T + vec3{0, 0, -0.06f}) * mat4::rotationZ(s.yaw), true);
        }
        if (t < 0.7f && !s.offer.model.empty()) push(out, s.offer, mat4::translation(s.item) * mat4::rotationZ(s.yaw + 0.6f), false);
    }
}

void Apparitions::lights(std::vector<Light>& out, vec3 eye) const {
    // Automaten: ihr Licht bleibt im Stromausfall an (Wegmarke) - die naechsten zwei
    static const vec3 kSign[3] = {{1.0f, 0.36f, 0.26f}, {0.42f, 0.62f, 1.0f}, {1.0f, 0.62f, 0.3f}};
    std::vector<std::pair<float, const Cuisine*>> near;
    for (const Cuisine& c : cc_)
        if (const float d = length(c.pos - eye); d < 34.0f) near.push_back({d, &c});
    std::sort(near.begin(), near.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    for (size_t i = 0; i < near.size() && i < 2; ++i) {
        const Cuisine& c = *near[i].second;
        const vec3 f{std::cos(c.yaw), std::sin(c.yaw), 0.0f};
        out.push_back({c.pos + f * (c.lurch + kCcFront + 0.35f) + vec3{0, 0, 1.25f + c.lift}, (vec3{1.0f, 0.93f, 0.82f} * 0.75f + kSign[c.variant] * 0.45f) * c.flicker, 7.5f, f});
    }
    if (shop_.present && length(shop_.screen - eye) < 20.0f) {
        const vec3 sd{std::cos(shop_.screenYaw), std::sin(shop_.screenYaw), 0.0f};
        const float k = shop_.burst > 0.0 ? 0.35f : 1.0f;
        out.push_back({shop_.screen + sd * 0.25f, vec3{1.0f, 0.3f, 0.26f} * (shop_.state == 3 ? 0.18f : 0.5f * k), 3.6f, sd});
    }
}

// --- Spielstand -----------------------------------------------------------------------------------------------------

Json Apparitions::toJson(int id) const {
    const Addict& a = add_[(size_t)id];
    Json j = Json::object();
    j.set("addicted", Json(a.on));
    j.set("eaten", Json((double)a.eaten));
    j.set("macchina", Json(a.macchinaSeen));
    return j;
}

void Apparitions::fromJson(int id, const Json& j) {
    Addict& a = add_[(size_t)id];
    a = Addict{};
    if (!j.isObject()) return;
    a.on = j["addicted"].asBool(false);
    a.eaten = (int)j["eaten"].asInt(0);
    a.macchinaSeen = j["macchina"].asBool(false);
}

}  // namespace lim::game
