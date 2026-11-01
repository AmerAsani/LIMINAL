// V13.3: Innenleben der Praesenz (nur fuer gameplay/presence*.cpp): Hilfen und der Zustand eines Wesens (Agent).
// Neue Wesen wohnen in eigenen Dateien (presence_<name>.cpp) und implementieren dort ihre Presence::spawnX/thinkX.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "gameplay/presence.hpp"
#include "world/level_def.hpp"
#include "world/room.hpp"
#include "world/sector.hpp"
#include "world/world.hpp"

namespace lim::game {

using anim::Entity;
using anim::Figure;

namespace pres {
inline constexpr float kPiF = 3.14159265f;
inline float wrapA(float a) { return std::remainder(a, 2.0f * kPiF); }
inline float hdist(vec3 a, vec3 b) { return std::hypot(a.x - b.x, a.y - b.y); }
inline vec3 flat(vec3 v) { return {v.x, v.y, 0.0f}; }
inline vec3 nrmF(vec3 v, vec3 fb) {
    const float l = length(v);
    return l > 1e-5f ? v / l : fb;
}
inline vec3 dirYaw(float y) { return {std::cos(y), std::sin(y), 0.0f}; }
inline vec3 rotateYaw(vec3 v, float a) { return {v.x * std::cos(a) - v.y * std::sin(a), v.x * std::sin(a) + v.y * std::cos(a), v.z}; }

// dunkel: kein Raumlicht (Leuchten aus, dunkler Raum oder Stromausfall) - die Taschenlampe zaehlt nicht.
// V11: (x, y) fuer die Welle des Stromausfalls
inline bool darkTile(const Presence::Context& c, const world::Tile* t, float x, float y) {
    if (!t) return false;
    if (t->light < 0.13f) return true;
    return blackoutCovers(c.view, t, x, y);
}
// trifft der Lichtkegel der Taschenlampe den Punkt?
inline bool inBeam(const Viewer& v, world::World& w, vec3 p, float range) {
    if (!v.flashOn) return false;
    vec3 d = p - v.flashPos;
    const float len = length(d);
    // V13.2: verdeckt nur, wenn auch vom Auge aus keine Sicht ist - dicht an der Wand liegt die Lampe in der Wandkachel
    return len < range && len > 0.05f && dot(d / len, v.flashDir) > 0.9f && (clearSight(w, v.flashPos, p) || clearSight(w, v.eye, p));
}
}  // namespace pres
using namespace pres;

// V13.2: Lichtreaktion - im Kegel der Lampe zuckt das Wesen zurueck (light 1), nach 'hold' s gewoehnt es sich (geht auf 0)
template <class A>
void lightReact(A& a, const Presence::Context& c, vec3 at, float range, double dt, double hold, const vec3* at2 = nullptr) {
    // V13.2: at2 - zweiter Punkt (Brust), falls eine Kante den Strahl zum Kopf verdeckt
    const bool lit = inBeam(c.view, *c.world, at, range) || (at2 && inBeam(c.view, *c.world, *at2, range));
    a.lightT = lit ? a.lightT + dt : 0.0;
    a.light = lit ? (float)std::clamp(1.0 - (a.lightT - hold) / 1.2, 0.0, 1.0) : 0.0f;
    a.lightFrom = c.view.flashPos;
}

struct Presence::Agent {
    Role role = Role::Smiler;
    bool human = false;
    Entity ent = Entity::Smiler;
    anim::CreatureAnimator cre;
    anim::Animator anim;
    FigureRig rig;
    int material = -1;
    vec3 pos, vel;
    float yaw = 0.0f;
    double age = 0.0, unseenT = 0.0, seenT = 0.0, stateT = 0.0, timer = 0.0, timer2 = 0.0, lostT = 0.0, stareT = 0.0, cool = 0.0;
    int state = 0, stage = 0;
    bool showcase = false, walkShow = false, sighted = false, frozen = false, noShadow = false, wasSeen = false, contact = false;
    bool stared = false;
    Sight sight;
    NavField field;
    vec3 goal;
    bool hasGoal = false;
    double speed = 1.0;
    float glow = 0.0f;
    // Eingaben der Animation
    bool look = false;
    vec3 lookAt;
    float tilt = 0.0f, lean = 0.0f, reach = 0.0f, jaw = 0.0f, alert = 0.0f, crouch = 0.0f, growl = 0.0f, fly = 0.0f, shiver = 0.0f;
    float hang = 0.0f, bait = 0.0f;  // V11 Buendel: verborgen in der Decke, Koederarm
    float light = 0.0f, perceive = 0.0f;  // V13.2: im Lichtkegel, nimmt etwas wahr (Richtung perceiveAt)
    float wave = 0.0f, talk = 0.0f, plea = 0.0f, strike = 0.0f;  // V13.2: Gesten (Zweibeiner, Fensterwesen)
    bool strikeLoop = false;
    // V13.2: neue Wesen - Art je Ebene, Hunger (Skin-Stealer), zweites Stadium (Wretch), allein (Ziel ohne Mitspieler
    // in der Naehe), Reizung, Wiedererkennen, Sprechen, Laute, Sprintstoss, Gruppe
    int mode = 0, group = -1;
    bool hungry = false, pleads = false, alone = true;
    double provoke = 0.0, recog = 0.0, speakT = 0.0, talkT = 0.0, growlT = 0.0, burst = 0.0;
    double lightT = 0.0;                  // V13.2: wie lange schon im Lichtkegel
    vec3 perceiveAt, lightFrom;
    vec3 up{0, 0, 1}, fwd{};
    // Deathmoth: Ruheplatz an Wand oder Decke, Kreisbahn um das Licht
    vec3 rest, restN{0, 0, 1}, restF{1, 0, 0};
    float phase = 0.0f, radius = 0.6f, bob = 0.0f, spin = 1.0f;
    // Hound: was er gehoert hat
    vec3 heard;
    // V11 Buendel: Lager (Deckenschacht), Schlaege beim Lauschen
    vec3 home;
    int knocks = 0;
    // V11: toedlicher Zugriff - Richtung vom Auge zur Kreatur, Kopf relativ zum Koerper (vorn, links, oben)
    vec3 killDir{1, 0, 0}, killHead{0, 0, 1.7f};
    float stareLimit = 7.0f;
    // V13.2 Koop: Kennung im Netz, Ziel (Spieler), Puppe beim Gast (Proben des Hosts zum Nachfuehren)
    u16 netId = 0;
    int target = 0;
    int variant = 0;  // V13.2: Modellvariante (anim::entityVariants)
    bool puppet = false, netLit = false;
    struct Sample {
        double t = 0.0;
        vec3 p;
        float yaw = 0.0f;
    };
    std::array<Sample, 8> samples{};
    int nSamples = 0;
    std::shared_ptr<void> ext;  // V13.3: eigener Zustand je Wesen (in dessen Datei definiert, z. B. presence_growler.cpp)

