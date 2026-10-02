// V6: Tests fuer neue Spielsysteme ohne Grafik.
//
//   gameplay_test <data-Ordner>
//
// Prueft Sprung (Hoehe, Landung, Kopffreiheit, Nachsicht an Kanten), Treppen
// (hinab ohne Mikro-Fall/Landegeraeusch), die Erkundungskarte (nur Gesehenes,
// Sichtlinie endet an Waenden, Speichern/Laden), die Koerperanimation
// (Gliederlaengen, Augenhoehe, Sprung- und Landepose, Gesten), die Ausdauer
// und das Ablegen von Items (Wurf, Liegen auf dem Boden, Speichern, Aufheben).
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>

#include "core/jobs.hpp"
#include "gameplay/body.hpp"
#include "gameplay/content.hpp"
#include "gameplay/explore.hpp"
#include "gameplay/player.hpp"
#include "gameplay/prefabs.hpp"
#include "gameplay/session.hpp"
#include "gameplay/stats.hpp"
#include "gameplay/visible.hpp"
#include "world/world.hpp"

using namespace lim;

static int gFails = 0, gChecks = 0;
static void check(bool ok, const std::string& what) {
    ++gChecks;
    if (!ok) {
        std::printf("  FEHLER: %s\n", what.c_str());
        ++gFails;
    }
}

// Kachelwelt aus einer Funktion (Boden, Decke; massiv wenn ceil <= floor)
struct FnWorld : phys::TileSource {
    std::function<void(i64, i64, float&, float&)> fn;
    world::Tile t;
    const world::Tile& at(i64 x, i64 y) override {
        float f = 0, c = 3;
        fn(x, y, f, c);
        t = world::Tile{};
        if (c > f) {
            t.flags = 0;
            t.floor = f;
            t.ceil = c;
        }
        return t;
    }
};

static void stepN(game::Player& p, phys::TileSource& w, int n, game::PlayerControl c = {}, int* lands = nullptr,
                  int* jumps = nullptr, double* maxZ = nullptr) {
    for (int i = 0; i < n; ++i) {
        p.update(1.0 / 120.0, c, w);
        c.jump = false;
        if (lands && p.landed) ++*lands;
        if (jumps && p.jumped) ++*jumps;
        if (maxZ) *maxZ = std::max(*maxZ, p.z);
    }
}

