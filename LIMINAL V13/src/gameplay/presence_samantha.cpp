// V13.3 Teil 4 Level 5: Samantha (KREATUR_Samantha.md; Wikidot Entity 26 Rev. 22, Level 5 Rev. 184 - CC BY-SA 3.0).
//
// Wiki: eine kleine schwarz-weisse Hauskatze mit gruenen Augen, die niemand fuer eine gewoehnliche Katze haelt; kraeftiger
// Kiefer mit gezackten Zaehnen, die Haut und Fleisch muehelos zerreissen. Sie spricht mehrere Sprachen, liest Gedanken, sieht
// hell und teleportiert sich. Gegen Fleisch gibt sie eine Lesung, deren Guete von der Guete des Fleischs abhaengt (Steak,
// guter Fisch, Haehnchen: ausfuehrlich; Dosenfleisch, Sardinen: aergerlich und vage). Wer ihr nichts gibt - auch wer nichts
// hat -, gegen den wird sie aeusserst gewalttaetig (kratzen, beissen; schwere Verletzungen, bisher keine Toten). Hoeflich
// bleiben, nicht reizen. Level 5: wandert manchmal durch die Haupthalle; wer sich naehert, bringe gutes Fleisch mit.
//
// Umsetzung (Auslegung im Briefing):
//   Auftreten   selten (Level-JSON presence.samantha: every, chance, count), nur in den Gaengen der Haupthalle, ungesehen
//               10-25 m Weg entfernt ("teleportiert"). Man hoert sie vor Sicht: Murmeln in fremden Sprachen, Schnurren.
//   Ansprechen  sieht sie einen Spieler (9 m, freie Sicht), setzt sie sich ihm zugewandt und spricht ihn an (Untertitel; sie
//               liest Gedanken - sie spricht seine Sprache). Sie fordert Fleisch.
//   Tausch      wer Fleisch dabei hat und bis 1,5 m herankommt, dem nimmt sie das beste Stueck (Steal; die Sitzung legt es ihr
//               vor die Pfoten), frisst es (reisst mit den Saegezaehnen) und gibt dann die Lesung (Gift; Guete im Staerkewert -
//               der Text entsteht in der Sitzung aus dem, was die Welt weiss). Nimmt man es ihr waehrend des Fressens weg: Reizung.
//   Ohne        wer ohne Fleisch naeher als 2,2 m kommt oder 5 s in 4 m bleibt, wird gewarnt (Buckel, Fauchen, "Mit leeren
//               Haenden?" - 1,6 s), dann springt sie ins Gesicht: 2 s Kratzen und Beissen (Hit, je 7; nie toedlich - sie haelt
//               ab 15 Gesundheit inne). Zurueckweichen ueber 4,5 m in der Warnung rettet.
//   Reizen     Lampe ins Gesicht (2 s), Rennen auf sie zu: erst eine Mahnung, beim zweiten Mal die Warnung.
//   Rueckzug    danach geht sie fort und ist fort, sobald niemand hinsieht; laengere Pause bis zum naechsten Auftritt.
// Mehrspieler: der Host rechnet alles; Fleisch und Lesung betreffen den Spieler, der ihr am naechsten ist (Ziel). Saetze und
// Laute als Klangnummern (Mimic) - die Gaeste hoeren dieselben Saetze mit Untertitel (kinNetRoute).
#include "gameplay/hotel_l5.hpp"
#include "gameplay/kin_l3.hpp"
#include "gameplay/presence.hpp"
#include "gameplay/presence_agent.hpp"

#include "core/log.hpp"

