#include "gameplay/stalker.hpp"

#include <algorithm>
#include <cmath>
#include <queue>

#include "core/log.hpp"
#include "gameplay/visible.hpp"
#include "world/room.hpp"
#include "world/world.hpp"

namespace lim::game {

namespace {
constexpr double kViewCos = 0.64;     // ~50 Grad um die Blickrichtung zaehlen als "im Bild"
constexpr double kFlashCos = 0.895;   // ~26 Grad: Kegel der Taschenlampe
constexpr double kFlashRange = 22.0;
constexpr float kLitLevel = 0.2f;     // Helligkeit der Kachel (V4-Licht), ab der man ihn erkennt

bool walkable(const world::Tile* t) {
    return t && t->open() && !(t->flags & world::TF_MACHINE) && t->clearance() >= 1.9f;
}

// freie Sicht ueber geladene Kacheln, ohne Chunks nachzuladen
bool sightLine(world::World& w, vec3 a, vec3 b) {
    vec3 d = b - a;
    float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    if (len < 0.5f) return true;
    for (float t = 0.25f; t < len - 0.3f; t += 0.2f) {
        vec3 q = a + d * (t / len);
        const world::Tile* tl = w.tileIfLoaded(ifloor(q.x), ifloor(q.y));
        if (!tl || tl->solid() || q.z < tl->floor + 0.05f || q.z > tl->ceil - 0.02f) return false;
    }
    return true;
}
}  // namespace

float Stalker::proximity() const {
    if (state_ != Hunt) return 0.0f;
    return (float)std::clamp(1.0 - (dist_ - 1.0) / 24.0, 0.0, 1.0);
}

void Stalker::vanish(double delay) {
    state_ = Idle;
    seen_ = false;
    next_ = std::max(next_, delay);
    dist_ = 1e9;
}

int Stalker::fieldAt(i64 x, i64 y) const {
    const i64 lx = x - fx_ + kR, ly = y - fy_ + kR;
    if (lx < 0 || ly < 0 || lx >= kN || ly >= kN || field_.empty()) return -1;
    return field_[(size_t)(ly * kN + lx)];
}

// Dijkstra von der Spielerkachel aus; Stufen bis 0,65 m, schraeg nur an freien Ecken vorbei
void Stalker::buildField(world::World& w, i64 cx, i64 cy) {
    fx_ = cx, fy_ = cy;
    field_.assign((size_t)kN * kN, -1);
    const world::Tile* start = w.tileIfLoaded(cx, cy);
    if (!walkable(start)) return;
    using Node = std::pair<int, int>;  // Kosten, Index
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> q;
    field_[(size_t)(kR * kN + kR)] = 0;
    q.push({0, kR * kN + kR});
    static const int dx[8] = {1, -1, 0, 0, 1, 1, -1, -1}, dy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    while (!q.empty()) {
        auto [c, idx] = q.top();
        q.pop();
        if (c != field_[(size_t)idx]) continue;
        const int lx = idx % kN, ly = idx / kN;
        const world::Tile* t = w.tileIfLoaded(fx_ - kR + lx, fy_ - kR + ly);
        if (!t) continue;
        for (int k = 0; k < 8; ++k) {
            const int nx = lx + dx[k], ny = ly + dy[k];
            if (nx < 0 || ny < 0 || nx >= kN || ny >= kN) continue;
            const world::Tile* n = w.tileIfLoaded(fx_ - kR + nx, fy_ - kR + ny);
            if (!walkable(n) || std::fabs(n->floor - t->floor) > 0.65f) continue;
            if (k >= 4) {  // schraeg: beide Nachbarn muessen frei sein
                if (!walkable(w.tileIfLoaded(fx_ - kR + nx, fy_ - kR + ly)) ||
                    !walkable(w.tileIfLoaded(fx_ - kR + lx, fy_ - kR + ny)))
                    continue;
            }
            const int nc = c + (k < 4 ? 10 : 14);
            int& f = field_[(size_t)(ny * kN + nx)];
            if (f < 0 || nc < f) {
                f = nc;
                q.push({nc, ny * kN + nx});
            }
        }
    }
}

// Liegt das Ziel im Licht (Leuchte ueber ihm, nicht im Stromausfall) oder im Lichtkegel?
bool Stalker::litAt(const View& v, world::World& w, vec3 target) const {
    if (v.flashOn) {
        vec3 d = target - v.flashPos;
        float len = std::sqrt(dot(d, d));
        if (len < kFlashRange && len > 0.01f && dot(d / len, v.flashDir) > kFlashCos) return true;
    }
    const world::Tile* t = w.tileIfLoaded(ifloor(x_), ifloor(y_));
    if (!t) return false;
    if (t->light < kLitLevel) return false;
    if (v.blackoutSeed >= 0 && v.blackoutAmount > 0.5f && t->room && (int)(t->room->seed & 31u) == v.blackoutSeed) return false;
    return true;
}

bool Stalker::seenBy(const View& v, world::World& w) const {
    for (float h : {1.75f, 1.15f, 0.5f}) {  // Kopf, Brust, Knie
        vec3 target{(float)x_, (float)y_, (float)z_ + h};
        vec3 d = target - v.eye;
        float len = std::sqrt(dot(d, d));
        if (len < 0.01f) return true;
        if (dot(d / len, v.look) < kViewCos) continue;
        if (!sightLine(w, v.eye, target)) continue;
        if (litAt(v, w, target)) return true;
    }
    return false;
}

// Erscheinen: ungesehen, auf dem Weg lo..hi (Kosten) entfernt, bevorzugt hinter dem Spieler
bool Stalker::spawn(const Player& p, const View& v, world::World& w, int lo, int hi) {
    struct Cand {
        i64 x, y;
        bool behind;
    };
    std::vector<Cand> cands;
    const vec3 flat = normalize(vec3{v.look.x, v.look.y, 0.0f});
    for (int ly = 0; ly < kN; ++ly)
        for (int lx = 0; lx < kN; ++lx) {
            const int f = field_[(size_t)(ly * kN + lx)];
            if (f < lo || f > hi) continue;
            const i64 x = fx_ - kR + lx, y = fy_ - kR + ly;
            const world::Tile* t = w.tileIfLoaded(x, y);
            if (!t || t->clearance() < 2.25f || (t->flags & (world::TF_STAIR | world::TF_DOOR))) continue;
            vec3 d{(float)x + 0.5f - (float)p.x, (float)y + 0.5f - (float)p.y, 0.0f};
            cands.push_back({x, y, dot(normalize(d), flat) < 0.1f});
        }
    if (cands.empty()) return false;
    for (int pass = 0; pass < 2; ++pass)
        for (int tries = 0; tries < 48; ++tries) {
            const Cand& c = cands[(size_t)rng_.randint(0, (i64)cands.size() - 1)];
            if (pass == 0 && !c.behind) continue;
            const world::Tile* t = w.tileIfLoaded(c.x, c.y);
            x_ = (double)c.x + 0.5, y_ = (double)c.y + 0.5, z_ = t->floor;
            // nicht sichtbar - auch nicht, wenn gerade Licht auf ihn fiele
            bool visible = false;
            for (float h : {1.75f, 1.0f}) {
                vec3 target{(float)x_, (float)y_, (float)z_ + h};
                vec3 d = target - v.eye;
                float len = std::sqrt(dot(d, d));
                if (dot(d / len, v.look) > 0.3 && sightLine(w, v.eye, target)) visible = true;
            }
            if (visible) continue;
            angle_ = std::atan2(p.y - y_, p.x - x_);
            return true;
        }
    return false;
}

void Stalker::update(double dt, const Player& p, const View& v, world::World& w, double sanity, bool enabled, bool suspended,
                     const Params& prm, std::vector<Event>& out) {
    if (!enabled) {
        if (state_ == Hunt) vanish(60.0);
        return;
    }
    if (suspended) {
        if (state_ == Hunt) {
            out.push_back({Event::Lost, pos()});
            vanish(45.0);
        }
        return;
    }
    const i64 pcx = ifloor(p.x), pcy = ifloor(p.y);
    fieldT_ -= dt;
    const bool fieldStale = pcx != fx_ || pcy != fy_ || fieldT_ <= 0.0;

    if (state_ == Idle) {
        next_ -= dt;
        if (next_ > 0.0 && !forced_) return;
        if (fieldStale) buildField(w, pcx, pcy), fieldT_ = 0.5;
        const int lo = forced_ && forcedDist_ > 0.0 ? (int)(forcedDist_ * 10.0 * 0.8) : 200;
        const int hi = forced_ && forcedDist_ > 0.0 ? (int)(forcedDist_ * 10.0 * 1.25) : 340;
        if (!spawn(p, v, w, lo, hi)) {
            next_ = 6.0;  // kein Platz (enge Gaenge, Rand der Welt): bald noch einmal
            forced_ = false;
            return;
        }
        forced_ = false;
        state_ = Hunt;
        huntT_ = lostT_ = seenT_ = 0.0;
        seen_ = sighted_ = false;
        puppet_ = Player(x_, y_, z_, angle_);
        puppet_.pitch = -0.2;
        body_.reset();
        if (!bodyInit_) body_.setGhost(), bodyInit_ = true;
        body_.update(0.0, puppet_);
        dist_ = std::hypot(x_ - p.x, y_ - p.y);
        log::info("Der Andere erscheint bei ({:.1f}, {:.1f}), {:.1f} m entfernt", x_, y_, dist_);
        out.push_back({Event::Appear, pos()});
        return;
    }

    // --- Jagd ------------------------------------------------------------------------------
    if (fieldStale) buildField(w, pcx, pcy), fieldT_ = 0.4;
    huntT_ += dt;
    dist_ = std::hypot(x_ - p.x, y_ - p.y);
    seen_ = seenBy(v, w);
    if (seen_) {
        seenT_ += dt;
        if (!sighted_) {
            sighted_ = true;
            out.push_back({Event::FirstSight, pos()});
        }
    }
    const int here = fieldAt(ifloor(x_), ifloor(y_));
    // Spur verloren: zu lange unterwegs, zu weit weg oder kein Weg mehr (nur ungesehen)
    lostT_ = here < 0 ? lostT_ + dt : 0.0;
    if (!seen_ && (huntT_ > 150.0 || dist_ > 52.0 || lostT_ > 5.0)) {
        if (lostT_ > 5.0 && huntT_ < 90.0 && spawn(p, v, w, 160, 300)) {
            lostT_ = 0.0;  // abgeschnitten: taucht woanders wieder auf
        } else {
            out.push_back({Event::Lost, pos()});
            vanish(rng_.uniform(120.0, 240.0) * prm.pause * (0.55 + 0.45 * std::clamp(sanity / 100.0, 0.0, 1.0)));
            return;
        }
    }
    if (seen_) return;  // erstarrt mitten im Schritt (der Koerper wird nicht weiterbewegt)

    // Zugriff: ungesehen und in Reichweite
    if (dist_ < 0.95 && std::fabs(z_ - p.z) < 1.2) {
        out.push_back({Event::Attack, pos()});
        log::info("Der Andere hat zugegriffen (Jagd {:.0f} s)", huntT_);
        vanish(rng_.uniform(120.0, 240.0) * prm.pause * (0.55 + 0.45 * std::clamp(sanity / 100.0, 0.0, 1.0)));
        return;
    }

    // Bewegung entlang des Gefaelles (weit weg: holt schneller auf)
    double speed = prm.speed * (here > 300 ? 1.5 : 1.0);
    double remaining = speed * dt, moved = 0.0;
    double tx = p.x, ty = p.y;
    for (int it = 0; it < 4 && remaining > 1e-4; ++it) {
        const i64 cx = ifloor(x_), cy = ifloor(y_);
        const int f = fieldAt(cx, cy);
        if (f < 0) break;
        if (f <= 14) {
            tx = p.x, ty = p.y;  // gleiche oder Nachbarkachel: direkt zu
        } else {
            int best = f;
            i64 bx = cx, by = cy;
            for (int ddy = -1; ddy <= 1; ++ddy)
                for (int ddx = -1; ddx <= 1; ++ddx) {
                    if (!ddx && !ddy) continue;
                    const int g = fieldAt(cx + ddx, cy + ddy);
                    if (g < 0 || g >= best) continue;
                    if (ddx && ddy && (fieldAt(cx + ddx, cy) < 0 || fieldAt(cx, cy + ddy) < 0)) continue;
                    best = g, bx = cx + ddx, by = cy + ddy;
                }
            if (best == f) break;
            tx = (double)bx + 0.5, ty = (double)by + 0.5;
        }
        const double ddx = tx - x_, ddy = ty - y_, len = std::hypot(ddx, ddy);
        if (len < 1e-4) break;
        const double step = std::min(remaining, len);
        x_ += ddx / len * step, y_ += ddy / len * step;
        remaining -= step, moved += step;
    }
    if (const world::Tile* t = w.tileIfLoaded(ifloor(x_), ifloor(y_)); t && t->open())
        z_ += (t->floor - z_) * std::min(1.0, dt * 12.0);
    // Blick: in Laufrichtung, aus der Naehe auf den Spieler
    const double wantA = dist_ < 6.0 ? std::atan2(p.y - y_, p.x - x_) : std::atan2(ty - y_, tx - x_);
    angle_ += std::remainder(wantA - angle_, 2.0 * 3.14159265358979) * std::min(1.0, dt * 8.0);

    // Schein-Spieler fuer die Gangart des Koerpers
    const double spd = dt > 0.0 ? moved / dt : 0.0;
    const double before = puppet_.bobPhase;
    puppet_.vx = std::cos(angle_) * spd, puppet_.vy = std::sin(angle_) * spd;
    puppet_.x = x_, puppet_.y = y_, puppet_.z = z_, puppet_.angle = angle_;
    puppet_.pitch = dist_ < 6.0 ? -0.05 : -0.2;
    puppet_.groundSpeed = spd;
    puppet_.gait += (std::min(1.0, spd / 3.0) - puppet_.gait) * std::min(1.0, dt * 6.0);
    puppet_.bobAmount = puppet_.gait;
    puppet_.bobPhase = std::fmod(puppet_.bobPhase + dt * (5.5 + spd * 1.3), 4096.0 * 3.14159265358979);
    puppet_.onGround = true;
    body_.update(dt, puppet_);
    if (spd > 0.4 && std::floor(before / 3.14159265358979) != std::floor(puppet_.bobPhase / 3.14159265358979))
        out.push_back({Event::Step, pos()});
}

void Stalker::collect(std::vector<VisibleMesh>& out) {
    if (state_ == Hunt && bodyInit_) body_.collect(out, false);
}

}  // namespace lim::game