static void testJump() {
    std::printf("Sprung & Treppen\n");
    FnWorld flat;
    flat.fn = [](i64, i64, float& f, float& c) { f = 0, c = 3.0f; };
    game::Player p(0.5, 0.5, 0.0, 0.0);
    stepN(p, flat, 30);
    game::PlayerControl c;
    c.jump = true;
    int lands = 0, jumps = 0;
    double maxZ = 0;
    stepN(p, flat, 240, c, &lands, &jumps, &maxZ);
    check(jumps == 1, "genau ein Absprung");
    check(maxZ > 0.5 && maxZ < 0.75, "Sprunghoehe 0,5..0,75 m (ist " + std::to_string(maxZ) + ")");
    check(lands == 1 && p.onGround && std::fabs(p.z) < 1e-9, "Landung auf dem Boden");

    // niedrige Decke (2,0 m): Sprung nur bis zur Decke, kein Festklemmen
    FnWorld low;
    low.fn = [](i64, i64, float& f, float& c) { f = 0, c = 2.0f; };
    game::Player q(0.5, 0.5, 0.0, 0.0);
    stepN(q, low, 10);
    maxZ = 0;
    c.jump = true;
    stepN(q, low, 200, c, nullptr, nullptr, &maxZ);
    check(maxZ <= 2.0 - q.shape.body + 1e-6, "Kopf bleibt unter der Decke");
    check(q.onGround, "nach Sprung unter niedriger Decke wieder am Boden");
    game::PlayerControl walk;
    walk.forward = 1;
    double x0 = q.x;
    stepN(q, low, 120, walk);
    check(q.x > x0 + 1.0, "unter niedriger Decke weiter begehbar");
    // zu niedrig (1,85 m): kein Sprung
    FnWorld lower;
    lower.fn = [](i64, i64, float& f, float& c) { f = 0, c = 1.85f; };
    game::Player r(0.5, 0.5, 0.0, 0.0);
    jumps = 0;
    c.jump = true;
    stepN(r, lower, 60, c, nullptr, &jumps);
    check(jumps == 0, "kein Sprung ohne Kopffreiheit");

    // Treppe hinab (0,3-m-Stufen): kein Fallen, keine Landegeraeusche, Kamera glatt
    FnWorld stairs;
    stairs.fn = [](i64 x, i64, float& f, float& c) {
        f = -0.3f * (float)std::clamp<i64>(x, 0, 8);
        c = f + 3.0f;
    };
    game::Player s(0.5, 0.5, 0.0, 0.0);
    lands = 0;
    double maxEyeJump = 0, lastEye = s.eye();
    for (int i = 0; i < 400; ++i) {
        s.update(1.0 / 120.0, walk, stairs);
        if (s.landed) ++lands;
        maxEyeJump = std::max(maxEyeJump, std::fabs(s.eye() - lastEye));
        lastEye = s.eye();
    }
    check(s.x > 8.0 && std::fabs(s.z + 2.4) < 1e-6, "Treppe hinab bis unten");
    check(lands == 0, "keine Landungen auf der Treppe");
    check(maxEyeJump < 0.06, "Kamera folgt Stufen weich (max. Sprung " + std::to_string(maxEyeJump) + " m je Bild)");

    // Kante (1 m tief): Fall mit Landung; Sprung kurz nach der Kante noch moeglich (Nachsicht)
    FnWorld ledge;
    ledge.fn = [](i64 x, i64, float& f, float& c) { f = x >= 2 ? -1.0f : 0.0f, c = 4.0f; };
    game::Player t(0.5, 0.5, 0.0, 0.0);
    lands = 0;
    stepN(t, ledge, 200, walk, &lands);
    check(lands == 1 && t.z < -0.99, "Fall von der Kante mit einer Landung");
    game::Player u(1.5, 0.5, 0.0, 0.0);
    int air = 0;
    for (int i = 0; i < 200 && air == 0; ++i) {
        u.update(1.0 / 120.0, walk, ledge);
        if (!u.onGround) air = i;
    }
    stepN(u, ledge, 6, walk);  // 50 ms nach dem Verlassen der Kante
    jumps = 0;
    game::PlayerControl wj = walk;
    wj.jump = true;
    stepN(u, ledge, 1, wj, nullptr, &jumps);
    check(jumps == 1, "Sprung kurz nach der Kante (Nachsicht)");
}

