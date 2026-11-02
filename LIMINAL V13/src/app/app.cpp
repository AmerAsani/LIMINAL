#include "app/app.hpp"

#include <chrono>
#include <cmath>
#include <cstring>
#include <ctime>
#include <format>
#include <random>
#include <set>
#include <thread>

#include "core/log.hpp"
#include "gameplay/kin_l3.hpp"  // V13.3 Level 3: Saetze und Waffenklaenge (Wretch, Skin-Stealer)
#include "platform/image_io.hpp"
#include "render/world_renderer.hpp"
#include "world/room.hpp"

#include <windows.h>
#include <psapi.h>  // V13: Arbeitsspeicher im Befehl perf (K32GetProcessMemoryInfo, kernel32)

namespace lim {

using input::Action;
using Clock = std::chrono::steady_clock;

App::App() = default;

App::~App() {
    // Reihenfolge: Koop (verweist auf die Sitzung, V13.2), Sitzung (wartet auf Weltjobs) vor Renderer und Jobs
    leaveCoop("");
    session_.reset();
    audio_.reset();
    renderer_.reset();
    jobs_.reset();
}

// --- Kommandozeile ---------------------------------------------------------------------------
bool App::parseArgs(const std::vector<std::string>& a) {
    for (size_t i = 0; i < a.size(); ++i) {
        const std::string& k = a[i];
        auto next = [&]() -> std::string { return i + 1 < a.size() ? a[++i] : std::string(); };
        if (k == "--seed") opt_.seed = std::atoll(next().c_str());
        else if (k == "--name") opt_.name = next();
        else if (k == "--difficulty") opt_.difficulty = next();
        else if (k == "--level") opt_.level = next();
        else if (k == "--level-file") opt_.levelFile = next();  // V13.3
        else if (k == "--demo") opt_.demo = true;
        else if (k == "--benchmark") opt_.benchmark = std::atoi(next().c_str());
        else if (k == "--shot") opt_.shot = next();
        else if (k == "--shot-frames") opt_.shotFrames = std::atoi(next().c_str());
        else if (k == "--size") {
            std::string v = next();
            size_t x = v.find('x');
            if (x != std::string::npos) opt_.width = std::atoi(v.c_str()), opt_.height = std::atoi(v.c_str() + x + 1);
        } else if (k == "--warp") opt_.warp = true;
        else if (k == "--gpu") gfx::Gpu::preferLowPower = next() == "sparsam";  // V6: --gpu sparsam | leistung
        else if (k == "--windowed") opt_.windowed = true;
        else if (k == "--debug") opt_.debug = true;
        else if (k == "--no-mouse") opt_.noMouse = true;
        else if (k == "--fast") opt_.fast = true;
        else if (k == "--fixed-dt") opt_.fixedDt = true;
        else if (k == "--quality") opt_.quality = std::atoi(next().c_str());
        else if (k == "--no-hud") opt_.noHud = true;
        else if (k == "--mode") {
            std::string v = next();
            opt_.mode = v == "terminal" || v == "hires" ? 1 : v == "ascii" ? 2 : v == "mono" ? 3 : 0;
        } else if (k == "--quit-after") opt_.quitAfter = std::atof(next().c_str());
        else if (k == "--yaw") opt_.yaw = std::atof(next().c_str());
        else if (k == "--pitch") opt_.pitch = std::atof(next().c_str());
        else if (k == "--pos") {
            std::string v = next();
            size_t c = v.find(',');
            if (c != std::string::npos) opt_.posX = std::atof(v.c_str()), opt_.posY = std::atof(v.c_str() + c + 1);
        } else if (k == "--save-file") opt_.saveFile = next();
        else if (k == "--dump-skeletons") opt_.dumpSkeletons = next();
        else if (k == "--room") opt_.room = next();
        else if (k == "--shot-ui") opt_.shotUi = next();
        else if (k == "--set") {
            std::string v = next();
            size_t eq = v.find('=');
            if (eq != std::string::npos) {
                std::string key = v.substr(0, eq), val = v.substr(eq + 1);
                if (auto j = Json::parse(val)) opt_.overrides.set(key, *j);
                else opt_.overrides.set(key, val);
            }
        }
        else if (k == "--script") opt_.script = next();
        else if (k == "--new") { /* wie V4: ohne Wirkung, das Hauptmenue bietet "Neues Spiel" */ }
    }
    return true;
}

// --- Start ---------------------------------------------------------------------------------------
// V13: Ruhepose und Knochenrahmen aller Wesen (Modellraum des Spiels: x vorn, y Seite 0, z oben) fuer
// tools/blender/creatures - die Modelle aus Blender werden um genau diese Knochen gebaut
static bool dumpSkeletons(const std::string& path) {
    Json root = Json::object();
    for (int e = 0; e < anim::kEntityCount; ++e) {
        const anim::CreatureSkeleton& sk = anim::entitySkeleton((anim::Entity)e);
        std::vector<mat4> frames;
        anim::creatureFrames(sk, anim::bindPose(sk), frames);
        Json ent = Json::object();
        ent.set("name", anim::entityName((anim::Entity)e));
        ent.set("height", (double)sk.height);
        ent.set("reach", (double)sk.reach);
        Json joints = Json::array();
        for (const vec3& j : sk.bind) {
            Json a = Json::array();
            a.push((double)j.x), a.push((double)j.y), a.push((double)j.z);
            joints.push(a);
        }
        ent.set("joints", joints);
        Json bones = Json::array();
        for (size_t b = 0; b < sk.bones.size(); ++b) {
            const anim::CBone& cb = sk.bones[b];
            Json o = Json::object();
            o.set("a", (double)cb.a), o.set("b", (double)cb.b), o.set("parent", (double)cb.parent);
            o.set("radius", (double)cb.radius), o.set("shadow", cb.shadow);
            Json f = Json::array();  // Rahmen spaltenweise (x, y, z, Ursprung)
            for (int c = 0; c < 4; ++c)
                for (int r = 0; r < 3; ++r) f.push((double)frames[b].at(r, c));
            o.set("frame", f);
            bones.push(o);
        }
        ent.set("bones", bones);
        root.set(anim::entityModel((anim::Entity)e), ent);
    }
    return paths::writeFileAtomic(path, root.dump(true), false);
}

int App::run(const std::vector<std::string>& args) {
    parseArgs(args);
    if (!opt_.dumpSkeletons.empty()) return dumpSkeletons(opt_.dumpSkeletons) ? 0 : 1;
    // eigenes Protokoll: V5 und V6 koennen nebeneinander installiert sein
    log::init(paths::join(paths::join(paths::localDataDir(), "logs"), "liminal_v13.log"));
    log::info("LIMINAL {} startet ({})", LIM_VERSION, paths::exeDir());
    std::string err;
    if (!init(err)) {
        log::error("Start fehlgeschlagen: {}", err);
        if (opt_.shot.empty() && opt_.benchmark == 0) MessageBoxW(nullptr, paths::widen(err + "\n\nDetails: " + log::filePath()).c_str(), L"LIMINAL",
                    MB_OK | MB_ICONERROR);
        return 1;
    }
    if (opt_.benchmark > 0) runBenchmark();
    else mainLoop();
    if (state_ == State::Playing && session_ && !session_->stats().dead && opt_.benchmark == 0 && opt_.shot.empty())
        saveGame();
    if (!testRun() && opt_.overrides.size() == 0) settings_.save();
    if (!opt_.script.empty()) log::info("Skript: {}", scriptFailed_ ? "FEHLGESCHLAGEN" : "OK");
    if (audio_) log::info("Audio beim Beenden: {}", audio_->statsLine());  // V13.3 (AUDIO_BUG.md)
    log::info("Beendet");
    log::shutdown();
    return scriptFailed_ ? 7 : 0;
}

// --- Test-Skript (--script) ------------------------------------------------------------------------
void App::scriptStep() {
    for (int k : scriptRelease_) input_.onKey(k, false);
    scriptRelease_.clear();
    // V11: hold - im Skriptbetrieb wird der Tastenzustand jedes Bild geloescht, also neu druecken
    for (auto it = scriptHold_.begin(); it != scriptHold_.end();) {
        if (--it->second <= 0) {
            it = scriptHold_.erase(it);
        } else {
            input_.onKey(it->first, true);
            ++it;
        }
    }
    if (state_ == State::Loading) return;
    if (scriptWait_ > 0) {
        --scriptWait_;
        return;
    }
    static const char* modes[] = {"Loading", "Title", "Hud", "Pause", "Inventory", "Dead", "Note", "Ending"};
    while (scriptPos_ < script_.size()) {
        std::string line = script_[scriptPos_++];
        while (!line.empty() && line.front() == ' ') line.erase(line.begin());
        while (!line.empty() && line.back() == ' ') line.pop_back();
        if (line.empty()) continue;
        size_t sp = line.find(' ');
        std::string cmd = line.substr(0, sp), arg = sp == std::string::npos ? "" : line.substr(sp + 1);
        if (cmd == "wait") {
            scriptWait_ = std::max(0, std::atoi(arg.c_str()) - 1);
            return;
        } else if (cmd == "key") {
            int code = input::keyFromName(arg);
            if (!code) {
                log::error("Skript: unbekannte Taste '{}'", arg);
                scriptFailed_ = true;
                continue;
            }
            input_.onKey(code, true);
            scriptRelease_.push_back(code);
            return;  // eine Taste je Bild
        } else if (cmd == "hold") {  // V11 (Tests): Taste halten: hold <Taste> <Bilder> (laeuft neben wait weiter)
            char kn[32] = {};
            int frames = 30;
            std::sscanf(arg.c_str(), "%31s %d", kn, &frames);
            int code = input::keyFromName(kn);
            if (!code) {
                log::error("Skript: unbekannte Taste '{}'", kn);
                scriptFailed_ = true;
                continue;
            }
            input_.onKey(code, true);
            scriptHold_.push_back({code, std::max(1, frames)});
        } else if (cmd == "text") {
            for (char32_t c : gfx::utf8To32(arg)) input_.onChar(c);
            return;
        } else if (cmd == "expect") {
            std::string want = arg, wantPage;
            if (auto sl = want.find('/'); sl != std::string::npos) wantPage = want.substr(sl + 1), want = want.substr(0, sl);
            std::string have = modes[(int)ui_.mode()];
            bool ok = have == want && (wantPage.empty() || ui_.page() == wantPage);
            if (!ok) {
                scriptFailed_ = true;
                log::error("Skript: erwartet {} , ist {}/{} (Befehl {})", arg, have, ui_.page(), scriptPos_);
            } else {
                log::info("Skript: {} ok", arg);
            }
        } else if (cmd == "shot") {
            scriptShot_ = arg;
            return;
        } else if (cmd == "haunt" && session_) {  // V7: Halluzination ausloesen (1 Schritte, 2 Ausfall, 3 mit Gestalt)
            session_->forceHaunt(std::atoi(arg.c_str()));
        } else if (cmd == "pos" && session_) {  // V8: Spieler versetzen (Tests): pos x y [blick] [neigung, V11]
            double x = 0, y = 0, yaw = 1e9, pitch = 1e9;
            if (std::sscanf(arg.c_str(), "%lf %lf %lf %lf", &x, &y, &yaw, &pitch) >= 2) {
                auto& pl = session_->player();
                pl.x = x, pl.y = y;
                pl.z = session_->world().tile(ifloor(x), ifloor(y)).floor;
                if (yaw < 1e8) pl.angle = yaw;
                if (pitch < 1e8) pl.pitch = pitch;
                session_->body().reset();
                resetHistory_ = true;
            }
        } else if (cmd == "near" && session_) {  // V13.2 (Tests, Bilder): near <wesen> <abstand> [seite] - der Spieler steht so weit
            // vor dem ersten Wesen dieser Art und sieht es an (seite: Winkel um das Wesen, 0 = vor ihm); frei begehbare Kachel
            char name[32] = {};
            double dist = 3.0, side = 0.0;
            std::sscanf(arg.c_str(), "%31s %lf %lf", name, &dist, &side);
            using R = game::Presence::Role;
            const std::string n = name;
            const R r = n == "faceling" ? R::Faceling : n == "skinstealer" ? R::SkinStealer : n == "wretch" ? R::Wretch : n == "rat" ? R::Rat
                        : n == "window" ? R::Window : n == "hound" ? R::Hound : n == "smiler" ? R::Smiler : n == "clump" ? R::Clump
                        : n == "duller" ? R::Duller : n == "arms" ? R::Arms : n == "wrangler" ? R::Wrangler
                        : n == "growler" ? R::Growler : n == "nguithrxurh" ? R::Nguithr : n == "ouroboros" ? R::Ouroboros
                        : n == "goblin" ? R::Goblin : n == "moth" ? R::Moth : n == "jerry" ? R::Jerry : R::Gestalt;  // V13.3; Teil 4: Motten (Level 2), Jerry
            auto& pr = session_->presence();
            if (!pr.has(r)) {
                log::warn("Skript: near - kein {}", game::Presence::roleName(r));
            } else {
                const vec3 at = pr.posOf(r);
                auto& w = session_->world();
                float baseZ = r == R::Window ? at.z - 1.9f : at.z, aimZ = r == R::Window ? at.z : at.z + (r == R::Rat ? 0.1f : 1.0f);
                if (r == R::Moth)  // V13.3 Teil 4: Motten an der Decke - der Boden darunter, Blick auf die Motte
                    if (const world::Tile* t0 = w.tileIfLoaded(ifloor(at.x), ifloor(at.y))) baseZ = t0->floor, aimZ = at.z;
                if (r == R::Jerry)  // V13.3 Teil 4: Jerry auf dem Rohr - der Boden darunter, Blick in sein Gesicht
                    if (const world::Tile* t0 = w.tileIfLoaded(ifloor(at.x), ifloor(at.y))) baseZ = t0->floor, aimZ = at.z + 0.42f;
                if (r == R::Nguithr)  // V13.3: die Netzkugeln an der Decke - der Boden darunter, Blick auf die Kugeln
                    if (const world::Tile* t0 = w.tileIfLoaded(ifloor(at.x), ifloor(at.y))) baseZ = t0->floor, aimZ = at.z - 0.6f;
                bool placed = false;
                // erst die gewuenschte Seite, dann reihum weitere Winkel (freie Sicht vom Auge zum Wesen)
                for (int k = 0; k < 13 && !placed; ++k) {
                    const float yaw0 = pr.yawOf(r) + (float)side + (k == 0 ? 0.0f : (float)((k + 1) / 2) * 0.48f * (k % 2 ? 1.0f : -1.0f));
                    for (double dd = dist; dd > 0.6 && !placed; dd -= 0.25) {
                        const double x = at.x + std::cos(yaw0) * dd, y = at.y + std::sin(yaw0) * dd;
                        const world::Tile* t = w.tileIfLoaded(ifloor(x), ifloor(y));
                        if (!t || !t->open() || std::fabs(t->floor - baseZ) > 0.6f) continue;
                        if (!game::clearSight(w, vec3{(float)x, (float)y, t->floor + (float)game::Player::kEye}, vec3{at.x, at.y, aimZ})) continue;
                        placed = true;
                    auto& pl = session_->player();
                    pl.x = x, pl.y = y, pl.z = t->floor;
                    pl.angle = std::atan2(at.y - y, at.x - x);
                    pl.pitch = std::atan2(aimZ - (t->floor + game::Player::kEye), dd) * 0.8;
                    session_->body().reset();
                    resetHistory_ = true;
                    log::info("Skript: near {} {:.1f} m", game::Presence::roleName(r), dd);
                    }
                }
                if (!placed) log::warn("Skript: near - kein freier Platz mit Sicht");
            }
        } else if (cmd == "goto" && session_) {  // V10 (Tests): zur naechsten Stelle einer Ebene: goto buero|keller|hotel
            const int want = arg == "hotel" ? 2 : (arg == "keller" ? 1 : 0);
            auto& pl = session_->player();
            auto& w = session_->world();
            const auto& lv = w.level();
            bool done = false;
            for (double r = 40.0; r <= 1400.0 && !done; r += 40.0)
                for (int k = 0; k < 24 && !done; ++k) {
                    const i64 x = ifloor(pl.x + std::cos(k * 0.2618) * r), y = ifloor(pl.y + std::sin(k * 0.2618) * r);
                    const world::Tile& t = w.tile(x, y);
                    if (!t.open() || !t.room || t.room->isCorridor || lv.regionOfZone(t.room->zone) != want || t.clearance() < 2.2f) continue;
                    pl.x = (double)x + 0.5, pl.y = (double)y + 0.5, pl.z = t.floor;
                    session_->body().reset();
                    resetHistory_ = true;
                    done = true;
                    log::info("Skript: goto {} -> ({}, {})", arg, x, y);
                }
        } else if (cmd == "visit" && session_) {  // V10 (Tests): vor eine Entity stellen: visit rolle abstand (in ihre Blickrichtung)
            using R = game::Presence::Role;
            char name[32] = {};
            double dist = 3.0;
            std::sscanf(arg.c_str(), "%31s %lf", name, &dist);
            const std::string n = name;
            const R role = n == "smiler" ? R::Smiler : n == "moth" ? R::Moth : n == "gestalt" ? R::Gestalt : n == "nest" ? R::Nest : R::Hound;
            auto& pr = session_->presence();
            if (pr.has(role)) {
                const vec3 q = pr.posOf(role);
                const float yw = pr.yawOf(role);
                auto& pl = session_->player();
                pl.x = q.x + std::cos(yw) * dist, pl.y = q.y + std::sin(yw) * dist;
                pl.z = session_->world().tile(ifloor(pl.x), ifloor(pl.y)).floor;
                pl.angle = dist >= 0.0 ? yw + kPi : yw, pl.pitch = 0.05;  // negativ: von der anderen Seite
                session_->body().reset();
                resetHistory_ = true;
            }
        } else if (cmd == "stalker" && session_) {  // V9/V10: ein Hound erscheint (etwa so viele Meter Weg entfernt)
            session_->forceStalker(std::atof(arg.c_str()));
        } else if ((cmd == "facestalker" || cmd == "face") && session_) {  // V9/V10 (Tests): zu einer Entity drehen (+ Versatz in rad)
            using R = game::Presence::Role;
            auto& pr = session_->presence();
            auto& pl = session_->player();
            double off = 0.0;
            R role = R::Hound;
            float lift = 0.6f;
            if (cmd == "face") {
                char name[32] = {};
                std::sscanf(arg.c_str(), "%31s %lf", name, &off);
                std::string n = name;
                role = n == "smiler" ? R::Smiler : n == "moth" ? R::Moth : n == "gestalt" ? R::Gestalt : n == "nest" ? R::Nest : n == "clump" ? R::Clump : R::Hound;
                lift = role == R::Smiler ? 1.7f : (role == R::Gestalt ? 1.6f : (role == R::Nest ? 1.3f : (role == R::Moth ? 0.0f : (role == R::Clump ? 1.6f : 0.6f))));
            } else {
                off = std::atof(arg.c_str());
            }
            if (pr.has(role)) {
                const vec3 q = pr.posOf(role);
                const double d = std::hypot(q.x - pl.x, q.y - pl.y);
                pl.angle = std::atan2(q.y - pl.y, q.x - pl.x) + off;
                pl.pitch = std::atan2(q.z + lift - pl.eye(), std::max(0.5, d));
            }
        } else if ((cmd == "stalkerinfo" || cmd == "figinfo") && session_) {  // V9/V10 (Tests): Zustand ins Protokoll
            using R = game::Presence::Role;
            const auto& pr = session_->presence();
            for (R r : {R::Smiler, R::Hound, R::Moth, R::Gestalt, R::Nest, R::Clump})
                if (pr.has(r))
                    log::info("Skript: {} x{} Abstand={:.2f} Zustand={} gesehen={} Health={:.0f} Sanity={:.0f}", game::Presence::roleName(r), pr.count(r),
                              pr.distOf(r), pr.stateOf(r), pr.seenRole(r), session_->stats().health, session_->stats().sanity);
            log::info("Skript: Entities aktiv={} Health={:.0f} Sanity={:.0f} Bereich={}", pr.activeCount(), session_->stats().health,
                      session_->stats().sanity, session_->currentRegion());
        } else if (cmd == "figure" && session_) {  // V10: Vorfuehrung - Entity N m vor dem Spieler, sieht ihn an
            char name[32] = {};
            double dist = 3.0, turn = 0.0, walk = 0.0;  // walk: Tempo in m/s (1 = Gehen)
            std::sscanf(arg.c_str(), "%31s %lf %lf %lf", name, &dist, &turn, &walk);
            std::string n = name;
            // Entities nach Namen (anim::Entity); player: der Mensch
            // V13.2: neue Wesen; player = 99 (Mensch)
            const int kind = n == "hound" ? 1 : n == "moth" ? 2 : n == "gestalt" ? 3 : n == "nest" ? 4 : n == "clump" ? 5 : n == "faceling" ? 6
                             : n == "skinstealer" ? 7 : n == "wretch" ? 8 : n == "rat" ? 9 : n == "window" ? 10 : n == "player" ? 99
                             : n == "duller" ? 11 : n == "arms" ? 12 : n == "wrangler" ? 13
                             : n == "growler" ? 14 : n == "nguithrxurh" ? 15 : n == "ouroboros" ? 16 : n == "goblin" ? 17
                             : n == "jerry" ? 18 : 0;  // V13.3; Teil 4: Jerry
            auto& pl = session_->player();
            vec3 at{(float)(pl.x + std::cos(pl.angle) * dist), (float)(pl.y + std::sin(pl.angle) * dist), 0.0f};
            at.z = session_->world().tile(ifloor(at.x), ifloor(at.y)).floor;
            session_->presence().showcase(kind, at, (float)(pl.angle + kPi + turn), (float)(walk > 0.0 && walk < 1.01 ? 1.2 : walk));
        } else if (cmd == "hiveat" && session_) {  // V13.2 (Tests): hiveat <x> <y> - Mottennest an dieser Tuerkachel
            int hx = -1, hy = -1;
            if (std::sscanf(arg.c_str(), "%d %d", &hx, &hy) == 2) session_->presence().forceHiveAt(hx, hy);
        } else if (cmd == "figstate" && session_) {  // V13.2 (Tests): figstate <feld> <wert> - Eingabe der Vorfuehrung
            char key[32] = {};
            float val = 0.0f;
            if (std::sscanf(arg.c_str(), "%31s %f", key, &val) < 2 || !session_->presence().showcaseSet(key, val))
                log::warn("Skript: figstate '{}' unbekannt oder keine Vorfuehrung", arg);
        } else if (cmd == "ouro" && session_) {  // V13.3 (Bilder): ouro <wall|tower|fill|quick|weld|kill> <wert> - Ouroboros-Baustelle steuern
            char key[32] = {};
            double val = 0.0;
            if (std::sscanf(arg.c_str(), "%31s %lf", key, &val) < 2 || !session_->presence().ouroSet(key, val))
                log::warn("Skript: ouro '{}' unbekannt oder keine Baustelle", arg);
        } else if (cmd == "jerry" && session_) {  // V13.3 Teil 4 (Bilder, Tests): jerry <quick|state|yaw|spell> <wert>
            char key[32] = {};
            double val = 0.0;
            if (std::sscanf(arg.c_str(), "%31s %lf", key, &val) < 2 || !session_->presence().jerrySet(key, val))
                log::warn("Skript: jerry '{}' unbekannt oder kein Jerry", arg);
        } else if (cmd == "goblin" && session_) {  // V13.3 (Bilder): goblin <phase|mob|friendly|quick|rout> <wert> - die Bande steuern
            char key[32] = {};
            double val = 0.0;
            if (std::sscanf(arg.c_str(), "%31s %lf", key, &val) < 2 || !session_->presence().goblinSet(key, val))
                log::warn("Skript: goblin '{}' unbekannt oder keine Bande", arg);
        } else if (cmd == "figclear" && session_) {  // V13 (Tests): Vorfuehrungen entfernen
            session_->presence().clearShowcase();
        } else if (cmd == "presence" && session_) {  // V10: Entity erscheinen lassen: presence <smiler|hound|moth|gestalt|blackout> [abstand]
            using R = game::Presence::Role;
            char name[32] = {};
            double dist = 0.0;
            std::sscanf(arg.c_str(), "%31s %lf", name, &dist);
            std::string n = name;
            if (n == "blackout") {
                session_->presence().forceBlackout();
            } else if (n == "encounter") {  // V11: Stromausfall als Welle, dann das Laecheln
                session_->presence().forceEncounter();
            } else {
                R r = n == "smiler" ? R::Smiler : n == "moth" || n == "nest" ? R::Moth : n == "gestalt" ? R::Gestalt : n == "clump" ? R::Clump : R::Hound;
                // V13.2: neue Wesen (Art nach der Ebene; sonst die Grundart)
                if (n == "faceling") r = R::Faceling;
                if (n == "skinstealer") r = R::SkinStealer;
                if (n == "wretch") r = R::Wretch;
                if (n == "rat") r = R::Rat;
                if (n == "window") r = R::Window;
                if (n == "duller") r = R::Duller;  // V13.3
                if (n == "arms") r = R::Arms;
                if (n == "wrangler") r = R::Wrangler;  // V13.3
                if (n == "growler") r = R::Growler;  // V13.3
                if (n == "nguithrxurh") r = R::Nguithr;
                if (n == "ouroboros") r = R::Ouroboros;  // V13.3: die Baustelle im Sektor Ouroboros
                if (n == "goblin") r = R::Goblin;        // V13.3: eine Bande Plague Goblins (Portal ausser Sicht)
                if (n == "jerry") r = R::Jerry;          // V13.3 Teil 4: Jerry auf seinem Rohr (Spawnpunkt der Karte)
                session_->presence().force(r, dist);
            }
        } else if (cmd == "audioinfo") {  // V11 (Tests): Zustand der Klanglandschaft ins Protokoll
            const audio::Acoustics& ac = soundscape_.acoustics();
            log::info("Audio: Leuchten {} Betten {} Stellen {} Brummen x{:.2f} | Raum t60 {:.2f} wet {:.2f} early {:.2f} Masse {:.0f}x{:.0f}x{:.1f} Hoerer {:.1f},{:.1f},{:.1f}",
                      soundscape_.activeLamps(), soundscape_.activeBeds(), soundscape_.activeSpots(), soundscape_.humSurge(), ac.t60, ac.wet,
                      ac.early, ac.roomW, ac.roomD, ac.roomH, ac.lx, ac.ly, ac.lz);
            if (audio_) log::info("Audio-Status: {}", audio_->statsLine());
        } else if (cmd == "audiostat") {  // V13.3 (Tests): Geraet, Stimmen, Pegel am Master-Ausgang ins Protokoll
            if (audio_) log::info("Audio-Status: {}", audio_->statsLine());
        } else if (cmd == "audiolog") {  // V13.3 (Tests): audiolog <s> - Audio-Status alle s Sekunden (0 aus)
            audioLogEvery_ = std::max(0.0, std::atof(arg.c_str()));
            audioLogNext_ = time_ + audioLogEvery_;
        } else if (cmd == "level" && session_) {  // V13.3 (Tests): level <id> [anker] - Ebenenwechsel wie durch einen Ausgang
            char id[64] = {}, anchor[64] = {};
            std::sscanf(arg.c_str(), "%63s %63s", id, anchor);
            changeLevel(id, anchor);
            return;  // die neue Sitzung erst ein Bild laufen lassen
        } else if (cmd == "focus") {  // V13.3 (Tests): focus off|on - Fokusverlust (Alt+Tab) ueber WM_ACTIVATE nachstellen
            const bool on = arg != "off" && arg != "0";
            simFocus_ = true;
            SendMessageW(window_.hwnd(), WM_ACTIVATE, on ? WA_ACTIVE : WA_INACTIVE, 0);
            log::info("Skript: Fokus {}", on ? "zurueck" : "verloren");
        } else if (cmd == "minimize" || cmd == "restore") {  // V13.3 (Tests): Fenster minimieren / wiederherstellen
            ShowWindow(window_.hwnd(), cmd == "minimize" ? SW_MINIMIZE : SW_RESTORE);
            if (cmd == "restore") SendMessageW(window_.hwnd(), WM_ACTIVATE, WA_ACTIVE, 0);
        } else if (cmd == "audiofault" && audio_) {  // V13.3 (Tests): audiofault <art> [wert] - Geraete-/Fehlerpfad gezielt ausloesen
            char kind[32] = {};
            int param = 0;
            std::sscanf(arg.c_str(), "%31s %d", kind, &param);
            using F = audio::Fault;
            const std::string k = kind;
            const F f = k == "invalidate" ? F::DeviceInvalidated : k == "default" ? F::DefaultChanged : k == "format" ? F::FormatChanged
                        : k == "exclusive" ? F::ExclusiveOverride : k == "remove" ? F::DeviceRemoved : k == "stall" ? F::StallEvents
                        : k == "mute" ? F::MuteOutput : k == "nan" ? F::NanOutput : F::InitFails;
            if (k != "invalidate" && k != "default" && k != "format" && k != "exclusive" && k != "remove" && k != "stall" && k != "mute" &&
                k != "nan" && k != "initfail") {
                log::error("Skript: audiofault - unbekannte Art '{}'", k);
                scriptFailed_ = true;
            } else {
                audio_->injectFault(f, param);
            }
        } else if (cmd == "audiorestart" && audio_) {  // V13.3 (Tests): Strom schliessen und neu oeffnen
            audio_->requestRestart("Skript");
        } else if (cmd == "audioexpect") {  // V13.3 (Tests): audioexpect <dBFS> - Pegel am Master-Ausgang (letzte Sekunde) mindestens so hoch
            const double want = std::atof(arg.c_str());
            const auto st = audio_ ? audio_->stats() : audio::AudioStats{};
            const double db = st.outRms > 1e-9f ? 20.0 * std::log10(st.outRms) : -180.0;
            if (!st.deviceOpen || db < want) {
                scriptFailed_ = true;
                log::error("Skript: audioexpect {} dBFS - Ausgang {:.1f} dBFS, Geraet {}", want, db, st.deviceOpen ? "offen" : "ZU");
            } else {
                log::info("Skript: audioexpect {} dBFS ok ({:.1f} dBFS)", want, db);
            }
        } else if (cmd == "resume") {  // V13.3 (Tests): Pausenmenue schliessen (wie "Weiter")
            if (ui_.mode() == ui::Mode::Pause) ui_.setMode(ui::Mode::Hud);
        } else if (cmd == "perf") {  // V10 (Tests): Zeiten und Dreiecke dieses Bildes ins Protokoll
            const auto& rs = renderer_->stats();
            u64 vramUse = 0, vramBudget = 0;  // V13: belegter Grafikspeicher (DXGI 1.4) und Arbeitsspeicher des Prozesses
            renderer_->videoMemory(vramUse, vramBudget);
            PROCESS_MEMORY_COUNTERS pmc{};
            pmc.cb = sizeof(pmc);
            K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
            log::info("Skript: perf Spiel {:.2f} ms, Grafik {:.2f} ms, {} Dreiecke, {} Netze, VRAM {} MB von {} MB, RAM {} MB (Spitze {} MB) | {}",
                      simMs_, renderMs_, rs.triangles, rs.meshes, vramUse >> 20, vramBudget >> 20, (u64)pmc.WorkingSetSize >> 20,
                      (u64)pmc.PeakWorkingSetSize >> 20, renderer_->gpuTimer().summary());
        } else if (cmd == "give" && session_) {  // V9: Item ins Inventar (Tests)
            session_->inventory().add(arg);
        } else if (cmd == "phen" && session_) {  // V13.3 (Tests, Bilder): Erscheinungen - phen infect <s> | garden <s> | claws |
            // look <set> <i> <abstand> [winkel grad] [hoehe]: vor eine Stelle stellen und hinsehen (set: paint, jester, fungus,
            // note, claws, bottle, expel); jester: auf das Bild der Spielleiterin treten (session_phenomena.cpp)
            game::Phenomena& P = session_->phenomena();
            char what[32] = {}, set[32] = {};
            double a = 0, d = 2.5, ang = 0, eh = 1.6;
            int i = 0;
            std::sscanf(arg.c_str(), "%31s", what);
            const std::string w = what;
            if (w == "infect" && std::sscanf(arg.c_str(), "%*s %lf", &a) == 1) P.infect(0), P.ailment(0).infectT = a;
            else if (w == "garden" && std::sscanf(arg.c_str(), "%*s %lf", &a) == 1) P.ailment(0).garden = a;
            else if (w == "claws") for (auto& cd : P.clawDoors()) cd.cool = 0.01;
            else if (w == "jester" || (w == "look" && std::sscanf(arg.c_str(), "%*s %31s %d %lf %lf %lf", set, &i, &d, &ang, &eh) >= 2)) {
                const std::string s = w == "jester" ? "jester" : set;
                vec3 at;
                float yaw = 0.0f;
                bool ok = true;
                if (s == "paint" && i < (int)P.paintings().size()) at = P.paintings()[(size_t)i].pos, yaw = P.paintings()[(size_t)i].yaw;
                else if (s == "jester") {
                    ok = false;
                    for (const auto& pt : P.paintings())
                        if (pt.jester) at = pt.pos, yaw = pt.yaw, ok = true;
                } else if (s == "fungus" && i < (int)P.fungusZones().size()) at = P.fungusZones()[(size_t)i].pos, yaw = P.fungusZones()[(size_t)i].yaw;
                else if (s == "note" && i < (int)P.noteSpots().size()) at = P.noteSpots()[(size_t)i].pos, yaw = P.noteSpots()[(size_t)i].yaw;
                else if (s == "bottle" && i < (int)P.bottleSpots().size()) at = P.bottleSpots()[(size_t)i].pos, yaw = P.bottleSpots()[(size_t)i].yaw;
                else if (s == "expel" && i < (int)P.expelSpots().size()) at = P.expelSpots()[(size_t)i].pos, yaw = P.expelSpots()[(size_t)i].yaw;
                else if (s == "claws" && i < (int)P.clawDoors().size()) at = P.clawDoors()[(size_t)i].spot.pos, yaw = P.clawDoors()[(size_t)i].spot.yaw;
                else ok = false;
                if (!ok) {
                    log::error("Skript: phen {} - keine solche Stelle", arg);
                    scriptFailed_ = true;
                } else {
                    auto& pl = session_->player();
                    const double t = yaw + ang * 0.0174533;
                    const double px = w == "jester" ? at.x : at.x + std::cos(t) * d, py = w == "jester" ? at.y : at.y + std::sin(t) * d;
                    pl.x = px, pl.y = py, pl.z = session_->world().tile(ifloor(px), ifloor(py)).floor;
                    if (w != "jester") {
                        pl.angle = std::atan2(at.y - py, at.x - px);
                        pl.pitch = -std::atan2(eh - 0.3, std::max(0.3, d));
                    }
                    session_->body().reset();
                    resetHistory_ = true;
                    log::info("Skript: phen {} -> ({:.2f}, {:.2f}) Ziel ({:.2f}, {:.2f})", arg, px, py, at.x, at.y);
                }
            }
        } else if (cmd == "health" && session_) {  // V9: Health setzen (Tests)
            session_->stats().health = std::clamp(std::atof(arg.c_str()), 1.0, 100.0);
        } else if (cmd == "sanity" && session_) {  // V7: Sanity setzen (Tests)
            session_->stats().sanity = std::clamp(std::atof(arg.c_str()), 0.0, 100.0);
        } else if (cmd == "coophost" && session_) {
            // V13.2/V13.3 (Tests): coophost <Port> <Datei> [lobby] - diese Sitzung hosten (direkt auf Port oder ueber die
            // Lobby aus koop.json); der lange Beitrittscode kommt in die Datei, sobald er da ist
            char file[260] = {}, mode[16] = {};
            int port = 27500;
            std::sscanf(arg.c_str(), "%d %259s %15s", &port, file, mode);
            std::string err;
            if (!loadNet(err)) {
                log::error("Skript: {}", err);
                scriptFailed_ = true;
                continue;
            }
            coop::NetSettings ns = netSettings(std::string(mode) == "lobby", false);
            ns.directPort = (u16)port;
            coopHost_ = std::make_unique<coop::CoopHost>(netApi_, "Host", coopBuild());
            if (!coopHost_->start(ns, 4, err)) {
                log::error("Skript: {}", err);
                coopHost_.reset();
                scriptFailed_ = true;
                continue;
            }
            coopHost_->attach(*session_, "", difficultyIndex(session_->difficulty));
            coopKeyFile_ = file;
            log::info("Skript: Koop-Host gestartet ({})", ns.useLobby ? "Lobby" : std::format("direkt, Port {}", coopHost_->port()));
        } else if (cmd == "coopjoin") {
            // V13.2/V13.3 (Tests): coopjoin <Adresse> <Port> <Datei mit Code> [lobby] - direkt zu Adresse:Port oder ueber die Lobby
            char addr[128] = {}, file[260] = {}, mode[16] = {};
            int port = 27500;
            std::sscanf(arg.c_str(), "%127s %d %259s %15s", addr, &port, file, mode);
            std::string code;
            if (FILE* f = std::fopen(file, "rb")) {
                char buf[64] = {};
                std::fread(buf, 1, sizeof(buf) - 1, f);
                std::fclose(f);
                code = buf;
            }
            std::string err;
            if (!joinCoop("Gast", code, std::string(mode) == "lobby", std::format("{}:{}", addr, port), false, err)) {
                log::error("Skript: {}", err);
                scriptFailed_ = true;
            }
        } else if (cmd == "egopose") {  // V13.2 (Tests): egopose 1 - auch von aussen die Lampenhaltung des Ego-Bilds
            egoPose_ = std::atoi(arg.c_str()) != 0;
        } else if (cmd == "vmdebug" && session_) {  // V13 (Tests): Teile ausblenden (1 Hand, 2 Koerper, 4 Lampe)
            session_->debugDraw = std::atoi(arg.c_str());
        } else if (cmd == "figcam") {  // V13 (Tests): figcam <az> <el> <abstand> [hoehe] [vor] | figcam off
            float az = 0, el = 0, d = 2.0f, h = 0.6f, ahead = 0.0f;
            if (arg == "off") figCam_.on = false;
            else if (std::sscanf(arg.c_str(), "%f %f %f %f %f", &az, &el, &d, &h, &ahead) >= 3) figCam_ = {true, az, el, d, h, ahead};
            resetHistory_ = true;
        } else if (cmd == "grace" && session_) {  // V13.3 (Bilder): grace <s> - so lange kein Auftritt der Regie (presence erzwingt weiter)
            session_->presence().setGrace(std::atof(arg.c_str()));
        } else if (cmd == "lurecam" && session_) {  // V13.3 (Bilder): lurecam <az> <el> <abstand> [hoehe] - freie Kamera auf den neuesten Leuchtstab
            float az = 0, el = 20, d = 2.0f, h = 0.0f;
            std::sscanf(arg.c_str(), "%f %f %f %f", &az, &el, &d, &h);
            const auto& ls = session_->lures().lures();
            if (ls.empty()) {
                log::warn("Skript: lurecam - kein Leuchtstab");
            } else {
                const vec3 t = ls.back().pos + vec3{0, 0, h};
                float a = az * 0.0174533f;
                const float e = el * 0.0174533f;
                if (az >= 900.0f && session_->presence().has(game::Presence::Role::Smiler)) {  // 1000 +/- Grad: vom Smiler aus hinter dem Stab
                    const vec3 sp = session_->presence().posOf(game::Presence::Role::Smiler);
                    a = std::atan2(t.y - sp.y, t.x - sp.x) + (az - 1000.0f) * 0.0174533f;
                }
                const vec3 c = t + vec3{std::cos(a) * std::cos(e), std::sin(a) * std::cos(e), std::sin(e)} * d, dv = t - c;
                freeCam_ = {true, c, std::atan2(dv.y, dv.x) * 57.29578f, std::atan2(dv.z, std::max(std::hypot(dv.x, dv.y), 1e-4f)) * 57.29578f, 0.0f};
                log::info("Skript: lurecam auf ({:.2f}, {:.2f}, {:.2f})", t.x, t.y, t.z);
                resetHistory_ = true;
            }
        } else if (cmd == "vmcam") {  // V13 (Tests): vmcam <az> <el> <abstand> | vmcam off - Kamera um die Hand
            float az = 0, el = 0, d = 0.5f;
            if (arg == "off") vmCam_.on = false;
            else if (std::sscanf(arg.c_str(), "%f %f %f", &az, &el, &d) >= 2) vmCam_ = {true, az, el, d};
            resetHistory_ = true;
        } else if (cmd == "cam" || cmd == "camat") {  // V13.3 (Tests): cam x y z blick neigung [fov] | camat x y z zx zy zz [fov] | cam off
            float v[7] = {0, 0, 0, 0, 0, 0, 0};
            const int n = std::sscanf(arg.c_str(), "%f %f %f %f %f %f %f", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6]);
            if (arg == "off") {
                freeCam_.on = false;
            } else if (cmd == "cam" && n >= 5) {
                freeCam_ = {true, {v[0], v[1], v[2]}, v[3], v[4], n >= 6 ? v[5] : 0.0f};
            } else if (cmd == "camat" && n >= 6) {
                const vec3 d = vec3{v[3], v[4], v[5]} - vec3{v[0], v[1], v[2]};
                const float h = std::sqrt(d.x * d.x + d.y * d.y);
                freeCam_ = {true, {v[0], v[1], v[2]}, std::atan2(d.y, d.x) * 57.29578f, std::atan2(d.z, std::max(h, 1e-4f)) * 57.29578f,
                            n >= 7 ? v[6] : 0.0f};
            } else {
                log::error("Skript: {} braucht Zahlen ({})", cmd, arg);
                scriptFailed_ = true;
            }
            resetHistory_ = true;
        } else if (cmd == "testlight") {  // V13.3 (Tests): testlight x y z zx zy zz [staerke] [kegel] | testlight off
            float v[8] = {0, 0, 0, 1, 0, 0, 1, 28};
            const int n = std::sscanf(arg.c_str(), "%f %f %f %f %f %f %f %f", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7]);
            if (arg == "off") testLight_.on = false;
            else if (n >= 6) testLight_ = {true, {v[0], v[1], v[2]}, normalize(vec3{v[3] - v[0], v[4] - v[1], v[5] - v[2]}), v[6], v[7]};
            else if (arg.rfind("eye", 0) == 0 && session_) {  // V13.3 (Bilder): testlight eye [staerke] [kegel] - ueber dem Kopf in Blickrichtung
                float pw = 0.6f, cone = 40.0f;
                std::sscanf(arg.c_str() + 3, "%f %f", &pw, &cone);
                const auto& pl = session_->player();
                const vec3 f = forwardFromAngles((float)pl.angle, (float)pl.pitch);
                testLight_ = {true, session_->body().eye() + vec3{0, 0, 0.25f} + f * 0.3f, f, pw, cone};  // vor dem Kopf (kein Schatten des eigenen Kopfes)
            }
            resetHistory_ = true;
        } else if (cmd == "worldlight") {  // V13.3 (Tests): Leuchten der Welt skalieren (0 = dunkel)
            worldLight_ = (float)std::atof(arg.c_str());
            resetHistory_ = true;
        } else if (cmd == "propdump" && session_) {  // V13.3 (Tests): Ausstattung, Schilder und Raeume als JSON
            propDump(arg);
        } else if (cmd == "quit") {
            quit_ = true;
            return;
        } else {
            log::error("Skript: unbekannter Befehl '{}'", cmd);
            scriptFailed_ = true;
        }
    }
    quit_ = true;  // Skript zu Ende
}

