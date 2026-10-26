#include "app/app.hpp"

#include <chrono>
#include <cmath>
#include <cstring>
#include <ctime>
#include <format>
#include <random>
#include <thread>

#include "core/log.hpp"
#include "platform/image_io.hpp"
#include "render/world_renderer.hpp"
#include "world/room.hpp"

#include <windows.h>

namespace lim {

using input::Action;
using Clock = std::chrono::steady_clock;

App::App() = default;

App::~App() {
    // Reihenfolge: Sitzung (wartet auf Weltjobs) vor Renderer und Jobs
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
int App::run(const std::vector<std::string>& args) {
    parseArgs(args);
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
            const int kind = n == "hound" ? 1 : n == "moth" ? 2 : n == "gestalt" ? 3 : n == "nest" ? 4 : n == "clump" ? 5 : n == "player" ? 6 : 0;
            auto& pl = session_->player();
            vec3 at{(float)(pl.x + std::cos(pl.angle) * dist), (float)(pl.y + std::sin(pl.angle) * dist), 0.0f};
            at.z = session_->world().tile(ifloor(at.x), ifloor(at.y)).floor;
            session_->presence().showcase(kind, at, (float)(pl.angle + kPi + turn), (float)(walk > 0.0 && walk < 1.01 ? 1.2 : walk));
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
                session_->presence().force(r, dist);
            }
        } else if (cmd == "audioinfo") {  // V11 (Tests): Zustand der Klanglandschaft ins Protokoll
            const audio::Acoustics& ac = soundscape_.acoustics();
            log::info("Audio: Leuchten {} Betten {} Stellen {} Brummen x{:.2f} | Raum t60 {:.2f} wet {:.2f} early {:.2f} Masse {:.0f}x{:.0f}x{:.1f} Hoerer {:.1f},{:.1f},{:.1f}",
                      soundscape_.activeLamps(), soundscape_.activeBeds(), soundscape_.activeSpots(), soundscape_.humSurge(), ac.t60, ac.wet,
                      ac.early, ac.roomW, ac.roomD, ac.roomH, ac.lx, ac.ly, ac.lz);
        } else if (cmd == "perf") {  // V10 (Tests): Zeiten und Dreiecke dieses Bildes ins Protokoll
            const auto& rs = renderer_->stats();
            log::info("Skript: perf Spiel {:.2f} ms, Grafik {:.2f} ms, {} Dreiecke, {} Netze | {}", simMs_, renderMs_, rs.triangles, rs.meshes,
                      renderer_->gpuTimer().summary());
        } else if (cmd == "give" && session_) {  // V9: Item ins Inventar (Tests)
            session_->inventory().add(arg);
        } else if (cmd == "health" && session_) {  // V9: Health setzen (Tests)
            session_->stats().health = std::clamp(std::atof(arg.c_str()), 1.0, 100.0);
        } else if (cmd == "sanity" && session_) {  // V7: Sanity setzen (Tests)
            session_->stats().sanity = std::clamp(std::atof(arg.c_str()), 0.0, 100.0);
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
    for (size_t a = 0, b; a <= opt_.script.size() && !opt_.script.empty(); a = b + 1) {
        b = opt_.script.find(';', a);
        if (b == std::string::npos) b = opt_.script.size();
        script_.push_back(opt_.script.substr(a, b - a));
    }
    if (opt_.fast) settings_.render.renderScale = 0.5f;
    if (opt_.mode >= 0) settings_.render.displayMode = opt_.mode;
    if (!content_.load(paths::dataDir(), err)) return false;
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
    if (audio_) audio_->stopAll();
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
    if (audio_) audio_->stopAll();
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
    Json data = session_->toJson();
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
        session_->player().walkSpeed = settings_.walkSpeed;
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
        if (!window_.pump() || window_.closeRequested()) quit_ = true;
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
        if (window_.minimized()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            continue;
        }
        frame(dt);
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
    game::GameSession& s = *session_;
    ++framesInPlay_;
    if (!opt_.shotUi.empty() && framesInPlay_ == 3) {
        prepareShotUi(opt_.shotUi, ui_, &s);
        if (!opt_.script.empty()) opt_.shotUi.clear();  // Skript: nur einmal (nicht erneut nach dem Laden)
    }
    // Fenster verlassen (Alt+Tab, Klick daneben): Spiel pausieren, damit die Sanity nicht im
    // Hintergrund weiter sinkt
    if (!window_.focused() && ui_.mode() == ui::Mode::Hud && !testRun() && !pilot_)
        ui_.openPause();
    bool paused = ui_.pausesGame();
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
    if (s.stats().dead && deathDelay_ <= 0.0 && ui_.mode() != ui::Mode::Dead && ui_.mode() != ui::Mode::Title) {
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
    auto at = [&](const std::string& name, vec3 pos, float volume, float radius, float pitch = 1.0f) {
        audio::PlayOpts o;
        o.volume = volume, o.pitch = pitch, o.vary = false;
        audio_->play3d(name, soundscape_.spatial(session_->world(), ear, pos, radius), o);
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
                ui_.toast(e.text, e.duration);
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
            case game::GameEvent::Blackout:
                if (e.index == 1) at("power_off", e.pos, 0.75f, 20.0f);
                else if (e.index == 0) at("power_on", e.pos, 0.6f, 20.0f);
                else if (e.index == 3) at("breaker_far", e.pos, 1.0f, 90.0f);  // V11: Welle, ein Schuetz faellt
                else at("lamp_tick", e.pos, 0.6f, 30.0f, 0.85f);
                break;
            case game::GameEvent::GhostSeen: audio_->play("hal_whisper", 0.5f); break;
            // V8: eine neue Ebene - erst faellt hinten eine Tuer zu, viel spaeter kennt man ihren Namen
            case game::GameEvent::DistantSlam: at("dist_door", e.pos, 0.85f, 70.0f, 0.88f); break;
            case game::GameEvent::SoundAt: at(e.sound, e.pos, e.intensity, 18.0f); break;  // V8: Aufzug
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
                switch ((PE::Type)e.index) {
                    case PE::Step:  // Hound: flache Handflaechen und Krallen auf dem Boden (ein Klang, Varianten)
                        if (role == Role::Hound) at("hound_step", e.pos, e.intensity * 0.9f, 22.0f, 0.95f + 0.1f * var);
                        // V11 Buendel: nasse Haende klatschen auf den Boden
                        if (role == Role::Clump)
                            at(std::format("clump_slap_{}", n % 5), e.pos, 0.35f + 0.4f * e.intensity, 16.0f, 0.92f + 0.16f * var);
                        break;
                    // V11 Buendel: Klopfen im Schacht, Herabfallen, Schleifen, Ausholen
                    case PE::Knock: at(std::format("clump_knock_{}", n % 3), e.pos, 0.9f, 30.0f); break;
                    case PE::Drop:
                        at(std::format("clump_slap_{}", n % 5), e.pos, 1.0f, 26.0f, 0.8f);
                        at(std::format("clump_crack_{}", n % 3), e.pos, 0.8f, 22.0f);
                        break;
                    case PE::Drag: at(std::format("clump_drag_{}", n % 2), e.pos, 0.55f, 18.0f, 0.9f + 0.2f * var); break;
                    case PE::Windup: at("clump_reach", e.pos, 1.0f, 20.0f); break;
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
                    case PE::Announce: at("smiler_open", e.pos, 1.0f, 30.0f); break;
                    case PE::Calm: at("smiler_fade", e.pos, 0.4f, 20.0f); break;
                    case PE::Lost: break;
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
    audio_->setPaused(ui_.pausesGame());
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
                       state_ == State::Playing && !ui_.pausesGame());
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
        if (!st.dead) audio_->setMuffle(uw ? 0.8f : 0.0f);
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
    // V6: Keuchen nach dem Sprinten. V11: einzelne Atemzuege (Ein- und Ausatmen), je atemloser desto
    // schneller und lauter
    float puff = (float)st.breathless();
    breathTimer -= dt;
    if (puff > 0.06f && !st.dead && breathTimer <= 0.0 && !ui_.pausesGame()) {
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
            game::ViewCamera vc = s.camera(third, clean ? 0.0 : dt);
            cam.pos = vc.pos;
            cam.yaw = vc.yaw;
            cam.pitch = vc.pitch;
            cam.roll = vc.roll;
            cam.fovY = fovY(vc.fovMul);
        } else {
            cam.pos = {(float)p.x, (float)p.y, (float)p.eye()};
            cam.yaw = (float)(p.angle + p.sway());
            cam.pitch = (float)p.pitch;
            cam.fovY = fovY();
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
        // V11: Schreck - weisser Blitz in den ersten Bildern, roter Rand, danach fast schwarz
        if (playing && scareT_ >= 0.0) {
            const float pop = std::exp(-(float)scareT_ * 11.0f);
            sp.moodExposure *= 1.0f + 3.0f * pop;
            sp.desaturate = std::max(sp.desaturate, 0.35f);
            sp.redEdge = 1.0f;
            if (scareCut_) sp.menuDim = std::max(sp.menuDim, 0.97f);
        }
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
            if (*v.meshId == -2) *v.meshId = renderer_->meshId(*v.model);
            if (*v.meshId >= 0) {
                gfx::MeshDraw md{*v.meshId, v.world, v.highlight, v.attached};
                md.bones = v.bones;
                md.dynamic = v.dynamic;
                md.cullCenter = v.cullCenter;
                md.cullRadius = v.cullRadius;
                md.material = v.material == game::kShadowLook ? (int)gfx::FM_ENT_SHADOW : v.material;  // V10
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
                L.range = 24.0f;
                L.spot = true;
                L.cosInner = 0.975f;  // ~13 Grad heller Kern
                L.cosOuter = 0.885f;  // ~28 Grad weicher Rand
                L.soft = 0.3f;
                L.size = 0.012f;
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
        // V7: Stromausfall (Halluzination)
        sp.blackoutSeed = s.blackout().seed;
        sp.blackoutAmount = s.blackout().amount;
        sp.blackoutOrigin = s.blackout().origin;  // V11: Welle
        sp.blackoutFront = s.blackout().front;
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
        if (clean) sp.menuDim = sp.redEdge = sp.desaturate = 0.0f;
        renderer_->renderScene(cam, sp, draws_);
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

}  // namespace lim