    void initBody(vec3 p, float y, unsigned seed) {
        pos = p, yaw = y;
        if (human) {
            anim.init(Figure::Player, p, y);
            rig.init(Figure::Player);
            rig.setPose(anim.pose());
        } else {
            cre.init(ent, p, y, seed);
            const auto& vars = anim::entityVariants(ent);
            rig.initEntity(ent, variant > 0 && variant < (int)vars.size() ? vars[(size_t)variant] : "");
            rig.setCreaturePose(cre.pose());
        }
    }
    void snapBody(vec3 p, float y) {
        pos = p, yaw = y;
        if (human) {
            anim.snap(p, y);
            rig.setPose(anim.pose());
        } else {
            cre.snap(p, y);
            rig.setCreaturePose(cre.pose());
        }
    }
    float height() const {
        if (ent == Entity::Nguithr && !human) return variant == 1 || variant == 2 ? 0.8f : 0.14f * cre.scale();  // V13.3: Netzkugeln / Spinne
        return human ? anim::figureBuild(Figure::Player).height : cre.height();
    }
    // Fuesse fuer die Wahrnehmung (die Motte fliegt: Mitte des Koerpers)
    vec3 feet() const {
        if (ent == Entity::Nguithr && !human) return pos - vec3{0, 0, variant == 1 || variant == 2 ? 0.85f : (up.z < -0.5f ? 0.16f * cre.scale() : 0.0f)};  // V13.3
        return ent == Entity::Moth && !human ? pos - vec3{0, 0, 0.06f} : pos;
    }
};

}  // namespace lim::game