bool App::init(std::string& err) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    settings_.load();
    if (opt_.quality >= 0 && opt_.quality <= 3) {
        settings_.applyQualityPreset(opt_.quality);
        settings_.autoQuality = false;  // feste Stufe fuer diesen Start: keine Automatik
    }
    settings_.fromJson(opt_.overrides);
    if (testRun()) opt_.noMouse = true;  // reproduzierbare Aufnahmen und Tests
    // V13.3: --script @datei - Befehle aus einer Datei (lange Bilderfolgen; Zeilenenden gelten wie ';')
    if (opt_.script.size() > 1 && opt_.script[0] == '@') {
        if (auto bytes = paths::readFile(opt_.script.substr(1))) {
            opt_.script.assign(bytes->begin(), bytes->end());
            for (char& ch : opt_.script)
                if (ch == '\n' || ch == '\r') ch = ';';
        } else {
            err = "Skriptdatei fehlt: " + opt_.script.substr(1);
            return false;
        }
    }
    for (size_t a = 0, b; a <= opt_.script.size() && !opt_.script.empty(); a = b + 1) {
        b = opt_.script.find(';', a);
        if (b == std::string::npos) b = opt_.script.size();
        script_.push_back(opt_.script.substr(a, b - a));
    }
    if (opt_.fast) settings_.render.renderScale = 0.5f;
    if (opt_.mode >= 0) settings_.render.displayMode = opt_.mode;
    if (!content_.load(paths::dataDir(), err)) return false;
    if (!opt_.levelFile.empty()) {  // V13.3 (Kontaktboegen): Testszene aus einer Datei
        std::string id;
        if (!content_.addLevelFile(opt_.levelFile, id, err)) return false;
        opt_.level = id;
    }
    prefabs_.loadDirectory(paths::join(paths::dataDir(), "prefabs"));
    input_.loadBindings(paths::join(paths::dataDir(), "input.json"));
    jobs_ = std::make_unique<JobSystem>();

    int w = opt_.width ? opt_.width : settings_.windowWidth, h = opt_.height ? opt_.height : settings_.windowHeight;
    bool fullscreen = settings_.fullscreen && !opt_.windowed && opt_.width == 0 && opt_.shot.empty();
    if (!window_.create(L"LIMINAL", w, h, fullscreen ? plat::DisplayMode::Borderless : plat::DisplayMode::Windowed, &input_)) {
        err = "Das Fenster konnte nicht erstellt werden.";
        return false;
    }
    lastWinMode_ = window_.displayMode();
    appliedWinW_ = settings_.windowWidth, appliedWinH_ = settings_.windowHeight;
    renderer_ = std::make_unique<gfx::Renderer>();
    if (!renderer_->init(window_, *jobs_, settings_.render, opt_.warp, err)) return false;
    if (renderer_->gpuInfo().warp)
        log::warn("Software-Rendering (WARP) aktiv - Grafikqualitaet wird reduziert"), settings_.applyQualityPreset(0),
            renderer_->applySettings(settings_.render);
    // V6: Grafik an die Hardware anpassen (Schaetzung jetzt, Messung im Hauptmenue); Tests/Benchmark
    // mit fester Stufe bleiben unberuehrt
    if (opt_.quality < 0 && opt_.overrides.size() == 0 && !testRun() &&
        autoTune_.prepare(settings_, renderer_->gpuInfo(), renderer_->outputWidth(), renderer_->outputHeight()))
        renderer_->applySettings(settings_.render);
    // V6: 2048er-Texturen brauchen ~1.5 GB - nicht auf Grafikkarten mit wenig eigenem Speicher
    {
        const auto& gi = renderer_->gpuInfo();
        if (settings_.render.textureSize > 1024 && !gi.integrated && gi.dedicatedVram < (u64)3 * 1024 * 1024 * 1024) {
            log::info("Texturqualitaet auf 1024 begrenzt ({} MB Grafikspeicher)", gi.dedicatedVram >> 20);
            settings_.render.textureSize = 1024;
            renderer_->applySettings(settings_.render);
        }
    }
    renderer_->startTextureGeneration();
    renderer_->startFigureGeneration();  // V10: Figuren im Hintergrund modellieren

    audio_ = std::make_unique<audio::AudioSystem>();
    audio_->init(*jobs_, paths::dataDir());
    audio_->setVolumes(settings_.masterVolume, settings_.effectsVolume, settings_.ambienceVolume);

    ui_.setMode(ui::Mode::Loading);
    if (opt_.debug) ui_.toggleDebug();
    return true;
}

