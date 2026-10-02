// Anwendung: verbindet alle Subsysteme und steuert den Ablauf
// (Laden -> Hauptmenue -> Spiel mit Pause/Inventar/Tod).
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "app/autotune.hpp"
#include "app/settings.hpp"
#include "app/soundscape.hpp"
#include "audio/audio.hpp"
#include "core/jobs.hpp"
#include "core/paths.hpp"
#include "gameplay/autopilot.hpp"
#include "gameplay/content.hpp"
#include "gameplay/prefabs.hpp"
#include "gameplay/savegame.hpp"
#include "gameplay/session.hpp"
#include "input/input.hpp"
#include "platform/window.hpp"
#include "render/renderer.hpp"
#include "ui/host.hpp"
#include "ui/ui.hpp"

namespace lim {

struct Options {
    long long seed = -1;
    std::string name, difficulty, level;
    bool demo = false;
    int benchmark = 0;          // Bilder
    std::string shot;           // Screenshot-Datei (automatisch, dann beenden)
    int shotFrames = 120;
    int width = 0, height = 0;
    bool warp = false, windowed = false, debug = false, noMouse = false, fast = false;
    bool fixedDt = false;       // V6 (Tests): feste Simulationszeit 1/60 s je Bild -> reproduzierbare Aufnahmen
    int quality = -1;           // V6 (Tests): Qualitaetsstufe 0..3 fuer diesen Start (--quality)
    bool noHud = false;         // V6 (Tests): Aufnahmen ohne Oberflaeche (--no-hud)
    int mode = -1;
    double quitAfter = 0.0;
    double yaw = 1e9, pitch = 1e9, posX = 1e9, posY = 1e9;
    std::string saveFile;       // bestimmten Spielstand laden
    std::string room;           // Test: im naechsten Raum dieses Typs beginnen
    std::string shotUi;         // Test: Oberflaeche vor der Aufnahme oeffnen (title, pause, inventory, dead, help, graphics, new, load)
    Json overrides = Json::object();  // --set schluessel=wert (Einstellungen fuer diese Sitzung)
    // Test: Eingaben abspielen und Zustaende pruefen, Befehle mit ';' getrennt:
    //   wait N | key Name | text Zeichen | expect Modus[/Seite] | shot Datei.png | quit
    // Exitcode 7, wenn ein "expect" fehlschlaegt.
    std::string script;
};

class App final : public ui::Host {
public:
    App();
    ~App() override;
    int run(const std::vector<std::string>& args);

    // --- ui::Host -------------------------------------------------------------------
    Settings& settings() override { return settings_; }
    void settingsChanged() override;
    const game::Content& content() override { return content_; }
    game::GameSession* session() override { return state_ == State::Playing ? session_.get() : nullptr; }
    gfx::Renderer& renderer() override { return *renderer_; }
    input::Input& input() override { return input_; }
    void newGame(const std::string& name, const std::string& difficulty, const std::string& level = "") override;
    bool loadGame(const game::SaveEntry& e) override;
    void saveGame() override;
    void toTitle() override;
    void quit() override { quit_ = true; }
    void playSound(const std::string& name, float volume = 1.0f) override;
    double time() const override { return time_; }
    bool hasSave() const override { return !currentSlotPath_.empty(); }
    void loadLastSave() override;
    float fps() const override { return fps_; }
    std::string debugInfo() override;

private:
    enum class State { Loading, Title, Playing };
    bool parseArgs(const std::vector<std::string>& args);
    bool init(std::string& error);
    void mainLoop();
    void frame(double dt);
    void updatePlaying(double dt);
    void updateTitle(double dt);
    void render(double dt);
    void renderWorld(double dt, bool clean);
    game::PlayerControl gatherControl(double dt);
    void handleEvents();
    void startTitleWorld();
    std::unique_ptr<game::GameSession> makeSession(long long seed, const std::string& difficulty,
                                                   const std::string& name, const std::string& level,
                                                   int layout = world::kLayoutV6);
    void attachSession(game::GameSession& s);
    void enterPlaying(std::unique_ptr<game::GameSession> s);
    void takeScreenshot(const std::string& path);
    float fovY(float mul = 1.0f) const;
    void updateAudio(double dt);
    void runBenchmark();
    void scriptStep();
    bool testRun() const { return !opt_.shot.empty() || opt_.benchmark > 0 || !opt_.script.empty(); }

    Options opt_;
    Settings settings_;
    game::Content content_;
    game::PrefabLibrary prefabs_;
    std::unique_ptr<JobSystem> jobs_;
    input::Input input_;
    plat::Window window_;
    std::unique_ptr<gfx::Renderer> renderer_;
    std::unique_ptr<audio::AudioSystem> audio_;
    std::unique_ptr<game::GameSession> session_;
    std::unique_ptr<game::Autopilot> pilot_;
    ui::Ui ui_;

    State state_ = State::Loading;
    bool quit_ = false;
    double time_ = 0.0;
    float fps_ = 0.0f;
    double pendYaw_ = 0.0, pendPitch_ = 0.0;
    std::string currentSlot_, currentSlotPath_;
    int framesInPlay_ = 0;
    int audioGen_ = 0;
    bool resetHistory_ = true;
    plat::DisplayMode lastWinMode_ = plat::DisplayMode::Windowed;  // erkennt Alt+Enter
    std::vector<gfx::MeshDraw> draws_;
    std::vector<game::VisibleMesh> visible_;
    std::vector<game::SoundSource> sounds_;
    app::Soundscape soundscape_;  // V6: Leuchten, Raumakustik, ferne Klaenge
    app::AutoTune autoTune_;      // V6: Grafik passt sich der Hardware an
    int appliedWinW_ = 0, appliedWinH_ = 0;
    // V7: Notausgang - Abblenden, dann die naechste Ebene
    std::string pendingLevel_;
    double exitTimer_ = 0.0;
    std::vector<std::pair<vec3, vec3>> exitSigns_;  // V7: Schilder in der Naehe (Lichtquellen)
    void changeLevel(const std::string& next);
    mat4 prevBodyRoot_ = mat4::identity();  // V6: TAA fuer den eigenen Koerper
    bool havePrevRoot_ = false;
    double simMs_ = 0, renderMs_ = 0;       // V6: Zeitanteile des letzten Bildes (Debug-Anzeige, Benchmark)
    int exhaustHints_ = 0;                  // V6: "Ausser Atem" nur die ersten Male als Meldung

    // --script
    std::vector<std::string> script_;
    size_t scriptPos_ = 0;
    int scriptWait_ = 0;
    std::vector<int> scriptRelease_;
    std::string scriptShot_;
    bool scriptFailed_ = false;
};

}  // namespace lim
