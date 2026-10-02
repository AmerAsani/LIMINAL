#include "app/app.hpp"

#include <chrono>
#include <cmath>
#include <ctime>
#include <format>
#include <random>
#include <thread>

#include "core/log.hpp"
#include "platform/image_io.hpp"
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
        else if (k == "--windowed") opt_.windowed = true;
        else if (k == "--debug") opt_.debug = true;
        else if (k == "--no-mouse") opt_.noMouse = true;
        else if (k == "--fast") opt_.fast = true;
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
    log::init(paths::join(paths::join(paths::localDataDir(), "logs"), "liminal.log"));
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
    if (state_ == State::Loading) return;
    if (scriptWait_ > 0) {
        --scriptWait_;
        return;
    }
    static const char* modes[] = {"Loading", "Title", "Hud", "Pause", "Inventory", "Dead"};
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

    int w = opt_.width ? opt_.width : 1600, h = opt_.height ? opt_.height : 900;
    bool fullscreen = settings_.fullscreen && !opt_.windowed && opt_.width == 0 && opt_.shot.empty();
    if (!window_.create(L"LIMINAL", w, h, fullscreen ? plat::DisplayMode::Borderless : plat::DisplayMode::Windowed, &input_)) {
        err = "Das Fenster konnte nicht erstellt werden.";
        return false;
    }
    lastWinMode_ = window_.displayMode();
    renderer_ = std::make_unique<gfx::Renderer>();
    if (!renderer_->init(window_, *jobs_, settings_.render, opt_.warp, err)) return false;
    if (renderer_->gpuInfo().warp)
        log::warn("Software-Rendering (WARP) aktiv - Grafikqualitaet wird reduziert"), settings_.applyQualityPreset(0),
            renderer_->applySettings(settings_.render);
    renderer_->startTextureGeneration();

    audio_ = std::make_unique<audio::AudioSystem>();
    audio_->init(*jobs_, paths::dataDir());
    audio_->setVolumes(settings_.masterVolume, settings_.effectsVolume, settings_.ambienceVolume);

    ui_.setMode(ui::Mode::Loading);
    if (opt_.debug) ui_.toggleDebug();
    return true;
}

// --- Sitzungen -----------------------------------------------------------------------------------
std::unique_ptr<game::GameSession> App::makeSession(long long seed, const std::string& difficulty,
                                                    const std::string& name, const std::string& level) {
    auto lvl = content_.level(level.empty() ? "level0" : level);
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
    session_ = makeSession(7, content_.defaultDifficulty(), "", "level0");
    attachSession(*session_);
    session_->placeAtSpawn();
    session_->preload();
    renderer_->syncMaterials(session_->world().bank());
    state_ = State::Title;
    ui_.setMode(ui::Mode::Title);
    resetHistory_ = true;
}