// --- Sitzungen -----------------------------------------------------------------------------------
std::unique_ptr<game::GameSession> App::makeSession(long long seed, const std::string& difficulty,
                                                    const std::string& name, const std::string& level, int layout) {
    // V8: ohne Angabe die zusammenhaengende Welt (alle Ebenen); einzelne Level nur noch fuer alte
    // Spielstaende und Tests (--level)
    std::shared_ptr<const world::LevelDef> lvl;
    if (level.empty() || (content_.world() && level == content_.world()->id) || layout >= world::kLayoutWorld)
        lvl = content_.world() ? content_.world() : content_.level("level0", layout);
    else
        lvl = content_.level(level, layout);
    auto s = std::make_unique<game::GameSession>(content_, prefabs_, lvl, seed, difficulty, name, *jobs_,
                                                 settings_.viewDistance);
    return s;
}

void App::attachSession(game::GameSession& s) {
    // Die bisherige Sitzung lebt noch kurz weiter; ihre Hintergrund-Jobs duerfen keine Chunks
    // mehr an den Renderer melden (sonst stuende Geometrie der alten Welt in der neuen)
    if (session_ && session_.get() != &s) session_->chunkListener = nullptr;
    renderer_->clearWorld();
    s.chunkListener = [this](const world::ChunkData& cd, bool loaded) {
        if (loaded) renderer_->onChunkLoaded(cd);
        else renderer_->onChunkUnloaded(cd);
    };
    s.player().walkSpeed = settings_.walkSpeed;
    s.player().bobEnabled = settings_.bob;
}

void App::startTitleWorld() {
    // Hinter dem Hauptmenue dreht sich langsam die Welt (Seed 7 wie in V4)
    if (audio_) audio_->stopAll(), audio_->resetMix();  // V13.3: keine Daempfung/Pause aus dem Spiel im Menue (B-020)
    ++audioGen_;
    session_.reset();
    session_ = makeSession(7, content_.defaultDifficulty(), "", "");
    attachSession(*session_);
    session_->placeAtSpawn();
    session_->preload();
    renderer_->syncMaterials(session_->world().bank());
    state_ = State::Title;
    ui_.setMode(ui::Mode::Title);
    if (!testRun()) ui_.fadeFromBlack(0.6);  // Aufnahmen/Tests ohne Ueberblendung
    resetHistory_ = true;
}

void App::enterPlaying(std::unique_ptr<game::GameSession> s) {
    if (audio_ && session_) log::info("Audio vor dem Wechsel: {}", audio_->statsLine());  // V13.3 (AUDIO_BUG.md)
    if (audio_) audio_->stopAll(), audio_->resetMix();
    ++audioGen_;
    session_ = std::move(s);
    state_ = State::Playing;
    ui_.setMode(ui::Mode::Hud);
    if (!testRun()) ui_.fadeFromBlack(0.7);  // Aufnahmen/Tests ohne Ueberblendung
    framesInPlay_ = 0;
    resetHistory_ = true;
    havePrevRoot_ = false;
    pendYaw_ = pendPitch_ = 0.0;
    renderer_->syncMaterials(session_->world().bank());
    pilot_.reset();
    if (opt_.demo) {
        pilot_ = std::make_unique<game::Autopilot>(session_->player(), (u64)session_->seed);
        ui_.toast("Demo: automatische Erkundung – beliebige Taste übernimmt", 5.0);
    }
}

void App::newGame(const std::string& name, const std::string& difficulty, const std::string& level) {
    std::random_device rd;
    long long seed = (long long)(((u64)rd() << 16 ^ rd()) % 2147483646ull) + 1;
    if (opt_.seed >= 0 && state_ == State::Loading) seed = opt_.seed;
    ui_.setLoading("Erzeuge Welt aus Seed " + std::to_string(seed) + " …", 0);
    auto s = makeSession(seed, difficulty, name, level.empty() ? opt_.level : level);
    attachSession(*s);
    s->slot = game::savegame::newSlotId();
    s->placeAtSpawn();
    if (!opt_.room.empty()) {
        // Test: Sektoren spiralfoermig um den Ursprung nach dem Raumtyp absuchen
        bool found = false;
        for (int rad = 0; rad < 12 && !found; ++rad)
            for (int sy = -rad; sy <= rad && !found; ++sy)
                for (int sx = -rad; sx <= rad && !found; ++sx) {
                    if (std::max(std::abs(sx), std::abs(sy)) != rad) continue;
                    world::Sector& sec = s->world().sector(sx, sy);
                    std::string key = opt_.room, zoneKey;
                    if (auto at = key.find('@'); at != std::string::npos) zoneKey = key.substr(at + 1), key = key.substr(0, at);
                    for (auto& r : sec.rooms)
                        if ((r->type->key == key || r->type->key.ends_with("/" + key)) &&  // V8: "ballsaal" = "level2/ballsaal"
                            (zoneKey.empty() || s->world().level().zones[(size_t)r->zone].key == zoneKey)) {
                            auto [cx, cy] = r->center();
                            s->player().x = cx;
                            s->player().y = cy;
                            s->player().z = r->floor;
                            s->player().angle = r->w() >= r->h() ? 0.0 : kPi * 0.5;
                            if (r->w() >= r->h()) s->player().x = r->x0 + 1.5;
                            else s->player().y = r->y0 + 1.5;
                            if (auto m = r->machineTile()) {
                                // Versorgungsraum: schraeg vor den Automaten, Blick auf Automat und Theke
                                double side = r->machineDirX != 0 ? 1.0 : 0.0;
                                s->player().x = m->x + 0.5 + r->machineDirX * 2.1 + (1.0 - side) * 0.9;
                                s->player().y = m->y + 0.5 + r->machineDirY * 2.1 + side * 0.9;
                                double mx = m->x + 0.5 - s->player().x, my = m->y + 0.5 - s->player().y;
                                s->player().angle = std::atan2(my, mx);
                                s->player().pitch = -0.12;
                            }
                            found = true;
                            log::info("Testraum {} bei ({:.1f}, {:.1f})", opt_.room, s->player().x, s->player().y);
                            break;
                        }
                }
    }
    if (opt_.posX < 1e8) s->player().x = opt_.posX, s->player().y = opt_.posY;
    if (opt_.yaw < 1e8) s->player().angle = opt_.yaw;
    if (opt_.pitch < 1e8) s->player().pitch = opt_.pitch;
    s->preload();
    currentSlot_ = s->slot;
    currentSlotPath_.clear();
    enterPlaying(std::move(s));
    ui_.showHelpHint(8.0);
    // V11: Seed und Schwierigkeit stehen im Pausenmenue und im Protokoll, nicht als Meldung im Bild
    log::info("Neues Spiel: {} · {} · Seed {}", session_->name, content_.difficulty(difficulty).label, session_->seed);
    // V9: leise Einleitung (nicht in Aufnahmen und Tests)
    if (!testRun() && session_->world().level().world())
        ui_.intro({"Du bist irgendwo durchgefallen. Es riecht nach feuchtem Teppich.",
                   "Die Leuchten summen. Irgendwo hinter dir summt etwas zurück.",
                   "Man sagt, ein Aufzug fährt hinaus – wenn er Strom hat."});
    if (opt_.shot.empty() && opt_.benchmark == 0) saveGame();
}

bool App::loadGame(const game::SaveEntry& e) {
    const Json& d = e.data;
    // Listenzusatz alter Versionen ("  (V5)") gehoert nicht zum Namen
    std::string name = d["name"].asString("Wanderer");
    for (const char* suffix : {"  (V3)", "  (V4)", "  (V5)", "  (V6)", "  (V7)", "  (V8)", "  (V9)"})
        if (name.size() > std::strlen(suffix) && name.ends_with(suffix)) name.resize(name.size() - std::strlen(suffix));
    // V6: Weltstruktur des Spielstands (fehlt bei V3/V4/V5 und fruehen V6-Staenden -> alter Aufbau, gleiche Welt)
    int layout = (int)d["layout"].asInt(world::kLayoutLegacy);
    auto s = makeSession(d["seed"].asInt(), d["difficulty"].asString("medium"), name, d["level"].asString("level0"), layout);
    attachSession(*s);
    s->fromJson(d);
    // Alte Staende (V3/V4/V5) werden als neuer V6-Slot weitergefuehrt
    s->slot = e.legacy() ? game::savegame::newSlotId() : e.slot;  // von savegame::load geprueft
    s->preload();
    currentSlot_ = s->slot;
    currentSlotPath_ = e.legacy() ? "" : e.path;
    if (e.legacy()) currentSlotPath_ = "legacy:" + e.path;
    std::string when = d["saved_at"].asString("");
    enterPlaying(std::move(s));
    ui_.toast(std::format("{} geladen ({})", session_->name, when), 3.0);
    return true;
}

// V7: Durch den Notausgang auf die naechste Ebene. Inventar, Werte, Taschenlampe, Notizbuch und
// zurueckgelegter Weg kommen mit; die neue Ebene hat einen eigenen Seed (aus dem alten abgeleitet).
void App::changeLevel(const std::string& next, const std::string& anchor) {
    if (!session_) return;
    auto lvl = content_.level(next, world::kLayoutV6);
    if (!lvl || lvl->id != next) {
        ui_.clearFadeOut();
        // V11: Uebergang ohne Ziel - die Gruben der Lochfelder in Level 0 fuehren ins Bodenlose: man kommt am letzten
        // sicheren Ort wieder zu sich. (Alle zehn Ebenen sind gebaut; fehlt ein Ziel trotzdem, geht es nicht weiter.)
        if (session_->rescueAfterFailedTransition()) {
            const std::string& at = session_->checkpointLabel();
            ui_.toast(at.empty() ? std::string("Du fällst ins Bodenlose – und kommst am Anfang wieder zu dir.")
                                 : std::format("Du fällst ins Bodenlose – und kommst bei „{}“ wieder zu dir.", at),
                      4.5);
        } else {
            if (!next.empty()) log::warn("Uebergang: Ziel '{}' gibt es nicht", next);
            ui_.toast(session_->world().level().mapped() ? "Hier geht es nicht weiter." : "Die Tür gibt nicht nach.", 3.0);
        }
        return;
    }
    game::GameSession& old = *session_;
    // V11: jede Ebene hat im ganzen Lauf denselben Seed (abgeleitet vom Lauf-Seed; Level 0 ist er selbst):
    // wer zurueckkehrt, findet dieselbe Belegung, und das Gedaechtnis der Ebene stellt Aufgenommenes wieder her
    const long long runSeed = (long long)old.runSeedOr();
    long long seed = next == "level0" ? runSeed : (long long)(hash64((u64)runSeed, "level", next.c_str()) % 2147483646ull) + 1;
    auto s = makeSession(seed, old.difficulty, old.name, next, world::kLayoutV6);
    if (coopHost_) s->setSupplyScale(coopHost_->supplyScaleNext());  // V13.2 Koop: Vorraete fuer die Gruppe
    else if (coopClient_) s->setSupplyScale(coopClient_->pendingSupply());
    attachSession(*s);
    s->slot = old.slot;
    s->carryOver(old);
    if (anchor.empty() || !s->placeAtAnchor(anchor)) s->placeAtSpawn();  // V11: am Gegenstueck des Uebergangs
    s->preload();
    log::info("Notausgang: {} -> {} (Seed {})", old.levelId, next, seed);
    enterPlaying(std::move(s));
    ui_.levelCard(lvl->id, lvl->name, lvl->displayTitle.empty() ? lvl->title : lvl->displayTitle);  // V12: Tafel bei der Ankunft
    if (opt_.shot.empty() && opt_.benchmark == 0) saveGame();
}

void App::loadLastSave() {
    if (currentSlotPath_.empty()) {
        toTitle();
        return;
    }
    std::string path = currentSlotPath_.rfind("legacy:", 0) == 0 ? currentSlotPath_.substr(7) : currentSlotPath_;
    game::SaveEntry e;
    if (!game::savegame::load(path, e) || !loadGame(e)) {
        toTitle();
        return;
    }
    session_->reviveAfterDeath();
}

void App::saveGame() {
    if (!session_ || state_ != State::Playing || session_->stats().dead) return;
    if (coopClient_) return;  // V13.2 Koop: Gaeste speichern nichts (der Host speichert fuer alle)
    Json data = session_->toJson();
    if (coopHost_ || coopSave_) {  // V13.2: eigener Koop-Stand (nie ueber einen Einzelspielerstand)
        data.set("coop", true);
        if (coopHost_) data.set("coop_guests", coopHost_->saveGuests());
    }
    data.set("name", session_->name);
    data.set("difficulty", session_->difficulty);
    data.set("level", session_->levelId);
    data.set("layout", session_->world().level().layout);
    data.set("seed", (long long)session_->seed);
    std::vector<u8> thumb;
    const int tw = 256, th = 144;
    // Vorschaubild ohne HUD und Menue: Szene einmal sauber zeichnen, dann abgreifen
    if (renderer_->texturesReady()) renderWorld(0.0, true);
    if (!renderer_->captureScene(tw, th, thumb)) thumb.clear();
    if (game::savegame::write(session_->slot, std::move(data), thumb, tw, th)) {
        currentSlotPath_ = paths::join(game::savegame::saveDir(), "slot_" + session_->slot + ".json");
        ui_.savedIndicator();
        log::info("Gespeichert: {}", currentSlotPath_);
    } else {
        ui_.toast("Speichern fehlgeschlagen!", 3.0);
    }
}

void App::toTitle() {
    if (state_ == State::Playing) saveGame();
    leaveCoop("");  // V13.2
    startTitleWorld();
}

void App::settingsChanged() {
    renderer_->applySettings(settings_.render);
    audio_->setVolumes(settings_.masterVolume, settings_.effectsVolume, settings_.ambienceVolume);
    auto want = settings_.fullscreen ? plat::DisplayMode::Borderless : plat::DisplayMode::Windowed;
    if (want != window_.displayMode() && opt_.shot.empty()) window_.setDisplayMode(want);
    // V6: Fenstergroesse und Grafik-Automatik (neu messen, wenn "Automatisch" gewaehlt wurde)
    if (opt_.shot.empty() && opt_.width == 0 && (settings_.windowWidth != appliedWinW_ || settings_.windowHeight != appliedWinH_)) {
        window_.setClientSize(settings_.windowWidth, settings_.windowHeight);
        appliedWinW_ = settings_.windowWidth, appliedWinH_ = settings_.windowHeight;
    }
    if (settings_.autoQuality && settings_.calibratedFor.empty() && !autoTune_.calibrating() && !testRun())
        autoTune_.requestCalibration();
    lastWinMode_ = window_.displayMode();
    if (session_) {
        if (!coopClient_) session_->player().walkSpeed = settings_.walkSpeed;  // V13.2: der Gast laeuft wie seine Figur beim Host
        session_->player().bobEnabled = settings_.bob;
        session_->world().setLoadRadius(settings_.viewDistance);
    }
    settings_.save();
}

void App::playSound(const std::string& name, float volume) {
    if (audio_) audio_->play(name, volume, 1.0f, audio::Bus::Ui);
}

// --- Hauptschleife -----------------------------------------------------------------------------
void App::mainLoop() {
    auto last = Clock::now();
    auto start = last;
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, 0x00000002 /*HIGH_RESOLUTION*/, TIMER_ALL_ACCESS);
    if (!timer) timer = CreateWaitableTimerW(nullptr, TRUE, nullptr);
    while (!quit_) {
        auto frameStart = Clock::now();
        double dt = std::chrono::duration<double>(frameStart - last).count();
        last = frameStart;
        dt = std::min(dt, 0.1);
        time_ = std::chrono::duration<double>(frameStart - start).count();
        if (opt_.fixedDt) {
            static double simTime = 0.0;
            dt = 1.0 / 60.0;
            simTime += dt;
            time_ = simTime;
        }
        if (opt_.quitAfter > 0 && time_ > opt_.quitAfter) quit_ = true;

        input_.beginFrame();
        if (!window_.pump() || window_.closeRequested()) {
            if (!quit_) log::info("Fenster geschlossen (WM_CLOSE/WM_QUIT)");  // V13.3: Grund des Beendens im Protokoll
            quit_ = true;
        }
        input_.pollGamepad();
        if (!opt_.shot.empty() || !opt_.script.empty()) {
            // Aufnahmen/Tests: keine echten Eingaben, nur die des Skripts
            input_.clearState();
            if (!opt_.script.empty()) scriptStep();
        }
        if (window_.consumeResize()) renderer_->resize(window_.width(), window_.height());
        if (window_.displayMode() != lastWinMode_) {
            // Alt+Enter: Einstellung nachziehen, sonst zeigt das Menue den alten Zustand und die
            // naechste Einstellungsaenderung wuerde den Wechsel rueckgaengig machen
            lastWinMode_ = window_.displayMode();
            settings_.fullscreen = lastWinMode_ == plat::DisplayMode::Borderless;
            if (!testRun() && opt_.overrides.size() == 0) settings_.save();
        }
        if (audio_) audio_->watchdog();  // V13.3: Geraet verloren, Strom steht, Thread haengt -> Neustart (AUDIO_BUG.md)
        if (window_.minimized()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            continue;
        }
        frame(dt);
        if (audioLogEvery_ > 0.0 && time_ >= audioLogNext_ && audio_) {  // V13.3 (Tests)
            audioLogNext_ = time_ + audioLogEvery_;
            log::info("Audio-Status: {}", audio_->statsLine());
        }
        // V6: Grafik-Automatik (Messung, dynamische Aufloesung)
        if (state_ != State::Loading && !testRun()) {
            std::string msg = autoTune_.update(dt, settings_, *renderer_, true, renderer_->gpuTimer().totalMs(),
                                               (float)(dt * 1000.0));
            if (!msg.empty()) {
                ui_.toast(msg, 4.0);
                if (session_) session_->world().setLoadRadius(settings_.viewDistance);
                settings_.save();
            }
        }
        if (!scriptShot_.empty()) {
            takeScreenshot(scriptShot_);
            scriptShot_.clear();
        }

        double ft = std::chrono::duration<double>(Clock::now() - frameStart).count();
        if (ft > 0) fps_ += ((float)(1.0 / std::max(ft, 1e-4)) - fps_) * 0.05f;
        // Bildratenbegrenzung (hochaufloesender Timer, Rest aktiv warten)
        int limit = settings_.maxFps;
        if (!window_.focused() && limit == 0) limit = 30;  // im Hintergrund sparsam
        if (limit > 0) {
            double target = 1.0 / limit;
            double remain = target - std::chrono::duration<double>(Clock::now() - frameStart).count();
            if (remain > 0.002 && timer) {
                LARGE_INTEGER due;
                due.QuadPart = -(LONGLONG)((remain - 0.0015) * 1e7);
                SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE);
                WaitForSingleObject(timer, 100);
            }
            while (std::chrono::duration<double>(Clock::now() - frameStart).count() < target) std::this_thread::yield();
        }
    }
    if (timer) CloseHandle(timer);
}

