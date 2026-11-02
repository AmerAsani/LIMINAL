// V13.3: purpifizierte tote Wesen in den Ziegelwaenden von Level 3 in der Sitzung (gameplay/purpification.hpp,
// KREATUR_L3_Purpur.md).
//
// Einzelspieler/Host ziehen den Verstand ab (Strahlung) - der Host auch fuer die Figuren der Gaeste aus deren Lage; der
// Gast rechnet fuer sich nur Klang, Hinweise, Lampe und Bild. Kein Netzverkehr (Protokoll unveraendert).
#include <algorithm>
#include <cmath>

#include "gameplay/session.hpp"

namespace lim::game {

void GameSession::updatePurple(double dt) {
    if (!purple_.any()) return;
    purpleEvents_.clear();
    const bool auth = netRole != NetRole::Guest;
    const vec3 eye = body_.eye();
    const FlashBeam fb = flashBeam();
    purple_.update(dt, eye, forwardFromAngles((float)player_.angle, (float)player_.pitch), !stats_.dead && !ride_.active, fb.on,
                   fb.pos, fb.dir, purpleEvents_);
    if (auth && !stats_.dead) stats_.sanity = std::max(0.0, stats_.sanity - dt * purple_.sanityDrain(eye));
    if (netRole == NetRole::Host)
        for (Pawn* p : pawns) {
            if (!p || !p->alive) continue;
            const vec3 pe{(float)p->player.x, (float)p->player.y, (float)p->player.eye()};
            p->stats.sanity = std::max(0.0, p->stats.sanity - dt * purple_.sanityDrain(pe));
        }
    for (const Purpification::Event& e : purpleEvents_) {
        if (e.kind == Purpification::Event::Toast) {
            events_.push_back({GameEvent::Toast, e.text, e.value});
        } else {
            GameEvent ev{GameEvent::SoundAt};
            ev.sound = e.text, ev.pos = e.pos, ev.intensity = e.value;
            events_.push_back(ev);
        }
    }
}

void GameSession::collectPurpleSounds(std::vector<SoundSource>& out) {
    for (Purpification::Site& s : purple_.sites()) out.push_back({ecs::kNull, &s.loop, s.glow, 0.9f, 13.0f, &s.voice});
}

}  // namespace lim::game
