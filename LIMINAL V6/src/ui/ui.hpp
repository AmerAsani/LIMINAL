// Oberflaeche: Hauptmenue, Pausenmenue, Einstellungen, Hilfe, HUD, Inventar,
// Todesbildschirm, Karte und Debug-Anzeige. Sofortmodus: Eingabe und Zeichnen
// geschehen in einem Durchlauf je Bild. Bedienung mit Maus, Tastatur und Gamepad.
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "core/core.hpp"
#include "core/math.hpp"
#include "gameplay/inventory.hpp"
#include "gameplay/savegame.hpp"
#include "ui/host.hpp"

namespace lim::gfx {
class UiRenderer;
}

namespace lim::ui {

enum class Mode { Loading, Title, Hud, Pause, Inventory, Dead };

// Grossbuchstaben fuer UTF-8-Text inkl. deutscher Umlaute (ß bleibt)
std::string upperUtf8(const std::string& s);

class Ui {
public:
    void frame(Host& h, double dt);  // Eingabe verarbeiten und zeichnen

    Mode mode() const { return mode_; }
    const std::string& page() const { return page_; }
    void setMode(Mode m);
    bool pausesGame() const { return mode_ == Mode::Pause || mode_ == Mode::Inventory || mode_ == Mode::Dead; }
    bool wantsMouse() const { return mode_ == Mode::Hud; }  // Maus fuer das Umsehen fangen?

    void toast(const std::string& text, double duration = 2.2);
    void zoneTitle(const std::string& text);
    void showHelpHint(double seconds) { helpUntil_ = now_ + seconds; }
    void savedIndicator() { savedUntil_ = now_ + 2.5; }
    // V6: weiche Ueberblendung aus Schwarz (Spielstart, Laden, Hauptmenue)
    void fadeFromBlack(double seconds) {
        fadeStart_ = -1.0;  // Start beim naechsten Bild (now_ ist dann aktuell)
        fadeLen_ = seconds;
    }
    bool mapOpen() const { return showMap_; }
    void setLoading(const std::string& text, float progress) {
        loadingText_ = text;
        loadingProgress_ = progress;
    }
    void openPause() { setMode(Mode::Pause); }
    void toggleMap() { showMap_ = !showMap_; }
    void toggleDebug() { showDebug_ = !showDebug_; }
    bool debugVisible() const { return showDebug_; }
    void openHelp();
    // Test/Automatisierung: Unterseite direkt oeffnen
    void openPage(const std::string& page) {
        pageStack_.clear();
        if (page != "main") pageStack_.push_back("main");
        page_ = page;
        if (page == "load") saves_ = game::savegame::list();
    }

private:
    struct Item {
        std::string label;
        std::function<std::string()> value;   // leer = kein Wert
        std::function<void(int)> adjust;      // links/rechts
        std::function<void()> activate;       // Enter/Klick
        std::string desc;
        bool muted = false;
    };
    struct ListState {
        int sel = 0;
        float scroll = 0;
    };

    // Bausteine
    float S() const { return scale_; }
    bool hovered(float x, float y, float w, float hh) const;
    bool clicked(float x, float y, float w, float hh) const;
    void panel(float x, float y, float w, float hh);
    void runList(Host& h, std::vector<Item>& items, ListState& st, float x, float y, float w, float rowH, int visibleRows);
    float header(float x, float y, float w, const std::string& title, const std::string& sub);
    void footer(float x, float y, float w, const std::string& hint);

    // Seiten
    void drawLoading(Host& h);
    void drawTitle(Host& h);
    void titleMain(Host& h, float x, float y, float w);
    void titleNew(Host& h, float x, float y, float w);
    void titleLoad(Host& h, float x, float y, float w);
    void drawPause(Host& h);
    bool settingsPage(Host& h, const std::string& page, float x, float y, float w, float maxH);
    std::vector<Item> settingsItems(Host& h, const std::string& page);
    void helpPage(Host& h, float x, float y, float w, float hgt);
    void drawHud(Host& h, double dt);
    void drawHotbar(Host& h, game::GameSession& s);
    void drawDebug(Host& h);
    // V6: runde Minimap (nur erkundete Bereiche) und grosse Karte (M)
    void drawMinimap(Host& h, game::GameSession& s, double dt);
    void drawBigMap(Host& h, game::GameSession& s, double dt);
    void buildMapImage(game::GameSession& s, i64 cx, i64 cy, int halfTiles, int texelsPerTile);
    void drawMapOverlay(Host& h, game::GameSession& s, float cx, float cy, float r, float metersVisible, float angle,
                        bool big);
    void drawInventory(Host& h);
    void drawDead(Host& h);
    void slotBox(Host& h, const game::Inventory& inv, int idx, float x, float y, float size, bool selected, bool cursor);

    gfx::UiRenderer* r_ = nullptr;
    input::Input* in_ = nullptr;
    Mode mode_ = Mode::Loading;
    float scale_ = 1.0f;
    float W_ = 0, H_ = 0;
    double now_ = 0.0;

    std::string page_ = "main";   // Unterseite von Titel/Pause
    std::vector<std::string> pageStack_;
    ListState lists_[12];
    int listIndex(const std::string& page) const;
    std::string newName_;
    int newDiff_ = 1;
    int newLevel_ = 0;
    int newSel_ = 0;
    std::vector<game::SaveEntry> saves_;
    int loadSel_ = 0;
    bool confirmDelete_ = false;
    std::string titleMsg_;
    float helpScroll_ = 0;
    bool helpFromGame_ = false;   // Hilfe direkt aus dem Spiel (H): Schliessen fuehrt zurueck ins Spiel
    bool inFrame_ = false;        // gerade in frame()?
    bool swallowInput_ = false;   // Modus wurde von aussen gewechselt: Tastendruecke dieses Bildes verwerfen

    // HUD
    struct Toast {
        std::string text;
        double start = 0, until = 0;
    };
    std::vector<Toast> toasts_;   // V6: bis zu 3 Meldungen gleichzeitig, weich ein-/ausgeblendet
    double fadeStart_ = 0, fadeLen_ = 0;
    float promptAlpha_ = 0;
    double staminaShow_ = 0;      // V6: Ausdauer-Leiste sichtbar bis
    std::string lastPrompt_;
    std::string zone_;
    double zoneUntil_ = 0;
    double helpUntil_ = 0, savedUntil_ = 0;
    bool showMap_ = false, showDebug_ = false;
    // V6: Karten-Bilder (nur bei Aenderung neu erzeugt)
    struct MapCache {
        u32 rev = 0xFFFFFFFFu;
        i64 cx = 0, cy = 0;
        double timer = 0;
        int half = 0, tpp = 0;
    };
    MapCache mini_, big_;
    std::vector<u8> mapPixels_;
    float bigAnim_ = 0;  // Einblenden der grossen Karte
    std::string loadingText_ = "Lade …";
    float loadingProgress_ = 0;

    // Inventar
    int invRow_ = 3, invCol_ = 0;
    std::optional<game::Stack> hand_;
    int handFrom_ = -1;
    void closeInventory(Host& h);
};

}  // namespace lim::ui