void App::frame(double dt) {
    switch (state_) {
        case State::Loading: {
            bool ready = renderer_->texturesReady();
            ui_.setLoading(ready ? "Erzeuge Welt …" : "Erzeuge Oberflächen und Klänge …", 0);
            render(dt);
            if (ready) {
                renderer_->renderScene({}, {}, {});  // Texturen hochladen
                if (!opt_.saveFile.empty()) {
                    game::SaveEntry e;
                    if (game::savegame::load(opt_.saveFile, e)) loadGame(e);
                    else startTitleWorld();
                } else if (opt_.seed >= 0 || opt_.demo || (!opt_.shot.empty() && opt_.shotUi.rfind("title", 0) != 0)) {
                    newGame(opt_.name.empty() ? "Wanderer" : opt_.name,
                            opt_.difficulty.empty() ? content_.defaultDifficulty() : opt_.difficulty);
                } else {
                    startTitleWorld();
                }
            }
            break;
        }
        case State::Title: updateTitle(dt); break;
        case State::Playing: updatePlaying(dt); break;
    }
}

game::PlayerControl App::gatherControl(double dt) {
    game::PlayerControl c;
    const auto& in = input_;
    c.forward = (in.actionDown(Action::MoveForward) ? 1.0f : 0.0f) - (in.actionDown(Action::MoveBack) ? 1.0f : 0.0f);
    c.side = (in.actionDown(Action::MoveRight) ? 1.0f : 0.0f) - (in.actionDown(Action::MoveLeft) ? 1.0f : 0.0f);
    c.forward += in.leftStick().y;
    c.side += in.leftStick().x;
    c.sprint = in.actionDown(Action::Sprint);
    c.sneak = in.actionDown(Action::Sneak);  // V11
    c.jump = in.actionPressed(Action::Jump);
    c.jumpHeld = in.actionDown(Action::Jump);  // V11: schwimmen
    c.turn =(in.actionDown(Action::LookRight) ? 1.0f : 0.0f) - (in.actionDown(Action::LookLeft) ? 1.0f : 0.0f);
    c.look = (in.actionDown(Action::LookUp) ? 1.0f : 0.0f) - (in.actionDown(Action::LookDown) ? 1.0f : 0.0f);
    const Settings& s = settings_;
    // Maus (wie V4: Empfindlichkeit, Achsenfaktoren, Invertieren, zeitbasierte Glaettung)
    if (s.mouseEnabled && !opt_.noMouse && window_.mouseCaptured()) {
        vec2 d = in.mouseDelta();
        double sx = s.invertX ? -1.0 : 1.0, sy = s.invertY ? -1.0 : 1.0;
        pendYaw_ += d.x * 0.0022 * s.mouseSensitivity * s.mouseX * sx;
        pendPitch_ -= d.y * 0.0017 * s.mouseSensitivity * s.mouseY * sy;
    }
    double a = s.mouseSmoothing <= 0.001f ? 1.0 : 1.0 - std::exp(-dt / (s.mouseSmoothing * 0.07));
    c.yawDelta = (float)(pendYaw_ * a);
    c.pitchDelta = (float)(pendPitch_ * a);
    pendYaw_ -= pendYaw_ * a;
    pendPitch_ -= pendPitch_ * a;
    // Gamepad: rechter Stick
    vec2 rs = in.rightStick();
    c.yawDelta += (float)(rs.x * std::fabs(rs.x) * 2.8 * s.padSensitivity * dt);
    c.pitchDelta += (float)(rs.y * std::fabs(rs.y) * 1.9 * s.padSensitivity * dt);
    return c;
}

// Test: Oberflaeche fuer automatische Aufnahmen vorbereiten
static void prepareShotUi(const std::string& what, ui::Ui& ui, game::GameSession* s) {
    if (what == "pause") ui.openPause();
    else if (what == "graphics" || what == "mouse" || what == "audio") {
        ui.openPause();
        ui.openPage(what);
    } else if (what == "help") ui.openHelp();
    else if (what == "inventory" && s) {
        for (int i = 0; i < 5; ++i) s->inventory().add("energy");
        for (int i = 0; i < 3; ++i) s->inventory().add("almond");
        s->inventory().swap(1, 12);
        ui.setMode(ui::Mode::Inventory);
    } else if (what == "dead" && s) {
        s->stats().sanity = 0;
        s->stats().zeroTime = s->stats().deathTime;
        s->stats().update(0.01);
    } else if (what == "insane" && s) {
        s->stats().sanity = 0;
        s->stats().zeroTime = s->stats().deathTime * 0.6;
        s->stats().red = 0.8;
    } else if (what == "hud" && s) {
        s->inventory().add("energy");
        s->inventory().add("almond");
        s->stats().sanity = 42;
        ui.toast("Seltener Fund!  Energy Bar aufgenommen", 60);
    } else if (what == "map" && s) {
        ui.toggleMap();  // V6: grosse Karte
    } else if (what == "use" && s) {
        s->inventory().add("almond");
        s->useSlot(0);  // V6: Geste "benutzen" (Hand fuehrt das Item zum Mund)
    } else if (what.rfind("title-", 0) == 0) ui.openPage(what.substr(6));
}

void App::updateTitle(double dt) {
    if (!opt_.shotUi.empty() && ++framesInPlay_ == 3) prepareShotUi(opt_.shotUi, ui_, nullptr);
    if (!opt_.shot.empty() && framesInPlay_ >= opt_.shotFrames) {
        render(dt);
        takeScreenshot(opt_.shot);
        quit_ = true;
        return;
    }
    if (coopClient_) {  // V13.2 Koop: Beitritt abwarten
        coopJoinPump(dt);
        if (state_ == State::Playing) return;
    }
    // langsame Kamerafahrt im Hintergrund
    session_->player().angle += dt * 0.06;
    session_->update(dt, {}, true);
    session_->events().clear();
    renderer_->syncMaterials(session_->world().bank());
    window_.setMouseCaptured(false);
    render(dt);
}

// V11: Wiedereinstieg am letzten Sicherungspunkt
bool App::canRespawn() const { return session_ && session_->world().level().mapped(); }

void App::respawn() {
    if (!session_) return;
    session_->respawnAtCheckpoint();
    ui_.setMode(ui::Mode::Hud);
    ui_.clearFadeOut();
    ui_.fadeFromBlack(1.8);
    scareT_ = -1.0, scareCut_ = false, deathDelay_ = 0.0, hitFlash_ = 0.0f;
    audio_->setMuffle(0.0f);
    resetHistory_ = true;
    ui_.toast(std::format("Zurück · {}", session_->checkpointLabel()), 3.0);
}

void App::updatePlaying(double dt) {
    // V11: Schreck - die erste Sekunde gehoert dem Gesicht, dann harter Schnitt; der Todesschirm folgt
    if (scareT_ >= 0.0) {
        scareT_ += dt;
        if (scareT_ < 0.3) resetHistory_ = true;  // schnelle Bewegung ohne Nachziehen (TAA)
        if (!scareCut_ && scareT_ >= 1.0) {
            scareCut_ = true;
            ui_.fadeToBlack(0.03);
            audio_->setMuffle(0.75f);
        }
    }
    if (deathDelay_ > 0.0) deathDelay_ -= dt;
    // V7: Notausgang - nach dem Abblenden die naechste Ebene laden
    if (exitTimer_ > 0.0) {
        exitTimer_ -= dt;
        if (exitTimer_ <= 0.0) {
            changeLevel(pendingLevel_, pendingAnchor_);
            return;
        }
    }
    // V9: die letzte Fahrt - nach dem Weiss der Abspann
    if (endingTimer_ > 0.0) {
        endingTimer_ -= dt;
        if (endingTimer_ <= 0.0) {
            ui_.showEnding();
            if (opt_.shot.empty() && opt_.benchmark == 0) saveGame();
        }
    }
    if (coop()) {  // V13.2 Koop: Netz lesen; bei Verbindungsende zum Titel
        coopBefore(dt);
        if (!session_ || state_ != State::Playing) return;
    }
    game::GameSession& s = *session_;
    ++framesInPlay_;
    if (!opt_.shotUi.empty() && framesInPlay_ == 3) {
        prepareShotUi(opt_.shotUi, ui_, &s);
        if (!opt_.script.empty()) opt_.shotUi.clear();  // Skript: nur einmal (nicht erneut nach dem Laden)
    }
    // Fenster verlassen (Alt+Tab, Klick daneben): Spiel pausieren, damit die Sanity nicht im
    // Hintergrund weiter sinkt
    if (!window_.focused() && ui_.mode() == ui::Mode::Hud && (!testRun() || simFocus_) && !pilot_)
        ui_.openPause();
    bool paused = ui_.pausesGame() && !coop();  // V13.2: im Koop laeuft die Welt weiter (Menues bremsen nur die Eingabe)
    // Spieltasten (nur ohne offenes Menue)
    if (ui_.mode() == ui::Mode::Hud) {
        if (pilot_ && input_.anyKeyPressed()) {
            pilot_.reset();
            ui_.toast("Steuerung übernommen");
        }
        if (input_.actionPressed(Action::Pause)) ui_.openPause();
        else if (input_.actionPressed(Action::Inventory)) ui_.setMode(ui::Mode::Inventory);
        else if (input_.actionPressed(Action::Help)) ui_.openHelp();
        if (input_.actionPressed(Action::Interact)) s.interact();
        if (input_.actionPressed(Action::UseItem)) s.useSlot(s.inventory().selected);
        // V6: Q legt ein Item des gewaehlten Platzes ab, Strg+Q den ganzen Stapel
        if (input_.actionPressed(Action::DropItem)) s.dropSlot(s.inventory().selected, input_.down(VK_CONTROL));
        if (input_.actionPressed(Action::Flashlight)) s.toggleFlashlight();  // V7
        for (int k = 0; k < 9; ++k)
            if (input_.actionPressed((Action)((int)Action::Hotbar1 + k))) s.selectSlot(k);
        if (input_.actionPressed(Action::HotbarNext)) s.selectSlot((s.inventory().selected + 1) % 9);
        if (input_.actionPressed(Action::HotbarPrev)) s.selectSlot((s.inventory().selected + 8) % 9);
        if (input_.actionPressed(Action::Map)) {
            ui_.toggleMap();
            playSound(ui_.mapOpen() ? "ui_select" : "ui_back", 0.6f);
        }
        if (input_.actionPressed(Action::ToggleCamera)) {
            settings_.thirdPerson = !settings_.thirdPerson;
            settingsChanged();
            ui_.toast(settings_.thirdPerson ? "Kamera: Schulterperspektive" : "Kamera: Ego-Perspektive");
        }
        if (input_.actionPressed(Action::Debug)) ui_.toggleDebug();
        if (input_.actionPressed(Action::ToggleBob)) {
            settings_.bob = !settings_.bob;
            settingsChanged();
            ui_.toast(std::string("Head-Bobbing ") + (settings_.bob ? "an" : "aus"));
        }
        if (input_.actionPressed(Action::SpeedUp) || input_.actionPressed(Action::SpeedDown)) {
            float d = input_.actionPressed(Action::SpeedUp) ? 0.5f : -0.5f;
            settings_.walkSpeed = std::clamp(settings_.walkSpeed + d, 1.0f, 8.0f);
            settingsChanged();
            ui_.toast(std::format("Laufgeschwindigkeit {:.1f} m/s", settings_.walkSpeed));
        }
        if (input_.pressed('V')) {
            static const char* names[] = {"Modern", "Terminal", "ASCII", "Monochrom"};
            settings_.render.displayMode = (settings_.render.displayMode + 1) % 4;
            settingsChanged();
            ui_.toast(std::string("Darstellung: ") + names[settings_.render.displayMode]);
        }
        if (input_.pressed('L')) {
            bool fast = settings_.render.renderScale > 0.55f;
            settings_.render.renderScale = fast ? 0.5f : 1.0f;
            settings_.quality = 4;
            settingsChanged();
            ui_.toast(std::string("Leistungsmodus ") + (fast ? "an" : "aus"));
        }
    }
    game::PlayerControl ctl;
    if (pilot_ && !paused) {
        struct Adapter : phys::TileSource {
            world::World& w;
            explicit Adapter(world::World& ww) : w(ww) {}
            const world::Tile& at(i64 x, i64 y) override { return w.tile(x, y); }
        } ad(s.world());
        ctl = pilot_->step(dt, s.player(), ad);
    } else if (ui_.mode() == ui::Mode::Hud) {
        ctl = gatherControl(dt);
    }
    auto tSim = Clock::now();
    s.hallucinations = settings_.hallucinations;  // V7
    s.figureMode = pilot_ ? 0 : settings_.figures;  // V10 (nicht in der Demo)
    s.figureDetail = std::max(0, settings_.render.figureDetail - autoTune_.figureBias());  // bei Last weniger
    s.realisticBody = settings_.realisticBody;
    s.update(dt, ctl, paused);
    simMs_ = std::chrono::duration<double, std::milli>(Clock::now() - tSim).count();
    renderer_->syncMaterials(s.world().bank());
    handleEvents();
    if (coop()) {  // V13.2 Koop: senden, Gruppe, Ebenenwechsel; Tod: zusehen statt Todesschirm
        if (!coopAfter(dt, ctl)) return;
        if (s.stats().dead) {
            if (scareT_ < 0.0 && !coopWasDead_) ui_.toast("Du bist tot. Die anderen spielen weiter – du kommst am Sicherungspunkt zurück.", 4.0);
            if (scareCut_ && scareT_ > 1.6) {  // nach dem Schnitt wieder hinsehen (Zuschauen)
                ui_.clearFadeOut();
                ui_.fadeFromBlack(0.8);
                audio_->setMuffle(0.0f);
                scareT_ = -1.0, scareCut_ = false;
            }
            if (input_.pressed(input::MOUSE_LEFT) && ui_.mode() == ui::Mode::Hud) {
                if (coopHost_) coopHost_->cycleSpectate();
                else coopClient_->cycleSpectate();
            }
        } else if (coopWasDead_) {  // zurueck
            scareT_ = -1.0, scareCut_ = false, deathDelay_ = 0.0, hitFlash_ = 0.0f;
            ui_.clearFadeOut();
            ui_.fadeFromBlack(1.0);
            audio_->setMuffle(0.0f);
            resetHistory_ = true;
            ui_.toast(std::format("Zurück · {}", s.checkpointLabel()), 3.0);
        }
        coopWasDead_ = s.stats().dead;
    }
    if (s.stats().dead && !coop() && deathDelay_ <= 0.0 && ui_.mode() != ui::Mode::Dead && ui_.mode() != ui::Mode::Title) {
        ui_.setMode(ui::Mode::Dead);
        log::info("Todesschirm (Schreck {:.2f} s, Ursache {})", scareT_, s.deathCause().empty() ? "Verstand" : s.deathCause());
    }
    window_.setMouseCaptured(ui_.wantsMouse() && window_.focused() && !opt_.noMouse);
    updateAudio(dt);
    if (input_.actionPressed(Action::Screenshot)) {
        render(dt);
        std::time_t t = std::time(nullptr);
        std::tm tm{};
        localtime_s(&tm, &t);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
        takeScreenshot(paths::join(paths::screenshotDir(), std::format("liminal_{}_{}.png", s.seed, buf)));
        return;
    }
    auto tRender = Clock::now();
    render(dt);
    renderMs_ = std::chrono::duration<double, std::milli>(Clock::now() - tRender).count();
    if (!opt_.shot.empty() && framesInPlay_ >= opt_.shotFrames && renderer_->texturesReady()) {
        takeScreenshot(opt_.shot);
        quit_ = true;
    }
}

// V11: Klang des Untergrunds (Varianten-Gruppe der erzeugten Klaenge)
static const char* stepSound(game::Surface s) {
    static const char* names[] = {"step_carpet", "step_concrete", "step_tile", "step_metal", "step_wood", "step_puddle"};
    const int i = (int)s;
    return i >= 0 && i < 6 ? names[i] : "step_concrete";
}

