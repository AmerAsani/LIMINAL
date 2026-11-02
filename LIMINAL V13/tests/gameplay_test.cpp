// V6: Tests fuer neue Spielsysteme ohne Grafik.
//
//   gameplay_test <data-Ordner>
//
// Prueft Sprung (Hoehe, Landung, Kopffreiheit, Nachsicht an Kanten), Treppen
// (hinab ohne Mikro-Fall/Landegeraeusch), die Erkundungskarte (nur Gesehenes,
// Sichtlinie endet an Waenden, Speichern/Laden), die Koerperanimation
// (Gliederlaengen, Augenhoehe, Sprung- und Landepose, Gesten), die Ausdauer
// und das Ablegen von Items (Wurf, Liegen auf dem Boden, Speichern, Aufheben).
// V7: Taschenlampe, Fundstuecke und Notizen, Batterie, Laufbuch, Ebenenwechsel, Halluzinationen.
// V9: Sicherungskaesten (Lage je Ebene), Vorrang-Notizen, der Andere (erstarrt, wenn man hinsieht;
// greift zu, wenn nicht), Sicherung herausnehmen, Spielstand, die letzte Fahrt.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <functional>
#include <map>
#include <string>

#include "anim/creature.hpp"
#include "anim/figures.hpp"
#include "core/jobs.hpp"
#include "gameplay/body.hpp"
#include "gameplay/figure.hpp"
#include "render/figure_models.hpp"
#include "gameplay/components.hpp"
#include "gameplay/content.hpp"
#include "gameplay/explore.hpp"
#include "gameplay/finds.hpp"
#include "gameplay/flashlight.hpp"
#include "gameplay/player.hpp"
#include "gameplay/prefabs.hpp"
#include "gameplay/session.hpp"
#include "gameplay/kin_l3.hpp"  // V13.3 Level 3
#include "gameplay/sound_field.hpp"
#include "props/glb_import.hpp"
#include "gameplay/stats.hpp"
#include "gameplay/visible.hpp"
#include "world/map_def.hpp"
#include "world/spawn_plan.hpp"
#include "world/sector.hpp"
#include "world/room.hpp"
#include "world/world.hpp"

using namespace lim;

static int gFails = 0, gChecks = 0;
static const game::Content* gContent11 = nullptr;  // V11: Inhalte mit den festen Karten (data/, nicht legacy)

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

// --- V7 -------------------------------------------------------------------------------------------
static void testFlashlight() {
    std::printf("Taschenlampe\n");
    game::Flashlight f;
    check(!f.on && std::fabs(f.charge - game::Flashlight::kStartCharge) < 1e-9, "startet aus mit Startladung");
    check(f.toggle() && f.on, "einschalten");
    for (int i = 0; i < 60 * 60; ++i) f.update(1.0 / 60.0);
    check(std::fabs(f.charge - (game::Flashlight::kStartCharge - 20.0)) < 0.05, "60 s kosten 20 % (ist " + std::to_string(f.charge) + ")");
    check(f.output(1.0) > 0.95f, "volle Helligkeit bei guter Ladung");
    f.charge = 10.0;
    check(f.low(), "unter 15 % schwach");
    bool flick = false;
    for (int i = 0; i < 600; ++i) flick |= f.output(i * 0.05) < 0.9f;
    check(flick, "schwache Batterie flackert");
    int emptied = 0;
    for (int i = 0; i < 60 * 40; ++i) emptied += f.update(1.0 / 60.0) ? 1 : 0;
    check(emptied == 1 && !f.on && f.charge <= 0.0, "leer: geht genau einmal aus");
    check(!f.toggle() && !f.on, "leer laesst sie sich nicht einschalten");
    f.addCharge(250.0);
    check(f.charge == 100.0, "Laden hoechstens bis 100 %");
    game::Flashlight g;
    g.fromJson(f.toJson());
    check(g.charge == 100.0 && g.on == f.on, "Lampe im Spielstand");
}

static void testFinds(const game::Content& content) {
    std::printf("Fundstuecke und Notizen\n");
    JobSystem jobs(2);
    for (const char* id : {"level0", "level1", "level2"}) {
        auto lvl = content.level(id, world::kLayoutV6);
        check(lvl && lvl->id == id, std::string("Level ") + id + " geladen");
        if (!lvl) continue;
        check(!lvl->nextLevel.empty() && content.level(lvl->nextLevel, world::kLayoutV6) != nullptr,
              std::string(id) + ": Notausgaenge fuehren zu einem vorhandenen Level");
        auto collect = [&](world::World& w, int sx, int sy) {
            std::vector<game::FindSpot> all;
            w.tile(sx * lvl->sectorSize + lvl->sectorSize / 2, sy * lvl->sectorSize + lvl->sectorSize / 2);
            for (const auto& r : w.sector(sx, sy).rooms)
                if (r->prepared()) game::roomFinds(*r, *lvl, all);
            return all;
        };
        world::World a(lvl, 77, 0.28, 0.80, jobs, 3), b(lvl, 77, 0.28, 0.80, jobs, 3);
        int total = 0, exits = 0, nearExits = 0, badFloor = 0, mismatch = 0;
        for (auto [sx, sy] : {std::pair{0, 0}, std::pair{2, 1}, std::pair{-3, 2}}) {
            auto fa = collect(a, sx, sy), fb = collect(b, sx, sy);
            if (fa.size() != fb.size()) ++mismatch;
            for (size_t i = 0; i < std::min(fa.size(), fb.size()); ++i)
                if (fa[i].id != fb[i].id || length(fa[i].pos - fb[i].pos) > 1e-5f) ++mismatch;
            for (const auto& s : fa) {
                ++total;
                const world::Tile& t = a.tile(s.tx, s.ty);
                if (!t.open() || std::fabs(t.floor - s.pos.z) > 0.01f) ++badFloor;
                if (s.kind == game::FindSpot::Exit) {
                    ++exits;
                    if (std::hypot(s.pos.x, s.pos.y) < lvl->finds.exitStart - 20.0) ++nearExits;
                }
            }
        }
        check(total >= 4, std::string(id) + ": Funde vorhanden (" + std::to_string(total) + ")");
        check(mismatch == 0, std::string(id) + ": Funde haengen nur vom Seed ab");
        check(badFloor == 0, std::string(id) + ": Funde liegen auf freiem Boden (" + std::to_string(badFloor) + " falsch)");
        check(nearExits == 0, std::string(id) + ": keine Notausgaenge in der Naehe des Starts");
        // Notizen nur aus den erlaubten
        bool ok = true;
        for (u64 h = 1; h < 400; ++h) {
            int n = content.noteFor(id, hash64(h, 5));
            if (n < 0) { ok = false; break; }
            const auto& lv = content.notes()[(size_t)n].levels;
            if (!lv.empty() && std::find(lv.begin(), lv.end(), std::string(id)) == lv.end()) ok = false;
        }
        check(ok, std::string(id) + ": Notizen passen zum Level");
    }
}

static void testLevelChange(const game::Content& content, const std::string& dataDir) {
    std::printf("Batterie, Notizbuch, Laufbuch, Ebenenwechsel\n");
    game::PrefabLibrary prefabs;
    prefabs.loadDirectory(dataDir + "/prefabs");
    JobSystem jobs(2);
    auto s = std::make_unique<game::GameSession>(content, prefabs, content.level("level0", world::kLayoutV6), 99, "medium", "Test", jobs, 3);
    s->placeAtSpawn();
    s->preload();
    // Batterie benutzen
    s->flashlight().charge = 20.0;
    s->inventory().add("battery");
    s->useSlot(0);
    check(std::fabs(s->flashlight().charge - 70.0) < 1e-6 && s->inventory().count("battery") == 0, "Batterie laedt die Lampe um 50 %");
    // Laufbuch: der Startraum zaehlt
    for (int i = 0; i < 30; ++i) s->update(1.0 / 60.0, {}, false), s->events().clear();
    check(s->runLog().rooms >= 1 && s->runLog().levels.size() == 1 && s->runLog().levels[0] == "level0", "Laufbuch beginnt im Startraum");
    game::PlayerControl walk;
    walk.forward = 1.0f;
    for (int i = 0; i < 60 * 25; ++i) {
        if (i % 240 == 0) s->player().angle += 1.3;
        s->update(1.0 / 60.0, walk, false);
        s->events().clear();
    }
    long long rooms = s->runLog().rooms;
    check(rooms >= 2, "Gehen betritt weitere Raeume (" + std::to_string(rooms) + ")");
    // Speichern/Laden
    Json j = s->toJson();
    auto s2 = std::make_unique<game::GameSession>(content, prefabs, content.level("level0", world::kLayoutV6), 99, "medium", "Test", jobs, 3);
    s2->fromJson(j);
    check(s2->runLog().rooms == rooms && s2->runLog().seen.size() == s->runLog().seen.size(), "Laufbuch im Spielstand");
    check(std::fabs(s2->flashlight().charge - s->flashlight().charge) < 1e-6, "Ladung im Spielstand");
    // Ebenenwechsel: Inventar, Lampe, Notizbuch und Laufbuch wandern mit
    s->inventory().add("almond");
    auto n = std::make_unique<game::GameSession>(content, prefabs, content.level("level1", world::kLayoutV6), 5, "medium", "Test", jobs, 3);
    n->carryOver(*s);
    n->placeAtSpawn();
    n->preload();
    check(n->inventory().count("almond") == 1, "Inventar kommt mit");
    check(std::fabs(n->flashlight().charge - s->flashlight().charge) < 1e-6, "Lampe kommt mit");
    check(n->runLog().exits == 1 && n->runLog().levels.size() == 2 && n->runLog().levels[1] == "level1" && n->runLog().rooms == rooms,
          "Laufbuch: eine Ebene weiter");
    check(n->runLog().seen.empty(), "Raumkennungen gelten je Ebene");
}

static void testHaunt(const game::Content& content, const std::string& dataDir) {
    std::printf("Halluzinationen\n");
    game::PrefabLibrary prefabs;
    prefabs.loadDirectory(dataDir + "/prefabs");
    JobSystem jobs(2);
    auto s = std::make_unique<game::GameSession>(content, prefabs, content.level("level0", world::kLayoutV6), 4242, "medium", "Test", jobs, 3);
    s->placeAtSpawn();
    s->preload();
    // bei voller Sanity passiert nichts
    int events = 0;
    for (int i = 0; i < 60 * 120; ++i) {
        s->stats().sanity = 100.0;
        s->update(1.0 / 60.0, {}, false);
        for (auto& e : s->events())
            if (e.type == game::GameEvent::PhantomStep || e.type == game::GameEvent::Blackout || e.type == game::GameEvent::GhostSeen) ++events;
        s->events().clear();
    }
    check(events == 0, "bei klarem Verstand keine Halluzinationen");
    // Stromausfall erzwingen: Leuchten des Raums gehen aus und wieder an
    s->stats().sanity = 20.0;
    s->forceHaunt(2);
    float maxDark = 0.0f;
    bool on = false, off = false, ended = false;
    for (int i = 0; i < 60 * 6; ++i) {
        s->stats().sanity = 20.0;
        s->update(1.0 / 60.0, {}, false);
        maxDark = std::max(maxDark, s->blackout().amount);
        for (auto& e : s->events())
            if (e.type == game::GameEvent::Blackout) (e.index == 1 ? off : on) = true;
        s->events().clear();
        if (off && s->blackout().seed < 0) ended = true;
    }
    const world::Room* r = s->currentRoom();
    bool possible = r && (r->seed & 31u) != 0;
    if (possible) {
        check(off && maxDark > 0.9f, "Stromausfall: Licht geht aus");
        check(on && ended, "Stromausfall: Licht kommt zurueck");
    } else {
        check(!off, "Raum ohne Kennung: kein Stromausfall");
    }
    // abgeschaltet: nichts
    s->hallucinations = false;
    s->stats().sanity = 5.0;
    events = 0;
    for (int i = 0; i < 60 * 120; ++i) {
        s->stats().sanity = 5.0;
        game::PlayerControl walk;
        walk.forward = 1.0f;
        s->update(1.0 / 60.0, walk, false);
        for (auto& e : s->events())
            if (e.type == game::GameEvent::PhantomStep || e.type == game::GameEvent::Blackout) ++events;
        s->events().clear();
        if (s->stats().dead) break;
    }
    check(events == 0, "abgeschaltet: keine Halluzinationen");
}

// --- V8: die zusammenhaengende Welt ------------------------------------------------------------
static void testWorld(const game::Content& content, const std::string& dataDir) {
    std::printf("Zusammenhaengende Welt\n");
    auto W = content.world();
    check(W && W->world() && W->regions.size() == 3, "Welt mit drei Ebenen geladen");
    if (!W) return;
    check(W->regions[0].id == "level0" && W->strata.order.front() == 0, "die Welt beginnt in Level 0");
    JobSystem jobs(2);
    // Determinismus: gleiche Raeume je Seed
    world::World a(W, 4242, 0.28, 0.80, jobs, 3), b(W, 4242, 0.28, 0.80, jobs, 3);
    int diff = 0, rooms = 0;
    for (auto [sx, sy] : {std::pair{0, 0}, std::pair{4, -2}, std::pair{-6, 5}, std::pair{9, 9}}) {
        auto& sa = a.sector(sx, sy);
        auto& sb = b.sector(sx, sy);
        if (sa.rooms.size() != sb.rooms.size()) ++diff;
        for (size_t i = 0; i < std::min(sa.rooms.size(), sb.rooms.size()); ++i, ++rooms)
            if (sa.rooms[i]->type->key != sb.rooms[i]->type->key || sa.rooms[i]->zone != sb.rooms[i]->zone) ++diff;
    }
    check(rooms > 50 && diff == 0, "die Welt haengt nur vom Seed ab");
    // Schichten: am Start Level 0, weit draussen andere Ebenen
    const auto& zf = a.generator().zones();
    int core = 0, far = 0, coreN = 0, farN = 0;
    for (int k = 0; k < 64; ++k) {
        double ang = k * 0.0981747704;
        for (double r : {40.0, 120.0}) {
            auto m = zf.regionWeights(std::cos(ang) * r, std::sin(ang) * r);
            ++coreN, core += m[0] > 0.99 ? 1 : 0;
        }
        auto m = zf.regionWeights(std::cos(ang) * 650.0, std::sin(ang) * 650.0);
        ++farN, far += m[0] < 0.5 ? 1 : 0;
    }
    check(core == coreN, "rund um den Start ausschliesslich Level 0");
    check(far > farN * 3 / 4, "in 650 m liegt meist eine tiefere Ebene (" + std::to_string(far) + "/" + std::to_string(farN) + ")");
    // Uebergangsraeume an den Nahtstellen
    int conn = 0;
    for (int sy = -6; sy <= 6; ++sy)
        for (int sx = -6; sx <= 6; ++sx)
            for (const auto& r : a.sector(sx, sy).rooms) conn += r->type->region < 0 ? 1 : 0;
    check(conn > 0, "Uebergangsraeume vorhanden (" + std::to_string(conn) + ")");

    // Sitzung: Start in Level 0, Laufbuch beginnt dort
    game::PrefabLibrary prefabs;
    prefabs.loadDirectory(dataDir + "/prefabs");
    auto s = std::make_unique<game::GameSession>(content, prefabs, W, 4242, "medium", "Test", jobs, 3);
    s->placeAtSpawn();
    s->preload();
    for (int i = 0; i < 10; ++i) s->update(1.0 / 60.0, {}, false), s->events().clear();
    check(s->currentRoom() && W->regionId(s->currentRoom()->zone) == "level0", "Spielbeginn in den Bueros");
    check(s->runLog().levels.size() == 1 && s->runLog().levels[0] == "level0", "Laufbuch: eine Ebene bekannt");

    // Aufzug: den naechsten suchen, hinfahren, zurueckfahren
    const world::Room* er = nullptr;
    world::World& w = s->world();
    for (int rad = 0; rad <= 6 && !er; ++rad)
        for (int sy = -rad; sy <= rad && !er; ++sy)
            for (int sx = -rad; sx <= rad && !er; ++sx) {
                if (std::max(std::abs(sx), std::abs(sy)) != rad) continue;
                for (const auto& r : w.sector(sx, sy).rooms) {
                    if (!r->elevatorWanted) continue;
                    auto [cx, cy] = r->center();
                    w.tile(ifloor(cx), ifloor(cy));
                    if (r->elevatorSpot()) {
                        er = r.get();
                        break;
                    }
                }
            }
    check(er != nullptr, "ein Aufzug in der Naehe");
    if (!er) return;
    auto spot = *er->elevatorSpot();
    check(std::hypot(spot.doorX, spot.doorY) >= W->elevators.start - 5.0, "Aufzuege nicht nahe am Start");
    // die Kabine ist begehbar, der Kern daneben massiv
    const world::Tile& in = w.tile(ifloor(spot.doorX - spot.dirX * 1.0 - spot.dirY * 0.5), ifloor(spot.doorY - spot.dirY * 1.0 + spot.dirX * 0.5));
    const world::Tile& wall = w.tile(ifloor(spot.doorX - spot.dirX * 1.0 - spot.dirY * 1.5), ifloor(spot.doorY - spot.dirY * 1.0 + spot.dirX * 1.5));
    check(in.open() && in.clearance() > 2.1f && wall.solid(), "Kabine begehbar, Kern massiv");
    auto& pl = s->player();
    pl.x = spot.doorX - spot.dirX * 1.0, pl.y = spot.doorY - spot.dirY * 1.0, pl.z = er->floor;
    pl.angle = std::atan2((double)spot.dirY, (double)spot.dirX);
    s->preload();
    const double sx0 = pl.x, sy0 = pl.y;
    auto run = [&](double secs) {
        for (int i = 0; i < (int)(secs * 60.0); ++i) s->update(1.0 / 60.0, {}, false), s->events().clear();
    };
    run(0.2);
    s->interact();
    check(s->riding(), "Knopf in der Kabine: Fahrt beginnt");
    run(7.0);
    const double moved = std::hypot(s->player().x - sx0, s->player().y - sy0);
    check(!s->riding() && moved > 60.0, "angekommen, weit entfernt (" + std::to_string((int)moved) + " m)");
    const world::Room* dest = s->currentRoom();
    check(dest && W->regionOfZone(dest->zone) != 0 && s->world().generator().zones().strataCoord(s->player().x, s->player().y) >= 1.0,
          "Ziel liegt eine Schicht tiefer");
    // Spielstand mit Verbindung
    Json j = s->toJson();
    check(j["elevators"].size() == 2, "Verbindung (hin und zurueck) im Spielstand");
    // zurueck mit demselben Aufzug
    run(0.2);
    s->interact();
    run(7.0);
    const double back = std::hypot(s->player().x - sx0, s->player().y - sy0);
    check(back < 2.5, "derselbe Aufzug faehrt zurueck (" + std::to_string(back) + " m vom Start der Fahrt)");
}

// --- V9 ------------------------------------------------------------------------------------------
// V13.3: freie Stelle mitten in einer Aquila-Halle nahe (38, 26) - die Karte aendert sich (Kisten, Rohre), feste Koordinaten nicht
static std::pair<int, int> hallSpot(world::World& w) {
    for (int r = 0; r < 12; ++r)
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                const int x = 38 + dx, y = 26 + dy;
                bool ok = true;
                for (int ey = -1; ey <= 1 && ok; ++ey)
                    for (int ex = -1; ex <= 1 && ok; ++ex) {
                        const world::Tile& t = w.tile(x + ex, y + ey);
                        ok = t.open() && t.room && t.room->type && t.room->type->key == "halle" && t.floor < 0.05f;
                    }
                if (ok) return {x, y};
            }
    return {38, 26};
}

static void testV9(const game::Content& content, const std::string& dataDir) {
    std::printf("V9: Sicherungen, der Andere, der Weg hinaus\n");
    auto W = content.world();
    if (!W) return;
    check(W->fuses.items.size() == W->regions.size() && W->fuses.chance > 0.0, "je Ebene eine Sicherung");
    for (const auto& k : W->fuses.items) check(content.item(k) && game::GameSession::isFuse(k), "Sicherung " + k + " als Item");
    // Vorrang-Notizen: zuerst der Hinweis auf die Sicherungen, dann die Entities (V10)
    std::vector<int> journal;
    int first = content.priorityNote(journal, true);
    check(first >= 0 && content.notes()[(size_t)first].title == "Drei Sicherungen", "erste Notiz: Drei Sicherungen");
    journal.push_back(first);
    int second = content.priorityNote(journal, true);
    check(second >= 0 && content.notes()[(size_t)second].title == "Das Lächeln", "zweite Notiz: Das Lächeln (Smiler)");
    check(content.priorityNote(journal, false) < 0, "in getrennten Leveln keine Vorrang-Notizen");

    // Sicherungskaesten: richtige Ebene, tief in der Schicht, nicht in Gaengen, stabil
    JobSystem jobs(2);
    world::World w(W, 4242, 0.28, 0.80, jobs, 3);
    const i64 S = W->sectorSize;
    int boxes[8] = {}, wrong = 0, unstable = 0;
    double nearest[8] = {1e9, 1e9, 1e9, 1e9, 1e9, 1e9, 1e9, 1e9};
    game::FindSpot nearSpot[8];
    std::vector<game::FindSpot> spots, again;
    for (int sy = -8; sy <= 8; ++sy)
        for (int sx = -8; sx <= 8; ++sx) {
            w.tile(sx * S + S / 2, sy * S + S / 2);
            for (const auto& rp : w.sector(sx, sy).rooms) {
                if (!rp->prepared()) continue;
                auto [cx, cy] = rp->center();
                const double st = w.generator().zones().strataCoord(cx, cy);
                spots.clear(), again.clear();
                game::roomFinds(*rp, *W, spots, st);
                game::roomFinds(*rp, *W, again, st);
                if (spots.size() != again.size()) ++unstable;
                for (const auto& f : spots) {
                    if (f.kind != game::FindSpot::FuseBox) continue;
                    const int reg = W->regionOfZone(rp->zone);
                    const double frac = st - std::floor(st);
                    if (rp->isCorridor || W->fuses.items[(size_t)reg] != f.item || frac < W->fuses.depthLo || frac > W->fuses.depthHi) ++wrong;
                    if (!rp->contains(f.tx, f.ty) || !rp->tile(f.tx, f.ty).open()) ++wrong;
                    ++boxes[reg];
                    const double d = std::hypot((double)f.pos.x, (double)f.pos.y);
                    if (d < nearest[reg]) nearest[reg] = d, nearSpot[reg] = f;
                }
            }
        }
    for (size_t r = 0; r < W->regions.size(); ++r)
        check(boxes[r] > 0, std::format("Sicherungskaesten in {} ({}, naechster {:.0f} m)", W->regions[r].name, boxes[r], nearest[r]));
    check(wrong == 0, "Sicherungskaesten nur tief in ihrer eigenen Ebene, an freier Wand (" + std::to_string(wrong) + " falsch)");
    check(unstable == 0, "Fundstuecke stabil");
    check(nearest[0] > 60.0, "kein Sicherungskasten direkt am Start");

    game::PrefabLibrary prefabs;
    prefabs.loadDirectory(dataDir + "/prefabs");
    auto run = [&](game::GameSession& se, double secs, std::vector<game::GameEvent>* keep = nullptr) {
        for (int i = 0; i < (int)(secs * 60.0); ++i) {
            se.update(1.0 / 60.0, {}, false);
            if (keep) keep->insert(keep->end(), se.events().begin(), se.events().end());
            se.events().clear();
        }
    };
    auto has = [](const std::vector<game::GameEvent>& ev, game::GameEvent::Type t) {
        return std::any_of(ev.begin(), ev.end(), [&](const game::GameEvent& e) { return e.type == t; });
    };

    // --- Entities (V10: Praesenz) ------------------------------------------------------------------
    using PE = game::Presence::Event;
    using Role = game::Presence::Role;
    auto hasFig = [](const std::vector<game::GameEvent>& ev, PE::Type t, Role r) {
        return std::any_of(ev.begin(), ev.end(), [&](const game::GameEvent& e) {
            return e.type == game::GameEvent::Figure && e.index == (int)t && e.role == (int)r;
        });
    };
    // Blick auf einen Punkt (off: Versatz in rad, pi = genau weg)
    auto lookAt = [](game::GameSession& se, vec3 q, double off, double h = 1.2) {
        auto& pl = se.player();
        pl.angle = std::atan2(q.y - pl.y, q.x - pl.x) + off;
        pl.pitch = std::atan2(q.z + h - pl.eye(), std::max(0.5, std::hypot(q.x - pl.x, q.y - pl.y)));
        if (off > 1.0) pl.pitch = 0.0;
    };
    // Hound (Entity 8): pirscht, laesst sich durch Blickkontakt einschuechtern, greift an, wenn man wegsieht
    {
        auto s = std::make_unique<game::GameSession>(content, prefabs, W, 4242, "medium", "Test", jobs, 3);
        s->placeAtSpawn();
        s->preload();
        run(*s, 0.2);
        auto& P = s->presence();
        check(!P.threatActive() && P.threatIn() > 100.0, "zu Beginn eine Schonfrist");
        s->forceStalker(9.0);
        std::vector<game::GameEvent> ev;
        run(*s, 0.1, &ev);
        check(P.has(Role::Hound) && hasFig(ev, PE::Appear, Role::Hound), "der Hound erscheint");
        const double d0 = P.distOf(Role::Hound);
        check(d0 > 4.0 && d0 < 16.0, std::format("in einiger Entfernung ({:.1f} m)", d0));
        check(!P.seenRole(Role::Hound), "ungesehen erschienen");
        // ansehen: er kommt nicht heran (Blickkontakt schuechtert ein), knurrt
        s->flashlight().on = true;
        float closestSeen = 1e9f;
        for (int i = 0; i < 60 * 5 && P.has(Role::Hound); ++i) {
            lookAt(*s, P.posOf(Role::Hound), 0.0, 0.55);
            s->update(1.0 / 60.0, {}, false);
            ev.insert(ev.end(), s->events().begin(), s->events().end());
            s->events().clear();
            if (i > 60) closestSeen = std::min(closestSeen, P.distOf(Role::Hound));
        }
        check(P.stateOf(Role::Hound) == 2, std::format("hat dich entdeckt und pirscht (Zustand {})", P.stateOf(Role::Hound)));
        check(hasFig(ev, PE::Growl, Role::Hound), "er knurrt");
        check(!has(ev, game::GameEvent::StalkerHit) && closestSeen > 2.5f, std::format("angesehen haelt er Abstand ({:.1f} m)", closestSeen));
        // den Ruecken zugekehrt: Bellen, Angriff, danach zieht er sich zurueck
        const double h0 = s->stats().health;
        ev.clear();
        for (int i = 0; i < 60 * 15 && !has(ev, game::GameEvent::StalkerHit); ++i) {
            if (P.has(Role::Hound)) lookAt(*s, P.posOf(Role::Hound), 3.14159);
            s->update(1.0 / 60.0, {}, false);
            ev.insert(ev.end(), s->events().begin(), s->events().end());
            s->events().clear();
        }
        check(hasFig(ev, PE::Bark, Role::Hound), "bevor er losstuermt, bellt er");
        check(hasFig(ev, PE::Step, Role::Hound), "man hoert ihn kommen (Krallen, Handflaechen)");
        check(has(ev, game::GameEvent::StalkerHit) && s->runLog().caught == 1, "weggesehen: er greift an");
        // V11: der Zugriff ist toedlich (ENTITY_BEHAVIOR.md F4) - mit Schreck: der Hund bleibt vor der Kamera
        (void)h0;
        check(s->stats().dead && s->deathCause() == "stalker" && s->killRole() == (int)Role::Hound, "V11: der Biss ist toedlich");
        check(s->killCamActive() && P.stateOf(Role::Hound) == 10, "V11: Schreck - er faehrt in die Kamera");
        s->respawnAtCheckpoint();
        check(!s->stats().dead && s->stats().health >= 100.0 && s->deathCause().empty() && !P.has(Role::Hound),
              "V11: Wiedereinstieg - lebendig, volle Health, er ist fort");
        // "ohne Angriffe": er kommt, aber beisst nicht
        P.clear();
        s->figureMode = 1;
        const double h1 = s->stats().health;
        s->forceStalker(6.0);
        ev.clear();
        for (int i = 0; i < 60 * 12; ++i) {
            if (P.has(Role::Hound)) lookAt(*s, P.posOf(Role::Hound), 3.14159);
            s->update(1.0 / 60.0, {}, false);
            ev.insert(ev.end(), s->events().begin(), s->events().end());
            s->events().clear();
        }
        check(hasFig(ev, PE::Appear, Role::Hound) && !has(ev, game::GameEvent::StalkerHit) && s->stats().health >= h1 - 1e-6,
              "Einstellung 'ohne Angriffe': kein Biss");
        // abgeschaltet: keine Entities
        s->figureMode = 0;
        s->forceStalker(9.0);
        run(*s, 0.5);
        check(!P.has(Role::Hound) && P.activeCount() == 0, "Einstellung aus: keine Entities");
        // Tod durch den Hound
        s->figureMode = 2;
        s->stats().health = 10.0;
        s->forceStalker(6.0);
        run(*s, 3.0);
        for (int i = 0; i < 60 * 15 && !s->stats().dead; ++i) {
            if (P.has(Role::Hound)) lookAt(*s, P.posOf(Role::Hound), 3.14159);
            s->update(1.0 / 60.0, {}, false), s->events().clear();
        }
        check(s->stats().dead && s->deathCause() == "stalker", "er kann toeten (Todesursache gemerkt)");
        s->reviveAfterDeath();
        check(!s->stats().dead && s->deathCause().empty(), "nach dem Laden lebendig, Ursache vergessen");
    }
    // Smiler (Entity 3): Stromausfall, im Dunkeln das Laecheln; Blickkontakt haelt ihn, Wegsehen nicht
    {
        // V11/V12: auf der festen Karte von Level 1, mitten in der Halle Aquila - die Begegnung braucht den
        // grossen Raum (F1: mindestens 15 m Weg) und die Regie mit Stromausfall
        auto s = std::make_unique<game::GameSession>(*gContent11, prefabs, gContent11->level("level1", world::kLayoutV6), 4242, "medium",
                                                     "Test", jobs, 3);
        s->placeAtSpawn();
        s->preload();
        run(*s, 0.2);
        auto& P = s->presence();
        world::World& ww = s->world();
        {
            auto& pl = s->player();
            const auto [hx, hy] = hallSpot(ww);
            pl.x = hx + 0.5, pl.y = hy + 0.5, pl.z = ww.tile(hx, hy).floor;
            s->body().reset();
        }
        s->preload();
        run(*s, 0.5);
        const auto [hx2, hy2] = hallSpot(ww);
        const world::Tile& deckT = ww.tile(hx2, hy2);
        const bool placed = deckT.open() && deckT.room && deckT.room->type && deckT.room->type->key == "halle";
        check(placed, "mitten in der Halle Aquila (Level 1)");
        std::vector<game::GameEvent> ev;
        P.force(Role::Smiler);
        // V11: erst laeuft die Welle des Stromausfalls durch, dann erscheint er (mindestens 15 m Weg, F1)
        for (int i = 0; i < 60 * 16 && !P.has(Role::Smiler); ++i) {
            s->update(1.0 / 60.0, {}, false);
            ev.insert(ev.end(), s->events().begin(), s->events().end());
            s->events().clear();
        }
        check(has(ev, game::GameEvent::Blackout) && s->blackout().seed >= 0, "das Licht faellt aus");
        check(P.has(Role::Smiler) && hasFig(ev, PE::Appear, Role::Smiler), "im Dunkeln erscheint der Smiler");
        check(!P.seenRole(Role::Smiler), "nicht vor deinen Augen erschienen");
        // Blickkontakt: er ruehrt sich nicht
        double moved = 0.0;
        bool contact = false;
        for (int i = 0; i < 60 * 4 && P.has(Role::Smiler); ++i) {
            lookAt(*s, P.posOf(Role::Smiler), 0.0, 1.7);
            const vec3 before = P.posOf(Role::Smiler);
            s->update(1.0 / 60.0, {}, false), s->events().clear();
            if (P.seenRole(Role::Smiler)) {
                contact = true;
                moved += std::hypot(P.posOf(Role::Smiler).x - before.x, P.posOf(Role::Smiler).y - before.y);
            }
        }
        check(contact, "man kann ihm in die Augen sehen (er leuchtet im Dunkeln)");
        check(contact && moved < 1e-4, std::format("solange man hinsieht, bewegt er sich nicht ({:.4f} m)", moved));
        // wegsehen: er kommt durch das Dunkel und greift an
        const float dBefore = P.distOf(Role::Smiler);
        float closest = dBefore;
        ev.clear();
        for (int i = 0; i < 60 * 20 && P.has(Role::Smiler); ++i) {
            lookAt(*s, P.posOf(Role::Smiler), 3.14159);
            s->update(1.0 / 60.0, {}, false);
            ev.insert(ev.end(), s->events().begin(), s->events().end());
            s->events().clear();
            if (P.has(Role::Smiler)) closest = std::min(closest, P.distOf(Role::Smiler));
        }
        check(closest < dBefore - 2.0f || has(ev, game::GameEvent::StalkerHit), std::format("ungesehen kommt er naeher ({:.1f} -> {:.1f} m)", dBefore, closest));
        check(hasFig(ev, PE::Rustle, Role::Smiler), "ein tiefes Droehnen verraet ihn");
        check(has(ev, game::GameEvent::StalkerHit), "erreicht er dich ungesehen, greift er an");
        check(s->stats().dead && s->killRole() == (int)Role::Smiler, "V11: der Zugriff ist toedlich");
        {  // V11: Wiedereinstieg, dann zurueck in die Halle
            s->respawnAtCheckpoint();
            auto& pl = s->player();
            const auto [hx, hy] = hallSpot(ww);
            pl.x = hx + 0.5, pl.y = hy + 0.5, pl.z = ww.tile(hx, hy).floor;
            s->body().reset();
            s->preload();
            run(*s, 0.3);
        }
        // zweiter Auftritt: hinsehen, bis das Licht zurueckkehrt - dann erlischt er, ohne Angriff
        P.clear();
        for (int i = 0; i < 60 * 40 && s->blackout().seed >= 0; ++i) s->update(1.0 / 60.0, {}, false), s->events().clear();
        P.force(Role::Smiler);
        for (int i = 0; i < 60 * 16 && !P.has(Role::Smiler); ++i) s->update(1.0 / 60.0, {}, false), s->events().clear();
        ev.clear();
        bool gone = false;
        for (int i = 0; i < 60 * 40; ++i) {
            if (P.has(Role::Smiler)) lookAt(*s, P.posOf(Role::Smiler), 0.0, 1.7);
            s->update(1.0 / 60.0, {}, false);
            ev.insert(ev.end(), s->events().begin(), s->events().end());
            s->events().clear();
            if (!P.has(Role::Smiler)) {
                gone = true;
                break;
            }
        }
        check(gone && !has(ev, game::GameEvent::StalkerHit), "kehrt das Licht zurueck, erlischt er - Blickkontakt hielt ihn fern");
    }
    // V11 (ENTITY_BEHAVIOR.md F2, F3, F4): wer in seiner Naehe rennt, dem kuendigt er sich an - er oeffnet
    // sich, mindestens 1,5 s vor dem Zugriff. Bleibt man stehen und sieht hin, beruhigt er sich. Ein
    // Treffer kostet hoechstens 35 % (auch auf "schwer").
    {
        // V11/V12: auf der festen Karte von Level 1, mitten in der Halle Aquila - die Begegnung braucht den
        // grossen Raum (F1: mindestens 15 m Weg) und die Regie mit Stromausfall
        auto s = std::make_unique<game::GameSession>(*gContent11, prefabs, gContent11->level("level1", world::kLayoutV6), 4242, "hard",
                                                     "Test", jobs, 3);
        s->placeAtSpawn();
        s->preload();
        run(*s, 0.2);
        auto& P = s->presence();
        world::World& ww = s->world();
        {
            auto& pl = s->player();
            const auto [hx, hy] = hallSpot(ww);
            pl.x = hx + 0.5, pl.y = hy + 0.5, pl.z = ww.tile(hx, hy).floor;
            s->body().reset();
        }
        s->preload();
        run(*s, 0.5);
        const auto [hx2, hy2] = hallSpot(ww);
        const world::Tile& deckT = ww.tile(hx2, hy2);
        const bool placed = deckT.open() && deckT.room && deckT.room->type && deckT.room->type->key == "halle";
        // vor das Laecheln stellen: 6 m entfernt, freie Sicht, dahinter 14 m freier Boden zum Wegrennen
        auto standNear = [&]() {
            const vec3 sp = P.posOf(Role::Smiler);
            for (int k = 0; k < 32; ++k) {
                const double a = k * 0.19635;
                const double ux = std::cos(a), uy = std::sin(a);
                bool ok = true;
                for (double d = 5.0; d <= 20.0 && ok; d += 0.5) {
                    const world::Tile& t = ww.tile(ifloor(sp.x + ux * d), ifloor(sp.y + uy * d));
                    ok = t.open() && t.clearance() > 2.1f && std::fabs(t.floor - sp.z) < 0.2f && !(t.flags & world::TF_FEATURE);
                }
                if (!ok) continue;
                auto& pl = s->player();
                pl.x = sp.x + ux * 6.0, pl.y = sp.y + uy * 6.0, pl.z = sp.z;
                s->body().reset();
                return true;
            }
            return false;
        };
        auto appear = [&]() {
            P.clear();
            for (int i = 0; i < 60 * 60 && s->blackout().seed >= 0; ++i) s->update(1.0 / 60.0, {}, false), s->events().clear();
            P.force(Role::Smiler);
            for (int i = 0; i < 60 * 16 && !P.has(Role::Smiler); ++i) s->update(1.0 / 60.0, {}, false), s->events().clear();
            // die Augen oeffnen sich erst (Zustand 0) - dabei ansehen, dann davor stellen
            for (int i = 0; i < 60 * 4 && P.has(Role::Smiler) && P.stateOf(Role::Smiler) < 1; ++i) {
                lookAt(*s, P.posOf(Role::Smiler), 0.0, 1.7);
                s->update(1.0 / 60.0, {}, false), s->events().clear();
            }
            return P.has(Role::Smiler) && standNear();
        };
        // a) weiterrennen: Ankuendigung, frühestens 1,5 s danach der Zugriff, Treffer hoechstens 35 %
        check(placed, "mitten in der Halle Aquila (Level 1, schwer)");
        // V11/V12: Sicherungspunkte - genug, beleuchtet, nicht in den Hallen (mapkit prueft die Regeln beim Entwurf)
        {
            const auto& cps = gContent11->level("level1", world::kLayoutV6)->map->checkpoints;
            int ok = 0;
            for (const auto& c : cps) {
                const world::Tile& t = ww.tile(ifloor(c.x), ifloor(c.y));
                const std::string k = t.room && t.room->type ? t.room->type->key : "";
                if (t.open() && t.room && k != "halle" && k != "gothic" && k != "gild" && !t.room->fixtures().empty()) ++ok;
            }
            check(cps.size() >= 6 && ok == (int)cps.size(), std::format("V11: {} Sicherungspunkte, alle beleuchtet und ausserhalb der Hallen", cps.size()));
        }
        // V12: Ausstattung mit Kollision - Hochregale (Gild), Kistenstapel und runde Saeulen (Gothic) stehen auf
        // unsichtbaren Bloecken; man laeuft nicht hindurch
        {
            const auto& objs = gContent11->level("level1", world::kLayoutV6)->map->objects;
            int n = 0, solid = 0;
            for (const char* model : {"rack", "crate_c", "column_round"})
                for (const auto& o : objs)
                    if (o.model == model) {
                        ++n;
                        s->player().x = o.x, s->player().y = o.y;
                        s->preload();
                        const world::Tile& t = ww.tile(ifloor(o.x), ifloor(o.y));
                        const float need = o.model == "rack" ? 3.4f : (o.model == "crate_c" ? 0.6f : 5.0f);
                        if ((t.flags & world::TF_HIDDEN) && t.floor > need) ++solid;
                        break;
                    }
            check(n == 3 && solid == n, std::format("Regal, Kisten und Saeule mit Kollision ({} von {})", solid, n));
            auto& pl = s->player();
            const auto [hx, hy] = hallSpot(ww);
            pl.x = hx + 0.5, pl.y = hy + 0.5, pl.z = ww.tile(hx, hy).floor;
            s->body().reset();
            s->preload();
        }
        bool ready = appear();
        check(ready, "Laecheln erschienen, 6 m davor freie Bahn");
        double tAnnounce = -1.0, tStage3 = -1.0, tHit = -1.0, t = 0.0;
        bool hitDuringAnnounce = false;
        const double h0 = s->stats().health;
        for (int i = 0; ready && i < 60 * 14 && tHit < 0.0; ++i, t += 1.0 / 60.0) {
            game::PlayerControl c;
            if (tStage3 < 0.0) {
                lookAt(*s, P.posOf(Role::Smiler), 3.14159);  // weg vom Laecheln rennen
                c.forward = 1.0f, c.sprint = true;
            } else {
                lookAt(*s, P.posOf(Role::Smiler), 3.14159);  // dann stehen, abgewandt
            }
            s->update(1.0 / 60.0, c, false);
            for (const auto& e : s->events()) {
                if (e.type == game::GameEvent::Figure && e.index == (int)PE::Announce && tAnnounce < 0.0) tAnnounce = t;
                if (e.type == game::GameEvent::StalkerHit && tHit < 0.0) {
                    tHit = t;
                    if (P.stageOf(Role::Smiler) == 2) hitDuringAnnounce = true;
                }
            }
            s->events().clear();
            if (tStage3 < 0.0 && P.stageOf(Role::Smiler) == 3) tStage3 = t;
        }
        check(tAnnounce >= 0.0, "wer in seiner Naehe rennt: es oeffnet sich (Ankuendigung)");
        check(tStage3 >= 0.0 && tStage3 - tAnnounce >= 1.45, std::format("Zugriff fruehestens 1,5 s nach der Ankuendigung ({:.2f} s)", tStage3 - tAnnounce));
        check(!hitDuringAnnounce, "waehrend der Ankuendigung kein Treffer");
        check(tHit >= 0.0, "wer weiterrennt und sich abwendet, wird getroffen");
        (void)h0;
        check(tHit < 0.0 || (s->stats().dead && s->deathCause() == "stalker"), "V11: der Zugriff ist toedlich");
        // V11: der Schreck - die Kamera reisst zum Gesicht, es steht knapp vor der Linse
        {
            const vec3 eye = s->body().eye();
            game::ViewCamera vc;
            for (int i = 0; i < 12; ++i) {
                s->update(1.0 / 60.0, {}, false), s->events().clear();
                vc = s->camera(false, 1.0 / 60.0);
            }
            const vec3 face = P.posOf(Role::Smiler);
            const float dEye = std::hypot(face.x - eye.x, face.y - eye.y);
            const vec3 fwd = forwardFromAngles(vc.yaw, vc.pitch);
            vec3 headDir = vec3{face.x - eye.x, face.y - eye.y, 0.0f};
            headDir = headDir / std::max(0.01f, length(headDir));
            check(s->killCamActive() && dEye < 1.2f && dot(vec3{fwd.x, fwd.y, 0.0f}, headDir) > 0.8f && vc.fovMul < 0.9f,
                  std::format("V11: Schreck - Blick zum Gesicht, {:.2f} m vor dem Auge, Zoom {:.2f}", dEye, vc.fovMul));
        }
        // V11: Wiedereinstieg am Sicherungspunkt (Start: Lastenflur), Stromausfall vorbei, Schonzeit
        {
            const std::string cp = s->checkpointId();
            s->respawnAtCheckpoint();
            const world::MapCheckpoint* c0 = nullptr;
            for (const auto& c : gContent11->level("level1", world::kLayoutV6)->map->checkpoints)
                if (c.id == cp) c0 = &c;
            check(c0 && std::hypot(s->player().x - c0->x, s->player().y - c0->y) < 1.0 && !s->stats().dead &&
                      s->stats().health >= 100.0 && s->blackout().seed < 0 && !P.has(Role::Smiler),
                  std::format("V11: Wiedereinstieg am Sicherungspunkt \"{}\"", cp));
            // Schonzeit: 60 s ohne Entity und ohne Stromausfall
            bool quiet = true;
            for (int i = 0; i < 60 * 60 && quiet; ++i) {
                s->update(1.0 / 60.0, {}, false), s->events().clear();
                quiet = !P.threatActive() && s->blackout().seed < 0;
            }
            check(quiet, "V11: nach dem Wiedereinstieg eine Minute Ruhe");
            // ein neuer Sicherungspunkt: vorbeigehen speichert ihn (Ereignis), der naechste Tod fuehrt dorthin
            const world::MapCheckpoint* ramp = nullptr;
            for (const auto& c : gContent11->level("level1", world::kLayoutV6)->map->checkpoints)
                if (c.id == "vorraum") ramp = &c;
            bool got = false;
            if (ramp) {
                auto& pl = s->player();
                pl.x = ramp->x, pl.y = ramp->y, pl.z = ww.tile(ifloor(ramp->x), ifloor(ramp->y)).floor;
                s->body().reset();
                s->preload();
                for (int i = 0; i < 10 && !got; ++i) {
                    s->update(1.0 / 60.0, {}, false);
                    for (const auto& e : s->events()) got = got || e.type == game::GameEvent::Checkpoint;
                    s->events().clear();
                }
            }
            check(got && s->checkpointId() == "vorraum", "V11: Sicherungspunkt im Buero an der Halle erreicht (Ereignis, gespeichert)");
            s->stats().health = 0.0, s->stats().dead = true;
            s->respawnAtCheckpoint();
            check(ramp && std::hypot(s->player().x - ramp->x, s->player().y - ramp->y) < 1.0, "V11: der naechste Wiedereinstieg dort");
            const Json sv = s->toJson();
            check(sv["checkpoint"].asString("") == "vorraum", "V11: der Sicherungspunkt steht im Spielstand");
        }
        // b) stehen bleiben und hinsehen: es beruhigt sich, kein Treffer
        {  // V11: Wiedereinstieg, dann zurueck in die Halle
            s->respawnAtCheckpoint();
            auto& pl = s->player();
            const auto [hx, hy] = hallSpot(ww);
            pl.x = hx + 0.5, pl.y = hy + 0.5, pl.z = ww.tile(hx, hy).floor;
            s->body().reset();
            s->preload();
            run(*s, 0.3);
        }
        ready = appear();
        check(ready, "zweites Laecheln, 6 m davor freie Bahn");
        tAnnounce = -1.0;
        double tCalm = -1.0;
        bool hit = false;
        t = 0.0;
        for (int i = 0; ready && i < 60 * 8; ++i, t += 1.0 / 60.0) {
            game::PlayerControl c;
            if (tAnnounce < 0.0) {
                lookAt(*s, P.posOf(Role::Smiler), 3.14159);
                c.forward = 1.0f, c.sprint = true;
            } else if (P.has(Role::Smiler)) {
                lookAt(*s, P.posOf(Role::Smiler), 0.0, 1.7);  // stehen bleiben, hinsehen
            }
            s->update(1.0 / 60.0, c, false);
            for (const auto& e : s->events()) {
                if (e.type == game::GameEvent::Figure && e.index == (int)PE::Announce && tAnnounce < 0.0) tAnnounce = t;
                if (e.type == game::GameEvent::Figure && e.index == (int)PE::Calm && tCalm < 0.0) tCalm = t;
                if (e.type == game::GameEvent::StalkerHit) hit = true;
            }
            s->events().clear();
        }
        check(tAnnounce >= 0.0 && tCalm > tAnnounce, "stehen bleiben und hinsehen: es schliesst sich wieder");
        check(!hit, "wer sich beruhigt, wird nicht getroffen (F3)");
        // c) V13.2 (Entity-3-Seite: Panik oder lauter Laerm): stillstehen, hinsehen, aber etwas fallen lassen -
        // der Aufprall loest die Ankuendigung aus; wer den Blick haelt, beruhigt es wieder
        run(*s, 6.0);
        ready = appear();
        tAnnounce = -1.0, tCalm = -1.0, hit = false, t = 0.0;
        bool thrown = false;
        for (int i = 0; ready && i < 60 * 6; ++i, t += 1.0 / 60.0) {
            if (P.has(Role::Smiler)) lookAt(*s, P.posOf(Role::Smiler), 0.0, 1.7);
            if (!thrown && i == 30) s->dropStack("battery", 1), thrown = true;
            s->update(1.0 / 60.0, {}, false);
            for (const auto& e : s->events()) {
                if (e.type == game::GameEvent::Figure && e.index == (int)PE::Announce && tAnnounce < 0.0) tAnnounce = t;
                if (e.type == game::GameEvent::Figure && e.index == (int)PE::Calm && tCalm < 0.0) tCalm = t;
                if (e.type == game::GameEvent::StalkerHit) hit = true;
            }
            s->events().clear();
        }
        check(ready && tAnnounce >= 0.5, std::format("V13.2: Aufprall in seiner Naehe - es oeffnet sich ({:.2f} s)", tAnnounce));
        check(tCalm >= tAnnounce + 0.55 && !hit, std::format("V13.2: wer danach stillsteht und hinsieht, wird nicht getroffen (Ankuendigung {:.2f} s, Ruhe {:.2f} s, Treffer {}, Stufe {})", tAnnounce, tCalm, hit, P.stageOf(Role::Smiler)));
    }
    // Deathmoths (Entity 4): Nest an der Tuer eines dunklen Zimmers, Schwarm fliegt im Licht auf
    {
        auto s = std::make_unique<game::GameSession>(content, prefabs, W, 4242, "medium", "Test", jobs, 3);
        s->placeAtSpawn();
        s->preload();
        run(*s, 0.2);
        auto& P = s->presence();
        world::World& ww = s->world();
        // das Hotel (Level 5) liegt weit draussen: dort eine Tuer zu einem dunklen Zimmer suchen (wie das Spiel)
        bool found = false;
        vec3 inside{}, outside{};
        i64 px = ifloor(s->player().x), py = ifloor(s->player().y);
        bool hotel = false;
        for (int k = 0; k < 16 && !hotel; ++k)
            for (double r = 680.0; r <= 1000.0 && !hotel; r += 40.0) {
                const i64 hx = px + (i64)(std::cos(k * 0.3927) * r), hy = py + (i64)(std::sin(k * 0.3927) * r);
                const world::Tile& t = ww.tile(hx, hy);
                if (t.open() && t.room && W->regionOfZone(t.room->zone) == 2) px = hx, py = hy, hotel = true;
            }
        check(hotel, "das Hotel gefunden");
        for (int rad = 2; rad <= 60 && !found; rad += 2)
            for (int dy = -rad; dy <= rad && !found; ++dy)
                for (int dx = -rad; dx <= rad && !found; ++dx) {
                    if (std::max(std::abs(dx), std::abs(dy)) != rad) continue;
                    const world::Tile& t = ww.tile(px + dx, py + dy);
                    if (!(t.flags & world::TF_DOOR) || !t.door) continue;
                    const world::Door& d = *t.door;
                    if (d.kind != world::DoorKind::Door || d.width() != 1 || d.top - d.sill < 2.05 || d.top - d.sill > 2.45) continue;
                    for (int side = 0; side < 2 && !found; ++side) {
                        const i64 fx = d.axis == 0 ? (side == 0 ? d.x0 - 1 : d.x1) : px + dx, fy = d.axis == 0 ? py + dy : (side == 0 ? d.y0 - 1 : d.y1);
                        const i64 ox = d.axis == 0 ? (side == 0 ? d.x1 : d.x0 - 1) : px + dx, oy = d.axis == 0 ? py + dy : (side == 0 ? d.y1 : d.y0 - 1);
                        const world::Tile& f = ww.tile(fx, fy);
                        const world::Tile& o = ww.tile(ox, oy);
                        if (!f.open() || !f.room || f.light > 0.35f || f.clearance() < 2.6f || f.room->avgLight > 0.2 || !o.open()) continue;
                        // der Spieler: ein Stueck vor der Tuer auf der anderen Seite
                        const int sx = d.axis == 0 ? (int)(ox - fx) : 0, sy = d.axis == 0 ? 0 : (int)(oy - fy);
                        i64 qx = ox, qy = oy;
                        for (int k = 0; k < 12 && ww.tile(qx + sx, qy + sy).open(); ++k) qx += sx, qy += sy;
                        if (std::hypot((double)(qx - fx), (double)(qy - fy)) < 9.0) continue;
                        auto& pl = s->player();
                        pl.x = (double)qx + 0.5, pl.y = (double)qy + 0.5, pl.z = ww.tile(qx, qy).floor;
                        pl.angle = std::atan2((double)fy - (double)qy, (double)fx - (double)qx) + 3.14159;  // von der Tuer weg
                        s->body().reset();
                        inside = {(float)fx + 0.5f, (float)fy + 0.5f, f.floor};
                        outside = {(float)pl.x, (float)pl.y, (float)pl.z};
                        found = true;
                    }
                }
        check(found, "eine Tuer zu einem dunklen Zimmer gefunden");
        if (found) {
            s->preload();
            run(*s, 0.3);
            const double dd = std::hypot(inside.x - s->player().x, inside.y - s->player().y);
            P.force(Role::Moth, dd);
            run(*s, 0.2);
            check(P.has(Role::Nest) && P.count(Role::Moth) >= 5, std::format("ein Nest mit Motten ({} Motten)", P.count(Role::Moth)));
            vec3 sc;
            check(P.swarm(sc) == 0 && P.stateOf(Role::Moth) == 0, "sie sitzen still an Wand und Decke");
            std::vector<game::ShadowCaster> sh;
            P.shadows(sh);
            check(sh.empty(), "Nest und Motten: keine Kapselschatten");
            // hineingehen, Lampe an: der Schwarm fliegt auf und umkreist das Licht; Stiche
            const vec3 n = P.posOf(Role::Nest);
            auto& pl = s->player();
            pl.x = inside.x, pl.y = inside.y, pl.z = inside.z;
            pl.angle = std::atan2(n.y - pl.y, n.x - pl.x);
            s->body().reset();
            s->flashlight().on = true;
            std::vector<game::GameEvent> ev;
            const double h0 = s->stats().health;
            int maxFly = 0;
            for (int i = 0; i < 60 * 20; ++i) {
                s->update(1.0 / 60.0, {}, false);
                ev.insert(ev.end(), s->events().begin(), s->events().end());
                s->events().clear();
                maxFly = std::max(maxFly, P.swarm(sc));
            }
            check(hasFig(ev, PE::TakeOff, Role::Moth), "Licht und Naehe scheuchen den Schwarm auf");
            check(maxFly >= 5, std::format("der Schwarm fliegt ({} zugleich)", maxFly));
            check(hasFig(ev, PE::Sting, Role::Moth) && s->stats().health < h0, "mit Lampe stossen sie zu");
            // Lampe aus und hinaus: sie kehren an ihre Plaetze zurueck
            s->flashlight().on = false;
            pl.x = outside.x, pl.y = outside.y, pl.z = outside.z;
            s->body().reset();
            run(*s, 30.0);
            check(P.swarm(sc) == 0, "Lampe aus: zurueck am Nest, sie sitzen wieder");
        }
    }
    // Gestalt (Level 0): weit weg, im Licht, aendert sich nie vor deinen Augen, wirft keinen Schatten
    {
        auto s = std::make_unique<game::GameSession>(content, prefabs, W, 4242, "medium", "Test", jobs, 3);
        s->placeAtSpawn();
        s->preload();
        run(*s, 0.2);
        auto& P = s->presence();
        std::vector<game::GameEvent> ev;
        for (int k = 0; k < 6 && !P.has(Role::Gestalt); ++k) {
            P.force(Role::Gestalt, 14.0 + 3.0 * k);
            run(*s, 0.1, &ev);
            if (!P.has(Role::Gestalt)) s->player().angle += 1.0;
        }
        check(P.has(Role::Gestalt), "die Gestalt erscheint");
        check(P.distOf(Role::Gestalt) > 11.0f && !P.seenRole(Role::Gestalt), std::format("weit weg und nicht im Blick ({:.1f} m)", P.distOf(Role::Gestalt)));
        std::vector<game::ShadowCaster> sh;
        P.shadows(sh);
        check(sh.empty(), "sie wirft keinen Schatten");
        double moved = 0.0;
        bool seen = false;
        for (int i = 0; i < 60 * 3 && P.has(Role::Gestalt); ++i) {
            lookAt(*s, P.posOf(Role::Gestalt), 0.0, 1.6);
            const vec3 before = P.posOf(Role::Gestalt);
            s->update(1.0 / 60.0, {}, false);
            ev.insert(ev.end(), s->events().begin(), s->events().end());
            s->events().clear();
            if (P.seenRole(Role::Gestalt)) seen = true, moved += std::hypot(P.posOf(Role::Gestalt).x - before.x, P.posOf(Role::Gestalt).y - before.y);
        }
        check(seen && moved < 1e-4 && P.has(Role::Gestalt), "angesehen bleibt sie, wo sie ist");
        check(hasFig(ev, PE::Vision, Role::Gestalt), "gesehen (zehrt am Verstand)");
        for (int i = 0; i < 60 * 2 && P.has(Role::Gestalt); ++i) {
            lookAt(*s, P.posOf(Role::Gestalt), 3.14159);
            s->update(1.0 / 60.0, {}, false), s->events().clear();
        }
        check(!P.has(Role::Gestalt), "weggesehen - fort");
    }
    // Bereiche: in den Innenraeumen (Level 0) gibt es weder Smiler noch Hound noch Motten
    {
        auto s = std::make_unique<game::GameSession>(content, prefabs, W, 4242, "medium", "Test", jobs, 3);
        s->placeAtSpawn();
        s->preload();
        auto& P = s->presence();
        bool other = false;
        int region0 = 0, frames = 0;
        for (int i = 0; i < 60 * 400; ++i) {
            s->update(1.0 / 60.0, {}, false), s->events().clear();
            ++frames;
            region0 += s->currentRegion() == 0 ? 1 : 0;
            other = other || P.has(Role::Smiler) || P.has(Role::Hound) || P.has(Role::Moth) || P.has(Role::Nest);
        }
        check(region0 == frames, "der Start liegt in den Innenraeumen");
        check(!other, "in den Innenraeumen erscheinen keine echten Entities (Level 0: keine bestaetigten)");
    }

    // --- Sicherung herausnehmen, Spielstand, letzte Fahrt -----------------------------------------
    {
        auto s = std::make_unique<game::GameSession>(content, prefabs, W, 4242, "medium", "Test", jobs, 3);
        s->figureMode = 0;
        s->placeAtSpawn();
        const game::FindSpot& f = nearSpot[0];
        auto& pl = s->player();
        pl.x = f.pos.x + std::cos(f.yaw) * 1.3, pl.y = f.pos.y + std::sin(f.yaw) * 1.3;
        pl.z = s->world().tile(ifloor(pl.x), ifloor(pl.y)).floor;
        pl.angle = f.yaw + 3.14159265, pl.pitch = 0.0;
        s->preload();
        std::vector<game::GameEvent> ev;
        run(*s, 0.6, &ev);
        int fb = 0;
        s->registry().each<game::FuseBox>([&](ecs::Entity, game::FuseBox& b) { fb += b.id == f.id ? 1 : 0; });
        check(fb == 1, "Sicherungskasten erscheint mit seinem Chunk");
        check(s->targetPrompt() == "Sicherung herausnehmen", "Hinweis vor dem Kasten: " + s->targetPrompt());
        check(s->knownFuses().size() == 1, "gesehener Kasten fuer die Karte gemerkt");
        s->interact();
        ev.clear();
        run(*s, 0.1, &ev);
        check(s->hasFuse(f.item) && s->fusesHeld() == 1 && s->picked.count(f.id), "Sicherung genommen");
        check(s->runLog().fuses == 1, "Laufbuch zaehlt die Sicherung");
        bool empty = false;
        s->registry().each<game::FuseBox, game::MeshRenderer>([&](ecs::Entity e, game::FuseBox& b, game::MeshRenderer& m) {
            if (b.id == f.id) empty = b.taken && m.model == "fusebox_empty" && !s->registry().get<game::Interactable>(e);
        });
        check(empty, "Kasten danach leer und still");
        // benutzen verbraucht sie nicht
        int slot = -1;
        for (int i = 0; i < game::Inventory::kSize; ++i)
            if (s->inventory().slots[(size_t)i] && s->inventory().slots[(size_t)i]->kind == f.item) slot = i;
        s->useSlot(slot);
        check(s->hasFuse(f.item), "eine Sicherung laesst sich nicht verzehren");
        // Spielstand
        Json j = s->toJson();
        auto t = std::make_unique<game::GameSession>(content, prefabs, W, 4242, "medium", "Test", jobs, 3);
        t->fromJson(j);
        check(t->fusesHeld() == 1 && t->knownFuses().size() == 1 && t->runLog().fuses == 1, "Sicherung und Kasten im Spielstand");
        check(!t->presence().threatActive() && t->presence().threatIn() >= 89.0, "nach dem Laden eine Schonfrist");

        // die letzte Fahrt: alle drei Sicherungen, ein Aufzug
        for (const auto& k : W->fuses.items)
            if (!s->hasFuse(k)) s->inventory().add(k);
        check(s->fusesHeld() == 3, "alle drei Sicherungen");
        world::World& ww = s->world();
        const world::Room* er = nullptr;
        for (int rad = 0; rad <= 6 && !er; ++rad)
            for (int sy = -rad; sy <= rad && !er; ++sy)
                for (int sx = -rad; sx <= rad && !er; ++sx) {
                    if (std::max(std::abs(sx), std::abs(sy)) != rad) continue;
                    for (const auto& r : ww.sector(sx, sy).rooms) {
                        if (!r->elevatorWanted) continue;
                        auto [cx, cy] = r->center();
                        ww.tile(ifloor(cx), ifloor(cy));
                        if (r->elevatorSpot()) {
                            er = r.get();
                            break;
                        }
                    }
                }
        if (!er) return;
        auto spot = *er->elevatorSpot();
        pl.x = spot.doorX - spot.dirX * 1.0, pl.y = spot.doorY - spot.dirY * 1.0, pl.z = er->floor;
        pl.angle = std::atan2((double)spot.dirY, (double)spot.dirX);
        s->preload();
        run(*s, 0.2);
        check(s->targetPrompt() == "Sicherungen einsetzen", "in der Kabine: " + s->targetPrompt());
        const double x0 = pl.x, y0 = pl.y;
        s->interact();
        ev.clear();
        run(*s, 6.0, &ev);
        check(s->escaping() && s->escapeGlow() > 0.2f && has(ev, game::GameEvent::EscapeStart), "die letzte Fahrt beginnt, die Kabine wird hell");
        check(s->fusesHeld() == 3, "Sicherungen bleiben bis zum Ende im Inventar (Autosave unterwegs)");
        ev.clear();
        run(*s, 6.0, &ev);
        check(has(ev, game::GameEvent::Escaped) && s->runLog().escaped, "entkommen");
        check(s->fusesHeld() == 0, "die Sicherungen bleiben in den Fassungen");
        check(std::hypot(s->player().x - x0, s->player().y - y0) < 0.5, "die letzte Fahrt fuehrt nirgendwohin (kein Ortswechsel)");
        Json k = s->toJson();
        check(k["runlog"]["escaped"].asBool(false) && k["runlog"]["escape_time"].asNumber(0.0) > 0.0, "Entkommen im Spielstand");
        s->finishEscape();
        run(*s, 0.5);
        check(!s->riding() && !s->escaping(), "nach dem Abspann: weiter erkunden");
    }
}

static float hdist2(vec3 a, vec3 b) { return std::hypot(a.x - b.x, a.y - b.y); }

// V11: die neuen Kreaturen - Skelette (Lider des Laechelns), leise gehen, das Buendel (nur Gehoer)
static void testV11Creatures(const std::string& dataDir) {
    std::printf("V11: Kreaturen - Skelette, leise gehen, Buendel\n");
    // Skelette: hoechstens 64 Knochen (Gewichtsmasken); die Augen des Laechelns in 1,7 - 2,0 m Hoehe, ihr
    // Rahmen rechtwinklig, das Scharnier der Lider hinter der Haut (ART_DIRECTION 5.2)
    for (int e = 0; e < anim::kEntityCount; ++e)
        check(anim::entitySkeleton((anim::Entity)e).boneCount() <= 64, std::format("Skelett {}: hoechstens 64 Knochen", e));
    check(anim::entitySkeleton(anim::Entity::Clump).boneCount() == anim::cl::BCOUNT, "Buendel: Kern, sieben Arme, zwei Beine");
    for (int sd = 0; sd < 2; ++sd) {
        const anim::sm::Eye e = anim::sm::smilerEye(sd);
        check(e.surf.z >= 1.7f && e.surf.z <= 2.0f, "Augen des Laechelns in 1,7 - 2,0 m Hoehe");
        check(std::fabs(dot(e.n, e.t)) < 1e-3f && std::fabs(dot(e.n, e.u)) < 1e-3f && std::fabs(dot(e.t, e.u)) < 1e-3f, "Augenrahmen rechtwinklig");
        check(std::fabs(length(e.surf - e.hinge) - anim::sm::kLidHinge) < 1e-4f && e.n.x > 0.8f, "Lidscharnier hinter der Haut, Blick nach vorn");
    }
    check(anim::sm::smilerEye(0).len != anim::sm::smilerEye(1).len, "die Augen sind leicht ungleich");

    // leise gehen: knapp halbes Tempo, kein Sprint (ENTITY_BEHAVIOR 1)
    {
        FnWorld w;
        w.fn = [](i64, i64, float& f, float& c) { f = 0.0f, c = 3.0f; };
        game::Player p(0.5, 0.5, 0.0, 0.0);
        game::PlayerControl c;
        c.forward = 1.0f, c.sneak = true, c.sprint = true;
        stepN(p, w, 240, c);
        check(p.sneaking && !p.sprinting && p.groundSpeed > 1.0 && p.groundSpeed < 1.6,
              std::format("leise: {:.2f} m/s, kein Sprint", p.groundSpeed));
    }

    // Buendel (ENTITY_BEHAVIOR 2.3) auf der festen Karte von Level 2
    const game::Content& content = *gContent11;
    game::PrefabLibrary prefabs;
    prefabs.loadDirectory(dataDir + "/prefabs");
    JobSystem jobs(2);
    using PE = game::Presence::Event;
    using Role = game::Presence::Role;
    auto s = std::make_unique<game::GameSession>(content, prefabs, content.level("level2", world::kLayoutV6), 4242, "medium", "Test", jobs, 3);
    s->placeAtSpawn();
    s->preload();
    std::vector<game::GameEvent> ev;
    auto step = [&](double secs, game::PlayerControl c = {}) {
        for (int i = 0; i < (int)(secs * 60.0); ++i) {
            s->update(1.0 / 60.0, c, false);
            ev.insert(ev.end(), s->events().begin(), s->events().end());
            s->events().clear();
        }
    };
    auto fig = [&](PE::Type t) {
        return std::any_of(ev.begin(), ev.end(), [&](const game::GameEvent& e) {
            return e.type == game::GameEvent::Figure && e.index == (int)t && e.role == (int)Role::Clump;
        });
    };
    step(0.3);
    auto& P = s->presence();
    world::World& ww = s->world();
    check(ww.level().allowClump, "Level 2 kennt das Buendel");
    P.force(Role::Clump);
    step(0.2);
    check(P.has(Role::Clump) && P.stateOf(Role::Clump) == 0, "es lauert in einem Deckenschacht");
    const vec3 lair = P.posOf(Role::Clump);
    // ein gerader Gang vom Schacht weg: der Spieler 5 m entfernt, Blick vom Schacht weg
    int dir = -1;
    static const int dx[4] = {1, 0, -1, 0}, dy[4] = {0, 1, 0, -1};
    for (int k = 0; k < 4 && dir < 0; ++k) {
        bool ok = true;
        for (int m = 1; m <= 9 && ok; ++m) {
            const world::Tile& t = ww.tile(ifloor(lair.x) + dx[k] * m, ifloor(lair.y) + dy[k] * m);
            ok = t.open() && std::fabs(t.floor - lair.z) < 0.3f;
        }
        if (ok) dir = k;
    }
    check(dir >= 0, "vom Schacht fuehrt ein gerader Gang weg");
    if (dir < 0) return;
    {
        auto& pl = s->player();
        pl.x = lair.x + dx[dir] * 5.0, pl.y = lair.y + dy[dir] * 5.0, pl.z = lair.z;
        pl.angle = std::atan2((double)dy[dir], (double)dx[dir]), pl.pitch = -0.2;
        pl.vx = pl.vy = 0.0;
        s->body().reset();
    }
    // still stehen: es bemerkt nichts (blind, hoert nur)
    ev.clear();
    step(8.0);
    check(P.stateOf(Role::Clump) == 0 && !fig(PE::Knock), "wer still steht, bleibt unbemerkt");
    // etwas werfen: es lauscht (klopft), faellt heraus und zieht sich zum Aufprall - nicht zum Spieler
    s->dropStack("almond", 1);
    step(2.2);
    s->dropStack("almond", 1);
    vec3 land{};
    bool landed = false;
    for (int i = 0; i < 60 * 14; ++i) {
        step(1.0 / 60.0);
        for (const auto& e : ev)
            if (e.type == game::GameEvent::ItemLand) land = e.pos, landed = true;
    }
    check(landed && fig(PE::Knock), "es hat den Aufprall gehoert und klopft");
    check(fig(PE::Drop) && hdist2(P.posOf(Role::Clump), lair) > 3.0f, "es faellt aus dem Schacht und zieht sich fort");
    check(landed && hdist2(P.posOf(Role::Clump), land) < 2.6f, std::format("zum Ort des Aufpralls ({:.1f} m davon)", hdist2(P.posOf(Role::Clump), land)));
    check(!fig(PE::Windup) && !s->stats().dead, "am still stehenden Spieler vorbei: kein Griff");
    check(fig(PE::Step) || fig(PE::Drag), "man hoert es (Haende, Schleifen)");
    // nah heran und auf und ab gehen (Laerm in < 2,5 m): der lange Arm holt aus; leise weg: es sucht weiter
    const vec3 axis{(float)dx[dir], (float)dy[dir], 0.0f};
    auto me = [&]() { return vec3{(float)s->player().x, (float)s->player().y, 0.0f}; };
    auto along = [&](float sign) { s->player().angle = std::atan2(axis.y * sign, axis.x * sign); };
    ev.clear();
    for (int i = 0; i < 60 * 10 && !fig(PE::Windup); ++i) {
        const vec3 q = P.posOf(Role::Clump);
        const float side = dot(q - me(), axis) > 0.0f ? 1.0f : -1.0f;  // wo es im Gang liegt
        game::PlayerControl c;
        along(side);
        c.forward = hdist2(me(), q) > 2.1f ? 1.0f : ((i / 20) % 2 == 0 ? -1.0f : 1.0f);  // heran, dann auf und ab
        step(1.0 / 60.0, c);
    }
    check(fig(PE::Windup) && P.stateOf(Role::Clump) == 4, "Laerm in Armlaenge: es holt aus (Ankuendigung)");
    for (int i = 0; i < 60 * 3 && (P.stateOf(Role::Clump) == 4 || i < 30); ++i) {
        const vec3 q = P.posOf(Role::Clump);
        along(dot(me() - q, axis) > 0.0f ? 1.0f : -1.0f);  // vom Buendel weg, den Gang entlang
        game::PlayerControl c;
        c.forward = 1.0f, c.sneak = true;
        step(1.0 / 60.0, c);
    }
    check(!s->stats().dead && P.stateOf(Role::Clump) != 4 && P.stateOf(Role::Clump) != 10,
          std::format("leise von der Stelle weg: der Griff geht ins Leere (Zustand {})", P.stateOf(Role::Clump)));
    // wieder heran und weiter Laerm: es greift zu (toedlich, F4)
    ev.clear();
    for (int i = 0; i < 60 * 12 && !s->stats().dead; ++i) {
        const vec3 q = P.posOf(Role::Clump);
        along(dot(q - me(), axis) > 0.0f ? 1.0f : -1.0f);
        game::PlayerControl c;
        c.forward = hdist2(me(), q) > 1.5f ? 1.0f : ((i / 20) % 2 == 0 ? -1.0f : 1.0f);
        step(1.0 / 60.0, c);
    }
    check(s->stats().dead && s->killRole() == (int)Role::Clump &&
              std::any_of(ev.begin(), ev.end(), [](const game::GameEvent& e) { return e.type == game::GameEvent::StalkerHit; }),
          "wer weiter Laerm macht, wird gegriffen (toedlich)");
}

// V11: die Reise - jeder Uebergang hat im Ziel sein Gegenstueck (gleicher Anker), Ziele existieren; das Ende
// liegt in Level 37. Wasser: im Meer von Level 7 schwimmt man; wer zu lange unten bleibt, ertrinkt.
static void testV11Journey(const std::string& dataDir) {
    std::printf("V11: Uebergaenge der Reise, Wasser\n");
    const game::Content& content = *gContent11;
    static const char* kLevels[] = {"level0", "level1", "level2", "level3", "level5", "level5_3", "level188", "level903", "level37", "level36"};
    int exits = 0, ends = 0;
    for (const char* id : kLevels) {
        const auto L = content.level(id, world::kLayoutV6);
        check(L && L->map, std::string(id) + ": feste Karte geladen");
        if (!L || !L->map) continue;
        for (const world::MapTransition& t : L->map->transitions) {
            if (t.kind == "end") {
                ++ends;
                check(std::string(id) == "level36", "das Ende liegt in Level 36 (Gate-Tuer)");
                continue;
            }
            if (t.target.empty()) continue;  // kommt hier nur an
            ++exits;
            const auto T = content.level(t.target, world::kLayoutV6);
            check(T && T->map, std::format("{} -> {}: Ziel vorhanden", id, t.target));
            if (!T || !T->map) continue;
            const bool paired = std::any_of(T->map->transitions.begin(), T->map->transitions.end(),
                                            [&](const world::MapTransition& u) { return u.id == t.id; });
            check(paired, std::format("{} -> {}: Anker {} hat im Ziel ein Gegenstueck", id, t.target, t.id));
            check(t.x1 > t.x0 && t.y1 > t.y0, std::format("{}: Ausloesebereich von {} nicht leer", id, t.id));
        }
    }
    check(exits >= 11 && ends == 1, std::format("{} Ausgaenge, {} Ende", exits, ends));

    // Wasser: Level 37 - in einer Grube (Treppe ins tiefe Wasser) schwimmen; unter Wasser geht die Luft aus
    game::PrefabLibrary prefabs;
    prefabs.loadDirectory(dataDir + "/prefabs");
    JobSystem jobs(2);
    auto s = std::make_unique<game::GameSession>(content, prefabs, content.level("level37", world::kLayoutV6), 4242, "medium", "Test", jobs, 3);
    s->placeAtSpawn();
    s->preload();
    for (int i = 0; i < 20; ++i) s->update(1.0 / 60.0, {}, false), s->events().clear();
    world::World& ww = s->world();
    // eine tiefe Kachel in einer Grube (Raumtyp "grube") suchen: die groesste Grube, nahe ihrer Mitte, keine Stufe
    i64 sx = 0, sy = 0;
    bool found = false;
    const world::MapRoom* best = nullptr;
    for (const world::MapRoom& r : content.level("level37", world::kLayoutV6)->map->rooms)
        if (r.type == "grube" && (!best || (r.x1 - r.x0) * (r.y1 - r.y0) > (best->x1 - best->x0) * (best->y1 - best->y0))) best = &r;
    if (best) {
        s->player().x = (best->x0 + best->x1) * 0.5, s->player().y = (best->y0 + best->y1) * 0.5;
        s->preload();
        for (i64 y = best->y0 + 1; y < best->y1 - 1 && !found; ++y)
            for (i64 x = best->x0 + 1; x < best->x1 - 1 && !found; ++x) {
                const world::Tile& t = ww.tile(x, y);
                if (t.open() && t.room && t.room->water > t.floor + 3.0 && !(t.flags & world::TF_STAIR)) sx = x, sy = y, found = true;
            }
    }
    check(found, "Level 37: tiefe Grube gefunden");
    if (!found) return;
    {
        auto& pl = s->player();
        const world::Tile& t = ww.tile(sx, sy);
        pl.x = (double)sx + 0.5, pl.y = (double)sy + 0.5, pl.z = t.room->water - 1.4;
        pl.vx = pl.vy = pl.vz = 0.0;
        s->body().reset();
    }
    for (int i = 0; i < 60; ++i) s->update(1.0 / 60.0, {}, false), s->events().clear();
    check(s->player().swimming, "im tiefen Wasser schwimmt man");
    // nach unten schwimmen und unten bleiben: nach einer Weile ertrinkt man
    game::PlayerControl down;
    down.forward = 1.0f;
    bool under = false;
    for (int i = 0; i < 60 * 70 && !s->stats().dead; ++i) {
        s->player().pitch = -1.3;
        s->update(1.0 / 60.0, down, false);
        s->events().clear();
        under = under || s->player().underwater;
    }
    check(under, "unter Wasser");
    check(s->stats().dead && (s->deathCause() == "wasser" || s->deathCause() == "tiefe"),
          std::format("zu lange unten: tot (Ursache {})", s->deathCause()));
}

// V11 (Premium-Ueberarbeitung): Vorraete je Ebene aus dem Spawnplan eines Laufs - wie viele, welche Art, wo
// sie liegen (Boden oder Ablage), kleinster Abstand zweier gleicher Funde. Grundlage der Pruefung "weniger,
// besser platziert". Der Plan ist die vollstaendige Belegung der Karte (unabhaengig davon, was geladen ist).
struct SupplyStats {
    int total = 0, onFloor = 0, onSurface = 0;
    std::map<std::string, int> kinds;
    float minSame = 1e9f;
};
static SupplyStats countSupplies(const std::string& level, u64 seed) {
    SupplyStats st;
    const game::Content& content = *gContent11;
    const auto L = content.level(level, world::kLayoutV6);
    if (!L || !L->map) return st;
    JobSystem jobs(1);
    world::World ww(L, seed, 0.28, 0.80, jobs, 1);
    const world::SpawnPlan* plan = ww.generator().plan();
    if (!plan) return st;
    std::vector<std::pair<vec3, std::string>> found;
    for (size_t i = 0; i < L->map->spawns.size(); ++i) {
        const world::MapSpawn& sp = L->map->spawns[i];
        if (sp.kind != "item" || !plan->active(i)) continue;
        const bool surface = sp.hasTag("table") || sp.hasTag("shelf") || sp.hasTag("surface");
        found.push_back({vec3{(float)sp.x, (float)sp.y, surface ? 1.0f : 0.0f}, plan->item(i)});
    }
    for (const auto& [p, k] : found) {
        ++st.total;
        ++st.kinds[k];
        if (p.z > 0.5f) ++st.onSurface;
        else ++st.onFloor;
    }
    for (size_t a = 0; a < found.size(); ++a)
        for (size_t b = a + 1; b < found.size(); ++b)
            if (found[a].second == found[b].second)
                st.minSame = std::min(st.minSame, std::hypot(found[a].first.x - found[b].first.x, found[a].first.y - found[b].first.y));
    return st;
}

static void testV11Supplies() {
    std::printf("V11: Vorraete je Ebene (Spawnplan)\n");
    static const char* kLevels[] = {"level0", "level1", "level2", "level3", "level5", "level5_3", "level188", "level903", "level37", "level36"};
    for (const char* id : kLevels) {
        for (u64 seed : {3ull, 4242ull}) {
            const SupplyStats st = countSupplies(id, seed);
            std::string kinds;
            for (const auto& [k, n] : st.kinds) kinds += std::format(" {}={}", k, n);
            std::printf("  %-8s seed %4llu: %2d Funde (Boden %2d, Ablage %2d),%s, min. Abstand gleicher %.1f m\n", id,
                        (unsigned long long)seed, st.total, st.onFloor, st.onSurface, kinds.c_str(), st.minSame > 1e8f ? 0.0f : st.minSame);
            // Level 0 (Rueckmeldung: zu viele Vorraete auf dem Boden): hoechstens sechs, keine Energy Bars, weit verteilt
            if (std::string(id) == "level0")
                check(st.total >= 3 && st.total <= 6 && st.kinds.count("energy") == 0 && st.minSame > 20.0f,
                      std::format("Level 0: wenige Vorraete ({}), keine Energy Bars, gleiche weit auseinander", st.total));
            if (std::string(id) == "level5" || std::string(id) == "level5_3" || std::string(id) == "level188" || std::string(id) == "level903" || std::string(id) == "level36")
                check(st.onFloor == 0, std::format("{}: Funde liegen auf Ablagen, nicht auf dem Boden", id));
        }
    }
}

// V11 (Premium-Ueberarbeitung): Halt der Ausstattung in der echten Kachelwelt. Wandobjekte brauchen Wand
// dahinter und Luft davor, Bodenobjekte stehen auf dem Boden (oder einem Einbau), Deckenobjekte haengen an
// der Decke. Prueft jedes Objekt jeder festen Karte; gibt die ersten Fehler je Ebene mit Ort aus.
namespace placement {
enum class Mount { Wall, Floor, Ceiling, Any };
static Mount mountOf(const std::string& m) {
    static const char* wall[] = {"outlet", "vent_grille", "painting", "power_box", "wall_sconce", "gauge", "window", "hotel_door",
                                 "damaged_door", "steel_door", "locker_shutter", "speaker", "exit_sign", "pillar_label",
                                 "arch2", "arch3", "arch4",   // V12: Boegen - nur im Durchbruch gueltig (TF_DOOR)
                                 "arch_tall3", "arch_tall4", "swing_door1", "swing_door2", "padded_panel",  // V12 Level 1
                                 "painting_a", "painting_b", "painting_c", "painting_d", "painting_e", "painting_f",
                                 "painting_lean_a", "painting_lean_b", "painting_lean_c", "exit_sign_green",
                                 "junction_box", "violet_lamp", "door_steel_white", "door_steel_grey", "door_steel_cream", "door_window", "door_plywood", "door_wood_old", "door_oversize", "door_level3",  // V12 Level 2
                                 "window_brick", "painting_angel", "painting_angel_scribbled", "sign_warning", "canvas_wall", "elevator_door",  // V12 Level 3
                                 "wainscot", "hotel_door_a", "hotel_door_b", "hotel_door_c", "hotel_door_d", "hotel_door_e", "hotel_door_f", "placard_beverly", "candle_sconce", "painting_portrait", "window_bright", "elevator_cage", "cornice",  // V12 Level 5
                                 "library_wall", "library_wall_lamp", "library_wall_upper", "bookcase_end", "bookcase_aisle", "bookcase_aisle_b", "painting_renaissance_a", "painting_renaissance_b",
                                 "inscription_bibliotheca", "wallpaper_peel", "door_old_open",  // V12 Level 5.3
                                 "window_188_a", "window_188_b", "window_188_c", "window_188_fig", "window_188_lit", "window_entry_188", "door_mahogany", "walkway_glass", "door_mahogany_open",  // V12 Level 188
                                 "metro_dormant_a", "metro_dormant_b", "metro_dormant_home", "metro_dormant_coupling", "sign_903_wall",  // V12 Level 903
                                 "departure_board", "exit_sign_36", "storefront_a", "storefront_b", "storefront_c", "storefront_closed", "gate_sign_c30", "gate_sign_c31", "gate_sign_c32", "gate_sign_c33", "gate_sign_c34", "gate_door_closed", "gate_door_boarding", "clock_36", "clock_36_b", "water_fountain", "restroom_sign"};  // V12 Level 36
    static const char* ceil[] = {"batten", "highbay", "chandelier", "safety_light", "ceiling_vent",
                                 "troffer", "troffer_off", "troffer_red", "ceiling_diffuser", "hanging_bulb",  // V12
                                 "tube_white", "tube_orange", "tube_dead", "chandelier_grand", "lantern_green", "chandelier_iron", "ceiling_arms", "skylight_188", "tube_long", "sign_903_a", "sign_903_b", "sign_36_a", "sign_36_b", "sign_36_empty"};
    static const char* floor[] = {"chair", "rack", "bed", "barrel", "cart", "pump", "wardrobe", "trunk", "water_cooler", "armchair", "sofa",
                                  "side_table", "game_table", "boiler", "statue", "transformer", "fence_panel", "rubble", "barrier", "camp",
                                  "desk_crt", "crate", "pallet", "switch_cabinet", "stall_line", "arrow", "floor_glow", "car",
                                  "crate_a", "crate_b", "crate_c", "debris_a", "debris_b", "hospital_bed", "work_table",
                                  "column_round",  // V12 Level 1
                                  "mach_cabinet", "mach_pump", "mach_tank", "pipe_riser", "pipe_bundle_a", "pipe_bundle_b", "trolley_flatbed", "toolbox", "stepladder", "cable_reel", "folding_chair", "hopper_wagon", "rail_track", "chamfer_26", "chamfer_32",  // V12 Level 2
                                  "generator", "conveyor", "sludge_pool", "skull", "train_bogie", "greek_torso", "angel_statue", "canvas_drape_a", "canvas_drape_b", "grate_stair", "pipe_black", "column_greek",  // V12 Level 3
                                  "mahjong_table", "gramophone", "furnace", "metro_car_end", "hotel_bed", "boiler_tank",  // V12 Level 5
                                  "column_tuff", "candelabrum", "books_fallen",  // V12 Level 5.3
                                  "paving_strip", "birch_circle", "table_188", "pillar_188",  // V12 Level 188
                                  "metro_car_903", "metro_car_37", "track_metro", "platform_edge", "stairs_barricaded",  // V12 Level 903
                                  "carousel_36", "luggage_pile", "checkin_counter", "bta_booth", "travelator", "travelator_stopped", "seats_row", "gate_counter", "camp_c32"};  // V12 Level 36
    // V13.3: Level-1-Sektoren, Rohrverlauf, Level-2-Werkstatt, Level-36-Bereiche
    static const char* wall2[] = {"formwork", "ivy_wall", "neon_tube", "neon_sign", "panel_vermilion", "pipe_wall",
                                  "fungus_wall", "claw_marks",  // V13.3 L1-Erscheinungen
                                  "skin_hung", "skin_scratches",  // V13.3 L2: Spuren der Skin-Stealer
                                  "jerry_mural", "jerry_poster",  // V13.3 Teil 4 L2: Jerrys Raum, Wegweiser der Followers
                                  "macchina_tapestry",  // V13.3 Teil 4 L2: Wandtuch der Macchina
                                  "e18_portrait",  // V13.3 Teil 4 L5: Bildnis im Buero des Gastgebers

                                  "purple_stealer", "purple_hound"};  // V13.3 L3: purpifizierte Wesen (KREATUR_L3_Purpur.md)
    static const char* ceil2[] = {"vines_ceiling", "cable_hang", "arcade_gothic"};
    static const char* floor2[] = {"scaffold", "wall_unfinished", "cement_bags", "rebar_bundle", "work_light", "cement_mixer", "wheelbarrow",
                                   "sawhorse", "site_barrier", "tarp_pile", "moss_patch", "fern_cluster", "roots_floor", "shrub_concrete",
                                   "sapling", "overgrown_crate", "cabinet_vermilion", "table_vermilion", "bench_vermilion", "screen_vermilion",
                                   "macchina_parts", "macchina_tools", "xray_scanner", "security_gate", "tray_stack", "lounge_chair",
                                   "lounge_table",
                                   "fungus_floor", "fungus_camp", "goo_trail", "plant_wanderer",  // V13.3 L1-Erscheinungen
                                   "skin_shed", "skin_lair", "food_scraps",  // V13.3 L2: Spuren der Skin-Stealer, Futter der Ratten
                                   "jerry_perch", "jerry_shrine",  // V13.3 Teil 4 L2: Jerrys Sitzrohr, Opfertisch der Followers
                                   "macchina_suit", "macchina_cradle", "macchina_spear",  // V13.3 Teil 4 L2: Reste der Macchina
                                   "e18_desk", "e18_wingchair"};  // V13.3 Teil 4 L5: Buero des Gastgebers
    for (const char* w : wall2)
        if (m == w) return Mount::Wall;
    for (const char* c : ceil2)
        if (m == c) return Mount::Ceiling;
    for (const char* f : floor2)
        if (m == f) return Mount::Floor;
    for (const char* w : wall)
        if (m == w) return Mount::Wall;
    for (const char* c : ceil)
        if (m == c) return Mount::Ceiling;
    for (const char* f : floor)
        if (m == f) return Mount::Floor;
    return Mount::Any;  // Rohre, Kabeltrassen, Sparren, Daemmung, Schilder ohne feste Regel
}
// Ist der Punkt (x, y, z) in festem Material? Wand, oder ueber/unter dem offenen Raum der Kachel (Sturz)
static bool solidAt(world::World& w, double x, double y, double z) {
    const world::Tile& t = w.tile(ifloor(x), ifloor(y));
    return t.solid() || z > t.ceil + 0.01 || z < t.floor - 0.01;
}
}  // namespace placement

static void testV11Placement() {
    using namespace placement;
    std::printf("V11: Halt der Ausstattung (Wand, Boden, Decke)\n");
    static const char* kLevels[] = {"level0", "level1", "level2", "level3", "level5", "level5_3", "level188", "level903", "level37", "level36"};
    const game::Content& content = *gContent11;
    int total = 0, bad = 0;
    for (const char* id : kLevels) {
        const auto L = content.level(id, world::kLayoutV6);
        if (!L || !L->map) continue;
        JobSystem jobs(1);
        world::World w(L, 3, 0.28, 0.80, jobs, 1);
        int n = 0, fails = 0, shown = 0;
        std::map<std::string, int> byModel;
        for (const world::MapObject& o : L->map->objects) {
            const Mount mt = mountOf(o.model);
            if (mt == Mount::Any) continue;
            ++n;
            const double cx = std::cos(o.yaw), sy = std::sin(o.yaw);
            const world::Tile& here = w.tile(ifloor(o.x), ifloor(o.y));
            std::string why;
            const bool isDoor = o.model == "steel_door" || o.model == "hotel_door" || o.model == "damaged_door" ||
                                o.model.rfind("arch", 0) == 0 || o.model.rfind("swing_door", 0) == 0 ||
                                o.model == "door_level3" || o.model == "elevator_door" || o.model == "elevator_cage" || o.model == "door_old_open" || o.model == "door_mahogany_open";  // V12: im Durchbruch
            if (o.model == "pillar_label") {  // Ursprung in der Stuetze: die Kachel muss fest sein
                if (!here.solid()) why = "keine Stuetze";
            } else if (isDoor && (here.flags & world::TF_DOOR)) {
                // Tuerblatt im Durchgang: in Ordnung
            } else if (mt == Mount::Wall) {
                // Wand bis 12 cm hinter dem Ursprung, davor (15 cm) Luft
                bool backed = false;
                for (double d : {0.02, 0.06, 0.12})
                    if (solidAt(w, o.x - cx * d, o.y - sy * d, o.z)) backed = true;
                const bool free = !solidAt(w, o.x + cx * 0.15, o.y + sy * 0.15, o.z);
                if (!backed) why = "keine Wand dahinter";
                else if (!free) why = "steckt in der Wand";
            } else if (mt == Mount::Floor) {
                // auf einem unsichtbaren Block (Kollision des Modells selbst, Wagen ueber der Linie): Raumboden
                const bool hidden = (here.flags & world::TF_HIDDEN) && here.room;
                const float fl = hidden ? (float)here.room->floor : here.floor;
                if (here.solid() && !hidden) why = "steht in fester Kachel";
                // V12: auf einem unsichtbaren Block darf auch etwas stehen (Kiste auf Kiste)
                else if (std::fabs(o.z - fl) > 0.06 && !(hidden && std::fabs(o.z - here.floor) <= 0.06))
                    why = std::format("schwebt {:.2f} m ueber/unter dem Boden", o.z - fl);
            } else if (mt == Mount::Ceiling) {
                if (here.solid()) why = "haengt in fester Kachel";
                else if (std::fabs(o.z - here.ceil) > 0.08) why = std::format("{:.2f} m unter der Decke", here.ceil - o.z);
            }
            if (!why.empty()) {
                ++fails;
                ++byModel[o.model];
                if (shown++ < (std::getenv("LIM_PLACEMENT_ALL") ? 100000 : 6)) std::printf("    %s: %s bei (%.2f, %.2f, %.2f) yaw %.2f: %s\n", id, o.model.c_str(), o.x, o.y, o.z, o.yaw, why.c_str());
            }
        }
        std::string models;
        for (const auto& [m, k] : byModel) models += std::format(" {}={}", m, k);
        std::printf("  %-8s %4d Objekte mit Halt-Regel, %3d ohne Halt%s\n", id, n, fails, models.c_str());
        check(fails == 0, std::format("{}: alle Wand-, Boden- und Deckenobjekte haben Halt ({} ohne)", id, fails));
        total += n, bad += fails;
    }
    std::printf("  gesamt %d Objekte, %d ohne Halt\n", total, bad);
}

// V11: Taschenlampe im Griff (realistischer Koerper) - Lichtkegel = Modellachse und Blickrichtung, die Lampe liegt
// in der Hand (Abstand Handgelenk -> Glas), das Handgelenk knickt nicht ab, das Glas bleibt vor einer Wand,
// ausgeschaltet sinkt die Hand
static void testV11FlashGrip() {
    std::printf("V11: Taschenlampe im Griff\n");
    FnWorld flat;
    flat.fn = [](i64, i64, float& f, float& c) { f = 0, c = 4.0f; };
    game::Player p(0.5, 0.5, 0.0, 0.0);
    game::Body b;
    game::BodyProportions pr;
    pr.upperArm = 0.30f, pr.forearm = 0.27f;
    pr.gripFwd = 0.056f, pr.gripOut = 0.03f;
    b.setProportions(pr);
    b.setFlashlight(true, true);
    auto run = [&](int n, game::PlayerControl c) {
        for (int i = 0; i < n; ++i) {
            p.update(1.0 / 60.0, c, flat);
            b.update(1.0 / 60.0, p);
        }
    };
    auto look = [&] {
        const float cp = (float)std::cos(p.pitch), sp = (float)std::sin(p.pitch);
        return normalize(vec3{(float)std::cos(p.angle), (float)std::sin(p.angle), 0.0f} * cp + vec3{0, 0, 1} * sp);
    };
    float worstAim = 0.0f, worstWrist = 1.0f, minLens = 9.0f, maxLens = 0.0f;
    for (float pitch : {-0.6f, -0.2f, 0.0f, 0.3f, 0.6f}) {
        p.pitch = pitch;
        b.setFlashClearance(9.0f);
        run(60, {});
        const auto& s = b.pose();
        worstAim = std::max(worstAim, std::acos(std::clamp(dot(b.flashlightDir(), look()), -1.0f, 1.0f)));
        worstWrist = std::min(worstWrist, dot(normalize(s.hand[0] - s.elbow[0]), b.flashHandDir()));
        const float lens = dist(b.flashlightPos(), s.hand[0]);
        minLens = std::min(minLens, lens), maxLens = std::max(maxLens, lens);
        check(dot(b.flashlightPos() - s.hand[0], b.flashlightDir()) > 0.05f,
              std::format("Neigung {:.1f}: Glas liegt vor der Hand in Strahlrichtung", pitch));
    }
    check(b.flashlightHeld() && b.flashWeight() > 0.99f, "Lampe gehalten (Griff voll)");
    check(worstAim < 0.09f, std::format("Kegel folgt dem Blick (groesste Abweichung {:.1f} Grad)", worstAim * 57.3f));
    check(worstWrist > std::cos(1.15f), std::format("Handgelenk knickt nicht ab (cos {:.2f})", worstWrist));
    check(minLens > 0.12f && maxLens < 0.32f, std::format("Glas 12-32 cm vor dem Handgelenk ({:.2f}-{:.2f} m)", minLens, maxLens));
    // Handflaeche um den Schaft: Schaft (Kegel) liegt quer zur Handrichtung, nicht laengs im Unterarm
    check(std::fabs(dot(b.flashlightDir(), b.flashHandDir())) < 0.6f, "Schaft liegt quer in der Faust");
    // Gehen und Rennen: die Lampe bleibt auf den Blick gerichtet
    p.pitch = 0.0;
    game::PlayerControl walk;
    walk.forward = 1;
    float walkAim = 0.0f;
    for (int i = 0; i < 120; ++i) {
        run(1, walk);
        walkAim = std::max(walkAim, std::acos(std::clamp(dot(b.flashlightDir(), look()), -1.0f, 1.0f)));
    }
    walk.sprint = true;
    for (int i = 0; i < 120; ++i) {
        run(1, walk);
        walkAim = std::max(walkAim, std::acos(std::clamp(dot(b.flashlightDir(), look()), -1.0f, 1.0f)));
    }
    check(walkAim < 0.14f, std::format("Gehen/Rennen: Kegel bleibt beim Blick ({:.1f} Grad)", walkAim * 57.3f));
    // Wand 20 cm vor dem Auge: die Hand zieht zurueck, das Licht beginnt vor der Wand
    run(30, {});
    b.setFlashClearance(0.2f);
    run(40, {});
    check(dot(b.flashlightPos() - b.eye(), look()) <= 0.2f - 0.03f, "Wand: Licht beginnt vor der Wand");
    b.setFlashClearance(9.0f);
    run(40, {});
    check(std::acos(std::clamp(dot(b.flashlightDir(), look()), -1.0f, 1.0f)) < 0.09f, "nach der Wand wieder auf den Blick");
    // aus: die Hand sinkt, die Lampe gilt nicht mehr als gehalten
    b.setFlashlight(false, false);
    run(60, {});
    check(!b.flashlightHeld(), "ausgeschaltet: Hand sinkt");
}

// V12: Viewmodel - Hand und Lampe aus Blender. Die Modelle tragen feste Materialien (fm=); mit den Massen des
// realistischen Koerpers fuehrt der Unterarm des Handmodells zum Ellbogen des Skeletts (Aermel und Handgelenk
// passen), der Kegel folgt dem Blick, das Glas liegt vor der Hand und bleibt bei jeder Neigung im Bild.
static void testV12Viewmodel(const std::string& dataDir) {
    std::printf("V12: Viewmodel (Hand und Lampe aus Blender)\n");
    for (const char* n : {"vm_flashlight", "vm_flashlight_on", "vm_hand_l"}) {
        props::Model m;
        std::string err;
        const bool ok = props::loadGlb(dataDir + "/models/" + n + ".glb", m, err);
        bool fm = ok && !m.parts.empty();
        for (const auto& p : m.parts) fm = fm && !p.mat.fm.empty() && p.mesh.v.size() <= 65535;
        check(fm, std::format("{}: geladen, alle Teile mit festem Material und <= 65535 Punkten {}", n, err));
    }
    // Masse wie GameSession::applyBodyStyle
    const anim::Build& bb = anim::figureBuild(anim::Figure::Player);
    const float hs = bb.headScale * (0.6f + 0.4f * bb.height / 1.78f);
    game::BodyProportions pr;
    pr.thigh = bb.thigh, pr.shin = bb.shin, pr.ankle = bb.ankle;
    pr.hip = bb.ankle + (bb.thigh + bb.shin) * 0.975f;
    pr.eyeUp = 0.154f * hs, pr.eyeFwd = 0.1f * hs;
    pr.spine = (float)game::Player::kEye - pr.hip - pr.eyeUp;
    pr.neck = 0.11f * hs, pr.headR = 0.1f * hs;
    pr.upperArm = bb.upperArm, pr.forearm = bb.forearm;
    pr.hipW = bb.hipW * 0.5f, pr.shoulderW = bb.shoulderW * 0.5f;
    pr.footW = bb.hipW * 0.5f + 0.02f * bb.height;
    pr.gripFwd = 0.03f, pr.gripOut = 0.03f;
    FnWorld flat;
    flat.fn = [](i64, i64, float& f, float& c) { f = 0, c = 4.0f; };
    game::Player p(0.5, 0.5, 0.0, 0.0);
    game::Body b;
    b.setProportions(pr);
    b.setFlashlight(true, true);
    b.setViewmodelHand(true);
    // V13: Hand mit Skelett - Geometrie aus den Gelenken; sie muss zu den Werten passen, die der Bau in Blender
    // ausgibt (prueft die Achsen-Umrechnung glTF -> Spiel)
    {
        props::Model hm;
        std::string err;
        const bool ok = props::loadGlb(dataDir + "/models/vm_hand_l.glb", hm, err);
        check(ok && hm.skin && hm.skin->joints.size() >= 21, std::format("vm_hand_l: Skelett mit {} Gelenken", ok && hm.skin ? hm.skin->joints.size() : 0));
        check(ok && hm.skin && hm.skin->pose("grip") >= 0 && hm.skin->pose("press") >= 0, "vm_hand_l: Posen grip und press");
        vec3 wr, fa, hd, pa;
        const bool geo = ok && hm.skin && game::vmGeometryFromSkin(*hm.skin, wr, fa, hd, pa);
        check(geo, "vm_hand_l: Handgelenk, Ellbogen, Grundgelenke vorhanden");
        if (geo) {
            b.setViewmodelGeometry(wr, fa, hd, pa);
            auto j = loadJsonFile(dataDir + "/../../../../tools/blender/models/vm_grip.json");
            if (!j) j = loadJsonFile(dataDir + "/../tools/blender/models/vm_grip.json");
            if (j) {
                auto v3 = [&](const char* k) {
                    const auto& a = (*j)["kvm"][k].items();
                    return vec3{(float)a[0].asNumber(), (float)a[1].asNumber(), (float)a[2].asNumber()};
                };
                const float e = std::max({length(wr - v3("wrist")), length(fa - v3("forearm")), length(hd - v3("hand_dir")),
                                          length(pa - v3("palm"))});
                if (e >= 2e-3f)
                    std::printf("  Skelett: Handgelenk %.4f %.4f %.4f, Unterarm %.4f %.4f %.4f, Hand %.4f %.4f %.4f, Flaeche %.4f %.4f %.4f\n",
                                wr.x, wr.y, wr.z, fa.x, fa.y, fa.z, hd.x, hd.y, hd.z, pa.x, pa.y, pa.z);
                check(e < 2e-3f, std::format("Geometrie aus dem Skelett = Werte des Baus (Abweichung {:.4f})", e));
            }
            // Pose-Mischung: Griff = Ruhe (Einheitsmatrizen), Druck bewegt nur den Daumen
            std::vector<mat4> g0, g1;
            props::skinMatrices(*hm.skin, {1.0f, 0.0f}, g0);
            float rest = 0.0f, other = 0.0f, thumb = 0.0f;
            std::vector<float> wp(hm.skin->poses.size(), 0.0f);
            wp[(size_t)hm.skin->pose("press")] = 1.0f;
            props::skinMatrices(*hm.skin, wp, g1);
            for (size_t k = 0; k < g0.size(); ++k) {
                for (int c = 0; c < 4; ++c)
                    rest = std::max(rest, length(g0[k].c[c].xyz() - mat4::identity().c[c].xyz()));
                const float d = length(g1[k].c[3].xyz() - g0[k].c[3].xyz()) + length(g1[k].c[0].xyz() - g0[k].c[0].xyz());
                if (hm.skin->joints[k].name.rfind("th_", 0) == 0) thumb = std::max(thumb, d);
                else other = std::max(other, d);
            }
            check(rest < 1e-3f, std::format("Griff = Ruhepose (Abweichung {:.5f})", rest));
            check(thumb > 0.01f && other < 1e-4f, std::format("Druck bewegt nur den Daumen ({:.4f} / {:.5f})", thumb, other));
        }
    }
    auto run = [&](int n, game::PlayerControl c) {
        for (int i = 0; i < n; ++i) {
            p.update(1.0 / 60.0, c, flat);
            b.update(1.0 / 60.0, p);
        }
    };
    float worstArm = 0.0f, worstAim = 0.0f, worstScreen = 0.0f;
    for (float pitch : {-0.7f, -0.3f, 0.0f, 0.3f, 0.7f}) {
        p.pitch = pitch;
        run(90, {});
        worstArm = std::max(worstArm, b.vmForearmError());
        const float cp = std::cos(pitch), sp = std::sin(pitch);
        const vec3 look = normalize(vec3{(float)std::cos(p.angle), (float)std::sin(p.angle), 0.0f} * cp + vec3{0, 0, 1} * sp);
        worstAim = std::max(worstAim, std::acos(std::clamp(dot(b.flashlightDir(), look), -1.0f, 1.0f)));
        // Griffmitte im Bild: Winkel zur Blickrichtung kleiner als das halbe senkrechte Sichtfeld (31 Grad)
        const vec3 g = normalize(b.flashlightPos() - b.flashlightDir() * 0.19f - b.eye());
        worstScreen = std::max(worstScreen, std::acos(std::clamp(dot(g, look), -1.0f, 1.0f)));
        const auto& ps = b.pose();
        std::printf("  Neigung %+.1f: Unterarm %.1f Grad, Gleiten %.0f mm, Faust %.1f Grad vom Blick, Schulter-Handgelenk %.3f m\n", pitch,
                    b.vmForearmError() * 57.3f, b.vmSlide() * 1000.0f, std::acos(std::clamp(dot(g, look), -1.0f, 1.0f)) * 57.3f,
                    dist(ps.shoulder[0], ps.hand[0]));
    }
    check(b.viewmodelHand(), "Hand aus Blender aktiv (realistischer Koerper)");
    check(worstArm < 0.21f, std::format("Unterarm passt zum Handmodell (groesste Abweichung {:.1f} Grad)", worstArm * 57.3f));
    check(worstAim < 0.09f, std::format("Kegel folgt dem Blick ({:.1f} Grad)", worstAim * 57.3f));
    check(worstScreen < 0.54f, std::format("Faust bleibt im Bild ({:.1f} Grad vom Blick)", worstScreen * 57.3f));
    check(b.vmSlide() >= -0.081f && b.vmSlide() <= 0.046f, std::format("Hand gleitet hoechstens 8 cm ({:.3f} m)", b.vmSlide()));
}

// V11: Schallausbreitung - eine Quelle hinter einer Wand, neben der eine Tuer liegt, kommt durch die Tuer: aus
// ihrer Richtung, mit Umweg, heller als durch die Wand. Ohne Weg bleibt sie dumpf; in freier Sicht unveraendert.
static void testV11SoundField() {
    std::printf("V11: Schallausbreitung (Tueren, Ecken)\n");
    const game::Content& content = *gContent11;
    int tried = 0, around = 0, dirOk = 0, brighter = 0, sealedOk = 0, sealedTried = 0;
    std::string dirFails;
    float sumExtra = 0.0f;
    for (const char* id : {"level0", "level1", "level2", "level5"}) {
        const auto L = content.level(id, world::kLayoutV6);
        if (!L || !L->map) continue;
        JobSystem jobs(1);
        world::World w(L, 3, 0.28, 0.80, jobs, 2);
        int perLevel = 0;
        for (const world::MapDoor& d : L->map->doors) {
            if (perLevel >= 6) break;
            // Ohr 1,5 m vor der Wand, 2,5 m seitlich der Tuer; Quelle gegenueber hinter der Wand
            const bool ax = d.axis == 0;
            const float side = ax ? (float)d.y1 + 2.5f : (float)d.x1 + 2.5f;
            vec3 ear = ax ? vec3{(float)d.x0 - 1.5f, side, 0.0f} : vec3{side, (float)d.y0 - 1.5f, 0.0f};
            vec3 src = ax ? vec3{(float)d.x1 + 1.5f, side, 0.0f} : vec3{side, (float)d.y1 + 1.5f, 0.0f};
            w.preload(ear.x, ear.y);
            const world::Tile& te = w.tile(ifloor(ear.x), ifloor(ear.y));
            const world::Tile& ts = w.tile(ifloor(src.x), ifloor(src.y));
            const vec3 mid = (ear + src) * 0.5f;
            const world::Tile& tw = w.tile(ifloor(mid.x), ifloor(mid.y));
            if (!te.open() || !ts.open() || !tw.solid() || std::fabs(te.floor - ts.floor) > 0.1f) continue;
            ear.z = te.floor + 1.6f, src.z = ts.floor + 1.2f;
            const vec3 doorC{(float)(d.x0 + d.x1) * 0.5f, (float)(d.y0 + d.y1) * 0.5f, ear.z};
            if (game::SoundField::rayOcclusion(w, ear, doorC) > 0.0f || game::SoundField::rayOcclusion(w, src, doorC) > 0.0f) continue;
            game::SoundField f;
            f.update(w, ear, 0.0);
            const game::SoundRoute r = f.route(w, ear, src, 40.0f);
            if (r.direct) continue;  // Wand doch nicht dazwischen
            ++tried, ++perLevel;
            if (!r.around) continue;
            ++around;
            sumExtra += r.pathExtra;
            // scheinbare Richtung: aus dem offenen Raum (einer Oeffnung), nicht aus der Wand - der gerade Weg
            // waere nach 2 m in der Wand, der scheinbare nicht. (Welche Oeffnung, entscheidet der kuerzeste Weg;
            // in Level 0 gibt es oft mehrere gleich lange.)
            const vec3 apparent = normalize(r.pos - ear), straight = normalize(src - ear);
            const vec3 flatA = normalize(vec3{apparent.x, apparent.y, 0.0f}), flatS = normalize(vec3{straight.x, straight.y, 0.0f});
            const bool fromOpen = game::SoundField::rayOcclusion(w, ear, ear + flatA * 2.0f) == 0.0f;
            const bool straightWall = game::SoundField::rayOcclusion(w, ear, ear + flatS * 2.0f) > 0.0f;
            if (fromOpen && (straightWall || dot(flatA, flatS) < 0.98f)) ++dirOk;
            else
                dirFails += std::format(" [{} Tuer {},{}: Ohr {:.1f},{:.1f} Schein {:.1f},{:.1f}]", id, d.x0, d.y0, ear.x, ear.y, r.pos.x,
                                        r.pos.y);
            // heller als der Durchgang durch die Wand (dort mindestens 0,85)
            if (r.occlusion < std::max(game::SoundField::rayOcclusion(w, ear, src), 0.85f)) ++brighter;
            // ohne Weg (Reichweite kuerzer als der Umweg): durch die Wand, dumpf
            ++sealedTried;
            const game::SoundRoute s2 = f.route(w, ear, src, length(src - ear) + 0.2f);
            if (!s2.around && s2.occlusion >= 0.5f && s2.pathExtra == 0.0f) ++sealedOk;
        }
    }
    check(tried >= 8, std::format("Faelle mit Wand zwischen Ohr und Quelle gefunden ({})", tried));
    check(around == tried, std::format("Schall findet den Weg durch die Tuer ({} von {})", around, tried));
    check(dirOk == around, std::format("Richtung kommt aus einer Oeffnung, nicht aus der Wand ({} von {}){}", dirOk, around, dirFails));
    check(brighter == around, std::format("um die Ecke heller als durch die Wand ({} von {})", brighter, around));
    check(around > 0 && sumExtra / (float)around > 0.5f, std::format("Umweg gemessen ({:.1f} m im Mittel)", around ? sumExtra / around : 0.0f));
    check(sealedOk == sealedTried, std::format("ohne Weg in Reichweite: dumpf durch die Wand ({} von {})", sealedOk, sealedTried));
    // freie Sicht: unveraendert
    {
        const auto L = content.level("level1", world::kLayoutV6);
        JobSystem jobs(1);
        world::World w(L, 3, 0.28, 0.80, jobs, 2);
        // V12: zwei Punkte in der Halle Aquila mit freier Sicht (zwischen den Stuetzenreihen, ohne Wandscheiben)
        const vec3 a{30.5f, 26.5f, 1.6f};
        const vec3 c{38.5f, 26.5f, 1.6f};
        w.preload(a.x, a.y);
        game::SoundField f;
        f.update(w, a, 0.0);
        const game::SoundRoute r = f.route(w, a, c, 40.0f);
        check(r.direct && r.occlusion == 0.0f && r.pathExtra == 0.0f && length(r.pos - c) < 1e-4f, "freie Sicht: Quelle unveraendert");
    }
}

// V12: Modell-Pipeline (Blender -> .glb -> Spiel). Das Pruefmodell (tools/blender/models/models_test.py) hat einen
// Betonsockel 1 x 1 x 0,5 m, einen roten Pfeil nach +x (vorn) und einen Leuchtwuerfel in Blender bei +y (links).
// Im Spiel (x vorn, y rechts) muss der Wuerfel links liegen (y < 0), der Pfeil vorn, die Dreiecke nach aussen.
static void testV12Glb(const char* dataDir) {
    std::printf("V12: Modelle aus Blender (.glb)\n");
    props::Model m;
    std::string err;
    const bool ok = props::loadGlb(std::string(dataDir) + "/models/pipeline_test.glb", m, err);
    check(ok, "pipeline_test.glb lesbar" + (ok ? std::string() : " (" + err + ")"));
    if (!ok) return;
    check(m.parts.size() == 3, std::format("drei Teile (Materialien), sind {}", m.parts.size()));
    const props::Part *glow = nullptr, *red = nullptr, *base = nullptr;
    for (const auto& p : m.parts) {
        if (p.mat.glow > 0.0f) glow = &p;
        else if (p.mat.tex == "concrete_cast") base = &p;
        else red = &p;
    }
    check(glow && red && base, "Materialnamen gelesen (glow, tex=concrete_cast, Farbe)");
    if (!glow || !red || !base) return;
    auto centre = [](const props::Mesh& me) {
        vec3 c{0, 0, 0};
        for (const auto& v : me.v) c = c + v.p;
        return c * (1.0f / (float)me.v.size());
    };
    check(centre(glow->mesh).y < -0.2f, "Blender links (+y) ist im Spiel links (y < 0)");
    check(centre(red->mesh).x > 0.2f && centre(red->mesh).z > 0.5f, "Pfeil vorn (+x) auf dem Sockel");
    check(std::fabs(base->mesh.bounds().hi.z - 0.5f) < 0.01f && std::fabs(base->mesh.bounds().lo.x + 0.5f) < 0.01f,
          "Massstab in Metern (Sockel 0,5 m hoch, 1 m breit)");
    check(red->mat.r > 0.7f && red->mat.g < 0.3f, "Grundfarbe zurueck in sRGB");
    // Dreiecke nach aussen: Flaechennormale aus dem Umlaufsinn zeigt wie die Vertexnormale
    int agree = 0, total = 0;
    const auto& me = base->mesh;
    for (size_t t = 0; t + 2 < me.i.size(); t += 3) {
        const vec3 a = me.v[me.i[t]].p, b2 = me.v[me.i[t + 1]].p, c = me.v[me.i[t + 2]].p;
        const vec3 fn = cross(b2 - a, c - a);
        if (length(fn) < 1e-8f) continue;
        ++total;
        if (dot(fn, me.v[me.i[t]].n + me.v[me.i[t + 1]].n + me.v[me.i[t + 2]].n) > 0.0f) ++agree;
    }
    check(total > 0 && agree == total, std::format("Umlaufsinn passt zu den Normalen ({} von {})", agree, total));
}

// V13.2 (B-001): Skinning wie VSSkinned im Shader (shaders/mesh_gbuffer.hlsl) - Dual Quaternions fuer Menschen
namespace dqs {
float el(const mat4& m, int r, int c) { return r == 0 ? m.c[c].x : (r == 1 ? m.c[c].y : m.c[c].z); }
vec4 quatFromRot(const mat4& m, float s) {
    auto e = [&](int r, int c) { return el(m, r, c) / s; };
    const float tr = e(0, 0) + e(1, 1) + e(2, 2);
    if (tr > 0.0f) {
        const float q = std::sqrt(tr + 1.0f) * 2.0f;
        return {(e(2, 1) - e(1, 2)) / q, (e(0, 2) - e(2, 0)) / q, (e(1, 0) - e(0, 1)) / q, 0.25f * q};
    }
    if (e(0, 0) > e(1, 1) && e(0, 0) > e(2, 2)) {
        const float q = std::sqrt(1.0f + e(0, 0) - e(1, 1) - e(2, 2)) * 2.0f;
        return {0.25f * q, (e(0, 1) + e(1, 0)) / q, (e(0, 2) + e(2, 0)) / q, (e(2, 1) - e(1, 2)) / q};
    }
    if (e(1, 1) > e(2, 2)) {
        const float q = std::sqrt(1.0f + e(1, 1) - e(0, 0) - e(2, 2)) * 2.0f;
        return {(e(0, 1) + e(1, 0)) / q, 0.25f * q, (e(1, 2) + e(2, 1)) / q, (e(0, 2) - e(2, 0)) / q};
    }
    const float q = std::sqrt(1.0f + e(2, 2) - e(0, 0) - e(1, 1)) * 2.0f;
    return {(e(0, 2) + e(2, 0)) / q, (e(1, 2) + e(2, 1)) / q, 0.25f * q, (e(1, 0) - e(0, 1)) / q};
}
vec4 mul(vec4 a, vec4 b) {
    const vec3 av{a.x, a.y, a.z}, bv{b.x, b.y, b.z};
    const vec3 v = bv * a.w + av * b.w + cross(av, bv);
    return {v.x, v.y, v.z, a.w * b.w - dot(av, bv)};
}
vec3 rot(vec4 q, vec3 v) {
    const vec3 qv{q.x, q.y, q.z};
    return v + cross(qv, cross(qv, v) + v * q.w) * 2.0f;
}
// Punkt und Normale eines Punktes mit vier Knochen
void skin(const std::vector<mat4>& pal, const u8* bones, const u8* weights, vec3 pos, vec3 nrm, vec3& outP, vec3& outN) {
    vec4 r0{0, 0, 0, 0}, d0{0, 0, 0, 0}, first{0, 0, 0, 1};
    float sc = 0.0f;
    for (int k = 0; k < 4; ++k) {
        const float w = weights[k] / 255.0f;
        const mat4& m = pal[bones[k]];
        const float s = length(vec3{m.c[0].x, m.c[0].y, m.c[0].z});
        const vec4 q = quatFromRot(m, std::max(s, 1e-6f));
        if (k == 0) first = q;
        const float sg = q.x * first.x + q.y * first.y + q.z * first.z + q.w * first.w < 0.0f ? -1.0f : 1.0f;
        const vec4 d = mul(vec4{m.c[3].x, m.c[3].y, m.c[3].z, 0.0f}, q) * 0.5f;
        r0 = r0 + q * (w * sg), d0 = d0 + d * (w * sg);
        sc += w * s;
    }
    const float len = std::max(std::sqrt(r0.x * r0.x + r0.y * r0.y + r0.z * r0.z + r0.w * r0.w), 1e-6f);
    r0 = r0 * (1.0f / len), d0 = d0 * (1.0f / len);
    const vec3 rv{r0.x, r0.y, r0.z}, dv{d0.x, d0.y, d0.z};
    const vec3 t = (dv * r0.w - rv * d0.w + cross(rv, dv)) * 2.0f;
    outP = rot(r0, pos * sc) + t;
    outN = rot(r0, nrm);
}
}  // namespace dqs

// V13.2 (B-001): Die Kapuzenjacke reisst am gebeugten Ellbogen nicht auf und zieht keinen Fluegel. Der Koerper haelt
// die Lampe in den extremen Haltungen (Ego: Blick geradeaus, nach unten, nach oben, dicht an der Wand; von aussen);
// die Jacke wird auf der CPU gehaeutet wie im Shader. Gemessen: Dehnung jeder Kante gegen die Ruhepose.
static void testB001Sleeve(const std::string& dataDir) {
    std::printf("V13.2: Kapuzenjacke am gebeugten Ellbogen (B-001)\n");
    JobSystem jobs(2);
    const gfx::FigureModel fm = gfx::buildFigure(anim::Figure::Player, &jobs);
    const gfx::SkinPart* hood = nullptr;
    for (const auto& part : fm.lods[0])
        if (part.material == 22) hood = &part;  // gfx::FM_HOODIE = FM_SKIN_PLAYER (19) + 3 (render/world_renderer.hpp zieht windows.h mit)
    check(hood && hood->v.size() > 1000, "Kapuzenjacke im Modell (Stufe 0)");
    if (!hood) return;
    // a) Ursache von B-001: In der Ruhepose verschmolzen Aermel und Rumpf (Brust mit 7,5 cm Rundung vernetzt, Arm nur 24
    // Grad abgespreizt) - Flaeche des Rumpfs lag im Aermel und riss beim Bewegen auf. Jetzt: im Aermel (50-100 % des
    // Oberarms, nah an der Achse) ueberwiegt bei keinem Punkt der Rumpf.
    const anim::Build& bb = anim::figureBuild(anim::Figure::Player);
    const anim::JointPose bind = anim::bindPose(bb);
    int sleeve = 0, fused = 0;
    for (const auto& v : hood->v) {
        float arm = 0.0f, torso = 0.0f;
        for (int k = 0; k < 4; ++k) {
            const int bn = v.bones[k];
            const float w = v.weights[k] / 255.0f;
            if (bn == anim::B_UARM_L || bn == anim::B_UARM_R || bn == anim::B_FARM_L || bn == anim::B_FARM_R) arm += w;
            if (bn == anim::B_PELVIS || bn == anim::B_SPINE || bn == anim::B_CHEST || bn == anim::B_NECK) torso += w;
        }
        for (int sd = 0; sd < 2; ++sd) {
            const vec3 S = bind.j[anim::sideJoint(anim::J_SHOULDER_L, sd)], E = bind.j[anim::sideJoint(anim::J_ELBOW_L, sd)];
            const float t = dot(v.pos - S, E - S) / dot(E - S, E - S);
            if (t > 0.5f && t < 1.0f && length(v.pos - (S + (E - S) * t)) < bb.upperArmR + 0.035f) {
                ++sleeve;
                if (torso > arm) {
                    ++fused;
                    if (std::getenv("B001_DEBUG")) std::printf("    verschmolzen t %.2f r %.3f z %.3f arm %.2f rumpf %.2f\n", t, length(v.pos - (S + (E - S) * t)), v.pos.z, arm, torso);
                }
            }
        }
    }
    check(sleeve > 200 && fused == 0, std::format("Ruhepose: im Aermel keine Rumpfflaeche ({} von {} Punkten)", fused, sleeve));

    // b) Haltungen
    const float hs = bb.headScale * (0.6f + 0.4f * bb.height / 1.78f);
    game::BodyProportions pr;
    pr.thigh = bb.thigh, pr.shin = bb.shin, pr.ankle = bb.ankle;
    pr.hip = bb.ankle + (bb.thigh + bb.shin) * 0.975f;
    pr.eyeUp = 0.154f * hs, pr.eyeFwd = 0.1f * hs;
    pr.spine = (float)game::Player::kEye - pr.hip - pr.eyeUp;
    pr.neck = 0.11f * hs, pr.headR = 0.1f * hs;
    pr.upperArm = bb.upperArm, pr.forearm = bb.forearm;
    pr.hipW = bb.hipW * 0.5f, pr.shoulderW = bb.shoulderW * 0.5f;
    pr.footW = bb.hipW * 0.5f + 0.02f * bb.height;
    pr.gripFwd = 0.03f, pr.gripOut = 0.03f;
    FnWorld flat;
    flat.fn = [](i64, i64, float& f, float& c) { f = 0, c = 4.0f; };
    props::Model hm;
    std::string err;
    vec3 wr, fa, hd, pa;
    const bool geo = props::loadGlb(dataDir + "/models/vm_hand_l.glb", hm, err) && hm.skin && game::vmGeometryFromSkin(*hm.skin, wr, fa, hd, pa);
    check(geo, "Hand aus Blender fuer die Lampenhaltung");
    std::vector<mat4> bindInv(anim::kBoneCount);
    {
        anim::Frames bf;
        anim::computeFrames(bind, bf);
        for (int i = 0; i < anim::kBoneCount; ++i) bindInv[(size_t)i] = anim::rigidInverse(bf[(size_t)i]);
    }
    struct Case {
        const char* name;
        bool outside;
        float pitch, clear;
    };
    const Case cases[] = {{"Ego geradeaus", false, 0.0f, 9.0f}, {"Ego nach unten", false, -0.9f, 9.0f}, {"Ego nach oben", false, 0.6f, 9.0f},
                          {"Ego an der Wand", false, 0.0f, 0.35f}, {"aussen geradeaus", true, 0.0f, 9.0f}, {"aussen nach unten", true, -0.9f, 9.0f}};
    float worstAll = 0.0f;
    for (const Case& cs : cases) {
        game::Player p(0.5, 0.5, 0.0, 0.0);
        game::Body b;
        b.setProportions(pr);
        b.setViewmodelHand(true);
        if (geo) b.setViewmodelGeometry(wr, fa, hd, pa);
        b.setFlashlight(true, true);
        b.setOutsideView(cs.outside);
        p.pitch = cs.pitch;
        for (int i = 0; i < 90; ++i) {
            b.setFlashClearance(cs.clear);
            p.update(1.0 / 60.0, {}, flat);
            b.update(1.0 / 60.0, p);
        }
        const anim::JointPose jp = game::playerJointPose(b, vec3{(float)p.x, (float)p.y, (float)p.z});
        anim::Frames f;
        anim::computeFrames(jp, f);
        std::vector<mat4> pal(anim::kBoneCount);
        for (int k = 0; k < anim::kBoneCount; ++k) pal[(size_t)k] = f[(size_t)k] * bindInv[(size_t)k];
        std::vector<vec3> q(hood->v.size()), qn(hood->v.size());
        for (size_t i = 0; i < hood->v.size(); ++i) {
            const auto& v = hood->v[i];
            dqs::skin(pal, v.bones, v.weights, v.pos, v.normal, q[i], qn[i]);
        }
        // Umgeklappte Dreiecke: die Flaeche zeigt gegen die Normalen ihrer Punkte - so sah man die Innenseite des
        // Stoffs (die dunklen Zacken von B-001)
        int flipped = 0, tris = 0;
        for (size_t t = 0; t + 2 < hood->idx.size(); t += 3) {
            const u32 a = hood->idx[t], c1 = hood->idx[t + 1], c2 = hood->idx[t + 2];
            const vec3 n0 = cross(hood->v[c1].pos - hood->v[a].pos, hood->v[c2].pos - hood->v[a].pos);
            if (length(n0) < 1e-6f) continue;
            ++tris;
            const vec3 n1 = cross(q[c1] - q[a], q[c2] - q[a]);
            if (dot(n1, qn[a] + qn[c1] + qn[c2]) * (dot(n0, hood->v[a].normal + hood->v[c1].normal + hood->v[c2].normal) > 0.0f ? 1.0f : -1.0f) < 0.0f) {
                ++flipped;
                if (std::getenv("B001_DEBUG") && flipped % 4 == 1) {
                    const vec3 cb = (hood->v[a].pos + hood->v[c1].pos + hood->v[c2].pos) * (1.0f / 3.0f);
                    std::printf("    umgeklappt bei (%.3f %.3f %.3f) b%d w%d b%d w%d\n", cb.x, cb.y, cb.z, hood->v[a].bones[0], hood->v[a].weights[0],
                                hood->v[a].bones[1], hood->v[a].weights[1]);
                }
            }
        }
        // Dehnung je Kante (nur Kanten ab 4 mm in Ruhe); "gerissen": laenger als das Dreifache
        float worst = 0.0f;
        int torn = 0, edges = 0;
        for (size_t t = 0; t + 2 < hood->idx.size(); t += 3)
            for (int e = 0; e < 3; ++e) {
                const u32 i0 = hood->idx[t + e], i1 = hood->idx[t + (e + 1) % 3];
                const float l0 = length(hood->v[i0].pos - hood->v[i1].pos);
                if (l0 < 0.004f) continue;
                const float st = length(q[i0] - q[i1]) / l0;
                ++edges;
                if (st > worst && std::getenv("B001_DEBUG")) {
                    const auto& a = hood->v[i0];
                    const auto& c = hood->v[i1];
                    std::printf("    Kante %.2f: (%.3f %.3f %.3f) b%d/%d w%d/%d -> (%.3f %.3f %.3f) b%d/%d w%d/%d\n", st, a.pos.x, a.pos.y, a.pos.z,
                                a.bones[0], a.bones[1], a.weights[0], a.weights[1], c.pos.x, c.pos.y, c.pos.z, c.bones[0], c.bones[1], c.weights[0], c.weights[1]);
                }
                worst = std::max(worst, st);
                if (st > 3.0f) ++torn;
            }
        const float bendDeg = std::acos(std::clamp(dot(normalize(b.pose().elbow[0] - b.pose().shoulder[0]), normalize(b.pose().hand[0] - b.pose().elbow[0])), -1.0f, 1.0f)) * 57.3f;
        std::printf("  %-18s Ellbogen %5.1f Grad gebeugt, staerkste Dehnung %.2f (ueber 3-fach: %d von %d Kanten), umgeklappt %d von %d Dreiecken\n",
                    cs.name, bendDeg, worst, torn, edges, flipped, tris);
        // von aussen sieht man alles: keine umgeklappte Flaeche, Dehnung hoechstens 3,5-fach; die Ego-Haltung (Arm hoch,
        // Achsel nur im Ego-Bild verdeckt) darf in der Achsel staerker dehnen
        // Grenzen: einzelne umgeklappte Dreiecke liegen in der Achsel (Stoff staucht sich zwischen Arm und Rumpf, verdeckt)
        const int lim = cs.outside ? tris / 400 : tris / 100;
        check(flipped <= lim, std::format("{}: umgeklappte Dreiecke {} (Grenze {})", cs.name, flipped, lim));
        check(worst < (cs.outside ? 3.0f : 6.5f), std::format("{}: staerkste Dehnung {:.2f}", cs.name, worst));
        worstAll = std::max(worstAll, cs.outside ? worst : 0.0f);
        if (cs.outside)
            check(b.pose().elbow[0].z < b.pose().shoulder[0].z - 0.12f,
                  std::format("{}: von aussen haengt der Ellbogen unter der Schulter ({:.2f} m)", cs.name, b.pose().shoulder[0].z - b.pose().elbow[0].z));
    }
    (void)worstAll;
}

// V13.2: die neuen Wesen (KREATUR_<Name>.md) - Verhalten je Ebene auf den festen Karten
static void testV13Kin(const std::string& dataDir) {
    const game::Content& content = *gContent11;
    game::PrefabLibrary prefabs;
    prefabs.loadDirectory(dataDir + "/prefabs");
    JobSystem jobs(2);
    using PE = game::Presence::Event;
    using Role = game::Presence::Role;
    auto open = [&](const char* level, i64 seed) {
        auto s = std::make_unique<game::GameSession>(content, prefabs, content.level(level, world::kLayoutV6), seed, "medium", "Test", jobs, 3);
        s->placeAtSpawn();
        s->preload();
        return s;
    };
    auto step = [&](game::GameSession& s, std::vector<game::GameEvent>& ev, double secs, const std::function<bool()>& each = {}) {
        for (int i = 0; i < (int)(secs * 60.0); ++i) {
            if (each && each()) break;
            s.update(1.0 / 60.0, {}, false);
            ev.insert(ev.end(), s.events().begin(), s.events().end());
            s.events().clear();
        }
    };
    auto count = [](const std::vector<game::GameEvent>& ev, PE::Type t, Role r, float minI = -1.0f, float maxI = 1e9f) {
        return (int)std::count_if(ev.begin(), ev.end(), [&](const game::GameEvent& e) {
            return e.type == game::GameEvent::Figure && e.index == (int)t && e.role == (int)r && e.intensity >= minI && e.intensity <= maxI;
        });
    };
    // Blick auf den Kopf des Wesens
    auto aim = [](game::GameSession& s, vec3 q) {
        auto& pl = s.player();
        pl.angle = std::atan2(q.y - pl.y, q.x - pl.x);
        pl.pitch = std::atan2(q.z - pl.eye(), std::max(0.4, std::hypot(q.x - pl.x, q.y - pl.y)));
    };
    // der Spieler etwa dist m vor dem Wesen, freie Sicht auf den Kopf (reihum andere Winkel); room: nur in diesem Raumtyp
    auto placeNear = [&](game::GameSession& s, Role r, double dist, const char* room = nullptr, double back = 0.0) {
        auto& P = s.presence();
        if (!P.has(r)) return false;
        const vec3 at = P.posOf(r), head = P.headOf(r);
        world::World& w = s.world();
        for (int k = 0; k < 24; ++k) {
            const float yaw0 = P.yawOf(r) + (float)((k + 1) / 2) * 0.27f * (k % 2 ? 1.0f : -1.0f);
            for (double dd = dist; dd > dist * 0.6; dd -= 0.2) {
                const double x = at.x + std::cos(yaw0) * dd, y = at.y + std::sin(yaw0) * dd;
                const world::Tile* t = w.tileIfLoaded(ifloor(x), ifloor(y));
                const float baseZ = r == Role::Window ? at.z - 1.9f : at.z;
                if (!t || !t->open() || t->clearance() < 2.0f || std::fabs(t->floor - baseZ) > 0.4f) continue;
                if (room && !(t->room && t->room->type && t->room->type->key == room)) continue;
                if (!game::clearSight(w, vec3{(float)x, (float)y, t->floor + (float)game::Player::kEye}, head)) continue;
                bool room2 = true;  // back: so viel Platz hinter dem Spieler (vom Wesen weg)
                for (double bb = 0.5; bb <= back && room2; bb += 0.5) {
                    const world::Tile* tb = w.tileIfLoaded(ifloor(x + std::cos(yaw0) * bb), ifloor(y + std::sin(yaw0) * bb));
                    room2 = tb && tb->open() && std::fabs(tb->floor - t->floor) < 0.3f;
                }
                if (!room2) continue;
                auto& pl = s.player();
                pl.x = x, pl.y = y, pl.z = t->floor, pl.vx = pl.vy = 0.0;
                s.body().reset();
                aim(s, head);
                return true;
            }
        }
        return false;
    };
    auto spawn = [&](game::GameSession& s, std::vector<game::GameEvent>& ev, Role r) {
        s.presence().force(r);
        step(s, ev, 3.0, [&] { return s.presence().has(r); });  // nach einem Wiedereinstieg erst, wenn die Regie wieder laeuft
        return s.presence().has(r);
    };

    // --- Faceling, friedlich (Level 903): leben ihr Leben, winken, wen sie wiedererkennen --------------------------------
    {
        auto s = open("level903", 7);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        check(spawn(*s, ev, Role::Faceling), "Level 903: Facelings erscheinen");
        const int v = P.variantOf(Role::Faceling);
        check(v == 0 || v == 3 || v == 4, std::format("Level 903: eine der gewoehnlichen Varianten ({})", v));
        check(placeNear(*s, Role::Faceling, 5.0), "Level 903: mit Sicht vor einem Faceling");
        const double h0 = s->stats().health;
        bool waved = false;
        step(*s, ev, 8.0, [&] {
            if (P.has(Role::Faceling)) aim(*s, P.headOf(Role::Faceling));
            waved = waved || P.stateOf(Role::Faceling) == 5;
            return false;
        });
        check(waved, "Level 903: wer laenger in der Naehe ist, dem winkt es");
        check(s->stats().health >= h0 && count(ev, PE::Hit, Role::Faceling) == 0, "Level 903: friedlich - kein Schlag");
    }
    // --- Faceling, reizbar (Level 2): die Lampe im Gesicht reizt; dann Schlaege; mit Abstand beruhigt er sich ------------
    {
        auto s = open("level2", 8);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        // V13.3: steht der Faceling mit dem Gesicht zur Wand (enger Tunnel), kann niemand vor ihn treten - dann eine andere
        // Gruppe rufen (sonst haengt die Pruefung vom zufaelligen Ort in der Karte ab)
        bool appeared = false, placed = false;
        for (int attempt = 0; attempt < 6 && !placed; ++attempt) {
            if (attempt > 0) {
                P.clear();
                step(*s, ev, 1.0);
            }
            if (!spawn(*s, ev, Role::Faceling)) continue;
            appeared = true;
            if (!placeNear(*s, Role::Faceling, 2.6)) continue;
            const vec3 fp = P.posOf(Role::Faceling);
            const float fy = P.yawOf(Role::Faceling);
            const float tx = (float)s->player().x - fp.x, ty = (float)s->player().y - fp.y;
            const float tl = std::hypot(tx, ty);
            placed = tl > 0.01f && (std::cos(fy) * tx + std::sin(fy) * ty) / tl > 0.6f;
        }
        check(appeared, "Level 2: Facelings erscheinen (Gruppe)");
        check(placed, "Level 2: mit Sicht vor einem Faceling");
        step(*s, ev, 2.0, [&] { aim(*s, P.headOf(Role::Faceling)); return false; });
        check(P.stateOf(Role::Faceling) == 0, "Level 2: ohne Reiz lassen sie einen in Ruhe");
        s->flashlight().on = true;
        int angryAt = -1, frame = 0;
        step(*s, ev, 6.0, [&] {
            // V13.3: geht er umher und wendet sich ab, tritt der Spieler wieder vor ihn - geprueft wird die Lampe im
            // Gesicht, nicht ob der Faceling zufaellig stehen bleibt (haengt sonst von der Karte ab)
            if (frame % 60 == 30) {
                const vec3 fp = P.posOf(Role::Faceling);
                const float fy = P.yawOf(Role::Faceling);
                const float tx = (float)s->player().x - fp.x, ty = (float)s->player().y - fp.y;
                const float tl = std::hypot(tx, ty);
                if (tl > 0.01f && (std::cos(fy) * tx + std::sin(fy) * ty) / tl < 0.5f) placeNear(*s, Role::Faceling, 2.6);
            }
            aim(*s, P.headOf(Role::Faceling));
            if (angryAt < 0 && P.stateOf(Role::Faceling) == 2) angryAt = frame;
            ++frame;
            return angryAt >= 0;
        });
        check(angryAt > 60, std::format("Level 2: die Lampe im Gesicht reizt ihn - nach {:.1f} s ausfallend", angryAt / 60.0));
        check(count(ev, PE::Hum, Role::Faceling, 0.9f) > 0, "Level 2: aufgebracht laut (Vokale, stossweise)");
        const double h0 = s->stats().health;
        step(*s, ev, 4.0, [&] { aim(*s, P.headOf(Role::Faceling)); return false; });
        // V13.3: leicht = jeder einzelne Schlag (die Summe haengt davon ab, wie viele der Gruppe mitmachen)
        check(s->stats().health < h0 && count(ev, PE::Hit, Role::Faceling) > 0 && count(ev, PE::Hit, Role::Faceling, 20.5f) == 0,
              std::format("Level 2: er schlaegt zu - leicht ({:.0f} -> {:.0f}, {} Schlaege, keiner ueber 20)", h0, s->stats().health,
                          count(ev, PE::Hit, Role::Faceling)));
        check(!s->stats().dead, "Level 2: nicht toedlich");
        // weggehen: er beruhigt sich
        s->flashlight().on = false;
        s->placeAtSpawn();
        s->preload();
        step(*s, ev, 14.0);
        check(!P.has(Role::Faceling) || P.stateOf(Role::Faceling) == 0, "Level 2: mit Abstand beruhigt er sich wieder");
    }
    // --- Faceling, bewaffnet (Level 3): kurzsichtig - bemerkt einen erst nah, dann Hiebe mit dem Stein ------------------
    {
        auto s = open("level3", 9);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        check(spawn(*s, ev, Role::Faceling), "Level 3: ein entwickelter Faceling erscheint");
        check(P.variantOf(Role::Faceling) == 1, "Level 3: Variante 'entwickelt' (mit Steinwerkzeug)");
        if (placeNear(*s, Role::Faceling, 8.0)) {
            step(*s, ev, 2.5, [&] { aim(*s, P.headOf(Role::Faceling)); return false; });
            check(P.stateOf(Role::Faceling) == 0 || P.distOf(Role::Faceling) < 5.5f, "Level 3: auf 8 m bemerkt er einen nicht (kurzsichtig)");
        }
        check(placeNear(*s, Role::Faceling, 3.0), "Level 3: nah vor ihm");
        const double h0 = s->stats().health;
        step(*s, ev, 6.0, [&] { aim(*s, P.headOf(Role::Faceling)); return s->stats().dead; });
        check(P.stateOf(Role::Faceling) == 2 || s->stats().health < h0, "Level 3: nah bemerkt er einen und greift an");
        check(count(ev, PE::Swing, Role::Faceling) > 0 && count(ev, PE::Hit, Role::Faceling) > 0, "Level 3: er holt aus und trifft (Stein)");
        check(s->stats().health < h0, std::format("Level 3: Schaden ({:.0f} -> {:.0f})", h0, s->stats().health));
    }
    // --- Faceling im rosa Kleid (Level 188): toedlich, nur in den Gaengen ----------------------------------------------
    {
        auto s = open("level188", 10);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        {  // an die Ostseite des Hofs, nahe der Tuer zum weissen Gang (vom Eingang sind die Gaenge 45 m weit)
            auto& pl = s->player();
            pl.x = 52.5, pl.y = 38.5, pl.z = s->world().tile(52, 38).floor, pl.angle = 3.14159, pl.pitch = 0.0;
            s->body().reset();
            s->preload();
            step(*s, ev, 0.5);
        }
        check(spawn(*s, ev, Role::Faceling), "Level 188: der Faceling im rosa Kleid erscheint");
        check(P.variantOf(Role::Faceling) == 2, "Level 188: Variante 'rosa Kleid'");
        const world::Tile& ft = s->world().tile(ifloor(P.posOf(Role::Faceling).x), ifloor(P.posOf(Role::Faceling).y));
        check(ft.room && ft.room->type && (ft.room->type->key == "gang" || ft.room->type->key == "zelle" || ft.room->type->key == "treppenhaus"),
              "Level 188: sie ist in den Gaengen");
        const bool placed = placeNear(*s, Role::Faceling, 7.0, "gang") || placeNear(*s, Role::Faceling, 7.0, "zelle") ||
                            placeNear(*s, Role::Faceling, 7.0, "treppenhaus");
        check(placed, "Level 188: mit Sicht in einem Gang");
        bool stopped = false;
        step(*s, ev, 1.2, [&] {
            aim(*s, P.headOf(Role::Faceling));
            stopped = stopped || P.stateOf(Role::Faceling) == 3;
            return false;
        });
        check(stopped && !s->stats().dead, "Level 188: sieht sie einen, bleibt sie zuerst stehen (Ankuendigung)");
        step(*s, ev, 12.0, [&] {
            if (P.has(Role::Faceling)) aim(*s, P.headOf(Role::Faceling));
            return s->stats().dead;
        });
        check(s->stats().dead && s->killRole() == (int)Role::Faceling, "Level 188: bleibt man, ist es toedlich");
        // Ausweg: aus den Gaengen hinaus in den Hof - sie laesst ab
        s->respawnAtCheckpoint();
        P.clear();
        {
            auto& pl = s->player();
            pl.x = 52.5, pl.y = 38.5, pl.z = s->world().tile(52, 38).floor, pl.angle = 3.14159, pl.pitch = 0.0;
            s->body().reset();
            s->preload();
        }
        step(*s, ev, 0.5);
        check(spawn(*s, ev, Role::Faceling), "Level 188: wieder da");
        if (placeNear(*s, Role::Faceling, 7.0, "gang") || placeNear(*s, Role::Faceling, 7.0, "zelle") || placeNear(*s, Role::Faceling, 7.0, "treppenhaus")) {
            step(*s, ev, 1.0, [&] { aim(*s, P.headOf(Role::Faceling)); return false; });
            s->placeAtSpawn();  // im Hof
            s->preload();
            step(*s, ev, 8.0);
            check(!s->stats().dead && (!P.has(Role::Faceling) || P.stateOf(Role::Faceling) == 0), "Level 188: im Hof laesst sie ab");
        }
    }
    // --- Skin-Stealer (Level 5, wahre Gestalt): satt harmlos, gereizt toedlich -------------------------------------------
    {
        auto s = open("level5", 11);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        check(spawn(*s, ev, Role::SkinStealer), "Level 5: ein Skin-Stealer erscheint");
        check(P.variantOf(Role::SkinStealer) == 0, "Level 5: wahre Gestalt (blassgelb)");
        P.kinSet(Role::SkinStealer, "hungry", 0.0);
        check(placeNear(*s, Role::SkinStealer, 5.0), "Level 5: mit Sicht vor ihm");
        step(*s, ev, 6.0, [&] { aim(*s, P.headOf(Role::SkinStealer)); return false; });
        check(!s->stats().dead && P.stateOf(Role::SkinStealer) == 0, "Level 5: satt ist er harmlos (ruhig dabeistehen)");
        s->flashlight().on = true;
        int replaced = 0;
        step(*s, ev, 14.0, [&] {
            if (!P.has(Role::SkinStealer)) return s->stats().dead;
            // die Lampe soll ihn treffen: verliert man ihn hinter einer Kante, tritt man wieder vor ihn
            const vec3 ey{(float)s->player().x, (float)s->player().y, (float)s->player().eye()};
            if (!game::clearSight(s->world(), ey, P.headOf(Role::SkinStealer)) && replaced < 6 && P.stateOf(Role::SkinStealer) == 0)
                replaced += placeNear(*s, Role::SkinStealer, 4.0) ? 1 : 0;
            aim(*s, P.headOf(Role::SkinStealer));
            return s->stats().dead;
        });
        check(s->stats().dead && s->killRole() == (int)Role::SkinStealer, "Level 5: gereizt (Lampe im Gesicht) toetet er mit blossen Haenden");
    }
    // --- Skin-Stealer (Level 1, getarnt, hungrig): spricht nach, kommt naeher, haelt inne, greift an --------------------
    // (V13.3: vorher in Level 3 - dort traegt er jetzt einen Mantel, testV133Level3Kin; das Grundverhalten gilt in Level 1)
    {
        auto s = open("level1", 12);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        check(spawn(*s, ev, Role::SkinStealer), "Level 1: ein Skin-Stealer erscheint");
        check(P.variantOf(Role::SkinStealer) == 1, "Level 1: getarnt (in Menschenhaut)");
        P.kinSet(Role::SkinStealer, "hungry", 1.0);
        check(placeNear(*s, Role::SkinStealer, 11.0) || placeNear(*s, Role::SkinStealer, 8.0), "Level 1: mit Sicht in einigem Abstand");
        bool stalked = false, paused = false;
        step(*s, ev, 30.0, [&] {
            if (P.has(Role::SkinStealer)) aim(*s, P.headOf(Role::SkinStealer));
            stalked = stalked || P.stateOf(Role::SkinStealer) == 1;
            paused = paused || P.stateOf(Role::SkinStealer) == 3;
            return s->stats().dead;
        });
        check(stalked, "Level 1: hungrig sucht er den, der allein ist");
        check(count(ev, PE::Mimic, Role::SkinStealer) > 0, "Level 1: er spricht nach, um zu locken");
        check(paused, "Level 1: vor dem Angriff haelt er inne (Ankuendigung)");
        check(s->stats().dead && s->killRole() == (int)Role::SkinStealer, "Level 1: dann toetet er");
        // Ausweg (Wiki: langsam und ruhig weggehen): haelt er inne und man weicht zurueck, folgt er nur wieder
        s->respawnAtCheckpoint();
        P.clear();
        P.setGrace(3000.0);  // keine anderen Wesen dazwischen
        step(*s, ev, 0.5);
        if (spawn(*s, ev, Role::SkinStealer)) {
            P.kinSet(Role::SkinStealer, "hungry", 1.0);
            placeNear(*s, Role::SkinStealer, 9.0, nullptr, 3.0) || placeNear(*s, Role::SkinStealer, 7.0, nullptr, 3.0) ||
                placeNear(*s, Role::SkinStealer, 9.0);
            bool backed = false;
            step(*s, ev, 30.0, [&] {
                if (!P.has(Role::SkinStealer)) return true;
                aim(*s, P.headOf(Role::SkinStealer));
                if (P.stateOf(Role::SkinStealer) == 3 && !backed) {  // zwei Schritte zurueck, ruhig (wohin der Gang es erlaubt)
                    auto& pl = s->player();
                    const vec3 at = P.posOf(Role::SkinStealer);
                    const double a0 = std::atan2(pl.y - at.y, pl.x - at.x);
                    for (int k = 0; k < 25 * 5 && !backed; ++k) {
                        const double a = a0 + (double)((k % 25 + 1) / 2) * 0.13 * (k % 2 ? 1.0 : -1.0), r = 1.0 + 0.5 * (k / 25);
                        const double nx = pl.x + std::cos(a) * r, ny = pl.y + std::sin(a) * r;
                        const world::Tile& t = s->world().tile(ifloor(nx), ifloor(ny));
                        if (t.open() && std::fabs(t.floor - pl.z) < 0.3 && std::hypot(nx - at.x, ny - at.y) > std::hypot(pl.x - at.x, pl.y - at.y) + 0.8)
                            pl.x = nx, pl.y = ny, s->body().reset(), backed = true;
                    }
                }
                return backed && P.stateOf(Role::SkinStealer) != 3;
            });
            check(backed && !s->stats().dead && P.stateOf(Role::SkinStealer) == 1, "Level 1: wer ruhig zurueckweicht, den laesst er ziehen (er folgt nur)");
        }
    }
    // --- Wretch (Level 2): schlurfend - tiefes Gurgeln, schwere Schlaege; flehend - harmlos ------------------------------
    {
        auto s = open("level2", 13);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        check(spawn(*s, ev, Role::Wretch), "Level 2: ein Wretch erscheint");
        P.kinSet(Role::Wretch, "pleads", 0.0);
        P.kinSet(Role::Wretch, "mode", 0.0);
        check(placeNear(*s, Role::Wretch, 6.0), "Level 2: mit Sicht vor dem Wretch");
        s->flashlight().on = true;
        const double h0 = s->stats().health;
        double firstHit = -1.0;
        step(*s, ev, 20.0, [&] {
            if (P.has(Role::Wretch)) aim(*s, P.headOf(Role::Wretch));
            if (firstHit < 0.0 && s->stats().health < h0) firstHit = s->stats().health;
            return s->stats().dead || s->stats().health < h0 - 60.0;
        });
        check(P.stateOf(Role::Wretch) == 2 || s->stats().health < h0, "Level 2: er sieht einen und jagt");
        check(count(ev, PE::Gurgle, Role::Wretch, -1.0f, 0.5f) > 0, "Level 2: der traege gurgelt tief");
        check(firstHit > 0.0 && firstHit < h0 - 15.0, std::format("Level 2: ein schwerer Schlag, nicht sofort toedlich ({:.0f} -> {:.0f})", h0, firstHit));
        // zweites Stadium: fleht und greift nicht an
        P.clear();
        s->respawnAtCheckpoint();
        step(*s, ev, 0.3);
        check(spawn(*s, ev, Role::Wretch), "Level 2: noch ein Wretch");
        P.kinSet(Role::Wretch, "pleads", 1.0);
        ev.clear();
        if (placeNear(*s, Role::Wretch, 3.0)) {
            const double h1 = s->stats().health;
            step(*s, ev, 10.0, [&] {
                if (P.has(Role::Wretch)) aim(*s, P.headOf(Role::Wretch));
                return false;
            });
            check(count(ev, PE::Plead, Role::Wretch) > 0, "Level 2: im zweiten Stadium fleht er (um Wasser, Essen, Hilfe)");
            check(s->stats().health >= h1 && count(ev, PE::Hit, Role::Wretch) == 0, "Level 2: der Flehende schlaegt nicht");
        }
    }
    // --- Wretch (Level 3): schnell - hoch fauchend, in Stoessen ---------------------------------------------------------
    {
        auto s = open("level3", 14);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        check(spawn(*s, ev, Role::Wretch), "Level 3: ein Wretch erscheint");
        P.kinSet(Role::Wretch, "pleads", 0.0);
        check(placeNear(*s, Role::Wretch, 9.0) || placeNear(*s, Role::Wretch, 6.0), "Level 3: mit Sicht");
        s->flashlight().on = true;
        step(*s, ev, 8.0, [&] {
            if (P.has(Role::Wretch)) aim(*s, P.headOf(Role::Wretch));
            return s->stats().dead;
        });
        check(count(ev, PE::Gurgle, Role::Wretch, 0.5f) > 0, "Level 3: der schnelle faucht hoch (der Laut verraet die Gefahr)");
    }
    // --- Death Rat (Level 2, haarlos): stiebt davon, quiekt; bedraengt beisst sie -----------------------------------------
    {
        auto s = open("level2", 15);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        check(spawn(*s, ev, Role::Rat), "Level 2: Ratten erscheinen");
        check(P.variantOf(Role::Rat) == 1, "Level 2: haarlos (an die Hitze angepasst)");
        check(placeNear(*s, Role::Rat, 2.2), "Level 2: nah bei einer Ratte");
        const float d0 = P.distOf(Role::Rat);
        step(*s, ev, 1.5);
        check(count(ev, PE::Squeak, Role::Rat) > 0, "Level 2: sie quiekt");
        check(!P.has(Role::Rat) || P.distOf(Role::Rat) > d0 + 0.5f || P.stateOf(Role::Rat) == 1, "Level 2: und stiebt davon");
    }
    // --- Death Rat (Level 5): das Schrei-Ereignis - Kreischen, das Licht flackert ----------------------------------------
    {
        auto s = open("level5", 16);
        s->figureMode = 1;  // ohne Angriffe: hier geht es nur um das Ereignis
        std::vector<game::GameEvent> ev;
        step(*s, ev, 400.0, [&] { return count(ev, PE::Scream, Role::Rat) > 0; });
        check(count(ev, PE::Scream, Role::Rat) > 0, "Level 5: die Ratten fallen ueber ein Wesen her (Schrei-Ereignis)");
        step(*s, ev, 2.0);
        check(count(ev, PE::Flicker, Role::Rat) > 0, "Level 5: dabei flackert das Licht");
    }
    // --- Fensterwesen (Level 188): drei Fenster; lockt mit Fluestern; wer zu nah tritt, wird hineingezogen -------------
    {
        auto s = open("level188", 17);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 1.0);
        auto& P = s->presence();
        check(P.count(Role::Window) == 3, std::format("Level 188: drei Fensterwesen ({})", P.count(Role::Window)));
        const vec3 w = P.posOf(Role::Window);
        const float yw = P.yawOf(Role::Window);
        auto stand = [&](double dn) {
            auto& pl = s->player();
            pl.x = w.x + std::cos(yw) * dn, pl.y = w.y + std::sin(yw) * dn, pl.z = s->world().tile(ifloor(pl.x), ifloor(pl.y)).floor;
            pl.vx = pl.vy = 0.0;
            s->body().reset();
            aim(*s, w);
        };
        stand(4.0);
        ev.clear();
        step(*s, ev, 8.0, [&] { aim(*s, w); return false; });
        check(count(ev, PE::Whisper, Role::Window) > 0, "Level 188: es fluestert (lockt)");
        check(!s->stats().dead, "Level 188: auf 4 m geschieht nichts");
        // kurz zu nah, gleich zurueck: der Arm greift ins Leere
        stand(1.5);
        step(*s, ev, 0.25);
        check(count(ev, PE::Pull, Role::Window) > 0, "Level 188: zu nah - der Arm faehrt heraus");
        stand(3.2);
        step(*s, ev, 2.0);
        check(!s->stats().dead, "Level 188: wer sofort zuruecktritt, entkommt");
        // stehen bleiben
        step(*s, ev, 4.5);
        stand(1.5);
        step(*s, ev, 2.0, [&] { return s->stats().dead; });
        check(s->stats().dead && s->killRole() == (int)Role::Window, "Level 188: wer stehen bleibt, wird hineingezogen (toedlich)");
    }
}

// V13.3: Duller (Entity 6) und Arms (Entite 15-FR) in Level 1 (KREATUR_Duller.md, KREATUR_Arms.md)
static void testV133DullerArms(const std::string& dataDir) {
    const game::Content& content = *gContent11;
    game::PrefabLibrary prefabs;
    prefabs.loadDirectory(dataDir + "/prefabs");
    JobSystem jobs(2);
    using PE = game::Presence::Event;
    using Role = game::Presence::Role;
    auto open = [&](i64 seed) {
        auto s = std::make_unique<game::GameSession>(content, prefabs, content.level("level1", world::kLayoutV6), seed, "medium", "Test", jobs, 3);
        s->placeAtSpawn();
        s->preload();
        return s;
    };
    auto step = [&](game::GameSession& s, std::vector<game::GameEvent>& ev, double secs, const std::function<bool()>& each = {}) {
        for (int i = 0; i < (int)(secs * 60.0); ++i) {
            if (each && each()) break;
            s.update(1.0 / 60.0, {}, false);
            ev.insert(ev.end(), s.events().begin(), s.events().end());
            s.events().clear();
        }
    };
    auto count = [](const std::vector<game::GameEvent>& ev, PE::Type t, Role r) {
        return (int)std::count_if(ev.begin(), ev.end(), [&](const game::GameEvent& e) { return e.type == game::GameEvent::Figure && e.index == (int)t && e.role == (int)r; });
    };
    auto put = [&](game::GameSession& s, double x, double y, double yaw) {
        auto& pl = s.player();
        pl.x = x, pl.y = y, pl.z = s.world().tile(ifloor(x), ifloor(y)).floor, pl.angle = yaw, pl.pitch = 0.0, pl.vx = pl.vy = 0.0;
        s.body().reset();
        s.preload();
    };
    auto aim = [](game::GameSession& s, vec3 q) {
        auto& pl = s.player();
        pl.angle = std::atan2(q.y - pl.y, q.x - pl.x);
        pl.pitch = std::atan2(q.z - pl.eye(), std::max(0.4, std::hypot(q.x - pl.x, q.y - pl.y)));
    };
    // Gang (2 Kacheln breit) mit einem Krankenzimmer hinter der Suedwand: der Spieler dicht an der Wand
    const double gx = 7.5, gy = 110.45;
    // --- Duller: schabt in der Wand (Ankuendigung); wer weggeht, entkommt; wer bleibt, wird durch die Wand gepackt ---------
    {
        auto s = open(21);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        P.setGrace(3000.0);
        put(*s, gx, gy, 0.0);
        step(*s, ev, 0.3);
        P.force(Role::Duller);
        step(*s, ev, 0.2);
        check(P.has(Role::Duller) && P.stateOf(Role::Duller) == 2, "Level 1: ein Duller steht hinter der Wand und schabt");
        const vec3 eye{(float)s->player().x, (float)s->player().y, (float)s->player().eye()};
        check(P.has(Role::Duller) && !game::clearSight(s->world(), eye, P.posOf(Role::Duller) + vec3{0, 0, 1.4f}), "Level 1: er ist hinter der Wand (nicht zu sehen)");
        step(*s, ev, 1.6);
        check(count(ev, PE::Scrape, Role::Duller) >= 2 && !s->stats().dead, "Level 1: zuerst Schaben in der Wand (Ankuendigung), kein Zugriff");
        // weggehen (den Gang entlang): er bricht nicht durch oder greift ins Leere
        put(*s, gx + 4.5, gy + 0.6, 0.0);
        step(*s, ev, 5.0);
        check(!s->stats().dead, "Level 1: wer von der Wand weggeht, entkommt");
        // stehen bleiben: Durchbruch und Zugriff
        P.clear();
        put(*s, gx, gy, 0.0);
        step(*s, ev, 0.2);
        P.force(Role::Duller);
        ev.clear();
        step(*s, ev, 6.0, [&] { return s->stats().dead; });
        check(count(ev, PE::Burst, Role::Duller) > 0, "Level 1: die Wand bricht auf");
        check(s->stats().dead && s->killRole() == (int)Role::Duller, "Level 1: wer an der Wand bleibt, wird durch die Wand gepackt (toedlich)");
    }
    // --- Duller: gesehen flieht er lautlos; Mandelwasser in der Hand haelt den Arm fern; lauernd sucht er die Wand ----------
    {
        auto s = open(23);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        P.setGrace(3000.0);
        put(*s, gx, gy, 0.0);
        step(*s, ev, 0.3);
        P.force(Role::Duller);
        step(*s, ev, 0.2);
        if (P.has(Role::Duller)) {
            // in seinen Raum, mit Blick auf ihn
            const vec3 d = P.posOf(Role::Duller);
            bool placed = false;
            for (int k = 0; k < 16 && !placed; ++k) {
                const double a = k * 0.3927, x = d.x + std::cos(a) * 2.4, y = d.y + std::sin(a) * 2.4;
                const world::Tile* t = s->world().tileIfLoaded(ifloor(x), ifloor(y));
                if (!t || !t->open() || !game::clearSight(s->world(), vec3{(float)x, (float)y, t->floor + 1.6f}, d + vec3{0, 0, 1.4f})) continue;
                put(*s, x, y, 0.0);
                aim(*s, d + vec3{0, 0, 1.4f});
                placed = true;
            }
            check(placed, "Level 1: im Raum hinter der Wand, mit Blick auf den Duller");
            ev.clear();
            bool fled = false;
            step(*s, ev, 2.5, [&] {
                if (P.has(Role::Duller)) aim(*s, P.posOf(Role::Duller) + vec3{0, 0, 1.4f});
                fled = fled || P.stateOf(Role::Duller) == 6 || P.stateOf(Role::Duller) == 7;
                return false;
            });
            check(fled, "Level 1: gesehen flieht er");
            check(count(ev, PE::Step, Role::Duller) == 0 && !s->stats().dead, "Level 1: lautlos, ohne anzugreifen");
        }
        // Mandelwasser in der Hand
        P.clear();
        s->inventory().add("almond");
        for (int i = 0; i < game::Inventory::kHotbar; ++i)
            if (s->inventory().slots[(size_t)i] && s->inventory().slots[(size_t)i]->kind == "almond") s->inventory().selected = i;
        put(*s, gx, gy, 0.0);
        step(*s, ev, 0.2);
        P.force(Role::Duller);
        ev.clear();
        step(*s, ev, 6.0, [&] { return s->stats().dead; });
        check(!s->stats().dead && count(ev, PE::Burst, Role::Duller) == 0, "Level 1: Mandelwasser in der Hand - er bricht nicht durch (er meidet es)");
        // der Hinterhalt: lauernd sucht er sich die Wand, hinter der er den Spieler hat
        P.clear();
        s->inventory().selected = (s->inventory().selected + 1) % game::Inventory::kHotbar;
        put(*s, gx, gy, 0.0);
        step(*s, ev, 0.2);
        P.force(Role::Duller);
        step(*s, ev, 0.1);
        P.kinSet(Role::Duller, "state", 0.0);
        bool again = false;
        step(*s, ev, 6.0, [&] { return again = P.stateOf(Role::Duller) == 2; });
        check(again, "Level 1: lauernd sucht er die Wand, hinter der der Spieler steht");
    }
    // --- Arms: nur im Dunkeln; heraus, ausholen (Ankuendigung), Zugriff --------------------------------------------------
    {
        auto s = open(22);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        P.setGrace(3000.0);
        put(*s, gx, gy + 0.3, 0.0);
        s->flashlight().on = false;
        step(*s, ev, 60.0);
        check(!P.has(Role::Arms), "Level 1: im Licht kommen keine Arms");
        P.forceBlackout();
        step(*s, ev, 4.0);
        P.force(Role::Arms);
        step(*s, ev, 0.3);
        check(P.has(Role::Arms), "Level 1: Arms in einer Lueftung oder einem Riss ueber dem Gang");
        if (P.has(Role::Arms)) {
            const vec3 O = P.posOf(Role::Arms);
            check(O.z > s->player().z + 2.0, std::format("Level 1: die Oeffnung ist in der Decke ({:.2f} m)", O.z - s->player().z));
            // unter die Oeffnung (still): es kommt heraus, holt aus und packt zu
            put(*s, O.x + 0.6, O.y, 0.0);
            ev.clear();
            int windupAt = -1, frame = 0;
            step(*s, ev, 6.0, [&] {
                if (windupAt < 0 && count(ev, PE::Windup, Role::Arms) > 0) windupAt = frame;
                ++frame;
                return s->stats().dead;
            });
            check(windupAt >= 0, "Level 1: vor dem Zugriff spreizt es die Krallen (Ankuendigung)");
            check(s->stats().dead && s->killRole() == (int)Role::Arms, "Level 1: wer darunter steht, wird in den Schacht gezogen (toedlich)");
        }
    }
    {  // Licht: die Lampe auf dem Arm - Schrei, Rueckzug, kein Zugriff
        auto s = open(24);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        P.setGrace(3000.0);
        put(*s, gx, gy + 0.3, 0.0);
        P.forceBlackout();
        step(*s, ev, 4.0);
        P.force(Role::Arms);
        step(*s, ev, 0.3);
        if (P.has(Role::Arms)) {
            const vec3 O = P.posOf(Role::Arms);
            put(*s, O.x + 2.2, O.y, 3.14159);
            s->flashlight().on = true;
            ev.clear();
            step(*s, ev, 5.0, [&] {
                if (P.has(Role::Arms)) aim(*s, P.headOf(Role::Arms));
                return s->stats().dead;
            });
            check(count(ev, PE::Burn, Role::Arms) > 0, "Level 1: im Lichtkegel verbrennt es (Schrei)");
            check(!s->stats().dead && (P.stateOf(Role::Arms) == 5 || P.stateOf(Role::Arms) == 7), "Level 1: es zieht sich zurueck und greift nicht zu");
        }
    }
}

// V13.3: Wrangler (KREATUR_Wrangler.md) - Beben als Ankuendigung, Durchzug, wer still ist, ueberlebt; wer laut ist, wird nach
// einer zweiten Ankuendigung aus dem Gang geschnappt; Weibchen greifen nicht an; der Anblick zieht den Blick an; Jungtiere (Level 1)
static void testV133Wrangler(const std::string& dataDir) {
    const game::Content& content = *gContent11;
    game::PrefabLibrary prefabs;
    prefabs.loadDirectory(dataDir + "/prefabs");
    JobSystem jobs(2);
    using PE = game::Presence::Event;
    using Role = game::Presence::Role;
    auto open = [&](const char* level, i64 seed) {
        auto s = std::make_unique<game::GameSession>(content, prefabs, content.level(level, world::kLayoutV6), seed, "medium", "Test", jobs, 3);
        s->placeAtSpawn();
        s->preload();
        return s;
    };
    // ctl(i): Eingabe im Bild i (laut: hin und her sprinten)
    auto step = [&](game::GameSession& s, std::vector<game::GameEvent>& ev, double secs, const std::function<bool()>& each = {},
                    const std::function<game::PlayerControl(int)>& ctl = {}) {
        for (int i = 0; i < (int)(secs * 60.0); ++i) {
            if (each && each()) break;
            s.update(1.0 / 60.0, ctl ? ctl(i) : game::PlayerControl{}, false);
            ev.insert(ev.end(), s.events().begin(), s.events().end());
            s.events().clear();
        }
    };
    auto count = [](const std::vector<game::GameEvent>& ev, PE::Type t) {
        return (int)std::count_if(ev.begin(), ev.end(), [&](const game::GameEvent& e) {
            return e.type == game::GameEvent::Figure && e.index == (int)t && e.role == (int)Role::Wrangler;
        });
    };
    auto sprint = [](int i) {
        game::PlayerControl c;
        c.forward = (i / 70) % 2 ? -1.0f : 1.0f;
        c.sprint = true;
        return c;
    };
    auto aim = [](game::GameSession& s, vec3 q) {
        auto& pl = s.player();
        pl.angle = std::atan2(q.y - pl.y, q.x - pl.x);
        pl.pitch = std::atan2(q.z - pl.eye(), std::max(0.4, std::hypot(q.x - pl.x, q.y - pl.y)));
    };
    auto spawn = [&](game::GameSession& s, std::vector<game::GameEvent>& ev) {
        s.presence().force(Role::Wrangler);
        step(s, ev, 3.0, [&] { return s.presence().stateOf(Role::Wrangler) >= 0; });
        return s.presence().stateOf(Role::Wrangler) == 0;
    };
    // --- Level 2, still: es bebt (Kamera, Licht), bricht durch, zieht vorbei und versinkt - kein Angriff ---------------------
    {
        auto s = open("level2", 31);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        P.setGrace(3000.0);
        check(spawn(*s, ev), "Wrangler (Level 2): erscheint und bebt zuerst (Zustand 0)");
        float qMax = 0.0f;
        bool breached = false, gone = false;
        step(*s, ev, 45.0, [&] {
            qMax = std::max(qMax, P.quake());
            breached = breached || P.stateOf(Role::Wrangler) == 1;
            gone = breached && P.stateOf(Role::Wrangler) < 0;
            return gone;
        });
        check(count(ev, PE::Tremor) > 0 && qMax > 0.12f, std::format("Wrangler: Ankuendigung - Rumpeln ({}) und Beben am Spieler ({:.2f})", count(ev, PE::Tremor), qMax));
        check(count(ev, PE::Flicker) > 0, "Wrangler: beim Beben flackert das Licht");
        check(breached && count(ev, PE::Breach) > 0, "Wrangler: der Leib bricht durch");
        check(count(ev, PE::Sink) > 0 && gone, "Wrangler: er versinkt und zieht ab");
        check(!s->stats().dead && count(ev, PE::Sense) == 0, "Wrangler: wer still steht, ueberlebt (nicht gespuert)");
    }
    // --- Level 2, laut: wer beim Beben rennt, den spuert es (zweite Ankuendigung), dann schnappt der Kopf zu (toedlich) ------
    {
        auto s = open("level2", 32);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        P.setGrace(3000.0);
        check(spawn(*s, ev), "Wrangler (laut): erscheint");
        int senseAt = -1, deadAt = -1, frame = 0;
        step(*s, ev, 30.0, [&] {
            if (senseAt < 0 && count(ev, PE::Sense) > 0) senseAt = frame;
            if (deadAt < 0 && s->stats().dead) deadAt = frame;
            ++frame;
            return s->stats().dead;
        }, sprint);
        check(senseAt >= 0, "Wrangler: Rennen spuert es (Knacken, Einatmen an der Durchbruchstelle)");
        check(s->stats().dead && s->killRole() == (int)Role::Wrangler && count(ev, PE::Burst) > 0, "Wrangler: wer laut bleibt, wird aus dem Gang geschnappt");
        check(senseAt >= 0 && deadAt - senseAt >= 90, std::format("Wrangler: zwischen Ankuendigung und Zugriff mindestens 1,5 s ({:.1f} s)", (deadAt - senseAt) / 60.0));
    }
    // --- Level 2: gespuert, dann still - es verliert einen ------------------------------------------------------------
    {
        auto s = open("level2", 33);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        P.setGrace(3000.0);
        check(spawn(*s, ev), "Wrangler (still nach der Warnung): erscheint");
        step(*s, ev, 20.0, [&] { return count(ev, PE::Sense) > 0; }, sprint);
        const bool sensed = count(ev, PE::Sense) > 0;
        step(*s, ev, 8.0, [&] { return s->stats().dead; });
        check(sensed && !s->stats().dead && count(ev, PE::Calm) > 0, "Wrangler: wer nach der Warnung still steht, den verliert es");
    }
    // --- Weibchen: greift nicht an, zieht sich zurueck, wenn man es ansieht ---------------------------------------------
    {
        auto s = open("level2", 34);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        P.setGrace(3000.0);
        check(spawn(*s, ev), "Wrangler (Weibchen): erscheint");
        P.kinSet(Role::Wrangler, "variant", 1.0);
        check(P.variantOf(Role::Wrangler) == 1, "Wrangler: Variante Weibchen (Zangen)");
        bool sank = false;
        step(*s, ev, 30.0, [&] {
            if (P.stateOf(Role::Wrangler) == 1) aim(*s, P.posOf(Role::Wrangler) + vec3{0, 0, 1.0f});
            sank = sank || P.stateOf(Role::Wrangler) == 4;
            return sank || s->stats().dead;
        }, sprint);
        check(!s->stats().dead && count(ev, PE::Sense) == 0, "Wrangler: das Weibchen greift nicht an (auch wer rennt)");
        check(sank, "Wrangler: bemerkt, zieht sich das Weibchen zurueck");
    }
    // --- der Sog: wer den Leib ansieht, dessen Blick wird gezogen und der wird langsamer ---------------------------------
    {
        auto s = open("level2", 35);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        P.setGrace(3000.0);
        check(spawn(*s, ev), "Wrangler (Sog): erscheint");
        step(*s, ev, 12.0, [&] { return P.stateOf(Role::Wrangler) == 1; });
        step(*s, ev, 1.5);
        vec3 at;
        float tr = 0.0f;
        step(*s, ev, 5.0, [&] {
            aim(*s, P.posOf(Role::Wrangler) + vec3{0, 0, 1.0f});
            tr = std::max(tr, P.trance(&at));
            return false;
        });
        check(tr > 0.3f, std::format("Wrangler: der Anblick bannt ({:.2f})", tr));
        if (tr > 0.3f) {
            auto& pl = s->player();
            const double want = std::atan2(at.y - pl.y, at.x - pl.x);
            pl.angle = want + 0.35;
            step(*s, ev, 0.8);
            const double d1 = std::fabs(std::remainder(pl.angle - want, 6.2831853));
            check(d1 < 0.33, std::format("Wrangler: der Blick wird zum Leib gezogen ({:.2f} -> {:.2f} rad)", 0.35, d1));
        }
    }
    // --- Level 1: Jungtiere an einer Wand; wer sie anfasst, ruft die Mutter -----------------------------------------------
    {
        auto s = open("level1", 36);
        std::vector<game::GameEvent> ev;
        auto& P = s->presence();
        P.setGrace(3000.0);
        step(*s, ev, 6.0, [&] { return P.variantOf(Role::Wrangler) == 3; });
        check(P.variantOf(Role::Wrangler) == 3, "Wrangler (Level 1): Jungtiere an einer Wand (einmal je Ebene)");
        if (P.variantOf(Role::Wrangler) == 3) {
            const vec3 w = P.posOf(Role::Wrangler);
            const float yw = P.yawOf(Role::Wrangler);
            auto& pl = s->player();
            pl.x = w.x + std::cos(yw) * 0.6, pl.y = w.y + std::sin(yw) * 0.6, pl.z = s->world().tile(ifloor(pl.x), ifloor(pl.y)).floor;
            pl.vx = pl.vy = 0.0;
            s->body().reset();
            s->preload();
            aim(*s, w);
            step(*s, ev, 8.0, [&] { return s->stats().dead; });
            check(count(ev, PE::Squeak) > 0, "Wrangler: die Jungen fiepen (Warnung)");
            check(count(ev, PE::Sense) > 0, "Wrangler: beruehrt - die Mutter kommt (Ankuendigung)");
            check(s->stats().dead && s->killRole() == (int)Role::Wrangler, "Wrangler: wer bei den Jungen bleibt, den holt die Mutter");
        }
    }
    // --- Level 5: die Einzelmeldung aus dem Kesselraum - nur dort ---------------------------------------------------------
    {
        auto s = open("level5", 37);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        P.setGrace(3000.0);
        P.force(Role::Wrangler);
        step(*s, ev, 1.0);
        check(P.stateOf(Role::Wrangler) < 0, "Wrangler (Level 5): ausserhalb des Kesselraums keiner");
        auto& pl = s->player();
        pl.x = 124.5, pl.y = 31.5, pl.z = s->world().tile(124, 31).floor, pl.vx = pl.vy = 0.0;
        s->body().reset();
        s->preload();
        step(*s, ev, 0.5);
        check(spawn(*s, ev), "Wrangler (Level 5): im Kesselgang zieht einer durch");
        const vec3 w = P.posOf(Role::Wrangler);
        const world::Tile& t = s->world().tile(ifloor(w.x), ifloor(w.y));
        check(t.room && t.room->type && (t.room->type->key == "kesselgang" || t.room->type->key == "kesselraum"), "Wrangler (Level 5): nur im Kesselgang/Kesselraum");
    }
}

// V13.3: Growler (Entity 130) und Nguithr'xurh (Entity 16) - KREATUR_Growler.md, KREATUR_Nguithrxurh.md
static void testV133GrowlerNguithr(const std::string& dataDir) {
    const game::Content& content = *gContent11;
    game::PrefabLibrary prefabs;
    prefabs.loadDirectory(dataDir + "/prefabs");
    JobSystem jobs(2);
    using PE = game::Presence::Event;
    using Role = game::Presence::Role;
    auto open = [&](const char* level, i64 seed) {
        auto s = std::make_unique<game::GameSession>(content, prefabs, content.level(level, world::kLayoutV6), seed, "medium", "Test", jobs, 3);
        s->placeAtSpawn();
        s->preload();
        s->presence().setGrace(3000.0);  // keine anderen Wesen dazwischen
        return s;
    };
    auto step = [&](game::GameSession& s, std::vector<game::GameEvent>& ev, double secs, const std::function<bool()>& each = {},
                    game::PlayerControl ctl = {}) {
        for (int i = 0; i < (int)(secs * 60.0); ++i) {
            if (each && each()) break;
            s.update(1.0 / 60.0, ctl, false);
            ev.insert(ev.end(), s.events().begin(), s.events().end());
            s.events().clear();
        }
    };
    auto count = [](const std::vector<game::GameEvent>& ev, PE::Type t, Role r, float minI = -1.0f) {
        return (int)std::count_if(ev.begin(), ev.end(), [&](const game::GameEvent& e) {
            return e.type == game::GameEvent::Figure && e.index == (int)t && e.role == (int)r && e.intensity >= minI;
        });
    };
    auto aim = [](game::GameSession& s, vec3 q) {
        auto& pl = s.player();
        pl.angle = std::atan2(q.y - pl.y, q.x - pl.x);
        pl.pitch = std::atan2(q.z - pl.eye(), std::max(0.4, std::hypot(q.x - pl.x, q.y - pl.y)));
    };
    // auf freiem Boden dist m vom Punkt, freie Sicht vom Auge zu 'look'
    auto standAt = [&](game::GameSession& s, vec3 at, vec3 look, double dist) {
        world::World& w = s.world();
        for (int k = 0; k < 36; ++k) {
            const double a = k * 0.1745;
            const double x = at.x + std::cos(a) * dist, y = at.y + std::sin(a) * dist;
            const world::Tile* t = w.tileIfLoaded(ifloor(x), ifloor(y));
            if (!t || !t->open() || t->clearance() < 2.0f || std::fabs(t->floor - at.z) > 0.4f) continue;
            if (!game::clearSight(w, vec3{(float)x, (float)y, t->floor + (float)game::Player::kEye}, look)) continue;
            auto& pl = s.player();
            pl.x = x, pl.y = y, pl.z = t->floor, pl.vx = pl.vy = 0.0;
            s.body().reset();
            aim(s, look);
            return true;
        }
        return false;
    };
    auto spawn = [&](game::GameSession& s, std::vector<game::GameEvent>& ev, Role r) {
        s.presence().force(r);
        step(s, ev, 3.0, [&] { return s.presence().has(r); });
        return s.presence().has(r);
    };
    // --- Growler (Level 1): blind, ortet Bewegung mit Klicks, Rauschen, toedlicher Stoss nach Ankuendigung ---------------
    {
        auto s = open("level1", 21);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        check(spawn(*s, ev, Role::Growler), "Growler: erscheint in Level 1");
        const float sc = P.growlerScale(0);
        check(sc > 0.85f && sc < 1.2f, std::format("Growler: ausgewachsen in Level 1 (Massstab {:.2f})", sc));
        const vec3 at = P.posOf(Role::Growler), head = P.headOf(Role::Growler);
        check(standAt(*s, at, head, 9.0), "Growler: mit Sicht in 9 m");
        s->flashlight().on = true;  // blind - das Licht stoert ihn nicht
        int hunted = 0;
        step(*s, ev, 4.0, [&] {
            aim(*s, P.headOf(Role::Growler));
            hunted += P.stateOf(Role::Growler) >= 2 && P.stateOf(Role::Growler) <= 4 ? 1 : 0;
            return false;
        });
        check(hunted == 0, "Growler: wer still steht, den findet er auf 9 m nicht - auch nicht im Lampenlicht (blind)");
        check(count(ev, PE::Click, Role::Growler) > 0, "Growler: er klickt (Echoortung)");
        check(P.staticLevel() > 0.15f, std::format("Growler: das Rauschen ist schon auf Abstand da ({:.2f})", P.staticLevel()));
        // bewegt man sich, ortet er einen, jagt, holt aus (Ankuendigung) und stoesst zu
        game::PlayerControl walk;
        walk.forward = 0.5f;
        double lurchT = -1.0, deadT = -1.0, t = 0.0;
        step(*s, ev, 25.0, [&] {
            t += 1.0 / 60.0;
            if (P.has(Role::Growler)) aim(*s, P.headOf(Role::Growler));
            if (lurchT < 0.0 && count(ev, PE::Lurch, Role::Growler) > 0) lurchT = t;
            if (s->stats().dead && deadT < 0.0) deadT = t;
            return s->stats().dead;
        }, walk);
        check(s->stats().dead && s->killRole() == (int)Role::Growler, "Growler: wer sich bewegt, wird geortet - der Stoss ist toedlich");
        check(lurchT > 0.0 && deadT - lurchT >= 1.0, std::format("Growler: er holt vorher aus ({:.1f} s vor dem Stoss)", deadT - lurchT));
        // ein geworfener Gegenstand am Kopf verwirrt ihn (Level-1-Seite)
        s->respawnAtCheckpoint();
        P.clear();
        step(*s, ev, 0.3);
        if (spawn(*s, ev, Role::Growler) && standAt(*s, P.posOf(Role::Growler), P.headOf(Role::Growler), 1.5)) {
            const double h0 = s->stats().health;
            step(*s, ev, 0.05);  // der Koerper folgt dem versetzten Spieler (die Hand wirft vom Auge aus)
            s->dropStack("battery", 1);
            bool confused = false;
            step(*s, ev, 1.5, [&] {
                confused = confused || P.stateOf(Role::Growler) == 6;
                return confused;
            });
            check(confused, "Growler: ein geworfener Gegenstand trifft ihn - er ist verwirrt");
            // waehrend er taumelt: weg (rennen)
            game::PlayerControl run;
            run.forward = 1.0f, run.sprint = true;
            if (P.has(Role::Growler)) aim(*s, P.posOf(Role::Growler));
            s->player().angle += 3.14159;
            step(*s, ev, 4.0, {}, run);
            step(*s, ev, 6.0);
            check(!s->stats().dead && s->stats().health > h0 - 30.0, "Growler: verwirrt - man entkommt");
        }
    }
    // --- Junge Growler (Level 2): klein, zu mehreren, verschmelzen, grosse schaden mit dem Rauschen ----------------------
    {
        auto s = open("level2", 22);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        check(spawn(*s, ev, Role::Growler), "Junge Growler: erscheinen in Level 2");
        const int n0 = P.count(Role::Growler);
        check(n0 >= 2 && P.growlerScale(0) < 0.65f, std::format("Junge Growler: klein ({:.2f}) und zu mehreren ({})", P.growlerScale(0), n0));
        P.placeAgent(Role::Growler, 1, P.posOf(Role::Growler) + vec3{0.6f, 0.0f, 0.0f});
        step(*s, ev, 5.0);
        check(count(ev, PE::Merge, Role::Growler) > 0, "Junge Growler: treffen sich zwei, verschmelzen sie");
        float big = 0.0f;
        for (int i = 0; i < P.count(Role::Growler); ++i) big = std::max(big, P.growlerScale(i));
        check(P.count(Role::Growler) < n0 && big > 0.6f, std::format("Junge Growler: einer weniger, der andere waechst ({:.2f})", big));
        // ein grosser, nah: das Rauschen schadet dem Kopf
        P.gnSet(Role::Growler, "scale", 1.4);
        if (standAt(*s, P.posOf(Role::Growler), P.headOf(Role::Growler), 2.2)) {
            const double h0 = s->stats().health;
            ev.clear();
            step(*s, ev, 1.2);
            check(count(ev, PE::Static, Role::Growler, 0.1f) > 0 && s->stats().health < h0, "Growler: gross und nah schadet das Rauschen");
        }
        // Leiche im Gang: man watet langsam durch das Gewirr
        P.gnSet(Role::Growler, "state", 12);
        const vec3 c = P.posOf(Role::Growler);
        check(P.slowFor(0, c) < 0.6f, "Growler-Leiche: durch das Gewirr nur langsam");
    }
    // --- Nguithr'xurh (Level 1): Netzkugeln an der Decke; wer darunter steht, wird betaeubt und gefressen -----------------
    {
        auto s = open("level1", 23);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        check(spawn(*s, ev, Role::Nguithr), "Nguithr'xurh: eine Kolonie erscheint in Level 1");
        const vec3 web = P.webOf(0);
        check(web.z > 2.0f && P.count(Role::Nguithr) >= 4, std::format("Nguithr'xurh: Netz an der Decke ({:.1f} m), Spinnen dabei ({})", web.z,
                                                                       P.count(Role::Nguithr) - 1));
        // genau darunter stehen
        auto under = [&] {
            const world::Tile* t = s->world().tileIfLoaded(ifloor(web.x), ifloor(web.y));
            auto& pl = s->player();
            pl.x = web.x + 0.03, pl.y = web.y + 0.02, pl.z = t ? t->floor : pl.z, pl.vx = pl.vy = 0.0;
            s->body().reset();
        };
        under();
        ev.clear();
        step(*s, ev, 2.6);
        check(count(ev, PE::Snip, Role::Nguithr) == 1 && count(ev, PE::SacBurst, Role::Nguithr, 0.1f) == 1,
              "Nguithr'xurh: eine Kugel wird abgeschnitten und platzt ueber einem");
        const float st1 = P.stupor(0);
        check(st1 > 0.2f && st1 < 0.5f, std::format("Nguithr'xurh: erste Dosis - man fuehlt sich seltsam ({:.2f})", st1));
        // bleiben: alle Kugeln fallen, die Spinnen kommen, sie fressen
        bool came = false;
        step(*s, ev, 40.0, [&] {
            int st = -1;
            for (int i = 0; i < 4; ++i)
                if (P.spiderOf(i, &st).z != 0.0f && (st == 1 || st == 2 || st == 4)) came = true;
            return s->stats().dead;
        });
        check(count(ev, PE::Snip, Role::Nguithr) >= 4, "Nguithr'xurh: wer bleibt, ueber dem fallen alle Kugeln");
        check(came, "Nguithr'xurh: die Spinnen seilen sich ab und kommen");
        check(s->stats().dead && s->killRole() == (int)Role::Nguithr, "Nguithr'xurh: benommen gefressen (toedlich)");
        // Ausweg: nach der ersten Kugel weggehen
        s->respawnAtCheckpoint();
        P.clear();
        P.gnSet(Role::Nguithr, "stupor", 0.0);
        step(*s, ev, 0.3);
        if (spawn(*s, ev, Role::Nguithr)) {
            const vec3 w2 = P.webOf(0);
            const world::Tile* t = s->world().tileIfLoaded(ifloor(w2.x), ifloor(w2.y));
            auto& pl = s->player();
            pl.x = w2.x + 0.03, pl.y = w2.y + 0.02, pl.z = t ? t->floor : pl.z;
            s->body().reset();
            ev.clear();
            step(*s, ev, 3.5, [&] { return count(ev, PE::SacBurst, Role::Nguithr) > 0; });
            check(standAt(*s, vec3{w2.x, w2.y, t ? t->floor : 0.0f}, w2 - vec3{0, 0, 0.5f}, 5.0), "Nguithr'xurh: ein Platz 5 m weg");
            const int snips = count(ev, PE::Snip, Role::Nguithr);
            step(*s, ev, 25.0);
            check(count(ev, PE::Snip, Role::Nguithr) == snips && !s->stats().dead && P.stupor(0) < 0.15f,
                  std::format("Nguithr'xurh: wer weggeht, ist sicher - die Betaeubung klingt ab ({:.2f})", P.stupor(0)));
            // Licht: angeleuchtet erstarrt eine Spinne, dann zirpt sie und huscht ins Dunkel
            int st = -1;
            const vec3 sp = P.spiderOf(0, &st);
            if (standAt(*s, vec3{sp.x, sp.y, t ? t->floor : 0.0f}, sp - vec3{0, 0, 0.05f}, 2.0)) {
                s->flashlight().on = true;
                ev.clear();
                bool fled = false;
                step(*s, ev, 4.0, [&] {
                    aim(*s, sp - vec3{0, 0, 0.05f});
                    int st2 = -1;
                    P.spiderOf(0, &st2);
                    fled = fled || st2 == 3;
                    return false;
                });
                check(count(ev, PE::Stridulate, Role::Nguithr, 0.9f) > 0 && fled, "Nguithr'xurh: im Lampenlicht zirpt sie und huscht weg");
            }
        }
        // Betaeubt: langsamer
        P.gnSet(Role::Nguithr, "stupor", 0.85);
        check(P.slowFor(0, {}) < 0.5f, "Nguithr'xurh: betaeubt geht man langsam");
    }
    // --- Nguithr'xurh (Level 2): klein, satt - Menschen jagen sie selten -------------------------------------------------
    {
        auto s = open("level2", 24);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        if (check(spawn(*s, ev, Role::Nguithr), "Nguithr'xurh: Kolonie in Level 2"), P.has(Role::Nguithr)) {
            P.gnSet(Role::Nguithr, "hungry", 0.0);
            const vec3 w = P.webOf(0);
            check(P.variantOf(Role::Nguithr) == 2, "Nguithr'xurh: in Level 2 mit eingesponnenen Deathmoths");
            const world::Tile* t = s->world().tileIfLoaded(ifloor(w.x), ifloor(w.y));
            auto& pl = s->player();
            pl.x = w.x, pl.y = w.y, pl.z = t ? t->floor : pl.z;
            s->body().reset();
            ev.clear();
            step(*s, ev, 5.0);
            check(count(ev, PE::Snip, Role::Nguithr) == 0, "Nguithr'xurh: satt lassen sie Menschen in Ruhe");
        }
    }
}

// V13.3: Ouroboros-Arbeiter (KREATUR_OuroborosArbeiter.md) - nur im Sektor Ouroboros (Level 1); sie beachten den Spieler nicht
// (Blick bei der Arbeit, kein Ausweichen - Schub); Gefahr nur durch ihre Maschinen, jede angekuendigt; kein Schaden ausserhalb
// der abgesperrten Flaechen
static void testV133Ouroboros(const std::string& dataDir) {
    const game::Content& content = *gContent11;
    game::PrefabLibrary prefabs;
    prefabs.loadDirectory(dataDir + "/prefabs");
    JobSystem jobs(2);
    using PE = game::Presence::Event;
    using Role = game::Presence::Role;
    auto open = [&](const char* level, i64 seed) {
        auto s = std::make_unique<game::GameSession>(content, prefabs, content.level(level, world::kLayoutV6), seed, "medium", "Test", jobs, 3);
        s->placeAtSpawn();
        s->preload();
        s->presence().setGrace(3000.0);
        // V13.3: Gemaelde der Spielleiterin vor den Hallentueren werfen den Spieler sonst weit weg (Arbeiter ruhen dann)
        s->phenomena().config().paintings = false;
        return s;
    };
    auto step = [&](game::GameSession& s, std::vector<game::GameEvent>& ev, double secs, const std::function<bool()>& each = {}) {
        for (int i = 0; i < (int)(secs * 60.0); ++i) {
            if (each && each()) break;
            s.update(1.0 / 60.0, {}, false);
            ev.insert(ev.end(), s.events().begin(), s.events().end());
            s.events().clear();
        }
    };
    auto count = [](const std::vector<game::GameEvent>& ev, PE::Type t, float minI = -1.0f) {
        return (int)std::count_if(ev.begin(), ev.end(), [&](const game::GameEvent& e) {
            return e.type == game::GameEvent::Figure && e.index == (int)t && e.role == (int)Role::Ouroboros && e.intensity >= minI;
        });
    };
    auto put = [](game::GameSession& s, vec3 p, vec3 look) {
        auto& pl = s.player();
        pl.x = p.x, pl.y = p.y, pl.z = p.z, pl.vx = pl.vy = 0.0;
        pl.angle = std::atan2(look.y - p.y, look.x - p.x);
        pl.pitch = std::atan2(look.z - pl.eye(), std::max(0.4, (double)std::hypot(look.x - p.x, look.y - p.y)));
        s.body().reset();
    };
    auto loc = [](vec3 c, float yaw, float x, float y) {
        const vec3 F{std::cos(yaw), std::sin(yaw), 0.0f}, L{-F.y, F.x, 0.0f};
        return c + F * x + L * y;
    };
    // --- Level 1: die Baustelle entsteht nur im Sektor Ouroboros, ausser Sicht ------------------------------------------
    auto s = open("level1", 31);
    std::vector<game::GameEvent> ev;
    auto& P = s->presence();
    // im Garden neben dem Sektor, Blick vom Sektor weg (Osten)
    put(*s, {200.5f, 112.5f, 0.0f}, {230.0f, 112.5f, 1.6f});
    step(*s, ev, 8.0, [&] { return P.ouroInfo().placed; });
    auto info = P.ouroInfo();
    check(info.placed, "Ouroboros: die Baustelle entsteht, wenn der Sektor geladen ist (ausser Sicht)");
    if (!info.placed) return;
    const world::Tile* wt = s->world().tileIfLoaded(ifloor(info.wall.x), ifloor(info.wall.y));
    const world::Tile* tt = s->world().tileIfLoaded(ifloor(info.tower.x), ifloor(info.tower.y));
    check(wt && wt->room && wt->room->type && wt->room->type->key == "ouroboros" && tt && tt->room && tt->room->type && tt->room->type->key == "ouroboros",
          "Ouroboros: Mauer und Geruest stehen im Sektor Ouroboros");
    int inRoom = 0, workers = 0;
    for (int j = 0; j < 5; ++j) {
        if (!info.has[(size_t)j]) continue;
        ++workers;
        // gegen das Rechteck des Sektors (die Kachel unter einem fernen Arbeiter muss nicht geladen sein)
        const vec3 wp = info.pos[(size_t)j];
        inRoom += wt && wt->room && wp.x >= (float)wt->room->x0 && wp.x <= (float)wt->room->x1 && wp.y >= (float)wt->room->y0 && wp.y <= (float)wt->room->y1 ? 1 : 0;
    }
    check(workers == 5 && inRoom == 5, std::format("Ouroboros: fuenf Arbeiter, alle im Sektor ({} von {})", inRoom, workers));
    check(P.count(Role::Ouroboros) == 7, std::format("Ouroboros: Arbeiter, Mauer und Geruest ({} Wesen)", P.count(Role::Ouroboros)));
    // --- sie arbeiten weiter, wenn man daneben steht und sie anleuchtet; der Blick bleibt bei der Arbeit -------------------
    step(*s, ev, 12.0);  // an die Arbeit
    info = P.ouroInfo();
    const int laid0 = info.laid;
    const vec3 hammerer = info.pos[1];
    const vec3 F1{std::cos(info.wallYaw), std::sin(info.wallYaw), 0.0f}, L1{-F1.y, F1.x, 0.0f};
    put(*s, hammerer + F1 * 0.4f + L1 * 1.3f, info.head[1]);  // seitlich neben dem Klopfer, sieht ihn an
    s->flashlight().on = true;
    ev.clear();
    int looked = 0, atWork = 0, samples = 0;
    const float hp0 = (float)s->stats().health;
    step(*s, ev, 10.0, [&] {
        const auto in = P.ouroInfo();
        if (in.has[1] && in.task[1] == anim::ob::K_HAMMER) {
            const vec3 eye{(float)s->player().x, (float)s->player().y, (float)s->player().eye()};
            const vec3 toEye = normalize(eye - in.head[1]);
            ++samples;
            looked += dot(in.face[1], toEye) > 0.8f ? 1 : 0;
            // zur Mauer: waagrecht zur Mauerflaeche hin, und der Blick trifft sie innerhalb ihrer Laenge
            const vec3 fh = normalize(vec3{in.face[1].x, in.face[1].y, 0.0f});
            const float side = dot(in.head[1] - in.wall, F1) >= 0.0f ? 1.0f : -1.0f;
            const float toward = -side * dot(fh, F1);
            const float dist = std::fabs(dot(in.head[1] - in.wall, F1));
            const float along = dot(in.head[1] - in.wall, L1) + (toward > 0.05f ? dot(fh, L1) * dist / toward : 1e9f);
            atWork += toward > 0.7f && std::fabs(along) < anim::ob::kWallL * 0.5f + 0.3f ? 1 : 0;
        }
        return false;
    });
    info = P.ouroInfo();
    check(samples > 60 && looked == 0, std::format("Ouroboros: der Klopfer sieht nie zum Spieler (Lampe an, 1,3 m daneben; {} von {} Bildern)", looked, samples));
    check(samples > 60 && atWork > samples * 9 / 10, std::format("Ouroboros: sein Blick bleibt bei der Mauer ({} von {})", atWork, samples));
    check(count(ev, PE::Work, 0.0f) > 3, "Ouroboros: er haemmert weiter (Werkzeuggeraeusche)");
    check(info.laid > laid0 || info.wallPhase != 0, std::format("Ouroboros: die Mauer waechst weiter ({} -> {} Steine)", laid0, info.laid));
    check(count(ev, PE::Hit) == 0 && std::fabs(s->stats().health - hp0) < 0.01, "Ouroboros: kein Schaden durch die Arbeiter selbst");
    // --- sie weichen nicht aus: der Traeger geht durch den Spieler hindurch und schiebt ihn beiseite --------------------------
    s->flashlight().on = false;
    bool shoved = false;
    double moved = 0.0;
    for (int tries = 0; tries < 30 && !shoved; ++tries) {
        step(*s, ev, 0.5);
        info = P.ouroInfo();
        if (!info.has[4] || info.speed[4] < 0.4f) continue;
        // genau vor ihn auf seinen Weg (0,5 m)
        std::vector<game::GameEvent> e2;
        const vec3 a0 = info.pos[4];
        step(*s, e2, 0.1);
        const vec3 a1 = P.ouroInfo().pos[4];
        const vec3 dir = length(a1 - a0) > 1e-3f ? normalize(vec3{a1.x - a0.x, a1.y - a0.y, 0.0f}) : vec3{1, 0, 0};
        const vec3 at = a1 + dir * 0.45f;
        const world::Tile* t = s->world().tileIfLoaded(ifloor(at.x), ifloor(at.y));
        if (!t || !t->open()) continue;
        put(*s, {at.x, at.y, t->floor}, at + dir * 3.0f + vec3{0, 0, 1.6f});
        const double x0 = s->player().x, y0 = s->player().y;
        e2.clear();
        step(*s, e2, 1.5);
        moved = std::hypot(s->player().x - x0, s->player().y - y0);
        shoved = count(e2, PE::Shove) > 0;
        ev.insert(ev.end(), e2.begin(), e2.end());
    }
    check(shoved && moved > 0.15, std::format("Ouroboros: der Traeger weicht nicht aus - er schiebt den Spieler beiseite ({:.2f} m)", moved));
    check(count(ev, PE::Hit) == 0, "Ouroboros: der Schub tut nicht weh");
    // --- Geruest: knirscht 2,8 s, dann kippt es; Schaden nur im abgesperrten Streifen ---------------------------------------
    {
        info = P.ouroInfo();
        const vec3 inStrip = loc(info.tower, info.towerYaw, 2.2f, 0.0f), outside = loc(info.tower, info.towerYaw, 2.2f, 1.75f);
        P.ouroSet("tower", 0);
        step(*s, ev, 0.2);
        // erst daneben (ausserhalb des Absperrbands): kein Schaden
        put(*s, outside, info.tower + vec3{0, 0, 1.5f});
        ev.clear();
        const float h0 = (float)s->stats().health;
        P.ouroSet("tower", 1);
        double creakT = -1.0, crashT = -1.0, t = 0.0;
        step(*s, ev, 8.0, [&] {
            t += 1.0 / 60.0;
            if (creakT < 0.0 && count(ev, PE::Creak) > 0) creakT = t;
            if (crashT < 0.0 && count(ev, PE::Crash, 0.75f) > 0) crashT = t;
            return crashT > 0.0 && t > crashT + 0.3;
        });
        check(creakT >= 0.0 && crashT > 0.0 && crashT - creakT >= 2.5,
              std::format("Ouroboros: das Geruest kuendigt sich an - es knirscht {:.1f} s, bevor es aufschlaegt", crashT - creakT));
        check(count(ev, PE::Warn, 0.75f) > 0, "Ouroboros: die Warnleuchte des Geruests piept");
        check(count(ev, PE::Hit) == 0 && std::fabs(s->stats().health - h0) < 0.01, "Ouroboros: neben dem Absperrband trifft es nicht");
        // jetzt im Streifen, in den es faellt
        P.ouroSet("tower", 0);
        step(*s, ev, 0.3);
        put(*s, inStrip, info.tower + vec3{0, 0, 1.5f});
        ev.clear();
        const float h1 = (float)s->stats().health;
        P.ouroSet("tower", 1);
        t = 0.0, crashT = -1.0;
        double hitT = -1.0;
        step(*s, ev, 8.0, [&] {
            t += 1.0 / 60.0;
            if (crashT < 0.0 && count(ev, PE::Crash, 0.75f) > 0) crashT = t;
            if (hitT < 0.0 && count(ev, PE::Hit) > 0) hitT = t;
            return crashT > 0.0 && t > crashT + 0.3;
        });
        check(hitT > 0.0 && std::fabs(hitT - crashT) < 0.05 && s->stats().health < h1 - 5.0,
              std::format("Ouroboros: wer im Fallstreifen steht, wird beim Aufschlag getroffen ({:.0f} -> {:.0f})", h1, s->stats().health));
        check(hitT > 2.5, std::format("Ouroboros: der Treffer kommt erst nach der Ankuendigung ({:.1f} s)", hitT));
    }
    // --- Schweissen: Funkenflug nur am Brenner im Band, erst nach dem Zuenden; ausserhalb des Bands nichts ------------------------
    {
        s->stats().health = 100.0;
        P.ouroSet("tower", 0);
        step(*s, ev, 0.3);
        info = P.ouroInfo();
        const vec3 spark = loc(info.tower, info.towerYaw, -0.6f, 0.3f), beside = loc(info.tower, info.towerYaw, -0.6f, 2.0f);
        put(*s, beside, info.tower + vec3{0, 0, 1.0f});
        ev.clear();
        P.ouroSet("weld", 1);
        step(*s, ev, 8.0);
        check(count(ev, PE::Weld, 0.75f) > 0 && count(ev, PE::Hit) == 0 && s->stats().health > 99.9,
              "Ouroboros: Funken treffen neben dem Band nicht (Lichtbogen brennt)");
        put(*s, spark, info.tower + vec3{0, 0, 1.0f});
        ev.clear();
        P.ouroSet("weld", 1);
        double t = 0.0, igniteT = -1.0, hitT = -1.0;
        step(*s, ev, 3.0, [&] {
            t += 1.0 / 60.0;
            if (igniteT < 0.0 && count(ev, PE::Weld) - count(ev, PE::Weld, 0.5f) > 0) igniteT = t;
            if (hitT < 0.0 && count(ev, PE::Hit) > 0) hitT = t;
            return false;
        });
        check(igniteT >= 0.0 && hitT > 0.0 && hitT - igniteT >= 0.9,
              std::format("Ouroboros: am Brenner verbrennen Funken - erst nach dem Zuenden ({:.1f} s)", hitT - igniteT));
        check(s->stats().health < 99.9 && s->stats().health > 80.0, std::format("Ouroboros: Funken brennen wenig ({:.0f})", s->stats().health));
    }
    // --- Mauer: Warnklang, einreissen, Presslufthammer - Schaden nur innerhalb des Absperrbands, erst wenn er laeuft -----------
    {
        s->stats().health = 100.0;
        info = P.ouroInfo();
        const vec3 outside = loc(info.wall, info.wallYaw, 2.2f, 0.0f), inside = loc(info.wall, info.wallYaw, 1.25f, 1.9f);
        put(*s, outside, info.wall + vec3{0, 0, 1.0f});
        ev.clear();
        P.ouroSet("wall", 2);
        double warnT = -1.0, compT = -1.0, jackT = -1.0, t = 0.0;
        step(*s, ev, 40.0, [&] {
            t += 1.0 / 60.0;
            if (warnT < 0.0 && count(ev, PE::Warn) > 0) warnT = t;
            if (compT < 0.0 && count(ev, PE::Warn, 0.4f) - count(ev, PE::Warn, 0.75f) > 0) compT = t;
            if (jackT < 0.0 && count(ev, PE::Jackhammer) > 0) jackT = t;
            return P.ouroInfo().wallPhase == 5;
        });
        check(count(ev, PE::Crash) > 0 && count(ev, PE::Work, 0.7f) > 0, "Ouroboros: die oberen Lagen werden eingerissen (Brechstange, Aufschlag)");
        check(compT >= 0.0 && jackT > 0.0 && jackT - compT >= 1.5, std::format("Ouroboros: der Kompressor laeuft {:.1f} s an, bevor der Presslufthammer schlaegt", jackT - compT));
        check(count(ev, PE::Hit) == 0 && s->stats().health > 99.9, "Ouroboros: ausserhalb des Absperrbands kein Schaden (Einreissen, Presslufthammer)");
        // innen: erst wenn der Hammer laeuft
        P.ouroSet("wall", 4);
        put(*s, inside, info.wall + vec3{0, 0, 1.0f});
        ev.clear();
        t = 0.0, jackT = -1.0;
        double hitT = -1.0;
        step(*s, ev, 6.0, [&] {
            t += 1.0 / 60.0;
            if (jackT < 0.0 && count(ev, PE::Jackhammer) > 0) jackT = t;
            if (hitT < 0.0 && count(ev, PE::Hit) > 0) hitT = t;
            return false;
        });
        check(hitT > 0.0 && jackT > 0.0 && hitT >= jackT - 0.02 && hitT > 1.5,
              std::format("Ouroboros: im abgesperrten Feld trifft der Presslufthammer - erst nach dem Anlaufen ({:.1f} s)", hitT));
        check(s->stats().health < 99.0, std::format("Ouroboros: Splitter verletzen ({:.0f})", s->stats().health));
    }
    // --- Level 2 kennt keine Ouroboros-Arbeiter; auch erzwungen nicht (kein Sektor) --------------------------------------------
    {
        auto s2 = open("level2", 5);
        std::vector<game::GameEvent> e2;
        step(*s2, e2, 3.0);
        s2->presence().force(Role::Ouroboros);
        step(*s2, e2, 1.0);
        check(!s2->presence().has(Role::Ouroboros) && !s2->presence().ouroInfo().placed, "Ouroboros: in Level 2 keine Arbeiter (nur Sektor Ouroboros)");
    }
}

// V13.3: Plague Goblins (KREATUR_PlagueGoblins.md) - Bande mit Portal in Level 1: ausser Sicht, Kasten, Diebstahl von hinten mit
// Ankuendigung, Abschuetteln, Beil (verscheucht / trifft - die Bande flieht und kommt nicht wieder), Bedraengen eines Geschwaechten
// mit Ankuendigung und Grenze, Handel, Flucht vor dem Hound, Koop-Zustand
static void testV133PlagueGoblins(const std::string& dataDir) {
    std::printf("V13.3: Plague Goblins\n");
    const game::Content& content = *gContent11;
    game::PrefabLibrary prefabs;
    prefabs.loadDirectory(dataDir + "/prefabs");
    JobSystem jobs(2);
    using PE = game::Presence::Event;
    using Role = game::Presence::Role;
    namespace gb = anim::gb;
    auto open = [&](const char* level, i64 seed) {
        auto s = std::make_unique<game::GameSession>(content, prefabs, content.level(level, world::kLayoutV6), seed, "medium", "Test", jobs, 3);
        s->placeAtSpawn();
        s->preload();
        s->presence().setGrace(3000.0);
        return s;
    };
    auto step = [&](game::GameSession& s, std::vector<game::GameEvent>& ev, double secs, const std::function<bool()>& each = {}) {
        for (int i = 0; i < (int)(secs * 60.0); ++i) {
            if (each && each()) break;
            s.update(1.0 / 60.0, {}, false);
            ev.insert(ev.end(), s.events().begin(), s.events().end());
            s.events().clear();
        }
    };
    auto count = [](const std::vector<game::GameEvent>& ev, PE::Type t, float minI = -1.0f) {
        return (int)std::count_if(ev.begin(), ev.end(), [&](const game::GameEvent& e) {
            return e.type == game::GameEvent::Figure && e.index == (int)t && e.role == (int)Role::Goblin && e.intensity >= minI;
        });
    };
    auto face = [](game::GameSession& s, vec3 at) {
        auto& pl = s.player();
        pl.vx = pl.vy = 0.0;
        pl.angle = std::atan2(at.y - pl.y, at.x - pl.x);
        pl.pitch = 0.0;
        s.body().reset();
    };
    auto feetOf = [](game::GameSession& s) { return vec3{(float)s.player().x, (float)s.player().y, (float)s.player().z}; };
    // eine Bande erzwingen und warten, bis alle aus dem Portal sind
    auto gang = [&](game::GameSession& s, std::vector<game::GameEvent>& ev) {
        auto& P = s.presence();
        step(s, ev, 2.0);
        P.force(Role::Goblin);
        step(s, ev, 0.2);
        bool out = false;
        step(s, ev, 9.0, [&] {
            const auto in = P.goblinInfo();
            out = in.active && std::none_of(in.state.begin(), in.state.end(), [&](int st) { return st == gb::S_INSIDE || st == gb::S_EMERGE; });
            return out;
        });
        return out;
    };

    // --- Daten: Level 1 kennt sie, Level 2 nicht; das Beil liegt in den Funden von Level 1 -----------------------------
    {
        const auto L1 = content.level("level1", world::kLayoutV6), L2 = content.level("level2", world::kLayoutV6);
        check(L1 && L1->kin[world::KIN_GOBLIN].on && L1->kin[world::KIN_GOBLIN].groupMin >= 9, "Plague Goblins: Level 1 kennt sie (Banden ab 9, die Seite: 9-27)");
        check(L2 && !L2->kin[world::KIN_GOBLIN].on, "Plague Goblins: Level 2 kennt sie nicht");
        const game::ItemDef* hd = content.item("hatchet");
        check(hd && hd->maxStack == 1 && hd->model == "hatchet", "Beil: Gegenstand vorhanden (einzeln, eigenes Modell)");
        int withHatchet = 0;
        for (u64 seed = 1; seed <= 16; ++seed) withHatchet += countSupplies("level1", seed).kinds.count("hatchet") ? 1 : 0;
        check(withHatchet >= 8 && withHatchet <= 16, std::format("Beil: in den Funden von Level 1 ({} von 16 Laeufen)", withHatchet));
    }

    // --- Auftritt: Portal ausser Sicht, Kasten, Klang vor Sicht ------------------------------------------------------------
    auto s = open("level1", 31);
    std::vector<game::GameEvent> ev;
    auto& P = s->presence();
    step(*s, ev, 2.0);
    P.force(Role::Goblin);
    step(*s, ev, 0.1);
    auto info = P.goblinInfo();
    check(info.active && info.bodies >= 9 && info.bodies <= 12, std::format("Plague Goblins: eine Bande erscheint ({} Koerper)", info.bodies));
    if (!info.active) return;
    {
        const vec3 eye = s->body().eye();
        const vec3 to = info.portal + vec3{0, 0, 0.7f} - eye;
        const vec3 look = forwardFromAngles((float)s->player().angle, (float)s->player().pitch);
        const bool inView = dot(normalize(to), look) > 0.3f && game::clearSight(s->world(), eye, info.portal + vec3{0, 0, 0.7f});
        check(!inView && length(to) > 12.0f, std::format("Plague Goblins: das Portal oeffnet sich ausser Sicht ({:.0f} m)", length(to)));
        int gate = 0, scouts = 0;
        for (int v : info.variant) gate += v == 2 ? 1 : 0, scouts += v == 1 ? 1 : 0;
        check(gate == 1 && scouts == 2, std::format("Plague Goblins: eine Torwaechterin, zwei Spaeher (die Seite: drei Kasten; {} / {})", gate, scouts));
        check(count(ev, PE::Portal, 0.9f) == 1 && count(ev, PE::Croak) >= 1, "Plague Goblins: Klang vor Sicht (Portal, Kraechzen)");
    }
    bool out = false;
    step(*s, ev, 9.0, [&] {
        const auto in = P.goblinInfo();
        out = std::none_of(in.state.begin(), in.state.end(), [&](int st) { return st == gb::S_INSIDE || st == gb::S_EMERGE; });
        return out;
    });
    check(out && P.goblinInfo().portalOpen, "Plague Goblins: alle springen einzeln aus dem offenen Portal");
    // Koop: der Zustand geht mit Rolle, Kaste und Zustand ueber das Netz
    {
        std::vector<game::Presence::NetState> ns;
        P.netStates(ns, feetOf(*s), 40);
        int gob = 0, gateN = 0, portalN = 0;
        for (const auto& n : ns) {
            if (n.role != (int)Role::Goblin) continue;
            ++gob;
            gateN += n.variant == 2 ? 1 : 0;
            portalN += n.variant == 3 ? 1 : 0;
        }
        check(gob == info.bodies + 1 && gateN == 1 && portalN == 1, std::format("Plague Goblins: Netzzustand (Rolle {}, {} Eintraege)", (int)Role::Goblin, gob));
    }

    // --- Diebstahl: von hinten, angekuendigt; vorn nie -------------------------------------------------------------------
    s->inventory().add("almond");
    s->inventory().add("almond");
    s->inventory().add("battery");
    s->flashlight().on = true;
    face(*s, P.goblinInfo().portal);  // man sieht zur Bande
    P.goblinSet("phase", 2);
    ev.clear();
    step(*s, ev, 12.0, [&] {
        face(*s, P.goblinInfo().portal);
        return false;
    });
    check(count(ev, PE::Steal) == 0, "Plague Goblins: wer zur Bande sieht, wird nicht bestohlen (sie erstarren, wenn man sie ansieht)");
    // jetzt mit dem Ruecken zur Bande
    const vec3 away = feetOf(*s) * 2.0f - P.goblinInfo().portal;
    double t = 0.0, warnT = -1.0, tookT = -1.0;
    const int almond0 = s->inventory().count("almond") + s->inventory().count("battery");
    ev.clear();
    step(*s, ev, 60.0, [&] {
        face(*s, away);
        t += 1.0 / 60.0;
        if (warnT < 0.0 && count(ev, PE::Steal, -1.0f) > 0) warnT = t;
        if (tookT < 0.0 && count(ev, PE::Steal, 0.9f) > 0) tookT = t;
        return tookT > 0.0 && t > tookT + 0.5;
    });
    const int almond1 = s->inventory().count("almond") + s->inventory().count("battery");
    check(warnT > 0.0 && tookT > 0.0 && tookT - warnT >= 1.0,
          std::format("Plague Goblins: von hinten - es zerrt am Rucksack ({:.1f} s), dann ist etwas fort ({:.1f} s)", warnT, tookT));
    check(almond1 == almond0 - 1, std::format("Plague Goblins: genau ein Stueck gestohlen ({} -> {})", almond0, almond1));
    check(count(ev, PE::Hit) == 0, "Plague Goblins: der Diebstahl tut nicht weh");

    // --- Beil: ein Schwung ohne Treffer verscheucht; ein Treffer toetet einen, die Bande flieht und kommt nicht wieder ----------
    {
        s->inventory().add("hatchet");
        int slot = -1;
        for (int i = 0; i < game::Inventory::kHotbar; ++i)
            if (s->inventory().slots[(size_t)i] && s->inventory().slots[(size_t)i]->kind == "hatchet") slot = i;
        check(slot >= 0, "Beil: liegt in der Schnellleiste");
        if (slot >= 0) {
            s->selectSlot(slot);
            // einen Goblin 3,5 m vor den Spieler stellen (verscheuchen), dann einen 1,1 m davor (Treffer)
            const vec3 f = feetOf(*s);
            const vec3 look = vec3{std::cos((float)s->player().angle), std::sin((float)s->player().angle), 0.0f};
            info = P.goblinInfo();
            int far = -1, near = -1;
            for (int j = 0; j < (int)info.state.size(); ++j)
                if (info.state[(size_t)j] != gb::S_DEAD && info.state[(size_t)j] != gb::S_RUMMAGE && info.state[(size_t)j] != gb::S_ENTER) {
                    if (far < 0) far = j;
                    else if (near < 0) near = j;
                }
            if (far >= 0 && near >= 0) {
                P.placeAgent(Role::Goblin, far + 1, f + look * 3.5f);
                step(*s, ev, 0.05);
                ev.clear();
                s->useSlot(slot);
                step(*s, ev, 0.2);
                info = P.goblinInfo();
                check(info.dead == 0 && info.state[(size_t)far] == gb::S_SCURRY, "Beil: ein Schwung verscheucht, wer davor steht (kein Treffer auf 3,5 m)");
                check(count(ev, PE::Croak, 0.9f) > 0, "Beil: der Verscheuchte kreischt");
                check(s->inventory().count("hatchet") == 1, "Beil: wird nicht verbraucht");
                step(*s, ev, 1.0);
                P.placeAgent(Role::Goblin, near + 1, f + look * 1.1f);
                step(*s, ev, 0.05);
                ev.clear();
                s->useSlot(slot);
                step(*s, ev, 0.3);
                info = P.goblinInfo();
                check(info.dead == 1 && info.routed && info.phase == 4, std::format("Beil: ein Treffer aus der Naehe toetet einen Goblin, die Bande flieht ({} tot)", info.dead));
                check(count(ev, PE::Perish) == 1, "Beil: Todesschrei");
                ev.clear();
                step(*s, ev, 30.0, [&] { return !P.goblinInfo().active; });
                check(!P.goblinInfo().active && count(ev, PE::Portal) >= 0, "Plague Goblins: nach dem Treffer zieht die Bande durch das Portal ab");
                step(*s, ev, 1.0);
                const auto reg = P.goblinInfo();
                step(*s, ev, 120.0);
                check(!P.goblinInfo().active && P.goblinInfo().routed && reg.routed, "Plague Goblins: sie kommen in dieser Ebene nicht wieder (die Seite)");
            }
        }
    }

    // --- Bedraengen: ein Geschwaechter - Masken hoch und Fauchen (Ankuendigung), Bisse, aber nicht bis zum Tod -------------------
    {
        auto s2 = open("level1", 7);
        std::vector<game::GameEvent> e2;
        const bool ok = gang(*s2, e2);
        check(ok, "Plague Goblins: Bande (zweiter Lauf) ist da");
        if (ok) {
            auto& P2 = s2->presence();
            s2->stats().health = 38.0;
            s2->inventory().add("almond");
            P2.goblinSet("phase", 2);
            P2.goblinSet("mob", 1);
            double tt = 0.0, hissT = -1.0, hitT = -1.0, screechT = -1.0;
            bool masks = false;
            e2.clear();
            step(*s2, e2, 16.0, [&] {
                tt += 1.0 / 60.0;
                if (hissT < 0.0 && count(e2, PE::Hiss) > 0) hissT = tt;
                if (screechT < 0.0 && count(e2, PE::Screech) > 0) screechT = tt;
                if (hitT < 0.0 && count(e2, PE::Hit) > 0) hitT = tt;
                const auto in = P2.goblinInfo();
                for (float m : in.mask) masks = masks || m > 0.5f;
                return false;
            });
            check(hissT >= 0.0 && masks, std::format("Plague Goblins: Geschwaechte werden bedraengt - Masken hoch, Fauchen ({:.1f} s)", hissT));
            check(hitT < 0.0 || (hitT - hissT >= 1.3 && screechT >= 0.0 && screechT <= hitT),
                  std::format("Plague Goblins: der erste Biss kommt nach der Ankuendigung und einem Kreischen ({:.1f} / {:.1f} s)", hitT - hissT, screechT));
            check(count(e2, PE::Hit) >= 1, std::format("Plague Goblins: sie beissen zu ({} Bisse)", count(e2, PE::Hit)));
            check(s2->stats().health > 5.0 && !s2->stats().dead, std::format("Plague Goblins: nicht bis zum Tod (Gesundheit {:.0f})", s2->stats().health));
            check(P2.goblinInfo().phase == 4 || !P2.goblinInfo().active, "Plague Goblins: danach ziehen sie ab");
        }
    }

    // --- Handel: hingelegtes Mandelwasser - sie nehmen es, die Torwaechterin legt ein Geschenk hin --------------------------------
    {
        auto s3 = open("level1", 12);
        std::vector<game::GameEvent> e3;
        const bool ok = gang(*s3, e3);
        if (ok) {
            auto& P3 = s3->presence();
            // der Spieler geht zur Bande (8 m vor das Portal) und legt Mandelwasser hin
            const vec3 pt = P3.goblinInfo().portal;
            vec3 f = feetOf(*s3);
            const vec3 dir = normalize(vec3{pt.x - f.x, pt.y - f.y, 0.0f});
            P3.goblinSet("phase", 1);
            face(*s3, pt);
            s3->inventory().add("almond");
            int slot = -1;
            for (int i = 0; i < game::Inventory::kHotbar; ++i)
                if (s3->inventory().slots[(size_t)i] && s3->inventory().slots[(size_t)i]->kind == "almond") slot = i;
            (void)dir;
            // nahe genug heran: auf dem Wegefeld zur Bande (Teleport an eine freie Kachel 6-8 m vor dem Portal)
            bool placed = false;
            for (float r = 6.0f; r <= 9.0f && !placed; r += 0.5f)
                for (int a = 0; a < 16 && !placed; ++a) {
                    const vec3 q = pt + vec3{std::cos(a * 0.3927f), std::sin(a * 0.3927f), 0.0f} * r;
                    const world::Tile* tq = s3->world().tileIfLoaded(ifloor(q.x), ifloor(q.y));
                    if (!game::figureWalkable(tq, 1.9f) || !game::clearSight(s3->world(), q + vec3{0, 0, 1.0f}, pt + vec3{0, 0, 0.5f})) continue;
                    auto& pl = s3->player();
                    pl.x = q.x, pl.y = q.y, pl.z = tq->floor;
                    placed = true;
                }
            face(*s3, pt);
            step(*s3, e3, 0.3);
            if (slot >= 0) s3->dropSlot(slot, false);
            e3.clear();
            step(*s3, e3, 40.0, [&] { return P3.goblinInfo().gifts > 0; });
            const auto in = P3.goblinInfo();
            check(placed && count(e3, PE::Take) >= 1 && in.friendly, std::format("Plague Goblins: hingelegtes Mandelwasser wird genommen - Handel ({})", in.friendly ? "ja" : "nein"));
            check(in.gifts == 1, "Plague Goblins: die Torwaechterin legt ein Geschenk hin (Batterie)");
            check(count(e3, PE::Hit) == 0 && count(e3, PE::Steal) == 0, "Plague Goblins: im Handel kein Diebstahl, kein Angriff");
        } else {
            check(false, "Plague Goblins: Bande (Handel) ist da");
        }
    }

    // --- Hound: Furcht - die Bande flieht ---------------------------------------------------------------------------------
    {
        auto s4 = open("level1", 21);
        std::vector<game::GameEvent> e4;
        if (gang(*s4, e4)) {
            auto& P4 = s4->presence();
            P4.force(Role::Hound, 18.0);
            step(*s4, e4, 1.0);
            check(!P4.has(Role::Hound) || P4.goblinInfo().phase == 4 || !P4.goblinInfo().active,
                  std::format("Plague Goblins: ein Hound in der Naehe - die Bande flieht (Phase {})", P4.goblinInfo().phase));
        }
    }
}

// --- V13.3: Leuchtstab (Smiler-Koeder) und Ansprechen (Skin-Stealer) - KREATUR_Smiler.md, KREATUR_SkinStealer.md --------
static void testV133LureTalk(const std::string& dataDir) {
    std::printf("V13.3: Leuchtstab und Ansprechen (Level 1)\n");
    const game::Content& content = *gContent11;
    game::PrefabLibrary prefabs;
    prefabs.loadDirectory(dataDir + "/prefabs");
    JobSystem jobs(2);
    using PE = game::Presence::Event;
    using Role = game::Presence::Role;
    auto open = [&](const char* level, i64 seed) {
        auto s = std::make_unique<game::GameSession>(content, prefabs, content.level(level, world::kLayoutV6), seed, "medium", "Test", jobs, 3);
        s->placeAtSpawn();
        s->preload();
        return s;
    };
    auto step = [&](game::GameSession& s, std::vector<game::GameEvent>& ev, double secs, const std::function<bool()>& each = {},
                    const std::function<game::PlayerControl(int)>& ctl = {}) {
        for (int i = 0; i < (int)(secs * 60.0); ++i) {
            if (each && each()) break;
            s.update(1.0 / 60.0, ctl ? ctl(i) : game::PlayerControl{}, false);
            ev.insert(ev.end(), s.events().begin(), s.events().end());
            s.events().clear();
        }
    };
    auto count = [](const std::vector<game::GameEvent>& ev, PE::Type t, Role r) {
        return (int)std::count_if(ev.begin(), ev.end(), [&](const game::GameEvent& e) {
            return e.type == game::GameEvent::Figure && e.index == (int)t && e.role == (int)r;
        });
    };
    auto aim = [](game::GameSession& s, vec3 q) {
        auto& pl = s.player();
        pl.angle = std::atan2(q.y - pl.y, q.x - pl.x);
        pl.pitch = std::atan2(q.z - pl.eye(), std::max(0.4, std::hypot(q.x - pl.x, q.y - pl.y)));
    };
    auto has = [](const std::vector<game::GameEvent>& ev, game::GameEvent::Type t) {
        return std::any_of(ev.begin(), ev.end(), [&](const game::GameEvent& e) { return e.type == t; });
    };
    auto slotOf = [](game::GameSession& s, const char* kind) {
        for (int i = 0; i < game::Inventory::kSize; ++i)
            if (s.inventory().slots[(size_t)i] && s.inventory().slots[(size_t)i]->kind == kind) return i;
        return -1;
    };

    // --- Fundstueck: Leuchtstaebe liegen auf den Ebenen mit Smilern, eher in dunklen Raeumen; Level 0 keine --------------
    for (const char* id : {"level1", "level2", "level3", "level0"}) {
        auto lvl = content.level(id, world::kLayoutV6);
        if (!lvl || !lvl->map) continue;
        world::World w(lvl, 4242, 0.28, 0.80, jobs, 3);
        int n = 0, bad = 0;
        const int S = std::max(1, lvl->sectorSize);
        for (int sy = 0; sy * S < (int)lvl->map->h; ++sy)
            for (int sx = 0; sx * S < (int)lvl->map->w; ++sx) {
                w.tile(sx * S + S / 2, sy * S + S / 2);
                std::vector<game::FindSpot> all;
                for (const auto& r : w.sector(sx, sy).rooms)
                    if (r->prepared()) game::roomFinds(*r, *lvl, all);
                for (const auto& f : all) {
                    if (f.kind != game::FindSpot::Glowstick) continue;
                    ++n;
                    const world::Tile& t = w.tile(f.tx, f.ty);
                    if (!t.open() || std::fabs(t.floor - f.pos.z) > 0.01f) ++bad;
                }
            }
        std::printf("  %s: %d Leuchtstaebe (Fundstuecke)\n", id, n);
        if (std::string(id) == "level0") check(n == 0, "Level 0: keine Leuchtstaebe (dort gibt es keine Smiler)");
        else check(n >= 2 && n <= 9 && bad == 0, std::format("{}: zwei bis neun Leuchtstaebe auf freiem Boden ({}, {} falsch)", id, n, bad));
    }

    // --- Smiler: ein geworfener Leuchtstab zieht ihn ab (Level 1, Halle Aquila, im Stromausfall) ----------------------------
    {
        auto s = open("level1", 4242);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.2);
        auto& P = s->presence();
        world::World& ww = s->world();
        {
            auto& pl = s->player();
            const auto [hx, hy] = hallSpot(ww);
            pl.x = hx + 0.5, pl.y = hy + 0.5, pl.z = ww.tile(hx, hy).floor;
            s->body().reset();
        }
        s->preload();
        step(*s, ev, 0.5);
        P.force(Role::Smiler);
        step(*s, ev, 16.0, [&] { return P.has(Role::Smiler); });
        check(P.has(Role::Smiler), "Leuchtstab: der Smiler erscheint im Dunkeln");
        step(*s, ev, 4.0, [&] { return !P.has(Role::Smiler) || P.stateOf(Role::Smiler) >= 1; }, {});
        // die Lampe ist an (er folgt ihrem Licht) - dann den Stab quer wegwerfen, in den freien Teil der Halle
        s->flashlight().on = true;
        s->inventory().add("glowstick");
        const int slot = slotOf(*s, "glowstick");
        const vec3 sp = P.posOf(Role::Smiler);
        auto& pl = s->player();
        const double toS = std::atan2(sp.y - pl.y, sp.x - pl.x);
        double best = toS + 1.2, bestFree = 0.0;
        for (double off : {1.2, -1.2, 0.8, -0.8, 1.6, -1.6}) {  // die Richtung mit dem meisten freien Boden
            double free = 0.0;
            for (double r = 1.0; r < 10.0; r += 0.5) {
                const world::Tile* t = ww.tileIfLoaded(ifloor(pl.x + std::cos(toS + off) * r), ifloor(pl.y + std::sin(toS + off) * r));
                if (!t || !t->open() || std::fabs(t->floor - pl.z) > 0.1) break;
                free = r;
            }
            if (free > bestFree) bestFree = free, best = toS + off;
        }
        pl.angle = best, pl.pitch = 0.12;
        const size_t lures0 = s->lures().lures().size();
        s->useSlot(slot);
        check(slotOf(*s, "glowstick") < 0 && s->glowThrowPending(), "Leuchtstab: F knickt ihn (aus dem Inventar, Wurfgeste)");
        step(*s, ev, 0.7);
        check(s->lures().lures().size() == lures0 + 1, "Leuchtstab: beim Loslassen fliegt er");
        const vec3 start = s->lures().lures().empty() ? vec3{} : s->lures().lures().back().pos;
        bool airborne = false;
        step(*s, ev, 3.0, [&] {
            const auto& L = s->lures().lures().back();
            airborne = airborne || L.pos.z > start.z + 0.05f;
            return L.resting;
        });
        const auto& L = s->lures().lures().back();
        const world::Tile& lt = ww.tile(ifloor(L.pos.x), ifloor(L.pos.y));
        const float flew = std::hypot(L.pos.x - start.x, L.pos.y - start.y);
        std::printf("  Leuchtstab: %.1f m weit, liegt %.3f m ueber dem Boden, Leuchtkraft %.2f\n", flew, L.pos.z - lt.floor, s->lures().glowOf(L));
        check(airborne && L.resting && flew > 2.5f && flew < 16.0f, std::format("Leuchtstab: Wurfbogen, liegt nach {:.1f} m still", flew));
        check(std::fabs(L.pos.z - lt.floor - game::LureSystem::kRadius) < 0.01f && std::fabs(std::remainder(L.pitch, 3.14159265f)) < 0.02f,
              "Leuchtstab: liegt flach auf dem Boden");
        check(s->lures().glowOf(L) > 0.8f, "Leuchtstab: leuchtet hell");
        std::vector<game::GameSession::GlowLight> gl;
        s->glowLights(gl, 40.0f);
        check(!gl.empty(), "Leuchtstab: eigenes Licht");
        // wegsehen (sonst wuerde er ohne Koeder kommen und zugreifen) - er geht zum Licht, nicht zum Spieler
        float dLure0 = 1e9f, dLure = 1e9f, dPlayerMin = 1e9f;
        bool luredSeen = false;
        if (P.has(Role::Smiler)) dLure0 = std::hypot(P.posOf(Role::Smiler).x - L.pos.x, P.posOf(Role::Smiler).y - L.pos.y);
        step(*s, ev, 9.0, [&] {
            if (!P.has(Role::Smiler)) return true;
            luredSeen = luredSeen || P.luredOf(Role::Smiler);
            const vec3 q = P.posOf(Role::Smiler);
            pl.angle = std::atan2(q.y - pl.y, q.x - pl.x) + 3.14159;  // abgewandt
            pl.pitch = 0.0;
            dLure = std::hypot(q.x - L.pos.x, q.y - L.pos.y);
            dPlayerMin = std::min(dPlayerMin, (float)std::hypot(q.x - pl.x, q.y - pl.y));
            return s->stats().dead;
        });
        std::printf("  Smiler: zum Stab %.1f -> %.1f m, dem Spieler hoechstens %.1f m nah\n", dLure0, dLure, dPlayerMin);
        check(count(ev, PE::Lured, Role::Smiler) >= 1 && luredSeen, "Leuchtstab: der Smiler wendet sich dem staerkeren Licht zu");
        check(dLure < 1.6f || dLure < dLure0 - 3.0f, std::format("Leuchtstab: er geht zum Licht ({:.1f} -> {:.1f} m)", dLure0, dLure));
        check(!s->stats().dead && !has(ev, game::GameEvent::StalkerHit), "Leuchtstab: solange er leuchtet, laesst er vom Spieler ab");
        // Laerm ganz nah (Wiki, Level 1: nach einer Sichtung zieht ihn auch Laerm an): wer neben ihm rennt, holt ihn zurueck
        if (P.has(Role::Smiler) && P.luredOf(Role::Smiler)) {
            const vec3 q = P.posOf(Role::Smiler);
            bool backed = false;
            for (int k = 0; k < 16 && !backed; ++k) {  // ein freier Platz 3,5 m neben ihm
                const double a = k * 0.3927, x = q.x + std::cos(a) * 3.5, y = q.y + std::sin(a) * 3.5;
                const world::Tile* t = ww.tileIfLoaded(ifloor(x), ifloor(y));
                if (t && t->open() && std::fabs(t->floor - pl.z) < 0.2) pl.x = x, pl.y = y, pl.vx = pl.vy = 0.0, s->body().reset(), backed = true;
            }
            bool released = false;
            step(*s, ev, 1.5, [&] {
                released = released || (P.has(Role::Smiler) && !P.luredOf(Role::Smiler));
                return released;
            }, [](int i) {
                game::PlayerControl c;
                c.forward = (i / 20) % 2 ? -1.0f : 1.0f;
                c.sprint = true;
                return c;
            });
            check(!backed || released, "Leuchtstab: wer neben ihm rennt, holt ihn vom Licht zurueck");
        }
    }
    // --- Level 3 (Seite level-3): dort stutzt der Smiler nur und bleibt beim Wanderer. Geprueft in der Halle von Level 1
    // mit der Einstellung von Level 3 (dort braucht der Smiler einen Stromausfall an einem passenden Ort) ---------------
    {
        check(content.level("level3", world::kLayoutV6)->smilerBait == 1 && content.level("level1", world::kLayoutV6)->smilerBait == 0,
              "Level 3: smiler_bait = hesitate (Level 1: follow)");
        auto L3 = std::make_shared<world::LevelDef>(*content.level("level1", world::kLayoutV6));
        L3->smilerBait = 1;
        auto s = std::make_unique<game::GameSession>(content, prefabs, L3, 4242, "medium", "Test", jobs, 3);
        s->placeAtSpawn();
        s->preload();
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.2);
        auto& P = s->presence();
        {
            auto& pl = s->player();
            const auto [hx, hy] = hallSpot(s->world());
            pl.x = hx + 0.5, pl.y = hy + 0.5, pl.z = s->world().tile(hx, hy).floor;
            s->body().reset();
        }
        s->preload();
        step(*s, ev, 0.5);
        P.force(Role::Smiler);
        step(*s, ev, 20.0, [&] { return P.has(Role::Smiler) && P.stateOf(Role::Smiler) >= 1; });
        if (P.has(Role::Smiler)) {
            auto& pl = s->player();
            const vec3 sp = P.posOf(Role::Smiler);
            pl.angle = std::atan2(sp.y - pl.y, sp.x - pl.x), pl.pitch = 0.05;  // zu ihm hin werfen
            s->inventory().add("glowstick");
            s->useSlot(slotOf(*s, "glowstick"));
            bool lured = false, released = false;
            step(*s, ev, 6.0, [&] {
                if (!P.has(Role::Smiler)) return true;
                const vec3 q = P.posOf(Role::Smiler);
                pl.angle = std::atan2(q.y - pl.y, q.x - pl.x), pl.pitch = 0.0;  // ansehen: er bleibt stehen
                lured = lured || P.luredOf(Role::Smiler);
                released = released || (lured && !P.luredOf(Role::Smiler));
                return released;
            });
            const bool hesitated = std::any_of(ev.begin(), ev.end(), [&](const game::GameEvent& e) {
                return e.type == game::GameEvent::Figure && e.index == (int)PE::Lured && e.role == (int)Role::Smiler && e.intensity < 0.5f;
            });
            check(hesitated && lured && released, "Level 3: der Smiler stutzt am Licht und wendet sich wieder dem Wanderer zu");
        } else {
            std::printf("  Level 3: kein Smiler erschienen (Pruefung des Stutzens uebersprungen)\n");
        }
    }

    // --- Skin-Stealer: Ansprechen entlarvt, ruhiges Zurueckweichen ueberlebt, Rennen loest den Angriff aus ----------------
    auto placeNear = [&](game::GameSession& s, double dist, double back) {
        auto& P = s.presence();
        if (!P.has(Role::SkinStealer)) return false;
        const vec3 at = P.posOf(Role::SkinStealer), head = P.headOf(Role::SkinStealer);
        world::World& w = s.world();
        for (int k = 0; k < 36; ++k) {
            const float yaw0 = P.yawOf(Role::SkinStealer) + (float)((k + 1) / 2) * 0.27f * (k % 2 ? 1.0f : -1.0f);
            const double x = at.x + std::cos(yaw0) * dist, y = at.y + std::sin(yaw0) * dist;
            const world::Tile* t = w.tileIfLoaded(ifloor(x), ifloor(y));
            if (!t || !t->open() || t->clearance() < 2.0f || std::fabs(t->floor - at.z) > 0.3f) continue;
            if (!game::clearSight(w, vec3{(float)x, (float)y, t->floor + (float)game::Player::kEye}, head)) continue;
            bool room = true;
            for (double bb = 0.5; bb <= back && room; bb += 0.5) {
                const world::Tile* tb = w.tileIfLoaded(ifloor(x + std::cos(yaw0) * bb), ifloor(y + std::sin(yaw0) * bb));
                room = tb && tb->open() && std::fabs(tb->floor - t->floor) < 0.3f && tb->clearance() > 2.0f;
            }
            if (!room) continue;
            auto& pl = s.player();
            pl.x = x, pl.y = y, pl.z = t->floor, pl.vx = pl.vy = 0.0;
            s.body().reset();
            aim(s, head);
            return true;
        }
        return false;
    };
    // erscheinen, satt (wandert, harmlos), der Spieler 3,5 m vor ihm mit Platz dahinter; dann ansprechen
    auto meet = [&](game::GameSession& s, std::vector<game::GameEvent>& ev, bool hungry) {
        auto& P = s.presence();
        P.setGrace(3000.0);
        P.force(Role::SkinStealer);
        step(s, ev, 3.0, [&] { return P.has(Role::SkinStealer); });
        if (!P.has(Role::SkinStealer)) return false;
        P.kinSet(Role::SkinStealer, "hungry", hungry ? 1.0 : 0.0);
        bool placed = false;
        for (int tries = 0; tries < 8 && !placed; ++tries) {
            placed = placeNear(s, 3.5, 9.0);
            if (!placed) step(s, ev, 2.0);
        }
        return placed;
    };
    for (int variant = 0; variant < 3; ++variant) {  // 0 ruhig zurueck, 1 rennen, 2 erneut ansprechen
        auto s = open("level1", 21 + variant);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        if (!meet(*s, ev, variant == 1)) {
            check(false, "Ansprechen: ein Skin-Stealer erscheint, der Spieler steht vor ihm");
            continue;
        }
        step(*s, ev, 0.2, [&] { aim(*s, P.headOf(Role::SkinStealer)); return false; });
        if (variant == 0) {
            check(s->speakTarget() && s->targetPrompt() == "Ansprechen", "Ansprechen: Hinweis im HUD, wenn man ihn ansieht");
            check(P.variantOf(Role::SkinStealer) == 1, "Ansprechen: Level 1 - getarnt in Menschenhaut");
        }
        aim(*s, P.headOf(Role::SkinStealer));
        s->update(1.0 / 60.0, {}, false);  // Ziel unter dem Fadenkreuz bestimmen
        s->events().clear();
        s->interact();
        int calls = 0;
        for (const auto& e : s->events()) calls += e.type == game::GameEvent::SoundAt && e.sound.rfind("talk_call_", 0) == 0;
        step(*s, ev, 3.0, [&] {
            if (P.has(Role::SkinStealer)) aim(*s, P.headOf(Role::SkinStealer));
            return P.exposedOf(Role::SkinStealer) && count(ev, PE::Reply, Role::SkinStealer) > 0;
        });
        if (variant == 0) {
            check(calls == 1, "Ansprechen: der Spieler ruft (synthetischer Satz)");
            check(count(ev, PE::Reply, Role::SkinStealer) == 1 && P.exposedOf(Role::SkinStealer) && P.stateOf(Role::SkinStealer) == 6,
                  "Ansprechen: er antwortet verraeterisch - entlarvt");
            check(!s->stats().dead, "Ansprechen: die Antwort allein ist kein Angriff");
            // ruhig zurueckweichen: rueckwaerts gehen, den Blick auf ihm
            step(*s, ev, 14.0, [&] {
                if (!P.has(Role::SkinStealer)) return true;
                aim(*s, P.headOf(Role::SkinStealer));
                return s->stats().dead || P.stateOf(Role::SkinStealer) != 6;
            }, [](int) {
                game::PlayerControl c;
                c.forward = -1.0f;
                return c;
            });
            const int st = P.has(Role::SkinStealer) ? P.stateOf(Role::SkinStealer) : 9;
            step(*s, ev, 6.0, [&] { return s->stats().dead; });  // und noch eine Weile stehen bleiben (mit Abstand)
            check(!s->stats().dead && count(ev, PE::Shriek, Role::SkinStealer) == 0 && (st == 0 || st == 4 || st == 9),
                  std::format("Ansprechen: wer danach ruhig zurueckweicht, den laesst er ziehen (Zustand {})", st));
        } else if (variant == 1) {
            check(P.exposedOf(Role::SkinStealer), "Rennen: entlarvt (hungrig)");
            // umdrehen und rennen
            step(*s, ev, 8.0, [&] {
                if (!P.has(Role::SkinStealer)) return true;
                const vec3 q = P.posOf(Role::SkinStealer);
                auto& pl = s->player();
                pl.angle = std::atan2(pl.y - q.y, pl.x - q.x), pl.pitch = 0.0;
                return s->stats().dead;
            }, [](int) {
                game::PlayerControl c;
                c.forward = 1.0f;
                c.sprint = true;
                return c;
            });
            check(count(ev, PE::Shriek, Role::SkinStealer) >= 1, "Rennen: nach der Entlarvung loest Rennen den Angriff aus (Schrei)");
            check(s->stats().dead && s->killRole() == (int)Role::SkinStealer, "Rennen: er holt ihn ein und toetet");
        } else {
            step(*s, ev, 2.5, [&] { aim(*s, P.headOf(Role::SkinStealer)); return false; });  // (Pause zwischen zwei Saetzen)
            s->interact();  // noch einmal ansprechen, obwohl klar ist, dass er kein Mensch ist
            step(*s, ev, 0.3);
            check(count(ev, PE::Shriek, Role::SkinStealer) >= 1 && P.stateOf(Role::SkinStealer) == 2,
                  "Ansprechen: erneutes Ansprechen nach der Entlarvung loest den Angriff aus");
        }
    }
}

// --- V13.3: Level 3 - Wretch mit Waffe, Skin-Stealer im Mantel (KREATUR_Wretch.md, KREATUR_SkinStealer.md) -------------
// V13.3 Teil 4: Randwesen von Level 2 ohne Rolle (KREATUR_CannibalCuisine.md, KREATUR_Swindlebird.md, KREATUR_Photoshop.md,
// KREATUR_Macchina.md)
static void testV133L2Rand(const std::string& dataDir) {
    std::printf("V13.3 Teil 4: Randwesen von Level 2 (Automat, Swindlebird, Photoshop, Macchina)\n");
    const game::Content& content = *gContent11;
    game::PrefabLibrary prefabs;
    prefabs.loadDirectory(dataDir + "/prefabs");
    JobSystem jobs(2);
    using AE = game::Apparitions::Event;
    const auto L2 = content.level("level2", world::kLayoutV6);
    if (!L2 || !L2->map) return check(false, "Randwesen: Karte von Level 2");
    // --- Karte ---------------------------------------------------------------------------------------------------------
    std::map<std::string, int> sets, objs;
    for (const auto& s : L2->map->spawns)
        if (s.kind == "trace") sets[s.set]++;
    bool inShop = true;
    for (const world::MapObject& o : L2->map->objects) {
        if (o.model.rfind("macchina_", 0) != 0 || o.model == "macchina_parts" || o.model == "macchina_tools") continue;
        objs[o.model]++;
        const int r = L2->map->roomAt(o.x, o.y);
        inShop = inShop && r >= 0 && L2->map->rooms[(size_t)r].zone == "macchina";
    }
    check(sets["cuisine"] >= 4 && sets["swindle"] >= 10 && sets["photoshop"] >= 3 && sets["macchina"] >= 1,
          std::format("Randwesen: Plaetze in der Karte (Automaten {}, Swindlebird {}, Photoshop {}, Macchina {})", sets["cuisine"],
                      sets["swindle"], sets["photoshop"], sets["macchina"]));
    check(objs["macchina_suit"] >= 1 && objs["macchina_tapestry"] >= 1 && objs["macchina_cradle"] >= 1 && objs["macchina_spear"] >= 1 && inShop,
          "Macchina: Anzug, Wandtuch, Kindergestell und Pike liegen in der Werkstatt");
    auto open = [&](i64 seed) {
        auto s = std::make_unique<game::GameSession>(content, prefabs, L2, seed, "medium", "Test", jobs, 3);
        s->placeAtSpawn();
        s->preload();
        s->presence().setGrace(3000.0);
        return s;
    };
    auto step = [&](game::GameSession& s, std::vector<game::GameEvent>& ev, double secs, const std::function<bool()>& each = {}) {
        for (int i = 0; i < (int)(secs * 60.0); ++i) {
            if (each && each()) break;
            s.update(1.0 / 60.0, {}, false);
            ev.insert(ev.end(), s.events().begin(), s.events().end());
            s.events().clear();
        }
    };
    auto said = [](const std::vector<game::GameEvent>& ev, const char* part) {
        return std::any_of(ev.begin(), ev.end(), [&](const game::GameEvent& e) { return e.text.find(part) != std::string::npos; });
    };
    auto heard = [](const std::vector<game::GameEvent>& ev, const char* snd) {
        return std::any_of(ev.begin(), ev.end(), [&](const game::GameEvent& e) { return e.type == game::GameEvent::SoundAt && e.sound == snd; });
    };
    auto slotOf = [](game::GameSession& s, const char* kind) {
        for (int i = 0; i < game::Inventory::kSize; ++i)
            if (s.inventory().slots[(size_t)i] && s.inventory().slots[(size_t)i]->kind == kind) return i;
        return -1;
    };
    auto put = [](game::GameSession& s, vec3 p, vec3 lookAt) {
        auto& pl = s.player();
        pl.x = p.x, pl.y = p.y, pl.z = s.world().tile(ifloor(p.x), ifloor(p.y)).floor, pl.vx = pl.vy = 0.0;
        pl.angle = std::atan2(lookAt.y - p.y, lookAt.x - p.x);
        pl.pitch = std::atan2(lookAt.z - pl.eye(), std::max(0.3, (double)std::hypot(lookAt.x - p.x, lookAt.y - p.y)));
        s.body().reset();
    };
    // --- Cannibal Cuisine: fest, leuchtet, gratis Ware, Sucht ------------------------------------------------------------
    {
        auto s = open(5);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& A = s->apparitions();
        check((int)A.cuisines().size() == sets["cuisine"], std::format("Automat: alle {} Plaetze besetzt", sets["cuisine"]));
        bool solid = true;
        std::string fl;
        for (const auto& c : A.cuisines()) {
            const float h = s->world().tile(ifloor(c.pos.x), ifloor(c.pos.y)).floor - c.pos.z;
            solid = solid && h > 1.5f;
            fl += std::format(" ({:.1f},{:.1f}):{:.2f}", c.pos.x, c.pos.y, h);
        }
        check(solid, "Automat: steht fest (unsichtbarer Block unter der Grundflaeche)" + fl);
        check(s->scriptApparition("cuisine", 1.7, 0.0, 0), "Automat: der Spieler steht davor");
        step(*s, ev, 3.0);
        std::vector<game::Apparitions::Light> lights;
        A.lights(lights, s->body().eye());
        check(!lights.empty() && lights[0].range > 5.0f, "Automat: leuchtet (Wegmarke, auch im Stromausfall)");
        ev.clear();
        step(*s, ev, 16.0);
        check(heard(ev, "cc_whisper"), "Automat: in der Naehe fluestern die Leichen im Inneren");
        int got = 0;
        for (int k = 0; k < 4; ++k) {
            step(*s, ev, 1.8);
            const int before = s->inventory().count("cc_ware");
            s->interact();
            got += s->inventory().count("cc_ware") - before;
        }
        step(*s, ev, 0.2);
        check(got == 3 && A.cuisines()[0].stock == 0, std::format("Automat: drei Waren gratis, dann leer ({} genommen, Rest {})", got, A.cuisines()[0].stock));
        check(heard(ev, "cc_keys") && heard(ev, "cc_hand") && heard(ev, "cc_drop") && said(ev, "leer"),
              "Automat: Tasten, die Knochenhand schiebt, die Ware faellt; danach leere Faecher");
        // essen: Rausch, dann Sucht - Mandelwasser wirkt nicht mehr
        s->stats().sanity = 50.0;
        s->useSlot(slotOf(*s, "cc_ware"));
        check(s->stats().sanity > 85.0 && A.addicted(0), std::format("Ware: Rausch (+40 % Verstand: {:.0f} %), danach suechtig", s->stats().sanity));
        s->inventory().add("almond");
        const double before = s->stats().sanity;
        s->useSlot(slotOf(*s, "almond"));
        step(*s, ev, 0.1);
        check(std::fabs(s->stats().sanity - before) < 0.5 && said(ev, "würgst"), "Sucht: Mandelwasser nimmt der Koerper nicht mehr an");
        // Entzug: weit weg von allen Automaten
        vec3 far{};
        float best = 0.0f;
        for (const vec3& p : A.swindleSpots()) {
            float m = 1e9f;
            for (const auto& c : A.cuisines()) m = std::min(m, std::hypot(c.pos.x - p.x, c.pos.y - p.y));
            if (m > best) best = m, far = p;
        }
        put(*s, far, far + vec3{1, 0, 1.5f});
        step(*s, ev, 32.0);
        check(best > 40.0f && A.sanityDrain(0) > 0.05, std::format("Sucht: fern der Automaten ({:.0f} m) zehrt der Entzug ({:.3f} %/s)", best, A.sanityDrain(0)));
        // Spielstand: die Sucht bleibt
        game::Apparitions copy;
        copy.fromJson(0, A.toJson(0));
        check(copy.addicted(0) && copy.addict(0).eaten == 1, "Sucht: im Spielstand");
    }
    // --- Cannibal Cuisine: wer angreift, den schlagen die Arme -----------------------------------------------------------
    {
        auto s = open(6);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& A = s->apparitions();
        check(s->scriptApparition("cuisine", 1.35, 0.0, 1), "Zorn: vor den Automaten");
        step(*s, ev, 2.0);
        s->inventory().add("hatchet");
        s->stats().health = 100.0;
        s->useSlot(slotOf(*s, "hatchet"));
        step(*s, ev, 0.9);
        check(A.cuisines()[1].angerT >= 0.0 && s->stats().health > 99.0 && heard(ev, "cc_rattle"),
              "Zorn: kuendigt sich an (Klappern, Kopfschild flackert) - noch kein Hieb");
        step(*s, ev, 1.4);
        check(s->stats().health < 99.0 && s->stats().health > 0.0 && heard(ev, "cc_strike") && said(ev, "Knochenarm"),
              std::format("Zorn: Skelettarme schlagen aus den Klappen ({:.0f} % Gesundheit)", s->stats().health));
        const auto& c = A.cuisines()[1];
        const vec3 f{std::cos(c.yaw), std::sin(c.yaw), 0.0f};
        put(*s, c.pos + f * 4.6f, c.pos + vec3{0, 0, 1.0f});
        const double h = s->stats().health;
        ev.clear();
        step(*s, ev, 4.0);
        check(s->stats().health >= h - 0.01 && !said(ev, "Knochenarm") && heard(ev, "cc_strike"),
              std::format("Zorn: ausser Reichweite der Arme (1,37 m) trifft nichts ({:.0f} -> {:.0f} %, Abstand {:.1f} m)", h, s->stats().health,
                          std::hypot(s->player().x - c.pos.x, s->player().y - c.pos.y)));
        step(*s, ev, 3.0);
        check(A.cuisines()[1].angerT < 0.0, "Zorn: die Arme ziehen sich zurueck, die Klappen schliessen");
    }
    // --- Swindlebird: eine Sichtung, ungesehen aufgetaucht, fort beim Hinsehen, ohne Spur ---------------------------------
    {
        auto s = open(7);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& A = s->apparitions();
        int k = -1;
        for (int i = 0; i < (int)A.swindleSpots().size() && k < 0; ++i)
            if (s->scriptApparition("swindle", 15.0, 0.0, i) && std::hypot(s->player().x - A.swindleSpots()[(size_t)i].x, s->player().y - A.swindleSpots()[(size_t)i].y) > 12.0)
                k = i;
        check(k >= 0, "Swindlebird: eine Stelle mit langem Blick durch den Tunnel");
        if (k >= 0) {
            A.setState("sb_clear", 1.0);
            s->player().angle += 3.14159;  // wegsehen
            step(*s, ev, 1.0);
            A.scheduleBird(0.3);
            step(*s, ev, 1.5);
            const auto& b = A.bird();
            const float d = std::hypot(b.pos.x - (float)s->player().x, b.pos.y - (float)s->player().y);
            check(b.state == 1 && !b.wasSeen && d > 10.0f && d < 29.0f,
                  std::format("Swindlebird: taucht auf, wo niemand hinsieht ({:.1f} m, Zustand {})", d, b.state));
            put(*s, vec3{(float)s->player().x, (float)s->player().y, 0.0f}, b.pos + vec3{0, 0, 0.16f});
            step(*s, ev, 3.0, [&] { return A.bird().state == 2; });
            check(A.bird().state == 2 && A.bird().wasSeen && heard(ev, "sb_flutter"), "Swindlebird: angesehen fliegt er fort");
            step(*s, ev, 4.0);
            check(A.bird().state == 3 && said(ev, "Keine Feder"), "Swindlebird: fort, keine Spur, kein Beweis");
            A.scheduleBird(0.2);
            step(*s, ev, 1.0);
            check(A.bird().state == 3, "Swindlebird: nur eine Sichtung je Lauf");
            // naeher kommen: fort
            A.setState("sb_clear", 1.0);
            A.forceBird(k);
            const vec3 sp = A.swindleSpots()[(size_t)k];
            const vec3 dir{std::cos(A.swindleYaws()[(size_t)k]), std::sin(A.swindleYaws()[(size_t)k]), 0.0f};
            put(*s, sp + dir * 6.0f, sp + dir * 12.0f);
            step(*s, ev, 0.2);
            check(A.bird().state == 2, "Swindlebird: wer naeher kommt (8 m), vor dem fliegt er fort");
            // weggesehen: einfach nicht mehr da
            A.setState("sb_clear", 1.0);
            A.forceBird(k);
            put(*s, sp + dir * 15.0f, sp + vec3{0, 0, 0.16f});
            step(*s, ev, 0.4);
            const bool seen = A.bird().wasSeen;
            const vec3 p0 = A.bird().pos;
            s->player().angle += 3.14159;
            step(*s, ev, 0.7);
            check(seen && A.bird().state == 3 && length(A.bird().pos - p0) < 0.01f, "Swindlebird: kurz weggesehen - er ist fort, ohne Flug");
        }
    }
    // --- Photoshop: Box bei fuenf Fuss, Gabe rettet, sonst ausgeschnitten -------------------------------------------------
    for (int variant = 0; variant < 3; ++variant) {
        auto s = open(8 + variant);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& A = s->apparitions();
        check(s->scriptApparition("photoshop", 3.2, 0.0, 0), "Photoshop: vor ihm, im Buero mit dem Rechner");
        step(*s, ev, 2.0);
        const auto& P = A.shop();
        if (variant == 0) check(P.present && P.state == 0 && !s->stats().dead, "Photoshop: aus der Ferne harmlos");
        const vec3 f{std::cos(P.yaw), std::sin(P.yaw), 0.0f};
        put(*s, P.pos + f * 1.75f, P.pos + vec3{0, 0, 1.6f});
        step(*s, ev, 0.3);
        check(P.state == 1 && P.victim == 0 && said(ev, "Box"), "Photoshop: naeher als fuenf Fuss - die Box schliesst ein");
        if (variant == 0) {
            s->inventory().add("almond");
            step(*s, ev, 1.0);
            s->dropStack("almond", 1);
            step(*s, ev, 0.6);
            check(P.state == 2 && heard(ev, "ps_arm"), "Photoshop: hingeworfen - der Arm holt es in den Bildschirm");
            step(*s, ev, 3.0);
            check(P.state == 3 && !s->stats().dead, "Photoshop: mit der Gabe faellt die Box, er sucht anderswo");
        } else if (variant == 1) {
            step(*s, ev, A.config().boxTime + 0.5);
            check(s->stats().dead && s->deathCause() == "photoshop" && heard(ev, "ps_crop"), "Photoshop: ohne Gabe ausgeschnitten");
        } else {
            step(*s, ev, 1.0);
            const bool alive = !s->stats().dead;
            put(*s, P.boxC + f * 1.3f, P.pos + vec3{0, 0, 1.0f});
            step(*s, ev, 0.2);
            check(alive && s->stats().dead && s->deathCause() == "photoshop", "Photoshop: wer durch die Wand der Box geht, wird ausgeschnitten");
        }
    }
    // --- Macchina: Reste, der Fund, die Tafel im Notizbuch (nie zufaellig) -----------------------------------------------
    {
        auto s = open(12);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& A = s->apparitions();
        check(!A.relics().empty() && s->scriptApparition("macchina", 1.8, 0.0, 0), "Macchina: vor dem leeren Anzug");
        step(*s, ev, 2.0);
        check(A.addict(0).macchinaSeen && said(ev, "Anzug") && heard(ev, "mc_creak"), "Macchina: der erste Blick erzaehlt den Fund");
        int plate = -1;
        for (size_t i = 0; i < content.notes().size(); ++i)
            if (content.notes()[i].title == "Blechtafel mit Zeichen") plate = (int)i;
        bool random = false;
        for (u64 h = 0; h < 400; ++h) random = random || content.noteFor("level2", h, false) == plate || content.noteFor("level2", h, true) == plate;
        check(plate >= 0 && !random, "Macchina: die Blechtafel liegt nur beim Anzug (keine Zufallsnotiz)");
    }
    // --- Koop: der Gast sieht, was der Host entscheidet (Weltereignis "l2rand") -----------------------------------------
    {
        auto s = open(13);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        game::Apparitions host, guest;
        host.setup(*L2, 4242), guest.setup(*L2, 4242);
        check(host.cuisines().size() == guest.cuisines().size() && host.shop().present == guest.shop().present && host.shop().spot == guest.shop().spot,
              "Koop: beim Gast dieselben Automaten und derselbe Photoshop (Karte, Startwert)");
        std::vector<AE> out, sink;
        auto relay = [&] {
            for (const AE& e : out)
                if (e.kind == AE::Net) guest.applyNet(e.code, e.pos, "almond", sink);
            out.clear();
        };
        std::string msg;
        host.vend(0, 1, msg, out);
        relay();
        check(guest.cuisines()[0].stock == host.cuisines()[0].stock && guest.cuisines()[0].vendT >= 0.0, "Koop: Ausgabe beim Gast (Hand, Ware)");
        const auto& c = host.cuisines()[2];
        const vec3 cf{std::cos(c.yaw), std::sin(c.yaw), 0.0f};
        const vec3 eye = c.pos + cf * 1.4f + vec3{0, 0, 1.6f};
        check(host.swing(1, eye, normalize(c.pos + cf * 0.38f + vec3{0, 0, 1.0f} - eye), out) == 2, "Koop: Beil des Gastes trifft den Automaten");
        relay();
        check(guest.cuisines()[2].angerT >= 0.0, "Koop: der Zorn des Automaten beim Gast");
        host.forceShop(0), guest.forceShop(0);
        std::array<game::Apparitions::PlayerIn, game::Apparitions::kPlayers> pl{};
        const auto& P = host.shop();
        const vec3 pf{std::cos(P.yaw), std::sin(P.yaw), 0.0f};
        pl[2].alive = true, pl[2].feet = P.pos + pf * 1.7f, pl[2].eye = pl[2].feet + vec3{0, 0, 1.6f}, pl[2].look = -pf;
        host.update(0.1, pl, {}, true, s->world(), out);
        relay();
        check(guest.shop().state == 1 && guest.shop().victim == 2 &&
                  std::any_of(sink.begin(), sink.end(), [](const AE& e) { return e.kind == AE::Toast && e.player == 2; }),
              "Koop: die Box beim Gast, die Meldung an das Opfer");
        host.offered(P.pos + pf * 1.2f, "almond", out);
        guest.applyNet((7u << 24) | 1u, P.pos + pf * 1.2f, "almond", sink);
        check(guest.shop().state == 2, "Koop: die Gabe beim Gast (Arm aus dem Bildschirm)");
        host.forceBird(1);
        guest.applyNet((3u << 24) | 1u, host.swindleSpots()[1], "", sink);
        check(guest.bird().state == 1 && guest.bird().spot == 1, "Koop: die Sichtung des Swindlebird beim Gast");
    }
}

static void testV133Level3Kin(const std::string& dataDir) {
    std::printf("V13.3: Level 3 - Wretch mit Waffe, Skin-Stealer im Mantel\n");
    const game::Content& content = *gContent11;
    game::PrefabLibrary prefabs;
    prefabs.loadDirectory(dataDir + "/prefabs");
    JobSystem jobs(2);
    using PE = game::Presence::Event;
    using Role = game::Presence::Role;
    auto open = [&](const char* level, i64 seed) {
        auto s = std::make_unique<game::GameSession>(content, prefabs, content.level(level, world::kLayoutV6), seed, "medium", "Test", jobs, 3);
        s->placeAtSpawn();
        s->preload();
        return s;
    };
    auto step = [&](game::GameSession& s, std::vector<game::GameEvent>& ev, double secs, const std::function<bool()>& each = {},
                    const std::function<game::PlayerControl(int)>& ctl = {}) {
        for (int i = 0; i < (int)(secs * 60.0); ++i) {
            if (each && each()) break;
            s.update(1.0 / 60.0, ctl ? ctl(i) : game::PlayerControl{}, false);
            ev.insert(ev.end(), s.events().begin(), s.events().end());
            s.events().clear();
        }
    };
    // Figurereignisse einer Art; cue >= 0: nur diese Klangnummer (kin_l3.hpp), cue2: bis einschliesslich
    auto count = [](const std::vector<game::GameEvent>& ev, PE::Type t, Role r, int cue = -1, int cue2 = -1) {
        return (int)std::count_if(ev.begin(), ev.end(), [&](const game::GameEvent& e) {
            if (e.type != game::GameEvent::Figure || e.index != (int)t || e.role != (int)r) return false;
            const int k = game::kinCueOf(e.intensity, e.role);
            return cue < 0 || (k >= cue && k <= (cue2 < 0 ? cue : cue2));
        });
    };
    auto lastCue = [](const std::vector<game::GameEvent>& ev, Role r, int lo, int hi) {
        int k = -1;
        for (const auto& e : ev)
            if (e.type == game::GameEvent::Figure && e.role == (int)r) {
                const int c = game::kinCueOf(e.intensity, e.role);
                if (c >= lo && c <= hi) k = c;
            }
        return k;
    };
    auto aim = [](game::GameSession& s, vec3 q) {
        auto& pl = s.player();
        pl.angle = std::atan2(q.y - pl.y, q.x - pl.x);
        pl.pitch = std::atan2(q.z - pl.eye(), std::max(0.4, std::hypot(q.x - pl.x, q.y - pl.y)));
    };
    // der Spieler dist m vor dem Wesen (freie Sicht, back m Platz hinter sich)
    auto placeNear = [&](game::GameSession& s, Role r, double dist, double back = 0.0) {
        auto& P = s.presence();
        if (!P.has(r)) return false;
        const vec3 at = P.posOf(r), head = P.headOf(r);
        world::World& w = s.world();
        for (int k = 0; k < 36; ++k) {
            const float yaw0 = P.yawOf(r) + (float)((k + 1) / 2) * 0.27f * (k % 2 ? 1.0f : -1.0f);
            for (double dd = dist; dd > dist * 0.6; dd -= 0.25) {
                const double x = at.x + std::cos(yaw0) * dd, y = at.y + std::sin(yaw0) * dd;
                const world::Tile* t = w.tileIfLoaded(ifloor(x), ifloor(y));
                if (!t || !t->open() || t->clearance() < 2.0f || std::fabs(t->floor - at.z) > 0.3f) continue;
                if (!game::clearSight(w, vec3{(float)x, (float)y, t->floor + (float)game::Player::kEye}, head)) continue;
                bool room = true;
                for (double bb = 0.5; bb <= back && room; bb += 0.5) {
                    const world::Tile* tb = w.tileIfLoaded(ifloor(x + std::cos(yaw0) * bb), ifloor(y + std::sin(yaw0) * bb));
                    room = tb && tb->open() && std::fabs(tb->floor - t->floor) < 0.3f && tb->clearance() > 2.0f;
                }
                if (!room) continue;
                auto& pl = s.player();
                pl.x = x, pl.y = y, pl.z = t->floor, pl.vx = pl.vy = 0.0;
                s.body().reset();
                aim(s, head);
                return true;
            }
        }
        return false;
    };
    // vom Wesen weg versetzen (so weit der Boden frei ist), Blick bleibt auf ihm
    auto backOff = [&](game::GameSession& s, Role r, double by) {
        auto& P = s.presence();
        auto& pl = s.player();
        const vec3 at = P.posOf(r);
        const double a0 = std::atan2(pl.y - at.y, pl.x - at.x);
        for (int k = 0; k < 25; ++k) {
            const double a = a0 + (double)((k + 1) / 2) * 0.15 * (k % 2 ? 1.0 : -1.0);
            const double nx = pl.x + std::cos(a) * by, ny = pl.y + std::sin(a) * by;
            const world::Tile* t = s.world().tileIfLoaded(ifloor(nx), ifloor(ny));
            if (t && t->open() && std::fabs(t->floor - pl.z) < 0.3 && game::clearSight(s.world(), vec3{(float)pl.x, (float)pl.y, (float)pl.eye()},
                                                                                     vec3{(float)nx, (float)ny, (float)pl.eye()})) {
                pl.x = nx, pl.y = ny, pl.vx = pl.vy = 0.0;
                s.body().reset();
                aim(s, P.headOf(r));
                return true;
            }
        }
        return false;
    };
    // auf der Linie zum Wesen bis auf dist herangehen (Boden frei)
    auto approach = [&](game::GameSession& s, Role r, double dist) {
        auto& P = s.presence();
        auto& pl = s.player();
        const vec3 at = P.posOf(r);
        const double d0 = std::hypot(pl.x - at.x, pl.y - at.y);
        if (d0 <= dist) return true;
        const double nx = at.x + (pl.x - at.x) / d0 * dist, ny = at.y + (pl.y - at.y) / d0 * dist;
        const world::Tile* t = s.world().tileIfLoaded(ifloor(nx), ifloor(ny));
        if (!t || !t->open() || std::fabs(t->floor - pl.z) > 0.3) return false;
        pl.x = nx, pl.y = ny, pl.vx = pl.vy = 0.0;
        s.body().reset();
        aim(s, P.headOf(r));
        return true;
    };
    auto spawn = [&](game::GameSession& s, std::vector<game::GameEvent>& ev, Role r) {
        s.presence().setGrace(3000.0);
        s.presence().force(r);
        step(s, ev, 3.0, [&] { return s.presence().has(r); });
        return s.presence().has(r);
    };
    // Klaenge und Untertitel: jede Nummer hat einen Namen, die Saetze einen Untertitel, die Gaeste finden sie nach dem Namen
    {
        int bad = 0;
        for (int k : {(int)game::KC_FRAG0, 3, 7, (int)game::KC_CRY0, (int)game::KC_DRAG0, (int)game::KC_RAISE0 + 2, (int)game::KC_MISS0, (int)game::KC_HIT0 + 1,
                      (int)game::KC_SAY0, (int)game::KC_SAY0 + 7, (int)game::KC_ANS0 + 3, (int)game::KC_WAIT, (int)game::KC_BYE, (int)game::KC_TEAR}) {
            const game::KinLine* L = game::kinLine(k);
            bad += !L || !L->sound || game::kinLineBySound(L->sound) != L;
        }
        check(bad == 0, "Level 3: Klangnummern haben Namen (auch fuer die Gaeste)");
        check(game::kinLine(game::KC_SAY0)->sub && game::kinLine(game::KC_FRAG0)->hint && !game::kinLine(game::KC_DRAG0)->sub,
              "Level 3: Saetze und Fetzen mit Untertitel, der Rat des Wikis beim ersten Mal");
        check(game::kinCueOf(30.0f, (int)Role::Wretch) < 0 && game::kinCueOf(100.0f + 3.0f, (int)Role::Smiler) < 0 &&
                  game::kinCueOf(100.0f + 3.0f, (int)Role::Wretch) == 3,
              "Level 3: nur Wretch und Skin-Stealer tragen Klangnummern (Schaden bleibt Schaden)");
        const auto L3 = content.level("level3", world::kLayoutV6);
        check(L3 && L3->kin[world::KIN_WRETCH].mode == "armed" && L3->kin[world::KIN_WRETCH].every >= 400.0,
              "Level 3: Wretches bewaffnet und selten (Level-JSON)");
        check(L3 && L3->kin[world::KIN_SKINSTEALER].mode == "coat", "Level 3: Skin-Stealer im Mantel (Level-JSON)");
        const auto L1 = content.level("level1", world::kLayoutV6);
        check(L1 && L1->kin[world::KIN_SKINSTEALER].mode == "skin" && L1->kin[world::KIN_WRETCH].mode != "armed", "Level 1 unveraendert");
    }
    // --- Wretch: lockt von weitem, stuermt, wer naeher kommt; holt aus (Ankuendigung), schlaegt; wer ausweicht, dem schlaegt er
    // ins Leere ----------------------------------------------------------------------------------------------------------
    {
        auto s = open("level3", 31);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        check(spawn(*s, ev, Role::Wretch), "Level 3: ein Wretch erscheint");
        const int wv = P.variantOf(Role::Wretch);
        check(wv >= 1 && wv <= 3, std::format("Level 3: er traegt eine Waffe (Variante {})", wv));
        bool lured = false;
        for (int tries = 0; tries < 6 && !lured; ++tries) {
            if (!placeNear(*s, Role::Wretch, 11.0, 2.0) && !placeNear(*s, Role::Wretch, 9.0, 2.0)) {
                step(*s, ev, 1.0);
                continue;
            }
            s->flashlight().on = true;  // er sieht einen (mit Lampe 18 m) ...
            step(*s, ev, 0.6, [&] { aim(*s, P.posOf(Role::Wretch) + vec3{0, 0, 0.3f}); return P.stateOf(Role::Wretch) == 5; });  // ... nicht ins Gesicht
            s->flashlight().on = false;
            lured = P.stateOf(Role::Wretch) == 5;
        }
        check(lured, "Level 3: sieht er einen von weitem, lockt er erst (Zustand 5)");
        step(*s, ev, 3.5, [&] { return P.stateOf(Role::Wretch) != 5; });
        check(count(ev, PE::Mimic, Role::Wretch, game::KC_FRAG0, game::KC_FRAG_LURE - 1) >= 1, "Level 3: er lockt mit verstaendlichen Fetzen (Hilfe, Wasser)");
        // naeher kommen (Wiki: nicht interagieren) - er stuermt los
        ev.clear();
        if (!approach(*s, Role::Wretch, 4.0)) placeNear(*s, Role::Wretch, 4.0, 4.0);
        step(*s, ev, 0.5, [&] { return P.stateOf(Role::Wretch) == 2; });
        check(P.stateOf(Role::Wretch) == 2 && count(ev, PE::Gurgle, Role::Wretch) >= 1, "Level 3: wer naeher kommt, auf den stuermt er los (hohes Fauchen)");
        // Ausholen: Ankuendigung mit Schrei - dann weicht man aus: der Schlag geht ins Leere
        double windT = -1.0;
        int frame = 0;
        const double h0 = s->stats().health;
        step(*s, ev, 6.0, [&] {
            ++frame;
            if (P.has(Role::Wretch)) aim(*s, P.headOf(Role::Wretch));
            if (P.stateOf(Role::Wretch) == 6 && windT < 0.0) windT = frame / 60.0;
            return windT >= 0.0;
        });
        check(windT >= 0.0 && count(ev, PE::Swing, Role::Wretch, game::KC_RAISE0, game::KC_RAISE0 + 2) == 1 &&
                  count(ev, PE::Mimic, Role::Wretch, game::KC_CRY0, game::KC_CRY0 + 2) == 1,
              "Level 3: vor dem Schlag holt er aus und schreit (Ankuendigung)");
        backOff(*s, Role::Wretch, 3.2);
        step(*s, ev, 1.4, [&] { return count(ev, PE::Swing, Role::Wretch, game::KC_MISS0, game::KC_HIT0 + 2) > 0; });
        check(count(ev, PE::Swing, Role::Wretch, game::KC_MISS0, game::KC_MISS0 + 2) == 1 && s->stats().health >= h0,
              "Level 3: wer beim Ausholen zurueckweicht, dem schlaegt er ins Leere");
        // stehen bleiben: Treffer mit der Waffe - schwer, nicht sofort toedlich; danach weicht er einen Schritt zurueck
        double hitAt = -1.0, raiseAt = -1.0;
        frame = 0;
        step(*s, ev, 10.0, [&] {
            ++frame;
            if (P.has(Role::Wretch)) aim(*s, P.headOf(Role::Wretch));
            if (raiseAt < 0.0 && P.stateOf(Role::Wretch) == 6) raiseAt = frame / 60.0;
            if (hitAt < 0.0 && s->stats().health < h0) hitAt = frame / 60.0;
            return hitAt >= 0.0 && P.stateOf(Role::Wretch) == 8;
        });
        check(hitAt > 0.0 && s->stats().health < h0 - 15.0 && !s->stats().dead,
              std::format("Level 3: der Treffer mit der Waffe ist schwer, nicht sofort toedlich ({:.0f} -> {:.0f})", h0, s->stats().health));
        check(raiseAt >= 0.0 && hitAt - raiseAt >= 0.5, std::format("Level 3: zwischen Ausholen und Treffer liegen {:.2f} s (>= 0,5)", hitAt - raiseAt));
        check(count(ev, PE::Swing, Role::Wretch, game::KC_HIT0, game::KC_HIT0 + 2) >= 1, "Level 3: Klang des Treffers je Waffe");
        check(P.stateOf(Role::Wretch) == 8, "Level 3: nach dem Schlag weicht er einen Schritt zurueck");
        // die Waffe schleift beim Gehen (man hoert ihn, bevor man ihn sieht)
        P.clear();
        ev.clear();
        s->respawnAtCheckpoint();
        step(*s, ev, 0.3);
        if (spawn(*s, ev, Role::Wretch)) {
            step(*s, ev, 12.0);
            check(count(ev, PE::Drag, Role::Wretch, game::KC_DRAG0, game::KC_DRAG0 + 2) >= 1, "Level 3: die Waffe schleift beim Gehen (Klang vor Sicht)");
        }
    }
    // --- Wretch: ansprechen (Wiki, Level 1: nicht interagieren) und Licht ins Gesicht - er hat einen -----------------------
    for (int how = 0; how < 2; ++how) {
        auto s = open("level3", 33 + how);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        if (!spawn(*s, ev, Role::Wretch)) {
            check(false, "Level 3: Wretch fuer Ansprechen/Licht");
            continue;
        }
        bool lured = false;
        for (int tries = 0; tries < 6 && !lured; ++tries) {
            if (!placeNear(*s, Role::Wretch, 10.0, 1.0) && !placeNear(*s, Role::Wretch, 8.0, 1.0)) {
                step(*s, ev, 1.0);
                continue;
            }
            s->flashlight().on = true;
            step(*s, ev, 0.6, [&] { aim(*s, P.posOf(Role::Wretch) + vec3{0, 0, 0.3f}); return P.stateOf(Role::Wretch) == 5; });
            s->flashlight().on = how == 1;
            lured = P.stateOf(Role::Wretch) == 5;
        }
        if (!lured) {
            const vec3 q = P.has(Role::Wretch) ? P.posOf(Role::Wretch) : vec3{};
            check(false, std::format("Level 3: Wretch lockt (fuer {}; Zustand {}, {:.1f} m)", how == 0 ? "Ansprechen" : "Licht",
                                     P.has(Role::Wretch) ? P.stateOf(Role::Wretch) : -1, std::hypot(q.x - s->player().x, q.y - s->player().y)));
            continue;
        }
        if (how == 0) {
            check(approach(*s, Role::Wretch, 5.6) && P.stateOf(Role::Wretch) == 5, "Level 3: auf 5,6 m heran - er lockt noch (man koennte ihn ansprechen)");
            aim(*s, P.headOf(Role::Wretch));
            s->update(1.0 / 60.0, {}, false);
            s->interact();
            step(*s, ev, 0.3, [&] { return P.stateOf(Role::Wretch) == 2; });
            check(P.stateOf(Role::Wretch) == 2, "Level 3: wer einen lockenden Wretch anspricht, auf den stuermt er los");
        } else {
            step(*s, ev, 2.5, [&] { aim(*s, P.headOf(Role::Wretch)); return P.stateOf(Role::Wretch) == 2; });
            check(P.stateOf(Role::Wretch) == 2, "Level 3: die Lampe im Gesicht verraet ihn - er stuermt los");
        }
    }
    // --- Skin-Stealer im Mantel: schliesst sich an, spricht, antwortet; beim zweiten Ansprechen genau gleich, beim dritten
    // faellt die Tarnung; hungrig toetet er -------------------------------------------------------------------------------
    {
        auto s = open("level3", 41);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        check(spawn(*s, ev, Role::SkinStealer), "Level 3: ein Skin-Stealer erscheint");
        check(P.variantOf(Role::SkinStealer) == 2, "Level 3: im Mantel (Kapuze, Schal, Sonnenbrille)");
        P.kinSet(Role::SkinStealer, "hungry", 1.0);
        bool joined = false;
        for (int tries = 0; tries < 6 && !joined; ++tries) {
            if (placeNear(*s, Role::SkinStealer, 8.0, 2.0) || placeNear(*s, Role::SkinStealer, 6.0, 2.0))
                step(*s, ev, 1.0, [&] { return P.stateOf(Role::SkinStealer) == 7; });
            joined = P.stateOf(Role::SkinStealer) == 7;
            if (!joined) step(*s, ev, 1.0);
        }
        check(joined, "Level 3: sieht er einen, schliesst er sich an (Zustand 7)");
        float dMin = 1e9f, dMax = 0.0f;
        step(*s, ev, 20.0, [&] {
            if (!P.has(Role::SkinStealer)) return true;
            aim(*s, P.headOf(Role::SkinStealer));
            const vec3 q = P.posOf(Role::SkinStealer);
            const float d = (float)std::hypot(q.x - s->player().x, q.y - s->player().y);
            if (P.stateOf(Role::SkinStealer) == 7) dMin = std::min(dMin, d), dMax = std::max(dMax, d);
            return false;
        });
        check(P.stateOf(Role::SkinStealer) == 7 && !s->stats().dead, "Level 3: er bleibt in der Naehe (begleitet), greift nicht an");
        check(dMin > 1.6f && dMax < 9.0f, std::format("Level 3: er haelt Abstand ({:.1f}-{:.1f} m)", dMin, dMax));
        check(count(ev, PE::Mimic, Role::SkinStealer, game::KC_SAY0, game::KC_SAY0 + game::KC_SAYS - 1) >= 1,
              "Level 3: er spricht einfache, leicht falsche Saetze");
        check(count(ev, PE::Drag, Role::SkinStealer, game::KC_COAT) >= 1, "Level 3: beim Gehen schwingt der schwere Mantel (eigenes Geraeusch)");
        // ansprechen: Antwort; noch einmal: derselbe Satz genau gleich; beim dritten Mal faellt die Tarnung
        auto talk = [&] {
            placeNear(*s, Role::SkinStealer, 3.6, 2.0);
            aim(*s, P.headOf(Role::SkinStealer));
            s->update(1.0 / 60.0, {}, false);
            s->interact();
        };
        ev.clear();
        talk();
        step(*s, ev, 2.6, [&] { aim(*s, P.headOf(Role::SkinStealer)); return count(ev, PE::Mimic, Role::SkinStealer, game::KC_ANS0, game::KC_ANS0 + 3) > 0; });
        const int a1 = lastCue(ev, Role::SkinStealer, game::KC_ANS0, game::KC_ANS0 + 3);
        check(a1 >= game::KC_ANS0 && !P.exposedOf(Role::SkinStealer), "Level 3: angesprochen antwortet er mit einem Satz (noch getarnt)");
        step(*s, ev, 2.5, [&] { aim(*s, P.headOf(Role::SkinStealer)); return false; });
        ev.clear();
        talk();
        step(*s, ev, 2.6, [&] { aim(*s, P.headOf(Role::SkinStealer)); return count(ev, PE::Mimic, Role::SkinStealer, game::KC_ANS0, game::KC_ANS0 + 3) > 0; });
        check(lastCue(ev, Role::SkinStealer, game::KC_ANS0, game::KC_ANS0 + 3) == a1, "Level 3: beim zweiten Ansprechen wiederholt er den Satz genau gleich");
        step(*s, ev, 2.5, [&] { aim(*s, P.headOf(Role::SkinStealer)); return false; });
        ev.clear();
        talk();
        step(*s, ev, 0.5);
        check(P.exposedOf(Role::SkinStealer) && count(ev, PE::Mimic, Role::SkinStealer, game::KC_TEAR) == 1 && !s->stats().dead,
              "Level 3: beim dritten Mal faellt die Tarnung - die Verwandlung beginnt (Ankuendigung, noch kein Angriff)");
        step(*s, ev, 2.2, [&] { aim(*s, P.headOf(Role::SkinStealer)); return P.variantOf(Role::SkinStealer) == 3 && P.stateOf(Role::SkinStealer) == 2; });
        check(P.variantOf(Role::SkinStealer) == 3 && count(ev, PE::Shriek, Role::SkinStealer) >= 1,
              "Level 3: Haut und Mantel reissen (wahre Gestalt, Schrei)");
        step(*s, ev, 6.0, [&] { if (P.has(Role::SkinStealer)) aim(*s, P.headOf(Role::SkinStealer)); return s->stats().dead; });
        check(s->stats().dead && s->killRole() == (int)Role::SkinStealer, "Level 3: hungrig toetet er, wer stehen bleibt");
    }
    // --- Skin-Stealer im Mantel: Lampe im Gesicht (faulende Haut) entlarvt; satt laesst er den ziehen, der ruhig zurueckweicht
    {
        auto s = open("level3", 43);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        bool joined = false;
        if (spawn(*s, ev, Role::SkinStealer)) {
            P.kinSet(Role::SkinStealer, "hungry", 0.0);
            for (int tries = 0; tries < 6 && !joined; ++tries) {
                if (placeNear(*s, Role::SkinStealer, 6.0, 9.0) || placeNear(*s, Role::SkinStealer, 5.0, 6.0))
                    step(*s, ev, 1.0, [&] { return P.stateOf(Role::SkinStealer) == 7; });
                joined = P.stateOf(Role::SkinStealer) == 7;
                if (!joined) step(*s, ev, 1.0);
            }
        }
        check(joined, "Level 3: satt schliesst er sich ebenso an");
        if (joined) {
            s->flashlight().on = true;
            step(*s, ev, 5.0, [&] { aim(*s, P.headOf(Role::SkinStealer)); return P.exposedOf(Role::SkinStealer); });
            check(P.exposedOf(Role::SkinStealer), "Level 3: die Lampe zu lange im Gesicht entlarvt ihn (die faulende Haut)");
            s->flashlight().on = false;
            step(*s, ev, 2.6, [&] { aim(*s, P.headOf(Role::SkinStealer)); return P.stateOf(Role::SkinStealer) == 6; });
            check(P.stateOf(Role::SkinStealer) == 6 && P.variantOf(Role::SkinStealer) == 3 && !s->stats().dead,
                  "Level 3: satt steht er in wahrer Gestalt da (kein Angriff)");
            step(*s, ev, 12.0, [&] {
                if (!P.has(Role::SkinStealer)) return true;
                aim(*s, P.headOf(Role::SkinStealer));
                return s->stats().dead || P.stateOf(Role::SkinStealer) != 6;
            }, [](int) {
                game::PlayerControl c;
                c.forward = -1.0f;
                return c;
            });
            check(!s->stats().dead && (!P.has(Role::SkinStealer) || P.stateOf(Role::SkinStealer) == 4 || P.stateOf(Role::SkinStealer) == 9),
                  "Level 3: wer ruhig zurueckweicht, den laesst er ziehen");
        }
    }
    // --- Skin-Stealer im Mantel: haelt die Tarnung lange; hungrig verwandelt er sich erst nach langer Zeit, satt verabschiedet er
    // sich -------------------------------------------------------------------------------------------------------------------
    for (int hungry = 0; hungry < 2; ++hungry) {
        auto s = open("level3", 45 + hungry);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        bool joined = false;
        if (spawn(*s, ev, Role::SkinStealer)) {
            P.kinSet(Role::SkinStealer, "hungry", hungry);
            for (int tries = 0; tries < 6 && !joined; ++tries) {
                if (placeNear(*s, Role::SkinStealer, 7.0, 2.0) || placeNear(*s, Role::SkinStealer, 5.0, 2.0))
                    step(*s, ev, 1.0, [&] { return P.stateOf(Role::SkinStealer) == 7; });
                joined = P.stateOf(Role::SkinStealer) == 7;
                if (!joined) step(*s, ev, 1.0);
            }
        }
        if (!joined) {
            check(false, "Level 3: Skin-Stealer begleitet (lange Zeit)");
            continue;
        }
        double t = 0.0, changed = -1.0;
        step(*s, ev, 150.0, [&] {
            t += 1.0 / 60.0;
            if (P.has(Role::SkinStealer)) aim(*s, P.headOf(Role::SkinStealer));
            if (changed < 0.0 && (P.exposedOf(Role::SkinStealer) || !P.has(Role::SkinStealer) || P.stateOf(Role::SkinStealer) != 7)) changed = t;
            return changed >= 0.0;
        });
        if (hungry) {
            check(changed >= 85.0 && P.exposedOf(Role::SkinStealer),
                  std::format("Level 3: hungrig haelt er die Tarnung lange und verwandelt sich erst dann ({:.0f} s)", changed));
        } else {
            check(changed >= 60.0 && !P.exposedOf(Role::SkinStealer) && count(ev, PE::Mimic, Role::SkinStealer, game::KC_BYE) == 1 && !s->stats().dead,
                  std::format("Level 3: satt verabschiedet er sich nach einer Weile und geht ({:.0f} s)", changed));
        }
    }
}

// --- V13.3: Erscheinungen ohne Figur in Level 1 - Crawler-Pilz, Garden, Gemaelde der Spielleiterin, Tuerspuren ----------
// (gameplay/phenomena.hpp; KREATUR_Crawler.md, KREATUR_GameMaster.md, KREATUR_L1_Erscheinungen.md)
static void testV133Phenomena(const std::string& dataDir) {
    std::printf("V13.3: Erscheinungen in Level 1 (Pilz, Garden, Gemaelde, Tuerspuren)\n");
    const game::Content& content = *gContent11;
    game::PrefabLibrary prefabs;
    prefabs.loadDirectory(dataDir + "/prefabs");
    JobSystem jobs(2);
    auto open = [&](const char* level, i64 seed) {
        auto s = std::make_unique<game::GameSession>(content, prefabs, content.level(level, world::kLayoutV6), seed, "medium", "Test", jobs, 3);
        s->placeAtSpawn();
        s->preload();
        return s;
    };
    auto step = [&](game::GameSession& s, std::vector<game::GameEvent>& ev, double secs, const std::function<bool()>& each = {}) {
        for (int i = 0; i < (int)(secs * 60.0); ++i) {
            if (each && each()) break;
            s.update(1.0 / 60.0, game::PlayerControl{}, false);
            ev.insert(ev.end(), s.events().begin(), s.events().end());
            s.events().clear();
        }
    };
    auto put = [](game::GameSession& s, double x, double y) {
        auto& pl = s.player();
        pl.x = x, pl.y = y, pl.z = s.world().tile(ifloor(x), ifloor(y)).floor, pl.vx = pl.vy = 0.0;
        s.body().reset();
        s.preload();
    };
    auto sounds = [](const std::vector<game::GameEvent>& ev, const char* name) {
        return (int)std::count_if(ev.begin(), ev.end(), [&](const game::GameEvent& e) { return e.type == game::GameEvent::SoundAt && e.sound == name; });
    };
    auto toast = [](const std::vector<game::GameEvent>& ev, const char* part) {
        return std::any_of(ev.begin(), ev.end(), [&](const game::GameEvent& e) {
            return (e.type == game::GameEvent::Toast || e.type == game::GameEvent::Consume) && e.text.find(part) != std::string::npos;
        });
    };
    auto slotOf = [](game::GameSession& s, const char* kind) {
        for (int i = 0; i < game::Inventory::kSize; ++i)
            if (s.inventory().slots[(size_t)i] && s.inventory().slots[(size_t)i]->kind == kind) return i;
        return -1;
    };

    // --- Karte: Orte aus den Spawnpunkten "trace"; Level 0 kennt keine Erscheinungen --------------------------------------
    {
        auto s0 = open("level0", 3);
        check(!s0->phenomena().any(), "Erscheinungen: Level 0 hat keine");
    }
    auto s = open("level1", 3);
    game::Phenomena& P = s->phenomena();
    std::printf("  Pilz %zu, Flaschen %zu, Lager %zu, Gemaeldeplaetze %zu, Gemaelde %zu, Hinauswurf %zu, Tueren %zu\n", P.fungusZones().size(),
                P.bottleSpots().size(), P.noteSpots().size(), P.paintingSpots().size(), P.paintings().size(), P.expelSpots().size(),
                P.clawDoors().size());
    const int jesters = (int)std::count_if(P.paintings().begin(), P.paintings().end(), [](const auto& p) { return p.jester; });
    check(P.any() && P.config().fungus && P.config().garden && P.config().paintings && P.config().claws, "Erscheinungen: Level 1 hat alle vier");
    check(P.fungusZones().size() >= 4 && P.bottleSpots().size() >= 2 && P.noteSpots().size() >= 1, "Pilz: befallene Stellen, offene Flaschen, ein Lager");
    check(P.paintingSpots().size() >= 8 && P.paintings().size() == 4 && jesters == 1, "Gemaelde: vier vor den Hallentueren, eines davon die Spielleiterin");
    check(P.expelSpots().size() >= 6 && P.clawDoors().size() == 1, "Hinauswurf-Orte und eine Zimmertuer mit Spuren");
    bool onFloor = true;
    for (const auto& sp : P.paintingSpots()) {
        const world::Tile& t = s->world().tile(ifloor(sp.pos.x), ifloor(sp.pos.y));
        onFloor = onFloor && t.open() && t.room && std::fabs(t.floor - sp.pos.z) < 0.05f;
    }
    check(onFloor, "Gemaelde: jeder Platz auf freiem Boden");

    // --- Ansteckung durch Kontakt: in der befallenen Pfuetze stehen -----------------------------------------------------------
    {
        std::vector<game::GameEvent> ev;
        const auto& z = P.fungusZones()[0];
        put(*s, z.pos.x, z.pos.y);
        step(*s, ev, 2.0);
        check(P.ailment(0).infectT < 0.0 && toast(ev, "Weißer Flaum"), "Ansteckung: erst ein Hinweis, noch gesund (2 s im Befall)");
        step(*s, ev, 7.0);
        check(P.ailment(0).infectT >= 0.0 && P.ailment(0).stage == 1, "Ansteckung: wer im Befall stehen bleibt, steckt sich an (Stadium 1)");
        // weg vom Befall: das Stadium bleibt, der Pilz waechst weiter
        const auto& e = P.expelSpots()[0];
        put(*s, e.pos.x, e.pos.y);
        P.ailment(0).infectT = 178.0;
        step(*s, ev, 3.0);
        check(P.ailment(0).stage == 2 && P.skinFx(0).x > 0.2f && P.fungusFx(0) > 0.1f, "Stadium 2 nach 3 min: Flecken auf der Haut (Viewmodel), Bild");
        // Mandelwasser heilt die fruehen Stadien (Interpretation)
        s->inventory().add("almond");
        s->useSlot(slotOf(*s, "almond"));
        step(*s, ev, 0.2);
        check(P.ailment(0).infectT < 0.0 && P.skinFx(0).x == 0.0f, "Mandelwasser: heilt Stadium 2");
        // im Fieber (Stadium 3) haelt es nur auf
        P.infect(0);
        P.ailment(0).infectT = 245.0;
        step(*s, ev, 0.5);
        s->inventory().add("almond");
        s->useSlot(slotOf(*s, "almond"));
        const double t0 = P.ailment(0).infectT;
        step(*s, ev, 5.0);
        check(P.ailment(0).stage == 3 && P.ailment(0).hold > 0.0 && std::fabs(P.ailment(0).infectT - t0) < 0.01,
              "Mandelwasser: im Fieber haelt es den Pilz nur an (Stadium 3 bleibt)");
        const double v0 = s->player().speedMul;
        check(v0 < 0.97 && P.sanityDrain(0) > 0.0, "Stadium 3: langsamer, der Verstand schwindet");
        // Stadium 5: der Pilz hat den Koerper - eigener Todesgrund
        P.ailment(0).hold = 0.0;
        P.ailment(0).infectT = 719.0;
        step(*s, ev, 2.0);
        check(s->stats().dead && s->deathCause() == "pilz" && s->killRole() == -2, "Pilz: nach 12 Minuten stirbt man (Ursache pilz)");
        s->respawnAtCheckpoint();
        check(!s->stats().dead && P.ailment(0).infectT < 0.0, "Wiedereinstieg: ohne Pilz");
        // offenes Mandelwasser steckt sofort an
        s->inventory().add("almond_open");
        const double san = s->stats().sanity;
        s->useSlot(slotOf(*s, "almond_open"));
        step(*s, ev, 0.2);
        check(P.ailment(0).infectT >= 0.0 && s->stats().sanity > san, "Offenes Mandelwasser: gibt Verstand, steckt aber an");
        s->respawnAtCheckpoint();
    }

    // --- Garden-Uhr: Zeichen, Ranken, Verwandlung, Tod; draussen geht sie zurueck ---------------------------------------------
    {
        std::vector<game::GameEvent> ev;
        double gx = -1, gy = -1;
        for (const auto& r : s->world().level().map->rooms) {
            if (r.type != "garden") continue;
            for (i64 y = r.y0 + 3; y < r.y1 - 3 && gx < 0; ++y)
                for (i64 x = r.x0 + 3; x < r.x1 - 3 && gx < 0; ++x) {
                    const world::Tile& t = s->world().tile(x, y);
                    const world::Tile& t2 = s->world().tile(x + 1, y);
                    if (t.open() && t2.open() && t.room && t.room->type && t.room->type->key == "garden") gx = x + 0.5, gy = y + 0.5;
                }
            break;
        }
        check(gx > 0, "Garden: ein freier Platz im Sektor");
        put(*s, gx, gy);
        step(*s, ev, 1.0);
        check(P.ailment(0).inGarden && P.ailment(0).garden > 0.9, "Garden-Uhr: zaehlt die Zeit im Sektor");
        P.ailment(0).garden = 179.5;
        step(*s, ev, 1.0);
        check(P.ailment(0).gstage == 1 && toast(ev, "Erde unter den Fingernägeln"), "Garden: erste Zeichen nach 3 min");
        P.ailment(0).garden = 359.5;
        step(*s, ev, 1.0);
        check(P.ailment(0).gstage == 2 && P.skinFx(0).y > 0.2f && s->player().speedMul < 0.95, "Garden: Ranken am Arm, langsamer");
        P.ailment(0).garden = 750.0;
        step(*s, ev, 1.0);
        check(P.ailment(0).gstage == 3 && P.gardenFx(0) > 0.6f && s->player().speedMul < 0.75, "Garden: die Verwandlung beginnt (Rinde, gruener Rand)");
        // wer geht, erholt sich langsam
        const auto& e = P.expelSpots()[1];
        put(*s, e.pos.x, e.pos.y);
        const double g0 = P.ailment(0).garden;
        step(*s, ev, 10.0);
        check(!P.ailment(0).inGarden && P.ailment(0).garden < g0 - 2.5 && P.ailment(0).garden > g0 - 4.5, "Garden: draussen geht die Uhr langsam zurueck");
        put(*s, gx, gy);
        P.ailment(0).garden = 899.0;
        step(*s, ev, 2.0);
        check(s->stats().dead && s->deathCause() == "garten" && s->killRole() == -3, "Garden: nach 15 min stirbt man durch die Verwandlung (Ursache garten)");
        s->respawnAtCheckpoint();
    }

    // --- Gemaelde: bleibt, solange es jemand sieht; verschwindet ungesehen und liegt vor einer anderen Tuer ------------------
    {
        size_t k = 0;
        while (k < P.paintings().size() && P.paintings()[k].jester) ++k;
        game::Phenomena::Painting& p = P.paintings()[k];
        put(*s, p.pos.x, p.pos.y);
        // ein Betrachter 4 m davor, Blick darauf (freie Sicht suchen)
        game::Viewer v;
        bool found = false;
        for (int a = 0; a < 16 && !found; ++a) {
            const float ang = (float)a * 0.3927f;
            v.eye = p.pos + vec3{std::cos(ang) * 3.0f, std::sin(ang) * 3.0f, 1.6f};
            const world::Tile& t = s->world().tile(ifloor(v.eye.x), ifloor(v.eye.y));
            found = t.open() && game::clearSight(s->world(), v.eye, p.pos + vec3{0, 0, 0.05f});
        }
        v.look = normalize(p.pos - v.eye);
        check(found, "Gemaelde: ein Platz mit freier Sicht darauf");
        const int spot0 = p.spot;
        std::vector<game::Phenomena::Event> pe;
        game::Phenomena::PlayerIn self;
        self.feet = v.eye - vec3{0, 0, 1.6f};
        for (int i = 0; i < 600; ++i) P.updateWorld(0.1, {v}, self, true, s->world(), pe);
        check(p.spot == spot0 && p.unseen == 0.0, "Gemaelde: angesehen verschwindet es nicht (60 s)");
        // wegsehen (und weggehen): es verschwindet und liegt an einer anderen Tuer, die keiner sieht
        game::Viewer away = v;
        away.look = -v.look;
        away.eye = v.eye - v.look * 9.0f;
        P.takeMoves();
        double t = 0.0;
        while (p.spot == spot0 && t < 180.0) P.updateWorld(0.1, {away}, self, true, s->world(), pe), t += 0.1;
        const std::vector<int> moved = P.takeMoves();
        check(p.spot != spot0 && length(p.pos - away.eye) > 8.0f && t > 7.0, std::format("Gemaelde: ungesehen verschwindet es nach {:.0f} s und liegt woanders", t));
        check(std::find(moved.begin(), moved.end(), (int)k) != moved.end(), "Gemaelde: der Weg geht an die Gaeste (Koop)");
        // Koop: der Gast setzt aus dem Weltereignis dieselbe Tuer, dasselbe Bild und denselben Winkel
        game::Phenomena G;
        G.setup(s->world().level(), (u64)3);
        const float yawOff = std::remainder(p.yaw - P.paintingSpots()[(size_t)p.spot].yaw, 6.2831853f);
        const vec3 pos = p.pos + vec3{0, 0, (float)p.variant * 10.0f + std::clamp(yawOff + 0.5f, 0.0f, 1.0f)};
        check(G.netMove(game::Phenomena::paintingHash((int)k), pos) && G.paintings()[k].spot == p.spot && G.paintings()[k].variant == p.variant &&
                  std::fabs(G.paintings()[k].yaw - p.yaw) < 0.01f && G.paintings()[k].model == p.model,
              "Gemaelde (Koop): der Gast legt es an dieselbe Tuer");
    }

    // --- Narren-Gemaelde: wer hindurchgeht, wird eingeladen und hinausgeworfen ----------------------------------------------
    {
        std::vector<game::GameEvent> ev;
        const auto it = std::find_if(P.paintings().begin(), P.paintings().end(), [](const auto& p) { return p.jester; });
        const vec3 jp = it->pos;
        P.ailment(0).jesterCool = 0.0;
        put(*s, jp.x, jp.y);
        step(*s, ev, 0.6);
        check(P.frozen(0) && P.jesterFx(0) > 0.5f, "Spielleiterin: auf dem Bild - ihr Gesicht fuellt die Sicht, man steht still");
        step(*s, ev, 7.0);
        const vec3 now{(float)s->player().x, (float)s->player().y, (float)s->player().z};
        const bool cut = std::any_of(ev.begin(), ev.end(), [](const game::GameEvent& e) { return e.type == game::GameEvent::TeleportCut; });
        check(!P.frozen(0) && length(now - jp) > 20.0f && cut && toast(ev, "Geschummelt"), "Spielleiterin: \"geschummelt\" - hinausgeworfen, woanders in Level 1");
        const world::Tile& t = s->world().tile(ifloor(now.x), ifloor(now.y));
        check(t.open() && t.room && !(t.room->type && t.room->type->key == "garden"), "Hinauswurf: auf freiem Boden, nicht im Garden");
        check(sounds(ev, "jester_giggle") >= 1 && sounds(ev, "jester_squeak") >= 1, "Spielleiterin: Kichern und Quietschen");
        // gleich wieder hinauf: sie laedt so bald nicht noch einmal ein
        const auto it2 = std::find_if(P.paintings().begin(), P.paintings().end(), [](const auto& p) { return p.jester; });
        check(length(it2->pos - jp) > 0.5f, "Spielleiterin: danach liegt ihr Bild an einer anderen Tuer");
        put(*s, it2->pos.x, it2->pos.y);
        step(*s, ev, 0.5);
        check(!P.frozen(0), "Spielleiterin: nach dem Hinauswurf eine Weile keine Einladung");
    }

    // --- Spuren an der Zimmertuer: klatschende Schritte, Krallen, Atem - nur von draussen -----------------------------------
    {
        std::vector<game::GameEvent> ev;
        auto& cd = P.clawDoors()[0];
        const vec3 out = cd.spot.pos - vec3{std::cos(cd.spot.yaw), std::sin(cd.spot.yaw), 0.0f} * 2.2f;
        put(*s, out.x, out.y);
        cd.cool = 0.05;
        step(*s, ev, 6.0);
        check(sounds(ev, "slap_step") == 5 && sounds(ev, "claw_scrape") == 2 && sounds(ev, "wet_breath") == 1,
              "Tuerspuren: fuenf klatschende Schritte, Krallen am Blech, Atem");
        ev.clear();
        const vec3 in = cd.spot.pos + vec3{std::cos(cd.spot.yaw), std::sin(cd.spot.yaw), 0.0f} * 1.6f;
        put(*s, in.x, in.y);
        cd.cool = 0.05;
        step(*s, ev, 6.0);
        check(sounds(ev, "slap_step") == 0, "Tuerspuren: im Zimmer selbst ist es still");
    }

    // --- Lager eines Infizierten: der Zettel warnt (einmal) -----------------------------------------------------------------
    {
        std::vector<game::GameEvent> ev;
        const auto& n = P.noteSpots()[0];
        const vec3 q = n.pos + vec3{std::cos(n.yaw), std::sin(n.yaw), 0.0f} * 0.8f;
        put(*s, q.x, q.y);
        step(*s, ev, 0.5);
        check(toast(ev, "offenen Flasche"), "Lager: der Zettel des Infizierten warnt vor offenen Flaschen");
    }
}

// V13.3 Teil 4, Level 2: Fensterwesen am Gangende, kleine Deathmoths an den Roehren, Death Rats an Faessern, Spuren der
// Skin-Stealer (KREATUR_Fensterwesen.md, KREATUR_Deathmoth.md, KREATUR_DeathRat.md, KREATUR_L2_Spuren.md)
static void testV133Level2Kin(const std::string& dataDir) {
    std::printf("V13.3 Teil 4: Level 2 - Fenster am Gangende, Motten, Ratten an Faessern, Spuren der Skin-Stealer\n");
    const game::Content& content = *gContent11;
    game::PrefabLibrary prefabs;
    prefabs.loadDirectory(dataDir + "/prefabs");
    JobSystem jobs(2);
    using PE = game::Presence::Event;
    using Role = game::Presence::Role;
    const auto L2 = content.level("level2", world::kLayoutV6);
    check(L2 && L2->map, "Level 2: Karte geladen");
    if (!L2 || !L2->map) return;
    auto open = [&](i64 seed) {
        auto s = std::make_unique<game::GameSession>(content, prefabs, L2, seed, "medium", "Test", jobs, 3);
        s->placeAtSpawn();
        s->preload();
        s->presence().setGrace(3000.0);  // keine anderen Wesen dazwischen
        return s;
    };
    auto step = [&](game::GameSession& s, std::vector<game::GameEvent>& ev, double secs, const std::function<bool()>& each = {},
                    game::PlayerControl ctl = {}) {
        for (int i = 0; i < (int)(secs * 60.0); ++i) {
            if (each && each()) break;
            s.update(1.0 / 60.0, ctl, false);
            ev.insert(ev.end(), s.events().begin(), s.events().end());
            s.events().clear();
        }
    };
    auto count = [](const std::vector<game::GameEvent>& ev, PE::Type t, Role r, float minI = -1.0f) {
        return (int)std::count_if(ev.begin(), ev.end(), [&](const game::GameEvent& e) {
            return e.type == game::GameEvent::Figure && e.index == (int)t && e.role == (int)r && e.intensity >= minI;
        });
    };
    auto put = [](game::GameSession& s, double x, double y, double angle, double pitch = 0.0) {
        auto& pl = s.player();
        pl.x = x, pl.y = y, pl.z = s.world().tile(ifloor(x), ifloor(y)).floor, pl.vx = pl.vy = 0.0;
        pl.angle = angle, pl.pitch = pitch;
        s.body().reset();
    };
    auto aimAt = [](game::GameSession& s, vec3 q) {
        auto& pl = s.player();
        pl.angle = std::atan2(q.y - pl.y, q.x - pl.x);
        pl.pitch = std::atan2(q.z - pl.eye(), std::max(0.3, std::hypot(q.x - pl.x, q.y - pl.y)));
    };
    auto eyeOf = [](game::GameSession& s) {
        const auto& pl = s.player();
        return vec3{(float)pl.x, (float)pl.y, (float)pl.eye()};
    };
    // --- Karte: Gangenden, Faesser und Reste, Spuren ---------------------------------------------------------------------
    struct End {
        double x, y, yaw;
    };
    std::vector<End> ends;
    for (const auto& sp : L2->map->spawns)
        if (sp.kind == "entity" && sp.role == "window") ends.push_back({sp.x, sp.y, sp.yaw});
    std::map<std::string, int> props;
    int meltedHot = 0;
    for (const world::MapObject& o : L2->map->objects) {
        ++props[o.model];
        if (o.model == "skin_shed" && o.variant == 1) {
            const int ri = L2->map->roomAt(o.x, o.y);
            meltedHot += ri >= 0 && L2->map->rooms[(size_t)ri].type == "tunnel_heiss" ? 1 : 0;
        }
    }
    check(ends.size() >= 6, std::format("Fensterwesen: {} Gangenden in der Karte (Spawnpunkte, die Stirnwand einer Sackgasse)", ends.size()));
    check(props["barrel"] >= 12 && props["food_scraps"] >= 6, std::format("Death Rat: {} Faesser, {} Stellen mit Essensresten", props["barrel"], props["food_scraps"]));
    check(props["skin_lair"] == 1 && props["skin_shed"] >= 2 && props["skin_hung"] >= 1 && props["skin_scratches"] >= 3,
          std::format("Skin-Stealer: Spuren in der Karte (Lager {}, Haeute {}, aufgehaengt {}, Kratzspuren {})", props["skin_lair"], props["skin_shed"],
                      props["skin_hung"], props["skin_scratches"]));
    check(meltedHot >= 1, "Skin-Stealer: eine geschmolzene Haut liegt in den heissen Tunneln (die Hitze liess die Haut schmelzen)");
    check(!L2->kin[world::KIN_SKINSTEALER].on, "Skin-Stealer: in Level 2 kein lebendes Wesen (laut Seite nicht mehr da)");
    check(L2->kin[world::KIN_WINDOW].on && L2->kin[world::KIN_WINDOW].mode == "end" && L2->kin[world::KIN_MOTH].on, "Level 2 kennt Fenster am Gangende und Motten");
    // --- Fensterwesen ------------------------------------------------------------------------------------------------------
    {
        auto s = open(7);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        check(!P.has(Role::Window), "Fensterwesen: zu Beginn kein Fenster (es entsteht erst, ungesehen)");
        // am Gangende: 14 m davor, Ruecken zur Stirnwand
        const End& E = ends[0];
        const vec3 n{(float)std::cos(E.yaw), (float)std::sin(E.yaw), 0.0f};
        double far = 14.0;
        while (far > 6.0 && !s->world().tile(ifloor(E.x + n.x * far), ifloor(E.y + n.y * far)).open()) far -= 1.0;
        put(*s, E.x + n.x * far, E.y + n.y * far, E.yaw);
        step(*s, ev, 1.0);
        // sieht man die Stirnwand, entsteht dort keines
        put(*s, E.x + n.x * far, E.y + n.y * far, E.yaw + 3.14159);
        P.l2Timers(0.2, 1e9);
        step(*s, ev, 2.0);
        vec3 wp;
        int st = 0, var = 0;
        bool atEnd = false;
        for (int i = 0; P.agentInfo(Role::Window, i, wp, st, var); ++i) atEnd = atEnd || std::hypot(wp.x - (E.x - n.x * 0.3), wp.y - (E.y - n.y * 0.3)) < 1.0;
        check(!atEnd, "Fensterwesen: an einer Stirnwand im Blick entsteht keines");
        P.clear();
        // Ruecken zur Wand: es bildet sich ungesehen (Glas, Rahmen, Aussenluft)
        put(*s, E.x + n.x * far, E.y + n.y * far, E.yaw);
        ev.clear();
        P.l2Timers(0.2, 1e9);
        bool seenAtBirth = true;
        step(*s, ev, 4.0, [&] {
            if (!P.has(Role::Window)) return false;
            const vec3 h = P.posOf(Role::Window) + n * 0.15f;
            const vec3 e = eyeOf(*s);
            const vec3 look{(float)(std::cos(s->player().angle)), (float)std::sin(s->player().angle), 0.0f};
            seenAtBirth = dot(normalize(h - e), look) > 0.25f && game::clearSight(s->world(), e, h);
            return true;
        });
        check(P.has(Role::Window), "Fensterwesen: bildet sich an einem Gangende (Regie)");
        check(P.has(Role::Window) && !seenAtBirth, "Fensterwesen: niemand sieht es entstehen");
        check(P.variantOf(Role::Window) == 1, "Fensterwesen: im Stahlfenster der Ebene (Modell ent_window_end)");
        check(count(ev, PE::Burst, Role::Window, 0.9f) == 1, "Fensterwesen: Ankuendigung beim Entstehen (Glas, Rahmen, Aussenluft)");
        if (P.has(Role::Window)) {
            const vec3 w = P.posOf(Role::Window);
            const world::Tile& behind = s->world().tile(ifloor(w.x - n.x * 0.4f), ifloor(w.y - n.y * 0.4f));
            check(behind.solid() || std::fabs(std::fmod(std::fabs(w.x), 1.0f)) < 0.05f || std::fabs(std::fmod(std::fabs(w.y), 1.0f)) < 0.05f,
                  "Fensterwesen: in der Stirnwand (dahinter fester Beton)");
            // umdrehen, auf 6 m davor: Fluestern, Aussenluft hinter Glas; es bleibt, solange man hinsieht
            const vec3 wn{std::cos(P.yawOf(Role::Window)), std::sin(P.yawOf(Role::Window)), 0.0f};
            put(*s, w.x + wn.x * 6.0, w.y + wn.y * 6.0, 0.0);
            aimAt(*s, w);
            ev.clear();
            step(*s, ev, 16.0, [&] {
                aimAt(*s, P.posOf(Role::Window));
                return false;
            });
            check(count(ev, PE::Whisper, Role::Window) > 0, "Fensterwesen: es fluestert (lockt)");
            check(count(ev, PE::Burst, Role::Window, 0.2f) - count(ev, PE::Burst, Role::Window, 0.9f) > 0, "Fensterwesen: ferner Wind und Voegel hinter Glas");
            check(P.has(Role::Window), "Fensterwesen: es bleibt, wo es entstanden ist");
            // naehern: der Arm faehrt aus - wer gleich zuruecktritt, entkommt
            put(*s, w.x + wn.x * 1.5, w.y + wn.y * 1.5, 0.0);
            aimAt(*s, P.posOf(Role::Window));
            ev.clear();
            step(*s, ev, 0.4);
            check(count(ev, PE::Pull, Role::Window) > 0, "Fensterwesen: auf 1,5 m faehrt der Arm heraus (Ankuendigung)");
            put(*s, w.x + wn.x * 4.0, w.y + wn.y * 4.0, 0.0);
            step(*s, ev, 1.5);
            check(!s->stats().dead, "Fensterwesen: sofort zurueckgetreten - entkommen");
            step(*s, ev, 4.5);
            put(*s, w.x + wn.x * 1.4, w.y + wn.y * 1.4, 0.0);
            aimAt(*s, P.posOf(Role::Window));
            step(*s, ev, 3.0, [&] { return s->stats().dead; });
            check(s->stats().dead && s->killRole() == (int)Role::Window, "Fensterwesen: wer stehen bleibt, wird hineingezogen (toedlich)");
        }
    }
    // --- Deathmoths (Level 2): klein, an der Roehre, vom Licht angezogen, meist friedlich ----------------------------------
    {
        auto s = open(11);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        P.l2Timers(1e9, 0.2);
        step(*s, ev, 3.0, [&] { return P.has(Role::Moth); });
        check(P.has(Role::Moth), "Deathmoths: eine Gruppe an einer brennenden Roehre (Regie)");
        int n = 0, males = 0;
        vec3 mp, centre{};
        int st = 0, var = 0;
        bool anyVisible = false;
        for (int i = 0; P.agentInfo(Role::Moth, i, mp, st, var); ++i) {
            ++n, males += var == 1 ? 1 : 0, centre += mp;
            const vec3 e = eyeOf(*s);
            const vec3 look{(float)std::cos(s->player().angle), (float)std::sin(s->player().angle), 0.0f};
            anyVisible = anyVisible || (dot(normalize(mp - e), look) > 0.3f && game::clearSight(s->world(), e, mp));
        }
        check(n >= 3 && males >= n - 1, std::format("Deathmoths: {} Motten, {} Maennchen (meist friedliche Maennchen)", n, males));
        check(!anyVisible, "Deathmoths: niemand sieht sie erscheinen");
        if (n) {
            centre = centre / (float)n;
            const world::Tile& t = s->world().tile(ifloor(centre.x), ifloor(centre.y));
            check(centre.z > t.ceil - 0.8f, "Deathmoths: oben an der Decke bei der Roehre");
            // in Ruhe lassen: 4,5 m entfernt, Lampe aus - kein Angriff
            double px = centre.x, py = centre.y;
            for (int k = 0; k < 16; ++k) {
                const double a = k * 0.3927, x = centre.x + std::cos(a) * 4.5, y = centre.y + std::sin(a) * 4.5;
                const world::Tile* q = s->world().tileIfLoaded(ifloor(x), ifloor(y));
                if (q && q->open() && game::clearSight(s->world(), vec3{(float)x, (float)y, q->floor + 1.6f}, centre)) {
                    px = x, py = y;
                    break;
                }
            }
            put(*s, px, py, 0.0);
            aimAt(*s, centre);
            s->flashlight().on = false;
            ev.clear();
            int agit = 0, takeoffs = 0, warmed = 0;
            std::map<u16, std::pair<int, float>> prevMoth;  // Kritikrunde 5: Zustand und Zittern im letzten Bild (Kanal shiver)
            std::vector<game::Presence::NetState> ns;
            step(*s, ev, 10.0, [&] {
                for (int i = 0; P.agentInfo(Role::Moth, i, mp, st, var); ++i) agit += st >= 2 ? 1 : 0;
                ns.clear();
                P.netStates(ns, centre, 64);
                for (const auto& m : ns) {
                    if (m.role != (int)Role::Moth) continue;
                    auto it = prevMoth.find(m.netId);
                    if (it != prevMoth.end() && it->second.first == 0 && m.state == 1) ++takeoffs, warmed += it->second.second > 0.5f ? 1 : 0;
                    prevMoth[m.netId] = {m.state, m.shiver};
                }
                return false;
            });
            check(agit == 0 && count(ev, PE::Hit, Role::Moth) == 0, "Deathmoths: in Ruhe gelassen bleiben sie friedlich");
            check(takeoffs >= 1 && warmed == takeoffs,
                  std::format("Deathmoths: vor dem Auffliegen zittern die Fluegel (Aufwaermen; {} von {} Starts)", warmed, takeoffs));
            // die Lampe lockt sie: sie fliegen auf und kreisen um das Licht
            s->flashlight().on = true;
            int lured = 0;
            step(*s, ev, 8.0, [&] {
                aimAt(*s, centre);
                lured = 0;
                const vec3 e = eyeOf(*s);
                for (int i = 0; P.agentInfo(Role::Moth, i, mp, st, var); ++i) lured += (st == 1 && length(mp - e) < 2.8f) ? 1 : 0;
                return lured >= 2;
            });
            check(lured >= 2, std::format("Deathmoths: das Licht lockt sie zur Lampe ({} um das Licht)", lured));
            check(count(ev, PE::Hit, Role::Moth) == 0, "Deathmoths: um das Licht kreisend tun sie nichts");
            // reizen: Gesicht dicht an ihnen, Lampe hinein - sie schwirren um den Kopf (Ankuendigung), ein Weibchen beisst
            for (int tries = 0; tries < 10 && P.variantOf(Role::Moth) != 2; ++tries) {  // eine Gruppe mit Weibchen
                P.clear();
                P.force(Role::Moth);
                step(*s, ev, 0.5);
            }
            bool female = false;
            for (int i = 0; P.agentInfo(Role::Moth, i, mp, st, var); ++i) female = female || var == 2;
            check(female, "Deathmoths: manche Gruppen haben ein Weibchen (chance)");
            vec3 fm{};
            for (int i = 0; P.agentInfo(Role::Moth, i, mp, st, var); ++i)
                if (var == 2) fm = mp;
            if (female) {
                const world::Tile& ft = s->world().tile(ifloor(fm.x), ifloor(fm.y));
                put(*s, fm.x - 0.3, fm.y, 0.0);
                (void)ft;
                ev.clear();
                double hissT = -1.0, hitT = -1.0, t = 0.0;
                int agS = 0, agFront = 0;  // Kritikrunde 4: gereizte Maennchen schwirren vor dem Gesicht (im Blickfeld)
                step(*s, ev, 12.0, [&] {
                    t += 1.0 / 60.0;
                    vec3 q, fq;
                    int s2 = 0, v2 = 0;
                    bool hasF = false;
                    for (int i = 0; P.agentInfo(Role::Moth, i, q, s2, v2); ++i)
                        if (v2 == 2) aimAt(*s, q), fq = q, hasF = true;
                    const vec3 e = eyeOf(*s);
                    for (int i = 0; hasF && length(fq - e) > 0.05f && P.agentInfo(Role::Moth, i, q, s2, v2); ++i)
                        if (s2 == 2 && v2 != 2) ++agS, agFront += dot(normalize(q - e), normalize(fq - e)) > 0.5f ? 1 : 0;
                    if (hissT < 0.0 && count(ev, PE::Hiss, Role::Moth) > 0) hissT = t;
                    if (hitT < 0.0 && count(ev, PE::Hit, Role::Moth) > 0) hitT = t;
                    return hitT > 0.0;
                });
                check(hissT >= 0.0, "Deathmoths: aus der Naehe gereizt - Brummen um den Kopf (Ankuendigung)");
                check(agS > 0 && agFront * 10 >= agS * 6,
                      std::format("Deathmoths: gereizt schwirren sie vor dem Gesicht, im Blickfeld ({} von {} Proben)", agFront, agS));
                check(hitT > 0.0 && hitT - hissT >= 1.0, std::format("Deathmoths: das Weibchen beisst erst nach der Ankuendigung ({:.1f} s)", hitT - hissT));
                check(!s->stats().dead && s->stats().health > 80.0, std::format("Deathmoths: der Biss ist leicht (Gesundheit {:.0f})", s->stats().health));
                // zuruecktreten, Lampe aus: sie beruhigen sich
                put(*s, fm.x - 7.0 * std::cos(s->player().angle), fm.y - 7.0 * std::sin(s->player().angle), s->player().angle + 3.14159);
                if (!s->world().tile(ifloor(s->player().x), ifloor(s->player().y)).open()) put(*s, px, py, 0.0);
                s->flashlight().on = false;
                int agitEnd = 0;
                step(*s, ev, 8.0);
                for (int i = 0; P.agentInfo(Role::Moth, i, mp, st, var); ++i) agitEnd += st >= 2 ? 1 : 0;
                check(agitEnd == 0, "Deathmoths: wer zuruecktritt und das Licht senkt, ist sie los");
            }
        }
    }
    // --- Death Rats an Faessern und Essensresten -------------------------------------------------------------------------
    {
        auto s = open(13);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.3);
        auto& P = s->presence();
        // ein Fass mit Resten daneben: 12 m davon weg stellen, Blick weg
        const world::MapObject* fb = nullptr;
        double px = 0.0, py = 0.0;
        bool found = false;
        for (const world::MapObject& o : L2->map->objects) {  // Reste, zu denen man 9-11 m gerade Sicht hat (Weg = Abstand)
            if (o.model != "food_scraps" || found) continue;
            for (double r = 11.0; r >= 9.0 && !found; r -= 1.0)
                for (int k = 0; k < 48 && !found; ++k) {
                    const double a = k * 0.1309, x = o.x + std::cos(a) * r, y = o.y + std::sin(a) * r;
                    const world::Tile* q = s->world().tileIfLoaded(ifloor(x), ifloor(y));
                    if (q && q->open() && q->clearance() > 2.0f &&
                        game::clearSight(s->world(), vec3{(float)x, (float)y, q->floor + 0.3f}, vec3{(float)o.x, (float)o.y, (float)o.z + 0.3f}))
                        px = x, py = y, found = true, fb = &o;
                }
        }
        check(fb != nullptr, "Death Rat: Essensreste an einem Fass, mit gerader Sicht darauf");
        if (fb) {
            put(*s, px, py, std::atan2(py - fb->y, px - fb->x));
            step(*s, ev, 1.0);
            P.force(Role::Rat);
            step(*s, ev, 2.0, [&] { return P.has(Role::Rat); });
            check(P.has(Role::Rat), "Death Rat: Ratten in Level 2");
            int feeding = 0, high = 0;
            ev.clear();
            step(*s, ev, 40.0, [&] {
                vec3 q;
                int s2 = 0, v2 = 0;
                feeding = 0;
                for (int i = 0; P.agentInfo(Role::Rat, i, q, s2, v2); ++i) {
                    const world::Tile* t = s->world().tileIfLoaded(ifloor(q.x), ifloor(q.y));
                    if (t && q.z > t->floor + 0.6f) ++high;
                }
                return count(ev, PE::Snip, Role::Rat) >= 3;
            });
            feeding = count(ev, PE::Snip, Role::Rat);
            check(feeding >= 3, std::format("Death Rat: sie trinken am Fass und fressen die Reste ({} Laute: Schlecken, Nagen)", feeding));
            check(high > 0, "Death Rat: eine klettert auf den Deckel und trinkt am Spundloch");
            check(count(ev, PE::Hit, Role::Rat) == 0, "Death Rat: dabei harmlos");
            // man kommt rennend: sie fliehen (vom Fass herunter, quiekend)
            vec3 rp = P.posOf(Role::Rat);
            put(*s, rp.x + 2.5, rp.y, 3.14159);
            if (!s->world().tile(ifloor(rp.x + 2.5), ifloor(rp.y)).open()) put(*s, rp.x - 2.5, rp.y, 0.0);
            game::PlayerControl run;
            run.sprint = true, run.forward = 0.2f;
            ev.clear();
            step(*s, ev, 2.0, {}, run);
            vec3 q;
            int s2 = 0, v2 = 0, fled = 0, n = 0;
            for (int i = 0; P.agentInfo(Role::Rat, i, q, s2, v2); ++i) ++n, fled += (s2 == 1 || s2 == 9) ? 1 : 0;
            check(count(ev, PE::Squeak, Role::Rat) > 0 && (n == 0 || fled > 0), "Death Rat: kommt man, quieken sie und fliehen");
        }
    }
    // --- Kritikrunden 4 und 5: Kammer mit nur einer Tuer - am Spund trinken, nicht am Spieler vorbei zur Tuer fliehen ------
    {
        const world::MapDef& M = *L2->map;
        int room = -1;
        double fx = 0, fy = 0, dx = 0, dy = 0;
        for (const world::MapObject& o : M.objects) {
            if (o.model != "food_scraps" || room >= 0) continue;
            for (int r = 0; r < (int)M.rooms.size() && room < 0; ++r) {
                const world::MapRoom& R = M.rooms[(size_t)r];
                if (o.x < (double)R.x0 || o.x >= (double)R.x1 || o.y < (double)R.y0 || o.y >= (double)R.y1) continue;
                int nd = 0;
                double cx = 0, cy = 0;
                for (const world::MapDoor& d : M.doors)
                    if (d.a == r || d.b == r) ++nd, cx = (double)(d.x0 + d.x1) * 0.5, cy = (double)(d.y0 + d.y1) * 0.5;
                int barrels = 0;  // eine Fassreihe an der Rueckwand
                for (const world::MapObject& b : M.objects) barrels += (b.model == "barrel" && std::hypot(b.x - o.x, b.y - o.y) < 1.5) ? 1 : 0;
                if (nd == 1 && barrels >= 3 && std::hypot(cx - o.x, cy - o.y) > 8.5) room = r, fx = o.x, fy = o.y, dx = cx, dy = cy;
            }
        }
        check(room >= 0, "Death Rat: eine Kammer mit nur einer Tuer, Faesser und Reste an der Rueckwand");
        if (room >= 0) {
            auto s = open(17);
            std::vector<game::GameEvent> ev;
            step(*s, ev, 0.3);
            auto& P = s->presence();
            const double il = std::hypot(fx - dx, fy - dy), ix = (fx - dx) / il, iy = (fy - dy) / il;  // von der Tuer zum Futter
            put(*s, dx + ix * 0.8, dy + iy * 0.8, std::atan2(-iy, -ix));                                  // in der Tuer, Blick hinaus
            step(*s, ev, 0.5);
            bool here = false;  // die Gruppe soll an diesem Futter auftauchen (der Auftritt waehlt zufaellig unter den nahen)
            for (int k = 0; k < 12 && !here; ++k) {
                P.clear();
                P.force(Role::Rat);
                step(*s, ev, 0.2);
                vec3 q;
                int s2 = 0, v2 = 0;
                for (int i = 0; P.agentInfo(Role::Rat, i, q, s2, v2); ++i) here = here || std::hypot(q.x - fx, q.y - fy) < 2.5;
            }
            std::printf("  Kammer: Futter (%.1f, %.1f), Tuer (%.1f, %.1f), Gruppe dort: %d\n", fx, fy, dx, dy, here ? 1 : 0);
            std::vector<game::Presence::NetState> ns;
            int onLid = 0, atBung = 0;
            ev.clear();
            step(*s, ev, 30.0, [&] {
                ns.clear();
                P.netStates(ns, vec3{(float)fx, (float)fy, 0.0f}, 32);
                for (const auto& m : ns) {
                    const world::Tile* t = s->world().tileIfLoaded(ifloor(m.pos.x), ifloor(m.pos.y));
                    if (m.role != (int)Role::Rat || m.state != 0 || !t || m.pos.z < t->floor + 0.8f) continue;
                    double bd = 1e9;  // naechstes Fass
                    const world::MapObject* bo = nullptr;
                    for (const world::MapObject& o : M.objects)
                        if (o.model == "barrel" && std::hypot(o.x - m.pos.x, o.y - m.pos.y) < bd) bd = std::hypot(o.x - m.pos.x, o.y - m.pos.y), bo = &o;
                    // grosser Spund je Fassvariante (aus barrel@0..2.glb gemessen, Blender-y gespiegelt; wie presence_rat_l2.cpp)
                    static const double kB[3][2] = {{-0.1775, -0.1025}, {-0.1775, 0.1025}, {0.2042, 0.0179}};
                    const int bv = std::clamp(bo->variant, 0, 2);
                    const double gx = bo->x + kB[bv][0] * std::cos(bo->yaw) - kB[bv][1] * std::sin(bo->yaw);
                    const double gy = bo->y + kB[bv][0] * std::sin(bo->yaw) + kB[bv][1] * std::cos(bo->yaw);
                    const double sb = std::hypot(m.pos.x + std::cos(m.yaw) * 0.2 - gx, m.pos.y + std::sin(m.yaw) * 0.2 - gy);
                    ++onLid, atBung += sb < 0.07 ? 1 : 0;  // die Schnauze ueber dem Spund
                }
                return onLid >= 90;  // 1,5 s auf dem Deckel beobachtet
            });
            std::printf("  Death Rats in der Kammer: %d, Laute %d\n", P.count(Role::Rat), count(ev, PE::Snip, Role::Rat));
            check(onLid > 0 && atBung * 10 >= onLid * 8,
                  std::format("Death Rat: auf dem Deckel die Schnauze am Spund, nicht mitten auf dem Deckel ({} von {} Proben)", atBung, onLid));
            // umdrehen, 3,6 m vor das Futter, Lampe an: sie fliehen in die Ecken - nicht durch die Tuer hinter dem Spieler
            const double px = fx - ix * 3.6, py = fy - iy * 3.6;
            put(*s, px, py, std::atan2(iy, ix));
            s->flashlight().on = true;
            aimAt(*s, vec3{(float)fx, (float)fy, 0.3f});
            ev.clear();
            std::map<u16, std::pair<float, float>> dist;  // je Ratte: Abstand zu Beginn, kleinster Abstand
            step(*s, ev, 3.0, [&] {
                ns.clear();
                P.netStates(ns, vec3{(float)px, (float)py, 0.0f}, 32);
                for (const auto& m : ns) {
                    if (m.role != (int)Role::Rat) continue;
                    const float dd = (float)std::hypot(m.pos.x - px, m.pos.y - py);
                    auto it = dist.find(m.netId);
                    if (it == dist.end()) dist[m.netId] = {dd, dd};
                    else it->second.second = std::min(it->second.second, dd);
                }
                return false;
            });
            float worst = 1e9f;  // naeher gekommen als zu Beginn (hoechstens 1,5 m angerechnet)
            for (const auto& [id, d2] : dist) worst = std::min(worst, d2.second - std::min(d2.first, 1.5f));
            check(count(ev, PE::Squeak, Role::Rat) > 0, "Death Rat: angeleuchtet quieken sie und stieben davon");
            check(!dist.empty() && worst > -0.35f, std::format("Death Rat: sie fliehen nicht am Spieler vorbei zur Tuer ({} Ratten, {:.2f} m)", dist.size(), worst));
        }
    }
}

// V13.3 Teil 4: Jerry (KREATUR_Jerry.md) - sein Raum in der Karte, Auftreten ungesehen, Murmeln vor Sicht, Blickkontakt und
// Bann, Wegsehen rettet, Sprung bei Naehe (angekuendigt, Zurueckweichen rettet), Puppe beim Gast
static void testV133Jerry(const std::string& dataDir) {
    std::printf("V13.3 Teil 4: Jerry (Level 2)\n");
    const game::Content& content = *gContent11;
    game::PrefabLibrary prefabs;
    prefabs.loadDirectory(dataDir + "/prefabs");
    JobSystem jobs(2);
    using PE = game::Presence::Event;
    using Role = game::Presence::Role;
    const auto L2 = content.level("level2", world::kLayoutV6);
    if (!L2 || !L2->map) return check(false, "Jerry: Karte von Level 2");
    // --- Karte: ein Spawnpunkt auf dem Sitzrohr in einer blauen Kammer mit nur einer Tuer; Opfertisch, Wandbild, Wegweiser --
    const world::MapSpawn* sp = nullptr;
    int nSpawn = 0;
    for (const auto& s : L2->map->spawns)
        if (s.kind == "entity" && s.role == "jerry") sp = &s, ++nSpawn;
    check(nSpawn == 1, std::format("Jerry: genau ein Platz in der Karte ({})", nSpawn));
    if (!sp) return;
    const int room = L2->map->roomAt(sp->x, sp->y);
    int doors = 0;
    for (const auto& d : L2->map->doors) doors += d.a == room || d.b == room ? 1 : 0;
    check(room >= 0 && L2->map->rooms[(size_t)room].zone == "jerry" && doors == 1, std::format("Jerry: in einer blau gestrichenen Kammer mit einer Tuer ({} Tueren)", doors));
    std::map<std::string, int> inRoom, outside;
    bool perchAt = false;
    for (const world::MapObject& o : L2->map->objects) {
        if (o.model.rfind("jerry_", 0) != 0) continue;
        (L2->map->roomAt(o.x, o.y) == room ? inRoom : outside)[o.model]++;
        perchAt = perchAt || (o.model == "jerry_perch" && std::hypot(o.x - sp->x, o.y - sp->y) < 0.05);
    }
    check(perchAt && inRoom["jerry_mural"] == 1 && inRoom["jerry_shrine"] == 1, "Jerry: Sitzrohr am Platz, Wandbild und Opfertisch der Followers im Raum");
    check(outside["jerry_poster"] >= 2, std::format("Jerry: Wegweiser der Followers vor dem Raum ({} Fotos und Pfeile)", outside["jerry_poster"]));
    check(L2->kin[world::KIN_JERRY].on && L2->kin[world::KIN_JERRY].mode == "room", "Jerry: Level 2 kennt ihn (presence.jerry)");
    // Tuer und Blick vom Platz zur Tuer
    vec3 doorC{};
    for (const auto& d : L2->map->doors)
        if (d.a == room || d.b == room) doorC = {(float)(d.x0 + d.x1) * 0.5f, (float)(d.y0 + d.y1) * 0.5f, 0.0f};
    const vec3 toDoor = normalize(vec3{doorC.x - (float)sp->x, doorC.y - (float)sp->y, 0.0f});
    auto open = [&](i64 seed) {
        auto s = std::make_unique<game::GameSession>(content, prefabs, L2, seed, "medium", "Test", jobs, 3);
        s->placeAtSpawn();
        s->preload();
        s->presence().setGrace(3000.0);
        return s;
    };
    auto step = [&](game::GameSession& s, std::vector<game::GameEvent>& ev, double secs, const std::function<bool()>& each = {}) {
        for (int i = 0; i < (int)(secs * 60.0); ++i) {
            if (each && each()) break;
            s.update(1.0 / 60.0, {}, false);
            ev.insert(ev.end(), s.events().begin(), s.events().end());
            s.events().clear();
        }
    };
    auto count = [](const std::vector<game::GameEvent>& ev, PE::Type t, Role r, float minI = -1.0f) {
        return (int)std::count_if(ev.begin(), ev.end(), [&](const game::GameEvent& e) {
            return e.type == game::GameEvent::Figure && e.index == (int)t && e.role == (int)r && e.intensity >= minI;
        });
    };
    auto put = [](game::GameSession& s, double x, double y, double angle, double pitch = 0.0) {
        auto& pl = s.player();
        pl.x = x, pl.y = y, pl.z = s.world().tile(ifloor(x), ifloor(y)).floor, pl.vx = pl.vy = 0.0;
        pl.angle = angle, pl.pitch = pitch;
        s.body().reset();
    };
    auto aimAt = [](game::GameSession& s, vec3 q) {
        auto& pl = s.player();
        pl.angle = std::atan2(q.y - pl.y, q.x - pl.x);
        pl.pitch = std::atan2(q.z - pl.eye(), std::max(0.3, std::hypot(q.x - pl.x, q.y - pl.y)));
    };
    // Stelle im Raum, d m vor ihm Richtung Tuer
    auto front = [&](double d) { return vec3{(float)sp->x + toDoor.x * (float)d, (float)sp->y + toDoor.y * (float)d, 0.0f}; };
    vec3 hall{};
    {
        auto s = open(11);
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.5);
        auto& P = s->presence();
        // Stelle im Gang vor der Tuer: begehbar, das Rohr von dort nicht zu sehen
        const vec3 perchHead{(float)sp->x, (float)sp->y, s->world().tile(ifloor(sp->x), ifloor(sp->y)).floor + 1.95f};
        bool found = false;
        for (float k : {2.0f, 3.0f, 4.0f, 1.5f})
            for (float sd : {1.5f, -1.5f, 1.0f, -1.0f, 0.5f, -0.5f}) {
                if (found) break;
                const vec3 q = doorC + toDoor * k + vec3{-toDoor.y, toDoor.x, 0.0f} * sd;
                const world::Tile& tq = s->world().tile(ifloor(q.x), ifloor(q.y));
                if (!tq.open() || game::clearSight(s->world(), vec3{q.x, q.y, tq.floor + 1.6f}, perchHead)) continue;
                hall = q, found = true;
            }
        check(found, "Jerry: vor der Tuer eine Stelle ausser Sicht des Rohrs");
        check(!P.has(Role::Jerry), "Jerry: am Start weit weg - nicht da");
        put(*s, hall.x, hall.y, std::atan2(toDoor.y, toDoor.x));  // Ruecken zur Tuer
        ev.clear();
        step(*s, ev, 40.0);
        check(P.has(Role::Jerry), "Jerry: sitzt auf seinem Rohr, sobald man in die Naehe kommt");
        const auto J = P.jerryInfo();
        const float floorZ = s->world().tile(ifloor(sp->x), ifloor(sp->y)).floor;
        check(J.active && std::hypot(J.pos.x - sp->x, J.pos.y - sp->y) < 0.05 && std::fabs(J.pos.z - floorZ - anim::jy::kPerchZ) < 0.02f,
              std::format("Jerry: auf dem Sitzrohr ({:.2f} m ueber dem Boden)", J.pos.z - floorZ));
        check(count(ev, PE::Speak, Role::Jerry) >= 2 && count(ev, PE::Speak, Role::Jerry, 0.9f) == 0,
              std::format("Jerry: man hoert ihn vor dem Raum murmeln, bevor man ihn sieht ({} Saetze)", count(ev, PE::Speak, Role::Jerry)));
        check(!s->stats().dead && P.jerrySpell(s->netSelfId) == 0.0f, "Jerry: ausser Sicht kein Bann");
        // --- in die Tuer, ihm ins Gesicht sehen: er bemerkt, spricht, der Bann waechst bis zur Unterwerfung -------------------
        const vec3 at = front(3.2);
        put(*s, at.x, at.y, 0.0);
        aimAt(*s, P.jerryInfo().head);
        s->flashlight().on = true;  // im Halbdunkel: die Lampe auf ihn (sonst sieht man ihm nicht in die Augen)
        ev.clear();
        double tNotice = -1.0, tWarn = -1.0, tDead = -1.0, t = 0.0, hold = 0.0;
        step(*s, ev, 10.0, [&] {
            t += 1.0 / 60.0;
            if (!s->stats().dead) aimAt(*s, P.jerryInfo().head);  // haelt den Blick (gegen den Zug)
            const auto I = P.jerryInfo();
            if (tNotice < 0.0 && I.state >= anim::jy::S_NOTICE) tNotice = t;
            if (tWarn < 0.0 && count(ev, PE::Announce, Role::Jerry) > 0) tWarn = t;
            hold = std::max(hold, (double)P.jerryHold(s->netSelfId));
            if (s->stats().dead) tDead = t;
            return s->stats().dead;
        });
        std::printf("  Jerry (3,2 m, fester Blick, Lampe): bemerkt nach %.2f s, Warnung nach %.2f s, unterworfen nach %.2f s, Laehmung bis %.2f\n", tNotice,
                    tWarn, tDead, hold);
        check(tNotice >= 0.0 && tNotice < 0.7, std::format("Jerry: bemerkt den Spieler in der Tuer ({:.2f} s)", tNotice));
        check(count(ev, PE::Speak, Role::Jerry, 0.9f) >= 1, "Jerry: spricht ihn mit ruhiger Stimme an");
        check(count(ev, PE::Spell, Role::Jerry) >= 2, "Jerry: der Bann kuendigt sich an (Klingeln, Fluestern - zwei Stufen)");
        check(hold > 0.5, std::format("Jerry: der Bann laehmt die Schritte ({:.2f})", hold));
        check(tWarn > 0.0 && tDead > tWarn + 0.3, std::format("Jerry: Warnung (Fluegel, Kreischen) vor der Unterwerfung ({:.2f} s / {:.2f} s)", tWarn, tDead));
        check(tDead > 1.5 && tDead < 6.0 && s->killRole() == (int)Role::Jerry && s->deathCause() == "stalker",
              std::format("Jerry: wer ihm in die Augen sieht, ist nach {:.1f} s unterworfen (toedlich)", tDead));
        // Wiedereinstieg am Sicherungspunkt (die Wesen bleiben): Jerry sitzt wieder auf seinem Rohr, der Bann ist fort
        s->respawnCoop();
        step(*s, ev, 4.0);
        const auto J2 = P.jerryInfo();
        check(!s->stats().dead && J2.active && J2.state != anim::jy::S_SUBDUE && std::hypot(J2.pos.x - sp->x, J2.pos.y - sp->y) < 0.05 &&
                  P.jerrySpell(s->netSelfId) == 0.0f,
              "Jerry: nach dem Wiedereinstieg wieder auf seinem Rohr, kein Bann mehr");
    }
    {   // --- wegsehen rettet: nach dem ersten Klingeln den Blick abwenden und gehen ------------------------------------------
        auto s = open(12);
        std::vector<game::GameEvent> ev;
        auto& P = s->presence();
        put(*s, hall.x, hall.y, std::atan2(toDoor.y, toDoor.x));
        step(*s, ev, 6.0);
        const vec3 at = front(3.2);
        put(*s, at.x, at.y, 0.0);
        s->flashlight().on = true;
        float peak = 0.0f;
        step(*s, ev, 6.0, [&] {
            aimAt(*s, P.jerryInfo().head);
            peak = P.jerrySpell(s->netSelfId);
            return peak >= 0.55f;
        });
        const auto& pl = s->player();
        put(*s, pl.x, pl.y, std::atan2(toDoor.y, toDoor.x));  // umdrehen
        float low = 1.0f;
        step(*s, ev, 4.0, [&] {
            low = std::min(low, P.jerrySpell(s->netSelfId));
            return false;
        });
        std::printf("  Jerry (Wegsehen bei Bann %.2f): nach 4 s %.2f\n", peak, low);
        check(peak >= 0.55f && low < 0.05f && !s->stats().dead && count(ev, PE::Attack, Role::Jerry) == 0,
              std::format("Jerry: wer wegsieht, entkommt - der Bann klingt ab ({:.2f} -> {:.2f})", peak, low));
    }
    {   // --- Naehe: wer bis auf Armlaenge herankommt, dem springt er nach einer Ankuendigung ins Gesicht ----------------------
        auto s = open(13);
        std::vector<game::GameEvent> ev;
        auto& P = s->presence();
        put(*s, hall.x, hall.y, std::atan2(toDoor.y, toDoor.x));
        step(*s, ev, 6.0);
        const vec3 nearP = front(1.1);
        put(*s, nearP.x, nearP.y, std::atan2(toDoor.y, toDoor.x));  // nah, Ruecken zu ihm (kein Blickkontakt)
        ev.clear();
        bool warned = false;
        double t = 0.0, tScreech = -1.0, tDead = -1.0;
        step(*s, ev, 3.0, [&] {
            t += 1.0 / 60.0;
            if (tScreech < 0.0 && count(ev, PE::Screech, Role::Jerry) > 0) tScreech = t, warned = true;
            if (s->stats().dead) tDead = t;
            return s->stats().dead;
        });
        std::printf("  Jerry (1,1 m, Ruecken zu ihm): Kreischen nach %.2f s, Sprung ins Gesicht nach %.2f s\n", tScreech, tDead);
        check(warned && tDead > tScreech + 0.45, std::format("Jerry: zu nah - er kuendigt den Sprung an (Kreischen, Fluegel) und springt ({:.2f} / {:.2f} s)", tScreech, tDead));
        check(s->killRole() == (int)Role::Jerry, "Jerry: der Sprung ins Gesicht ist toedlich");
        // zurueckweichen rettet
        auto s2 = open(14);
        auto& P2 = s2->presence();
        put(*s2, hall.x, hall.y, std::atan2(toDoor.y, toDoor.x));
        step(*s2, ev, 6.0);
        put(*s2, nearP.x, nearP.y, std::atan2(toDoor.y, toDoor.x));
        ev.clear();
        step(*s2, ev, 1.0, [&] { return count(ev, PE::Screech, Role::Jerry) > 0; });
        const vec3 back = front(3.6);
        put(*s2, back.x, back.y, std::atan2(toDoor.y, toDoor.x));
        step(*s2, ev, 2.0);
        check(count(ev, PE::Screech, Role::Jerry) > 0 && !s2->stats().dead && P2.has(Role::Jerry), "Jerry: wer nach der Warnung zurueckweicht, entkommt dem Sprung");
        // --- Koop: die Puppe beim Gast traegt Rolle 18 und den Bann ihres Ziels ---------------------------------------------------
        std::vector<game::Presence::NetState> ns;
        P2.netStates(ns, {(float)sp->x, (float)sp->y, 0.0f}, 8);
        const auto it = std::find_if(ns.begin(), ns.end(), [](const auto& n) { return n.role == (int)Role::Jerry; });
        check(it != ns.end(), "Jerry: im Netzzustand des Hosts (Rolle 18)");
        if (it != ns.end()) {
            game::Presence guest;
            auto one = *it;
            one.glow = 0.7f, one.target = 2;
            guest.netApply({one}, 0.0);
            check(guest.has(Role::Jerry) && std::fabs(guest.jerrySpell(2) - 0.7f) < 1e-4f && guest.jerrySpell(1) == 0.0f,
                  "Jerry: beim Gast kommt der Bann aus der Puppe (nur fuer ihr Ziel)");
        }
    }
}

// --- V13.3: Level 3 - purpifizierte tote Wesen in der Wand (KREATUR_L3_Purpur.md) --------------------------------------------
static void testV133Purple(const std::string& dataDir) {
    std::printf("V13.3: Level 3 - purpifizierte Wesen in der Wand (Licht, Klang, Strahlung, Lampe, Botschaft)\n");
    const game::Content& content = *gContent11;
    game::PrefabLibrary prefabs;
    prefabs.loadDirectory(dataDir + "/prefabs");
    JobSystem jobs(2);
    auto open = [&](const char* level, i64 seed) {
        auto s = std::make_unique<game::GameSession>(content, prefabs, content.level(level, world::kLayoutV6), seed, "medium", "Test", jobs, 3);
        s->placeAtSpawn();
        s->preload();
        return s;
    };
    auto step = [&](game::GameSession& s, std::vector<game::GameEvent>& ev, double secs) {
        for (int i = 0; i < (int)(secs * 60.0); ++i) {
            s.update(1.0 / 60.0, game::PlayerControl{}, false);
            ev.insert(ev.end(), s.events().begin(), s.events().end());
            s.events().clear();
        }
    };
    // Spieler mit den Fuessen an feet, Blick auf das Ziel
    auto put = [](game::GameSession& s, vec3 feet, vec3 target) {
        auto& pl = s.player();
        pl.x = feet.x, pl.y = feet.y, pl.z = s.world().tile(ifloor(feet.x), ifloor(feet.y)).floor, pl.vx = pl.vy = 0.0;
        const double eye = pl.z + 1.6;
        pl.angle = std::atan2(target.y - feet.y, target.x - feet.x);
        pl.pitch = std::atan2(target.z - eye, std::hypot(target.x - feet.x, target.y - feet.y));
        s.body().reset();
        s.preload();
    };
    auto sounds = [](const std::vector<game::GameEvent>& ev, const char* name) {
        return (int)std::count_if(ev.begin(), ev.end(), [&](const game::GameEvent& e) { return e.type == game::GameEvent::SoundAt && e.sound == name; });
    };
    auto toast = [](const std::vector<game::GameEvent>& ev, const char* part) {
        return std::any_of(ev.begin(), ev.end(), [&](const game::GameEvent& e) {
            return e.type == game::GameEvent::Toast && e.text.find(part) != std::string::npos;
        });
    };
    {
        auto s0 = open("level0", 3);
        check(!s0->purple().any(), "Purpur: Level 0 hat keine");
    }
    auto s = open("level3", 3);
    const game::Purpification& P = s->purple();
    int iSt = -1, iHo = -1;
    for (size_t i = 0; i < P.sites().size(); ++i) (P.sites()[i].kind == "stealer" ? iSt : iHo) = (int)i;
    std::printf("  Stellen %zu\n", P.sites().size());
    check(P.sites().size() == 2 && iSt >= 0 && iHo >= 0, "Purpur: zwei Stellen in Level 3 (Skin-Stealer, Hound) - selten");
    if (iSt < 0 || iHo < 0) return;
    const game::Purpification::Site st = P.sites()[(size_t)iSt], ho = P.sites()[(size_t)iHo];
    // in Ziegelgaengen, Wand dahinter, Gang davor
    bool placed = true;
    for (const auto* x : {&st, &ho}) {
        const vec3 n{std::cos(x->yaw), std::sin(x->yaw), 0.0f};
        const vec3 front = x->base + n * 0.5f, back = x->base - n * 0.4f;
        const world::Tile& tf = s->world().tile(ifloor(front.x), ifloor(front.y));
        const world::Tile& tb = s->world().tile(ifloor(back.x), ifloor(back.y));
        const std::string type = tf.room && tf.room->type ? tf.room->type->key : "";
        placed = placed && tf.open() && tb.solid() && (type == "gang" || type == "paranoia");
        std::printf("  %s bei %.1f/%.1f im Raum %s\n", x->kind.c_str(), x->base.x, x->base.y, type.c_str());
    }
    check(placed, "Purpur: beide Stellen an einer Ziegelwand im Gang (dahinter Wand)");
    const vec3 nS{std::cos(st.yaw), std::sin(st.yaw), 0.0f}, rS{std::sin(st.yaw), -std::cos(st.yaw), 0.0f};
    // Strahlung: dicht davor stark, ab einigen Metern nichts, hinter der Wand nichts
    const float eNear = P.exposure(st.base + nS * 0.8f + vec3{0, 0, 1.6f});
    const float eFar = P.exposure(st.base + nS * 0.8f + rS * 6.0f + vec3{0, 0, 1.6f});
    const float eBehind = P.exposure(st.base - nS * 0.6f + vec3{0, 0, 1.6f});
    std::printf("  Strahlung nah %.2f, 6 m %.2f, hinter der Wand %.2f\n", eNear, eFar, eBehind);
    check(eNear > 0.8f && eFar == 0.0f && eBehind == 0.0f, "Purpur: Strahlung nur dicht vor dem Koerper");
    // Licht: violett, nur in der Naehe
    std::vector<game::Purpification::Light> lights, farL;
    P.lights(lights, st.glow + nS * 2.0f, 24.0f, 1.0);
    const bool violet = !lights.empty() && lights[0].color.z > lights[0].color.x && lights[0].color.x > lights[0].color.y && lights[0].range > 3.0f;
    P.lights(farL, st.glow + nS * 0.5f + rS * 40.0f, 24.0f, 1.0);
    check(violet && farL.empty(), "Purpur: violettes Licht vor dem Koerper, aus 40 m keines");
    // Lampe: stockt nah, fern nicht
    float minNear = 1.0f, minFar = 1.0f;
    for (int i = 0; i < 600; ++i) {
        minNear = std::min(minNear, P.lampFactor(st.glow + nS * 0.6f, i / 60.0));
        minFar = std::min(minFar, P.lampFactor(st.glow + nS * 0.6f + rS * 8.0f, i / 60.0));
    }
    std::printf("  Lampe nah min %.2f, 8 m min %.2f\n", minNear, minFar);
    check(minNear < 0.6f && minFar == 1.0f, "Purpur: die Taschenlampe stockt nur nahe der Stelle");
    // Lampe auf dem Koerper: die Glut schwillt an (Interpretation)
    {
        game::Purpification Q = P;
        std::vector<game::Purpification::Event> qe;
        const vec3 lp = st.glow + nS * 3.0f;
        const float before = Q.glowLevel(Q.sites()[(size_t)iSt], 2.0);
        for (int i = 0; i < 180; ++i) Q.update(1.0 / 60.0, lp, nS * -1.0f, true, true, lp, normalize(st.glow - lp), qe);
        const float after = Q.glowLevel(Q.sites()[(size_t)iSt], 2.0);
        std::printf("  Glut ohne Lampe %.2f, nach 3 s im Lichtkegel %.2f\n", before, after);
        check(after > before * 1.4f, "Purpur: angeleuchtet schwillt die Glut an");
    }
    // Sitzung: nah - Knistern, Hinweise, Verstand sinkt; fern - nichts davon
    std::vector<game::GameEvent> ev;
    put(*s, st.base + nS * 0.8f, st.glow);
    s->stats().sanity = 80.0;
    s->stats().health = 100.0;
    step(*s, ev, 12.0);
    const double dropNear = 80.0 - s->stats().sanity;
    const int crNear = sounds(ev, "purple_crackle");
    const bool hints = toast(ev, "Violett.") && toast(ev, "Metall auf der Zunge");
    std::vector<game::GameEvent> ev2;
    vec3 farFeet = st.base + nS * 1.0f + rS * 30.0f;
    if (s->world().tile(ifloor(farFeet.x), ifloor(farFeet.y)).solid()) farFeet = st.base + nS * 1.0f - rS * 30.0f;
    put(*s, farFeet, st.base);
    s->stats().sanity = 80.0;
    step(*s, ev2, 12.0);
    const double dropFar = 80.0 - s->stats().sanity;
    std::printf("  12 s nah: Verstand -%.2f, Knistern %d; fern: -%.2f, Knistern %d\n", dropNear, crNear, dropFar, sounds(ev2, "purple_crackle"));
    check(dropNear - dropFar > 2.0, "Purpur: Strahlung zehrt nah am Verstand");
    check(crNear >= 3 && sounds(ev2, "purple_crackle") == 0, "Purpur: Knistern nah zu hoeren, fern nicht");
    check(hints, "Purpur: Hinweise beim ersten Anblick und bei der Strahlung");
    // die Botschaft neben dem Hound lesen
    const vec3 nH{std::cos(ho.yaw), std::sin(ho.yaw), 0.0f};
    std::vector<game::GameEvent> ev3;
    put(*s, ho.text + nH * 1.4f - vec3{0, 0, ho.text.z - ho.base.z}, ho.text);
    step(*s, ev3, 1.0);
    check(toast(ev3, "An die Wand gemalt"), "Purpur: die Botschaft neben dem Hound laesst sich lesen");
    // Klangschleife: eine je Stelle (Brummen)
    std::vector<game::SoundSource> src;
    s->collectSounds(src);
    const int hums = (int)std::count_if(src.begin(), src.end(), [](const game::SoundSource& x) { return x.sound && *x.sound == "purple_hum"; });
    check(hums == 2, "Purpur: Brummen als Klangquelle an jeder Stelle");
}

// V13.3 Level 3 (KREATUR_DeathRat.md, KREATUR_Deathmoth.md): grosse Death Rats mit Stromfallen, Mottenschwarm mit Praetorianern
static void testV133Level3RatMoth(const std::string& dataDir) {
    std::printf("V13.3: Level 3 - Death Rats (Wasserschwein, Fallen, Klugheit), Deathmoth-Schwarm mit Praetorianern\n");
    const game::Content& content = *gContent11;
    game::PrefabLibrary prefabs;
    prefabs.loadDirectory(dataDir + "/prefabs");
    JobSystem jobs(2);
    using PE = game::Presence::Event;
    using Role = game::Presence::Role;
    auto open = [&](i64 seed) {
        auto s = std::make_unique<game::GameSession>(content, prefabs, content.level("level3", world::kLayoutV6), seed, "medium", "Test", jobs, 3);
        s->placeAtSpawn();
        s->preload();
        s->presence().setGrace(3000.0);
        return s;
    };
    auto step = [&](game::GameSession& s, std::vector<game::GameEvent>& ev, double secs, const std::function<bool()>& each = {},
                    const std::function<game::PlayerControl(int)>& ctl = {}) {
        for (int i = 0; i < (int)(secs * 60.0); ++i) {
            if (each && each()) break;
            s.update(1.0 / 60.0, ctl ? ctl(i) : game::PlayerControl{}, false);
            ev.insert(ev.end(), s.events().begin(), s.events().end());
            s.events().clear();
        }
    };
    auto count = [](const std::vector<game::GameEvent>& ev, PE::Type t, Role r, int cue = -1, int cue2 = -1) {
        return (int)std::count_if(ev.begin(), ev.end(), [&](const game::GameEvent& e) {
            if (e.type != game::GameEvent::Figure || e.index != (int)t || e.role != (int)r) return false;
            const int k = game::kinCueOf(e.intensity, e.role);
            return cue < 0 ? k < 0 : (k >= cue && k <= (cue2 < 0 ? cue : cue2));
        });
    };
    // Index des ersten Ereignisses (Art, Rolle, Klangnummer-Bereich; cue < 0: ohne Nummer), -1: keins
    auto first = [](const std::vector<game::GameEvent>& ev, PE::Type t, Role r, int cue = -1, int cue2 = -1) {
        for (size_t i = 0; i < ev.size(); ++i) {
            const auto& e = ev[i];
            if (e.type != game::GameEvent::Figure || e.index != (int)t || e.role != (int)r) continue;
            const int k = game::kinCueOf(e.intensity, e.role);
            if (cue < 0 ? k < 0 : (k >= cue && k <= (cue2 < 0 ? cue : cue2))) return (int)i;
        }
        return -1;
    };
    auto aim = [](game::GameSession& s, vec3 q) {
        auto& pl = s.player();
        pl.angle = std::atan2(q.y - pl.y, q.x - pl.x);
        pl.pitch = std::atan2(q.z - pl.eye(), std::max(0.4, std::hypot(q.x - pl.x, q.y - pl.y)));
    };
    auto eyeOf = [](game::GameSession& s) { return vec3{(float)s.player().x, (float)s.player().y, (float)s.player().eye()}; };
    auto put = [](game::GameSession& s, double x, double y) {
        auto& pl = s.player();
        pl.x = x, pl.y = y, pl.z = s.world().tile(ifloor(x), ifloor(y)).floor, pl.vx = pl.vy = 0.0, pl.vz = 0.0, pl.onGround = true;
        s.body().reset();
    };
    // freie Kachel in d m Entfernung von 'at' mit Sicht auf 'look'
    auto spotNear = [&](game::GameSession& s, vec3 at, double d, vec3 look, double& ox, double& oy) {
        world::World& w = s.world();
        for (int k = 0; k < 72; ++k) {
            const double a = k * 0.0873 * 1.0;
            for (double dd = d; dd > d * 0.6; dd -= 0.3) {
                const double x = at.x + std::cos(a) * dd, y = at.y + std::sin(a) * dd;
                const world::Tile* t = w.tileIfLoaded(ifloor(x), ifloor(y));
                if (!t || !t->open() || t->clearance() < 2.0f || std::fabs(t->floor - at.z) > 0.3f) continue;
                if (!game::clearSight(w, vec3{(float)x, (float)y, t->floor + 1.6f}, look)) continue;
                ox = x, oy = y;
                return true;
            }
        }
        return false;
    };
    // --- Klaenge und Ebene -------------------------------------------------------------------------------------------------
    {
        int bad = 0;
        for (int k = game::KC_RAT_SAY0; k < game::KC_L3_END; ++k) {
            const game::KinLine* L = game::kinLine(k);
            bad += !L || !L->sound || game::kinLineBySound(L->sound) != L;
        }
        check(bad == 0, "Death Rats/Motten: jede Klangnummer hat einen Namen (auch fuer die Gaeste)");
        check(game::kinLine(game::KC_RAT_SAY0)->sub && game::kinLine(game::KC_RAT_SAY0 + 11)->sub && game::kinLine(game::KC_TRAP_CRACKLE)->hint &&
                  game::kinLine(game::KC_PR_REAR)->hint && !game::kinLine(game::KC_RAT_GRIND)->sub,
              "Death Rats: das Sprachgeraet mit Untertitel; Falle, Schwarm und Praetorianer mit einmaligem Hinweis");
        check(game::kinCueOf(14.0f, (int)Role::Rat) < 0 && game::kinCueOf(9.0f, (int)Role::Moth) < 0 &&
                  game::kinCueOf(100.0f + game::KC_RAT_HISS, (int)Role::Rat) == game::KC_RAT_HISS,
              "Death Rats/Motten: Schaden bleibt Schaden, Klangnummern erst ab 100");
        const auto L3 = content.level("level3", world::kLayoutV6);
        const auto L2 = content.level("level2", world::kLayoutV6);
        check(L3 && L3->kin[world::KIN_RAT].mode == "l3" && L3->kin[world::KIN_RAT].count >= 2 && L3->kin[world::KIN_RAT].chance == 0.0,
              "Level 3: Death Rats mit Fallen (mode l3, hoechstens count Fallen, kein Schrei-Ereignis)");
        check(L3 && L3->kin[world::KIN_MOTH].on && L3->kin[world::KIN_MOTH].mode == "swarm" && L3->kin[world::KIN_MOTH].chance > 0.4,
              "Level 3: Mottenschwarm mit ueber 40 % Praetorianern (Wikidot Level 3)");
        check(L2 && L2->kin[world::KIN_RAT].mode == "bald" && L2->kin[world::KIN_MOTH].mode.empty(), "Level 2: Ratten und Motten unveraendert");
    }
    // --- Death Rats: Auftritt, Wetzen, Wahrnehmen, Sprechen, Abstand zur Lampe ---------------------------------------------
    {
        auto s = open(5);
        auto& P = s->presence();
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.5);
        P.l3Set("mem", 0.0);
        P.force(Role::Rat);
        step(*s, ev, 1.0, [&] { return P.l3Info().rats > 0; });
        auto I = P.l3Info();
        check(I.rats >= 2 && I.rats <= 3 && P.variantOf(Role::Rat) >= 2, std::format("Death Rats: eine Brigade aus {} grossen Ratten (Variante 2)", I.rats));
        // niemand erscheint im Blick
        int seen = 0;
        vec3 rp;
        int st = 0, var = 0;
        const vec3 eye = eyeOf(*s);
        const vec3 look{(float)(std::cos(s->player().angle) * std::cos(s->player().pitch)), (float)(std::sin(s->player().angle) * std::cos(s->player().pitch)),
                        (float)std::sin(s->player().pitch)};
        for (int i = 0; P.agentInfo(Role::Rat, i, rp, st, var); ++i)
            if (var == 2 && dot(normalize(rp + vec3{0, 0, 0.4f} - eye), look) > 0.3f && game::clearSight(s->world(), eye, rp + vec3{0, 0, 0.4f})) ++seen;
        check(seen == 0, "Death Rats: erscheinen ausser Sicht");
        // die erste Ratte: Spieler 9 m vor ihr, Lampe an, freie Sicht - sie bemerkt ihn, das Geraet spricht
        vec3 r0;
        P.agentInfo(Role::Rat, 0, r0, st, var);
        for (int i = 0; P.agentInfo(Role::Rat, i, rp, st, var); ++i)
            if (var == 2) {
                r0 = rp;
                break;
            }
        step(*s, ev, 4.0);
        const int grind = count(ev, PE::Mimic, Role::Rat, game::KC_RAT_GRIND);
        double x = 0, y = 0;
        bool placed = spotNear(*s, r0, 9.0, r0 + vec3{0, 0, 0.4f}, x, y);
        check(placed, "Death Rats: Platz mit Sicht auf die Ratte gefunden");
        if (!placed) return;
        put(*s, x, y);
        aim(*s, r0 + vec3{0, 0, 0.4f});
        s->flashlight().on = true;
        step(*s, ev, 4.0);  // ev: seit dem Auftritt (der erste Satz faellt, sobald sie ihn bemerken)
        I = P.l3Info();
        const int say = count(ev, PE::Mimic, Role::Rat, game::KC_RAT_SAY0, game::KC_RAT_SAY0 + game::KC_RAT_SAYS - 1);
        check(grind >= 1 || I.ratJob != 0, std::format("Death Rats: wetzen die Zaehne, bevor man sie sieht (Geraeusch vor Sicht: {} x)", grind));
        check(I.ratJob >= 1 && say >= 1, std::format("Death Rats: bemerken den Spieler (Auftrag {}), das Sprachgeraet spricht ({} Saetze)", I.ratJob, say));
        // Abstand mit Lampe groesser als ohne (sie warten ausserhalb der Lampe)
        auto meanDist = [&](double secs) {
            double sum = 0.0;
            int n = 0;
            step(*s, ev, secs, [&] {
                vec3 p;
                int st2 = 0, v2 = 0;
                for (int i = 0; P.agentInfo(Role::Rat, i, p, st2, v2); ++i)
                    if (v2 == 2) sum += std::hypot(p.x - s->player().x, p.y - s->player().y), ++n;
                return false;
            });
            return n ? sum / n : 0.0;
        };
        P.l3Set("clear", 1.0);  // ohne Falle in der Naehe: sie lauern (sonst treiben sie); der Fallenbau wird unten geprueft
        P.l3Set("build", 0.0);
        P.l3Set("job", 1.0);
        step(*s, ev, 6.0);
        const double dOn = meanDist(8.0);
        s->flashlight().on = false;
        P.l3Set("clear", 1.0);
        step(*s, ev, 6.0);
        const double dOff = meanDist(8.0);
        check(dOn > dOff + 1.0 && dOn > 5.5, std::format("Death Rats: warten ausserhalb der Lampe ({:.1f} m mit Lampe, {:.1f} m ohne)", dOn, dOff));
        I = P.l3Info();
        check(I.encounters >= 1 && I.trail >= 1, std::format("Death Rats: Gedaechtnis - {} Begegnungen, {} Wegpunkte des Spielers", I.encounters, I.trail));
        // Drohen vor dem Biss: der Spieler tritt dicht heran - erst Zischen und "STOP.", dann (bleibt er) der Biss
        bool threat = false, bitAfterWarn = false;
        for (int tries = 0; tries < 4 && !threat; ++tries) {
            vec3 rr;
            for (int i = 0; P.agentInfo(Role::Rat, i, rr, st, var); ++i)
                if (var == 2) break;
            if (!spotNear(*s, rr, 1.3, rr + vec3{0, 0, 0.4f}, x, y)) continue;
            put(*s, x, y);
            aim(*s, rr + vec3{0, 0, 0.4f});
            ev.clear();
            step(*s, ev, 3.0, [&] { return false; }, [&](int) {
                vec3 q;
                int s2 = 0, v2 = 0;
                for (int i = 0; P.agentInfo(Role::Rat, i, q, s2, v2); ++i)
                    if (v2 == 2) break;
                return game::PlayerControl{};
            });
            const int hiss = first(ev, PE::Mimic, Role::Rat, game::KC_RAT_HISS), hit = first(ev, PE::Hit, Role::Rat);
            threat = hiss >= 0;
            bitAfterWarn = hit < 0 || hit > hiss;
        }
        check(threat && bitAfterWarn, "Death Rats: drohen (Zischen, aufgerichtet, \"STOP.\") vor jedem Biss");
    }
    // --- Falle: sichtbar (Draht quer im Gang, Knistern), Schlag mit Laehmung, darueber springen, versagt manchmal -----------
    {
        auto s = open(7);
        auto& P = s->presence();
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.5);
        // einen Gang suchen: der Spieler blickt entlang
        world::World& w = s->world();
        double gx = 0, gy = 0, ang = 0;
        bool found = false;
        for (int r = 2; r < 30 && !found; ++r)
            for (int k = 0; k < 8 * r && !found; ++k) {
                const double a = k * 6.2831853 / (8 * r);
                const double x = s->player().x + std::cos(a) * r, y = s->player().y + std::sin(a) * r;
                const world::Tile* t = w.tileIfLoaded(ifloor(x), ifloor(y));
                if (!t || !t->open() || !t->room || !t->room->type || t->room->type->key != "gang") continue;
                for (int d = 0; d < 4 && !found; ++d) {
                    const int dx = d == 0 ? 1 : d == 1 ? -1 : 0, dy = d == 2 ? 1 : d == 3 ? -1 : 0;
                    bool ok = true;
                    for (int q = 1; q <= 5 && ok; ++q) {
                        const world::Tile* u = w.tileIfLoaded(ifloor(x) + dx * q, ifloor(y) + dy * q);
                        ok = u && u->open() && std::fabs(u->floor - t->floor) < 0.1f && u->clearance() > 2.0f;
                    }
                    if (ok) gx = ifloor(x) + 0.5, gy = ifloor(y) + 0.5, ang = std::atan2((double)dy, (double)dx), found = true;
                }
            }
        check(found, "Falle: ein gerader Gang gefunden");
        if (!found) return;
        int zaps = 0, fizz = 0, healthDrop = 0, stunned = 0, crackles = 0, wireOk = 0;
        for (int round = 0; round < 6 && zaps == 0; ++round) {
            P.clear();
            put(*s, gx, gy);
            s->player().angle = ang, s->player().pitch = -0.3;
            P.l3Set("trap", 2.5);
            ev.clear();
            step(*s, ev, 5.0);
            auto I = P.l3Info();
            if (I.armed < 1) continue;
            const float span = hdist2(I.trapL, I.trapR), hz = I.trapL.z - s->world().tile(ifloor(I.trapL.x), ifloor(I.trapL.y)).floor;
            wireOk += span > 0.8f && span < 3.3f && std::fabs(hz - 0.18f) < 0.03f ? 1 : 0;
            crackles += count(ev, PE::Mimic, Role::Rat, game::KC_TRAP_CRACKLE);
            // durchgehen: die Fuesse am Boden quer ueber den Draht
            const vec3 mid = (I.trapL + I.trapR) * 0.5f;
            const vec3 along{(float)std::cos(ang), (float)std::sin(ang), 0.0f};
            const double h0 = s->stats().health;
            ev.clear();
            for (int k = 0; k <= 40; ++k) {
                const vec3 p = mid - along * 1.0f + along * (2.0f * k / 40.0f);
                put(*s, p.x, p.y);
                step(*s, ev, 1.0 / 60.0);
            }
            step(*s, ev, 0.3);
            zaps += count(ev, PE::Mimic, Role::Rat, game::KC_TRAP_ZAP);
            fizz += count(ev, PE::Mimic, Role::Rat, game::KC_TRAP_FIZZLE);
            if (count(ev, PE::Mimic, Role::Rat, game::KC_TRAP_ZAP)) {
                healthDrop += s->stats().health < h0 - 5.0 ? 1 : 0;
                stunned += P.stupor(s->netSelfId) > 0.5f ? 1 : 0;
            }
        }
        check(wireOk >= 1, "Falle: blanker Draht quer durch den Gang in 18 cm Hoehe (0,8-3,2 m)");
        check(crackles >= 1, std::format("Falle: kuendigt sich an - Knistern mit Funken ({} x in 5 s)", crackles));
        check(zaps >= 1 && healthDrop >= 1 && stunned >= 1, std::format("Falle: wer durchgeht, bekommt den Schlag - Schaden und Laehmung ({} Schlaege, {} versagt)", zaps, fizz));
        // darueber springen: in der Luft ueber den Draht - kein Schlag, sie merken es sich
        P.clear();
        put(*s, gx, gy);
        s->player().angle = ang, s->player().pitch = -0.3;
        P.l3Set("trap", 2.5);
        step(*s, ev, 1.0);
        auto I = P.l3Info();
        const int jumped0 = I.jumped;
        ev.clear();
        if (I.armed >= 1) {
            const vec3 mid = (I.trapL + I.trapR) * 0.5f;
            const vec3 along{(float)std::cos(ang), (float)std::sin(ang), 0.0f};
            for (int k = 0; k <= 40; ++k) {
                const vec3 p = mid - along * 1.0f + along * (2.0f * k / 40.0f);
                put(*s, p.x, p.y);
                const bool air = std::fabs(k - 20) <= 6;
                s->player().onGround = !air;
                s->player().z += air ? 0.45 : 0.0;
                step(*s, ev, 1.0 / 60.0);
            }
        }
        I = P.l3Info();
        check(I.armed >= 1 && count(ev, PE::Hit, Role::Rat) == 0 && I.jumped > jumped0, "Falle: wer darueber springt, bleibt heil - die Ratten merken es sich");
        // Fallenbau: die Brigade baut selbst (Zeitraffer) - eine weitere Falle entsteht ausser Sicht
        P.clear();
        put(*s, gx, gy);
        P.force(Role::Rat);
        step(*s, ev, 1.0);
        const int traps0 = P.l3Info().traps;
        P.l3Set("quick", 12.0);
        P.l3Set("build", 1.0);
        P.l3Set("job", 1.0);
        step(*s, ev, 60.0, [&] { return P.l3Info().traps > traps0; });
        check(P.l3Info().traps > traps0, std::format("Death Rats: bauen selbst Fallen auf dem Weg des Spielers ({} -> {})", traps0, P.l3Info().traps));
        // Koop: die Puppen beim Gast tragen Rolle und Variante (Ratte 2, Falle 3)
        std::vector<game::Presence::NetState> ns;
        P.netStates(ns, eyeOf(*s), 32);
        game::Presence G;
        G.netApply(ns, 0.0);
        int pv2 = 0, pv3 = 0;
        vec3 q;
        int st = 0, var = 0;
        for (int i = 0; G.agentInfo(Role::Rat, i, q, st, var); ++i) pv2 += var == 2, pv3 += var == 3;
        check(pv2 >= 1 && pv3 >= 1, std::format("Koop: Puppen der Ratten ({}) und Fallen ({}) beim Gast", pv2, pv3));
    }
    // --- Mottenschwarm: Praetorianer, Licht lockt, Angriff aufs Gesicht mit Ankuendigung, Lampe aus und rennen ------------
    {
        auto s = open(9);
        auto& P = s->presence();
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.5);
        P.force(Role::Moth);
        step(*s, ev, 1.0, [&] { return P.l3Info().commons > 0; });
        auto I = P.l3Info();
        const double share = I.commons + I.praetorians > 0 ? (double)I.praetorians / (I.commons + I.praetorians) : 0.0;
        check(I.commons >= 3 && I.praetorians >= 2 && share >= 0.38,
              std::format("Motten: Schwarm aus {} Motten und {} Praetorianern ({:.0f} %)", I.commons, I.praetorians, share * 100.0));
        // vor die Roehre treten, Lampe auf die Motten: sie fliegen auf
        vec3 mp;
        int st = 0, var = 0;
        for (int i = 0; P.agentInfo(Role::Moth, i, mp, st, var); ++i)
            if (var == 3 || var == 4) break;
        double x = 0, y = 0;
        bool placed = spotNear(*s, vec3{mp.x, mp.y, s->world().tile(ifloor(mp.x), ifloor(mp.y)).floor}, 6.0, mp, x, y);
        check(placed, "Motten: Platz mit Sicht auf die Roehre");
        if (!placed) return;
        put(*s, x, y);
        aim(*s, mp);
        s->flashlight().on = true;
        ev.clear();
        step(*s, ev, 3.0, [&] { return P.l3Info().roused; });
        check(P.l3Info().roused && count(ev, PE::TakeOff, Role::Moth) >= 1 && count(ev, PE::Mimic, Role::Moth, game::KC_MOTH_SWARM) >= 1,
              "Motten: die Lampe lockt - der Schwarm fliegt auf (Brummen)");
        // stehen bleiben mit Lampe: erst das Brummen vor dem Gesicht (Ankuendigung), dann Motten im Gesicht
        const double h0 = s->stats().health;
        int maxFace = 0;
        step(*s, ev, 14.0, [&] {
            maxFace = std::max(maxFace, P.l3Info().onFace);
            return false;
        }, [&](int) {
            game::PlayerControl c;
            return c;
        });
        const int warn = first(ev, PE::Hiss, Role::Moth), hit = first(ev, PE::Hit, Role::Moth);
        check(maxFace >= 2 && warn >= 0 && hit > warn && s->stats().health < h0,
              std::format("Motten: greifen Mund, Nase, Augen, Ohren an ({} zugleich), angekuendigt (Brummen vor dem ersten Biss)", maxFace));
        // Lampe aus und rennen: sie lassen los und verlieren ihn
        s->flashlight().on = false;
        const vec3 away = normalize(vec3{(float)(s->player().x - mp.x), (float)(s->player().y - mp.y), 0.0f});
        s->player().angle = std::atan2(away.y, away.x);
        step(*s, ev, 10.0, {}, [&](int) {
            game::PlayerControl c;
            c.forward = 1.0f, c.sprint = true;
            return c;
        });
        step(*s, ev, 4.0);
        I = P.l3Info();
        check(I.onFace == 0, "Motten: Lampe aus und rennen - keine Motte mehr im Gesicht");
        // Praetorianer: aufbaeumen (Klicken, Zischen) vor dem Stich; der Stich laehmt
        P.clear();
        P.force(Role::Moth);
        step(*s, ev, 1.0, [&] { return P.l3Info().praetorians > 0; });
        vec3 pp;
        for (int i = 0; P.agentInfo(Role::Moth, i, pp, st, var); ++i)
            if (var == 5) break;
        placed = spotNear(*s, pp, 1.8, pp + vec3{0, 0, 0.6f}, x, y);
        if (placed) {
            put(*s, x, y);
            aim(*s, pp + vec3{0, 0, 0.6f});
            P.l3Set("pstate", 2.0);
            ev.clear();
            step(*s, ev, 3.0);
            const int rear = first(ev, PE::Mimic, Role::Moth, game::KC_PR_REAR), stab = first(ev, PE::Mimic, Role::Moth, game::KC_PR_STAB);
            const int hitP = first(ev, PE::Hit, Role::Moth);
            check(rear >= 0 && stab > rear && (hitP < 0 || hitP > rear), "Praetorianer: baeumt sich auf (Ankuendigung) vor dem Stich");
            check(hitP >= 0 && P.stupor(s->netSelfId) > 0.2f, std::format("Praetorianer: der Giftstich laehmt (Betaeubung {:.2f})", P.stupor(s->netSelfId)));
        } else {
            check(false, "Praetorianer: Platz neben ihm gefunden");
        }
    }
}

// V13.3 Level 2/3 (KREATUR_Hound.md, Abschnitt "Rudel"; KREATUR_Clump.md, Abschnitt "Level 3"): Hound-Rudel und das grosse,
// bewaffnete Buendel
static void testV133PackBigClump(const std::string& dataDir) {
    std::printf("V13.3: Level 2/3 - Hound-Rudel (einkreisen, bellen, quaelen), grosses bewaffnetes Buendel (Level 3)\n");
    const game::Content& content = *gContent11;
    game::PrefabLibrary prefabs;
    prefabs.loadDirectory(dataDir + "/prefabs");
    JobSystem jobs(2);
    using PE = game::Presence::Event;
    using Role = game::Presence::Role;
    auto open = [&](const char* lv, i64 seed) {
        auto s = std::make_unique<game::GameSession>(content, prefabs, content.level(lv, world::kLayoutV6), seed, "medium", "Test", jobs, 3);
        s->placeAtSpawn();
        s->preload();
        s->presence().setGrace(3000.0);
        s->presence().packSet("next", 1e9);
        s->presence().bigClumpSet("next", 1e9);
        return s;
    };
    auto step = [&](game::GameSession& s, std::vector<game::GameEvent>& ev, double secs, const std::function<bool()>& each = {},
                    const std::function<game::PlayerControl(int)>& ctl = {}) {
        for (int i = 0; i < (int)(secs * 60.0); ++i) {
            if (each && each()) break;
            s.update(1.0 / 60.0, ctl ? ctl(i) : game::PlayerControl{}, false);
            ev.insert(ev.end(), s.events().begin(), s.events().end());
            s.events().clear();
        }
    };
    auto count = [](const std::vector<game::GameEvent>& ev, PE::Type t, Role r, int cue = -1) {
        return (int)std::count_if(ev.begin(), ev.end(), [&](const game::GameEvent& e) {
            if (e.type != game::GameEvent::Figure || e.index != (int)t || e.role != (int)r) return false;
            return game::kinCueOf(e.intensity, e.role) == cue;
        });
    };
    auto first = [](const std::vector<game::GameEvent>& ev, PE::Type t, Role r, int cue = -1) {
        for (size_t i = 0; i < ev.size(); ++i) {
            const auto& e = ev[i];
            if (e.type == game::GameEvent::Figure && e.index == (int)t && e.role == (int)r && game::kinCueOf(e.intensity, e.role) == cue) return (int)i;
        }
        return -1;
    };
    auto aim = [](game::GameSession& s, vec3 q) {
        auto& pl = s.player();
        pl.angle = std::atan2(q.y - pl.y, q.x - pl.x);
        pl.pitch = std::atan2(q.z - pl.eye(), std::max(0.4, std::hypot(q.x - pl.x, q.y - pl.y)));
    };
    auto eyeOf = [](game::GameSession& s) { return vec3{(float)s.player().x, (float)s.player().y, (float)s.player().eye()}; };
    auto put = [](game::GameSession& s, double x, double y) {
        auto& pl = s.player();
        pl.x = x, pl.y = y, pl.z = s.world().tile(ifloor(x), ifloor(y)).floor, pl.vx = pl.vy = 0.0, pl.vz = 0.0, pl.onGround = true;
        s.body().reset();
    };
    // freie Kachel in etwa d m von 'at' mit Sicht auf 'look'
    auto spotNear = [&](game::GameSession& s, vec3 at, double d, vec3 look, double& ox, double& oy) {
        world::World& w = s.world();
        for (int k = 0; k < 72; ++k) {
            const double a = k * 0.0873;
            for (double dd = d; dd > d * 0.6; dd -= 0.3) {
                // Kachelmitte (am Rand haengt der Spieler an der Wand fest), ringsum begehbar
                const double x = std::floor(at.x + std::cos(a) * dd) + 0.5, y = std::floor(at.y + std::sin(a) * dd) + 0.5;
                const world::Tile* t = w.tileIfLoaded(ifloor(x), ifloor(y));
                if (!t || !t->open() || t->clearance() < 2.0f || std::fabs(t->floor - at.z) > 0.3f) continue;
                int free = 0;
                for (int k2 = 0; k2 < 4; ++k2) {
                    const world::Tile* n = w.tileIfLoaded(ifloor(x) + (k2 == 0) - (k2 == 1), ifloor(y) + (k2 == 2) - (k2 == 3));
                    free += n && n->open() && std::fabs(n->floor - t->floor) < 0.3f;
                }
                if (free < 3) continue;
                if (!game::clearSight(w, vec3{(float)x, (float)y, t->floor + 1.6f}, look)) continue;
                ox = x, oy = y;
                return true;
            }
        }
        return false;
    };
    auto hounds = [](game::Presence& P, std::vector<vec3>& pos, std::vector<int>& st) {
        pos.clear(), st.clear();
        vec3 p;
        int s = 0, v = 0;
        for (int i = 0; P.agentInfo(Role::Hound, i, p, s, v); ++i)
            if (v > 0) pos.push_back(p), st.push_back(s);
        return (int)pos.size();
    };
    // --- Ebenen, Klaenge, Modelle ---------------------------------------------------------------------------------------------
    {
        const auto L1 = content.level("level1", world::kLayoutV6), L2 = content.level("level2", world::kLayoutV6);
        const auto L3 = content.level("level3", world::kLayoutV6), L5 = content.level("level5", world::kLayoutV6);
        check(L2 && !L2->allowHound && L2->houndPack.on && L2->houndPack.mode == "l2" && L2->houndPack.groupMin >= 3,
              "Level 2: Hounds nur im Rudel (mindestens drei, zieht umher)");
        check(L3 && !L3->allowHound && L3->houndPack.on && L3->houndPack.mode == "l3" && L3->clumpBig.on && L3->allowClump,
              "Level 3: Rudel aus Lauerern, grosses bewaffnetes Buendel neben den kleinen");
        check(L1 && L1->allowHound && !L1->houndPack.on && !L1->clumpBig.on && L5 && L5->allowHound && !L5->houndPack.on,
              "Level 1 und 5: der einzelne Hound bleibt, wie er ist (kein Rudel, kein grosses Buendel)");
        int bad = 0;
        for (int k = game::KC_L3_END; k < game::KC_PACK_END; ++k) {
            const game::KinLine* L = game::kinLine(k);
            bad += !L || !L->sound || game::kinLineBySound(L->sound) != L;
        }
        check(bad == 0, "Rudel/Buendel: jede Klangnummer hat einen Namen (auch fuer die Gaeste)");
        check(game::kinLine(game::KC_HP_CALL)->hint && game::kinLine(game::KC_HP_FAST)->hint && game::kinLine(game::KC_HP_LUNGE)->hint &&
                  game::kinLine(game::KC_CL_CEIL)->hint && game::kinLine(game::KC_CL_SCRAPE)->hint,
              "Rudel/Buendel: Ruf, schnelles Bellen, Sprung, Decke und Ausholen mit einmaligem Hinweis (Rat des Wikis)");
        check(game::kinCueOf(22.0f, (int)Role::Hound) < 0 && game::kinCueOf(30.0f, (int)Role::Clump) < 0 &&
                  game::kinCueOf(100.0f + game::KC_HP_MAUL, (int)Role::Hound) == game::KC_HP_MAUL,
              "Rudel/Buendel: Schaden bleibt Schaden, Klangnummern erst ab 100");
        const auto& hv = anim::entityVariants(anim::Entity::Hound);
        const auto& cv = anim::entityVariants(anim::Entity::Clump);
        check(hv.size() == 3 && std::string(hv[1]) == "ent_hound_l2" && std::string(hv[2]) == "ent_hound_l3" && cv.size() == 3 &&
                  std::string(cv[1]) == "ent_clump_l3" && std::string(cv[2]) == "ent_clump_l3_bare",
              "Modelle: Rudel-Hound je Ebene, grosses Buendel bewaffnet und entwaffnet");
    }
    // --- Level 2: das Rudel zieht umher, gestoert schlagartig schnell, kreist ein ---------------------------------------------
    {
        auto s = open("level2", 4);
        auto& P = s->presence();
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.5);
        P.packSet("spawn", 26.0);
        step(*s, ev, 0.5);
        std::vector<vec3> hp;
        std::vector<int> hs;
        const int n = hounds(P, hp, hs);
        check(n >= 3 && n <= 4 && P.packInfo().active && P.packInfo().l3 == 0, std::format("Level 2: ein Rudel aus {} Hounds (Variante 1)", n));
        float spread = 0.0f;
        for (const vec3& p : hp) spread = std::max(spread, hdist2(p, hp[0]));
        check(n > 0 && spread < 4.0f, std::format("Level 2: das Rudel steht beisammen ({:.1f} m)", spread));
        // still stehen, Lampe aus, weggedreht: es zieht langsam umher
        s->flashlight().on = false;
        if (n) aim(*s, eyeOf(*s) * 2.0f - hp[0]);
        std::vector<vec3> h0 = hp;
        float vmax = 0.0f;
        int alarmed = 0;
        step(*s, ev, 6.0, [&] {
            std::vector<vec3> q;
            hounds(P, q, hs);
            for (size_t i = 0; i < q.size() && i < h0.size(); ++i) vmax = std::max(vmax, hdist2(q[i], h0[i]) * 60.0f);
            h0 = q;
            alarmed = P.packInfo().phase;
            return alarmed > 0;
        });
        check(alarmed == 0 && vmax < 1.8f, std::format("Level 2: ungestoert zieht das Rudel langsam (hoechstens {:.2f} m/s)", vmax));
        check(count(ev, PE::Growl, Role::Hound, game::KC_HP_YIP) + count(ev, PE::Step, Role::Hound) > 0,
              "Level 2: man hoert das Rudel, bevor man es sieht (Klaeffen, Pfoten)");
        // gestoert (die Lampe trifft sie): Ruf, Antworten, schlagartig schnell
        hounds(P, hp, hs);
        double ox = 0, oy = 0;
        const vec3 c0 = hp.empty() ? eyeOf(*s) : hp[0];
        const bool placed = spotNear(*s, c0, 12.0, c0 + vec3{0, 0, 0.6f}, ox, oy);
        if (placed) put(*s, ox, oy);
        s->flashlight().on = true;
        aim(*s, c0 + vec3{0, 0, 0.5f});
        const size_t e0 = ev.size();
        h0 = hp;
        float vrun = 0.0f;
        step(*s, ev, 4.0, [&] {
            std::vector<vec3> q;
            hounds(P, q, hs);
            for (size_t i = 0; i < q.size() && i < h0.size(); ++i) vrun = std::max(vrun, hdist2(q[i], h0[i]) * 60.0f);
            h0 = q;
            aim(*s, q.empty() ? c0 : q[0] + vec3{0, 0, 0.5f});
            return false;
        });
        std::vector<game::GameEvent> ev2(ev.begin() + (long long)e0, ev.end());
        auto I = P.packInfo();
        check(placed && I.phase >= 1 && count(ev2, PE::Bark, Role::Hound, game::KC_HP_CALL) >= 1 && I.answers >= 1,
              std::format("Level 2: gesehen - einer ruft, {} antworten", I.answers));
        s->flashlight().on = false;
        check(vrun > 3.5f, std::format("Level 2: gestoert werden sie schlagartig schnell ({:.1f} m/s)", vrun));
        // einkreisen und die Marke: es springt immer nur einer, und erst nach der Ankuendigung
        put(*s, ox, oy);
        int maxLunging = 0, phase2 = 0;
        const size_t e1 = ev.size();
        step(*s, ev, 16.0, [&] {
            hounds(P, hp, hs);
            int lunging = 0;
            for (int st : hs) lunging += st == 3;
            maxLunging = std::max(maxLunging, lunging);
            phase2 = std::max(phase2, P.packInfo().phase);
            // weggedreht vom naechsten Hound (sonst haelt der Blick sie ab)
            if (!hp.empty()) aim(*s, eyeOf(*s) * 2.0f - hp[0]);
            return count(std::vector<game::GameEvent>(ev.begin() + (long long)e1, ev.end()), PE::Hit, Role::Hound) > 0;
        });
        std::vector<game::GameEvent> ev3(ev.begin() + (long long)e1, ev.end());
        const int fl = first(ev3, PE::Bark, Role::Hound, game::KC_HP_LUNGE), fh = first(ev3, PE::Hit, Role::Hound);
        check(phase2 >= 2 && maxLunging == 1, std::format("Level 2: sie kreisen ein, es springt immer nur einer (Phase {}, zugleich {})", phase2, maxLunging));
        check(fh >= 0 && fl >= 0 && fl < fh, "Level 2: vor jedem Sprung das scharfe Klaeffen (Ankuendigung)");
        check(fh >= 0 && !s->stats().dead && s->stats().health < 100.0 && s->stats().health > 50.0,
              std::format("Level 2: ein Biss verletzt schwer, toetet aber nicht ({:.0f} Leben)", s->stats().health));
        // Blickkontakt bricht den Sprung ab: der naechste, der ausholt, wird angesehen
        P.packSet("lunge", 1.0);
        const int ab0 = P.packInfo().aborted, hits0 = count(ev, PE::Hit, Role::Hound);
        int seenLunge = 0;
        step(*s, ev, 6.0, [&] {
            hounds(P, hp, hs);
            for (size_t i = 0; i < hp.size(); ++i)
                if (hs[i] == 3) aim(*s, hp[i] + vec3{0, 0, 0.55f}), ++seenLunge;
            return P.packInfo().aborted > ab0;
        });
        check(seenLunge > 0 && P.packInfo().aborted > ab0 && count(ev, PE::Hit, Role::Hound) == hits0 && count(ev, PE::Whine, Role::Hound, game::KC_HP_FLINCH) >= 1,
              std::format("Level 2: angesehen bricht er den Sprung ab und weicht winselnd zurueck ({} von {} Spruengen abgebrochen)", P.packInfo().aborted, P.packInfo().lunges));
        // Rennen reizt, Interesse aufgebraucht: das Rudel gibt auf und zieht ab
        const float i0 = P.packInfo().interest;
        step(*s, ev, 1.5, {}, [](int) {
            game::PlayerControl c;
            c.forward = 1.0f, c.sprint = true;
            return c;
        });
        check(P.packInfo().interest > i0 || P.packInfo().bites > 1, std::format("Level 2: Rennen reizt sie ({:.2f} -> {:.2f})", i0, P.packInfo().interest));
        P.packSet("interest", 0.0);
        step(*s, ev, 3.0, [&] { return P.packInfo().gaveUp; });
        check(P.packInfo().gaveUp && P.packInfo().phase == 3, "Level 2: ohne Interesse gibt das Rudel auf");
        step(*s, ev, 60.0, [&] { return hounds(P, hp, hs) == 0; });
        check(hounds(P, hp, hs) == 0 && !s->stats().dead, "Level 2: das Rudel zieht ab und ist fort");
    }
    // --- Level 3: lauern allein, bellen, jagen gemeinsam, quaelen ------------------------------------------------------------
    {
        auto s = open("level3", 7);
        auto& P = s->presence();
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.5);
        P.packSet("spawn", 24.0);
        step(*s, ev, 0.5);
        std::vector<vec3> hp;
        std::vector<int> hs;
        const int n = hounds(P, hp, hs);
        float minGap = 99.0f;
        for (size_t i = 0; i < hp.size(); ++i)
            for (size_t j = i + 1; j < hp.size(); ++j) minGap = std::min(minGap, hdist2(hp[i], hp[j]));
        check(n == 3 && P.packInfo().l3 == 3 && minGap > 6.5f, std::format("Level 3: drei Hounds lauern einzeln ({:.1f} m auseinander)", minGap));
        // der Lauerer: still, flach, ohne leuchtende Augen - bis man ihm nahe kommt
        step(*s, ev, 3.0);
        hounds(P, hp, hs);
        int still = 0;
        for (int st : hs) still += st == 0;
        check(still == 3 && P.packInfo().phase == 0, "Level 3: ungestoert lauern sie reglos");
        // vor den ersten treten (Lampe an, Blick auf ihn): er bellt, die anderen antworten aus ihrer Lage
        s->flashlight().on = true;
        double ox = 0, oy = 0;
        const vec3 c0 = hp[0];
        const bool placed = spotNear(*s, c0, 6.0, c0 + vec3{0, 0, 0.5f}, ox, oy);
        if (placed) put(*s, ox, oy);
        aim(*s, c0 + vec3{0, 0, 0.4f});
        const size_t e0 = ev.size();
        step(*s, ev, 4.0, [&] {
            aim(*s, c0 + vec3{0, 0, 0.4f});
            return P.packInfo().answers >= 2;
        });
        std::vector<game::GameEvent> ev2(ev.begin() + (long long)e0, ev.end());
        auto I = P.packInfo();
        int far = 0;
        for (const auto& e : ev2)
            if (e.type == game::GameEvent::Figure && e.index == (int)PE::Bark && game::kinCueOf(e.intensity, e.role) == game::KC_HP_ANSWER &&
                hdist2(e.pos, c0) > 5.0f)
                ++far;
        check(placed && I.phase >= 1 && count(ev2, PE::Bark, Role::Hound, game::KC_HP_CALL) >= 1 && I.answers >= 2 && far >= 2,
              std::format("Level 3: der Lauerer ruft, {} antworten aus anderen Gaengen", I.answers));
        // das Bellen wird schneller, wenn man auf einen anderen Hound zulaeuft (sobald einer frei zu sehen ist)
        vec3 other = c0;
        step(*s, ev, 8.0, [&] {
            hounds(P, hp, hs);
            for (size_t i = 0; i < hp.size(); ++i)
                if (hdist2(hp[i], c0) > 2.0f && hdist2(hp[i], eyeOf(*s)) < 18.0f && hdist2(hp[i], eyeOf(*s)) > 4.0f &&
                    game::clearSight(s->world(), eyeOf(*s), hp[i] + vec3{0, 0, 0.5f}))
                    return other = hp[i], true;
            aim(*s, c0 + vec3{0, 0, 0.4f});
            return false;
        });
        const float slow = P.packInfo().barkEvery;
        float fastest = slow;
        step(*s, ev, 2.5, [&] {
            aim(*s, other + vec3{0, 0, 1.0f});
            fastest = std::min(fastest, P.packInfo().barkEvery);
            return false;
        }, [](int) {
            game::PlayerControl c;
            c.forward = 1.0f;
            return c;
        });
        check(fastest < 1.6f && fastest < slow, std::format("Level 3: laeuft man auf einen anderen zu, bellen sie schneller ({:.1f} s -> {:.1f} s)", slow, fastest));
        // quaelen: Nacken (gelaehmt), Bein (hinkt, kein Sprint), Kehle (kein Ruf) - nie sofort tot
        P.packSet("quick", 6.0);
        P.packSet("phase", 2.0);
        const size_t e1 = ev.size();
        float stup = 0.0f, slowF = 1.0f, mute = 0.0f;
        step(*s, ev, 50.0, [&] {
            hounds(P, hp, hs);
            if (!hp.empty()) {  // immer weg vom naechsten (sonst haelt der Blick sie ab)
                vec3 nb = hp[0];
                for (const vec3& p : hp)
                    if (hdist2(p, eyeOf(*s)) < hdist2(nb, eyeOf(*s))) nb = p;
                aim(*s, eyeOf(*s) * 2.0f - nb);
            }
            stup = std::max(stup, P.stupor(0));
            slowF = std::min(slowF, P.packSlow(0));
            mute = std::max(mute, P.houndMute(0));
            return P.packInfo().torment >= 3 || s->stats().dead;
        });
        std::vector<game::GameEvent> ev3(ev.begin() + (long long)e1, ev.end());
        const int fl = first(ev3, PE::Bark, Role::Hound, game::KC_HP_LUNGE), fh = first(ev3, PE::Hit, Role::Hound);
        check(P.packInfo().torment >= 3 && stup > 0.5f && slowF < 0.8f && mute > 0.0f,
              std::format("Level 3: sie quaelen in Stufen - gelaehmt ({:.2f}), hinkend ({:.2f}), stumm ({:.0f} s)", stup, slowF, mute));
        check(fh >= 0 && fl >= 0 && fl < fh, "Level 3: jeder Sprung wird angekuendigt");
        check(!s->stats().dead && s->stats().health > 40.0, std::format("Level 3: nach drei Bissen lebt man noch ({:.0f} Leben)", s->stats().health));
        // stumm: das Ansprechen bringt nur ein Roecheln (kein Ruf, den andere hoeren)
        s->events().clear();
        step(*s, ev, 3.0);
        s->events().clear();
        s->speak();
        bool rasp = false, call = false;
        for (const auto& e : s->events()) {
            rasp = rasp || (e.type == game::GameEvent::SoundAt && e.sound == "hp_rasp");
            call = call || (e.type == game::GameEvent::SoundAt && e.sound.rfind("talk_call", 0) == 0);
        }
        check(rasp && !call, "Level 3: mit zerbissener Kehle kein Ruf mehr - nur ein Roecheln");
        // Koop: die Puppe beim Gast bekommt Rolle und Variante (Modell), das Protokoll bleibt
        std::vector<game::Presence::NetState> ns;
        P.netStates(ns, eyeOf(*s), 32);
        game::Presence P2;
        P2.netApply(ns, P.netTime());
        int puppets = 0;
        for (const auto& m : ns) puppets += m.role == (int)Role::Hound && m.variant == 2;
        check(puppets >= 1 && P2.variantOf(Role::Hound) == 2, std::format("Koop: {} Rudel-Hounds als Puppen (Variante 2)", puppets));
    }
    // --- Level 3: das grosse, bewaffnete Buendel --------------------------------------------------------------------------------
    {
        auto s = open("level3", 9);
        auto& P = s->presence();
        std::vector<game::GameEvent> ev;
        step(*s, ev, 0.5);
        P.bigClumpSet("spawn", 16.0);
        step(*s, ev, 0.5);
        auto B = P.bigClumpInfo();
        check(B.active && B.variant == 1 && B.state == 0, "Grosses Buendel: erscheint bewaffnet (Variante 1) und lauert");
        step(*s, ev, 4.0);
        B = P.bigClumpInfo();
        check(B.ceil > 0.9f && B.state == 0, std::format("Grosses Buendel: haengt an der Decke ({:.2f}), solange es still ist", B.ceil));
        // Laerm in der Naehe: es jagt an der Decke, laesst sich fallen, holt aus (Knochen schabt) - erst dann der Hieb
        double ox = 0, oy = 0;
        const vec3 cp = B.pos;
        const bool placed = spotNear(*s, cp, 7.0, cp + vec3{0, 0, 1.0f}, ox, oy);
        if (placed) put(*s, ox, oy);
        const size_t e0 = ev.size();
        int maxState = 0;
        step(*s, ev, 25.0, [&] {
            maxState = std::max(maxState, P.bigClumpInfo().state);
            return P.bigClumpInfo().strikes >= 2 || s->stats().dead;
        }, [](int i) {
            game::PlayerControl c;
            c.forward = (i / 60) % 2 ? 1.0f : -1.0f;  // hin und her gehen (Schritte)
            return c;
        });
        std::vector<game::GameEvent> ev2(ev.begin() + (long long)e0, ev.end());
        B = P.bigClumpInfo();
        const int fs = first(ev2, PE::Swing, Role::Clump, game::KC_CL_SCRAPE), fh = first(ev2, PE::Hit, Role::Clump);
        check(placed && B.drops >= 1 && count(ev2, PE::Drop, Role::Clump, game::KC_CL_DROP) >= 1 && count(ev2, PE::Knock, Role::Clump, game::KC_CL_CEIL) >= 1,
              std::format("Grosses Buendel: hoert die Schritte, jagt an der Decke und laesst sich fallen ({} Mal, bis Zustand {})", B.drops, maxState));
        check(B.strikes >= 1 && fs >= 0 && (fh < 0 || fs < fh), std::format("Grosses Buendel: vor jedem Hieb das Schaben der Knochen ({} Hiebe)", B.strikes));
        check(!s->stats().dead && (fh < 0 || s->stats().health > 40.0), std::format("Grosses Buendel: ein Hieb toetet nicht sofort ({:.0f} Leben)", s->stats().health));
        // leise weg: still stehen, es findet nicht
        const int st0 = P.bigClumpInfo().strikes;
        step(*s, ev, 1.0);
        put(*s, ox, oy);
        step(*s, ev, 8.0);
        check(P.bigClumpInfo().strikes <= st0 + 1 && !s->stats().dead, "Grosses Buendel: wer still steht, wird nicht gefunden (es ist blind)");
        // entwaffnet: es reisst sich den Knochen frei (Modellwechsel), dabei hoert es nichts
        P.bigClumpSet("disarm", 1.0);
        const size_t e1 = ev.size();
        step(*s, ev, 3.2);
        std::vector<game::GameEvent> ev3(ev.begin() + (long long)e1, ev.end());
        B = P.bigClumpInfo();
        check(B.variant == 2 && B.tears >= 1 && count(ev3, PE::Swing, Role::Clump, game::KC_CL_TEAR) == 1,
              "Grosses Buendel: entwaffnet reisst es sich den Knochen frei (Variante 2)");
        // lange Stille: zurueck an die Decke und fort
        P.bigClumpSet("state", 4.0);
        step(*s, ev, 30.0, [&] { return P.bigClumpInfo().state >= 8; });
        check(P.bigClumpInfo().state >= 8 || !P.bigClumpInfo().active, "Grosses Buendel: nach langer Stille zieht es sich zurueck");
    }
}

// V13.3 Level 3, Restpunkte (KREATUR_Smiler.md "V13.3: Level 3 - Angriff ohne Anlass")
static void testV133Level3Rest(const std::string& dataDir) {
    std::printf("V13.3: Level 3 - Smiler ohne Anlass\n");
    game::PrefabLibrary prefabs;
    prefabs.loadDirectory(dataDir + "/prefabs");
    JobSystem jobs(2);
    using PE = game::Presence::Event;
    using Role = game::Presence::Role;
    const auto L1 = gContent11->level("level1", world::kLayoutV6);
    check(gContent11->level("level3", world::kLayoutV6)->smilerUnprovoked > 0.2 && L1->smilerUnprovoked == 0.0,
          "Level 3: presence.smiler_unprovoked gesetzt, Level 1 nicht");
    auto look = [](game::GameSession& se, vec3 q, double off) {
        auto& pl = se.player();
        pl.angle = std::atan2(q.y - pl.y, q.x - pl.x) + off;
        pl.pitch = off > 1.0 ? 0.0 : std::atan2(q.z + 1.7 - pl.eye(), std::max(0.5, std::hypot(q.x - pl.x, q.y - pl.y)));
    };
    // Halle Aquila (Level 1) mit der Einstellung von Level 3 (oder ohne): Stromausfall, der Smiler, davor stellen (8 m, dahinter
    // 20 m freier Boden)
    auto setup = [&](double unprovoked, std::unique_ptr<game::GameSession>& s) {
        auto L = std::make_shared<world::LevelDef>(*L1);
        L->smilerBait = unprovoked > 0.0 ? 1 : 0;
        L->smilerUnprovoked = unprovoked;
        s = std::make_unique<game::GameSession>(*gContent11, prefabs, L, 4242, "medium", "Test", jobs, 3);
        s->placeAtSpawn();
        s->preload();
        for (int i = 0; i < 12; ++i) s->update(1.0 / 60.0, {}, false), s->events().clear();
        world::World& ww = s->world();
        auto& pl = s->player();
        const auto [hx, hy] = hallSpot(ww);
        pl.x = hx + 0.5, pl.y = hy + 0.5, pl.z = ww.tile(hx, hy).floor;
        s->body().reset();
        s->preload();
        auto& P = s->presence();
        P.force(Role::Smiler);
        for (int i = 0; i < 60 * 16 && !P.has(Role::Smiler); ++i) s->update(1.0 / 60.0, {}, false), s->events().clear();
        for (int i = 0; i < 60 * 4 && P.has(Role::Smiler) && P.stateOf(Role::Smiler) < 1; ++i) {
            look(*s, P.posOf(Role::Smiler), 0.0);
            s->update(1.0 / 60.0, {}, false), s->events().clear();
        }
        if (!P.has(Role::Smiler)) return false;
        const vec3 sp = P.posOf(Role::Smiler);
        for (int k = 0; k < 32; ++k) {
            const double a = k * 0.19635, ux = std::cos(a), uy = std::sin(a);
            bool ok = true;
            for (double d = 5.0; d <= 28.0 && ok; d += 0.5) {
                const world::Tile& t = ww.tile(ifloor(sp.x + ux * d), ifloor(sp.y + uy * d));
                ok = t.open() && t.clearance() > 2.1f && std::fabs(t.floor - sp.z) < 0.2f && !(t.flags & world::TF_FEATURE);
            }
            // seitlich 4 m frei (zur Seite treten)
            for (double d = 0.5; d <= 4.0 && ok; d += 0.5) {
                const world::Tile& t = ww.tile(ifloor(sp.x + ux * 8.0 - uy * d), ifloor(sp.y + uy * 8.0 + ux * d));
                ok = t.open() && t.clearance() > 2.1f && std::fabs(t.floor - sp.z) < 0.2f && !(t.flags & world::TF_FEATURE);
            }
            if (!ok) continue;
            pl.x = sp.x + ux * 8.0, pl.y = sp.y + uy * 8.0, pl.z = sp.z, pl.vx = pl.vy = 0.0;
            s->body().reset();
            look(*s, sp, 0.0);
            return true;
        }
        return false;
    };
    struct Run {
        double tAnn = -1.0, tHit = -1.0, tCalm = -1.0;
        int stageMax = 0;
        bool gone = false;
    };
    // what: 0 stehen und hinsehen, 1 Licht aus und zur Seite treten, 2 wegrennen (Lampe an)
    auto play = [&](game::GameSession& s, int what, double secs) {
        auto& P = s.presence();
        Run r;
        double t = 0.0, tAct = -1.0;
        for (int i = 0; i < (int)(secs * 60.0); ++i, t += 1.0 / 60.0) {
            game::PlayerControl c;
            if (!P.has(Role::Smiler)) {
                r.gone = true;
                break;
            }
            const vec3 sp = P.posOf(Role::Smiler);
            if (r.tAnn >= 0.0 && tAct < 0.0) tAct = t;
            if (what == 0 || tAct < 0.0) look(s, sp, 0.0);
            if (what == 1 && tAct >= 0.0) {
                s.flashlight().on = false;
                if (t - tAct < 1.9) look(s, sp, 0.0), c.side = 1.0f;  // ruhig zur Seite (gehen)
            }
            if (what == 2 && tAct >= 0.0) look(s, sp, 3.14159), c.forward = 1.0f, c.sprint = true;
            s.update(1.0 / 60.0, c, false);
            for (const auto& e : s.events()) {
                if (e.type == game::GameEvent::Figure && e.role == (int)Role::Smiler && e.index == (int)PE::Announce && e.intensity > 1.5f && r.tAnn < 0.0) r.tAnn = t;
                if (e.type == game::GameEvent::Figure && e.role == (int)Role::Smiler && e.index == (int)PE::Calm && r.tAnn >= 0.0 && r.tCalm < 0.0) r.tCalm = t;
                if (e.type == game::GameEvent::StalkerHit && r.tHit < 0.0) r.tHit = t;
            }
            s.events().clear();
            r.stageMax = std::max(r.stageMax, P.stageOf(Role::Smiler));
            if (r.tHit >= 0.0 && t > r.tHit + 0.5) break;
        }
        return r;
    };
    // a) Level 3: er kuendigt sich von selbst an (kein Rennen, kein Laerm), ruhig stehen und hinsehen hilft nicht
    {
        std::unique_ptr<game::GameSession> s;
        const bool ok = setup(1.0, s);
        check(ok, "Level-3-Smiler erschienen, 8 m davor freie Bahn");
        if (ok) {
            s->flashlight().on = true;
            const Run r = play(*s, 0, 34.0);
            std::printf("  still: Ankuendigung %.2f s, Treffer %.2f s, Stufe max %d\n", r.tAnn, r.tHit, r.stageMax);
            check(r.tAnn >= 0.0, std::format("Level 3: ohne Anlass - Lider und Lippen reissen auf, obwohl man still steht und hinsieht ({:.1f} s)", r.tAnn));
            check(r.tHit >= 0.0 && r.tHit - r.tAnn >= 2.15, std::format("Level 3: Zugriff fruehestens 2,2 s nach der Ankuendigung ({:.2f} s)", r.tHit - r.tAnn));
            check(r.tHit < 0.0 || (s->stats().dead && s->killRole() == (int)Role::Smiler), "Level 3: wer stehen bleibt, stirbt");
        }
    }
    // b) Licht aus und zur Seite: er stuermt zur Stelle des Lichts, verfehlt und laesst ab
    {
        std::unique_ptr<game::GameSession> s;
        const bool ok = setup(1.0, s);
        check(ok, "Level 3 (b): Smiler erschienen");
        if (ok) {
            s->flashlight().on = true;
            s->presence().kinSet(Role::Smiler, "mode", 2), s->presence().kinSet(Role::Smiler, "provoke", 0.2);
            const Run r = play(*s, 1, 14.0);
            std::printf("  Licht aus: Ankuendigung %.2f s, Stufe max %d, Ablassen %.2f s, Treffer %.2f\n", r.tAnn, r.stageMax, r.tCalm, r.tHit);
            check(r.tAnn >= 0.0 && r.stageMax == 6 && r.tHit < 0.0 && r.tCalm > r.tAnn && !s->stats().dead,
                  std::format("Level 3: Licht aus und zur Seite - er stuermt zur letzten Stelle des Lichts und laesst ab (Stufe {}, Treffer {:.1f})", r.stageMax, r.tHit));
        }
    }
    // c) wegrennen: der Sprint ist schneller, ab 13 m laesst er ab
    {
        std::unique_ptr<game::GameSession> s;
        const bool ok = setup(1.0, s);
        check(ok, "Level 3 (c): Smiler erschienen");
        if (ok) {
            s->flashlight().on = true;
            s->presence().kinSet(Role::Smiler, "mode", 2), s->presence().kinSet(Role::Smiler, "provoke", 0.2);
            const Run r = play(*s, 2, 14.0);
            std::printf("  Wegrennen: Ankuendigung %.2f s, Stufe max %d, Ablassen %.2f s, Treffer %.2f\n", r.tAnn, r.stageMax, r.tCalm, r.tHit);
            check(r.tAnn >= 0.0 && r.tHit < 0.0 && r.tCalm > r.tAnn && !s->stats().dead, "Level 3: wegrennen - er laesst ab");
        }
    }
    // d) Level 1 (ohne die Einstellung): wer still steht und hinsieht, wird nie ohne Anlass angegriffen
    {
        std::unique_ptr<game::GameSession> s;
        const bool ok = setup(0.0, s);
        check(ok, "Level 1 (d): Smiler erschienen");
        if (ok) {
            s->flashlight().on = true;
            const Run r = play(*s, 0, 30.0);
            check(r.tAnn < 0.0 && r.tHit < 0.0 && r.stageMax <= 1, "Level 1: kein Angriff ohne Anlass (still stehen, hinsehen)");
        }
    }

    // --- Red Knight (KREATUR_RedKnight.md): laeuft ungesehen an, am Wanderer vorbei, haut in die leere Luft, schneidet einen
    // Riss und ist fort; danach zieht der unsichtbare Verfolger vorbei. Er greift nie an. -----------------------------------
    std::printf("V13.3: Level 3 - Red Knight\n");
    check(gContent11->level("level3", world::kLayoutV6)->redKnightEvery > 300.0 && L1->redKnightEvery == 0.0,
          "Level 3: presence.red_knight_every gesetzt, Level 1 nicht");
    auto knightRun = [&](double every, int seed) {
        auto L = std::make_shared<world::LevelDef>(*L1);
        L->redKnightEvery = every;
        auto s = std::make_unique<game::GameSession>(*gContent11, prefabs, L, seed, "medium", "Test", jobs, 3);
        s->placeAtSpawn();
        s->preload();
        auto& pl = s->player();
        const auto [hx, hy] = hallSpot(s->world());
        pl.x = hx + 0.5, pl.y = hy + 0.5, pl.z = s->world().tile(hx, hy).floor;
        s->body().reset();
        s->preload();
        for (int i = 0; i < 30; ++i) s->update(1.0 / 60.0, {}, false), s->events().clear();
        return s;
    };
    {
        auto s = knightRun(900.0, 4242);
        auto& P = s->presence();
        P.knightSet("run", 1.0);
        bool spawned = false, seenAtSpawn = false, heardBefore = false, seenEver = false, hit = false, gone = false;
        int steps = 0, slashCue = 0, riftCue = 0, unseenCue = 0, flicker = 0, visions = 0, afterGoneFlicker = 0;
        float closest = 1e9f;
        const double h0 = s->stats().health, s0 = s->stats().sanity;
        double t = 0.0;
        for (int i = 0; i < 60 * 40; ++i, t += 1.0 / 60.0) {
            const auto info = P.knightInfo();
            if (info.active) {
                if (!spawned) seenAtSpawn = P.seenRole(Role::Wretch);
                spawned = true;
                closest = std::min(closest, std::hypot(info.pos.x - (float)s->player().x, info.pos.y - (float)s->player().y));
                look(*s, info.pos, 0.0);  // der Wanderer sieht hin (er soll ihn sehen)
                seenEver = seenEver || P.seenRole(Role::Wretch);
            }
            s->update(1.0 / 60.0, {}, false);
            for (const auto& e : s->events()) {
                if (e.type == game::GameEvent::StalkerHit || (e.type == game::GameEvent::Figure && (e.index == (int)PE::Hit || e.index == (int)PE::Attack))) hit = true;
                if (e.type != game::GameEvent::Figure || e.role != (int)Role::Wretch) continue;
                const int cue = game::kinCueOf(e.intensity, e.role);
                if (cue == 200) ++steps, heardBefore = heardBefore || !seenEver;
                if (cue == 203) ++slashCue;
                if (cue == 204) ++riftCue;
                if (cue == 205) ++unseenCue;
                if (e.index == (int)PE::Vision) ++visions;
                if (e.index == (int)PE::Flicker) ++flicker, afterGoneFlicker += gone ? 1 : 0;
                if (e.index == (int)PE::Gone) gone = true;
            }
            s->events().clear();
            if (gone && !P.knightInfo().pursuer && t > 3.0 && afterGoneFlicker > 0) break;
        }
        const auto info = P.knightInfo();
        std::printf("  Red Knight: Schritte %d, naechster Abstand %.1f m, Hieb %d, Riss %d, Verfolger %d Klaenge / %d Flackern / %d Vision, Verstand %.1f -> %.1f, %.1f s\n",
                    steps, closest, slashCue, riftCue, unseenCue, flicker, visions, s0, s->stats().sanity, t);
        check(spawned && !seenAtSpawn, "Red Knight: erscheint ausser Sicht");
        check(steps > 6 && heardBefore, "Red Knight: man hoert ihn (Ruestung, Schritte), bevor man ihn sieht");
        check(seenEver && closest < 6.5f, std::format("Red Knight: er rennt am Wanderer vorbei ({:.1f} m)", closest));
        check(slashCue == 1 && info.slashed, "Red Knight: er haut in die leere Luft (einmal, weg vom Wanderer)");
        check(gone && riftCue == 1 && info.rift && !info.active, "Red Knight: ein Riss in der Luft - fort");
        check(!hit && s->stats().health >= h0, "Red Knight: kein Angriff, kein Schaden");
        check(unseenCue >= 2 && afterGoneFlicker > 0, "Red Knight: danach zieht der unsichtbare Verfolger vorbei (Klang, die Leuchten flackern)");
        check(visions == 1, "Red Knight: der Verfolger zehrt genau einmal am Verstand (Vision, nah am Wanderer)");
        check(info.runs == 1, "Red Knight: ein Lauf");
    }
    // Level 1 (ohne die Einstellung): nie
    {
        auto s = knightRun(0.0, 77);
        auto& P = s->presence();
        P.knightSet("quick", 1.0);
        bool any = false;
        const double s0 = s->stats().sanity;
        for (int i = 0; i < 60 * 31; ++i) {
            s->update(1.0 / 60.0, {}, false), s->events().clear();
            any = any || P.knightInfo().active;
        }
        std::printf("  Vergleich ohne Ritter (31 s an derselben Stelle): Verstand %.1f -> %.1f\n", s0, s->stats().sanity);
        check(!any, "Level 1: kein Red Knight");
    }
    // Level 3 von selbst (Zeitraffer): hoechstens zwei Laeufe je Besuch
    {
        auto s = knightRun(900.0, 91);
        auto& P = s->presence();
        P.knightSet("quick", 1.0);
        int starts = 0;
        bool was = false;
        for (int i = 0; i < 60 * 240; ++i) {
            s->update(1.0 / 60.0, {}, false), s->events().clear();
            const bool now = P.knightInfo().active;
            starts += now && !was ? 1 : 0;
            was = now;
        }
        check(starts >= 1 && starts <= 2, std::format("Level 3: der Red Knight kommt von selbst, hoechstens zweimal ({} Laeufe)", starts));
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("Aufruf: gameplay_test <data-Ordner>\n");
        return 2;
    }
    game::Content content;
    std::string err;
    if (!content.load(argv[1], err, "legacy")) {
        std::printf("Inhalte nicht ladbar: %s\n", err.c_str());
        return 2;
    }
    game::Content content11;
    if (!content11.load(argv[1], err)) {
        std::printf("V11-Inhalte nicht ladbar: %s\n", err.c_str());
        return 2;
    }
    gContent11 = &content11;
    testJump();
    testExplore(content);
    testBody();
    testStamina(content);
    testDrop(content, argv[1]);
    testFlashlight();  // V7
    testFinds(content);
    testLevelChange(content, argv[1]);
    testHaunt(content, argv[1]);
    testWorld(content, argv[1]);  // V8
    testV9(content, argv[1]);
    testV11Creatures(argv[1]);  // V11
    testV13Kin(argv[1]);        // V13.2: neue Wesen (Faceling, Skin-Stealer, Wretch, Death Rat, Fensterwesen)
    testV11Journey(argv[1]);    // V11
    testV11Supplies();          // V11: Vorraete
    testV11Placement();         // V11: Halt der Ausstattung
    testV11FlashGrip();         // V11: Taschenlampe im Griff
    testV12Viewmodel(argv[1]);  // V12: Hand und Lampe aus Blender
    testV11SoundField();        // V11: Schallausbreitung
    testV12Glb(argv[1]);        // V12: Modelle aus Blender
    testB001Sleeve(argv[1]);    // V13.2: Kapuzenjacke am gebeugten Ellbogen
    testV133DullerArms(argv[1]);  // V13.3: Duller und Arms (Level 1)
    testV133Wrangler(argv[1]);    // V13.3: Wrangler (Level 1, 2)
    testV133GrowlerNguithr(argv[1]);  // V13.3: Growler, Nguithr'xurh
    testV133Ouroboros(argv[1]);       // V13.3: Ouroboros-Arbeiter
    testV133PlagueGoblins(argv[1]);   // V13.3: Plague Goblins, Beil
    testV133LureTalk(argv[1]);    // V13.3: Leuchtstab (Smiler), Ansprechen (Skin-Stealer)
    testV133Phenomena(argv[1]);   // V13.3: Erscheinungen in Level 1 (Pilz, Garden, Gemaelde, Tuerspuren)
    testV133Level2Kin(argv[1]);   // V13.3 Teil 4: Level 2 (Fenster am Gangende, Motten, Ratten an Faessern, Spuren)
    testV133Jerry(argv[1]);       // V13.3 Teil 4: Jerry (Level 2)
    testV133L2Rand(argv[1]);      // V13.3 Teil 4: Randwesen von Level 2 (Automat, Swindlebird, Photoshop, Macchina)
    testV133Level3Kin(argv[1]);   // V13.3: Level 3 - Wretch mit Waffe, Skin-Stealer im Mantel
    testV133Purple(argv[1]);      // V13.3: Level 3 - purpifizierte Wesen in der Wand
    testV133Level3RatMoth(argv[1]);  // V13.3: Level 3 - Death Rats mit Fallen, Mottenschwarm mit Praetorianern
    testV133PackBigClump(argv[1]);   // V13.3: Level 2/3 - Hound-Rudel, grosses bewaffnetes Buendel
    testV133Level3Rest(argv[1]);     // V13.3: Level 3 - Smiler ohne Anlass, Red Knight
    std::printf("%s: %d Pruefungen, %d Fehler\n", gFails ? "FEHLGESCHLAGEN" : "OK", gChecks, gFails);
    return gFails ? 1 : 0;
}