static void testExplore(const game::Content& content) {
    std::printf("Erkundungskarte\n");
    JobSystem jobs(2);
    auto lvl = content.level("level0");
    world::World w(lvl, 1234, 0.28, 0.8, jobs, 3);
    auto [sx, sy, sz, sa] = w.findSpawn();
    w.preload(sx, sy);
    game::ExploreMap ex;
    check(ex.knownTiles() == 0 && !ex.known((i64)sx, (i64)sy), "Karte beginnt leer");
    ex.reveal(w, sx, sy, sz, 0.0, true);
    size_t n1 = ex.knownTiles();
    check(n1 > 20, "Umgebung aufgedeckt (" + std::to_string(n1) + " Kacheln)");
    check(ex.known(ifloor(sx), ifloor(sy)), "eigene Kachel bekannt");
    // nichts ausserhalb der Sichtweite
    bool far = false;
    for (i64 y = ifloor(sy) - 30; y <= ifloor(sy) + 30; ++y)
        for (i64 x = ifloor(sx) - 30; x <= ifloor(sx) + 30; ++x)
            if (ex.known(x, y) && std::hypot(x + 0.5 - sx, y + 0.5 - sy) > game::ExploreMap::kRadius + 1.5) far = true;
    check(!far, "keine Kachel ausserhalb der Sichtweite aufgedeckt");
    // Sichtlinie: keine bekannte offene Kachel liegt vollstaendig hinter einer Wand
    // (alle Linien vom Spieler zu Mitte und Ecken der Kachel kreuzen massive Kacheln)
    int hidden = 0;
    auto lineBlocked = [&](double tx, double ty) {
        double d = std::hypot(tx - sx, ty - sy);
        int steps = std::max(1, (int)(d / 0.02));
        for (int k = 1; k < steps; ++k) {
            i64 cx = ifloor(sx + (tx - sx) * k / steps), cy = ifloor(sy + (ty - sy) * k / steps);
            if (cx == ifloor(tx) && cy == ifloor(ty)) break;
            const world::Tile* t = w.tileIfLoaded(cx, cy);
            if (t && t->solid()) return true;
        }
        return false;
    };
    for (i64 y = ifloor(sy) - 8; y <= ifloor(sy) + 8; ++y)
        for (i64 x = ifloor(sx) - 8; x <= ifloor(sx) + 8; ++x) {
            game::MapCell c = ex.at(x, y);
            if (!c.known() || c.kind == game::MapKind::Wall) continue;
            bool all = true;
            const double pts[5][2] = {{0.5, 0.5}, {0.02, 0.02}, {0.98, 0.02}, {0.02, 0.98}, {0.98, 0.98}};
            for (const auto& q : pts) all = all && lineBlocked(x + q[0], y + q[1]);
            if (all) ++hidden;
        }
    check(hidden == 0, "keine Kachel vollstaendig hinter Waenden aufgedeckt (" + std::to_string(hidden) + ")");
    // Gehen deckt mehr auf, Bekanntes bleibt
    double px = sx, py = sy;
    for (int i = 0; i < 80; ++i) {
        const world::Tile* t = w.tileIfLoaded(ifloor(px + std::cos(sa) * 0.25), ifloor(py + std::sin(sa) * 0.25));
        if (!t || t->solid()) break;
        px += std::cos(sa) * 0.25, py += std::sin(sa) * 0.25;
        w.update(px, py);
        ex.reveal(w, px, py, t->floor, 0.05);
    }
    size_t n2 = ex.knownTiles();
    check(n2 > n1, "Gehen deckt weitere Kacheln auf");
    check(ex.known(ifloor(sx), ifloor(sy)), "Startkachel bleibt bekannt");
    // Speichern und Laden
    Json j = ex.toJson();
    game::ExploreMap ey;
    ey.fromJson(j);
    check(ey.knownTiles() == n2, "gleiche Anzahl nach Laden");
    check(ey.at(ifloor(sx), ifloor(sy)).kind == game::MapKind::Pending, "Aussehen folgt nach dem Laden");
    for (const auto& [k, cd] : w.chunks()) ey.refreshChunk(*cd, w.bank());
    int diff = 0;
    for (i64 y = ifloor(sy) - 20; y <= ifloor(sy) + 20; ++y)
        for (i64 x = ifloor(sx) - 20; x <= ifloor(sx) + 20; ++x) {
            auto a = ex.at(x, y), b = ey.at(x, y);
            if (a.kind != b.kind || a.light != b.light || a.color != b.color) ++diff;
        }
    check(diff == 0, "Karte nach Laden identisch (" + std::to_string(diff) + " Abweichungen)");
}

static float dist(vec3 a, vec3 b) { return length(a - b); }

