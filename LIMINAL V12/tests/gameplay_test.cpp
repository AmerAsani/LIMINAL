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
#include "gameplay/components.hpp"
#include "gameplay/content.hpp"
#include "gameplay/explore.hpp"
#include "gameplay/finds.hpp"
#include "gameplay/flashlight.hpp"
#include "gameplay/player.hpp"
#include "gameplay/prefabs.hpp"
#include "gameplay/session.hpp"
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
            pl.x = 38.5, pl.y = 26.5, pl.z = ww.tile(38, 26).floor;
            s->body().reset();
        }
        s->preload();
        run(*s, 0.5);
        const world::Tile& deckT = ww.tile(38, 26);
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
            pl.x = 38.5, pl.y = 26.5, pl.z = ww.tile(38, 26).floor;
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
            pl.x = 38.5, pl.y = 26.5, pl.z = ww.tile(38, 26).floor;
            s->body().reset();
        }
        s->preload();
        run(*s, 0.5);
        const world::Tile& deckT = ww.tile(38, 26);
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
            pl.x = 38.5, pl.y = 26.5, pl.z = ww.tile(38, 26).floor;
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
            pl.x = 38.5, pl.y = 26.5, pl.z = ww.tile(38, 26).floor;
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
    static const char* kLevels[] = {"level0", "level1", "level2", "level3", "level4", "level5", "level6", "level7", "level19", "level37"};
    int exits = 0, ends = 0;
    for (const char* id : kLevels) {
        const auto L = content.level(id, world::kLayoutV6);
        check(L && L->map, std::string(id) + ": feste Karte geladen");
        if (!L || !L->map) continue;
        for (const world::MapTransition& t : L->map->transitions) {
            if (t.kind == "end") {
                ++ends;
                check(std::string(id) == "level37", "das Ende liegt in Level 37");
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

    // Wasser: Level 7 - im Meer schwimmen; unter Wasser geht die Luft aus
    game::PrefabLibrary prefabs;
    prefabs.loadDirectory(dataDir + "/prefabs");
    JobSystem jobs(2);
    auto s = std::make_unique<game::GameSession>(content, prefabs, content.level("level7", world::kLayoutV6), 4242, "medium", "Test", jobs, 3);
    s->placeAtSpawn();
    s->preload();
    for (int i = 0; i < 20; ++i) s->update(1.0 / 60.0, {}, false), s->events().clear();
    world::World& ww = s->world();
    // eine Kachel im Meer suchen (Raumtyp "meer"), weit weg von der Treppe
    i64 sx = 0, sy = 0;
    bool found = false;
    for (i64 y = 10; y < 200 && !found; y += 5)
        for (i64 x = 10; x < 200 && !found; x += 5) {
            const world::Tile& t = ww.tile(x, y);
            if (t.open() && t.room && t.room->type && t.room->type->key == "meer" && t.room->water > t.floor + 3.0) sx = x, sy = y, found = true;
        }
    check(found, "Level 7: tiefes Meer gefunden");
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
    static const char* kLevels[] = {"level0", "level1", "level2", "level3", "level4", "level5", "level6", "level7", "level19", "level37"};
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
            if (std::string(id) == "level5" || std::string(id) == "level7")
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
                                 "junction_box", "violet_lamp", "door_steel_white", "door_steel_grey", "door_steel_cream", "door_window", "door_plywood", "door_wood_old", "door_oversize", "door_level3"};  // V12 Level 2
    static const char* ceil[] = {"batten", "highbay", "chandelier", "safety_light", "ceiling_vent",
                                 "troffer", "troffer_off", "troffer_red", "ceiling_diffuser", "hanging_bulb",  // V12
                                 "tube_white", "tube_orange", "tube_dead"};
    static const char* floor[] = {"chair", "rack", "bed", "barrel", "cart", "pump", "wardrobe", "trunk", "water_cooler", "armchair", "sofa",
                                  "side_table", "game_table", "boiler", "statue", "transformer", "fence_panel", "rubble", "barrier", "camp",
                                  "desk_crt", "crate", "pallet", "switch_cabinet", "stall_line", "arrow", "floor_glow", "car",
                                  "crate_a", "crate_b", "crate_c", "debris_a", "debris_b", "hospital_bed", "work_table",
                                  "column_round",  // V12 Level 1
                                  "mach_cabinet", "mach_pump", "mach_tank", "pipe_riser", "pipe_bundle_a", "pipe_bundle_b", "trolley_flatbed", "toolbox", "stepladder", "cable_reel", "folding_chair", "hopper_wagon", "rail_track", "chamfer_26", "chamfer_32"};  // V12 Level 2
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
    static const char* kLevels[] = {"level0", "level1", "level2", "level3", "level4", "level5", "level6", "level7", "level19", "level37"};
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
                                o.model == "door_level3";  // V12: im Durchbruch
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
    check(b.vmSlide() >= -0.081f && b.vmSlide() <= 0.031f, std::format("Hand gleitet hoechstens 8 cm ({:.3f} m)", b.vmSlide()));
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
    testV11Journey(argv[1]);    // V11
    testV11Supplies();          // V11: Vorraete
    testV11Placement();         // V11: Halt der Ausstattung
    testV11FlashGrip();         // V11: Taschenlampe im Griff
    testV12Viewmodel(argv[1]);  // V12: Hand und Lampe aus Blender
    testV11SoundField();        // V11: Schallausbreitung
    testV12Glb(argv[1]);        // V12: Modelle aus Blender
    std::printf("%s: %d Pruefungen, %d Fehler\n", gFails ? "FEHLGESCHLAGEN" : "OK", gChecks, gFails);
    return gFails ? 1 : 0;
}