void App::handleEvents() {
    auto& evs = session_->events();
    // V11: Klaenge mit Ort nehmen den Weg des Schalls (durch Tueren, um Ecken), nicht den durch die Wand
    const auto& pl = session_->player();
    const vec3 ear{(float)pl.x, (float)pl.y, (float)pl.eye()};
    // V13.3: Hoerer vor den Klaengen dieses Bildes setzen - play3d verwirft Quellen ausser Reichweite, und nach einem
    // Ebenenwechsel/Wiedereinstieg stand der Hoerer sonst noch an der alten Stelle (falsches Culling im ersten Bild)
    audio_->setListener(ear, (float)pl.angle);
    auto at = [&](const std::string& name, vec3 pos, float volume, float radius, float pitch = 1.0f) {
        audio::PlayOpts o;
        o.volume = volume, o.pitch = pitch, o.vary = false;
        audio_->play3d(name, soundscape_.spatial(session_->world(), ear, pos, radius), o);
    };
    // V13.3 Level 3 (gameplay/kin_l3.hpp): Satz oder Waffenklang; nah genug mit Untertitel, beim ersten Mal der Rat des Wikis
    auto kinSay = [&](const game::KinLine& L, vec3 pos, float pitch) {
        at(L.sound, pos, L.volume, L.radius, pitch);
        if (!L.sub || length(pos - ear) > L.radius * 0.6f) return;
        ui_.toast(L.sub, L.subSec);
        static std::set<std::string> hinted;
        if (L.hint && hinted.insert(L.hint).second) ui_.toast(L.hint, 5.5f);
    };
    for (const auto& e : evs) {
        switch (e.type) {
            case game::GameEvent::Toast: ui_.toast(e.text, e.duration); break;
            case game::GameEvent::ZoneTitle: ui_.zoneTitle(e.text); break;
            case game::GameEvent::Pickup:
                ui_.toast(e.text, e.duration);
                audio_->play("pickup", 0.9f);
                if (e.rare) audio_->play("pickup_rare", 0.8f);  // V6: seltener Fund klingt besonders
                break;
            case game::GameEvent::InventoryFull: ui_.toast(e.text, e.duration); break;
            case game::GameEvent::Consume:
                if (!e.text.empty()) ui_.toast(e.text, e.duration);  // V13.3: das Beil schwingt ohne Meldung
                if (!e.sound.empty()) audio_->play(e.sound, 0.9f);
                break;
            case game::GameEvent::Footstep: {
                // V11: Varianten waehlt das Audiosystem (nie zweimal dieselbe), es streut Tonhoehe und Pegel
                audio::PlayOpts o;
                o.volume = 0.35f + 0.45f * e.intensity;
                audio_->play2d(stepSound(e.surface), o);
                break;
            }
            case game::GameEvent::Land: audio_->play("land", 0.4f + 0.6f * e.intensity); break;
            case game::GameEvent::Exhausted:
                // nur die ersten Male als Hinweis, danach genuegen Keuchen und Anzeige
                if (exhaustHints_++ < 2) ui_.toast(e.text, e.duration);
                audio_->play("gasp", 0.7f);
                break;
            case game::GameEvent::Drop:
                ui_.toast(e.text, e.duration);
                audio_->play("jump", 0.35f, 1.25f);  // kurzes Rascheln beim Wurf
                break;
            case game::GameEvent::ItemLand:
                at("item_land", e.pos, 0.35f + 0.65f * e.intensity, 12.0f, 0.9f + 0.2f * hash01((u32)(e.pos.x * 97.0f)));
                break;
            case game::GameEvent::Jump:
                // Abdruck: Schritt auf dem Untergrund plus leises Kleiderrascheln
                audio_->play(stepSound(e.surface), 0.6f, 1.08f);
                audio_->play("jump", 0.55f);
                break;
            case game::GameEvent::Died:
                // V11: nach dem Schreck erst der Schnitt ins Schwarz, dann (2,4 s) der Todesschirm
                if (scareT_ < 0.0) audio_->play("death", 1.0f);
                if (scareT_ < 0.0) deathDelay_ = 1.2;
                break;
            case game::GameEvent::Checkpoint:  // V11: sicherer Ort - gespeichert
                ui_.toast(std::format("Sicherer Ort · {}", e.text), e.duration);
                at("lamp_start", e.pos, 0.45f, 10.0f);
                saveGame();
                break;
            case game::GameEvent::AutosaveDue: saveGame(); break;
            // V7
            case game::GameEvent::FlashToggle: audio_->play(e.intensity > 0.5f ? "flash_on" : "flash_off", 0.55f); break;
            case game::GameEvent::NoteRead:
                ui_.openNote(e.index);
                audio_->play("paper", 0.7f);
                break;
            case game::GameEvent::ExitOpened:
                if (exitTimer_ <= 0.0 && !e.text.empty()) {
                    pendingLevel_ = e.text;
                    pendingAnchor_ = e.anchor;
                    exitTimer_ = 1.15;
                    ui_.fadeToBlack(1.0);
                    audio_->play(e.sound.empty() ? "exit_door" : e.sound, 0.9f);  // V11: Sturz ins Loch klingt anders
                }
                break;
            case game::GameEvent::FlashEmpty:
                ui_.toast(e.text, e.duration);
                audio_->play("flash_off", 0.4f, 0.8f);
                break;
            // V7: Halluzinationen
            case game::GameEvent::PhantomStep: {
                static u32 n = 0;
                ++n;
                at(stepSound(e.surface), e.pos, 0.6f, 16.0f, 0.9f + 0.06f * hash01(n * 29u));
                break;
            }
            case game::GameEvent::OtherStep: {  // V13.2 Koop: Schritte eines Mitspielers
                static u32 n = 0;
                ++n;
                at(stepSound(e.surface), e.pos, 0.35f + 0.45f * e.intensity, 18.0f, 0.95f + 0.08f * hash01(n * 31u));
                break;
            }
            case game::GameEvent::Blackout:
                if (e.index == 1) at("power_off", e.pos, 0.75f, 20.0f);
                else if (e.index == 0) at("power_on", e.pos, 0.6f, 20.0f);
                else if (e.index == 3) at("breaker_far", e.pos, 1.0f, 90.0f);  // V11: Welle, ein Schuetz faellt
                else at("lamp_tick", e.pos, 0.6f, 30.0f, 0.85f);
                break;
            case game::GameEvent::GhostSeen: audio_->play("hal_whisper", 0.5f); break;
            // V8: eine neue Ebene - erst faellt hinten eine Tuer zu, viel spaeter kennt man ihren Namen
            case game::GameEvent::DistantSlam: at("dist_door", e.pos, 0.85f, 70.0f, 0.88f); break;
            case game::GameEvent::SoundAt:
                if (const game::KinLine* L = game::kinLineBySound(e.sound)) {  // V13.3 Level 3 (Gast): Satz mit Untertitel
                    kinSay(*L, e.pos, 1.0f);
                    break;
                }
                at(e.sound, e.pos, e.intensity, 18.0f);
                break;  // V8: Aufzug
            case game::GameEvent::TeleportCut: resetHistory_ = true; break;
            // V9: Sicherungen, die Entities (V10), die letzte Fahrt
            case game::GameEvent::FuseTaken:
                ui_.toast(e.text, e.duration);
                at("fuse_take", e.pos, 0.95f, 16.0f);
                audio_->play("pickup_rare", 0.55f);
                break;
            case game::GameEvent::StalkerAppear: break;  // das Brummen setzt leise ein (updateAudio)
            case game::GameEvent::StalkerStep: {
                static u32 n = 0;
                ++n;
                at("hound_step", e.pos, 0.8f, 28.0f, 0.86f + 0.12f * hash01(n * 31u + 7u));
                break;
            }
            // V11: das Laecheln kuendigt sich an (es oeffnet sich), der Hund knurrt - kein Schreckklang
            case game::GameEvent::StalkerSeen:
                if (e.role == (int)game::Presence::Role::Smiler) at("smiler_open", e.pos + vec3{0, 0, 1.4f}, 0.8f, 30.0f);
                else at("hound_growl", e.pos + vec3{0, 0, 0.6f}, 0.75f, 26.0f);
                break;
            case game::GameEvent::StalkerHit:
                // V11: toedlicher Zugriff - der Schreck: Schrei, weisser Blitz, das Gesicht in der Kamera,
                // nach einer Sekunde der harte Schnitt ins Schwarz (updatePlaying)
                if (e.role == (int)game::Presence::Role::Clump) {  // V11: Buendel - nasser Griff, Knacken, Brüllen tiefer
                    audio_->play("clump_grab", 1.0f, 1.0f, audio::Bus::Effects);
                    audio_->play("scare_hound", 0.85f, 0.78f, audio::Bus::Effects);
                } else if (e.role == (int)game::Presence::Role::Duller) {  // V13.3: Griff durch die Wand, Beton bricht, Schleifen
                    audio_->play("duller_grab", 1.0f, 1.0f, audio::Bus::Effects);
                } else if (e.role == (int)game::Presence::Role::Arms) {  // V13.3: Krallen, dann Schlaege im Schacht, die sich entfernen
                    audio_->play("arms_take", 1.0f, 1.0f, audio::Bus::Effects);
                } else if (e.role == (int)game::Presence::Role::Wrangler) {  // V13.3: aus dem Gang geschnappt
                    audio_->play("wrangler_snatch", 1.0f, 1.0f, audio::Bus::Effects);
                } else if (e.role == (int)game::Presence::Role::Growler) {  // V13.3: Drahtspitzen, Rauschen
                    audio_->play("growler_impale", 1.0f, 1.0f, audio::Bus::Effects);
                    audio_->play("scare_hound", 0.7f, 0.66f, audio::Bus::Effects);
                } else if (e.role == (int)game::Presence::Role::Nguithr) {  // V13.3: frisst bei lebendigem Leib
                    audio_->play("ngui_feed", 1.0f, 1.0f, audio::Bus::Effects);
                    audio_->play("scare_smiler", 0.6f, 1.25f, audio::Bus::Effects);
                } else if (e.role == (int)game::Presence::Role::Jerry) {  // V13.3 Teil 4: ganz nah gesprochen, Schwingen, Herzschlag
                    audio_->play("jerry_subdue", 1.0f, 1.0f, audio::Bus::Effects);
                } else {
                    audio_->play(e.role == (int)game::Presence::Role::Smiler ? "scare_smiler" : "scare_hound", 1.0f, 1.0f, audio::Bus::Effects);
                }
                hitFlash_ = 1.0f;
                scareT_ = 0.0;
                scareCut_ = false;
                deathDelay_ = 2.4;
                resetHistory_ = true;
                break;
            case game::GameEvent::StalkerGone: at("dist_door", e.pos + vec3{0, 0, 1.5f}, 0.45f, 60.0f, 0.8f); break;
            // V10: die Entities - jede klingt anders
            case game::GameEvent::Figure: {
                using PE = game::Presence::Event;
                using Role = game::Presence::Role;
                static u32 n = 0;
                ++n;
                const Role role = (Role)e.role;
                const float var = hash01(n * 31u + 7u);
                if (const game::KinLine* L = game::kinLine(game::kinCueOf(e.intensity, e.role))) {  // V13.3 Level 3: Klangnummer
                    kinSay(*L, e.pos, L->sub ? 1.0f : 0.96f + 0.08f * var);
                    break;
                }
                switch ((PE::Type)e.index) {
                    case PE::Step:  // Hound: flache Handflaechen und Krallen auf dem Boden (ein Klang, Varianten)
                        if (role == Role::Hound) at("hound_step", e.pos, e.intensity * 0.9f, 22.0f, 0.95f + 0.1f * var);
                        // V11 Buendel: nasse Haende klatschen auf den Boden
                        if (role == Role::Clump)
                            at(std::format("clump_slap_{}", n % 5), e.pos, 0.35f + 0.4f * e.intensity, 16.0f, 0.92f + 0.16f * var);
                        // V13.2: Zweibeiner - Schuhe nach Untergrund (Faceling), schwer (Skin-Stealer: barfuss, tiefer), der
                        // Wretch schlurft (das schleifende Bein leiser, gedehnt); Ratten trippeln kaum hoerbar
                        if (role == Role::Faceling) at(stepSound(e.surface), e.pos, 0.42f * e.intensity + 0.1f, 18.0f, 0.97f + 0.08f * var);
                        if (role == Role::SkinStealer) at("hound_step", e.pos, 0.55f + 0.3f * e.intensity, 22.0f, 0.62f + 0.06f * var);
                        if (role == Role::Wretch) at(stepSound(e.surface), e.pos, 0.3f + 0.4f * e.intensity, 18.0f, e.intensity < 0.5f ? 0.7f : 0.9f + 0.1f * var);
                        if (role == Role::Rat) at("hound_step", e.pos, 0.08f, 6.0f, 2.2f + 0.3f * var);
                        if (role == Role::Duller) at("duller_step", e.pos, 0.3f + 0.3f * e.intensity, 14.0f, 0.95f + 0.1f * var);  // V13.3
                        // V13.3: Growler - dumpfer, weicher Aufschlag, Draht kratzt, das Gewirr raschelt nach
                        if (role == Role::Growler) at(std::format("growler_drag_{}", n % 3), e.pos, 0.35f + 0.5f * e.intensity, 22.0f, 0.88f + 0.16f * var);
                        // V13.3: Ouroboros-Arbeiter - schwere Gummistiefel
                        if (role == Role::Ouroboros) at(std::format("ouro_step_{}", n % 3), e.pos, 0.3f + 0.35f * e.intensity, 18.0f, 0.92f + 0.12f * var);
                        // V13.3: Plague Goblins - Krallen tickern auf dem Boden, der Umhang streift nach
                        if (role == Role::Goblin) at(std::format("goblin_step_{}", n % 3), e.pos, 0.18f + 0.35f * e.intensity, 12.0f, 0.94f + 0.14f * var);
                        break;
                    // V11 Buendel: Klopfen im Schacht, Herabfallen, Schleifen, Ausholen
                    case PE::Knock:
                        if (role == Role::Moth) at(std::format("moth_tap_{}", n % 3), e.pos, 0.5f, 9.0f, 0.9f + 0.2f * var);  // V13.3 Teil 4: ans Glas der Roehre
                        else at(std::format("clump_knock_{}", n % 3), e.pos, 0.9f, 30.0f);
                        break;
                    case PE::Drop:
                        if (role == Role::Arms) {  // V13.3: die Krallen schnappen ins Leere
                            at("arms_snap", e.pos, 0.9f, 16.0f, 0.95f + 0.1f * var);
                            break;
                        }
                        at(std::format("clump_slap_{}", n % 5), e.pos, 1.0f, 26.0f, 0.8f);
                        at(std::format("clump_crack_{}", n % 3), e.pos, 0.8f, 22.0f);
                        break;
                    case PE::Drag: at(std::format("clump_drag_{}", n % 2), e.pos, 0.55f, 18.0f, 0.9f + 0.2f * var); break;
                    case PE::Windup: at(role == Role::Arms ? "arms_hiss" : "clump_reach", e.pos, role == Role::Arms ? 0.8f : 1.0f, 20.0f); break;
                    case PE::Breath: at("hound_pant", e.pos + vec3{0, 0, 0.5f}, 0.75f, 12.0f); break;
                    case PE::Growl: at("hound_growl", e.pos + vec3{0, 0, 0.5f}, 0.45f + 0.5f * e.intensity, 22.0f); break;
                    case PE::Whine: at("hound_whine", e.pos + vec3{0, 0, 0.5f}, 0.6f, 26.0f); break;
                    case PE::Bark: at("hound_bark", e.pos + vec3{0, 0, 0.5f}, 0.85f, 34.0f); break;
                    // das Laecheln kommt naeher: nasses Klicken von Zaehnen im Dunkeln
                    case PE::Rustle: at("smiler_teeth", e.pos + vec3{0, 0, 1.4f}, 0.35f + 0.4f * e.intensity, 14.0f); break;
                    case PE::Stare: audio_->play("smiler_teeth", 0.25f, 0.85f); break;
                    case PE::Gone:
                        if (role == Role::Smiler) at("smiler_fade", e.pos + vec3{0, 0, 1.4f}, 0.6f, 20.0f);
                        break;
                    case PE::TakeOff: at("moth_takeoff", e.pos, 0.8f, 20.0f); break;
                    case PE::Sting:
                        audio_->play("moth_sting", 0.8f);
                        hitFlash_ = std::max(hitFlash_, 0.3f);
                        break;
                    case PE::Flicker: at("lamp_tick", e.pos, 0.7f, 30.0f, 0.75f + 0.1f * var); break;
                    case PE::Vision: audio_->play("gestalt_sight", 0.4f); break;
                    // V11 (F2): das Laecheln oeffnet sich - Ankuendigung des Zugriffs; beruhigt: es verklingt
                    case PE::Announce:
                        if (role == Role::Jerry) {  // V13.3 Teil 4: Jerry breitet die Fluegel aus und kreischt - letzte Warnung
                            at(std::format("jerry_screech_{}", n % 2), e.pos, 1.0f, 40.0f);
                            at("jerry_flap_1", e.pos, 0.9f, 24.0f);
                        } else {
                            at("smiler_open", e.pos, 1.0f, 30.0f);
                        }
                        break;
                    case PE::Calm:
                        if (role == Role::Wrangler) at("wrangler_settle", e.pos, 0.8f, 30.0f);  // V13.3: es hat einen verloren
                        else at("smiler_fade", e.pos, 0.4f, 20.0f);
                        break;
                    case PE::Lost: break;
                    // V13.2: neue Wesen
                    case PE::Hit:
                        if (role == Role::Moth) at("moth_bite", e.pos, 0.85f, 10.0f, 0.95f + 0.1f * var);  // V13.3 Teil 4: Weibchen beisst
                        else at(std::format("hit_body_{}", n % 2), e.pos, 0.9f, 14.0f, 0.9f + 0.15f * var);
                        hitFlash_ = std::max(hitFlash_, std::clamp(e.intensity / 40.0f, 0.2f, 0.7f));  // nur eigene Treffer kommen hier an
                        break;
                    case PE::Mimic:
                        if (role == Role::Goblin) {  // V13.3: die Torwaechterin versucht ein Wort (heiser, gekraechzt)
                            at(std::format("goblin_mimic_{}", n % 2), e.pos, 0.85f, 18.0f, 0.97f + 0.06f * var);
                            break;
                        }
                        at(std::format("skin_mimic_{}", n % 6), e.pos, 0.55f + 0.3f * e.intensity, 26.0f, 0.97f + 0.06f * var);
                        break;
                    // V13.3: Plague Goblins - Kraechzen/Schnattern, Rucksack, Aufheben, Drohen, Sprung, Portal, Tod
                    case PE::Croak:
                        if (e.intensity < 0.25f) at(std::format("goblin_chatter_{}", n % 3), e.pos, 0.5f, 16.0f, 0.93f + 0.14f * var);
                        else if (e.intensity < 0.75f) at(std::format("goblin_croak_{}", n % 3), e.pos, 0.8f, 26.0f, 0.94f + 0.12f * var);
                        else at(std::format("goblin_squawk_{}", n % 2), e.pos, 1.0f, 42.0f, 0.96f + 0.08f * var);
                        break;
                    case PE::Steal:
                        if (e.intensity < 0.25f) at("goblin_rummage", e.pos, 0.95f, 6.0f);
                        else if (e.intensity < 0.75f) at(std::format("goblin_chatter_{}", n % 3), e.pos, 0.8f, 12.0f, 1.1f);
                        else at("goblin_stash", e.pos, 0.9f, 8.0f);
                        break;
                    case PE::Take: at("goblin_stash", e.pos, 0.6f, 10.0f, 0.95f + 0.1f * var); break;
                    case PE::Hiss:
                        if (role == Role::Moth) at("moth_buzz", e.pos, 0.6f + 0.4f * e.intensity, 12.0f);  // V13.3 Teil 4: gereizte Motten um den Kopf
                        else at("goblin_hiss", e.pos, 1.0f, 24.0f);
                        break;
                    case PE::Screech:
                        if (role == Role::Jerry) at(std::format("jerry_screech_{}", n % 2), e.pos, e.intensity > 0.7f ? 1.0f : 0.8f, 40.0f, 0.96f + 0.08f * var);  // V13.3 Teil 4
                        else at(std::format("goblin_screech_{}", n % 2), e.pos, 1.0f, 28.0f, 0.96f + 0.08f * var);
                        break;
                    case PE::Portal: at(e.intensity > 0.5f ? "goblin_portal_open" : "goblin_portal_close", e.pos, 1.0f, 46.0f); break;
                    case PE::Perish: at("goblin_death", e.pos, 1.0f, 30.0f); break;
                    case PE::Whisper: at(std::format("window_whisper_{}", n % 4), e.pos, 0.35f + 0.5f * e.intensity, 12.0f); break;
                    case PE::Plead: at(std::format("wretch_plead_{}", n % 3), e.pos, 0.7f, 20.0f, 0.95f + 0.1f * var); break;
                    case PE::Gurgle:
                        if (e.intensity > 0.5f) at(std::format("wretch_snarl_{}", n % 3), e.pos, 0.8f, 30.0f, 0.95f + 0.1f * var);
                        else at(std::format("wretch_gurgle_{}", n % 3), e.pos, 0.75f, 24.0f, 0.9f + 0.1f * var);
                        break;
                    case PE::Hum:
                        if (e.intensity > 0.75f) at(std::format("faceling_outburst_{}", n % 2), e.pos, 0.8f, 24.0f, 0.95f + 0.1f * var);
                        else at(std::format("faceling_hum_{}", n % 4), e.pos, 0.4f + 0.4f * e.intensity, 14.0f, 0.95f + 0.1f * var);
                        break;
                    case PE::Squeak:
                        if (role == Role::Wrangler) at(std::format("wrangler_young_{}", n % 4), e.pos, 0.3f + 0.6f * e.intensity, 12.0f, 0.95f + 0.1f * var);  // V13.3
                        else at(std::format("rat_squeak_{}", n % 4), e.pos, 0.25f + 0.5f * e.intensity, 12.0f, 0.95f + 0.1f * var);
                        break;
                    case PE::Scream: at("rat_scream", e.pos, 1.0f, 40.0f); break;
                    case PE::Swing: at("stone_swing", e.pos, 0.8f, 12.0f, 0.95f + 0.1f * var); break;
                    case PE::Pull: at("window_pull", e.pos, 0.9f, 14.0f); break;
                    // V13.3: Duller (Schaben in der Wand, Putz, Durchbruch), Arms (Schaben im Schacht, Lueftung, Schrei im Licht)
                    case PE::Scrape:
                        if (role == Role::Duller) at(std::format("duller_scrape_{}", n % 4), e.pos, 0.35f + 0.55f * e.intensity, 12.0f, 0.95f + 0.1f * var);
                        else at(std::format("arms_scrape_{}", n % 4), e.pos, 0.3f + 0.35f * e.intensity, 14.0f, 0.95f + 0.1f * var);
                        break;
                    case PE::Bulge: at(std::format("duller_bulge_{}", n % 2), e.pos, 0.6f + 0.4f * e.intensity, 14.0f); break;
                    case PE::Burst:
                        if (role == Role::Duller) at("duller_burst", e.pos, 1.0f, 30.0f);
                        else if (role == Role::Wrangler) at(std::format("wrangler_burst_{}{}", e.intensity > 0.75f ? 'm' : 'f', n % 2), e.pos, 1.0f, 40.0f);  // V13.3
                        // V13.3 Teil 4: Fensterwesen in Level 2 - es bildet sich (Glas, Rahmen, Aussenluft) / ferner Wind und Voegel
                        else if (role == Role::Window && e.intensity > 0.5f) at("window_form", e.pos, 0.9f, 26.0f);
                        else if (role == Role::Window) at(std::format("window_air_{}", n % 3), e.pos, 0.55f, 22.0f, 0.96f + 0.08f * var);
                        else at("arms_vent", e.pos, 0.35f, 9.0f);
                        break;
                    case PE::Burn: at("arms_burn", e.pos, 1.0f, 30.0f); break;
                    case PE::Retract: at(role == Role::Duller ? "duller_retract" : "arms_retract", e.pos, 0.7f * e.intensity + 0.2f, 16.0f); break;
                    // V13.3: Wrangler - Stoesse in der Tiefe, Durchbruch, Gleiten und Stoehnen, Ankuendigung, Versinken
                    case PE::Tremor: at(std::format("wrangler_tremor_{}", n % 3), e.pos, 0.45f + 0.55f * e.intensity, 60.0f, 0.92f + 0.12f * var); break;
                    case PE::Breach: at(std::format("wrangler_breach_{}", n % 2), e.pos, 1.0f, 50.0f); break;
                    case PE::Slither:
                        if (e.intensity > 0.75f) at(std::format("wrangler_groan_{}", n % 3), e.pos, 0.9f, 60.0f, 0.95f + 0.08f * var);
                        else at(std::format("wrangler_squelch_{}", n % 4), e.pos, 0.6f, 22.0f, 0.9f + 0.2f * var);
                        break;
                    case PE::Sense:
                        at(std::format("wrangler_sense_{}", n % 2), e.pos, 1.0f, 30.0f);
                        if (e.intensity < 0.75f) at(std::format("wrangler_clack_{}", n % 3), e.pos, 0.8f, 20.0f);
                        break;
                    case PE::Sink: at(std::format("wrangler_sink_{}", n % 2), e.pos, 0.9f, 50.0f); break;
                    // V13.3: Growler, Nguithr'xurh
                    case PE::Click: at(std::format("growler_click_{}", n % 4), e.pos, 0.45f + 0.4f * e.intensity, 26.0f, 1.15f - 0.3f * e.intensity + 0.05f * var); break;
                    case PE::Lurch: at("growler_surge", e.pos, 1.0f, 34.0f); break;
                    case PE::Merge: at("growler_merge", e.pos, 1.0f, 40.0f); break;
                    case PE::Collapse: at("growler_collapse", e.pos, 0.5f + 0.5f * e.intensity, 30.0f); break;
                    case PE::Snip:
                        // V13.3 Teil 4: Death Rat am Fass (0: schleckt) und an Essensresten (1: nagt)
                        if (role == Role::Rat) at(std::format("rat_{}_{}", e.intensity > 0.5f ? "gnaw" : "lap", n % 3), e.pos, 0.5f, 9.0f, 0.95f + 0.1f * var);
                        else at("ngui_snip", e.pos, 0.45f, 9.0f, 0.95f + 0.1f * var);
                        break;
                    case PE::SacBurst:
                        at(std::format("ngui_burst_{}", n % 2), e.pos, 0.75f, 14.0f, 0.95f + 0.1f * var);
                        // Koop (Gast): die Kugel platzt auf mir - die Wirkung rechnet der Host, das Bild hier selbst
                        if (coopClient_ && session_) {
                            const auto& pl = session_->player();
                            if (std::hypot(pl.x - e.pos.x, pl.y - e.pos.y) < 1.25 && std::fabs(pl.z - e.pos.z) < 0.7)
                                session_->presence().addDose(session_->netSelfId, 0.3f);
                        }
                        break;
                    case PE::Skitter: at(std::format("ngui_skitter_{}", n % 3), e.pos, 0.2f + 0.6f * e.intensity, 9.0f, 0.9f + 0.2f * var); break;
                    case PE::Stridulate:
                        if (e.intensity > 0.6f) at("ngui_chitter_1", e.pos, 0.85f, 14.0f, 0.95f + 0.1f * var);
                        else at("ngui_chitter_0", e.pos, 0.35f + 0.4f * e.intensity, 11.0f, 0.9f + 0.2f * var);
                        break;
                    // V13.3: Ouroboros-Arbeiter - Werkzeug, Maschinen, Warnklang, Geruest, Stimmen, Schub, Vergehen
                    case PE::Work:
                        if (e.intensity < 0.12f) at(std::format("ouro_tap_{}", n % 3), e.pos, 0.7f, 28.0f, 0.95f + 0.1f * var);
                        else if (e.intensity < 0.37f) at(std::format("ouro_trowel_{}", n % 2), e.pos, 0.5f, 14.0f, 0.95f + 0.1f * var);
                        else if (e.intensity < 0.62f) at(std::format("ouro_set_{}", n % 2), e.pos, 0.6f, 18.0f, 0.95f + 0.1f * var);
                        else if (e.intensity < 0.87f) at("ouro_pry", e.pos, 0.8f, 26.0f);
                        else at(std::format("ouro_break_{}", n % 2), e.pos, 0.9f, 34.0f);
                        break;
                    case PE::Weld:
                        if (e.intensity < 0.5f) at("ouro_ignite", e.pos, 0.7f, 20.0f);
                        else at(std::format("ouro_weld_{}", n % 2), e.pos, 0.55f, 20.0f, 0.97f + 0.06f * var);
                        break;
                    case PE::Jackhammer: at(std::format("ouro_jack_{}", n % 2), e.pos, 1.0f, 46.0f, 0.98f + 0.04f * var); break;
                    case PE::Warn: at(std::format("ouro_warn_{}", e.intensity < 0.25f ? 0 : (e.intensity < 0.75f ? 1 : 2)), e.pos, 0.9f, 40.0f); break;
                    case PE::Creak: at(std::format("ouro_creak_{}", n % 2), e.pos, 1.0f, 36.0f); break;
                    case PE::Crash: at(e.intensity > 0.75f ? "ouro_crash_0" : "ouro_crash_1", e.pos, 1.0f, e.intensity > 0.75f ? 55.0f : 40.0f); break;
                    case PE::Murmur:
                        if (e.intensity > 0.75f) at(std::format("ouro_murmur_{}", n % 6), e.pos, 0.85f, 7.0f, 0.95f + 0.1f * var);
                        else at(std::format("ouro_murmur_{}", n % 6), e.pos, 0.5f, 18.0f, 0.92f + 0.16f * var);
                        break;
                    case PE::Shove: at("ouro_shove", e.pos, 0.9f, 6.0f); break;
                    case PE::Dissolve: at("ouro_dissolve", e.pos, 0.8f, 26.0f); break;
                    // V13.3: Smiler am Leuchtkoeder, Skin-Stealer antwortet / stuermt los (KREATUR_Smiler.md, KREATUR_SkinStealer.md)
                    case PE::Lured:
                        at("smiler_lured", e.pos, 0.75f, 24.0f);
                        ui_.toast(session_->world().level().smilerBait == 1 ? "Der Smiler stutzt nur – hier lässt er sich kaum ablenken."
                                                                             : "Der Smiler wendet sich dem Licht zu – jetzt ruhig weg.",
                                  3.2f);
                        break;
                    case PE::Reply: {  // Gast: ohne Angabe (intensity 1) - irgendeine der Antworten
                        const int k = e.intensity < 0.99f ? (int)(e.intensity * 16.0f) : (int)(n % 12);
                        at(std::format("skin_reply_{}", std::clamp(k, 0, 11)), e.pos, 0.85f, 24.0f);
                        static bool replyHint = false;  // einmal je Spielstart: der Rat des Wikis (Level 1)
                        if (!replyHint) replyHint = true, ui_.toast("Das ist kein Mensch. Ruhig zurückweichen – nicht rennen, nicht noch einmal ansprechen.", 4.5f);
                        break;
                    }
                    case PE::Shriek: at("skin_shriek", e.pos, 1.0f, 34.0f); break;
                    // V13.3 Teil 4: Jerry - ruhige Menschenstimme (nah: spricht dich an; leise: murmelt hinter der Tuer),
                    // Fluegel, Putzen und Knirschen; der Bann setzt ein (im Kopf, ohne Richtung)
                    case PE::Speak:
                        if (e.intensity > 0.7f) at(std::format("jerry_talk_{}", n % 6), e.pos, 0.95f, 24.0f, 0.97f + 0.06f * var);
                        else at(std::format("jerry_mutter_{}", n % 3), e.pos, 0.75f, 22.0f, 0.95f + 0.08f * var);
                        break;
                    case PE::Flap: at(e.intensity > 0.7f ? "jerry_flap_1" : "jerry_flap_0", e.pos, 0.8f, 18.0f, 0.95f + 0.1f * var); break;
                    case PE::Preen: at(e.intensity > 0.5f ? "jerry_preen" : "jerry_grind", e.pos, 0.6f, 9.0f, 0.95f + 0.1f * var); break;
                    case PE::Spell: audio_->play("jerry_spell_on", 0.55f + 0.4f * std::min(1.0f, e.intensity), 1.0f, audio::Bus::Effects); break;
                    default: break;
                }
                break;
            }
            case game::GameEvent::EscapeStart:
                ui_.toast(e.text, e.duration);
                audio_->play("fuse_take", 0.8f, 0.85f);
                break;
            case game::GameEvent::Escaped:
                if (endingTimer_ <= 0.0) {
                    ui_.fadeToWhite(2.4);
                    endingTimer_ = 2.6;
                }
                break;
            case game::GameEvent::RegionFound:
                ui_.regionTitle(e.text);
                audio_->play("region_found", 0.6f);
                log::info("Neue Ebene entdeckt: {}", e.text);
                break;
            default: break;
        }
    }
    evs.clear();
}