namespace lim::game {

namespace {
using namespace anim::sa;
constexpr float kNotice = 9.0f;     // so weit bemerkt sie einen (freie Sicht)
constexpr float kTrade = 1.5f;      // so nah: sie nimmt das Fleisch
constexpr float kTooClose = 2.2f;   // ohne Fleisch so nah: Warnung
constexpr float kEscape = 4.5f;     // in der Warnung so weit weg: sie laesst ab
constexpr double kWarnTime = 1.6, kLeapTime = 0.32, kLatchTime = 2.0, kEatTime = 3.6;
struct SamAg {
    double talkT = 2.0, murmurT = 3.0, purrT = 6.0, waitT = 0.0, rudeT = 0.0, sitT = 0.0, nextHit = 0.0;
    int asks = 0, rude = 0, line = 0;
    std::string meat;          // das genommene Stueck
    float quality = 0.0f;
    vec3 meatAt, leapFrom, latchDir{1, 0, 0}, latchEye;
    bool gaveRead = false, attacked = false, provoked = false;
};
struct SamStats {
    int trades = 0, attacks = 0, hits = 0, warnings = 0;
    float quality = -1.0f;
    bool hold = false;
    int pose = -1;
};
template <class A>
SamAg& sag(A& a) {
    if (!a.ext) a.ext = std::make_shared<SamAg>();
    return *static_cast<SamAg*>(a.ext.get());
}
// das beste Fleisch im Inventar des Ziels (Wiki: Steak, guter Fisch, Haehnchen gut; Dosenfleisch, Sardinen minderwertig)
std::string bestMeat(const Presence::Context& c, float& q) {
    q = 0.0f;
    if (!c.loot) return {};
    std::string best;
    for (const std::string& k : *c.loot) {
        const float v = hotel::meatQuality(k);
        if (v > q) q = v, best = k;
    }
    return best;
}
}  // namespace

// --- Auftreten: ungesehen in einem Gang der Haupthalle -------------------------------------------------------------------
bool Presence::spawnSamantha(const Context& c, std::vector<Event>& out, bool forced) {
    if (!c.world || !c.player) return false;
    for (const auto& a : agents_)
        if (a->role == Role::Samantha && !a->showcase && !a->puppet) return false;  // ein Einzelwesen (Wiki: einzigartig)
    const auto allowed = [&](vec3 q) { return roomAllowed(c, q, world::KIN_SAMANTHA); };
    vec3 p;
    bool ok = hiddenSpawn(c, forced ? 60 : 100, 250, false, p, allowed) || hiddenSpawn(c, 60, 600, false, p, allowed);
    if (!ok && forced) ok = hiddenSpawn(c, 30, 900, true, p, allowed) || hiddenSpawn(c, 30, 900, true, p);  // Skripte, Tests: hinter dem Spieler
    if (!ok) return false;
    auto a = std::make_unique<Agent>();
    a->role = Role::Samantha;
    a->ent = Entity::Samantha;
    a->initBody(p, (float)rng_.uniform(-kPiF, kPiF), (unsigned)rng_.randint(1, 1 << 30));
    a->state = rng_.random() < 0.5 ? S_SIT : S_WALK;
    a->crouch = a->state == S_SIT ? 1.0f : 0.0f;
    a->timer2 = rng_.uniform(1.0, 4.0);
    sag(*a);
    out.push_back({Event::Appear, Role::Samantha, p});
    log::info("Praesenz: Samantha bei ({:.1f}, {:.1f}), {:.0f} m", p.x, p.y, hdist(p, c.view.eye));
    agents_.push_back(std::move(a));
    ++samVisits_;
    return true;
}

void Presence::regieSamantha(double dt, const Context& c, std::vector<Event>& out) {
    // fort (teleportiert): entfernen - ausserhalb der Wesen-Schleife (B-026)
    std::erase_if(agents_, [&](const auto& a) { return a->role == Role::Samantha && !a->showcase && !a->puppet && a->state == S_GONE; });
    if (!c.kin || !c.kin[world::KIN_SAMANTHA].on) return;
    const world::KinDef& K = c.kin[world::KIN_SAMANTHA];
    if (has(Role::Samantha)) return;
    if (samTry_ <= 0.0 && samVisits_ == 0) samTry_ = K.every * rng_.uniform(0.35, 0.8);  // erster Auftritt
    samTry_ -= dt * samQuick_;
    if (samTry_ > 0.0) return;
    samTry_ = K.every * rng_.uniform(0.7, 1.4);
    if (K.count > 0 && samVisits_ >= K.count) return;      // "manchmal": hoechstens so oft je Besuch der Ebene
    if (rng_.random() >= std::clamp(K.chance, 0.0, 1.0)) return;
    spawnSamantha(c, out, false);
}

// --- Denken --------------------------------------------------------------------------------------------------------------
void Presence::thinkSamantha(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    using hotel::SamCue;
    SamAg& S = sag(a);
    if (!samStats_) samStats_ = std::make_shared<SamStats>();
    SamStats& st = *static_cast<SamStats*>(samStats_.get());
    const double q = dt * samQuick_;
    a.stateT += q;
    const vec3 head = a.cre.headPos();
    const vec3 eye = c.view.eye;
    const vec3 feet{eye.x, eye.y, eye.z - (float)Player::kEye};
    const float hd = hdist(a.pos, eye);
    const bool clear = hdist(head, eye) < 20.0f && clearSight(*c.world, head, eye);
    const bool seen = a.sight.level > 0.25f;
    auto say = [&](int cue) { out.push_back({Event::Mimic, Role::Samantha, head, hotel::kSamCueBase + (float)cue}); };
    lightReact(a, c, head, 9.0f, dt, 99.0);
    auto faceTo = [&](vec3 p, float rate) {
        const float want = std::atan2(p.y - a.pos.y, p.x - a.pos.x);
        a.yaw = wrapA(a.yaw + std::clamp(wrapA(want - a.yaw), -rate * (float)q, rate * (float)q));
    };
    float meatQ = 0.0f;
    const std::string meat = bestMeat(c, meatQ);
    // Haltung fuer Bilder (Skript): fest in einer Pose, keine Entscheidungen
    if (st.hold) {
        a.look = true, a.lookAt = eye;
        faceTo(eye, 6.0f);
        a.crouch = st.pose == S_WALK || st.pose == S_WARN || st.pose == S_ATTACK ? 0.0f : 1.0f;
        a.talk = st.pose == S_ASK ? 1.0f : 0.0f;
        a.growl = st.pose == S_WARN ? 1.0f : 0.0f;
        a.shiver = st.pose == S_WARN ? 1.0f : 0.0f;
        a.jaw = st.pose == S_WARN || st.pose == S_ATTACK ? 1.0f : 0.0f;
        a.alert = st.pose >= S_ASK ? 1.0f : 0.3f;
        a.bait = st.pose == S_EAT ? 1.0f : 0.0f;
        a.perceiveAt = a.pos + dirYaw(a.yaw) * 0.18f;
        a.reach = st.pose == S_ATTACK ? 1.0f : 0.0f;
        a.hang = st.pose == S_ATTACK ? 1.0f : 0.0f;
        a.strike = st.pose == S_ATTACK ? (float)std::fmod(time_ * 0.8, 1.0) : 0.0f;
        a.tilt = st.pose == S_ASK ? 0.55f : 0.0f;
        a.wave = st.pose == S_WARN ? 1.0f : 0.0f;
        a.state = st.pose;
        return;
    }
    a.look = false, a.talk = 0.0f, a.jaw = 0.0f, a.growl = 0.0f, a.shiver = 0.0f, a.reach = 0.0f, a.hang = 0.0f, a.strike = 0.0f;
    a.bait = 0.0f, a.perceive = 0.0f, a.wave = 0.0f;
    a.tilt += (0.0f - a.tilt) * (float)std::min(1.0, q * 2.0);
    switch (a.state) {
        case S_SIT:
        case S_WALK: {
            a.alert = 0.2f;
            if (a.state == S_WALK) {
                a.crouch = 0.0f;
                if (!wander(a, q, c, 0.42, world::KIN_SAMANTHA) && a.timer2 <= 0.0) a.state = S_SIT, a.stateT = 0.0;
                if (a.state == S_WALK && a.stateT > rng_.uniform(9.0, 16.0) && !a.hasGoal) a.state = S_SIT, a.stateT = 0.0;
            } else {
                a.crouch = 1.0f;
                if (a.stateT > 12.0 + 6.0 * std::sin((double)a.netId)) a.state = S_WALK, a.stateT = 0.0, a.timer2 = 0.0;
            }
            // vor Sicht: Murmeln in fremden Sprachen, Schnurren (nah)
            S.murmurT -= q;
            const int cost = fld().at(ifloor(a.pos.x), ifloor(a.pos.y));
            if (S.murmurT <= 0.0 && cost >= 0 && cost < 190) {
                S.murmurT = rng_.uniform(7.0, 12.0);
                say(hotel::SAM_MURMUR0 + S.line++ % 3);
                a.talk = 0.6f;
            }
            S.purrT -= q;
            if (S.purrT <= 0.0 && a.state == S_SIT && hd < 7.0f) S.purrT = rng_.uniform(5.0, 9.0), say(hotel::SAM_PURR);
            if (clear && hd < kNotice && (seen || hd < 5.0f) && c.mode > 0) {
                a.state = S_ASK, a.stateT = 0.0;
                S.talkT = 0.5, S.waitT = 0.0;
                out.push_back({Event::FirstSight, Role::Samantha, head, 0.5f});
            }
            break;
        }
        case S_ASK: {
            a.crouch = hd < 3.0f ? 1.0f : (a.stateT > 1.0 ? 1.0f : 0.4f);
            a.alert = 1.0f;
            a.look = true, a.lookAt = eye;
            faceTo(eye, 5.0f);
            a.tilt = 0.45f * std::sin((float)a.stateT * 0.6f);  // legt den Kopf schief, weit
            if (S.gaveRead) {  // nach der Lesung: noch einen Blick, dann geht sie (Runde 3: sie fauchte danach den Leeren an)
                a.talk = a.stateT < 0.0 ? 1.0f : 0.0f;
                if (a.stateT > 2.5) a.state = S_LEAVE, a.stateT = 0.0, say(hotel::SAM_BYE);
                break;
            }
            // naeher kommen lassen; sie selbst geht ein paar Schritte auf den Spieler zu, wenn er stehen bleibt
            if (hd > 3.2f && a.stateT > 3.0 && clear) {
                a.crouch = 0.0f;
                walkTo(a, fld(), 0.5, q, c);
            }
            S.talkT -= q;
            if (S.talkT <= 0.0) {
                S.talkT = rng_.uniform(4.5, 6.5);
                say(S.asks == 0 ? hotel::SAM_ASK0 : (meat.empty() ? hotel::SAM_ASK2 : hotel::SAM_ASK1));
                ++S.asks;
                S.waitT = 0.0;
            }
            a.talk = S.talkT > (S.asks > 0 ? 2.2 : 99.0) ? 1.0f : 0.0f;  // spricht in den ersten gut 2 s eines Satzes
            if (!meat.empty() && hd < kTrade && clear) {  // Tausch: sie nimmt das beste Stueck
                S.meat = meat, S.quality = meatQ;
                S.meatAt = a.pos + dirYaw(a.yaw) * 0.17f;
                out.push_back({Event::Steal, Role::Samantha, S.meatAt, 1.0f, 0, meat});
                Event sp{Event::Spill, Role::Samantha, S.meatAt, 1.0f, 0, meat};
                out.push_back(sp);
                say(meatQ >= 0.7f ? hotel::SAM_THANK_GOOD : hotel::SAM_THANK_POOR);
                a.state = S_EAT, a.stateT = 0.0;
                st.quality = meatQ;
                log::info("Praesenz: Samantha nimmt {} (Guete {:.1f})", meat, meatQ);
                break;
            }
            // Reizen: Lampe ins Gesicht, Rennen auf sie zu
            if (a.light > 0.5f) S.rudeT += q;
            const bool rush = c.noise >= 0.95 && hd < 5.0f;
            if (S.rudeT > 2.0 || rush) {
                S.rudeT = 0.0;
                if (++S.rude == 1) say(hotel::SAM_RUDE);
                else S.provoked = true;
            }
            if (meat.empty() && hd < 4.0f) S.waitT += q;
            if (S.provoked || (meat.empty() && (hd < kTooClose || S.waitT > 5.0)) ) {
                a.state = S_WARN, a.stateT = 0.0;
                ++st.warnings;
                say(hotel::SAM_WARN);
                break;
            }
            if (hd > 13.0f || (!clear && a.stateT > 4.0 && a.unseenT > 6.0)) a.state = S_LEAVE, a.stateT = 0.0, say(hotel::SAM_BYE);
            break;
        }
        case S_EAT: {
            a.crouch = 1.0f, a.alert = 0.6f;
            a.bait = a.stateT > 0.3 ? 1.0f : 0.0f;
            a.perceive = 1.0f, a.perceiveAt = S.meatAt;
            if (std::fmod(a.stateT, 1.2) < q && a.stateT > 0.4) say(hotel::SAM_EAT);
            // nimmt man es ihr weg: Reizung
            bool there = !c.loose;
            if (c.loose)
                for (const Loose& l : *c.loose)
                    if (l.kind == S.meat && hdist(l.pos, S.meatAt) < 0.6f) there = true;
            if (!there && a.stateT > 0.5 && a.stateT < kEatTime - 0.3) {
                S.provoked = true;
                a.state = S_WARN, a.stateT = 0.0;
                ++st.warnings;
                say(hotel::SAM_WARN);
                break;
            }
            if (a.stateT > kEatTime) {
                out.push_back({Event::Take, Role::Samantha, S.meatAt, 1.0f, 0, S.meat});
                out.push_back({Event::Gift, Role::Samantha, head, S.quality, 0, S.meat});  // die Lesung (Text: Sitzung)
                say(S.quality >= 0.7f ? hotel::SAM_READ_GOOD : hotel::SAM_READ_POOR);
                ++st.trades;
                S.gaveRead = true;
                a.state = S_ASK, a.stateT = -2.5;  // sie spricht (Lesung), dann geht sie
                S.talkT = 99.0;
            }
            break;
        }
        case S_WARN: {
            a.crouch = 0.0f, a.alert = 1.0f, a.growl = 1.0f, a.shiver = 1.0f, a.jaw = a.stateT < 1.3 ? 0.85f : 1.0f, a.wave = 1.0f;
            a.look = true, a.lookAt = eye;
            faceTo(eye, 8.0f);
            if (a.stateT > kWarnTime) {
                if (hd < kEscape && clear && c.mode >= 2 && c.view.eye.z - a.pos.z < 2.4f) {
                    a.state = S_ATTACK, a.stateT = 0.0;
                    S.leapFrom = a.pos;
                    S.latchDir = nrmF(flat(a.pos - eye), dirYaw(a.yaw + kPiF));
                    S.latchEye = eye;
                    S.nextHit = kLeapTime + 0.25;
                    S.attacked = true;
                    ++st.attacks;
                    say(hotel::SAM_SCREAM);
                    log::info("Praesenz: Samantha springt ({:.1f} m)", hd);
                } else {
                    a.state = S.gaveRead || S.provoked ? S_LEAVE : S_ASK, a.stateT = 0.0, S.waitT = 0.0, S.provoked = false;
                    if (a.state == S_LEAVE) say(hotel::SAM_BYE);
                }
            }
            break;
        }
        case S_ATTACK: {
            a.crouch = 0.0f, a.alert = 1.0f, a.jaw = 1.0f, a.shiver = 0.6f, a.growl = 0.4f;
            a.look = true, a.lookAt = eye;
            S.latchEye = eye;  // folgt dem Gesicht
            S.latchDir = nrmF(flat(a.pos - eye) * 0.3f + S.latchDir * 0.7f, S.latchDir);
            const vec3 latch = eye + S.latchDir * 0.36f - vec3{0, 0, 0.46f};  // Runde 3: zu hoch - das Gesicht vor dem Auge
            a.yaw = std::atan2(-S.latchDir.y, -S.latchDir.x);
            if (a.stateT < kLeapTime) {  // Sprung im Bogen
                const float u = (float)(a.stateT / kLeapTime), e = u * u * (3.0f - 2.0f * u);
                a.pos = S.leapFrom + (latch - S.leapFrom) * e + vec3{0, 0, 0.25f * std::sin(u * kPiF)};
                a.reach = 1.0f, a.hang = 0.4f * u;
            } else if (a.stateT < kLeapTime + kLatchTime) {  // am Gesicht: Kratzen und Beissen
                a.pos = latch + vec3{(float)rng_.uniform(-1.0, 1.0), (float)rng_.uniform(-1.0, 1.0), (float)rng_.uniform(-1.0, 1.0)} * 0.008f;
                a.reach = 1.0f, a.hang = 1.0f;
                a.strike = (float)std::fmod((a.stateT - kLeapTime) * 1.6, 1.0);
                if (a.stateT >= S.nextHit) {
                    S.nextHit += 0.45;
                    // nie toedlich (Wiki: bisher keine Toten): ab 15 Gesundheit nur noch Kratzen ohne Schaden
                    const double dmg = std::min(7.0, std::max(0.0, (c.health - 15.0) / 2.5));
                    if (dmg > 0.5) {
                        out.push_back({Event::Hit, Role::Samantha, head, (float)dmg});
                        ++st.hits;
                    }
                    say(hotel::SAM_BITE);
                }
            } else {  // laesst los, faellt herab, rennt davon
                const vec3 land = eye + S.latchDir * 0.9f;
                const world::Tile* t = c.world->tileIfLoaded(ifloor(land.x), ifloor(land.y));
                a.pos = t && t->open() ? vec3{land.x, land.y, t->floor} : S.leapFrom;
                a.state = S_LEAVE, a.stateT = 0.0;
                a.snapBody(a.pos, a.yaw);
            }
            break;
        }
        case S_LEAVE: {
            a.crouch = 0.0f, a.alert = 0.5f;
            if (!a.hasGoal || a.stateT < q * 1.5) pickGoal(c, a, 80, 220, true);
            if (a.hasGoal) walkTo(a, a.field, S.attacked ? 2.6 : 0.8, q, c);
            // fort, sobald niemand hinsieht (Teleportation, Wiki) - oder nach einer Weile
            if ((a.unseenT > 0.8 && a.stateT > 0.6) || a.stateT > 14.0) {
                a.state = S_GONE, a.stateT = 0.0;
                out.push_back({Event::Gone, Role::Samantha, a.pos});
                samTry_ = std::max(samTry_, (c.kin ? c.kin[world::KIN_SAMANTHA].every : 300.0) * 1.3);
            }
            break;
        }
        default: break;
    }
}

Presence::SamInfo Presence::samInfo() const {
    SamInfo r;
    if (samStats_) {
        const SamStats& st = *static_cast<const SamStats*>(samStats_.get());
        r.trades = st.trades, r.attacks = st.attacks, r.hits = st.hits, r.warnings = st.warnings, r.quality = st.quality;
    }
    for (const auto& a : agents_) {
        if (a->role != Role::Samantha || a->showcase) continue;
        r.active = a->state != S_GONE, r.state = a->state, r.pos = a->pos, r.head = a->cre.headPos(), r.target = a->target;
        break;
    }
    return r;
}

bool Presence::samSet(const std::string& key, double v) {
    if (!samStats_) samStats_ = std::make_shared<SamStats>();
    SamStats& st = *static_cast<SamStats*>(samStats_.get());
    if (key == "quick") return samQuick_ = std::clamp(v, 0.1, 20.0), true;
    if (key == "hold") return st.hold = v > 0.5, true;
    if (key == "pose") return st.pose = (int)v, st.hold = v >= 0.0, true;
    if (key == "next") return samTry_ = v, true;
    for (auto& a : agents_) {
        if (a->role != Role::Samantha || a->showcase || a->puppet) continue;
        if (key == "state") return a->state = (int)v, a->stateT = 0.0, true;
        if (key == "yaw") return a->yaw = (float)v, true;
    }
    return false;
}

}  // namespace lim::game