static void testBody() {
    std::printf("Koerper\n");
    FnWorld flat;
    flat.fn = [](i64, i64, float& f, float& c) { f = 0, c = 4.0f; };
    game::Player p(0.5, 0.5, 0.0, 0.0);
    game::Body b;
    auto run = [&](int n, game::PlayerControl c) {
        for (int i = 0; i < n; ++i) {
            p.update(1.0 / 60.0, c, flat);
            b.update(1.0 / 60.0, p);
            c.jump = false;
        }
    };
    run(60, {});
    const auto& s = b.pose();
    check(std::fabs(b.eye().z - (float)game::Player::kEye) < 0.04f, "Augenhoehe im Stand ~1,62 m");
    for (int i = 0; i < 2; ++i) {
        check(std::fabs(dist(s.hip[i], s.knee[i]) - 0.47f) < 1e-3f && std::fabs(dist(s.knee[i], s.ankle[i]) - 0.45f) < 1e-3f,
              "Beinlaengen bleiben erhalten");
        check(std::fabs(dist(s.shoulder[i], s.elbow[i]) - 0.30f) < 1e-3f, "Oberarmlaenge bleibt erhalten");
        check(s.ankle[i].z < 0.1f, "Fuesse im Stand am Boden");
    }
    // Blick ganz nach unten: Auge wandert nach vorn; Schultern bleiben hinter der Kamera,
    // Becken und Fuesse liegen im Sichtfeld (senkrecht ~62 Grad)
    vec3 eyeFlat = b.eye();
    p.pitch = -game::Player::kPitchMax;
    b.update(1.0 / 60.0, p);
    check(dot(b.eye() - eyeFlat, s.fwd) > 0.05f, "Auge wandert beim Blick nach unten nach vorn");
    {
        float pc = (float)-game::Player::kPitchMax;
        vec3 look = normalize(s.fwd * std::cos(pc) + vec3(0, 0, std::sin(pc)));
        auto inView = [&](vec3 q) { return dot(normalize(q - b.eye()), look) > std::cos(0.54f); };
        check(inView(s.pelvis) && inView(s.ankle[0]) && inView(s.ankle[1]), "Blick nach unten: Becken und Fuesse sichtbar");
        check(!inView(s.shoulder[0]) && !inView(s.shoulder[1]), "Blick nach unten: Schultern hinter der Kamera");
    }
    p.pitch = 0;
    // Gehen: Fuesse wechseln sich ab, einer hebt ab
    game::PlayerControl walk;
    walk.forward = 1;
    float maxLift = 0, minFootDiff = 1e9f, maxFootDiff = -1e9f;
    for (int i = 0; i < 120; ++i) {
        run(1, walk);
        maxLift = std::max({maxLift, s.ankle[0].z, s.ankle[1].z});
        float d = dot(s.ankle[0] - s.ankle[1], s.fwd);
        minFootDiff = std::min(minFootDiff, d), maxFootDiff = std::max(maxFootDiff, d);
    }
    check(maxLift > 0.1f, "Fuss hebt beim Gehen ab");
    check(minFootDiff < -0.3f && maxFootDiff > 0.3f, "Beine schwingen gegengleich");
    // Sprinten: Ellbogen staerker gebeugt
    game::PlayerControl spr = walk;
    spr.sprint = true;
    run(90, spr);
    vec3 ua = normalize(s.elbow[1] - s.shoulder[1]), fa = normalize(s.hand[1] - s.elbow[1]);
    check(p.sprinting && dot(ua, fa) < 0.6f, "Sprint: angewinkelte Arme");
    // Sprung: Haende ueber die Schultern
    run(30, {});
    game::PlayerControl j;
    j.jump = true;
    float maxHand = -1;
    for (int i = 0; i < 20; ++i) {
        run(1, j);
        j.jump = false;
        maxHand = std::max(maxHand, s.hand[0].z - s.shoulder[0].z);
    }
    check(maxHand > 0.1f, "Sprung: Arme fliegen hoch");
    // Landung: Kamera taucht ein
    float minEye = 9;
    for (int i = 0; i < 90; ++i) {
        run(1, {});
        if (p.onGround) minEye = std::min(minEye, b.eye().z);
    }
    check(minEye < (float)game::Player::kEye - 0.03f, "Landung: Knie federn, Kamera taucht ein");
    // Gesten und Netze
    std::vector<game::VisibleMesh> fp, tp;
    b.gesture(game::Body::Gesture::Use, {}, "almond", 0.11f);
    run(20, {});
    b.collect(fp, true);
    b.collect(tp, false);
    bool held = false;
    for (auto& v : fp) held |= *v.model == "almond";
    check(held, "Benutzen: Item in der Hand sichtbar");
    check(tp.size() > fp.size(), "Schulterkamera zeigt zusaetzlich den Kopf");
    bool attached = true;
    for (auto& v : fp) attached &= v.attached;
    check(attached, "Koerper-Netze sind als mitbewegt markiert");
    float handToEye = dist(s.hand[1], b.eye());
    check(handToEye < 0.6f && dot(s.hand[1] - b.eye(), s.fwd) > 0.25f, "Benutzen: Hand vor dem Gesicht");
    run(80, {});
    check(!b.gestureActive(), "Geste endet");
}

