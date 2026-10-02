#include "gameplay/haunt.hpp"

#include <algorithm>
#include <cmath>

#include "core/log.hpp"
#include "gameplay/visible.hpp"
#include "world/room.hpp"
#include "world/world.hpp"

namespace lim::game {

namespace {
double intensity(double sanity, double below) { return std::clamp((below - sanity) / below, 0.0, 1.0); }

// freie Sicht ueber geladene Kacheln (Augenhoehe), ohne Chunks nachzuladen
bool sightLine(world::World& w, vec3 a, vec3 b) {
    vec3 d = b - a;
    float len = std::hypot(d.x, d.y);
    for (float t = 0.3f; t < len - 0.3f; t += 0.25f) {
        vec3 q = a + d * (t / len);
        const world::Tile* tl = w.tileIfLoaded(ifloor(q.x), ifloor(q.y));
        if (!tl || tl->solid() || q.z < tl->floor + 0.2f || q.z > tl->ceil - 0.05f) return false;
    }
    return true;
}
}  // namespace

void Haunt::reset() {
    stepsLeft_ = 0;
    echoes_.clear();
    boSeed_ = -1, boAmount_ = 0.0f, boT_ = boLen_ = 0.0;
    ghost_ = false;
    forced_ = 0;
}

// Verlauf eines Ausfalls: zweimal Zucken, dunkel, dann stotternd zurueck
float Haunt::blackoutCurve(double t, double len) const {
    if (len < 0.6) return (t > 0.04 && t < len - 0.05) ? 0.92f : 0.15f;  // kurzes Flackern
    static const double off[] = {0.0, 0.07, 0.15, 0.24, 0.30};
    static const float offA[] = {0.95f, 0.25f, 0.97f, 0.45f, 0.97f};
    static const double on[] = {0.0, 0.10, 0.16, 0.30, 0.36, 0.5};
    static const float onA[] = {0.35f, 0.97f, 0.2f, 0.85f, 0.1f, 0.0f};
    if (t < 0.35) {
        float a = offA[0];
        for (int i = 0; i < 5; ++i)
            if (t >= off[i]) a = offA[i];
        return a;
    }
    double r = t - (len - 0.55);
    if (r < 0.0) return 0.97f;
    float a = onA[0];
    for (int i = 0; i < 6; ++i)
        if (r >= on[i]) a = onA[i];
    return a;
}

bool Haunt::startBlackout(const world::Room* room, double len) {
    if (!room || boSeed_ >= 0) return false;
    int seed = (int)(room->seed & 31u);
    if (seed == 0) return false;  // Seed 0 teilen sich Objekte ohne Raum
    boSeed_ = seed;
    boT_ = 0.0;
    boLen_ = len;
    return true;
}

bool Haunt::spawnGhost(const Player& p, world::World& world, const world::Room* room) {
    if (!room) return false;
    const vec3 eye = p.eyePos();
    // Blickrichtung (waagrecht) und leichte Abweichungen: der am weitesten freie Punkt im selben Raum
    vec3 best;
    float bestLen = 0.0f;
    for (float da : {0.0f, 0.18f, -0.18f, 0.35f, -0.35f}) {
        float a = (float)p.angle + da;
        vec3 dir{std::cos(a), std::sin(a), 0.0f};
        float len = 0.0f;
        for (float t = 1.0f; t < 22.0f; t += 0.5f) {  // weiter weg verschwindet sie im Dunst
            vec3 q = eye + dir * t;
            const world::Tile* tl = world.tileIfLoaded(ifloor(q.x), ifloor(q.y));
            if (!tl || !tl->open() || tl->room != room || tl->ceil - tl->floor < 2.3f || std::fabs(tl->floor - (float)p.z) > 0.4f)
                break;
            len = t;
        }
        if (len > bestLen) bestLen = len, best = eye + dir * (len - 0.6f);
    }
    if (bestLen < 10.0f) {
        log::info("Halluzination: kein Platz fuer die Gestalt (freie Sicht {:.1f} m)", bestLen);
        return false;
    }
    const world::Tile* tl = world.tileIfLoaded(ifloor(best.x), ifloor(best.y));
    if (!tl) return false;
    log::info("Halluzination: Gestalt in {:.1f} m bei ({:.1f}, {:.1f})", bestLen, best.x, best.y);
    ghostPos_ = {best.x, best.y, tl->floor};
    ghost_ = true;
    ghostSeen_ = false;
    ghostT_ = ghostLook_ = ghostAway_ = 0.0;
    ghostSeed_ = (int)(room->seed & 31u);
    ghostInit_ = false;
    return true;
}

void Haunt::update(double dt, const Player& p, world::World& world, const world::Room* room, double sanity, bool stepped,
                   bool enabled, std::vector<Event>& out) {
    if (!enabled && !forced_) {
        if (boSeed_ >= 0 || ghost_ || stepsLeft_) reset();
        return;
    }
    const double iSteps = intensity(sanity, 55.0), iDark = intensity(sanity, 35.0), iGhost = intensity(sanity, 25.0);
    const vec3 eye = p.eyePos();
    const vec3 look{(float)std::cos(p.angle), (float)std::sin(p.angle), 0.0f};
    ghostCooldown_ -= dt;

    // --- Phantomschritte -------------------------------------------------------------------
    if (iSteps > 0.0) nextSteps_ -= dt * (0.3 + iSteps);
    if ((nextSteps_ <= 0.0 && p.gait > 0.4 && stepsLeft_ == 0) || forced_ == 1) {
        stepsLeft_ = (int)rng_.randint(3, 7);
        stepDir_ = vec3{0, 0, 0} - look;  // von hinten
        nextSteps_ = rng_.uniform(30.0, 70.0);
        if (forced_ == 1) forced_ = 0;
    }
    if (stepsLeft_ > 0 && stepped) echoes_.push_back(0.2 + rng_.uniform(0.0, 0.05));
    for (size_t i = 0; i < echoes_.size();) {
        echoes_[i] -= dt;
        if (echoes_[i] > 0.0) {
            ++i;
            continue;
        }
        echoes_.erase(echoes_.begin() + (std::ptrdiff_t)i);
        if (stepsLeft_ <= 0) continue;
        // hinter dem Spieler, so weit die Sicht reicht (4,5 m, sonst naeher)
        vec3 pos = eye + stepDir_ * 1.6f;
        for (float dist : {4.5f, 3.2f, 2.2f}) {
            vec3 q = eye + stepDir_ * dist;
            if (sightLine(world, eye, q)) {
                pos = q;
                break;
            }
        }
        pos.z = (float)p.z;
        out.push_back({Event::Step, pos});
        --stepsLeft_;
    }
    // umgedreht: die Schritte verstummen - da ist nichts
    if (stepsLeft_ > 0 && dot(look, stepDir_) > 0.2f) stepsLeft_ = 0, echoes_.clear();

    // --- Stromausfall ----------------------------------------------------------------------
    if (iDark > 0.0 && boSeed_ < 0 && !ghost_) nextBlackout_ -= dt * (0.35 + iDark);
    const bool litRoom = room && !room->fixtures().empty() && room->lightStyle != "dark" && room->lightStyle != "void";
    if (((nextBlackout_ <= 0.0 && litRoom) || forced_ >= 2) && boSeed_ < 0 && !ghost_) {
        nextBlackout_ = rng_.uniform(45.0, 110.0);
        bool wantGhost = forced_ == 3 || (iGhost > 0.0 && ghostCooldown_ <= 0.0 && rng_.random() < 0.35 + 0.5 * iGhost);
        if (startBlackout(room, rng_.uniform(1.6, 3.6))) {
            boGhost_ = wantGhost;
            boLoud_ = true;
            out.push_back({Event::PowerOff, eye});
        }
        forced_ = 0;
    }
    if (boSeed_ >= 0) {
        boT_ += dt;
        // im Dunkeln: die Gestalt bezieht Stellung
        if (boGhost_ && boT_ > 0.5 && !ghost_) {
            boGhost_ = false;
            if (spawnGhost(p, world, room)) ghostCooldown_ = 70.0;
        }
        boAmount_ = blackoutCurve(boT_, boLen_);
        if (boLoud_ && boLen_ >= 0.6 && boT_ - dt < boLen_ - 0.55 && boT_ >= boLen_ - 0.55) out.push_back({Event::PowerOn, eye});
        if (boT_ >= boLen_) boSeed_ = -1, boAmount_ = 0.0f;
    }

    // --- Der Andere ------------------------------------------------------------------------
    if (ghost_) {
        ghostT_ += dt;
        vec3 to = ghostPos_ + vec3{0, 0, 1.3f} - eye;
        float dist = std::hypot(to.x, to.y);
        vec3 toN = to / std::max(dist, 0.01f);
        bool lit = boSeed_ < 0 || boAmount_ < 0.3f;
        bool inView = dot(vec3{toN.x, toN.y, 0.0f}, look) > 0.93f && sightLine(world, eye, ghostPos_ + vec3{0, 0, 1.3f});
        if (inView && lit) {
            ghostLook_ += dt;
            if (!ghostSeen_) ghostSeen_ = true, out.push_back({Event::GhostSeen, ghostPos_});
        }
        ghostAway_ = dot(vec3{toN.x, toN.y, 0.0f}, look) < 0.5f ? ghostAway_ + dt : 0.0;
        bool vanish = dist < 8.5f || ghostLook_ > 3.0 || ghostT_ > 25.0;
        if (vanish && boSeed_ < 0) {
            // kurzes Flackern in ihrem Raum, und sie ist fort
            boSeed_ = ghostSeed_, boT_ = 0.0, boLen_ = 0.32, boLoud_ = false;
            out.push_back({Event::GhostGone, ghostPos_});
        }
        if (boSeed_ == ghostSeed_ && !boLoud_ && boT_ > 0.12) ghost_ = false;
        if (ghostAway_ > 1.5 && ghostT_ > 8.0) ghost_ = false;  // unbemerkt verschwunden
        if (ghost_) {
            // steht still, wendet sich dir zu und senkt leicht den Kopf
            ghostPlayer_ = Player(ghostPos_.x, ghostPos_.y, ghostPos_.z, std::atan2(eye.y - ghostPos_.y, eye.x - ghostPos_.x));
            ghostPlayer_.pitch = -0.25;
            if (!ghostInit_) ghostBody_.reset(), ghostBody_.setGhost(), ghostInit_ = true;
            ghostBody_.update(dt, ghostPlayer_);
        }
    }
}

void Haunt::collect(std::vector<VisibleMesh>& out) {
    if (ghost_ && ghostInit_) ghostBody_.collect(out, false);
}

}  // namespace lim::game
