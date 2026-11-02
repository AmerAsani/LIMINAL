// V13.3 Teil 4: Jerry (KREATUR_Jerry.md; Wikidot Entity 7 Rev. 14, Level 2 Rev. 93).
//
// Wiki: ein blauer Ara (wie der Hyazinth-Ara), spricht und denkt wie ein erwachsener Mensch. Wer ihm begegnet, kann
// umprogrammiert werden - kurz starr, dann eine Geste der Ehrfurcht, danach gehoert er zu den Followers of Jerry; was es
// ausloest, ist unklar. Wer nach dem ersten Blick sofort wegrennt, ueberlebt (Videos 2014, Level 2 2018 - dort stand er auf
// einem rostigen Rohr); wer naeher geht und ihn beruehren will, nicht (2019). Er springt Feinde an (2022, mit Kreischen).
// Level 2: an seinen Raum in der Basis der Followers gebunden; wen er oder sie bemerken, der entkommt nicht.
//
// Umsetzung (Interpretation, im Briefing begruendet): Jerry sitzt auf einem rostigen Rohr in seinem Raum (Spawnpunkt
// "entity"/"jerry" der Karte, tools/levels/level2.py); er verlaesst das Rohr nie (nur zum Sprung). Die Gefahr ist der
// Blickkontakt:
//   Ankuendigung  er murmelt vor sich hin (ruhige Menschenstimme, man hoert ihn vor dem Raum), putzt sich, knirscht mit dem
//                 Schnabel; die Spuren der Followers markieren Gang und Raum (Karte).
//   Bemerken      sieht er einen Spieler (freie Sicht, 17 m), springt sein Kopf herum; dann spricht er ihn an, die
//                 Halskrause straeubt sich, die Pupillen verengen sich.
//   Bann          nur bei gegenseitigem Blick (sein Kopf zeigt auf den Spieler, der Spieler sieht ihm ins Gesicht): waechst
//                 in etwa 3 s (aus der Naehe schneller); wegsehen oder ausser Sicht gehen laesst ihn abklingen. Ab 0,3
//                 klingelt es (Event::Spell), ab 0,45 zieht er den Blick zu sich und laehmt die Schritte (Sitzung), ab 0,85
//                 breitet er die Fluegel aus und kreischt (Announce - letzte Warnung), bei 1 ist der Spieler unterworfen
//                 (Attack; Zustand 10: Fluegel weit, die Kamera erstarrt an seinem Gesicht).
//   Naehe         wer bis auf 1,6 m herankommt (wie zum Beruehren), dem springt er nach 0,55 s Ankuendigung (Fluegel,
//                 Kreischen) ins Gesicht - zurueckweichen rettet.
//   Rueckzug      ist keiner mehr zu sehen, wendet er sich ab, plustert sich auf, putzt sich.
// Mehrspieler: der Host rechnet den Bann je Spieler (jerryGaze, alle Spieler); die Puppe beim Gast traegt im Kanal glow den
// Bann ihres Ziels (target), daraus Blickzug und Laehmung beim Gast; Laute ueber die vorhandenen Ereignisse.
#include "gameplay/presence.hpp"
#include "gameplay/presence_agent.hpp"

#include "core/log.hpp"

