// V13.3 Level 3 (KREATUR_Smiler.md, Abschnitt "V13.3: Level 3 - Angriff ohne Anlass"; Wikidot level-3 Rev. 18, CC BY-SA 3.0):
// Die Smiler dort verbergen Augen und Zaehne hinter Lidern und Lippen und gelten als Wesen, die unter allen Umstaenden
// angreifen - anders als die Smiler anderswo, die nur gereizt oder erschreckt zugreifen. Rat der Seite: im Zweifel ruhig und
// leise umkehren, nicht darauf eingehen.
//
// Umsetzung (presence.smiler_unprovoked: Anteil der Smiler, die es tun; 0 = nie, wie auf allen anderen Ebenen):
//   geplant    beim ersten Denken wird gewuerfelt; wer es tut, wartet 9..22 s, waehrend der Wanderer 4..15 m entfernt ist
//              (zu nah: es waere unfair; zu weit: er bemerkt es nicht). Ruhig stehen und hinsehen aendert nichts daran.
//   Ankuendigung (2,2 s, Stufe 4): die Lider und Lippen pressen sich zu (das Gesicht wird fast dunkel), dann reissen sie auf -
//              Augen und Zaehne in voller Helligkeit, das Laecheln weit offen, ein nasses Abloesen, Zaehneklappern und ein
//              steigender, duenner Ton (smiler_peel; Announce mit Staerke 2). Er ruehrt sich dabei nicht.
//   Ansturm    (Stufe 5): 4,2 m/s auf den Wanderer zu, hoechstens 6 s.
//              - Weglaufen hilft: der Sprint ist schneller (5,7 m/s); waechst der Abstand ueber 13 m, laesst er ab.
//              - Licht aus hilft: das Licht zieht ihn an (Entity 3). Ist die Lampe aus und steht der Wanderer im Dunkeln,
//                verliert er ihn und stuermt zu der Stelle, an der er das Licht zuletzt sah (Stufe 6); wer dort nicht mehr
//                steht (1,6 m), ist sicher - er laesst ab und vergeht.
//              - Raumlicht hilft wie immer: betritt er eine beleuchtete Kachel, vergeht er (thinkSmiler).
//   Ablassen   Calm-Ereignis (das Laecheln verklingt), dann vergeht er (Zustand 8).
#include "gameplay/presence.hpp"
#include "gameplay/presence_agent.hpp"

#include "core/log.hpp"

