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
#include "coop/coop.hpp"
#include "coop/netapi.hpp"
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
    std::string dumpSkeletons;  // V13: Skelette der Wesen als JSON schreiben und beenden (Blender-Pipeline)
    std::string room;           // Test: im naechsten Raum dieses Typs beginnen
    std::string shotUi;         // Test: Oberflaeche vor der Aufnahme oeffnen (title, pause, inventory, dead, help, graphics, new, load)
    Json overrides = Json::object();  // --set schluessel=wert (Einstellungen fuer diese Sitzung)
    // Test: Eingaben abspielen und Zustaende pruefen, Befehle mit ';' getrennt:
    //   wait N | key Name | text Zeichen | expect Modus[/Seite] | shot Datei.png | quit
    // Exitcode 7, wenn ein "expect" fehlschlaegt.
    std::string script;
    std::string levelFile;  // V13.3 (Kontaktboegen): zusaetzliches Level aus einer Datei laden und starten
};

namespace app {
// V13.3: Dauerklaenge des Wranglers - Rumpeln, Leib, Sog (app/wrangler_audio.cpp)
void wranglerLoops(audio::AudioSystem& au, Soundscape& ss, game::GameSession& s, vec3 ear, int gen, bool dead);
}  // namespace app

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
    bool canRespawn() const override;  // V11
    void respawn() override;
    void loadLastSave() override;
    float fps() const override { return fps_; }
    std::string debugInfo() override;
    // V13.2 Koop (Beta)
    CoopView coopView() override;
    bool hasCoopSave() override;
    bool hostCoop(const std::string& name, const std::string& difficulty, bool useLobby, bool relayOnly, bool resume, std::string& error) override;
    bool joinCoop(const std::string& name, const std::string& code, bool useLobby, const std::string& address, bool relayOnly, std::string& error) override;
    std::string coopLobbyName() override;
    void copyText(const std::string& text) override;
    void kickCoop(int id, bool ban) override;

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
    std::vector<vec4> boneRows_;                 // V10: Knochenmatrizen der Figuren
    std::vector<game::ShadowCaster> casters_;    // V10: Schattenkapseln
    app::Soundscape soundscape_;  // V6: Leuchten, Raumakustik, ferne Klaenge
    float dustFall_ = 0.0f;       // V13.3: Beben (Wrangler) - gefallener Staub (m)
    app::AutoTune autoTune_;      // V6: Grafik passt sich der Hardware an
    int appliedWinW_ = 0, appliedWinH_ = 0;
    // V7: Notausgang - Abblenden, dann die naechste Ebene
    std::string pendingLevel_, pendingAnchor_;  // V11: Anker des Uebergangs
    double exitTimer_ = 0.0;
    std::vector<std::pair<vec3, vec3>> exitSigns_;  // V7: Schilder in der Naehe (Lichtquellen)
    std::vector<vec3> elevLights_;                  // V8: Kabinenlichter
    // V9: Sicherungskaesten (oranges Flackern), Abspann nach der letzten Fahrt, Zugriff des Anderen
    std::vector<game::GameSession::FuseLight> fuseLights_;
    double endingTimer_ = 0.0;
    float hitFlash_ = 0.0f;
    // V11: Schreck beim toedlichen Zugriff, danach der Todesschirm
    double scareT_ = -1.0, deathDelay_ = 0.0;
    bool scareCut_ = false;
    void changeLevel(const std::string& next, const std::string& anchor = "");
    mat4 prevBodyRoot_ = mat4::identity();  // V6: TAA fuer den eigenen Koerper
    bool havePrevRoot_ = false;
    double simMs_ = 0, renderMs_ = 0;       // V6: Zeitanteile des letzten Bildes (Debug-Anzeige, Benchmark)
    int exhaustHints_ = 0;                  // V6: "Ausser Atem" nur die ersten Male als Meldung

    // --script
    std::vector<std::string> script_;
    size_t scriptPos_ = 0;
    int scriptWait_ = 0;
    std::vector<int> scriptRelease_;
    std::vector<std::pair<int, int>> scriptHold_;  // V11: gehaltene Tasten (Code, verbleibende Bilder)
    std::string scriptShot_;
    // V13 (Tests): Debug-Kamera um die Hand mit der Lampe: Winkel um die Lampenachse (az, el in Grad) und Abstand
    struct VmCam {
        bool on = false;
        float az = 0, el = 0, dist = 0.5f;
    } vmCam_;
    // V13 (Tests): Kamera um die zuletzt gezeigte Vorfuehrung (figure): Winkel relativ zu ihrer Blickrichtung, Abstand, Hoehe
    struct FigCam {
        bool on = false;
        float az = 0, el = 0, dist = 2.0f, h = 0.6f, ahead = 0.0f;
    } figCam_;
    bool egoPose_ = false;  // V13.2 (Tests): Ego-Lampenhaltung auch von aussen zeigen
    // V13.3 (Kontaktboegen, Wegbilder): freie Kamera (Position, Blick in Grad, Bildwinkel), Testlampe (Spot wie die
    // Taschenlampe, frei gestellt), Leuchten der Welt skalieren (0 = dunkel)
    struct FreeCam {
        bool on = false;
        vec3 pos{};
        float yaw = 0, pitch = 0, fov = 0;  // Grad; fov 0 = wie im Spiel
    } freeCam_;
    struct TestLight {
        bool on = false;
        vec3 pos{}, dir{1, 0, 0};
        float power = 1.0f, cone = 28.0f;
    } testLight_;
    float worldLight_ = 1.0f;
    void propDump(const std::string& path);  // V13.3: Ausstattung, Schilder und Raeume der Ebene als JSON
    bool scriptFailed_ = false;

    // --- V13.2 Koop (Beta, MULTIPLAYER_ARCHITEKTUR.md) ----------------------------------------------
    coop::NetApi netApi_;
    std::unique_ptr<coop::CoopHost> coopHost_;
    std::unique_ptr<coop::CoopClient> coopClient_;
    bool coop() const { return coopHost_ || coopClient_; }
    bool loadNet(std::string& error);
    u32 coopBuild() const;
    int difficultyIndex(const std::string& key) const;
    void coopBefore(double dt);                                 // Netz lesen (vor dem Spielschritt)
    bool coopAfter(double dt, const game::PlayerControl& ctl);  // senden, Ebenenwechsel; false: Sitzung ersetzt
    void coopJoinPump(double dt);                               // Titel: Beitritt abwarten, dann die Ebene bauen
    void leaveCoop(const std::string& reason);
    double coopJoinT_ = 0.0;
    bool coopWasDead_ = false;
    std::string coopName_;
    bool coopSave_ = false;  // dieser Lauf ist ein Koop-Stand (Host)
    std::string coopKeyFile_;  // Skript (Tests): Beitrittscode des Hosts in diese Datei
    coop::NetSettings netSettings(bool useLobby, bool relayOnly);  // koop.json (Programmordner, dann Nutzerordner) + Menue
};

}  // namespace lim