namespace lim::game {

namespace {
constexpr float kSee = 17.0f;        // so weit sieht er (freie Sicht)
constexpr float kContactJ = 0.88f;   // sein Blick zeigt auf den Spieler (cos 28 Grad)
constexpr float kContactP = 0.94f;   // der Spieler sieht ihm ins Gesicht (cos 20 Grad)
constexpr float kRate = 0.32f;       // Bann je s bei Blickkontakt (aus der Ferne; nah bis doppelt)
constexpr float kLunge = 1.6f;       // so nah: er springt
struct JerryAg {
    vec3 perch;                // Wurzel auf dem Rohr
    float home = 0.0f;         // Blickrichtung des Koerpers in Ruhe (zur Tuer)
    vec3 gaze{1, 0, 0};        // wohin sein Kopf zeigt (Welt)
    vec3 want{1, 0, 0};        // wohin er sehen will
    int fixOn = -1;            // Spieler, den er fixiert
    vec3 fixEye;               // dessen Auge
    double noticeT = 0.0, lostT = 99.0, talkT = 1.5, mutterT = 4.0, preenT = 8.0, preenOn = 0.0, flapT = 22.0, flapOn = 0.0;
    double lookT = 0.0, grindT = 5.0, talkOn = 0.0;
    int lungeOn = -1;          // Ziel des Sprungs (Ankuendigung)
    double lungeT = 0.0;
    bool flying = false;       // Sprungflug zum Gesicht (Zugriff)
    vec3 flyFrom, killEye, killDir{1, 0, 0};
    double backT = -1.0;       // Rueckflug aufs Rohr
    int subdued = -1;          // wen er gerade unterwirft
};
template <class A>
JerryAg& jag(A& a) {
    if (!a.ext) a.ext = std::make_shared<JerryAg>();
    return *static_cast<JerryAg*>(a.ext.get());
}
template <class A>
const JerryAg* jagc(const A& a) { return a.ext ? static_cast<const JerryAg*>(a.ext.get()) : nullptr; }
vec3 turnToward(vec3 g, vec3 w, float maxA) {  // Kopf dreht hoechstens maxA (rad) in Richtung w
    const float d = std::clamp(dot(g, w), -1.0f, 1.0f), ang = std::acos(d);
    if (ang <= maxA || ang < 1e-4f) return w;
    vec3 ax = cross(g, w);
    if (length(ax) < 1e-5f) ax = {0, 0, 1};
    ax = normalize(ax);
    const float c = std::cos(maxA), s = std::sin(maxA);
    return normalize(g * c + cross(ax, g) * s + ax * (dot(ax, g) * (1.0f - c)));
}
}  // namespace

// --- Auftreten: einmal je Ebene auf dem Rohr seines Raums; fort, wenn weit weg und lange ungesehen -----------------------
bool Presence::spawnJerry(const Context& c, std::vector<Event>& out, bool forced) {
    (void)out;
    if (!c.world || !c.player) return false;
    for (const auto& a : agents_)
        if (a->role == Role::Jerry && !a->showcase && !a->puppet) return false;  // es gibt nur einen (Wiki: einzeln)
    const world::Sector* sec = c.world->sectorIfReady(c.player->x, c.player->y);
    if (!sec) return false;
    for (const auto& sp : sec->entities) {
        if (sp.item != "jerry") continue;
        const world::Tile* t = c.world->tileIfLoaded(ifloor(sp.x), ifloor(sp.y));
        if (!t) return false;
        const vec3 perch{sp.x, sp.y, t->floor + anim::jy::kPerchZ};
        // ungesehen: der Raum liegt ausser Sicht jedes Spielers (Ausnahme: erzwungen, Skripte und Tests)
        if (!forced) {
            if (perceive(c.view, *c.world, perch, 0.5f).level > 0.05f) return false;
            if (c.others)
                for (const Other& o : *c.others)
                    if (perceive(o.view, *c.world, perch, 0.5f).level > 0.05f) return false;
        }
        auto a = std::make_unique<Agent>();
        a->role = Role::Jerry;
        a->ent = Entity::Jerry;
        a->initBody(perch, sp.yaw, (unsigned)rng_.randint(1, 1 << 30));
        JerryAg& J = jag(*a);
        J.perch = perch, J.home = sp.yaw, J.gaze = J.want = dirYaw(sp.yaw);
        a->state = anim::jy::S_REST;
        a->crouch = 0.5f;
        log::info("Praesenz: Jerry sitzt auf seinem Rohr bei ({:.1f}, {:.1f})", perch.x, perch.y);
        agents_.push_back(std::move(a));
        return true;
    }
    return false;
}

void Presence::regieJerry(double dt, const Context& c, std::vector<Event>& out) {
    const bool on = c.kin && c.kin[world::KIN_JERRY].on;
    // weit weg und lange ungesehen: fort (er ist dann einfach nicht da - Avatar-Hypothese, Wiki)
    std::erase_if(agents_, [&](const auto& a) {
        if (a->role != Role::Jerry || a->showcase || a->puppet || a->state == anim::jy::S_SUBDUE) return false;
        const int cost = fld().at(ifloor(a->pos.x), ifloor(a->pos.y));
        return (cost < 0 || cost > 1100) && a->unseenT > 15.0;
    });
    if (!on) return;
    jerryTry_ -= dt;
    if (jerryTry_ > 0.0 || has(Role::Jerry)) return;
    jerryTry_ = 3.0;
    spawnJerry(c, out, false);
}

// --- Denken: Blick, Stimme, Gefieder, Sprung -------------------------------------------------------------------------
void Presence::thinkJerry(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    using namespace anim::jy;
    JerryAg& J = jag(a);
    const double q = dt * jerryQuick_;
    a.stateT += q;
    const vec3 head = a.cre.headPos();
    const vec3 eye = c.view.eye;
    const float d = length(eye - head);
    // sieht er den Spieler (das Ziel dieses Takts)? freie Sicht, nah genug; im Dunkeln ohne Lampe nur, wenn er sich bewegt
    const bool clear = d < kSee && clearSight(*c.world, head, eye);
    const bool lampOn = c.view.flashOn;
    const vec3 toEye = nrmF(eye - head, dirYaw(a.yaw));
    const bool inCone = dot(J.gaze, toEye) > -0.2f;  // weites Blickfeld (Seitenaugen), nur nicht genau hinter ihm
    const bool litP = !darkTile(c, c.world->tileIfLoaded(ifloor(eye.x), ifloor(eye.y)), eye.x, eye.y);
    const bool sees = clear && (inCone || c.noise > 0.4) && (lampOn || litP || c.noise > 0.1 || d < 8.0f);
    lightReact(a, c, head, 14.0f, dt, 99.0);
    if (a.state == S_SUBDUE) {  // Unterwerfung oder Sprung ins Gesicht: Kamera haelt sein Gesicht (Sitzung)
        a.look = true;
        a.lookAt = J.killEye;
        a.fly = 1.0f, a.growl = 1.0f, a.perceive = 1.0f, a.alert = 1.0f, a.crouch = 0.0f;
        a.talk = J.flying ? 0.0f : 1.0f;
        a.jaw = J.flying ? 1.0f : 0.12f;
        a.reach = J.flying ? 1.0f : 0.0f;
        a.hang = J.flying ? 1.0f : 0.0f;
        a.wave = J.flying ? 1.0f : 0.25f;
        if (J.flying) {  // in 0,25 s vom Rohr bis knapp vor das Auge
            const float u = (float)std::min(1.0, a.stateT / 0.25);
            const vec3 tgt = J.killEye + J.killDir * 0.42f - vec3{0, 0, 0.42f};
            const vec3 p = J.flyFrom + (tgt - J.flyFrom) * (u * u * (3.0f - 2.0f * u));
            const float yaw = std::atan2(-J.killDir.y, -J.killDir.x);
            a.snapBody(p + vec3{(float)rng_.uniform(-1.0, 1.0), (float)rng_.uniform(-1.0, 1.0), 0.0f} * 0.006f, yaw);
        }
        if (a.stateT > 2.8) {
            a.state = S_TURN, a.stateT = 0.0;
            J.subdued = -1;
            if (J.flying) J.flying = false, J.backT = 0.0, J.flyFrom = a.pos;
        }
        return;
    }
    if (J.backT >= 0.0) {  // zurueck aufs Rohr (1 s, flatternd)
        J.backT += q;
        const float u = (float)std::min(1.0, J.backT / 1.0);
        a.pos = J.flyFrom + (J.perch - J.flyFrom) * (u * u * (3.0f - 2.0f * u)) + vec3{0, 0, 0.25f * std::sin(u * 3.14159f)};
        a.wave = 1.0f - u, a.hang = 1.0f - u, a.fly = 1.0f - 0.6f * u;
        if (u >= 1.0f) J.backT = -1.0, a.pos = J.perch, a.hang = 0.0f, a.wave = 0.0f;
        return;
    }
    a.pos = J.perch;
    // --- wohin er sieht ---------------------------------------------------------------------------------------------
    if (sees) {
        if (J.fixOn != a.target) J.noticeT = 0.0;
        J.fixOn = a.target, J.fixEye = eye, J.lostT = 0.0;
        J.noticeT += q;
    } else {
        J.lostT += q;
        if (J.lostT > 3.5) J.fixOn = -1;
    }
    if (a.light > 0.0f && clear) J.want = nrmF(c.view.flashPos - head, J.want);  // die Lampe zieht seinen Blick
    else if (J.fixOn >= 0 && J.lostT < 3.5) J.want = nrmF(J.fixEye - head, J.want);
    else {  // umsehen: mal zur Tuer, mal in den Raum, mal zur Seite
        J.lookT -= q;
        if (J.lookT <= 0.0) {
            J.lookT = rng_.uniform(1.2, 3.5);
            const float yw = J.home + (float)rng_.uniform(-1.4, 1.4);
            J.want = nrmF(vec3{std::cos(yw), std::sin(yw), (float)rng_.uniform(-0.35, 0.15)}, dirYaw(J.home));
        }
    }
    // Papageienkopf: schnell herumgerissen, wenn er etwas bemerkt (9 rad/s), sonst ruhiger
    J.gaze = turnToward(J.gaze, J.want, (float)q * (J.fixOn >= 0 && J.noticeT < 0.6 ? 9.0f : 3.0f));
    a.look = true;
    a.lookAt = head + J.gaze * 2.0f;
    // Koerper: dreht sich langsam mit, wenn der Kopf weit herum muss
    const float gy = std::atan2(J.gaze.y, J.gaze.x);
    const float rel = wrapA(gy - a.yaw);
    if (std::fabs(rel) > 1.3f && J.fixOn >= 0) a.yaw = wrapA(a.yaw + std::clamp(rel, -1.0f, 1.0f) * (float)q * 0.9f);
    else if (J.fixOn < 0) a.yaw = wrapA(a.yaw + std::clamp(wrapA(J.home - a.yaw), -1.0f, 1.0f) * (float)q * 0.4f);
    // --- Zustand ----------------------------------------------------------------------------------------------------
    const float sp = J.fixOn >= 0 && J.fixOn < 4 ? spell_[(size_t)J.fixOn] : 0.0f;
    switch (a.state) {
        case S_REST:
        case S_TURN:
            if (J.fixOn >= 0 && J.noticeT > 0.0) {
                a.state = S_NOTICE, a.stateT = 0.0;
                out.push_back({Event::Flap, Role::Jerry, head, 0.3f});  // ruckt hoch, schuettelt das Gefieder
            }
            break;
        case S_NOTICE:
            if (J.fixOn < 0) a.state = S_TURN, a.stateT = 0.0;
            else if (a.stateT > 0.6) a.state = S_FIX, a.stateT = 0.0, J.talkT = 0.2;
            break;
        case S_FIX:
        case S_WARN:
            if (J.fixOn < 0 && a.stateT > 1.0) {
                a.state = S_TURN, a.stateT = 0.0;
                out.push_back({Event::Preen, Role::Jerry, head, 1.0f});
            }
            if (a.state == S_WARN && sp < 0.7f && a.stateT > 1.2) a.state = S_FIX, a.stateT = 0.0;
            break;
        default: break;
    }
    // --- Sprung: wer ihm zu nahe kommt (wie zum Beruehren), dem springt er ins Gesicht ---------------------------------
    const float hd = hdist(head, eye);
    if (J.lungeOn < 0 && a.state != S_LUNGE && hd < kLunge && std::fabs(eye.z - head.z) < 1.0f && clear) {
        J.lungeOn = a.target, J.lungeT = 0.0;
        a.state = S_LUNGE, a.stateT = 0.0;
        out.push_back({Event::Screech, Role::Jerry, head, 0.5f});
        out.push_back({Event::Flap, Role::Jerry, head, 1.0f});
        log::info("Praesenz: Jerry - Sprung angekuendigt ({:.1f} m)", hd);
    }
    if (a.state == S_LUNGE) {
        J.lungeT += q;
        if (J.lungeT > 0.55) {
            if (hd < kLunge + 0.4f && clear && c.mode >= 2) {  // zu nah geblieben: Zugriff
                out.push_back({Event::Screech, Role::Jerry, head, 1.0f});
                out.push_back({Event::Attack, Role::Jerry, head, 1.0f});
                a.state = S_SUBDUE, a.stateT = 0.0;
                J.flying = true, J.flyFrom = a.pos, J.killEye = eye, J.killDir = nrmF(flat(head - eye), dirYaw(a.yaw + kPiF));
                J.subdued = a.target;
                log::info("Praesenz: Jerry - Sprung ins Gesicht");
            } else {
                a.state = S_FIX, a.stateT = 0.0;
            }
            J.lungeOn = -1;
        }
    }
    // --- Kanaele der Animation ----------------------------------------------------------------------------------------
    const bool fix = a.state == S_FIX || a.state == S_WARN || a.state == S_LUNGE;
    a.alert = a.state == S_REST ? 0.0f : (a.state == S_TURN ? 0.2f : 1.0f);
    a.crouch = a.state == S_REST || a.state == S_TURN ? 0.6f : 0.0f;
    a.growl = fix ? 0.45f + 0.55f * sp : 0.0f;
    a.perceive = std::max(a.light, fix ? 0.5f + 0.5f * sp : (a.state == S_NOTICE ? 0.8f : 0.0f));
    a.lean = fix ? 0.25f + 0.5f * sp : 0.0f;
    a.fly = a.state == S_LUNGE ? 1.0f : (a.state == S_WARN ? 0.85f : (sp > 0.6f ? 0.35f : 0.0f));
    a.reach = a.state == S_LUNGE ? 0.35f : 0.0f;
    a.jaw = a.state == S_LUNGE ? 0.9f : (a.state == S_WARN && a.stateT < 0.8 ? 0.8f : 0.0f);
    a.tilt = fix ? 0.25f * std::sin((float)time_ * 0.9f) : 0.0f;
    // Putzen und Flattern in Ruhe
    if (a.state == S_REST || a.state == S_TURN) {
        J.preenT -= q;
        if (J.preenT <= 0.0 && J.preenOn <= 0.0) J.preenOn = rng_.uniform(3.0, 6.0), J.preenT = rng_.uniform(9.0, 18.0);
        if (J.preenOn > 0.0) {
            J.preenOn -= q;
            a.shiver = 1.0f;
            if (std::fmod(J.preenOn, 1.3) < q) out.push_back({Event::Preen, Role::Jerry, head, 1.0f});
        } else {
            a.shiver = 0.0f;
        }
        J.flapT -= q;
        if (J.flapT <= 0.0) J.flapT = rng_.uniform(25.0, 40.0), J.flapOn = 0.9, out.push_back({Event::Flap, Role::Jerry, head, 0.3f});
        J.grindT -= q;
        if (J.grindT <= 0.0) J.grindT = rng_.uniform(6.0, 14.0), out.push_back({Event::Preen, Role::Jerry, head, 0.0f});
    } else {
        a.shiver = 0.0f, J.preenOn = 0.0;
    }
    if (J.flapOn > 0.0) {
        J.flapOn -= q;
        a.fly = std::max(a.fly, 0.6f), a.wave = 0.6f;
    } else if (a.state != S_LUNGE) {
        a.wave = 0.0f;
    }
    // Stimme: murmelt vor sich hin (Geraeusch vor Sicht), spricht den fixierten Spieler an; der Schnabel spricht mit
    // (Satz etwa 1,6 s, Synthese synth_jerry.cpp)
    J.talkOn -= q;
    if (fix) {
        J.talkT -= q;
        if (J.talkT <= 0.0) {
            J.talkT = rng_.uniform(2.2, 3.6);
            J.talkOn = 1.7;
            out.push_back({Event::Speak, Role::Jerry, head, 1.0f});
        }
    } else if (a.state == S_REST || a.state == S_TURN) {
        J.mutterT -= q;
        if (J.mutterT <= 0.0) {
            J.mutterT = rng_.uniform(6.0, 12.0);
            const int cost = fld().at(ifloor(a.pos.x), ifloor(a.pos.y));
            if (cost >= 0 && cost < 260) J.talkOn = 1.5, out.push_back({Event::Speak, Role::Jerry, head, 0.35f});
        }
    }
    a.talk = J.talkOn > 0.0 ? (fix ? 1.0f : 0.6f) : 0.0f;
    a.glow = a.target >= 0 && a.target < 4 ? spell_[(size_t)a.target] : 0.0f;  // Bann des Ziels - fuer die Puppe beim Gast
}

// --- Blick und Bann aller Spieler (Host/Einzelspieler; nach den Wesen eines Takts) --------------------------------------
void Presence::jerryGaze(double dt, const Context& c, std::vector<Event>& out) {
    using namespace anim::jy;
    if (c.puppet || !c.world) return;
    Agent* jp = nullptr;
    for (auto& a : agents_)
        if (a->role == Role::Jerry && !a->showcase && !a->puppet) jp = a.get();
    struct Pl {
        int id;
        const Viewer* v;
    };
    std::vector<Pl> pls;
    if (c.selfAlive) pls.push_back({c.selfId, &c.view});
    if (c.others)
        for (const Other& o : *c.others) pls.push_back({o.id, &o.view});
    std::array<bool, 4> live{};
    for (const Pl& p : pls)
        if (p.id >= 0 && p.id < 4) live[(size_t)p.id] = true;
    for (size_t i = 0; i < 4; ++i)
        if (!live[i]) spell_[i] = 0.0f, spellStage_[i] = 0;  // tot oder nicht da: kein Bann (nach dem Wiedereinstieg frei)
    if (!jp) {
        for (size_t i = 0; i < 4; ++i) spell_[i] = std::max(0.0f, spell_[i] - (float)dt * 0.6f), spellStage_[i] = spell_[i] < 0.2f ? 0 : spellStage_[i];
        return;
    }
    Agent& a = *jp;
    JerryAg& J = jag(a);
    const vec3 head = a.cre.headPos();
    const float q = (float)(dt * jerryQuick_);
    for (const Pl& p : pls) {
        if (p.id < 0 || p.id >= 4) continue;
        float& s = spell_[(size_t)p.id];
        int& stage = spellStage_[(size_t)p.id];
        const vec3 toP = p.v->eye - head;
        const float d = length(toP);
        const vec3 dn = d > 1e-4f ? toP / d : vec3{1, 0, 0};
        const bool clear = d < kSee && clearSight(*c.world, head, p.v->eye);
        const bool hisLook = dot(J.gaze, dn) > kContactJ && (a.state == S_FIX || a.state == S_WARN || a.state == S_LUNGE || a.state == S_NOTICE);
        // der Spieler sieht ihm ins Gesicht: Blickrichtung, freie Sicht, und Jerry ist ueberhaupt zu sehen (Licht, Lampe)
        const bool hisFace = dot(p.v->look, -dn) > kContactP && perceive(*p.v, *c.world, a.feet(), a.height()).level > 0.2f;
        const bool contact = clear && hisLook && hisFace && a.state != S_SUBDUE && J.backT < 0.0;
        if (contact) {
            const float near = std::clamp((7.0f - d) / 5.0f, 0.0f, 1.0f);
            s = std::min(1.0f, s + q * kRate * (1.0f + near));
        } else {
            s = std::max(0.0f, s - q * (clear && hisLook ? 0.3f : 0.55f));
        }
        if (c.mode < 2) s = std::min(s, 0.84f);  // ohne Angriffe: nur bis zur Warnung
        // Stufen (je Aufbau einmal): Klingeln, Blickzug, Warnung, Unterwerfung
        if (s >= 0.3f && stage < 1) stage = 1, out.push_back({Event::Spell, Role::Jerry, head, 0.3f, p.id});
        if (s >= 0.6f && stage < 2) stage = 2, out.push_back({Event::Spell, Role::Jerry, head, 0.6f, p.id});
        if (s >= 0.85f && stage < 3) {
            stage = 3;
            out.push_back({Event::Announce, Role::Jerry, head, 1.0f, p.id});
            out.push_back({Event::Flap, Role::Jerry, head, 1.0f, p.id});
            if (a.state != S_LUNGE) a.state = S_WARN, a.stateT = 0.0;
            log::info("Praesenz: Jerry - Warnung (Bann {:.2f}, Spieler {})", s, p.id);
        }
        if (s >= 1.0f && stage < 4 && c.mode >= 2) {
            stage = 4;
            out.push_back({Event::Attack, Role::Jerry, head, 1.0f, p.id});
            a.state = S_SUBDUE, a.stateT = 0.0;
            J.killEye = p.v->eye, J.flying = false, J.subdued = p.id;
            log::info("Praesenz: Jerry - Spieler {} unterworfen", p.id);
        }
        if (s < 0.15f) stage = 0;
        else if (s < 0.45f && stage > 1) stage = 1;
    }
}

float Presence::jerrySpell(int target, vec3* at) const {
    for (const auto& a : agents_) {
        if (a->role != Role::Jerry || a->showcase) continue;
        if (at) *at = a->cre.headPos();
        if (a->puppet) return a->target == target ? std::clamp(a->glow, 0.0f, 1.0f) : 0.0f;  // Gast: aus der Puppe
        break;
    }
    return target >= 0 && target < 4 ? spell_[(size_t)target] : 0.0f;
}

float Presence::jerryHold(int target) const {
    const float s = jerrySpell(target);
    const float u = std::clamp((s - 0.35f) / 0.65f, 0.0f, 1.0f);
    return u * u * (3.0f - 2.0f * u);
}

Presence::JerryInfo Presence::jerryInfo() const {
    JerryInfo r;
    for (const auto& a : agents_) {
        if (a->role != Role::Jerry || a->showcase) continue;
        r.active = true, r.state = a->state, r.pos = a->pos, r.head = a->cre.headPos(), r.yaw = a->yaw;
        if (const JerryAg* J = jagc(*a)) r.gaze = J->gaze, r.fixOn = J->fixOn;
        break;
    }
    return r;
}

bool Presence::jerrySet(const std::string& key, double v) {
    if (key == "quick") return jerryQuick_ = std::clamp(v, 0.1, 20.0), true;
    if (key == "spell") return spell_[0] = (float)std::clamp(v, 0.0, 1.0), true;
    for (auto& a : agents_) {
        if (a->role != Role::Jerry || a->showcase || a->puppet) continue;
        if (key == "state") return a->state = (int)v, a->stateT = 0.0, true;
        if (key == "yaw") return a->yaw = (float)v, jag(*a).home = (float)v, jag(*a).gaze = jag(*a).want = dirYaw((float)v), true;
    }
    return false;
}

}  // namespace lim::game
