// V13.3: zwei Verhaltensregeln aus der Wesenliste von Level 1 (Wikidot, Seite level-1, Rev. 83; entity-3, Rev. 83;
// entity-10, Rev. 61; CC BY-SA 3.0) - KREATUR_Smiler.md und KREATUR_SkinStealer.md:
//   Smiler       angezogen von Licht; Rat: Licht aus oder die Lichtquelle als Koeder wegwerfen. Ein geworfener, leuchtender
//                Leuchtstab ist ein staerkeres Licht - der Smiler wendet sich ab, gleitet durch das Dunkel zum Stab und
//                bleibt bei ihm, solange er leuchtet. Level 3 (Seite level-3): dort stutzen sie nur kurz.
//   Skin-Stealer greift meist erst nach laengerer Interaktion an; Rat: ansprechen - ist klar, dass es kein Mensch ist, ruhig
//                zurueckweichen. Angesprochen antwortet er nach einer Pause mit nachgeahmten, falsch betonten Silben.
//                Ruhig zurueckweichen (gehen, schleichen) laesst ihn ziehen; Rennen, erneutes Ansprechen oder zu lange
//                Naehe loesen den Angriff aus.
#include "gameplay/presence.hpp"
#include "gameplay/presence_agent.hpp"

#include "core/log.hpp"