static void testStamina(const game::Content& content) {
    std::printf("Ausdauer\n");
    game::Stats st(content.difficulty("medium"));
    double t = 0;
    while (st.canSprint() && t < 30) st.updateStamina(0.05, true, true), t += 0.05;
    check(t > 6.0 && t < 8.0, "Dauersprint ~7 s auf Medium (ist " + std::to_string(t) + " s)");
    check(st.exhausted && !st.canSprint(), "danach erschoepft, Sprint gesperrt");
    double r = 0;
    while (!st.canSprint() && r < 30) st.updateStamina(0.05, false, false), r += 0.05;
    check(r > 1.5 && r < 4.0, "Sprint nach kurzer Erholung wieder moeglich (" + std::to_string(r) + " s)");
    check(st.stamina >= game::Stats::kRecoverAt - 0.5, "erst ab der Marke");
    while (st.stamina < 99.9 && r < 60) st.updateStamina(0.05, false, false), r += 0.05;
    check(r < 9.0, "im Stehen nach wenigen Sekunden wieder voll");
    // Energy Bar fuellt auf und halbiert den Verbrauch
    st.stamina = 5, st.exhausted = true;
    st.consume(*content.item("energy"));
    check(st.stamina > 99.0 && st.canSprint(), "Energy Bar fuellt Ausdauer auf");
    double t2 = 0;
    while (st.canSprint() && t2 < 60) st.updateStamina(0.05, true, true), t2 += 0.05;
    check(t2 > 12.0, "mit Energy Bar laenger sprinten (" + std::to_string(t2) + " s)");
    // Speichern
    game::Stats s2(content.difficulty("medium"));
    st.stamina = 42;
    s2.fromJson(st.toJson());
    check(std::fabs(s2.stamina - 42) < 1e-6, "Ausdauer im Spielstand");
}

static int countDropped(game::GameSession& s, bool restingOnly, vec3* pos = nullptr) {
    int n = 0;
    s.registry().each<game::Dropped, game::Transform>([&](ecs::Entity, game::Dropped& d, game::Transform& t) {
        if (restingOnly && !d.resting) return;
        ++n;
        if (pos) *pos = t.pos;
    });
    return n;
}

static void testDrop(const game::Content& content, const std::string& dataDir) {
    std::printf("Items ablegen\n");
    game::PrefabLibrary prefabs;
    prefabs.loadDirectory(dataDir + "/prefabs");
    JobSystem jobs(2);
    auto make = [&] {
        return std::make_unique<game::GameSession>(content, prefabs, content.level("level0"), 1234, "medium", "Test", jobs, 3);
    };
    auto s = make();
    s->placeAtSpawn();
    s->preload();
    s->inventory().add("almond");
    s->inventory().add("almond");
    s->inventory().add("almond");
    check(s->dropSlot(0, false), "Q legt ab");
    check(s->inventory().count("almond") == 2 && s->droppedCount() == 1, "ein Item weniger im Inventar");
    for (int i = 0; i < 180; ++i) s->update(1.0 / 60.0, {}, false), s->events().clear();
    vec3 pos;
    check(countDropped(*s, true, &pos) == 1, "Item liegt nach dem Wurf ruhig");
    const world::Tile* t = s->world().tileIfLoaded(ifloor(pos.x), ifloor(pos.y));
    check(t && t->open() && std::fabs(pos.z - t->floor) < 0.02f, "Item liegt auf dem Boden (nicht in der Wand)");
    double d = std::hypot(pos.x - s->player().x, pos.y - s->player().y);
    check(d > 0.3 && d < 2.0, "vor dem Spieler gelandet (" + std::to_string(d) + " m)");
    // Strg+Q: ganzer Stapel
    check(s->dropSlot(0, true) && s->inventory().count("almond") == 0 && s->droppedCount() == 3, "Strg+Q legt den Stapel ab");
    for (int i = 0; i < 180; ++i) s->update(1.0 / 60.0, {}, false), s->events().clear();
    check(countDropped(*s, true) == 3, "alle drei liegen");
    // Speichern und Laden
    Json j = s->toJson();
    auto s2 = make();
    s2->fromJson(j);
    s2->preload();
    vec3 p2;
    check(countDropped(*s2, true, &p2) == 3 && s2->droppedCount() == 3, "abgelegte Items nach dem Laden wieder da");
    // wieder aufheben: zum Item drehen und E
    auto& pl = s2->player();
    pl.angle = std::atan2(p2.y - pl.y, p2.x - pl.x);
    s2->update(1.0 / 60.0, {}, false);
    s2->interact();
    check(s2->inventory().count("almond") == 1 && s2->droppedCount() == 2, "abgelegtes Item wieder aufgehoben");
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("Aufruf: gameplay_test <data-Ordner>\n");
        return 2;
    }
    game::Content content;
    std::string err;
    if (!content.load(argv[1], err)) {
        std::printf("Inhalte nicht ladbar: %s\n", err.c_str());
        return 2;
    }
    testJump();
    testExplore(content);
    testBody();
    testStamina(content);
    testDrop(content, argv[1]);
    std::printf("%s: %d Pruefungen, %d Fehler\n", gFails ? "FEHLGESCHLAGEN" : "OK", gChecks, gFails);
    return gFails ? 1 : 0;
}