namespace lim::game {

namespace {
constexpr double kAnnounce = 2.2;   // s
constexpr double kChargeMax = 6.0;  // s
constexpr double kChargeSpeed = 4.2, kSearchSpeed = 3.6;
constexpr float kOutrun = 13.0f, kMissed = 1.6f;
}  // namespace

bool Presence::unprovokedSmiler(Agent& a, double dt, const Context& c, std::vector<Event>& out, float d, bool dark) {
    (void)dark;
    if (c.smilerUnprovoked <= 0.0 || a.showcase || a.puppet) return false;
    // mode: 0 noch nicht gewuerfelt, 1 tut es nicht, 2 geplant (provoke: Rest der Wartezeit), 3 vorbei
    if (a.mode == 0) {
        a.mode = rng_.uniform(0.0, 1.0) < c.smilerUnprovoked ? 2 : 1;
        a.provoke = rng_.uniform(9.0, 22.0);
        if (a.mode == 2) log::info("Praesenz: Smiler (Level 3) wird ohne Anlass angreifen (in {:.0f} s)", a.provoke);
    }
    const vec3 face = a.cre.headPos();
    if (a.mode == 2 && a.stage <= 1) {
        if (a.lureId != 0) return false;  // stutzt gerade am Leuchtstab (Level 3: kurz)
        if (d > 4.0f && d < 15.0f) a.provoke -= dt;
        if (a.provoke > 0.0) return false;
        a.mode = 3;
        a.stage = 4;
        a.timer2 = kAnnounce;
        a.burst = 0.0;
        a.heard = vec3{(float)c.player->x, (float)c.player->y, (float)c.player->z};  // wo er den Wanderer zuletzt "sah"
        out.push_back({Event::Announce, Role::Smiler, face, 2.0f});
        log::info("Praesenz: Smiler (Level 3) - Lider und Lippen reissen auf, Angriff ohne Anlass ({:.1f} m)", d);
        return true;
    }
    if (a.stage < 4) return false;
    // das Licht zieht ihn an (Entity 3): solange die Lampe brennt oder der Wanderer im Raumlicht steht, weiss er, wo er ist
    const world::Tile* pt = c.world->tileIfLoaded(ifloor(c.player->x), ifloor(c.player->y));
    const bool playerLit = c.view.flashOn || (pt && !darkTile(c, pt, (float)c.player->x, (float)c.player->y));
    if (playerLit && a.stage <= 5) a.heard = vec3{(float)c.player->x, (float)c.player->y, (float)c.player->z};
    a.lookAt = a.stage == 6 ? a.heard + vec3{0, 0, 1.0f} : c.view.eye;
    if (a.stage == 4) {  // Ankuendigung: erst zupressen (fast dunkel), dann aufreissen, zitternd weit offen
        a.timer2 -= dt;
        const double t = kAnnounce - a.timer2;
        if (t < 0.4) a.glow = (float)std::max(0.12, 1.0 - t / 0.4 * 0.88), a.jaw = 0.08f;
        else if (t < 0.65) a.glow = std::min(1.0f, 0.12f + (float)((t - 0.4) / 0.25)), a.jaw = std::min(1.0f, (float)((t - 0.4) / 0.25));
        else a.glow = 1.0f, a.jaw = 1.0f;
        a.tilt = t > 0.65 ? 0.06f * (float)std::sin(t * 37.0) : 0.0f;
        if (a.timer2 <= 0.0) {
            a.stage = 5;
            a.burst = 0.0;
            a.tilt = 0.0f;
            log::info("Praesenz: Smiler (Level 3) - Ansturm");
        }
        return true;
    }
    a.glow = 1.0f, a.jaw = 1.0f;
    a.burst += dt;
    auto giveUp = [&](const char* why) {
        out.push_back({Event::Calm, Role::Smiler, face});
        a.state = 8;
        a.stage = 1;
        log::info("Praesenz: Smiler (Level 3) laesst ab ({})", why);
    };
    // Zugriff
    if (d < 1.05f && std::fabs(a.pos.z - (float)c.player->z) < 1.4f) {
        if (c.mode >= 2) {
            out.push_back({Event::Attack, Role::Smiler, face, 1.0f});
            startKill(a, c);
            return true;
        }
        out.push_back({Event::Flicker, Role::Smiler, face});
        out.push_back({Event::Gone, Role::Smiler, face});
        a.state = 9;
        return true;
    }
    if (a.stage == 5) {
        if (!playerLit && d > 1.8f) {  // Licht aus, der Wanderer im Dunkeln: zur Stelle, an der er ihn zuletzt im Licht sah
            a.field.build(*c.world, ifloor(a.heard.x), ifloor(a.heard.y), 1.9f);
            a.stage = 6;
            log::info("Praesenz: Smiler (Level 3) - das Licht ist aus, er stuermt zur letzten Stelle ({:.1f} m)", hdist(a.pos, a.heard));
            return true;
        }
        if (d > kOutrun) return giveUp("abgehaengt"), true;
        if (a.burst > kChargeMax) return giveUp("Zeit"), true;
        if (!walkTo(a, fld(), kChargeSpeed, dt, c)) return giveUp("kein Weg"), true;
        return true;
    }
    // Stufe 6: zur letzten Stelle des Lichts
    if (c.view.flashOn && d < 9.0f) {  // die Lampe geht wieder an: er hat ihn wieder
        a.stage = 5;
        return true;
    }
    if (hdist(a.pos, a.heard) < 0.9f || a.burst > kChargeMax || !walkTo(a, a.field, kSearchSpeed, dt, c)) {
        if (d > kMissed) giveUp("verfehlt, Licht aus");
        else a.stage = 5;  // er steht noch da
    }
    return true;
}

}  // namespace lim::game