namespace lim::game {

// --- Smiler am Leuchtkoeder ------------------------------------------------------------------------------------------
bool Presence::baitSmiler(Agent& a, double dt, const Context& c, std::vector<Event>& out, float d, bool dark) {
    a.bait = 0.0f;
    a.lureT = std::max(0.0, a.lureT - dt);
    if (!c.lures || a.state < 1 || a.state >= 8 || a.stage >= 3) {  // im Zugriff hilft kein Koeder mehr
        a.lureId = 0;
        return false;
    }
    const vec3 face = a.cre.headPos();
    // welches Licht ist staerker - vom Smiler aus gesehen? Die eingeschaltete Lampe zaehlt 1, ein frischer Stab bis 1,6,
    // beides schwaecher mit dem Abstand. Den Schein des Stabs sieht er um Ecken, wenn er nah ist (unter 6 m)
    const float own = c.view.flashOn ? 1.0f / (1.0f + d / 6.0f) : 0.0f;
    const Lure* best = nullptr;
    float bestS = own;
    for (const Lure& L : *c.lures) {
        if (L.strength < 0.06f || L.id == a.lureSkip) continue;
        const float dl = hdist(L.pos, a.pos);
        if (dl > 22.0f || std::fabs(L.pos.z - a.pos.z) > 2.5f) continue;
        if (dl > 6.0f && !clearSight(*c.world, face, L.pos + vec3{0, 0, 0.12f})) continue;
        const float s = L.strength / (1.0f + dl / 6.0f);
        if (s > bestS) bestS = s, best = &L;
    }
    // Laerm ganz in seiner Naehe (Rennen, harte Landung - Wiki, Level 1: nach einer Sichtung zieht ihn auch Laerm an):
    // er laesst vom Licht ab und wendet sich wieder dem Spieler zu (diesen Stab beachtet er nicht mehr)
    bool noisy = (c.player->sprinting && d < 5.0f) || (c.noise >= 0.7 && d < 5.0f);
    if (c.pings)
        for (const auto& [where, vol] : *c.pings) noisy = noisy || (vol >= 0.5f && hdist(where, a.pos) < 5.0f);
    if (best && noisy) {
        log::info("Praesenz: Smiler - Laerm ganz nah, er laesst vom Licht ab");
        a.lureSkip = best->id;
        best = nullptr;
    }
    if (!best) {
        if (a.lureId != 0) log::info("Praesenz: Smiler - das Licht haelt ihn nicht mehr");
        a.lureId = 0;
        return false;
    }
    if (best->id != a.lureId) {
        a.lureId = best->id;
        out.push_back({Event::Lured, Role::Smiler, face, c.smilerBait == 1 ? 0.0f : 1.0f});
        if (a.stage == 2) a.stage = 1;  // die Ankuendigung bricht ab: es hat sich abgewandt
        if (c.smilerBait == 1) {
            a.lureT = 2.6;  // Level 3: er zuckt, haelt an, ueberlegt - dann weiter zum Spieler
        } else {
            a.field.build(*c.world, ifloor(best->pos.x), ifloor(best->pos.y), 1.1f);
            a.goal = best->pos, a.hasGoal = true;
        }
        log::info("Praesenz: Smiler - wendet sich dem Leuchtstab zu ({:.1f} m, {})", hdist(best->pos, a.pos),
                  c.smilerBait == 1 ? "stutzt nur" : "folgt dem Licht");
    }
    a.look = true;
    a.lookAt = best->pos + vec3{0, 0, 0.05f};
    a.perceive = 1.0f;
    a.perceiveAt = best->pos;
    a.light = 0.0f;
    a.bait = 1.0f;  // Animationskanal "Koeder": beim Gast sieht die Puppe zum Stab
    const float dl = hdist(a.pos, best->pos);
    if (c.smilerBait == 1) {
        if (a.lureT <= 0.0) {  // ueberlegt - und bleibt beim Wanderer
            a.lureSkip = a.lureId;
            a.lureId = 0;
            log::info("Praesenz: Smiler - wendet sich wieder dem Spieler zu (Level 3)");
            return false;
        }
        a.jaw = 0.3f;
        a.tilt = 0.22f * std::sin((float)a.lureT * 9.0f);  // zuckt
        return true;
    }
    // er bleibt beim Licht, solange es leuchtet; wer weit weg ist (oder ausser Sicht), verliert dabei sein Interesse
    if (d > 12.0f || !clearSight(*c.world, c.view.eye, face)) a.lostT = std::min(16.0, a.lostT + dt * 0.7);
    if (!dark || a.age > 170.0) return false;  // Raumlicht oder zu lange da: wie sonst verloeschen
    a.jaw = std::clamp(1.3f - dl / 4.0f, 0.3f, 0.85f);  // das Laecheln oeffnet sich, je naeher er dem Licht ist
    if (dl > 1.1f) {
        if (hdist(best->pos, a.goal) > 0.8f) {  // der Stab rutscht noch: Weg neu
            a.field.build(*c.world, ifloor(best->pos.x), ifloor(best->pos.y), 1.1f);
            a.goal = best->pos;
        }
        // nur durch das Dunkel (wie sonst): am Rand von Raumlicht bleibt er stehen und starrt hinueber
        double tx, ty;
        if (a.field.next(a.pos.x, a.pos.y, tx, ty)) {
            const world::Tile* nt = c.world->tileIfLoaded(ifloor(tx), ifloor(ty));
            if (darkTile(c, nt, (float)tx, (float)ty)) walkTo(a, a.field, 1.7, dt, c);
        }
    } else {
        const float want = std::atan2(best->pos.y - a.pos.y, best->pos.x - a.pos.x);
        a.yaw += wrapA(want - a.yaw) * (float)std::min(1.0, dt * 3.0);
    }
    return true;
}

bool Presence::luredOf(Role r) const {
    for (const auto& a : agents_)
        if (a->role == r && !a->showcase) return a->lureId != 0;
    return false;
}

// --- Ansprechen ------------------------------------------------------------------------------------------------------
bool Presence::speakable(world::World& w, vec3 eye, vec3 look, float maxDist, vec3* head) const {
    float best = 1e9f;
    for (const auto& ap : agents_) {
        const Agent& a = *ap;
        if (a.showcase || a.state >= 8) continue;
        if (a.role != Role::SkinStealer && a.role != Role::Faceling && a.role != Role::Wretch) continue;
        const vec3 h = a.cre.headPos(), dv = h - eye;
        const float dist = length(dv);
        if (dist > maxDist || dist < 0.3f || dot(dv / dist, look) < 0.95f) continue;  // ~18 Grad um die Blickrichtung
        if (!clearSight(w, eye, h) && !clearSight(w, eye, a.pos + vec3{0, 0, 1.2f})) continue;
        if (dist < best) {
            best = dist;
            if (head) *head = h;
        }
    }
    return best < 1e8f;
}

bool Presence::address(world::World& w, vec3 eye, vec3 look, int by, int phrase) {
    vec3 head;
    if (!speakable(w, eye, look, 6.5f, &head)) return false;
    for (auto& ap : agents_) {
        Agent& a = *ap;
        if (a.showcase || a.role != Role::SkinStealer || length(a.cre.headPos() - head) > 0.05f) continue;
        a.addressed = true, a.addrBy = by, a.phrase = std::clamp(phrase, 0, 3);
        log::info("Praesenz: Skin-Stealer wird angesprochen (Spieler {}, Satz {})", by, phrase);
        return true;
    }
    return false;  // ein Faceling oder Wretch: keine Antwort
}

bool Presence::exposedOf(Role r) const {
    for (const auto& a : agents_)
        if (a->role == r && !a->showcase) return a->exposed;
    return false;
}

// Zustaende: 5 angesprochen (haelt inne, lauscht, antwortet nach einer Pause), 6 entlarvt (steht, Kopf schief, beobachtet)
bool Presence::talkSkinStealer(Agent& a, double dt, const Context& c, std::vector<Event>& out, float d, bool los) {
    const vec3 eye = c.view.eye, head = a.cre.headPos();
    auto setState = [&](int s) { a.state = s, a.stateT = 0.0; };
    auto attack = [&](const char* why) {
        out.push_back({Event::Shriek, Role::SkinStealer, head, 1.0f});
        log::info("Praesenz: Skin-Stealer - {}: Angriff", why);
        setState(2);
    };
    if (a.state != 5 && a.state != 6) a.tilt = 0.0f;
    // Dauer der Begegnung (Wiki, Level 1: sie greifen meist erst nach laengerer Interaktion an): nah und in Sicht
    if (d < 5.0f && los) a.chat += dt;
    else a.chat = std::max(0.0, a.chat - dt * 0.25);
    if (a.addressed) {
        a.addressed = false;
        if (a.exposed && a.state != 2) {  // erneutes Ansprechen, obwohl klar ist, dass er kein Mensch ist
            attack("erneut angesprochen");
            return true;
        }
        if (a.state <= 4) {
            setState(5);
            a.replyT = rng_.uniform(1.1, 2.1);  // die Antwort kommt zu spaet
            a.chat += 3.0;
        }
    }
    if (a.state == 5) {  // haelt inne, sieht her, legt den Kopf schief - dann die Antwort
        a.look = true, a.lookAt = eye;
        a.perceive = 1.0f, a.perceiveAt = eye;
        a.tilt = 0.22f * (float)std::min(1.0, a.stateT / 0.7);
        if (c.player->sprinting && d < 12.0f) {
            attack("Rennen");
            return true;
        }
        a.replyT -= dt;
        if (a.replyT <= 0.0) {
            const int style = (int)rng_.randint(0, 2);
            out.push_back({Event::Reply, Role::SkinStealer, head, ((float)(a.phrase * 3 + style) + 0.5f) / 16.0f});
            a.talkT = 1.9;
            a.exposed = true;
            a.burst = d;  // Abstand bei der Antwort - wer von hier ruhig zurueckweicht, den laesst er ziehen
            log::info("Praesenz: Skin-Stealer antwortet (Satz {}, Art {}) - entlarvt bei {:.1f} m", a.phrase, style, d);
            setState(6);
        }
        return true;
    }
    if (a.state == 6) {  // entlarvt: steht, sieht her, der Kopf zu schief, der Mund arbeitet nach
        a.look = true, a.lookAt = eye;
        a.perceive = 1.0f, a.perceiveAt = eye;
        a.tilt = 0.38f + 0.05f * std::sin((float)a.stateT * 1.3f);
        a.alert = (float)std::min(1.0, a.stateT / 5.0);
        if (a.talkT > 0.0) a.talkT -= dt, a.talk = 1.0f;
        if (c.player->sprinting && d < 14.0f) {
            attack("Rennen nach der Entlarvung");
            return true;
        }
        if (d < 1.6f && a.stateT > 0.8) {
            attack("zu nah");
            return true;
        }
        if (a.stateT > 9.0 && d < a.burst + 2.0f) {  // bleibt man stehen, wird die Begegnung zu lang
            attack("zu lange Interaktion");
            return true;
        }
        if (d > a.burst + 3.5f || d > 11.0f) {  // ruhig zurueckgewichen: er laesst ihn ziehen
            a.cool = 60.0, a.provoke = 0.0, a.chat = 0.0;
            if (a.hungry) setState(4), pickGoal(c, a, 260, 380, true);
            else setState(0);
            log::info("Praesenz: Skin-Stealer - ruhig zurueckgewichen ({:.1f} m), er laesst ihn ziehen", d);
        }
        return true;
    }
    // ohne Ansprechen: steht man lange nah bei einem satten Skin-Stealer, spricht er selbst - ein paar nachgeahmte Silben,
    // dann steht er da wie entlarvt (ruhiges Zurueckweichen hilft, Stehenbleiben nicht)
    if (a.state == 0 && a.chat > 26.0 && a.cool <= 0.0) {
        log::info("Praesenz: Skin-Stealer - lange Interaktion, er spricht von selbst");
        out.push_back({Event::Mimic, Role::SkinStealer, head, a.mode == 1 ? 1.0f : 0.6f});
        a.chat = 0.0, a.talkT = 1.6, a.exposed = true, a.burst = d;
        setState(6);
        return true;
    }
    return false;
}

}  // namespace lim::game
