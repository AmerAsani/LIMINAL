// Hauptmenue, Pausenmenue, Einstellungen und Hilfe.
#include <algorithm>
#include <cmath>
#include <format>

#include "app/settings.hpp"
#include "gameplay/content.hpp"
#include "gameplay/session.hpp"
#include "input/input.hpp"
#include "render/font.hpp"
#include "render/renderer.hpp"
#include "ui/theme.hpp"
#include "ui/ui.hpp"

namespace lim::ui {

using namespace theme;
using input::Action;

namespace {

std::string onOff(bool b) { return b ? "An" : "Aus"; }
std::string pct(float v) { return std::format("{} %", (int)std::lround(v * 100)); }

template <class T>
T step(T v, T d, T lo, T hi) {
    return std::clamp(v + d, lo, hi);
}

template <class T>
int cycleIndex(const std::vector<T>& vals, T cur, int dir) {
    int i = 0;
    for (int k = 0; k < (int)vals.size(); ++k)
        if (vals[(size_t)k] == cur) i = k;
    return (i + dir + (int)vals.size()) % (int)vals.size();
}

struct HelpLine {
    char kind;  // h Ueberschrift, k Taste, t Text, leer
    const char* a;
    const char* b;
};

const HelpLine kHelp[] = {
    {'h', "Bewegung", ""},
    {'k', "W / S", "vorwärts / rückwärts gehen"},
    {'k', "A / D", "seitlich gehen"},
    {'k', "Shift", "sprinten (gedrückt halten, kostet Ausdauer)"},
    {'k', "Leertaste  (Gamepad B)", "springen – auch auf Theken, Kisten und Podeste"},
    {'k', "+ / −", "Laufgeschwindigkeit ändern"},
    {'t', "Treppen und Stufen steigst du einfach hinauf und hinunter, von hohen Kanten fällst du. "
          "Unter niedrigen Decken ist kein Platz zum Springen.", ""},
    {'t', "Sprinten zehrt an der Ausdauer (schmale Leiste zwischen Health und Sanity). Ist sie leer, "
          "bist du außer Atem: Du kannst erst wieder sprinten, wenn sie sich bis zur Marke erholt hat. "
          "Im Stehen erholst du dich schneller als im Gehen.", ""},
    {' ', "", ""},
    {'h', "Körper & Kamera", ""},
    {'k', "F5  (Gamepad R3)", "Ego-Perspektive / Schulterkamera"},
    {'t', "Du bist ein Strichmännchen. Schau nach unten, um deine Beine zu sehen – beim Gehen, Sprinten, "
          "Springen und Landen bewegt sich dein Körper mit. Beim Aufnehmen und Benutzen von Items "
          "greifen deine Hände zu. Unter »Grafik & Anzeige« lässt sich der Körper ausblenden.", ""},
    {' ', "", ""},
    {'h', "Umsehen", ""},
    {'k', "Maus", "umsehen (links/rechts, oben/unten)"},
    {'k', "Pfeiltasten", "Alternative zur Maus"},
    {'k', "Gamepad", "linker Stick gehen, rechter Stick umsehen, L3 sprinten"},
    {'t', "Die Maus wird nur geführt, solange das Spielfenster aktiv ist und kein Menü offen ist. "
          "Empfindlichkeit, Achsen, Invertieren und Glättung stellst du unter »Maus & Steuerung« ein.", ""},
    {' ', "", ""},
    {'h', "Items & Inventar", ""},
    {'k', "E  (Gamepad A)", "Item aufnehmen (wenn [E] eingeblendet wird)"},
    {'k', "F  (Gamepad X)", "gewähltes Item der Schnellleiste benutzen"},
    {'k', "Q  (Gamepad ↓)", "gewähltes Item ablegen (Strg+Q: ganzer Stapel) – auch im Inventar"},
    {'k', "1 – 9 / Mausrad", "Platz der Schnellleiste wählen (Gamepad LB/RB)"},
    {'k', "Tab  (Gamepad Y)", "Inventar öffnen / schließen"},
    {'k', "Im Inventar", "Pfeile/WASD oder Maus, Enter/Klick nehmen und ablegen, F benutzen,"},
    {'k', "", "1 – 9 legt das Item auf diesen Platz der Schnellleiste"},
    {'t', "Items sind selten. Du findest sie auf den Theken kleiner Versorgungsräume – "
          "achte auf den rot leuchtenden Automaten. Energy Bars sind ein seltener Fund: "
          "sie schimmern leicht auf der Theke und wirken stärker als früher. Abgelegte Items "
          "bleiben liegen (auch im Spielstand) und lassen sich wieder aufheben.", ""},
    {'k', "Energy Bar  (selten)", "+30 % Sanity, +30 % Tempo (75 s), Ausdauer voll, Sprint halb so teuer"},
    {'k', "Mandelwasser", "+35 % Sanity"},
    {' ', "", ""},
    {'h', "Health & Sanity", ""},
    {'t', "Deine Sanity sinkt stetig (je nach Schwierigkeit 10–22 % pro Minute). Erreicht sie 0 %, "
          "färbt sich der Bildschirmrand zunehmend rot und deine Health schwindet – nach wenigen "
          "Sekunden verlierst du endgültig den Verstand. Ein Item rettet dich, danach erholt sich "
          "deine Health langsam.", ""},
    {'t', "Das Spiel speichert alle 3 Minuten automatisch (sowie beim Beenden). Nach dem Tod "
          "kannst du den letzten Spielstand laden oder zum Hauptmenü zurückkehren.", ""},
    {' ', "", ""},
    {'h', "Menü", ""},
    {'k', "ESC  (Gamepad Start)", "Menü öffnen / schließen (Spiel pausiert)"},
    {'k', "↑ ↓  oder  W S", "Eintrag wählen"},
    {'k', "← →  oder  A D", "Wert ändern"},
    {'k', "Enter / Leertaste", "bestätigen, umschalten"},
    {'k', "ESC / Rücktaste", "eine Ebene zurück"},
    {' ', "", ""},
    {'h', "Karte", ""},
    {'t', "Die runde Minimap oben rechts zeigt nur, was du selbst erkundet hast: Alles, was du beim "
          "Gehen gesehen hast, bleibt eingezeichnet – auch im Spielstand. Wände erscheinen als helle "
          "Kontur, Durchgänge orange, Automaten pink, Items gelb. Der Pfeil zeigt deine Blickrichtung.", ""},
    {'k', "M  (Gamepad Back)", "große Karte öffnen / schließen"},
    {' ', "", ""},
    {'h', "Weitere Funktionen", ""},
    {'k', "F3 oder I", "Debug-Anzeige (FPS, Position, Raum, Seed …)"},
    {'k', "V", "Darstellung: Modern / Terminal / ASCII / Monochrom"},
    {'k', "L", "Leistungsmodus (halbe interne Auflösung)"},
    {'k', "B", "Head-Bobbing an / aus"},
    {'k', "P oder F12", "Screenshot (PNG in »Bilder\\LIMINAL«)"},
    {'k', "Alt + Enter", "Vollbild / Fenster"},
    {'k', "H oder F1", "diese Hilfe öffnen"},
    {' ', "", ""},
    {'h', "Die Welt", ""},
    {'t', "LIMINAL hat kein Ziel und keine Gegner. Du erkundest eine unendliche, prozedural erzeugte "
          "Welt aus Innenräumen: helle Büroflure, Beton und Industrie, unterirdische Tunnel, riesige "
          "leere Hallen und enge Wartungsbereiche. Türen sind offene Durchgänge – du gehst einfach hindurch.", ""},
    {'t', "Die Zonen gehen fließend ineinander über. Je weiter du dich vom Start entfernst, desto "
          "häufiger begegnen dir seltene, seltsame Räume.", ""},
    {'t', "Jede Welt entsteht aus einem Seed. Mit demselben Seed (siehe Debug-Anzeige) betrittst du "
          "exakt dieselbe Welt erneut – auch dieselbe wie in V4:  LIMINAL.exe --seed 1234", ""},
};

}  // namespace

// --- Einstellungsseiten --------------------------------------------------------------------------
std::vector<Ui::Item> Ui::settingsItems(Host& h, const std::string& page) {
    Settings& s = h.settings();
    std::vector<Item> v;
    auto changed = [&h] { h.settingsChanged(); };
    auto custom = [&h] {
        h.settings().quality = 4;
        h.settings().autoQuality = false;  // V6: eigene Wahl beendet die Automatik
        h.settingsChanged();
    };
    auto back = [this] {
        page_ = pageStack_.empty() ? "main" : pageStack_.back();
        if (!pageStack_.empty()) pageStack_.pop_back();
    };
    if (page == "mouse") {
        v.push_back({"Maussteuerung", [&s] { return onOff(s.mouseEnabled); },
                     [&s, changed](int) { s.mouseEnabled = !s.mouseEnabled, changed(); }, nullptr,
                     "Umsehen mit der Maus ein- oder ausschalten (Pfeiltasten und Gamepad funktionieren immer)."});
        v.push_back({"Empfindlichkeit", [&s] { return std::format("{:.1f}", s.mouseSensitivity); },
                     [&s, changed](int d) { s.mouseSensitivity = step(s.mouseSensitivity, d * 0.1f, 0.1f, 5.0f), changed(); },
                     nullptr, "Grundempfindlichkeit für beide Achsen."});
        v.push_back({"X-Achse  (links / rechts)", [&s] { return std::format("{:.1f}x", s.mouseX); },
                     [&s, changed](int d) { s.mouseX = step(s.mouseX, d * 0.1f, 0.2f, 3.0f), changed(); }, nullptr,
                     "Zusätzlicher Faktor für das Drehen nach links und rechts."});
        v.push_back({"Y-Achse  (oben / unten)", [&s] { return std::format("{:.1f}x", s.mouseY); },
                     [&s, changed](int d) { s.mouseY = step(s.mouseY, d * 0.1f, 0.2f, 3.0f), changed(); }, nullptr,
                     "Zusätzlicher Faktor für das Schauen nach oben und unten."});
        v.push_back({"X-Achse invertieren", [&s] { return onOff(s.invertX); },
                     [&s, changed](int) { s.invertX = !s.invertX, changed(); }, nullptr,
                     "Maus nach rechts dreht nach links (und umgekehrt)."});
        v.push_back({"Y-Achse invertieren", [&s] { return onOff(s.invertY); },
                     [&s, changed](int) { s.invertY = !s.invertY, changed(); }, nullptr,
                     "Maus nach oben schaut nach unten – wie im Flugsimulator."});
        v.push_back({"Glättung", [&s] { return pct(s.mouseSmoothing); },
                     [&s, changed](int d) { s.mouseSmoothing = step(s.mouseSmoothing, d * 0.05f, 0.0f, 0.9f), changed(); },
                     nullptr, "0 % = direkte Reaktion, höher = weichere, fließendere Kamerabewegung."});
        v.push_back({"Gamepad-Empfindlichkeit", [&s] { return std::format("{:.1f}", s.padSensitivity); },
                     [&s, changed](int d) { s.padSensitivity = step(s.padSensitivity, d * 0.1f, 0.2f, 3.0f), changed(); },
                     nullptr, "Drehgeschwindigkeit mit dem rechten Stick."});
        v.push_back({"Standardwerte", nullptr, nullptr, [&s, changed] { s.resetMouse(), changed(); },
                     "Alle Maus- und Gamepad-Einstellungen auf die Standardwerte zurücksetzen."});
    } else if (page == "graphics") {
        static const char* q[] = {"Niedrig", "Mittel", "Hoch", "Ultra", "Benutzerdefiniert"};
        static const char* modes[] = {"Modern", "Terminal", "ASCII", "Monochrom"};
        // V6: "Automatisch" misst die Hardware und waehlt die passende Stufe (siehe app/autotune.cpp)
        v.push_back({"Grafikqualität",
                     [&s] {
                         if (s.autoQuality) return std::string("Automatisch (") + q[std::clamp(s.quality, 0, 4)] + ")";
                         return std::string(q[std::clamp(s.quality, 0, 4)]);
                     },
                     [&s, changed](int d) {
                         int cur = s.autoQuality ? 0 : std::clamp(s.quality, 0, 3) + 1;
                         int nxt = (cur + d + 5) % 5;
                         if (nxt == 0) {
                             s.autoQuality = true;
                             s.calibratedFor.clear();  // neu messen
                         } else {
                             s.autoQuality = false;
                             s.applyQualityPreset(nxt - 1);
                         }
                         changed();
                     },
                     nullptr,
                     "Automatisch: misst die Leistung deines PCs und wählt die höchste Stufe, die flüssig läuft. "
                     "Niedrig: schwache Grafikchips. Mittel: normale PCs. Hoch: aktuelle Grafikkarten. "
                     "Ultra: leistungsstarke PCs (darf teuer sein). Einzelne Änderungen machen daraus »Benutzerdefiniert«."});
        v.push_back({"Dynamische Auflösung", [&s] { return onOff(s.dynamicResolution); },
                     [&s, changed](int) { s.dynamicResolution = !s.dynamicResolution, changed(); }, nullptr,
                     "Senkt bei Last kurz die interne Auflösung, um die Bildrate zu halten (Ziel: »Bildrate (max.)«, sonst 60 FPS)."});
        v.push_back({"Darstellung", [&s] { return std::string(modes[s.render.displayMode]); },
                     [&s, changed](int d) { s.render.displayMode = (s.render.displayMode + d + 4) % 4, changed(); },
                     nullptr, "Modern: volle Grafik. Terminal/ASCII/Monochrom: die Zeichen-Darstellungen aus V4 als Retro-Filter (Taste V)."});
        v.push_back({"Interne Auflösung", [&s] { return pct(s.render.renderScale); },
                     [&s, custom](int d) { s.render.renderScale = step(s.render.renderScale, d * 0.05f, 0.5f, 1.5f), custom(); },
                     nullptr, "Unter 100 % schneller, über 100 % schärfer (Supersampling)."});
        v.push_back({"Schatten", [&s] { return std::string(s.render.shadows == 0 ? "Aus" : (s.render.shadows == 1 ? "Normal" : "Hoch")); },
                     [&s, custom](int d) { s.render.shadows = (s.render.shadows + d + 3) % 3, custom(); }, nullptr,
                     "Weiche Schatten aller Leuchten. Hoch: längere Schattenstrahlen (große Hallen)."});
        v.push_back({"Umgebungsverdeckung",
                     [&s] { return std::string(s.render.ssao == 0 ? "Aus" : (s.render.ssao == 1 ? "GTAO" : "GTAO hoch")); },
                     [&s, custom](int d) { s.render.ssao = (s.render.ssao + d + 3) % 3, custom(); }, nullptr,
                     "Ground-Truth Ambient Occlusion: physikalisch fundierte Verdeckung in Ecken, an Kanten und unter "
                     "Gegenständen, mit Mehrfachreflexion. Hoch: mehr Abtastschritte und genauere Kontaktschatten."});
        v.push_back({"Indirektes Licht", [&s] { return onOff(s.render.indirect); },
                     [&s, custom](int) { s.render.indirect = !s.render.indirect, custom(); }, nullptr,
                     "Ein Licht-Bounce im Bildraum: beleuchtete Flächen hellen ihre Umgebung farbig auf "
                     "(braucht Umgebungsverdeckung und Kantenglättung)."});
        v.push_back({"Kontaktschatten", [&s] { return onOff(s.render.contactShadows); },
                     [&s, custom](int) { s.render.contactShadows = !s.render.contactShadows, custom(); }, nullptr,
                     "Feine Schatten von Items, dem eigenen Körper und kleinen Kanten."});
        v.push_back({"Oberflächendetails", [&s] { return onOff(s.render.parallax); },
                     [&s, custom](int) { s.render.parallax = !s.render.parallax, custom(); }, nullptr,
                     "Parallax-Occlusion-Mapping (echte Tiefe in Fugen, Gittern, Ziegeln) und Gebrauchsspuren "
                     "wie Wasserflecken, Schmutz und Abrieb."});
        v.push_back({"Kantenglättung (TAA)", [&s] { return onOff(s.render.taa); },
                     [&s, custom](int) { s.render.taa = !s.render.taa, custom(); }, nullptr,
                     "Temporales Anti-Aliasing: glatte Kanten, ruhige Muster, weiche Halbschatten."});
        v.push_back({"Bloom", [&s] { return onOff(s.render.bloom); },
                     [&s, custom](int) { s.render.bloom = !s.render.bloom, custom(); }, nullptr,
                     "Weiches Überstrahlen heller Leuchten."});
        v.push_back({"Spiegelungen (SSR)", [&s] { return onOff(s.render.ssr); },
                     [&s, custom](int) { s.render.ssr = !s.render.ssr, custom(); }, nullptr,
                     "Spiegelungen auf glatten Böden (Fliesen, Beton, Metall) aus dem sichtbaren Bild."});
        v.push_back({"Volumetrisches Licht",
                     [&s] {
                         static const char* q[] = {"Niedrig", "Mittel", "Hoch"};
                         return s.render.volumetric <= 0.0f ? std::string("Aus") : std::string(q[std::clamp(s.render.volumetricQuality, 1, 3) - 1]);
                     },
                     [&s, custom](int d) {
                         int lv = s.render.volumetric <= 0.0f ? 0 : std::clamp(s.render.volumetricQuality, 1, 3);
                         lv = (lv + d + 4) % 4;
                         s.render.volumetric = lv == 0 ? 0.0f : 1.0f;
                         if (lv > 0) s.render.volumetricQuality = lv;
                         custom();
                     },
                     nullptr, "Lichtkegel und Lichtschächte im Dunst der Räume, mit Schatten von Wänden, Säulen und Türen."});
        v.push_back({"Beleuchtung",
                     [&s] {
                         static const char* l[] = {"Niedrig", "Mittel", "Hoch", "Ultra"};
                         return std::string(l[std::clamp(s.render.lightQuality, 0, 3)]);
                     },
                     [&s, custom](int d) { s.render.lightQuality = (s.render.lightQuality + d + 4) % 4, custom(); }, nullptr,
                     "Wie viele Leuchten gleichzeitig berechnet werden und wie weit ihr Licht reicht (160 / 384 / 1024 / 2048). "
                     "Niedriger hilft vor allem in großen, offenen Hallen."});
        v.push_back({"Partikel",
                     [&s] {
                         static const char* l[] = {"Aus", "Wenig", "Normal", "Viel"};
                         return std::string(l[s.render.dust ? std::clamp(s.render.particles, 0, 3) : 0]);
                     },
                     [&s, custom](int d) {
                         int p = s.render.dust ? std::clamp(s.render.particles, 0, 3) : 0;
                         p = (p + d + 4) % 4;
                         s.render.particles = p;
                         s.render.dust = p > 0;
                         custom();
                     },
                     nullptr, "Feine Schwebeteilchen in der Luft, die im Licht der Lampen aufleuchten."});
        v.push_back({"Nachbearbeitung",
                     [&s] {
                         static const char* l[] = {"Aus", "Dezent", "Voll"};
                         int p = s.detectPostQuality();
                         return p < 0 ? std::string("Eigene") : std::string(l[p]);
                     },
                     [&s, changed](int d) {
                         int p = s.detectPostQuality();
                         p = ((p < 0 ? 2 : p) + d + 3) % 3;
                         s.applyPostQuality(p);
                         changed();
                     },
                     nullptr, "Bloom, Filmkorn, Vignette, Farbsäume und Nachschärfen gemeinsam. Einzeln weiter unten einstellbar."});
        v.push_back({"Bildschärfe", [&s] { return s.render.sharpen <= 0.001f ? std::string("Aus") : pct(s.render.sharpen); },
                     [&s, custom](int d) { s.render.sharpen = step(s.render.sharpen, d * 0.05f, 0.0f, 1.0f), custom(); }, nullptr,
                     "Kontrastadaptives Nachschärfen: gleicht die Weichheit der Kantenglättung und geringerer interner Auflösung aus."});
        v.push_back({"Texturqualität",
                     [&s] { return std::string(s.render.textureSize <= 512 ? "Mittel (512)" : (s.render.textureSize <= 1024 ? "Hoch (1024)" : "Ultra (2048)")); },
                     [&s, custom](int d) {
                         std::vector<int> vals{512, 1024, 2048};
                         s.render.textureSize = vals[(size_t)cycleIndex(vals, s.render.textureSize, d)];
                         custom();
                     },
                     nullptr, "Auflösung der Oberflächen. Grafikspeicher mit Kompression: Mittel ~50 MB, Hoch ~190 MB, Ultra ~770 MB (ohne: 90 / 360 / 1400 MB)."});
        v.push_back({"Texturkompression", [&s] { return onOff(s.render.textureCompression); },
                     [&s, changed](int) { s.render.textureCompression = !s.render.textureCompression, changed(); }, nullptr,
                     "Blockkompression (BC1/BC5) der Oberflächen: etwa 45 % weniger Grafikspeicher bei kaum sichtbarem Unterschied."});
        v.push_back({"Anisotrope Filterung", [&s] { return std::format("{}x", s.render.anisotropy); },
                     [&s, custom](int d) {
                         std::vector<int> vals{1, 2, 4, 8, 16};
                         s.render.anisotropy = vals[(size_t)cycleIndex(vals, s.render.anisotropy, d)];
                         custom();
                     },
                     nullptr, "Schärfe schräg gesehener Böden und Wände."});
        v.push_back({"Sichtweite", [&s] { return std::format("{} m", s.viewDistance * 16); },
                     [&s, custom](int d) { s.viewDistance = std::clamp(s.viewDistance + d, 2, 6), custom(); }, nullptr,
                     "Wie weit die Welt um dich herum geladen und gezeichnet wird."});
        v.push_back({"Helligkeit", [&s] { return std::format("{:+.1f}", s.render.exposureBias); },
                     [&s, changed](int d) { s.render.exposureBias = step(s.render.exposureBias, d * 0.1f, -2.0f, 2.0f), changed(); },
                     nullptr, "Belichtungskorrektur in Blendenstufen."});
        v.push_back({"Filmkorn", [&s] { return pct(s.render.filmGrain); },
                     [&s, changed](int d) { s.render.filmGrain = step(s.render.filmGrain, d * 0.05f, 0.0f, 1.0f), changed(); }, nullptr, ""});
        v.push_back({"Vignette", [&s] { return pct(s.render.vignette); },
                     [&s, changed](int d) { s.render.vignette = step(s.render.vignette, d * 0.05f, 0.0f, 1.0f), changed(); }, nullptr, ""});
        v.push_back({"Chromatische Aberration", [&s] { return pct(s.render.chromatic); },
                     [&s, changed](int d) { s.render.chromatic = step(s.render.chromatic, d * 0.05f, 0.0f, 1.0f), changed(); }, nullptr, ""});
        v.push_back({"Sichtfeld", [&s] { return s.fov <= 0 ? std::string("Auto") : std::format("{}°", (int)s.fov); },
                     [&s, changed](int d) {
                         float f = s.fov <= 0 ? 85.0f : s.fov;
                         f += d * 5.0f;
                         s.fov = (f < 60.0f || f > 115.0f) ? 0.0f : f;
                         changed();
                     },
                     nullptr, "Horizontales Sichtfeld. Auto passt es an das Seitenverhältnis an."});
        v.push_back({"Bildrate (max.)", [&s] { return s.maxFps ? std::format("{} FPS", s.maxFps) : std::string("Unbegrenzt"); },
                     [&s, changed](int d) {
                         std::vector<int> vals{0, 30, 60, 90, 120, 144, 165, 240};
                         s.maxFps = vals[(size_t)cycleIndex(vals, s.maxFps, d)];
                         changed();
                     },
                     nullptr, "Obergrenze der Bildrate. Weniger entlastet Grafikkarte und Prozessor."});
        v.push_back({"VSync", [&s] { return onOff(s.render.vsync); },
                     [&s, changed](int) { s.render.vsync = !s.render.vsync, changed(); }, nullptr,
                     "Synchronisiert mit dem Bildschirm (kein Tearing)."});
        v.push_back({"Fenstergröße",
                     [&s] { return s.fullscreen ? std::string("Vollbild (Bildschirm)") : std::format("{} × {}", s.windowWidth, s.windowHeight); },
                     [&s, changed](int d) {
                         static const int sizes[][2] = {{1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160}};
                         int cur = 0;
                         for (int k = 0; k < 5; ++k)
                             if (sizes[k][0] == s.windowWidth && sizes[k][1] == s.windowHeight) cur = k;
                         cur = (cur + d + 5) % 5;
                         s.windowWidth = sizes[cur][0], s.windowHeight = sizes[cur][1];
                         changed();
                     },
                     nullptr, "Größe des Fensters (ohne Vollbild). Die Bildschärfe steuert zusätzlich »Interne Auflösung«."});
        v.push_back({"Vollbild", [&s] { return onOff(s.fullscreen); },
                     [&s, changed](int) { s.fullscreen = !s.fullscreen, changed(); }, nullptr,
                     "Randloses Vollbild. Alt+Enter schaltet jederzeit um."});
        v.push_back({"Head-Bobbing", [&s] { return onOff(s.bob); }, [&s, changed](int) { s.bob = !s.bob, changed(); },
                     nullptr, "Leichtes Mitschwingen der Kamera beim Gehen, Sprinten und Seitwärtsgehen (Taste B)."});
        v.push_back({"Perspektive", [&s] { return std::string(s.thirdPerson ? "Schulterkamera" : "Ego"); },
                     [&s, changed](int) { s.thirdPerson = !s.thirdPerson, changed(); }, nullptr,
                     "Ego-Perspektive aus dem Kopf des Strichmännchens oder Kamera hinter der Schulter (Taste F5)."});
        v.push_back({"Eigener Körper", [&s] { return onOff(s.showBody); },
                     [&s, changed](int) { s.showBody = !s.showBody, changed(); }, nullptr,
                     "Zeigt in der Ego-Perspektive deinen Strichmännchen-Körper (schau nach unten)."});
        v.push_back({"Minimap", [&s] { return onOff(s.minimap); }, [&s, changed](int) { s.minimap = !s.minimap, changed(); },
                     nullptr, "Runde Karte oben rechts. Sie zeigt nur Bereiche, die du erkundet hast."});
        v.push_back({"Minimap-Ausrichtung", [&s] { return std::string(s.minimapRotate ? "Dreht mit" : "Norden oben"); },
                     [&s, changed](int) { s.minimapRotate = !s.minimapRotate, changed(); }, nullptr,
                     "Dreht mit: deine Blickrichtung zeigt immer nach oben. Norden oben: feste Karte, der Pfeil dreht sich."});
        v.push_back({"Laufgeschwindigkeit", [&s] { return std::format("{:.1f} m/s", s.walkSpeed); },
                     [&s, changed](int d) { s.walkSpeed = step(s.walkSpeed, d * 0.5f, 1.0f, 8.0f), changed(); }, nullptr,
                     "Normales Gehtempo. Mit Shift sprintest du etwa doppelt so schnell."});
    } else if (page == "audio") {
        v.push_back({"Gesamtlautstärke", [&s] { return pct(s.masterVolume); },
                     [&s, changed](int d) { s.masterVolume = step(s.masterVolume, d * 0.05f, 0.0f, 1.0f), changed(); }, nullptr, ""});
        v.push_back({"Effekte", [&s] { return pct(s.effectsVolume); },
                     [&s, changed](int d) { s.effectsVolume = step(s.effectsVolume, d * 0.05f, 0.0f, 1.0f), changed(); }, nullptr,
                     "Schritte, Items, Automaten."});
        v.push_back({"Atmosphäre", [&s] { return pct(s.ambienceVolume); },
                     [&s, changed](int d) { s.ambienceVolume = step(s.ambienceVolume, d * 0.05f, 0.0f, 1.0f), changed(); },
                     nullptr, "Brummen der Leuchtstoffröhren und Raumklang der Zonen."});
    }
    Item b{"Zurück", nullptr, nullptr, back, ""};
    b.muted = true;
    v.push_back(b);
    return v;
}

// Liefert false, wenn die Seite verlassen wurde.
bool Ui::settingsPage(Host& h, const std::string& page, float x, float y, float w, float maxH) {
    float s = S();
    auto items = settingsItems(h, page);
    ListState& st = lists_[listIndex(page)];
    float rowH = 46 * s;
    int visible = std::max(3, (int)((maxH - 150 * s) / rowH));
    runList(h, items, st, x + 24 * s, y, w - 48 * s, rowH, visible);
    float dy = y + rowH * std::min(visible, (int)items.size()) + 14 * s;
    const Item& cur = items[(size_t)std::clamp(st.sel, 0, (int)items.size() - 1)];
    for (const auto& line : r_->wrap(cur.desc, 19 * s, w - 90 * s)) {
        r_->text(x + 40 * s, dy, 19 * s, line, kMuted);
        dy += 26 * s;
    }
    if (in_->actionPressed(Action::UiBack)) {
        h.playSound("ui_back");
        page_ = pageStack_.empty() ? "main" : pageStack_.back();
        if (!pageStack_.empty()) pageStack_.pop_back();
        return false;
    }
    return true;
}

void Ui::helpPage(Host& h, float x, float y, float w, float hgt) {
    float s = S();
    // Zeilen aufbauen
    struct Line {
        std::string a, b;
        char kind;
    };
    std::vector<Line> lines;
    for (const auto& e : kHelp) {
        if (e.kind == 't') {
            for (auto& l : r_->wrap(e.a, 19 * s, w - 110 * s)) lines.push_back({l, "", 't'});
        } else {
            lines.push_back({e.a, e.b, e.kind});
        }
    }
    float lh = 28 * s;
    int visible = std::max(4, (int)(hgt / lh));
    int maxScroll = std::max(0, (int)lines.size() - visible);
    if (in_->actionPressed(Action::UiUp)) helpScroll_ -= 1;
    if (in_->actionPressed(Action::UiDown)) helpScroll_ += 1;
    helpScroll_ -= (float)in_->wheel() * 3;
    helpScroll_ = std::clamp(helpScroll_, 0.0f, (float)maxScroll);
    int first = (int)helpScroll_;
    for (int i = first; i < std::min((int)lines.size(), first + visible); ++i) {
        const Line& l = lines[(size_t)i];
        float ly = y + (float)(i - first) * lh;
        if (l.kind == 'h') r_->text(x + 40 * s, ly, 20 * s, l.a, kTitle, gfx::FontFace::Bold);
        else if (l.kind == 'k') {
            r_->text(x + 56 * s, ly, 19 * s, l.a, kValue);
            r_->text(x + 330 * s, ly, 19 * s, l.b, kText);
        } else if (l.kind == 't') r_->text(x + 56 * s, ly, 19 * s, l.a, kMuted);
    }
    if (in_->actionPressed(Action::UiBack) || in_->actionPressed(Action::UiConfirm) || in_->actionPressed(Action::Help)) {
        h.playSound("ui_back");
        page_ = pageStack_.empty() ? "main" : pageStack_.back();
        if (!pageStack_.empty()) pageStack_.pop_back();
        // direkt aus dem Spiel geoeffnet (H/F1): zurueck ins Spiel statt ins Pausenmenue
        if (helpFromGame_ && mode_ == Mode::Pause && page_ == "main") setMode(Mode::Hud);
        helpFromGame_ = false;
    }
}

// --- Hauptmenue ------------------------------------------------------------------------------------
void Ui::drawTitle(Host& h) {
    float s = S();
    float w = std::min(W_ - 60 * s, 900 * s);
    float hh = std::min(H_ - 60 * s, 860 * s);
    float x = (W_ - w) * 0.5f, y = (H_ - hh) * 0.5f;
    if (page_ == "main" || page_ == "new") {
        hh = std::min(hh, 620 * s);
        y = (H_ - hh) * 0.5f;
    }
    panel(x, y, w, hh);
    static const std::pair<const char*, const char*> subs[] = {
        {"main", "unendliche Innenräume"}, {"new", "Neues Spiel"}, {"load", "Spiel fortsetzen"}, {"mouse", "Maus & Steuerung"},
        {"graphics", "Grafik & Anzeige"}, {"audio", "Audio"}, {"help", "Hilfe"}, {"settings", "Einstellungen"}};
    std::string sub;
    for (auto& [p, t] : subs)
        if (page_ == p) sub = t;
    float cy = header(x, y, w, "L I M I N A L", sub);
    float footY = y + hh - 58 * s;
    if (page_ == "main") titleMain(h, x, cy, w);
    else if (page_ == "new") titleNew(h, x, cy, w);
    else if (page_ == "load") titleLoad(h, x, cy, w);
    else if (page_ == "help") helpPage(h, x, cy, w, footY - cy - 10 * s);
    else if (page_ == "settings") {
        std::vector<Item> items = {
            {"Maus & Steuerung", nullptr, nullptr, [this] { pageStack_.push_back(page_), page_ = "mouse"; },
             "Empfindlichkeit, Achsen, Invertieren, Glättung und Gamepad."},
            {"Grafik & Anzeige", nullptr, nullptr, [this] { pageStack_.push_back(page_), page_ = "graphics"; },
             "Qualität, Darstellung, Schatten, Sichtweite, Sichtfeld, Bildrate, Vollbild."},
            {"Audio", nullptr, nullptr, [this] { pageStack_.push_back(page_), page_ = "audio"; }, "Lautstärken."},
            {"Hilfe", nullptr, nullptr, [this] { pageStack_.push_back(page_), page_ = "help", helpScroll_ = 0; },
             "Alle Steuerungen und Funktionen im Überblick."},
            {"Zurück", nullptr, nullptr, [this] { page_ = "main"; }, ""},
        };
        items.back().muted = true;
        runList(h, items, lists_[11], x + 24 * s, cy, w - 48 * s, 50 * s, 6);
        if (in_->actionPressed(Action::UiBack)) {
            h.playSound("ui_back");
            page_ = "main";
        }
    } else {
        settingsPage(h, page_, x, cy, w, footY - cy);
    }
    std::string hint = page_ == "new"  ? "Namen eintippen    ↑↓ Auswahl    ←→ Level / Schwierigkeit    Enter weiter    ESC zurück"
                       : page_ == "load" ? (confirmDelete_ ? "Entf nochmal drücken = Spielstand löschen"
                                                           : "↑↓ wählen    Enter laden    Entf löschen    ESC zurück")
                       : page_ == "help" ? "↑↓ / Mausrad blättern    ESC zurück"
                       : page_ == "main" ? (titleMsg_.empty() ? "↑↓ wählen    Enter bestätigen    ESC zweimal = beenden" : titleMsg_)
                                         : "↑↓ Auswahl    ←→ Wert    Enter OK    ESC zurück";
    footer(x, footY, w, hint);
    // V6: dezente Versionsangabe
    r_->text(W_ - 24 * s, H_ - 40 * s, 16 * s, "LIMINAL V6", rgba8(150, 140, 112, 150), gfx::FontFace::Bold, gfx::Align::Right);
}

void Ui::titleMain(Host& h, float x, float y, float w) {
    float s = S();
    std::vector<Item> items = {
        {"Neues Spiel", nullptr, nullptr, [this] {
             page_ = "new";
             newSel_ = 0;
             newName_.clear();
         }, ""},
        {"Spiel fortsetzen", nullptr, nullptr, [this] {
             saves_ = game::savegame::list();
             if (saves_.empty()) titleMsg_ = "Noch keine Spielstände vorhanden.";
             else {
                 page_ = "load";
                 loadSel_ = 0;
                 confirmDelete_ = false;
             }
         }, ""},
        {"Einstellungen", nullptr, nullptr, [this] { page_ = "settings"; }, ""},
        {"Beenden", nullptr, nullptr, [&h] { h.quit(); }, ""},
    };
    runList(h, items, lists_[0], x + 24 * s, y + 10 * s, w - 48 * s, 58 * s, 4);
    // ESC waehlt zuerst "Beenden" aus; erst ein zweites ESC beendet (kein versehentliches Schliessen)
    if (in_->actionPressed(Action::UiBack)) {
        if (lists_[0].sel == 3) h.quit();
        else lists_[0].sel = 3, h.playSound("ui_back");
    }
}

void Ui::titleNew(Host& h, float x, float y, float w) {
    float s = S();
    const auto& diffs = h.content().difficulties();
    const auto& levels = h.content().levels();
    newDiff_ = std::clamp(newDiff_, 0, (int)diffs.size() - 1);
    newLevel_ = std::clamp(newLevel_, 0, (int)levels.size() - 1);
    // Zeilen: Name, (Level), Schwierigkeit, Spiel starten, Zurueck
    enum Row { RName, RLevel, RDiff, RStart, RBack };
    std::vector<Row> rows = {RName};
    if (levels.size() > 1) rows.push_back(RLevel);
    rows.insert(rows.end(), {RDiff, RStart, RBack});
    const int n = (int)rows.size();
    newSel_ = std::clamp(newSel_, 0, n - 1);
    Row cur = rows[(size_t)newSel_];
    // Texteingabe fuer den Namen (nur in der ersten Zeile)
    bool typing = cur == RName;
    if (typing) {
        for (char32_t c : in_->text())
            // nur darstellbare Zeichen (sonst "?"), hoechstens 24 Zeichen
            if (gfx::utf8To32(newName_).size() < 24 && c != U'\t' && r_->hasGlyph(c)) {
                std::u32string one(1, c);
                newName_ += gfx::utf32To8(one);
            }
        if (in_->pressed(VK_BACK) && !newName_.empty()) {
            auto u = gfx::utf8To32(newName_);
            u.pop_back();
            newName_ = gfx::utf32To8(u);
        }
    }
    auto up = typing ? in_->pressed(VK_UP) || in_->padPressed(input::Pad::DUp) : in_->actionPressed(Action::UiUp);
    auto down = typing ? (in_->pressed(VK_DOWN) || in_->pressed(VK_TAB) || in_->padPressed(input::Pad::DDown))
                       : in_->actionPressed(Action::UiDown);
    if (up) newSel_ = (newSel_ + n - 1) % n, h.playSound("ui_move");
    if (down) newSel_ = (newSel_ + 1) % n, h.playSound("ui_move");
    cur = rows[(size_t)newSel_];
    int dir = in_->actionPressed(Action::UiLeft) ? -1 : (in_->actionPressed(Action::UiRight) ? 1 : 0);
    if (dir && !typing) {
        if (cur == RDiff) newDiff_ = (newDiff_ + dir + (int)diffs.size()) % (int)diffs.size(), h.playSound("ui_move");
        if (cur == RLevel) newLevel_ = (newLevel_ + dir + (int)levels.size()) % (int)levels.size(), h.playSound("ui_move");
    }
    bool confirm = typing ? (in_->pressed(VK_RETURN) || in_->padPressed(input::Pad::A)) : in_->actionPressed(Action::UiConfirm);
    float rowH = 58 * s;
    std::string cursor = (typing && std::fmod(now_, 1.0) < 0.5) ? "▌" : " ";
    const auto& lv = *levels[(size_t)newLevel_].def;
    for (int i = 0; i < n; ++i) {
        float ry = y + 10 * s + i * rowH;
        if (in_->cursorMoved && hovered(x + 24 * s, ry, w - 48 * s, rowH)) newSel_ = i;
        if (clicked(x + 24 * s, ry, w - 48 * s, rowH)) {
            newSel_ = i;
            confirm = true;
        }
        bool sel = i == newSel_;
        Row row = rows[(size_t)i];
        if (sel) r_->rect(x + 24 * s, ry + 2 * s, w - 48 * s, rowH - 4 * s, kSelBg, 6 * s);
        static const char* labels[] = {"Name", "Level", "Schwierigkeit", "Spiel starten", "Zurück"};
        vec4 tc = sel ? kSelText : (row == RBack ? kMuted : kText);
        r_->text(x + 46 * s, ry + 14 * s, 24 * s, std::string(sel ? "▸  " : "    ") + labels[row], tc,
                 sel ? gfx::FontFace::Bold : gfx::FontFace::Regular);
        std::string val;
        bool placeholder = false;
        if (row == RName) {
            placeholder = newName_.empty();
            val = (placeholder ? std::string("Wanderer") : newName_) + (sel ? cursor : " ");
        } else if (row == RLevel) val = lv.name + (lv.title.empty() ? "" : " – " + lv.title);
        else if (row == RDiff) val = diffs[(size_t)newDiff_].label;
        if ((row == RLevel || row == RDiff) && sel) val = "◂  " + val + "  ▸";
        r_->text(x + 360 * s, ry + 14 * s, 24 * s, val,
                 placeholder ? (sel ? rgba8(90, 76, 40) : kMuted) : (sel ? kSelText : kValue));
    }
    float dy = y + 10 * s + n * rowH + 20 * s;
    const auto& d = diffs[(size_t)newDiff_];
    r_->text(x + 46 * s, dy, 20 * s, d.label + ": " + d.description, kValue);
    if (confirm) {
        h.playSound("ui_select");
        Row r = rows[(size_t)newSel_];
        if (r == RStart) {
            std::string name = newName_;
            while (!name.empty() && name.back() == ' ') name.pop_back();
            h.newGame(name.empty() ? "Wanderer" : name, d.key, lv.id);
        } else if (r == RBack) page_ = "main";
        else newSel_ = std::min(newSel_ + 1, n - 1);
    }
    if (in_->pressed(VK_ESCAPE) || in_->padPressed(input::Pad::B) || (!typing && in_->actionPressed(Action::UiBack))) {
        h.playSound("ui_back");
        page_ = "main";
    }
}
void Ui::titleLoad(Host& h, float x, float y, float w) {
    float s = S();
    int n = (int)saves_.size();
    if (!n) {
        page_ = "main";
        return;
    }
    loadSel_ = std::clamp(loadSel_, 0, n - 1);
    if (in_->actionPressed(Action::UiUp)) loadSel_ = (loadSel_ + n - 1) % n, confirmDelete_ = false, h.playSound("ui_move");
    if (in_->actionPressed(Action::UiDown)) loadSel_ = (loadSel_ + 1) % n, confirmDelete_ = false, h.playSound("ui_move");
    float entryH = 150 * s;
    int visible = std::max(1, (int)((H_ * 0.62f) / entryH));
    int first = std::clamp(loadSel_ - visible / 2, 0, std::max(0, n - visible));
    bool confirm = in_->actionPressed(Action::UiConfirm);
    for (int i = first; i < std::min(n, first + visible); ++i) {
        const auto& e = saves_[(size_t)i];
        float ey = y + (float)(i - first) * entryH;
        if (in_->cursorMoved && hovered(x + 24 * s, ey, w - 48 * s, entryH - 10 * s)) loadSel_ = i;
        if (clicked(x + 24 * s, ey, w - 48 * s, entryH - 10 * s)) {
            loadSel_ = i;
            confirm = true;
        }
        bool sel = i == loadSel_;
        r_->rect(x + 24 * s, ey, w - 48 * s, entryH - 10 * s, sel ? rgba8(60, 48, 24, 235) : rgba8(22, 20, 16, 200), 8 * s,
                 sel ? 1.5f * s : 0.0f, kSelBg);
        // Vorschaubild
        float tw = 224 * s, th = 126 * s;
        std::string key = "save:" + e.path;
        auto* srv = h.renderer().uiImage(key);
        if (!srv && !e.thumb.empty()) srv = h.renderer().uiImage(key, e.thumb.data(), e.thumbW, e.thumbH);
        if (srv) r_->image(srv, x + 36 * s, ey + 7 * s, tw, th);
        else r_->rect(x + 36 * s, ey + 7 * s, tw, th, rgba8(30, 28, 22));
        const Json& d = e.data;
        const Json& st = d["stats"];
        int pt = (int)st["play_time"].asNumber(0);
        float tx = x + 36 * s + tw + 24 * s;
        r_->text(tx, ey + 10 * s, 24 * s, d["name"].asString("?"), sel ? kValue : kTitle, gfx::FontFace::Bold);
        r_->text(tx, ey + 44 * s, 18 * s,
                 std::format("{}   ·   Seed {}", h.content().difficulty(d["difficulty"].asString("medium")).label,
                             d["seed"].asInt()),
                 kText);
        r_->text(tx, ey + 70 * s, 18 * s, "Gespeichert " + d["saved_at"].asString(""), kMuted);
        r_->text(tx, ey + 96 * s, 18 * s,
                 std::format("Spielzeit {}:{:02} min   ·   {:.0f} m   ·   Health {} %   ·   Sanity {} %", pt / 60, pt % 60,
                             d["player"]["distance"].asNumber(0), (int)std::lround(st["health"].asNumber(100)),
                             (int)std::lround(st["sanity"].asNumber(100))),
                 kMuted);
    }
    if (confirm) {
        h.playSound("ui_select");
        confirmDelete_ = false;
        if (!h.loadGame(saves_[(size_t)loadSel_])) titleMsg_ = "Spielstand konnte nicht geladen werden.";
    }
    if (in_->actionPressed(Action::UiDelete)) {
        const auto& e = saves_[(size_t)loadSel_];
        if (e.legacy()) {
            titleMsg_ = "Spielstände aus V3/V4/V5 bleiben erhalten und werden nicht gelöscht.";
        } else if (confirmDelete_) {
            game::savegame::remove(e);
            saves_ = game::savegame::list();
            confirmDelete_ = false;
            if (saves_.empty()) page_ = "main";
        } else {
            confirmDelete_ = true;
        }
    }
    if (in_->actionPressed(Action::UiBack)) {
        h.playSound("ui_back");
        page_ = "main";
        confirmDelete_ = false;
    }
}

// --- Pausenmenue --------------------------------------------------------------------------------------
void Ui::drawPause(Host& h) {
    float s = S();
    float w = std::min(W_ - 60 * s, 820 * s);
    float hh = std::min(H_ - 60 * s, page_ == "main" ? 720 * s : 900 * s);
    float x = (W_ - w) * 0.5f, y = (H_ - hh) * 0.5f;
    panel(x, y, w, hh);
    static const std::pair<const char*, const char*> subs[] = {{"main", ""}, {"mouse", "Maus & Steuerung"},
                                                               {"graphics", "Grafik & Anzeige"}, {"audio", "Audio"},
                                                               {"help", "Hilfe"}};
    std::string sub;
    for (auto& [p, t] : subs)
        if (page_ == p) sub = t;
    float cy = header(x, y, w, "L I M I N A L", sub);
    float footY = y + hh - 58 * s;
    if (page_ == "main") {
        std::vector<Item> items = {
            {"Weiter erkunden", nullptr, nullptr, [this] { setMode(Mode::Hud); },
             "Zurück ins Spiel. Die Maus wird wieder zum Umsehen verwendet."},
            {"Maus & Steuerung", nullptr, nullptr, [this] { pageStack_.push_back(page_), page_ = "mouse"; },
             "Empfindlichkeit, Achsen, Invertieren, Glättung und Gamepad."},
            {"Grafik & Anzeige", nullptr, nullptr, [this] { pageStack_.push_back(page_), page_ = "graphics"; },
             "Qualität, Darstellung, Schatten, Sichtweite, Sichtfeld, Bildrate und Bewegung."},
            {"Audio", nullptr, nullptr, [this] { pageStack_.push_back(page_), page_ = "audio"; }, "Lautstärken."},
            {"Hilfe", nullptr, nullptr, [this] { pageStack_.push_back(page_), page_ = "help", helpScroll_ = 0; },
             "Alle Steuerungen und Funktionen des Spiels im Überblick."},
            {"Spiel speichern", nullptr, nullptr, [&h] { h.saveGame(); },
             "Speichert den aktuellen Spielstand. Er erscheint im Hauptmenü unter »Spiel fortsetzen«."},
            {"Zum Hauptmenü", nullptr, nullptr, [&h] { h.toTitle(); }, "Speichert und kehrt zum Hauptmenü zurück."},
            {"Spiel beenden", nullptr, nullptr, [&h] { h.quit(); },
             "LIMINAL schließen. Spielstand und Einstellungen werden gespeichert."},
        };
        ListState& st = lists_[0];
        runList(h, items, st, x + 24 * s, cy, w - 48 * s, 48 * s, 8);
        float dy = cy + 8 * 48 * s + 12 * s;
        for (const auto& line : r_->wrap(items[(size_t)std::clamp(st.sel, 0, 7)].desc, 19 * s, w - 90 * s)) {
            r_->text(x + 40 * s, dy, 19 * s, line, kMuted);
            dy += 26 * s;
        }
        if (auto* ses = h.session()) {
            const auto& stt = ses->stats();
            int left = std::max(0, (int)(game::GameSession::kAutosave - ses->autosaveTimer));
            r_->text(x + 40 * s, footY - 58 * s, 18 * s,
                     std::format("Seed {}   ·   {:.0f} m zurückgelegt   ·   Health {} %   ·   Sanity {} %   ·   Autosave in {}:{:02} min",
                                 ses->seed, ses->player().distance, (int)std::lround(stt.health),
                                 (int)std::lround(stt.sanity), left / 60, left % 60),
                     kMuted);
        }
        // ESC/B oder erneut Start (Gamepad) schliesst das Menue
        if (in_->actionPressed(Action::UiBack) || in_->actionPressed(Action::Pause)) {
            h.playSound("ui_back");
            setMode(Mode::Hud);
        }
    } else if (page_ == "help") {
        helpPage(h, x, cy, w, footY - cy - 10 * s);
    } else {
        settingsPage(h, page_, x, cy, w, footY - cy);
    }
    footer(x, footY, w, page_ == "help" ? "↑↓ / Mausrad blättern    ESC zurück" : "↑↓ Auswahl    ←→ Wert    Enter OK    ESC zurück");
}

}  // namespace lim::ui
