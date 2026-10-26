// V13.2: die neuen Wesen aus dem Abgleich mit dem Wiki (KREATUR_<Name>.md): Faceling, Skin-Stealer, Wretch, Death Rat,
// Fensterwesen - Auftreten (Level-JSON presence.<wesen>), Umhergehen und Verhalten. V13.3: aus presence.cpp ausgelagert.
#include "gameplay/presence.hpp"
#include "gameplay/presence_agent.hpp"

#include "core/log.hpp"

namespace lim::game {

namespace {
int kinOf(Presence::Role r) { return (int)r - (int)Presence::Role::Faceling; }
// Faceling: 0 friedlich (Level 903), 1 reizbar (Level 2), 2 feindselig mit Steinwerkzeug (Level 3), 3 toedlich (Level 188)
int facelingMode(const std::string& m) { return m == "irritable" ? 1 : m == "armed" ? 2 : m == "lethal" ? 3 : 0; }
}  // namespace

bool Presence::roomAllowed(const Context& c, vec3 p, int kin) const {
    if (!c.kin) return true;
    const auto& rooms = c.kin[kin].rooms;
    if (rooms.empty()) return true;
    const world::Tile* t = c.world->tileIfLoaded(ifloor(p.x), ifloor(p.y));
    if (!t || !t->room || !t->room->type) return false;
    for (const std::string& k : rooms)
        if (k == t->room->type->key) return true;
    return false;
}

// umhergehen: zum Ziel (4..24 m um den Spieler, in erlaubten Raeumen), dort eine Weile stehen; true: geht gerade
bool Presence::wander(Agent& a, double dt, const Context& c, double speed, int kin) {
    if (a.timer2 > 0.0) {
        a.timer2 -= dt;
        return false;
    }
    if (a.hasGoal && hdist(a.pos, a.goal) > 0.8f && walkTo(a, a.field, speed, dt, c)) return true;
    a.hasGoal = false;
    a.timer2 = rng_.uniform(2.0, 7.0);
    const float clear = a.ent == Entity::Rat ? 0.4f : 1.9f;
    std::vector<std::pair<i64, i64>> cand;
    fld().ring(40, 240, cand);
    for (int tries = 0; tries < 40 && !cand.empty(); ++tries) {
        auto [x, y] = cand[(size_t)rng_.randint(0, (i64)cand.size() - 1)];
        const world::Tile* t = c.world->tileIfLoaded(x, y);
        if (!t || !figureWalkable(t, clear)) continue;
        const vec3 g{(float)x + 0.5f, (float)y + 0.5f, t->floor};
        const float dd = hdist(g, a.pos);
        if (dd > (a.ent == Entity::Rat ? 7.0f : 18.0f) || dd < 2.0f || !roomAllowed(c, g, kin)) continue;
        a.field.build(*c.world, x, y, clear);
        if (a.field.at(ifloor(a.pos.x), ifloor(a.pos.y)) < 0) continue;
        a.goal = g;
        a.hasGoal = true;
        break;
    }
    return false;
}

bool Presence::spawnKin(Role r, const Context& c, std::vector<Event>& out) {
    if (!c.kin) return false;
    const int k = kinOf(r);
    const world::KinDef& K = c.kin[k];
    const bool rat = r == Role::Rat;
    // ausser Sicht, 14..30 m Weg (Ratten 8..20 m); erlaubter Raum; Wretch bevorzugt das Dunkel (die Seite: im Schatten)
    vec3 p;
    bool ok = false;
    const auto allowed = [&](vec3 q) { return roomAllowed(c, q, k); };
    for (int tries = 0; tries < 8 && !ok; ++tries) {
        // erst nah (14..30 m Weg), sonst weiter (Raeume der Art koennen weiter weg liegen - die Gaenge in Level 188)
        if (!hiddenSpawn(c, rat ? 80 : 140, rat ? 200 : 300, false, p, allowed) && !hiddenSpawn(c, rat ? 80 : 140, 900, false, p, allowed)) return false;
        if (r == Role::Wretch && tries < 5 && !darkTile(c, c.world->tileIfLoaded(ifloor(p.x), ifloor(p.y)), p.x, p.y)) continue;
        ok = true;
    }
    if (!ok) return false;
    const int n = (int)rng_.randint(K.groupMin, K.groupMax);
    const int group = nextGroup_++;
    int made = 0;
    for (int i = 0; i < n; ++i) {
        vec3 q = p;
        if (i > 0) {  // die anderen in der Naehe (freie Kachel bis 2 m)
            bool found = false;
            for (int t = 0; t < 12 && !found; ++t) {
                const vec3 c2 = p + vec3{(float)rng_.uniform(-2.0, 2.0), (float)rng_.uniform(-2.0, 2.0), 0.0f};
                const world::Tile* tl = c.world->tileIfLoaded(ifloor(c2.x), ifloor(c2.y));
                if (figureWalkable(tl, rat ? 0.4f : 1.9f) && std::fabs(tl->floor - p.z) < 0.4f) q = {c2.x, c2.y, tl->floor}, found = true;
            }
            if (!found) continue;
        }
        auto a = std::make_unique<Agent>();
        a->role = r;
        a->ent = (Entity)r;
        a->group = group;
        a->glow = 1.0f;
        switch (r) {
            case Role::Faceling:
                a->mode = facelingMode(K.mode);
                // Varianten: 1 entwickelt, 2 rosa Kleid; sonst verschieden (0, 3 Frau, 4 aelterer Mann - Entity 9: Haar, Haut,
                // Kleidung und Koerperbau sind sehr verschieden)
                a->variant = a->mode == 2 ? 1 : (a->mode == 3 ? 2 : (int)std::array<int, 3>{0, 3, 4}[(size_t)rng_.randint(0, 2)]);
                break;
            case Role::SkinStealer:
                a->mode = K.mode == "skin" ? 1 : 0;
                a->variant = a->mode;
                a->hungry = rng_.uniform(0.0, 1.0) < K.chance;
                break;
            case Role::Wretch:
                a->mode = K.mode == "sprinter" ? 1 : K.mode == "mixed" ? (int)rng_.randint(0, 1) : 0;
                a->pleads = rng_.uniform(0.0, 1.0) < K.chance;
                break;
            case Role::Rat: a->variant = K.mode == "bald" ? 1 : 0; break;
            default: break;
        }
        a->initBody(q, (float)rng_.uniform(-kPiF, kPiF), (unsigned)rng_.randint(1, 1 << 30));
        a->rig.setGlow(1.0f);
        a->timer2 = rng_.uniform(0.5, 3.0);
        out.push_back({Event::Appear, r, q});
        agents_.push_back(std::move(a));
        ++made;
    }
    log::info("Praesenz: {} x{} bei ({:.1f}, {:.1f}), {:.0f} m", roleName(r), made, p.x, p.y, hdist(p, c.view.eye));
    return made > 0;
}

// --- Faceling (Entity 9) --------------------------------------------------------------------------------------------
// Menschen ohne Gesicht, schwaecher als ein Mensch. Je Ebene (KREATUR_Faceling.md):
//   0 friedlich   leben ihr Leben nach, gehen umher; wen sie wiedererkennen, dem winken sie
//   1 reizbar     in Gruppen; lassen einen in Ruhe - gereizt (Lampe lange im Gesicht, Bedraengen, Anrempeln) werden sie
//                 ausfallend: sie schlagen zu (leichter Schaden) und beruhigen sich nach einer Weile; die Gruppe macht mit
//   2 bewaffnet   feindselig, mit Steinwerkzeugen; kurzsichtig (kleine Augen), schwerhoerig (Ohrenschmalz): sie bemerken
//                 einen erst nah - dann Hiebe mit dem Stein
//   3 toedlich    im rosa Kleid, nur in den Gaengen: bleibt stehen und neigt den Kopf, dann kommt sie, zuletzt im Lauf
void Presence::thinkFaceling(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    if (a.state == 10) {
        killPose(a, dt, c);
        return;
    }
    const vec3 eye = c.view.eye;
    const float d = hdist(a.pos, eye);
    const vec3 head = a.cre.headPos();
    const bool los = clearSight(*c.world, head + vec3{0, 0, 0.05f}, eye);
    const vec3 toP = nrmF(flat(eye - a.pos), {1, 0, 0});
    const float facing = dot(toP, dirYaw(a.yaw));
    a.stateT += dt;
    a.cool -= dt;
    const vec3 chest = a.pos + vec3{0, 0, 1.25f};
    lightReact(a, c, head, 12.0f, dt, 1e9, &chest);  // solange das Licht auf ihm liegt
    const bool lit = a.light > 0.5f;
    const float range = a.mode == 2 ? (lit ? 8.0f : 5.0f) : (a.mode == 3 ? 18.0f : 14.0f);
    const bool sees = los && d < range && (facing > 0.2f || d < 3.0f);
    const bool hears = (c.player->sprinting && d < (a.mode == 2 ? 6.0f : 14.0f)) || (a.mode == 2 && d < 2.5f && c.player->gait > 0.3f);
    a.wave = 0.0f, a.reach = 0.0f, a.strike = 0.0f, a.alert = 0.0f, a.perceive = 0.0f, a.jaw = 0.0f;
    auto setState = [&](int s) { a.state = s, a.stateT = 0.0; };
    auto angry = [&](Agent& x) {
        if (x.state == 2) return;
        x.state = 2, x.stateT = 0.0;
        out.push_back({Event::Hum, Role::Faceling, x.cre.headPos(), 1.0f});
    };
    switch (a.state) {
        case 0: {  // geht umher, steht, lebt nach
            a.look = sees && d < 9.0f;
            a.lookAt = eye;
            if (a.mode == 0) {  // friedlich: wiedererkennen (lange genug in der Naehe gesehen), dann winken
                if (sees && d < 9.0f) a.recog += dt;
                if (a.recog > 2.5 && a.cool <= 0.0 && sees && d < 9.0f) {
                    setState(5);
                    a.cool = rng_.uniform(35.0, 60.0);
                    break;
                }
                if (d < 1.6f) break;  // nicht in den Weg laufen
            } else if (a.mode == 1) {  // reizbar
                if (lit && d < 9.0f && facing > 0.3f) a.provoke += dt;                   // Lampe im Gesicht
                if (d < 1.3f) a.provoke += dt * 0.8;                                    // bedraengt
                if (d < 1.6f && c.player->sprinting) a.provoke += 3.0;                  // angerempelt
                a.provoke = std::max(0.0, a.provoke - dt * 0.25);
                if (a.provoke > 2.5) {
                    angry(a);
                    // die Gruppe reagiert mit
                    for (auto& o : agents_)
                        if (o.get() != &a && o->role == Role::Faceling && o->group == a.group && hdist(o->pos, a.pos) < 10.0f && rng_.uniform(0.0, 1.0) < 0.65)
                            angry(*o);
                    break;
                }
            } else if (a.mode == 2) {  // feindselig: wer nah genug ist, wird bemerkt
                if (sees || hears) {
                    angry(a);
                    break;
                }
            } else if (a.mode == 3) {  // toedlich: sieht sie einen in den Gaengen, bleibt sie stehen
                if (sees && roomAllowed(c, eye, kinOf(Role::Faceling))) {
                    setState(3);
                    out.push_back({Event::Hum, Role::Faceling, head, 0.5f});
                    break;
                }
            }
            if (rng_.uniform(0.0, 1.0) < dt / 14.0) out.push_back({Event::Hum, Role::Faceling, head, 0.0f});  // leise Vokale
            wander(a, dt, c, a.mode == 3 ? 0.8 : 1.0, kinOf(Role::Faceling));
            break;
        }
        case 2: {  // aufgebracht / Angriff
            a.look = true;
            a.lookAt = eye;
            a.alert = 1.0f;
            const double speed = a.mode == 2 ? 2.7 : 2.3;
            if (d > 1.3f) walkTo(a, fld(), speed, dt, c);
            else a.yaw += wrapA(std::atan2(toP.y, toP.x) - a.yaw) * (float)std::min(1.0, dt * 6.0);
            // Schlag: bewaffnet ein Hieb mit dem Stein (Ausholen 0,5 s, Treffer bei 0,62), sonst Fuchteln
            if (a.mode == 2) {
                if (a.timer > 0.0) {
                    a.timer += dt / 0.9;
                    a.strike = (float)std::min(a.timer, 0.999);
                    if (a.timer >= 0.62 && a.timer - dt / 0.9 < 0.62 && d < 1.9f && c.mode >= 2)
                        out.push_back({Event::Hit, Role::Faceling, head, 22.0f});
                    if (a.timer >= 1.0) a.timer = 0.0, a.cool = 1.2;
                } else if (d < 1.8f && a.cool <= 0.0) {
                    a.timer = 1e-4;
                    out.push_back({Event::Swing, Role::Faceling, head});
                }
            } else {
                a.reach = d < 1.8f ? 1.0f : 0.0f;
                if (d < 1.5f && a.cool <= 0.0 && c.mode >= 2) {
                    out.push_back({Event::Hit, Role::Faceling, head, 8.0f});
                    a.cool = 1.4;
                }
            }
            if (rng_.uniform(0.0, 1.0) < dt / 3.0) out.push_back({Event::Hum, Role::Faceling, head, 1.0f});
            // reizbar: beruhigt sich nach 8-12 s oder mit Abstand; bewaffnet: verliert einen aus den kurzsichtigen Augen
            const bool calm = a.mode == 1 ? (a.stateT > 10.0 || d > 12.0f) : ((!sees && a.unseenT > 5.0 && d > 6.0f) || d > 18.0f);
            if (calm) setState(0), a.provoke = 0.0, a.timer = 0.0, a.strike = 0.0f;
            break;
        }
        case 3: {  // toedlich (rosa Kleid): Kopf schief - kommt - rennt
            a.look = true;
            a.lookAt = eye;
            const bool inHalls = roomAllowed(c, eye, kinOf(Role::Faceling));
            if (a.stateT < 1.6) {
                a.perceive = 1.0f;
                a.perceiveAt = eye;
                break;
            }
            a.alert = 1.0f;
            if (!inHalls || (a.unseenT > 6.0 && d > 8.0f) || d > 30.0f) {  // aus den Gaengen hinaus oder ausser Sicht: sie laesst ab
                setState(0);
                a.hasGoal = false;
                break;
            }
            const bool run = d < 9.0f && los;
            a.reach = d < 2.6f ? 1.0f - d / 2.6f : 0.0f;
            if (d < 1.1f && std::fabs(a.pos.z - (float)c.player->z) < 1.0f && c.mode >= 2) {
                out.push_back({Event::Attack, Role::Faceling, head, 1.0f});
                startKill(a, c);
                break;
            }
            walkTo(a, fld(), run ? 3.7 : 1.5, dt, c);
            break;
        }
        case 5: {  // winken (friedlich)
            a.look = true;
            a.lookAt = eye;
            a.wave = 1.0f;
            a.yaw += wrapA(std::atan2(toP.y, toP.x) - a.yaw) * (float)std::min(1.0, dt * 3.0);
            if (a.stateT > 3.2) setState(0), a.recog = 0.0;
            break;
        }
        default: break;
    }
    // fort, wenn weit weg und ungesehen
    if (a.state == 0 && ((d > 48.0f && a.unseenT > 3.0) || (a.age > 420.0 && a.unseenT > 5.0))) a.state = 9;
}

// --- Skin-Stealer (Entity 10) ---------------------------------------------------------------------------------------
// Satt: geht ziellos umher, harmlos, solange man ihn nicht reizt (Lampe lange im Gesicht, zu nah, anrennen). Hungrig:
// sucht Menschen, die allein unterwegs sind - kommt naeher (ungesehen schneller), spricht nach, um zu locken, haelt
// inne (Ankuendigung) und toetet mit blossen Haenden. Getarnt (mode 1) traegt er Menschenhaut. Ausweg: ruhig weggehen,
// Abstand und Deckung; im Mehrspieler: zusammenbleiben (er sucht Einzelne).
void Presence::thinkSkinStealer(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    if (a.state == 10) {
        killPose(a, dt, c);
        return;
    }
    const vec3 eye = c.view.eye;
    const float d = hdist(a.pos, eye);
    const vec3 head = a.cre.headPos();
    const bool los = clearSight(*c.world, head + vec3{0, 0, 0.05f}, eye);
    const vec3 toP = nrmF(flat(eye - a.pos), {1, 0, 0});
    const float facing = dot(toP, dirYaw(a.yaw));
    a.stateT += dt;
    a.cool -= dt;
    const vec3 chest = a.pos + vec3{0, 0, 1.45f};
    lightReact(a, c, head, 14.0f, dt, 1e9, &chest);
    const bool sees = los && d < 22.0f && (facing > 0.1f || d < 4.0f);
    a.talk = 0.0f, a.reach = 0.0f, a.perceive = 0.0f, a.alert = 0.0f, a.jaw = 0.0f;
    auto setState = [&](int s) { a.state = s, a.stateT = 0.0; };
    auto speak = [&](double lo, double hi) {  // nachsprechen: einzelne Worte, um zu locken
        a.speakT -= dt;
        if (a.speakT <= 0.0 && d < 20.0f) {
            a.speakT = rng_.uniform(lo, hi);
            a.talkT = rng_.uniform(1.0, 1.8);
            out.push_back({Event::Mimic, Role::SkinStealer, head, a.mode == 1 ? 1.0f : 0.6f});
        }
        if (a.talkT > 0.0) a.talkT -= dt, a.talk = 1.0f;
    };
    switch (a.state) {
        case 0: {  // satt: umhergehen; schaut, wenn er einen sieht
            a.look = sees && d < 14.0f;
            a.lookAt = eye;
            if (a.light > 0.5f && d < 10.0f) a.provoke += dt;
            if (d < 2.0f) a.provoke += dt * 0.7;
            if (d < 4.0f && c.player->sprinting) a.provoke += dt * 2.0;
            a.provoke = std::max(0.0, a.provoke - dt * 0.2);
            if (a.provoke > 3.0) {  // gereizt
                setState(2);
                break;
            }
            if (a.hungry && a.alone && sees) {
                setState(1);
                a.speakT = rng_.uniform(0.5, 2.0);
                break;
            }
            if (a.look && a.mode == 1) speak(7.0, 13.0);  // getarnt spricht er Worte nach (spricht kaum)
            if (a.look && d < 8.0f) {
                a.perceive = 1.0f;
                a.perceiveAt = eye;
                break;  // bleibt stehen und sieht her
            }
            wander(a, dt, c, 0.9, kinOf(Role::SkinStealer));
            break;
        }
        case 1: {  // hungrig: kommt naeher, lockt mit Worten
            a.look = los;
            a.lookAt = eye;
            speak(3.5, 6.5);
            if (!a.alone) {  // nicht allein: er gibt sich harmlos und zieht weiter
                setState(0);
                break;
            }
            if (d < 6.5f && los && a.stateT > 1.0 && a.cool <= 0.0) {
                setState(3);  // haelt inne: Ankuendigung
                a.burst = d;  // Abstand zu Beginn - weicht man ruhig zurueck, greift er nicht an
                break;
            }
            walkTo(a, fld(), a.unseenT > 0.5 ? 2.3 : 1.15, dt, c);
            if ((a.unseenT > 25.0 && d > 20.0f) || a.stateT > 90.0) setState(4), pickGoal(c, a, 260, 380, true);
            break;
        }
        case 3: {  // haelt inne (1,4 s), der Kiefer oeffnet sich - dann greift er an. Wer ruhig zurueckweicht (Wiki: langsam
                   // und ruhig weggehen), den laesst er ziehen und folgt erst wieder spaeter; wer rennt, reizt ihn
            a.look = true;
            a.lookAt = eye;
            a.alert = 1.0f;
            a.jaw = (float)std::min(1.0, a.stateT / 1.4);
            a.perceive = 1.0f;
            a.perceiveAt = eye;
            if (c.player->sprinting) {
                setState(2);
            } else if (a.stateT > 1.4) {
                if (d > a.burst + 0.7) setState(1), a.cool = 6.0;  // weicht zurueck: wieder nur folgen
                else setState(2);
            }
            break;
        }
        case 2: {  // Angriff: stuermt, greift mit beiden Haenden
            a.look = true;
            a.lookAt = eye;
            a.alert = 1.0f;
            a.jaw = 0.8f;
            a.reach = d < 2.6f ? 1.0f - d / 2.6f : 0.0f;
            if (d < 1.2f && std::fabs(a.pos.z - (float)c.player->z) < 1.0f && c.mode >= 2) {
                out.push_back({Event::Attack, Role::SkinStealer, head, 1.0f});
                startKill(a, c);
                break;
            }
            walkTo(a, fld(), 4.0, dt, c);
            if ((a.unseenT > 6.0 && d > 10.0f) || d > 28.0f) setState(4), pickGoal(c, a, 260, 380, true);
            break;
        }
        case 4: {  // zieht sich zurueck
            a.look = false;
            if (!a.hasGoal || !walkTo(a, a.field, 1.4, dt, c)) pickGoal(c, a, 260, 380, true);
            if ((d > 30.0f && a.unseenT > 2.0) || (a.stateT > 40.0 && a.unseenT > 2.0)) a.state = 9;
            break;
        }
        default: break;
    }
    if (a.state == 0 && ((d > 50.0f && a.unseenT > 3.0) || (a.age > 480.0 && a.unseenT > 5.0))) a.state = 9;
}

// --- Wretch (Entity 15) ---------------------------------------------------------------------------------------------
// Feindselig, haelt sich im Schatten. Zwei Arten (die Seite: der Laut verraet die Gefahr): mode 0 schlurft (tiefes,
// langsames Gurgeln), mode 1 sprintet schneller als ein Mensch (hohes Fauchen) - in Stoessen, dazwischen ringt er nach
// Luft. Schlaege mit uebermenschlicher Kraft (Schaden, nicht sofort toedlich). Im zweiten Stadium (pleads) fleht er
// um Wasser, Essen, Hilfe und greift nicht an.
void Presence::thinkWretch(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    const vec3 eye = c.view.eye;
    const float d = hdist(a.pos, eye);
    const vec3 head = a.cre.headPos();
    const bool los = clearSight(*c.world, head + vec3{0, 0, 0.05f}, eye);
    const vec3 toP = nrmF(flat(eye - a.pos), {1, 0, 0});
    const float facing = dot(toP, dirYaw(a.yaw));
    a.stateT += dt;
    a.cool -= dt;
    const vec3 chest = a.pos + vec3{0, 0, 1.1f};
    lightReact(a, c, head, 12.0f, dt, 1.2, &chest);
    const bool playerLit = c.view.flashOn || litPoint(c.view, *c.world, eye - vec3{0, 0, 0.6f});
    const bool sees = los && d < (playerLit ? 18.0f : 7.0f) && (facing > 0.1f || d < 4.0f);
    const int here = fld().at(ifloor(a.pos.x), ifloor(a.pos.y));
    const float pathD = here >= 0 ? (float)here / 10.0f : 1e9f;
    const bool hears = (c.player->sprinting && pathD < (a.mode == 1 ? 24.0f : 16.0f)) || (pathD < 6.0f && c.player->gait > 0.3f);
    const float pitch = a.mode == 1 ? 1.0f : 0.0f;
    a.reach = 0.0f, a.plea = 0.0f, a.growl = 0.0f, a.alert = 0.0f;
    auto setState = [&](int s) { a.state = s, a.stateT = 0.0; };
    a.timer -= dt;
    if (a.timer <= 0.0 && d < 26.0f) {  // Laute: Gurgeln (traege), Fauchen (schnell), Flehen (zweites Stadium)
        a.timer = a.pleads ? rng_.uniform(3.5, 6.5) : rng_.uniform(a.state == 2 ? 2.0 : 5.0, a.state == 2 ? 4.0 : 11.0);
        out.push_back({a.pleads ? Event::Plead : Event::Gurgle, Role::Wretch, head, pitch});
        a.growlT = 1.2;
    }
    if (a.growlT > 0.0) a.growlT -= dt, a.growl = 1.0f;
    if (a.pleads) {  // fleht: dreht sich zu, kommt langsam nach (Abstand 3 m), greift nie an
        a.look = los && d < 16.0f;
        a.lookAt = eye;
        if (a.look) {
            a.plea = 1.0f;
            if (d > 3.2f && d < 14.0f) walkTo(a, fld(), 0.6, dt, c);
            else a.yaw += wrapA(std::atan2(toP.y, toP.x) - a.yaw) * (float)std::min(1.0, dt * 2.0);
        } else {
            wander(a, dt, c, 0.5, kinOf(Role::Wretch));
        }
        if ((d > 40.0f && a.unseenT > 3.0) || (a.age > 240.0 && a.unseenT > 3.0)) a.state = 9;
        return;
    }
    switch (a.state) {
        case 0: {  // lauert im Schatten, schlurft umher
            a.look = false;
            if (sees || hears) {
                setState(2);
                a.timer = 0.0;  // gleich ein Laut
                a.burst = rng_.uniform(4.0, 6.0);
                break;
            }
            wander(a, dt, c, a.mode == 1 ? 1.1 : 0.55, kinOf(Role::Wretch));
            break;
        }
        case 2: {  // jagt
            a.look = los;
            a.lookAt = eye;
            a.alert = 1.0f;
            // Sprinter in Stoessen: 4-6 s Lauf, 2-3 s ringt er nach Luft (die Gelegenheit zu entkommen)
            double speed = 1.35;
            if (a.mode == 1) {
                a.burst -= dt;
                if (a.burst <= 0.0 && a.timer2 <= 0.0) a.timer2 = rng_.uniform(2.0, 3.0);
                if (a.timer2 > 0.0) {
                    a.timer2 -= dt;
                    speed = 0.4;
                    a.growl = 1.0f;
                    if (a.timer2 <= 0.0) a.burst = rng_.uniform(4.0, 6.0);
                } else {
                    speed = 4.9;
                }
            }
            if (d < 1.6f) {
                a.reach = 1.0f;
                a.yaw += wrapA(std::atan2(toP.y, toP.x) - a.yaw) * (float)std::min(1.0, dt * 5.0);
                if (a.cool <= 0.0 && c.mode >= 2 && std::fabs(a.pos.z - (float)c.player->z) < 1.0f) {
                    out.push_back({Event::Hit, Role::Wretch, head, a.mode == 1 ? 26.0f : 34.0f});
                    a.cool = a.mode == 1 ? 1.3 : 1.8;
                }
            } else {
                walkTo(a, fld(), speed, dt, c);
            }
            if (sees || hears) a.lostT = 0.0;
            else a.lostT += dt;
            if (a.lostT > (a.mode == 1 ? 4.0 : 7.0) || d > 34.0f) setState(4), pickGoal(c, a, 220, 360, true);
            break;
        }
        case 4: {  // laesst ab, zurueck ins Dunkel
            a.look = false;
            if (sees && d < 10.0f) {
                setState(2);
                break;
            }
            if (!a.hasGoal || !walkTo(a, a.field, a.mode == 1 ? 1.2 : 0.6, dt, c)) pickGoal(c, a, 220, 360, true);
            if ((d > 30.0f && a.unseenT > 2.0) || (a.stateT > 45.0 && a.unseenT > 2.0)) a.state = 9;
            break;
        }
        default: break;
    }
    if (a.state == 0 && ((d > 50.0f && a.unseenT > 3.0) || (a.age > 420.0 && a.unseenT > 5.0))) a.state = 9;
}

// --- Death Rat (Entity 24) ------------------------------------------------------------------------------------------
// Meist harmlos. Klein, in Gruppen, huscht an den Waenden entlang, schnueffelt, frisst. Kommt man nah (oder rennt), stieben
// sie auseinander und quieken; bedraengt (Licht lange auf ihr, sehr nah) beisst eine zu - wie die Seite sagt: man weiss nie,
// wie sie reagieren. Sie fallen ueber andere Wesen her (das "Screaming Event": Kreischen, Flackern) - siehe regie.
void Presence::thinkRat(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    const vec3 feet{(float)c.player->x, (float)c.player->y, (float)c.player->z};
    const float d = hdist(a.pos, feet);
    a.stateT += dt;
    a.cool -= dt;
    lightReact(a, c, a.pos + vec3{0, 0, 0.08f}, 10.0f, dt, 1e9);
    a.reach = 0.0f, a.jaw = 0.0f, a.perceive = 0.0f;
    auto setState = [&](int s) { a.state = s, a.stateT = 0.0; };
    if (a.light > 0.5f && d < 4.0f) a.provoke += dt;
    if (d < 0.9f) a.provoke += dt * 1.5;
    a.provoke = std::max(0.0, a.provoke - dt * 0.3);
    const bool scare = d < (c.player->sprinting ? 6.0f : 3.0f) || (a.light > 0.5f && d < 6.0f);
    switch (a.state) {
        case 0: {  // schnueffeln, fressen, huschen
            if (a.provoke > 2.5 && d < 1.6f && a.cool <= 0.0) {  // bedraengt: beisst zu
                a.reach = 1.0f;
                a.cool = 3.0;
                a.provoke = 0.0;
                out.push_back({Event::Squeak, Role::Rat, a.pos, 1.0f});
                if (c.mode >= 2) out.push_back({Event::Hit, Role::Rat, a.pos, 6.0f});
                setState(1);
                break;
            }
            if (scare) {
                setState(1);
                out.push_back({Event::Squeak, Role::Rat, a.pos, 0.6f});
                // weg vom Spieler
                const vec3 away = nrmF(flat(a.pos - feet), dirYaw(a.yaw));
                a.hasGoal = false;
                for (int t = 0; t < 10 && !a.hasGoal; ++t) {
                    const vec3 g = a.pos + rotateYaw(away, (float)rng_.uniform(-0.9, 0.9)) * (float)rng_.uniform(4.0, 7.0);
                    const world::Tile* tl = c.world->tileIfLoaded(ifloor(g.x), ifloor(g.y));
                    if (!figureWalkable(tl, 0.4f)) continue;
                    a.field.build(*c.world, ifloor(g.x), ifloor(g.y), 0.4f);
                    if (a.field.at(ifloor(a.pos.x), ifloor(a.pos.y)) < 0) continue;
                    a.goal = {g.x, g.y, tl->floor};
                    a.hasGoal = true;
                }
                break;
            }
            if (!wander(a, dt, c, 0.35, kinOf(Role::Rat))) {  // steht: schnueffeln oder fressen
                a.perceive = std::fmod(a.age, 9.0) < 4.0 ? 1.0f : 0.0f;
                a.perceiveAt = a.pos + dirYaw(a.yaw) * 1.5f + vec3{0, 0, 0.3f};
                a.jaw = a.perceive > 0.5f ? 0.0f : 1.0f;
            }
            if (rng_.uniform(0.0, 1.0) < dt / 9.0 && d < 14.0f) out.push_back({Event::Squeak, Role::Rat, a.pos, 0.2f});
            break;
        }
        case 1: {  // stiebt davon
            if (!a.hasGoal || !walkTo(a, a.field, 2.2, dt, c) || hdist(a.pos, a.goal) < 0.6f) {
                if (!scare) setState(0), a.hasGoal = false, a.timer2 = rng_.uniform(1.0, 3.0);
                else if (a.stateT > 2.0) a.state = 9;  // fort in eine Ritze (ungesehen) oder weiter weg
            }
            break;
        }
        default: break;
    }
    if (a.state == 9 && a.sight.level > 0.25f) a.state = 1, a.stateT = 0.0;  // nicht vor den Augen verschwinden
    if (a.state == 0 && ((d > 40.0f && a.unseenT > 3.0) || (a.age > 360.0 && a.unseenT > 5.0))) a.state = 9;
}

// --- Fensterwesen (Entity 2) ----------------------------------------------------------------------------------------
// Sitzt in einem Fenster und bleibt dort. Lockt mit Fluestern (trostend) und mit der Hand. Wer naeher als etwa 1,9 m vor
// das Fenster tritt, nach dem greift ein Arm heraus - wer nicht sofort zuruecktritt, wird hineingezogen (toedlich).
void Presence::placeWindows(const Context& c) {
    windowsPlaced_ = true;
    if (!c.kin || !c.kin[world::KIN_WINDOW].on) return;
    const world::KinDef& K = c.kin[world::KIN_WINDOW];
    const world::Sector* sec = c.world->sectorIfReady(c.player->x, c.player->y);
    if (!sec) {
        windowsPlaced_ = false;  // spaeter noch einmal
        return;
    }
    std::vector<const world::Sector::Spot*> cand;
    for (const auto& sp : sec->spots)
        if (sp.kind == "window" && sp.model == (K.mode.empty() ? "window_188_c" : K.mode) && sp.z < 3.0f) cand.push_back(&sp);
    for (int i = 0; i < K.count && !cand.empty(); ++i) {
        const size_t k = (size_t)rng_.randint(0, (i64)cand.size() - 1);
        const auto* sp = cand[k];
        cand.erase(cand.begin() + (std::ptrdiff_t)k);
        auto a = std::make_unique<Agent>();
        a->role = Role::Window;
        a->ent = Entity::Window;
        a->glow = 1.0f;
        a->noShadow = true;
        a->initBody({sp->x, sp->y, sp->z}, sp->yaw, (unsigned)rng_.randint(1, 1 << 30));
        a->rig.setGlow(1.0f);
        a->timer = rng_.uniform(1.0, 4.0);
        log::info("Praesenz: Fensterwesen bei ({:.1f}, {:.1f})", sp->x, sp->y);
        agents_.push_back(std::move(a));
    }
}

void Presence::thinkWindow(Agent& a, double dt, const Context& c, std::vector<Event>& out) {
    const vec3 eye = c.view.eye;
    const vec3 n = dirYaw(a.yaw), l{-n.y, n.x, 0.0f};
    const vec3 rel = eye - a.pos;
    const float dn = dot(rel, n), dl = dot(rel, l);
    const float d = hdist(a.pos, eye);
    const vec3 head = a.cre.headPos();
    a.stateT += dt;
    a.cool -= dt;
    const bool front = dn > 0.15f && std::fabs(dl) < 1.0f;  // vor der Oeffnung
    const bool sees = dn > 0.2f && d < 16.0f && clearSight(*c.world, head + n * 0.1f, eye);
    a.look = sees;
    a.lookAt = eye;
    a.perceive = sees && d < 10.0f ? 1.0f : 0.0f;
    a.wave = sees && d < 14.0f ? 1.0f : 0.0f;
    if (a.state == 10) {  // haelt fest und zieht hinein
        a.reach = 1.0f;
        if (a.stateT > 2.6) a.state = 0, a.stateT = 0.0, a.reach = 0.0f;
        return;
    }
    // Fluestern: trostend, lockend - je naeher, desto oefter
    a.timer -= dt;
    if (a.timer <= 0.0 && d < 11.0f && dn > 0.0f) {
        a.timer = rng_.uniform(2.5, 5.0) * (0.5 + 0.5 * std::clamp(d / 11.0f, 0.0f, 1.0f));
        out.push_back({Event::Whisper, Role::Window, head + n * 0.2f, std::clamp(1.0f - d / 11.0f, 0.2f, 1.0f)});
    }
    switch (a.state) {
        case 0: {  // lockt
            a.reach = 0.0f;
            if (front && dn < 1.9f && a.cool <= 0.0) {
                a.state = 2, a.stateT = 0.0;
                out.push_back({Event::Pull, Role::Window, a.pos + n * 0.3f});
            }
            break;
        }
        case 2: {  // der Arm faehrt heraus (0,7 s), dann greift er - wer zurueckgetreten ist, entkommt
            a.reach = 1.0f;
            if (a.stateT > 0.7) {
                if (front && dn < 2.3f && std::fabs((float)c.player->z - (a.pos.z - 1.9f)) < 1.2f && c.mode >= 2) {
                    out.push_back({Event::Attack, Role::Window, head, 1.0f});
                    a.state = 10, a.stateT = 0.0;
                } else {
                    a.state = 0, a.stateT = 0.0, a.cool = 4.0;
                }
            }
            break;
        }
        default: break;
    }
}

// --- V13.2 Koop -----------------------------------------------------------------------------------

}  // namespace lim::game