// Atmosphaere: Brummen der Leuchten, Raumklang der Zonen, Automaten, Herzschlag
void App::updateAudio(double dt) {
    if (!audio_ || !session_) return;
    auto& s = *session_;
    const auto& p = s.player();
    audio_->setListener({(float)p.x, (float)p.y, (float)p.eye()}, (float)p.angle);
    // V13.2: im Koop laeuft die Welt im Menue weiter - also auch ihr Klang (man hoert, was einen erwischt)
    const bool worldPaused = ui_.pausesGame() && !coop();
    audio_->setPaused(worldPaused);
    static int heartId = -1;
    static double breathTimer = 0.0;
    static int boundGen = -1;
    if (boundGen != audioGen_) {
        // neue Sitzung: Dauerklaenge neu starten. V11: Die Atmosphaere tragen die Klangbetten des Levels
        // (Soundscape), nicht mehr ein Klang je Zone.
        boundGen = audioGen_;
        soundscape_.reset(*audio_);
        heartId = -1;
        breathTimer = 0.0;
    }
    const world::Room* room = s.currentRoom();
    // V6: Leuchten als raeumliche Quellen, Raumakustik, ferne Klaenge
    const vec3 ear{(float)p.x, (float)p.y, (float)p.eye()};
    soundscape_.setBlackout(s.blackout().seed, s.blackout().amount, s.blackout().origin, s.blackout().front);  // V7, V11: Welle
    soundscape_.update(dt, time_, *audio_, s.world(), room, ear, s.stats().sanity,
                       state_ == State::Playing && !worldPaused);
    // Automatenbrummen als raeumliche Quellen
    sounds_.clear();
    s.collectSounds(sounds_);
    static std::vector<int> prevVoices, curVoices;
    static int voicesGen = -1;
    if (voicesGen != audioGen_) voicesGen = audioGen_, prevVoices.clear();  // neue Sitzung: alles schon gestoppt
    curVoices.clear();
    for (auto& src : sounds_) {
        vec3 d = src.pos - vec3{(float)p.x, (float)p.y, (float)p.eye()};
        bool inRange = dot(d, d) < src.radius * src.radius * 1.3f;
        if (inRange && *src.voice < 0) *src.voice = audio_->startLoop(*src.sound, src.volume, audio::Bus::Effects);
        if (!inRange && *src.voice >= 0) {
            audio_->stopLoop(*src.voice);
            *src.voice = -1;
        }
        if (*src.voice >= 0) {
            // V6: hinter Waenden dumpfer (Automatenbrummen, Tropfen)
            audio_->setLoopSpatial(*src.voice, soundscape_.spatial(s.world(), ear, src.pos, src.radius));
            curVoices.push_back(*src.voice);
        }
    }
    // Klaenge von Objekten, die es nicht mehr gibt (entladene Chunks ...), beenden
    std::sort(curVoices.begin(), curVoices.end());
    for (int v : prevVoices)
        if (!std::binary_search(curVoices.begin(), curVoices.end(), v)) audio_->stopLoop(v);
    prevVoices.swap(curVoices);
    // Herzschlag bei 0 % Sanity
    const auto& st = s.stats();
    bool beat = st.sanity <= 0.0 && !st.dead;
    if (beat && heartId < 0) heartId = audio_->startLoop("heartbeat", 0.8f, audio::Bus::Effects);
    if (!beat && heartId >= 0) {
        audio_->stopLoop(heartId);
        heartId = -1;
    }
    if (heartId >= 0) audio_->setLoopPitch(heartId, 1.0f + (float)(st.zeroTime / st.deathTime) * 0.8f);
    // V11: unter Wasser - alles dumpf, dazu das Rauschen des Wassers
    {
        static int uwId = -1, uwGen = -1;
        if (uwGen != audioGen_) uwGen = audioGen_, uwId = -1;
        const bool uw = s.underwater() && !st.dead;
        if (uw && uwId < 0) uwId = audio_->startLoop("underwater", 0.5f, audio::Bus::Effects);
        if (!uw && uwId >= 0) audio_->stopLoop(uwId), uwId = -1;
        // V13.3: betaeubt (Nguithr'xurh) klingt alles dumpf
        // V13.3: Fieber und Rinde in den Ohren (Erscheinungen)
        if (!st.dead) audio_->setMuffle(std::max({uw ? 0.8f : 0.0f, 0.6f * s.presence().stupor(s.netSelfId), s.phenomena().muffle(0),
                                                  0.7f * s.presence().jerrySpell(s.netSelfId)}));  // V13.3 Teil 4: Jerrys Bann
    }
    // V13.3: das Rauschen eines Growlers - im Kopf (ohne Richtung), je naeher, desto lauter; die Betaeubung klingt in den Ohren
    {
        static int statId = -1, dazeId = -1, gen13 = -1;
        if (gen13 != audioGen_) gen13 = audioGen_, statId = dazeId = -1;
        const float stc = st.dead || worldPaused ? 0.0f : s.presence().staticLevel();
        if (stc > 0.02f && statId < 0) {
            statId = audio_->startLoop("growler_static", 0.0f, audio::Bus::Creatures);
            audio_->setLoopFadeIn(statId, 0.8f);
        }
        if (stc <= 0.01f && statId >= 0) audio_->stopLoop(statId), statId = -1;
        if (statId >= 0) {
            audio_->setLoopVolume(statId, std::min(1.0f, 0.12f + 0.95f * stc));
            audio_->setLoopPitch(statId, 0.9f + 0.2f * stc);
        }
        const float daze = st.dead ? 0.0f : s.presence().stupor(s.netSelfId);
        if (daze > 0.05f && dazeId < 0) {
            dazeId = audio_->startLoop("ngui_daze", 0.0f, audio::Bus::Effects);
            audio_->setLoopFadeIn(dazeId, 2.0f);
        }
        if (daze <= 0.03f && dazeId >= 0) audio_->stopLoop(dazeId), dazeId = -1;
        if (dazeId >= 0) audio_->setLoopVolume(dazeId, std::min(1.0f, 0.2f + 0.8f * daze));
    }
    // V13.3 Teil 4: Jerrys Bann - Klingeln und Fluestern im Kopf, je staerker, desto lauter; alles andere wird dumpf
    {
        static int spellId = -1, genJ = -1;
        if (genJ != audioGen_) genJ = audioGen_, spellId = -1;
        const float sp = st.dead || worldPaused ? 0.0f : s.presence().jerrySpell(s.netSelfId);
        if (sp > 0.05f && spellId < 0) {
            spellId = audio_->startLoop("jerry_spell", 0.0f, audio::Bus::Effects);
            audio_->setLoopFadeIn(spellId, 0.6f);
        }
        if (sp <= 0.03f && spellId >= 0) audio_->stopLoop(spellId), spellId = -1;
        if (spellId >= 0) audio_->setLoopVolume(spellId, std::min(1.0f, 0.15f + 0.95f * sp));
    }
    // V9/V10: jagt eine Entity, liegt ein tiefes Brummen an ihrer Stelle; Herzschlag, wenn sie nah ist;
    // der Schwarm flattert; draussen: Wind
    {
        static int droneId = -1, nearHeart = -1, outsideId = -1, gen9 = -1, flutterId = -1;
        static float lastProx = 0.0f;  // V11: Nachstille
        static double hush = 0.0;
        if (gen9 != audioGen_) gen9 = audioGen_, droneId = nearHeart = outsideId = flutterId = -1, lastProx = 0.0f, hush = 0.0;  // neue Sitzung: alles gestoppt
        const auto& pr = s.presence();
        using R = game::Presence::Role;
        // V11: nur das Laecheln hat einen Dauerklang (Druck in der Tiefe, nasser Atem); den Hund hoert man
        // an Schritten, Hecheln und Knurren
        const bool on = pr.hunting() && pr.has(R::Smiler) && !st.dead && ui_.mode() != ui::Mode::Ending;
        if (on && droneId < 0) {
            droneId = audio_->startLoop("smiler_presence", 0.0f, audio::Bus::Creatures);
            audio_->setLoopFadeIn(droneId, 2.5f);
        }
        if (!on && droneId >= 0) audio_->stopLoop(droneId), droneId = -1;
        if (droneId >= 0) {
            const float prox = pr.threatProximity();
            const vec3 sp = pr.posOf(R::Smiler);
            audio_->setLoopSpatial(droneId, soundscape_.spatial(s.world(), ear, sp + vec3{0, 0, 1.4f}, 30.0f));
            audio_->setLoopVolume(droneId, 0.35f + 0.55f * prox);
            audio_->setLoopPitch(droneId, 0.96f + 0.06f * prox);
        }
        vec3 sc;
        const int flying = st.dead ? 0 : pr.swarm(sc);
        if (flying > 0 && flutterId < 0) {
            flutterId = audio_->startLoop("moth_flutter", 0.0f, audio::Bus::Creatures);
            audio_->setLoopFadeIn(flutterId, 0.4f);
        }
        if (flying == 0 && flutterId >= 0) audio_->stopLoop(flutterId), flutterId = -1;
        if (flutterId >= 0) {
            audio_->setLoopSpatial(flutterId, soundscape_.spatial(s.world(), ear, sc, 14.0f));
            audio_->setLoopVolume(flutterId, std::min(1.0f, 0.3f + 0.1f * (float)flying));
        }
        const float prox = pr.threatActive() && !st.dead ? pr.threatProximity() : 0.0f;
        if (prox > 0.45f && nearHeart < 0 && heartId < 0) nearHeart = audio_->startLoop("heartbeat", 0.0f, audio::Bus::Effects);
        if ((prox < 0.35f || heartId >= 0) && nearHeart >= 0) audio_->stopLoop(nearHeart), nearHeart = -1;
        if (nearHeart >= 0) {
            audio_->setLoopVolume(nearHeart, std::clamp((prox - 0.35f) * 1.4f, 0.0f, 0.75f));
            audio_->setLoopPitch(nearHeart, 0.85f + 0.5f * prox);
        }
        const bool outside = ui_.mode() == ui::Mode::Ending;
        if (outside && outsideId < 0) {
            outsideId = audio_->startLoop("bed_l19_wind", 0.75f, audio::Bus::Ambience);  // V11: Wind draussen
            audio_->setLoopFadeIn(outsideId, 4.0f);
        }
        if (!outside && outsideId >= 0) audio_->stopLoop(outsideId), outsideId = -1;
        // V11: Stromausfall oder draussen - die Klangbetten (Lueftung, Rohre) verstummen fast.
        // Stille als Mittel: waehrend einer Begegnung treten die Betten zurueck (das Wesen ist zu hoeren), ist sie
        // vorbei, bleibt es neun Sekunden fast still, bevor das Gebaeude wieder "atmet" (die letzten vier blenden
        // auf); ferne Einzelklaenge pausieren so lange
        if (lastProx > 0.4f && prox < 0.1f && !st.dead) hush = 9.0;
        lastProx = prox;
        hush = std::max(0.0, hush - dt);
        const float hushAmt = (float)std::clamp(hush / 4.0, 0.0, 1.0);
        soundscape_.setQuiet(hush > 0.0 || prox > 0.3f);
        soundscape_.setBedDuck(outside || st.dead ? 1.0f
                                                  : std::max({std::clamp(s.blackout().amount * 0.7f, 0.0f, 0.7f), prox * 0.5f, hushAmt * 0.75f}));
    }
    app::wranglerLoops(*audio_, soundscape_, s, ear, audioGen_, st.dead);  // V13.3: Rumpeln, Leib, Sog (app/wrangler_audio.cpp)
    // V6: Keuchen nach dem Sprinten. V11: einzelne Atemzuege (Ein- und Ausatmen), je atemloser desto
    // schneller und lauter
    float puff = (float)st.breathless();
    breathTimer -= dt;
    if (puff > 0.06f && !st.dead && breathTimer <= 0.0 && !worldPaused) {
        audio::PlayOpts o;
        o.volume = 0.25f + 0.5f * puff;
        o.pitch = 0.96f + 0.1f * puff;
        audio_->play2d("breath", o);
        breathTimer = 1.9 - 0.9 * puff;
    }
}

// mul (V6): Zuschlag auf das Sichtfeld (z. B. beim Sprinten), wirkt auf tan(fov/2)
float App::fovY(float mul) const {
    float aspect = (float)window_.width() / (float)std::max(1, window_.height());
    float v;
    if (settings_.fov > 0.0f) {
        v = 2.0f * std::atan(std::tan(settings_.fov * kPi / 360.0f) / aspect);
    } else {
        // automatisch: senkrecht etwa 62 Grad, horizontal hoechstens 105 Grad
        v = 62.0f * kPi / 180.0f;
        float hMax = 105.0f * kPi / 180.0f;
        float h = 2.0f * std::atan(std::tan(v * 0.5f) * aspect);
        if (h > hMax) v = 2.0f * std::atan(std::tan(hMax * 0.5f) / aspect);
    }
    return mul == 1.0f ? v : 2.0f * std::atan(std::tan(v * 0.5f) * mul);
}

void App::render(double dt) {
    renderer_->ui().begin(renderer_->outputWidth(), renderer_->outputHeight());
    renderWorld(dt, false);
    if (!(opt_.noHud && state_ == State::Playing)) ui_.frame(*this, dt);
    renderer_->renderUi();
    renderer_->present();
}