void App::enterPlaying(std::unique_ptr<game::GameSession> s) {
    if (audio_) audio_->stopAll();
    ++audioGen_;
    session_ = std::move(s);
    state_ = State::Playing;
    ui_.setMode(ui::Mode::Hud);
    framesInPlay_ = 0;
    resetHistory_ = true;
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
                        if (r->type->key == key && (zoneKey.empty() || s->world().level().zones[(size_t)r->zone].key == zoneKey)) {
                            auto [cx, cy] = r->center();
                            s->player().x = cx;
                            s->player().y = cy;
                            s->player().z = r->floor;
                            s->player().angle = r->w() >= r->h() ? 0.0 : kPi * 0.5;
                            if (r->w() >= r->h()) s->player().x = r->x0 + 1.5;
                            else s->player().y = r->y0 + 1.5;
                            if (auto m = r->machineTile()) {
                                // Versorgungsraum: in die Raummitte, Blick auf Automat und Theke
                                s->player().x = cx - r->machineDirX * 0.8;
                                s->player().y = cy - r->machineDirY * 0.8;
                                double mx = m->x + 0.5 - s->player().x, my = m->y + 0.5 - s->player().y;
                                s->player().angle = std::atan2(my, mx);
                                s->player().pitch = -0.25;
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
    ui_.toast(std::format("{}  ·  {}  ·  Seed {}", session_->name, content_.difficulty(difficulty).label, session_->seed), 3.0);
    if (opt_.shot.empty() && opt_.benchmark == 0) saveGame();
}

bool App::loadGame(const game::SaveEntry& e) {
    const Json& d = e.data;
    auto s = makeSession(d["seed"].asInt(), d["difficulty"].asString("medium"), d["name"].asString("Wanderer"),
                         d["level"].asString("level0"));
    attachSession(*s);
    s->fromJson(d);
    // Alte Staende (V3/V4) werden als neuer V5-Slot weitergefuehrt
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
    c.turn = (in.actionDown(Action::LookRight) ? 1.0f : 0.0f) - (in.actionDown(Action::LookLeft) ? 1.0f : 0.0f);
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
        ui.toast("Energy Bar aufgenommen", 60);
        ui.toggleMap();
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

void App::updatePlaying(double dt) {
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
        for (int k = 0; k < 9; ++k)
            if (input_.actionPressed((Action)((int)Action::Hotbar1 + k))) s.selectSlot(k);
        if (input_.actionPressed(Action::HotbarNext)) s.selectSlot((s.inventory().selected + 1) % 9);
        if (input_.actionPressed(Action::HotbarPrev)) s.selectSlot((s.inventory().selected + 8) % 9);
        if (input_.actionPressed(Action::Map)) ui_.toggleMap();
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
    s.update(dt, ctl, paused);
    renderer_->syncMaterials(s.world().bank());
    handleEvents();
    if (s.stats().dead && ui_.mode() != ui::Mode::Dead && ui_.mode() != ui::Mode::Title) ui_.setMode(ui::Mode::Dead);
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
    render(dt);
    if (!opt_.shot.empty() && framesInPlay_ >= opt_.shotFrames && renderer_->texturesReady()) {
        takeScreenshot(opt_.shot);
        quit_ = true;
    }
}

void App::handleEvents() {
    auto& evs = session_->events();
    for (const auto& e : evs) {
        switch (e.type) {
            case game::GameEvent::Toast: ui_.toast(e.text, e.duration); break;
            case game::GameEvent::ZoneTitle: ui_.zoneTitle(e.text); break;
            case game::GameEvent::Pickup:
                ui_.toast(e.text, e.duration);
                audio_->play("pickup", 0.9f);
                break;
            case game::GameEvent::InventoryFull: ui_.toast(e.text, e.duration); break;
            case game::GameEvent::Consume:
                ui_.toast(e.text, e.duration);
                if (!e.sound.empty()) audio_->play(e.sound, 0.9f);
                break;
            case game::GameEvent::Footstep: {
                static const char* names[] = {"step_carpet", "step_concrete", "step_tile", "step_metal", "step_wood"};
                static u32 n = 0;
                ++n;
                std::string name = std::format("{}_{}", names[(int)e.surface], hash32(n) % 4);
                audio_->play(name, 0.35f + 0.45f * e.intensity, 0.94f + 0.12f * hash01(n * 7u));
                break;
            }
            case game::GameEvent::Land: audio_->play("land", 0.4f + 0.6f * e.intensity); break;
            case game::GameEvent::Died:
                audio_->play("death", 1.0f);
                break;
            case game::GameEvent::AutosaveDue: saveGame(); break;
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
    static int humId = -1, heartId = -1;
    static int zoneIds[world::kMaxZones];
    static int boundGen = -1;
    const auto& zones = s.world().level().zones;
    if (boundGen != audioGen_) {
        // neue Sitzung: Dauerklaenge neu starten (Atmosphaere je Zone ueber ihren Schluessel)
        boundGen = audioGen_;
        humId = audio_->startLoop("hum", 0.0f);
        heartId = -1;
        for (size_t i = 0; i < (size_t)world::kMaxZones; ++i)
            zoneIds[i] = i < zones.size() ? audio_->startLoop("amb_" + zones[i].key, 0.0f) : -1;
    }
    const world::Room* room = s.currentRoom();
    float humTarget = 0.0f;
    if (room) {
        const std::string& ls = room->lightStyle;
        humTarget = (ls == "panels" || ls == "panels_wide" || ls == "panels_long" || ls == "bright") ? 0.22f
                    : ls == "dark" || ls == "void" ? 0.03f
                                                   : 0.1f;
        for (size_t i = 0; i < zones.size(); ++i)
            if (zoneIds[i] >= 0) audio_->setLoopVolume(zoneIds[i], (float)room->zoneW[i] * 0.5f);
    }
    static float hum = 0.0f;
    hum += (humTarget - hum) * (float)std::min(1.0, dt * 1.5);
    audio_->setLoopVolume(humId, hum);
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
            audio_->setLoopPosition(*src.voice, src.pos, src.radius);
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
}

float App::fovY() const {
    float aspect = (float)window_.width() / (float)std::max(1, window_.height());
    if (settings_.fov > 0.0f) return 2.0f * std::atan(std::tan(settings_.fov * kPi / 360.0f) / aspect);
    // automatisch: senkrecht etwa 62 Grad, horizontal hoechstens 105 Grad
    float v = 62.0f * kPi / 180.0f;
    float hMax = 105.0f * kPi / 180.0f;
    float h = 2.0f * std::atan(std::tan(v * 0.5f) * aspect);
    if (h > hMax) v = 2.0f * std::atan(std::tan(hMax * 0.5f) / aspect);
    return v;
}

void App::render(double dt) {
    renderer_->ui().begin(renderer_->outputWidth(), renderer_->outputHeight());
    renderWorld(dt, false);
    ui_.frame(*this, dt);
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
        gfx::CameraState cam;
        cam.pos = {(float)p.x, (float)p.y, (float)p.eye()};
        cam.yaw = (float)(p.angle + p.sway());
        cam.pitch = (float)p.pitch;
        cam.fovY = fovY();
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
        // Stimmung: V4-Helligkeit des Raums steuert die Belichtung mit (Auge gewoehnt sich langsam)
        static float mood = 1.0f;
        if (const world::Room* room = s.currentRoom()) {
            float target = std::clamp(0.42f + 0.62f * (float)room->avgLight, 0.45f, 1.15f);
            mood += (target - mood) * (float)std::min(1.0, dt * 0.8);
        }
        sp.moodExposure = mood;
        sp.resetHistory = resetHistory_;
        resetHistory_ = false;
        visible_.clear();
        s.collectMeshes(visible_);
        draws_.clear();
        for (auto& v : visible_) {
            if (*v.meshId == -2) *v.meshId = renderer_->meshId(*v.model);
            if (*v.meshId >= 0) draws_.push_back({*v.meshId, v.world, v.highlight});
        }
        if (clean) sp.menuDim = sp.redEdge = sp.desaturate = 0.0f;
        renderer_->renderScene(cam, sp, draws_);
    }
}

void App::takeScreenshot(const std::string& path) {
    std::vector<u8> rgba;
    int w, h;
    if (renderer_->captureBackbuffer(rgba, w, h) && gfx::savePng(path, rgba.data(), w, h)) {
        ui_.toast("Screenshot gespeichert: " + path, 3.0);
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
        out += std::format("Zone: {} {:.0f}%   Licht {}  Grundhelligkeit {:.2f}   Türen {}\n", lv.zones[(size_t)r->zone].name,
                           r->zoneW[(size_t)r->zone] * 100.0, r->lightStyle, r->ambient, r->doors.size());
    }
    auto& w = s.world();
    out += std::format("Geladen: {} Sektoren  {} Räume  {} Chunks   erzeugt {}   Jobs {}   Entities {}\n", w.loadedSectors(),
                       w.loadedRooms(), w.loadedChunks(), w.generatedChunks(), w.jobsInFlight(), s.registry().count());
    const auto& st = s.stats();
    out += std::format("Health {:.0f}  Sanity {:.1f}  Tempo x{:.2f}  Items {} im Inventar   Autosave in {} s",
                       st.health, st.sanity, st.speedMul(), s.inventory().count(),
                       std::max(0, (int)(game::GameSession::kAutosave - s.autosaveTimer)));
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
    for (int i = 0; i < opt_.benchmark && !quit_; ++i) {
        auto t0 = Clock::now();
        input_.beginFrame();
        window_.pump();
        time_ += dt;
        updatePlaying(dt);
        double ft = std::chrono::duration<double>(Clock::now() - t0).count();
        if (i > 30) {
            total += ft;
            worst = std::max(worst, ft);
        }
    }
    int n = std::max(1, opt_.benchmark - 31);
    const auto& rs = renderer_->stats();
    std::string res = std::format(
        "Benchmark: {} Bilder, {}x{} intern, Mittel {:.2f} ms ({:.0f} FPS), langsamstes Bild {:.1f} ms, Strecke {:.0f} m, "
        "{} Chunks, {} Lichter",
        opt_.benchmark, rs.internalW, rs.internalH, total / n * 1000.0, n / std::max(total, 1e-6), worst * 1000.0,
        session_->player().distance, session_->world().loadedChunks(), rs.lights);
    log::info("{}", res);
    paths::writeFileAtomic(paths::join(paths::localDataDir(), "benchmark.txt"), res + "\n", false);
}

}  // namespace lim