// Szene ohne Oberflaeche; clean = ohne Menue-Abdunklung/Effekte (Vorschaubilder)
void App::renderWorld(double dt, bool clean) {
    if (state_ == State::Loading || !session_) {
        renderer_->renderBlank({0.02f, 0.02f, 0.018f});
    } else {
        auto& s = *session_;
        const auto& p = s.player();
        const bool playing = state_ == State::Playing;
        const bool third = playing && settings_.thirdPerson;
        gfx::CameraState cam;
        if (playing) {
            // V6: Kamera im Kopf des Strichmaennchens (bzw. hinter der Schulter)
            s.body().setOutsideView(third && !egoPose_);  // V13.2 (B-001): Lampenhaltung fuer die Ansicht von aussen
            game::ViewCamera vc = s.camera(third, clean ? 0.0 : dt);
            if (coop() && s.stats().dead && scareT_ < 0.0) {  // V13.2 Koop: tot - einem Mitspieler zusehen
                const int sid = coopHost_ ? coopHost_->spectate() : coopClient_->spectate();
                game::Pawn* sp = sid > 0 ? (coopHost_ ? coopHost_->pawnOf(sid) : coopClient_->pawnOf(sid)) : nullptr;
                if (!sp && coopClient_ && sid == 0) sp = coopClient_->pawnOf(0);
                if (sp) {
                    const float a = (float)sp->player.angle, pt = (float)sp->player.pitch;
                    const vec3 f{std::cos(a) * std::cos(pt), std::sin(a) * std::cos(pt), std::sin(pt)};
                    vc.pos = sp->eye() - f * 2.2f + vec3{0, 0, 0.35f};
                    vc.yaw = a, vc.pitch = pt - 0.12f, vc.roll = 0.0f;
                }
            }
            cam.pos = vc.pos;
            cam.yaw = vc.yaw;
            cam.pitch = vc.pitch;
            cam.roll = vc.roll;
            cam.fovY = fovY(vc.fovMul);
            vec3 fp;
            float fyaw = 0.0f;
            // V13.2 (Tests): ohne Vorfuehrung und in der Schulteransicht kreist figcam um den Spieler selbst
            const bool aroundPlayer = figCam_.on && third && !s.presence().showcasePos(fp, fyaw);
            if (aroundPlayer) fp = vec3{(float)p.x, (float)p.y, (float)p.z}, fyaw = (float)p.angle;
            if (figCam_.on && (aroundPlayer || s.presence().showcasePos(fp, fyaw))) {
                // V13: um das Wesen, az = 0 vor ihm (in seiner Blickrichtung), Blick auf die Hoehe h
                const vec3 tgt = fp + vec3{std::cos(fyaw) * figCam_.ahead, std::sin(fyaw) * figCam_.ahead, figCam_.h};
                const float a = fyaw + figCam_.az * 0.0174533f, e = figCam_.el * 0.0174533f;
                const vec3 d{std::cos(e) * std::cos(a), std::cos(e) * std::sin(a), std::sin(e)};
                cam.pos = tgt + d * figCam_.dist;
                cam.yaw = std::atan2(-d.y, -d.x);
                cam.pitch = std::asin(std::clamp(-d.z, -1.0f, 1.0f));
                cam.roll = 0.0f;
                cam.fovY = fovY(1.0f) * 0.75f;
            } else if (vmCam_.on && s.body().flashlightHeld()) {
                // V13: im Rahmen der Lampe (x zum Glas, z oben, y links) um die Griffmitte, Blick auf sie
                const vec3 x = s.body().flashlightDir(), z = s.body().flashUp(), y = cross(z, x);
                const float a = vmCam_.az * 0.0174533f, e = vmCam_.el * 0.0174533f;
                const vec3 d = x * (std::cos(e) * std::cos(a)) + y * (std::cos(e) * std::sin(a)) + z * std::sin(e);
                cam.pos = s.body().flashGrip() + d * vmCam_.dist;
                cam.yaw = std::atan2(-d.y, -d.x);
                cam.pitch = std::asin(std::clamp(-d.z, -1.0f, 1.0f));
                cam.roll = 0.0f;
                cam.fovY = fovY(1.0f) * 0.6f;
            }
        } else {
            cam.pos = {(float)p.x, (float)p.y, (float)p.eye()};
            cam.yaw = (float)(p.angle + p.sway());
            cam.pitch = (float)p.pitch;
            cam.fovY = fovY();
        }
        if (freeCam_.on) {  // V13.3 (Tests): freie Kamera
            cam.pos = freeCam_.pos;
            cam.yaw = freeCam_.yaw * 0.0174533f;
            cam.pitch = freeCam_.pitch * 0.0174533f;
            cam.roll = 0.0f;
            if (freeCam_.fov > 1.0f) cam.fovY = freeCam_.fov * 0.0174533f;
        }
        if (playing && !freeCam_.on && !figCam_.on && !vmCam_.on) {  // V13.3: das Bild schwankt (Rauschen), traege (Betaeubung)
            const float stc = s.presence().staticLevel(), daze = s.presence().stupor(s.netSelfId), t = (float)time_;
            cam.roll += stc * (0.05f * std::sin(t * 2.3f) + 0.025f * std::sin(t * 7.9f)) + daze * 0.07f * std::sin(t * 0.6f);
            cam.yaw += stc * 0.012f * std::sin(t * 13.0f) * std::sin(t * 3.1f) + daze * 0.03f * std::sin(t * 0.43f);
            cam.pitch += daze * 0.025f * std::sin(t * 0.51f + 1.0f);
            cam.fovY *= 1.0f + 0.04f * stc * std::sin(t * 4.0f) + 0.05f * daze;
            const float jsp = s.presence().jerrySpell(s.netSelfId);  // V13.3 Teil 4: Jerrys Bann - der Blick verengt sich auf ihn
            cam.fovY *= 1.0f - 0.16f * jsp * jsp - 0.22f * jsp * jsp * jsp;
        }
        if (playing && coop()) {  // V13.2: Namen ueber den Mitspielern (nach allen Kameras; dieselben Matrizen wie das Bild)
            std::vector<ui::Ui::NameTag> tags;
            const float aspect = (float)window_.width() / (float)std::max(1, window_.height());
            const mat4 vp = perspectiveReversedInfinite(cam.fovY, aspect, 0.05f) * viewMatrix(cam.pos, cam.yaw, cam.pitch, cam.roll);
            const auto ps = coopView().players;
            for (const game::Pawn* pw : s.pawns) {
                if (!pw || !pw->alive) continue;
                const vec3 head = pw->eye() + vec3{0, 0, 0.24f};
                const float dist = length(head - cam.pos);
                const vec4 c = vp * vec4(head, 1.0f);
                if (c.w < 0.3f || dist > 40.0f) continue;
                std::string nm = pw->name;
                for (const auto& pi : ps)
                    if (pi.id == pw->id) nm = pi.name;
                tags.push_back({0.5f + 0.5f * c.x / c.w, 0.5f - 0.5f * c.y / c.w, std::clamp(1.4f - dist / 30.0f, 0.25f, 1.0f), nm});
            }
            ui_.setNameTags(std::move(tags));
        }
        gfx::SceneParams sp;
        sp.fogColor = s.fogColor();
        sp.fogDensity = s.fogDensity() * settings_.render.fogDensity;
        sp.viewDistance = (float)(settings_.viewDistance * 16);
        sp.time = (float)time_;
        sp.dt = (float)dt;
        const auto& st = s.stats();
        sp.redEdge = state_ == State::Playing ? (float)(st.dead ? 1.0 : st.red) : 0.0f;
        sp.redPulse = (st.zeroTime > 0 && std::sin(time_ * 6.0) > 0.75) ? 1.0f : 0.0f;
        bool menu = state_ == State::Title || ui_.mode() == ui::Mode::Pause || ui_.mode() == ui::Mode::Inventory ||
                    ui_.mode() == ui::Mode::Dead;
        static float dim = 0.0f;
        dim += ((menu ? 1.0f : 0.0f) - dim) * (float)std::min(1.0, dt * 8.0);
        sp.menuDim = state_ == State::Title ? 0.7f : dim * 0.85f;
        sp.desaturate = (float)std::clamp((25.0 - st.sanity) / 25.0, 0.0, 1.0) * 0.45f;
        if (state_ == State::Playing) {  // V13.3: Pilz, Garden, Einladung der Spielleiterin (session_phenomena.cpp)
            sp.fungus = s.phenomena().fungusFx(0);
            sp.garden = s.phenomena().gardenFx(0);
            sp.jester = s.phenomena().jesterFx(0);
        }
        // V9/V10: eine nahe, jagende Entity entzieht dem Bild die Farbe; ein Angriff faerbt den Rand rot
        if (playing) {
            sp.desaturate = std::min(0.85f, sp.desaturate + s.presence().threatProximity() * 0.4f);
            hitFlash_ = std::max(0.0f, hitFlash_ - (float)dt * 0.45f);
            sp.redEdge = std::max(sp.redEdge, hitFlash_);
        }
        // Stimmung: V4-Helligkeit des Raums steuert die Belichtung mit (Auge gewoehnt sich langsam)
        static float mood = 1.0f;
        if (const world::Room* room = s.currentRoom()) {
            float target = std::clamp(0.42f + 0.62f * (float)room->avgLight, 0.45f, 1.15f);
            mood += (target - mood) * (float)std::min(1.0, dt * 0.8);
        }
        sp.moodExposure = mood;
        const float glow = playing ? s.escapeGlow() : 0.0f;  // V9: die Kabine wird immer heller
        if (glow > 0.0f) sp.moodExposure = mood * (1.0f + 2.2f * glow * glow), sp.desaturate = std::max(sp.desaturate, 0.5f * glow);
        // V13.3: Rauschen eines Growlers - Bildrauschen wie zwischen zwei Sendern, Farbsaeume, die Farbe weicht; Betaeubung
        // (Nguithr'xurh) - dunkler, weichgezeichnet, entsaettigt
        if (playing) {
            const float stc = s.presence().staticLevel(), daze = s.presence().stupor(s.netSelfId);
            if (stc > 0.0f) {
                const float flick = 0.7f + 0.3f * std::sin((float)time_ * 37.0f) * std::sin((float)time_ * 11.0f);
                sp.grainBoost = 9.0f * stc * stc * flick;
                sp.chromaBoost = 5.0f * stc;
                sp.desaturate = std::max(sp.desaturate, 0.55f * stc);
            }
            if (daze > 0.0f) {
                sp.menuDim = std::max(sp.menuDim, 0.55f * smoothstep(0.15f, 1.0f, daze));
                sp.desaturate = std::max(sp.desaturate, 0.45f * daze);
                sp.moodExposure *= 1.0f - 0.35f * daze;
            }
            // V13.3 Teil 4: Jerrys Bann - die Farbe weicht, der Rand dunkelt, das Bild wird enger (Kamera)
            if (const float jsp = s.presence().jerrySpell(s.netSelfId); jsp > 0.0f) {
                sp.menuDim = std::max(sp.menuDim, 0.6f * smoothstep(0.3f, 1.0f, jsp));
                sp.desaturate = std::max(sp.desaturate, 0.6f * jsp);
            }
        }
        // V13.3 (KREATUR_L3_Purpur.md): Strahlung der purpifizierten Wesen - Bildrauschen, beim Knistern ein Schub
        if (playing) {
            const auto& pl = s.player();
            const float pg = s.purple().grain(vec3{(float)pl.x, (float)pl.y, (float)pl.eye()});
            if (pg > 0.0f) sp.grainBoost = std::max(sp.grainBoost, 2.2f * pg), sp.chromaBoost = std::max(sp.chromaBoost, 0.8f * pg);
        }
        // V11: Schreck - weisser Blitz in den ersten Bildern, roter Rand, danach fast schwarz
        if (playing && scareT_ >= 0.0) {
            const float pop = std::exp(-(float)scareT_ * 11.0f);
            sp.moodExposure *= 1.0f + 3.0f * pop;
            sp.desaturate = std::max(sp.desaturate, 0.35f);
            sp.redEdge = 1.0f;
            if (scareCut_) sp.menuDim = std::max(sp.menuDim, 0.97f);
        }
        sp.noNearFade = playing && vmCam_.on;  // V13
        sp.resetHistory = resetHistory_;
        resetHistory_ = false;
        visible_.clear();
        s.collectMeshes(visible_);
        // V6: eigener Koerper (Ego-Perspektive ohne Kopf); TAA rechnet die Spielerbewegung heraus
        if (playing && (third || settings_.showBody)) {
            s.collectBody(visible_, !third);
            mat4 root = s.body().rootMatrix();
            sp.attachedReproject = (havePrevRoot_ && !sp.resetHistory) ? prevBodyRoot_ * inverse(root) : mat4::identity();
            prevBodyRoot_ = root;
            havePrevRoot_ = true;
        }
        draws_.clear();
        for (auto& v : visible_) {
            if (*v.meshId == -2) {
                *v.meshId = renderer_->meshId(*v.model);
                if (*v.meshId < 0 && v.model->rfind("prop:", 0) == 0)  // V13.3: bewegliche Ausstattung (Gemaelde)
                    *v.meshId = renderer_->propMesh(v.model->substr(5), s.world().bank());
                // V13: Nachweis, welches Netz fuer ein Wesen gezeichnet wird (einmal je Name; Quelle steht beim Hochladen)
                if (v.model->rfind("ent_", 0) == 0) {
                    static std::set<std::string> logged;
                    if (logged.insert(*v.model).second) log::info("Wesen im Bild: Netz {} = ID {}", *v.model, *v.meshId);
                }
            }
            if (*v.meshId >= 0) {
                gfx::MeshDraw md{*v.meshId, v.world, v.highlight, v.attached};
                md.bones = v.bones;
                md.dynamic = v.dynamic;
                md.cullCenter = v.cullCenter;
                md.cullRadius = v.cullRadius;
                md.material = v.material == game::kShadowLook ? (int)gfx::FM_ENT_SHADOW : v.material;  // V10
                md.clip = v.clip;  // V13
                md.dualQuat = v.dualQuat;  // V13.2
                md.fx = v.fx;              // V13.3: Pilz, Ranken (KREATUR_Crawler.md)
                draws_.push_back(md);
            }
        }
        // V10: Knochen der Figuren (3 Zeilen je Matrix) und ihre Schattenkapseln
        boneRows_.clear();
        for (const mat4& m : s.bonePalette()) boneRows_.insert(boneRows_.end(), {m.row(0), m.row(1), m.row(2)});
        sp.bones = &boneRows_;
        casters_.clear();
        s.collectShadows(casters_);
        game::packShadows(casters_, sp.capsules, sp.capsuleFigures);
        // V7: Taschenlampe - Spotlicht mit Schatten aus dem Lampenkopf
        if (playing) {
            game::FlashBeam fb = s.flashBeam();
            if (fb.on) {
                gfx::ExtraLight L;
                L.pos = fb.pos;
                L.dir = fb.dir;
                L.color = vec3{1.0f, 0.93f, 0.8f} * (3.6f * fb.power);
                L.color = L.color * s.purple().lampFactor(fb.pos, time_);  // V13.3: nahe purpifizierten Wesen stockt die Lampe
                L.range = 24.0f;
                L.spot = true;
                L.cosInner = 0.975f;  // ~13 Grad heller Kern
                L.cosOuter = 0.885f;  // ~28 Grad weicher Rand
                L.soft = 0.3f;
                L.size = 0.012f;
                sp.extraLights.push_back(L);
            }
            // V13.2 Koop: die Lampen der Mitspieler (gleicher Kegel, ohne Schatten ab dem dritten)
            int others = 0;
            for (const game::Pawn* pw : s.pawns) {
                vec3 lp, ld;
                float pwr = 0.0f;
                if (!pw || !s.pawnBeam(*pw, lp, ld, pwr) || sp.extraLights.size() >= 8) continue;
                gfx::ExtraLight L;
                L.pos = lp;
                L.dir = ld;
                L.color = vec3{1.0f, 0.93f, 0.8f} * (3.6f * pwr);
                L.range = 24.0f;
                L.spot = true;
                L.cosInner = 0.975f;
                L.cosOuter = 0.885f;
                L.soft = 0.3f;
                L.size = 0.012f;
                L.shadows = ++others <= 2;
                sp.extraLights.push_back(L);
            }
            // V13.3: Leuchtstaebe (geworfen, in der Hand) - gruenes Licht nach allen Seiten, die zwei naechsten mit Schatten
            std::vector<game::GameSession::GlowLight> glows;
            s.glowLights(glows, 30.0f);
            for (size_t gi = 0; gi < glows.size() && gi < 4 && sp.extraLights.size() < 9; ++gi) {
                gfx::ExtraLight L;
                L.pos = glows[gi].pos;
                L.dir = {0.0f, 0.0f, 1.0f};
                L.color = vec3{0.34f, 1.0f, 0.22f} * (0.8f * glows[gi].power);
                L.range = 2.5f + 4.5f * glows[gi].power;
                L.spill = 1.0f;
                L.size = 0.06f;
                L.soft = 0.12f;
                L.shadows = gi < 2;
                sp.extraLights.push_back(L);
            }
        }
        // V8: Kabinenlicht der Aufzuege in der Naehe (offen oder man steht drin)
        elevLights_.clear();
        s.elevatorLights(elevLights_, 16.0f);
        for (const vec3& lp : elevLights_) {
            if (sp.extraLights.size() >= 10) break;
            gfx::ExtraLight L;
            L.pos = lp;
            L.dir = {0.0f, 0.0f, -1.0f};
            L.color = vec3{1.0f, 0.95f, 0.86f} * (0.9f + 7.0f * glow * glow);
            L.range = 4.5f + 6.0f * glow;
            L.spill = 0.55f;
            L.size = 0.5f;
            L.soft = 0.5f;
            L.shadows = false;
            sp.extraLights.push_back(L);
        }
        // V9: Sicherungskaesten - unruhiges, oranges Licht (Funken, Brummen)
        fuseLights_.clear();
        if (playing) s.fuseLights(fuseLights_, 24.0f);
        for (const auto& fl : fuseLights_) {
            if (sp.extraLights.size() >= 10) break;
            const float tt = (float)time_ * 9.0f + fl.phase;
            float flick = 0.62f + 0.22f * std::sin(tt) * std::sin(tt * 2.7f + 1.3f);
            if (hash01((u32)(time_ * 14.0) * 2654435761u + (u32)(fl.phase * 100.0f)) > 0.93f) flick = 1.25f;  // Funke
            gfx::ExtraLight L;
            L.pos = fl.pos;
            L.dir = fl.dir;
            L.color = vec3{1.0f, 0.48f, 0.12f} * (0.85f * flick);
            L.range = 6.5f;
            L.spill = 0.6f;
            L.size = 0.12f;
            L.soft = 0.5f;
            L.shadows = false;
            sp.extraLights.push_back(L);
        }
        // V13.3: Ouroboros-Baustelle - Lichtbogen beim Schweissen (kalt, flackernd), Warnleuchten (bernstein, blinkend)
        ouroLights_.clear();
        if (playing) s.presence().ouroLights(ouroLights_, cam.pos);
        for (const auto& [lp, col] : ouroLights_) {
            if (sp.extraLights.size() >= 10) break;
            gfx::ExtraLight L;
            L.pos = lp;
            L.dir = {0.0f, 0.0f, -1.0f};
            L.color = col;
            L.range = 7.0f;
            L.spill = 1.0f;
            L.size = 0.05f;
            L.soft = 0.3f;
            L.shadows = false;
            sp.extraLights.push_back(L);
        }
        // V13.3 (KREATUR_L3_Purpur.md): purpifizierte Wesen - violettes Licht vor dem Koerper (atmet, Schwall, Lampe darauf)
        if (playing) {
            static std::vector<game::Purpification::Light> purpleLights;
            purpleLights.clear();
            s.purple().lights(purpleLights, cam.pos, 24.0f, time_);
            for (const auto& pl : purpleLights) {
                if (sp.extraLights.size() >= 10) break;
                gfx::ExtraLight L;
                L.pos = pl.pos;
                L.dir = {0.0f, 0.0f, -1.0f};
                L.color = pl.color;
                L.range = pl.range;
                L.spill = 1.0f;
                L.size = 0.3f;
                L.soft = 0.45f;
                L.shadows = false;
                sp.extraLights.push_back(L);
            }
        }
        // V11: Level 6 - Tasten: was in Armlaenge ist, zeichnet sich ganz schwach ab (kein Schatten, kein Glanz)
        if (playing && s.world().level().touchLight) {
            gfx::ExtraLight L;
            L.pos = vec3{(float)s.player().x, (float)s.player().y, (float)s.player().eye() - 0.3f};
            L.dir = vec3{0.0f, 0.0f, -1.0f};
            L.color = vec3{0.55f, 0.6f, 0.66f} * 1.1f;
            L.range = 2.4f;
            L.spill = 1.0f;
            L.size = 0.3f;
            L.soft = 0.3f;
            L.shadows = false;
            sp.extraLights.push_back(L);
        }
        // V13.3 (Tests): Testlampe (wie die Taschenlampe, frei gestellt) und Leuchten der Welt
        if (testLight_.on) {
            gfx::ExtraLight L;
            L.pos = testLight_.pos;
            L.dir = testLight_.dir;
            L.color = vec3{1.0f, 0.93f, 0.8f} * (3.6f * testLight_.power);
            L.range = 24.0f;
            L.spot = true;
            L.cosOuter = std::cos(testLight_.cone * 0.0174533f);
            L.cosInner = std::cos(testLight_.cone * 0.0174533f * 0.46f);
            L.soft = 0.3f;
            L.size = 0.012f;
            sp.extraLights.insert(sp.extraLights.begin(), L);
        }
        sp.worldLightScale = worldLight_;
        // V7: Stromausfall (Halluzination)
        sp.blackoutSeed = s.blackout().seed;
        sp.blackoutAmount = s.blackout().amount;
        sp.blackoutOrigin = s.blackout().origin;  // V11: Welle
        sp.blackoutFront = s.blackout().front;
        // V13.3: Beben (Wrangler) - Staub rieselt
        dustFall_ += (float)dt * (0.9f * s.presence().quake());
        sp.dustFall = dustFall_, sp.dustBoost = s.presence().quake();
        // V7: Notausgaenge - das gruene Schild faerbt Wand und Boden davor
        exitSigns_.clear();
        s.exitSigns(exitSigns_, 40.0f);
        for (const auto& [pos, dir] : exitSigns_) {
            if (sp.extraLights.size() >= 8) break;
            gfx::ExtraLight L;
            L.pos = pos + dir * 0.1f;
            L.dir = dir;
            L.color = vec3{0.12f, 1.0f, 0.36f} * 0.42f;
            L.range = 5.5f;
            L.spill = 0.35f;
            L.size = 0.18f;
            L.soft = 0.5f;
            L.shadows = false;
            sp.extraLights.push_back(L);
        }
        if (clean) sp.menuDim = sp.redEdge = sp.desaturate = sp.fungus = sp.garden = sp.jester = 0.0f;
        renderer_->renderScene(cam, sp, draws_);
    }
}

// V13.3 (Kontaktboegen, Objektpruefung, Schilderzaehlung): alles, was die Ebene an Ausstattung zeigt, je Stueck mit Lage,
// Boden und Decke der Kachel, Abstand zur Wand hinter dem Ursprung und Hoehe des Modells; dazu Notausgaenge und Raeume.
// Nur feste Karten (die ganze Karte liegt in Sektor 0,0).
void App::propDump(const std::string& path) {
    auto& s = *session_;
    auto& w = s.world();
    const world::Sector& sec = w.sector(0, 0);
    std::string out = std::format("{{\"level\":\"{}\",\"props\":[", w.level().id);
    int n = 0;
    for (const auto& pr : sec.props) {
        std::string name;
        int variant = 0;
        if (!pr.model || !props::modelName(pr.model, name, variant)) name = "?";
        const world::Tile& t = w.tile(ifloor(pr.x), ifloor(pr.y));
        // Wand hinter dem Ursprung (gegen die Blickrichtung des Modells) und rundum
        auto solidAt = [&](double x, double y) { return w.tile(ifloor(x), ifloor(y)).solid(); };
        double back = 9.0, around = 9.0;
        for (double d = 0.0; d <= 3.0; d += 0.02)
            if (solidAt(pr.x - std::cos(pr.yaw) * d, pr.y - std::sin(pr.yaw) * d)) {
                back = d;
                break;
            }
        for (int a = 0; a < 16; ++a)
            for (double d = 0.0; d <= std::min(around, 3.0); d += 0.02)
                if (solidAt(pr.x + std::cos(a * 0.3927) * d, pr.y + std::sin(a * 0.3927) * d)) {
                    around = std::min(around, d);
                    break;
                }
        const Aabb b = pr.model ? pr.model->bounds : Aabb{};
        std::string room = t.room ? std::format("\"{}\",\"{}\",[{},{},{},{}]", t.room->type ? t.room->type->key : "",
                                                w.level().zones[(size_t)std::max(0, t.room->zone)].key, t.room->x0, t.room->y0,
                                                t.room->x1, t.room->y1)
                                  : std::string("\"\",\"\",[]");
        out += std::format("{}\n{{\"name\":\"{}\",\"v\":{},\"p\":[{:.3f},{:.3f},{:.3f}],\"yaw\":{:.4f},\"s\":[{:.3f},{:.3f},{:.3f}],"
                           "\"floor\":{:.3f},\"ceil\":{:.3f},\"solid\":{},\"back\":{:.2f},\"around\":{:.2f},"
                           "\"lo\":[{:.3f},{:.3f},{:.3f}],\"hi\":[{:.3f},{:.3f},{:.3f}],\"tris\":{},\"room\":[{}]}}",
                           n++ ? "," : "", name, variant, pr.x, pr.y, pr.z, pr.yaw, pr.sx, pr.sy, pr.sz, t.floor, t.ceil,
                           t.solid() ? 1 : 0, back, around, b.lo.x, b.lo.y, b.lo.z, b.hi.x, b.hi.y, b.hi.z,
                           [&] {
                               size_t k = 0;
                               if (pr.model)
                                   for (const auto& part : pr.model->parts) k += part.mesh.i.size() / 3;
                               return k;
                           }(),
                           room);
    }
    out += "],\n\"exits\":[";
    n = 0;
    s.registry().each<game::ExitDoor, game::Transform>([&](ecs::Entity, game::ExitDoor&, game::Transform& t) {
        out += std::format("{}[{:.2f},{:.2f},{:.2f},{:.3f}]", n++ ? "," : "", t.pos.x, t.pos.y, t.pos.z, t.yaw);
    });
    out += "],\n\"rooms\":[";
    n = 0;
    for (const auto& r : sec.rooms)
        out += std::format("{}\n[{},{},{},{},{:.2f},{:.2f},\"{}\",{}]", n++ ? "," : "", r->x0, r->y0, r->x1, r->y1, r->floor,
                           r->height, r->type ? r->type->key : "", r->isCorridor ? 1 : 0);
    out += "]}\n";
    if (FILE* f = _wfopen(paths::widen(path).c_str(), L"wb")) {
        std::fwrite(out.data(), 1, out.size(), f);
        std::fclose(f);
        log::info("Ausstattung: {} Stueck nach {}", sec.props.size(), path);
    } else {
        log::error("propdump: kann {} nicht schreiben", path);
        scriptFailed_ = true;
    }
}

void App::takeScreenshot(const std::string& path) {
    std::vector<u8> rgba;
    int w, h;
    if (renderer_->captureBackbuffer(rgba, w, h) && gfx::savePng(path, rgba.data(), w, h)) {
        // V11: der Pfad steht im Protokoll; im Bild nur eine kurze Bestaetigung (in Aufnahmen und Tests gar nichts)
        if (!testRun()) ui_.toast("Screenshot gespeichert", 2.0);
        log::info("Screenshot: {}", path);
    } else {
        ui_.toast("Screenshot fehlgeschlagen", 3.0);
    }
}

std::string App::debugInfo() {
    if (!session_) return "";
    auto& s = *session_;
    const auto& p = s.player();
    const auto& rs = renderer_->stats();
    const auto& gi = renderer_->gpuInfo();
    std::string out = std::format("FPS {:.0f}   {}x{} intern   {} Chunks   {} Lichter   {}k Dreiecke   {} Objekte\n", fps_,
                                  rs.internalW, rs.internalH, rs.chunksDrawn, rs.lights, rs.triangles / 1000, rs.meshes);
    out += std::format("Bildzeit: Spiel {:.2f} ms   Grafik (CPU) {:.2f} ms   synchron erzeugt {}\n", simMs_, renderMs_,
                       s.world().syncWork());
    out += renderer_->gpuTimer().summary() + "\n";
    out += std::format("GPU {}{}\n", gi.adapter, gi.warp ? " (WARP)" : "");
    out += std::format("Position x={:.2f} y={:.2f} z={:.2f}   Blick {:.0f}°   Neigung {:+.2f}\n", p.x, p.y, p.z,
                       std::fmod(p.angle * 180.0 / kPi + 360.0, 360.0), p.pitch);
    const int S = s.world().sectorSize();
    out += std::format("Seed {}   Sektor ({},{})   Chunk ({},{})\n", s.seed, floorDiv(ifloor(p.x), S), floorDiv(ifloor(p.y), S),
                       ifloor(p.x) >> 4, ifloor(p.y) >> 4);
    if (const world::Room* r = s.currentRoom()) {
        const auto& lv = s.world().level();
        out += std::format("Raum {},{}#{}: {} [{}]  {}x{}  Höhe {:.1f} m  Boden {:.1f} m\n", r->sx, r->sy, r->index,
                           r->type->label, world::categoryName(r->type->category), r->w(), r->h(), r->height, r->floor);
        if (lv.world()) {  // V8: Ebene und Schicht
            auto share = s.world().generator().zones().regionWeights(p.x, p.y);
            std::string mix;
            for (size_t k = 0; k < lv.regions.size(); ++k)
                if (share[k] > 0.005) mix += std::format("  {} {:.0f}%", lv.regions[k].name, share[k] * 100.0);
            out += std::format("Ebene: {}   Schicht {:.2f}  ·{}\n", s.regionName(),
                               s.world().generator().zones().strataCoord(p.x, p.y), mix);
        }
        out += std::format("Zone: {} {:.0f}%   Licht {}  Grundhelligkeit {:.2f}   Türen {}\n", lv.zones[(size_t)r->zone].name,
                           r->zoneW[(size_t)r->zone] * 100.0, r->lightStyle, r->ambient, r->doors.size());
    }
    auto& w = s.world();
    out += std::format("Geladen: {} Sektoren  {} Räume  {} Chunks   erzeugt {}   Jobs {}   Entities {}\n", w.loadedSectors(),
                       w.loadedRooms(), w.loadedChunks(), w.generatedChunks(), w.jobsInFlight(), s.registry().count());
    const auto& st = s.stats();
    out += std::format("Health {:.0f}  Sanity {:.1f}  Tempo x{:.2f}  Items {} im Inventar   Autosave in {} s\n",
                       st.health, st.sanity, st.speedMul(), s.inventory().count(),
                       std::max(0, (int)(game::GameSession::kAutosave - s.autosaveTimer)));
    out += std::format("Karte {} m² erkundet ({} Chunks)   Kamera {}   {}  vz {:+.1f}  Gang {:.2f}{}", s.explore().knownTiles(),
                       s.explore().chunkCount(), settings_.thirdPerson ? "Schulter" : "Ego",
                       p.onGround ? "am Boden" : "in der Luft", p.vz, p.gait, p.sprinting ? "  Sprint" : "");
    return out;
}

// --- Benchmark (wie V4 --benchmark): Autopilot, Zeiten ins Protokoll ---------------------------------
void App::runBenchmark() {
    while (!renderer_->texturesReady()) {
        input_.beginFrame();
        window_.pump();
        render(0.016);
    }
    renderer_->renderScene({}, {}, {});
    newGame("Benchmark", content_.defaultDifficulty());
    pilot_ = std::make_unique<game::Autopilot>(session_->player(), (u64)session_->seed);
    double total = 0, worst = 0;
    const double dt = 1.0 / 60.0;
    u64 sync0 = session_->world().syncWork();
    int slow = 0;
    for (int i = 0; i < opt_.benchmark && !quit_; ++i) {
        auto t0 = Clock::now();
        u64 syncBefore = session_->world().syncWork();
        input_.beginFrame();
        window_.pump();
        time_ += dt;
        updatePlaying(dt);
        double ft = std::chrono::duration<double>(Clock::now() - t0).count();
        if (i > 30) {
            total += ft;
            worst = std::max(worst, ft);
            // V6: Ausreisser mit Aufschluesselung protokollieren (Ursache von Rucklern finden)
            if (ft > 0.012 && slow++ < 12)
                log::info("Benchmark: Bild {} {:.1f} ms (Spiel {:.1f}, Grafik {:.1f}, synchron {})", i, ft * 1000.0, simMs_,
                          renderMs_, session_->world().syncWork() - syncBefore);
        }
    }
    log::info("Benchmark: {} synchron erzeugte Sektoren/Chunks, {} Bilder ueber 12 ms", session_->world().syncWork() - sync0,
              slow);
    int n = std::max(1, opt_.benchmark - 31);
    const auto& rs = renderer_->stats();
    std::string res = std::format(
        "Benchmark: {} Bilder, {}x{} intern, Mittel {:.2f} ms ({:.0f} FPS), langsamstes Bild {:.1f} ms, Strecke {:.0f} m, "
        "{} Chunks, {} Lichter",
        opt_.benchmark, rs.internalW, rs.internalH, total / n * 1000.0, n / std::max(total, 1e-6), worst * 1000.0,
        session_->player().distance, session_->world().loadedChunks(), rs.lights);
    log::info("{}", res);
    log::info("Benchmark: {}", renderer_->gpuTimer().summary());
    res += "\n" + renderer_->gpuTimer().summary();
    paths::writeFileAtomic(paths::join(paths::localDataDir(), "benchmark_v13.txt"), res + "\n", false);
}

// --- V13.2/V13.3 Koop -------------------------------------------------------------------------------
bool App::loadNet(std::string& error) {
    if (netApi_.ok()) return true;
    return coop::loadNetLibrary(netApi_, error);
}

// Pruefsumme der Spieldaten: Host und Gast muessen dieselbe Welt erzeugen (Version, Gegenstaende, Ebenen, Schwierigkeiten)
u32 App::coopBuild() const {
    u64 h = hash64(0x13020001ull, "LIMINAL V13.2 Koop", (u64)net::coop::kProtoVersion);
    for (const auto& it : content_.items()) h = hash64(h, it.key.c_str(), it.name.c_str());
    for (const auto& lv : content_.levels()) h = hash64(h, lv.def ? lv.def->id.c_str() : "");
    for (const auto& d : content_.difficulties()) h = hash64(h, d.key.c_str());
    return (u32)(h ^ (h >> 32));
}

int App::difficultyIndex(const std::string& key) const {
    const auto& d = content_.difficulties();
    for (size_t i = 0; i < d.size(); ++i)
        if (d[i].key == key) return (int)i;
    return 0;
}

bool App::hasCoopSave() {
    for (const auto& e : game::savegame::list())
        if (e.data["coop"].asBool(false)) return true;
    return false;
}

// koop.json: zuerst neben dem Spiel (vom Herausgeber), dann im Nutzerordner (eigene Lobby); MULTIPLAYER_SETUP.md
coop::NetSettings App::netSettings(bool useLobby, bool relayOnly) {
    coop::NetSettings ns;
    for (const std::string& dir : {paths::exeDir(), paths::localDataDir()}) {
        const std::string file = paths::join(dir, "koop.json");
        const auto text = paths::readFile(file);
        if (!text) continue;
        std::string perr;
        const auto j = Json::parse(*text, &perr);
        if (!j || !j->isObject()) {
            log::warn("koop.json ({}) nicht lesbar: {}", file, perr);
            continue;
        }
        if ((*j)["lobby"].isString()) ns.lobby = (*j)["lobby"].asString();
        // festgelegtes Zertifikat der Lobby: Fingerabdruck oder PEM-Datei neben koop.json
        if (const std::string c = (*j)["lobby_cert"].asString(std::string()); !c.empty()) {
            if (c.find(':') != std::string::npos && c.size() == 95) ns.lobbyCert = c;
            else if (const auto pem = paths::readFile(paths::join(dir, c))) ns.lobbyCert = *pem;
            else log::warn("koop.json: Zertifikat {} nicht lesbar", c);
        }
        if ((*j)["ice_servers"].isArray()) {
            ns.ice.clear();
            for (const Json& e : (*j)["ice_servers"].items())
                if (e.isString()) ns.ice += (ns.ice.empty() ? "" : " ") + e.asString();
        }
        if ((*j)["direct_port"].isNumber()) ns.directPort = (u16)std::clamp<long long>((*j)["direct_port"].asInt(27500), 1024, 65535);
        if ((*j)["relay_only"].isBool()) ns.relayOnly = (*j)["relay_only"].asBool(false);
    }
    ns.useLobby = useLobby;
    ns.relayOnly = ns.relayOnly || relayOnly;
    if (const char* b = std::getenv("LIMINAL_COOP_BIND")) ns.bind = b;  // nur Tests (zwei Spiele auf einem Rechner)
    return ns;
}

std::string App::coopLobbyName() {
    const coop::NetSettings ns = netSettings(true, false);
    std::string h = ns.lobby;
    if (const size_t p = h.find("://"); p != std::string::npos) h = h.substr(p + 3);
    return h.substr(0, h.find('/'));
}

void App::copyText(const std::string& text) { input::Input::setClipboardText(text); }

bool App::hostCoop(const std::string& name0, const std::string& difficulty, bool useLobby, bool relayOnly, bool resume, std::string& error) {
    std::string name;
    for (char c : name0)
        if (std::isalnum((unsigned char)c) || c == ' ' || c == '_' || c == '-') name += c;
    if (name.empty() || name.size() > 16) name = name.substr(0, 16);
    if (name.empty()) name = "Host";
    if (!loadNet(error)) return false;
    auto host = std::make_unique<coop::CoopHost>(netApi_, name, coopBuild());
    if (!host->start(netSettings(useLobby, relayOnly), 4, error)) return false;
    if (resume) {
        game::SaveEntry best;
        bool found = false;
        for (const auto& e : game::savegame::list())
            if (e.data["coop"].asBool(false) && !found) best = e, found = true;  // neueste zuerst
        if (!found || !loadGame(best)) {
            error = "Kein Koop-Stand gefunden.";
            return false;
        }
        host->loadGuests(best.data["coop_guests"]);
    } else {
        newGame(name, difficulty, "");
    }
    if (!session_ || state_ != State::Playing) {
        error = "Spiel konnte nicht starten.";
        return false;
    }
    coopHost_ = std::move(host);
    coopSave_ = true;
    coopHost_->attach(*session_, "", difficultyIndex(session_->difficulty));
    ui_.toast(useLobby ? std::string("Koop offen · der Beitrittscode steht gleich im Pausenmenü unter »Mitspieler«")
                       : std::format("Koop offen (direkt, Port {}) · Code im Pausenmenü unter »Mitspieler«", coopHost_->port()),
              8.0);
    log::info("Koop: Host gestartet ({})", useLobby ? "Lobby" : "direkt");
    return true;
}

bool App::joinCoop(const std::string& name, const std::string& code, bool useLobby, const std::string& address, bool relayOnly, std::string& error) {
    size_t symbols = 0;
    for (char ch : code) symbols += std::isalnum((unsigned char)ch) ? 1 : 0;
    if (symbols != 12 && symbols != 24) {
        error = "Beitrittscode: 12 Zeichen (über die Lobby) oder 24 Zeichen (lang, für direkt); Bindestriche egal.";
        return false;
    }
    if (!useLobby && address.empty()) {
        error = "Adresse des Hosts fehlt.";
        return false;
    }
    if (!loadNet(error)) return false;
    auto c = std::make_unique<coop::CoopClient>(netApi_, coopBuild());
    if (!c->connect(netSettings(useLobby, relayOnly), address, code, name, error)) return false;
    coopClient_ = std::move(c);
    coopName_ = name;
    coopJoinT_ = 0.0;
    ui_.coopMessage(coopClient_->progress());
    // im Spiel (Skript): sofort weiter warten; im Titel uebernimmt updateTitle
    return true;
}

void App::coopJoinPump(double dt) {
    coopJoinT_ += dt;
    coopClient_->pump();
    net::coop::Welcome w;
    if (coopClient_->takeWelcome(w)) {
        const auto& diffs = content_.difficulties();
        const std::string diff = diffs.empty() ? std::string("medium") : diffs[std::min<size_t>(w.difficulty, diffs.size() - 1)].key;
        ui_.setLoading(std::format("Lade {} …", w.level), 0);
        auto s = makeSession(w.seed, diff, coopName_, w.level);
        s->setSupplyScale(w.supply / 100.0);  // dieselben Vorraete wie beim Host
        attachSession(*s);
        s->runSeed = w.runSeed;
        s->slot.clear();
        s->placeAtSpawn();
        if (!w.anchor.empty()) s->placeAtAnchor(w.anchor);
        s->preload();
        enterPlaying(std::move(s));
        coopClient_->attach(*session_);
        coopSave_ = false;
        currentSlotPath_.clear();
        ui_.coopMessage("");
        const LnStats st = coopClient_->netStats();
        ui_.toast(std::format("Koop: beigetreten als Spieler {} ({})", coopClient_->selfId(),
                              st.path == LN_PATH_RELAY ? "über das Relay" : st.path == LN_PATH_NAT ? "direkt durch NAT" : "direkt"),
                  4.0);
        return;
    }
    if (coopClient_->state() != coop::CoopClient::State::Closed) ui_.coopMessage(coopClient_->progress());
    if (coopClient_->state() == coop::CoopClient::State::Closed || coopJoinT_ > 45.0) {
        const std::string why = coopClient_->state() == coop::CoopClient::State::Closed ? coopClient_->closeReason() : "Host antwortet nicht";
        ui_.coopMessage("Beitritt fehlgeschlagen: " + (why.empty() ? std::string("getrennt") : why));
        coopClient_->stop();
        coopClient_.reset();
    }
}

void App::coopBefore(double dt) {
    if (coopHost_) {
        coopHost_->beforeUpdate(dt);
        if (!coopKeyFile_.empty() && !coopHost_->code(true).empty()) {  // Skript (Tests): Code in die Datei
            if (FILE* f = std::fopen(coopKeyFile_.c_str(), "wb")) {
                std::fprintf(f, "%s", coopHost_->code(true).c_str());
                std::fclose(f);
            }
            coopKeyFile_.clear();
        }
        std::string why;
        if (coopHost_->takeFailure(why)) {  // Raum liess sich nicht oeffnen: zurueck ins Einzelspiel, Meldung
            ui_.toast("Koop nicht gestartet: " + why, 8.0);
            log::warn("Koop: Raum nicht offen ({})", why);
            leaveCoop(why);
        }
    }
    if (coopClient_ && coopClient_->state() != coop::CoopClient::State::Playing) {  // Beitritt aus dem Spiel heraus (Skript)
        coopJoinPump(dt);
        if (!coopClient_ && state_ == State::Playing) ui_.toast("Beitritt fehlgeschlagen.", 3.0);
        return;
    }
    if (coopClient_) {
        coopClient_->beforeUpdate(dt);
        if (coopClient_->state() == coop::CoopClient::State::Closed) {
            const std::string why = coopClient_->closeReason();
            leaveCoop(why);
            startTitleWorld();
            ui_.coopMessage("Verbindung beendet: " + (why.empty() ? std::string("getrennt") : why));
            ui_.openPage("coop");
        }
    }
}

bool App::coopAfter(double dt, const game::PlayerControl& ctl) {
    std::string lvl, anchor;
    if (coopHost_) {
        coopHost_->afterUpdate(dt);
        if (coopHost_->takeEscape()) {  // das Ende der Reise (Level 36): fuer alle
            session_->events().push_back({game::GameEvent::Escaped});
            handleEvents();
        }
        if (coopHost_->takeLevelChange(lvl, anchor)) {
            ui_.fadeToBlack(0.3);
            changeLevel(lvl, anchor);
            if (session_) coopHost_->attach(*session_, anchor, difficultyIndex(session_->difficulty));
            return false;
        }
    }
    if (coopClient_) {
        coopClient_->afterUpdate(dt, ctl);
        if (coopClient_->takeEscape()) {
            session_->events().push_back({game::GameEvent::Escaped});
            handleEvents();
        }
        if (coopClient_->takeLevelChange(lvl, anchor)) {
            changeLevel(lvl, anchor);
            if (session_) coopClient_->attach(*session_);
            return false;
        }
    }
    return true;
}

void App::leaveCoop(const std::string& reason) {
    if (coopHost_) {
        coopHost_->stop();
        coopHost_.reset();
        log::info("Koop: Host beendet");
    }
    if (coopClient_) {
        coopClient_->stop();
        coopClient_.reset();
        log::info("Koop: verlassen{}", reason.empty() ? "" : " (" + reason + ")");
    }
    if (session_) {
        session_->netRole = game::GameSession::NetRole::Solo;
        session_->pawns.clear();
        session_->netAction = nullptr;
    }
    coopWasDead_ = false;
}

void App::kickCoop(int id, bool ban) {
    if (coopHost_) coopHost_->kick(id, ban);
}

ui::Host::CoopView App::coopView() {
    CoopView v;
    if (!coop() || !session_) return v;
    v.active = true;
    v.host = coopHost_ != nullptr;
    std::vector<coop::PlayerInfo> ps;
    if (coopHost_) {
        v.code = coopHost_->code(false);
        v.codeLong = coopHost_->code(true);
        v.direct = coopHost_->direct();
        v.lobbyStatus = coopHost_->lobbyStatus();
        v.port = coopHost_->port();
        v.status = coopHost_->status();
        ps = coopHost_->players();
        v.dead = coopHost_->selfDead();
        v.respawnIn = (int)std::ceil(coopHost_->selfRespawnIn());
        if (game::Pawn* p = coopHost_->pawnOf(coopHost_->spectate())) v.spectating = p->name;
    } else {
        v.status = coopClient_->status();
        ps = coopClient_->players();
        v.dead = coopClient_->dead();
        v.respawnIn = coopClient_->respawnIn();
        for (const auto& p : ps)
            if (p.id == coopClient_->spectate()) v.spectating = p.name;
    }
    if (coopClient_ && !ps.empty()) {  // Gast: die eigene Verbindung zum Host
        const LnStats st = coopClient_->netStats();
        for (auto& p : ps)
            if (p.id == 0) p.rttMs = (int)st.rttMs, p.path = st.path, p.lossPct = (int)std::lround(st.loss * 100.0f);
    }
    for (const auto& p : ps) v.players.push_back({p.id, p.name, p.alive, p.ready, p.connected, p.self, p.rttMs, p.path, p.lossPct});
    return v;
}

}  // namespace lim
