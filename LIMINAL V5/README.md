# LIMINAL V5

Ein First-Person-Erkundungsspiel in einer unendlichen, prozedural erzeugten Welt aus Innenräumen: Büroflure, Betonhallen, unterirdische Tunnel, riesige leere Hallen und enge Wartungsgänge. Deine Sanity schwindet mit der Zeit, und nur seltene Vorräte halten dich bei Verstand.

V5 ist eine komplette Neuentwicklung in **C++20** mit einem eigenen **Direct3D-11-Renderer**. V4 lief als Python-Programm mit Zeichengrafik im Terminal. Die Welt ist dieselbe: Die Generierung ist bitgenau aus V4 übernommen, sodass derselbe Seed dieselben Räume erzeugt. V4-Spielstände lassen sich weiterspielen.

---

## Starten

Doppelklick auf **`LIMINAL.exe`**.

Der Starter prüft zuerst, ob der PC alles Nötige hat:

- Windows-Version
- Direct3D 11
- Microsoft C-Laufzeit
- Vollständigkeit des Spielordners

Danach startet er `LiminalGame.exe`. Fehlt etwas, erklärt er verständlich, was zu tun ist. Wo es geht, bietet er die **offizielle** Installation von Microsoft an:

- Er lädt nur von `aka.ms` / microsoft.com.
- Er prüft die digitale Signatur der heruntergeladenen Datei.
- Er installiert erst, nachdem du zugestimmt hast.

Einen versteckten Download oder eine versteckte Installation gibt es nicht.

**Voraussetzungen**

| | |
|---|---|
| System | Windows 10 oder 11, 64 Bit |
| Grafik | Direct3D 11 (Feature-Level 11.0), aktueller Treiber. Ohne passende Grafikkarte gibt es einen (langsamen) Software-Modus. |
| Laufzeit | Universal C Runtime (in Windows 10/11 enthalten) |

Das Spiel ist statisch gebaut und braucht keine zusätzlichen DLLs. Es nutzt nur Windows-Systembibliotheken.

---

## Neu in V5

### Technik
- **C++20** statt Python.
- **Modularer Aufbau:** Die Engine besteht aus Modulen mit klaren Abhängigkeiten, siehe [Architektur](#architektur).
- **Streaming auf mehreren Kernen:** Sektoren, Räume und Chunk-Netze entstehen in einem Job-System auf Worker-Threads. Der Hauptthread übernimmt nur die fertigen Ergebnisse.
- **Datengesteuert:** Level, Zonen, Raumtypen, Lichtstile, Items, Schwierigkeiten, Tastenbelegung und Prefabs (Entities) liegen als JSON unter `data/`.

### Grafik
- **Deferred Rendering mit PBR:** GGX/Lambert. Alle Leuchten sind Flächenlichter.
- **Beleuchtung pro Kachel:** Ein Compute-Shader teilt das Bild in Kacheln (16 × 16 Pixel) und berechnet für jede nur die Lichter, die sie betreffen (bis zu 128 je Kachel).
- **Weiche Schatten aller Lampen.** Grundlage ist die Kachelwelt als Höhenfeld: Kein Licht dringt durch Wände, Treppen werfen korrekte Schatten.
- **Weitere Effekte:**
  - SSAO (Kontaktschatten)
  - Spiegelungen auf glatten Böden (SSR)
  - Lichtschein im Dunst (volumetrisch)
  - Bloom
  - automatische Belichtung
  - AgX-Tonemapping
  - Filmkorn, Vignette, chromatische Aberration
- **TAA (temporales Anti-Aliasing):** glättet Kanten und die weichen Halbschatten.
- **Prozedurale PBR-Texturen** bis 2048 × 2048: Tapeten, Teppich, Fliesen, Beton, Ziegel, Paneele, Gitter. Mit Mipmaps, Normal- und Rauheitskarten und großflächiger Variation gegen sichtbare Wiederholung.
- **Retro-Darstellungen aus V4** als Filter (Taste **V**): Terminal, ASCII, Monochrom.
- **Leistung:** Auf einer RTX 3070 bei 1920 × 1080 mit allen Effekten im Mittel **2,1 ms pro Bild (≈ 470 FPS)**. Das langsamste Bild braucht 3,4 ms (Benchmark über 3000 Bilder mit rund 180 Lichtern).
  - Nur ein Draw-Call pro Chunk.
  - Frustum-Culling, Lichtauswahl nach Entfernung, Tiefenvorsortierung.
  - Puffer werden von Bild zu Bild wiederverwendet statt neu angelegt.

### Spiel
- **Neues Level:** *Level 1 – Das Tiefgeschoss* (Kellergänge, Betonkeller, Technikgeschoss). Es besteht nur aus Daten, siehe `data/levels/level1.json`. Das Level wählst du unter *Neues Spiel*.
- **Klang:** Alle Klänge werden prozedural erzeugt.
  - Brummen der Leuchtstoffröhren, Automaten, Schritte je Untergrund, Herzschlag
  - Ambience je Zone
  - positionsabhängig (Stereo), latenzarm über WASAPI
- **Gamepad-Unterstützung** (XInput), frei belegbare Tasten in `data/input.json`.
- **Spielstände:** Spielstände mit Vorschaubild, abgesichert gegen Abstürze (temporäre Datei + `.bak`).
- **Fehlerbehandlung:** Bei einem Absturz schreibt das Spiel einen Fehlerbericht (Minidump).

---

## Steuerung

| Eingabe | Funktion |
|---|---|
| W / A / S / D | gehen |
| Maus (Pfeiltasten als Alternative) | umsehen |
| Shift | sprinten |
| + / − | Laufgeschwindigkeit |
| **E** | Item aufnehmen |
| **F** | gewähltes Item der Schnellleiste benutzen |
| **1–9 / Mausrad** | Platz der Schnellleiste wählen |
| **Tab** | Inventar |
| M | Karte |
| ESC | Menü (Einstellungen, Hilfe, Speichern, Hauptmenü) |
| H oder F1 | Hilfe |
| F3 oder I | Debug-Anzeige |
| V | Darstellung: Modern / Terminal / ASCII / Monochrom |
| L | Leistungsmodus (halbe interne Auflösung) |
| B | Head-Bobbing |
| P oder F12 | Screenshot (PNG in *Bilder\LIMINAL*) |
| Alt + Enter | Vollbild / Fenster |

**Gamepad**

| Eingabe | Funktion |
|---|---|
| linker Stick | gehen |
| rechter Stick | umsehen |
| L3 | sprinten |
| A | aufnehmen |
| X | benutzen |
| Y | Inventar |
| LB / RB | Schnellleiste |
| Start | Menü |
| Back | Karte |

Alle Tasten lassen sich in `data/input.json` ändern.

---

## Einstellungen

Du findest sie im Menü (ESC → Einstellungen). Die Einstellungen werden in `%APPDATA%\LIMINAL\settings_v5.json` gespeichert. Beim ersten Start übernimmt V5 die Werte aus V3/V4 (`settings_v3.json`), soweit sie passen.

- **Grafik & Anzeige:**
  - Qualitätsstufen Niedrig / Mittel / Hoch / Ultra
  - Einzeloptionen: interne Auflösung (50–150 %), Schatten, SSAO, TAA, Bloom, SSR, volumetrisches Licht, Texturqualität, anisotrope Filterung, Sichtweite, Helligkeit, Filmkorn, Vignette, chromatische Aberration, Sichtfeld, Bildratenbegrenzung, VSync, Vollbild, Head-Bobbing, Laufgeschwindigkeit
- **Maus & Steuerung:** Empfindlichkeit, Achsen getrennt, Invertieren, Glättung, Gamepad-Empfindlichkeit.
- **Audio:** Gesamt, Effekte, Ambience.

**Nur in der Datei:** Folgende Feinwerte lassen sich nur in der Einstellungsdatei ändern:

- `ambient_strength`, `light_intensity`
- `fog_brightness`, `fog_density`
- `ao_direct`
- `exposure_auto`, `exposure_base`, `bloom_strength`

Ungültige Werte werden beim Laden auf sinnvolle Bereiche begrenzt.

---

## Spielstände und Kompatibilität

| Was | Wo |
|---|---|
| Spielstände V5 | `%APPDATA%\LIMINAL\saves_v5\` (JSON mit Vorschaubild, `.bak` als Sicherung) |
| Spielstände V4 | `%APPDATA%\LIMINAL\saves_v4\`. Erscheinen in der Liste mit „(V4)“, werden nur gelesen und beim Speichern als V5-Stand fortgeführt. |
| Spielstand V3 | `%APPDATA%\LIMINAL\save_v3.json`. Wird ebenfalls gefunden. |
| Einstellungen | `%APPDATA%\LIMINAL\settings_v5.json` |
| Screenshots | `Bilder\LIMINAL\` |
| Protokoll | `%LOCALAPPDATA%\LIMINAL\logs\liminal.log` |
| Fehlerberichte | `%LOCALAPPDATA%\LIMINAL\crash\` |

V5 verändert keine Dateien von V3 oder V4. Mit der Umgebungsvariable `LIMINAL_USER_DIR` lässt sich der Benutzerordner umlegen, z. B. für eine portable Installation auf einem USB-Stick.

---

## Kommandozeile

Alle Optionen gelten für `LIMINAL.exe` und `LiminalGame.exe`.

| Option | Wirkung |
|---|---|
| `--seed N` `--name X` `--difficulty easy\|medium\|hard` `--level level1` | direkt ein neues Spiel starten |
| `--windowed` `--size 1600x900` | Fenstermodus und Größe |
| `--warp` | Software-Rendering (ohne Grafikkarte) |
| `--mode modern\|terminal\|ascii\|mono` | Darstellung |
| `--debug` | Debug-Anzeige einschalten |
| `--set schluessel=wert` | Einstellung nur für diesen Start ändern, z. B. `--set ssr=false` |
| `--benchmark N` | N Bilder automatische Kamerafahrt, Ergebnis im Protokoll |
| `--demo` | automatische Kamerafahrt (Vorführung) |
| `--shot datei.png` `--shot-frames N` `--room typ[@zone]` `--shot-ui seite` `--pos x,y` `--yaw` `--pitch` | Test- und Bildwerkzeuge |
| `--script "wait 40; key Esc; expect Pause/main; …"` | automatischer Bedientest: spielt Tastendrücke ab und prüft Menüzustände (`wait N`, `key Taste`, `text …`, `expect Modus[/Seite]`, `shot datei.png`, `quit`). Exitcode 7 bei Fehler. |

---

## Architektur

```
src/
  core/      Grundlagen: Mathematik, Zufall (V4-kompatibel), JSON, Pfade, Protokoll, Job-System
  platform/  Fenster, Raw-Input, Bilddateien (WIC), Absturzbericht
  input/     Aktionen, Tastenbelegung, XInput
  world/     Level-Definition (Daten), Zonen, Raumgenerator (bitgenau V4), Sektoren,
             Streaming, Chunk-Aufbau (Kacheln, Lichter, Netze)
  physics/   Kollision gegen die Kachelwelt (Kapsel, Stufen, Kanten)
  ecs/       Entity-Component-System (Sparse Sets)
  render/    Direct3D 11: GPU-Hilfen, Welt-/Mesh-/UI-Renderer, prozedurale Texturen, Schrift, Post-Processing
  audio/     WASAPI-Mixer, prozedurale Klänge, positionsabhängige Wiedergabe
  gameplay/  Spieler, Werte, Inventar, Items, Prefabs, Komponenten, Spielsitzung, Spielstände
  ui/        Menüs, HUD, Inventar, Karte (sprechen über eine Host-Schnittstelle mit der Anwendung)
  app/       Anwendung: verbindet alle Module, Hauptschleife, Einstellungen
shaders/     HLSL; werden beim Bauen zu .cso kompiliert
launcher/    LIMINAL.exe (Starter ohne C-Laufzeit)
data/        Level, Items, Schwierigkeiten, Tastenbelegung, Prefabs
tests/       Test der Weltgenerierung gegen Referenzdaten aus V4
tools/       Build-, Paket- und Toolchain-Skripte
```

**Grundregeln**

- **Welt, Spiellogik, Darstellung und Menüs sind getrennt.**
  - `world` kennt weder Renderer noch Spiel.
  - `gameplay` kennt den Renderer nicht. Es liefert nur Listen sichtbarer Modelle und Klangquellen.
  - Der Renderer bekommt fertige Chunk-Daten.
  - Die UI spricht nur über `ui::Host` mit der Anwendung.
- **Level sind reine Daten.** Der Generator liest Zonen, Raumtypen und Lichtstile aus der Level-Datei.

---

## Erweitern

### Neues Level

1. `data/levels/level1.json` kopieren, z. B. nach `data/levels/level2.json`.
2. `id`, `name` und `title` ändern.
3. Zonen, Raumtypen und Lichtstile anpassen.

Das Level erscheint automatisch unter *Neues Spiel*. Du musst nichts kompilieren.

**Zonen** (`zones`): Bereiche der Welt, die weich ineinander übergehen.

- `key`, `name`
- Farben: `wall`, `floor`, `ceil`, `trim`, `lamp`, `fog`
- `fog_density`, `ambient`, `light_style`
- Muster: `wall_pattern` (`plain`, `wallpaper`, `blocks`, `bricks`, `panels`, `concrete`), `floor_pattern` / `ceil_pattern` (`none`, `carpet`, `slabs`, `bigtiles`, `grate`, `tiles`, `checker`, `lines`)
- Raumaufteilung: `min_leaf`, `max_leaf`, `big_chance`, `corridor_chance`, `corridor_width`, `wall_thickness`, `loop_chance`
- Höhen: `h_room`, `h_corridor`, `h_big`, `door_height`
- optional: `sunken`, `sunken_depth`, `origin_bias`, `door_clearance`, `wide_doors`

Die Ambience der Zone heißt `amb_<key>`. Für eigene Zonen legst du dazu eine WAV-Datei unter `data/sounds/` ab.

**Raumtypen** (`room_types`):

- `key`, `label`, `category` (`normal`, `corridor`, `large`, `unusual`, `rare`)
- Größen: `short`, `long`
- `affinity`: Gewicht je Zone
- `features`: `counter`, `pillars`, `pillars_sparse`, `thick_pillars`, `giant_pillars`, `colonnade`, `cubicles`, `shelves`, `crates`, `machines`, `podium`, `monolith`, `beams`, `vault`, `pipes`, `checker`
- optional: `height`, `sunken`, `stair_step`, `light`, `ambient_mul`, `fog_mul`, `supply`, `biggish`, `extra_doors`, `multi_doors`, `style`

**Lichtstile** (`light_styles`):

- Anordnung: `panels`, `panels_wide`, `panels_long`, `pools`, `wall`, `emergency`, `void`, `bright`, `dark`
- Werte je Stil: `extra`, `intensity`, `ambient`, `dark_fog`
- `center_fallback`: Räume, in die das Lampenraster keine Lampe setzt, bekommen eine Lampe in der Mitte.

**Items in Versorgungsräumen:** `supply_items` mit Gewichten.

**Entities** (`entities`): Setzt Prefabs in die Mitte passender Räume. Level 1 nutzt das für tropfendes Wasser in Kellerräumen:

```json
"entities": [
  { "prefab": "water_drip", "room_types": ["gewoelbe", "tunnel"], "chance": 0.35, "height": 2.2 }
]
```

`data/levels/level0.json` ist die unveränderte V4-Welt. Der Test `worldgen_test` prüft, dass sie bitgenau der V4-Welt entspricht.

### Neues Item

1. In `data/items.json` einen Eintrag anlegen:
   - `key`, `name`, `description`
   - Wirkung: `sanity`, `health`, `speed` mit `speed_time`
   - `model`, `icon`, `scale`, `use_sound`, `max_stack`
2. Den Eintrag in `supply_items` eines Levels aufnehmen.

Ein neues 3D-Modell registrierst du in `src/render/meshes.cpp` (`MeshLibrary::init`). Danach verwendest du es in Items und Prefabs über seinen Namen. Ein neues Symbol legst du in `src/render/texture_gen.cpp` an.

### Neue Entities, NPCs, interaktive Objekte

**Prefabs** in `data/prefabs/*.json` setzen sich aus Komponenten zusammen:

| Komponente | Bedeutung |
|---|---|
| `transform` | Position, Drehung, Größe |
| `mesh` | sichtbares Modell, Sichtweite |
| `pickup` | aufnehmbares Item |
| `interactable` | Hinweistext, Reichweite |
| `audio_emitter` | Dauerklang an einer Position |
| `wanderer` | läuft selbstständig zwischen zufälligen Zielen umher und bleibt auf begehbaren Flächen |

Beispiel für ein umherwanderndes Objekt, das nur aus Daten besteht:

```json
{
  "name": "mein_npc",
  "components": {
    "transform": { "scale": 1.0 },
    "mesh": { "model": "almond", "draw_distance": 60 },
    "wanderer": { "speed": 1.2 }
  }
}
```

Über `entities` in der Level-Datei erscheint es in der Welt.

**Neue Komponenten** brauchen Code. Die Schritte:

1. Struktur in `src/gameplay/components.hpp` anlegen.
2. JSON-Leser in `src/gameplay/prefabs.cpp` registrieren.
3. System als Funktion in `GameSession` schreiben und in `GameSession::update` aufrufen. Vorlage: `updateWanderers`.

### Klänge

Die Klänge werden in `src/audio/synth.cpp` erzeugt. Eine Datei `data/sounds/<name>.wav` ersetzt den gleichnamigen Klang, z. B. `step_carpet_0.wav` oder `amb_office.wav`.

---

## Bauen

Du brauchst keine Visual-Studio-Installation. Das Skript lädt eine **portable Toolchain** in `.toolchain\` (nur dieser Ordner, keine Systemänderung):

- llvm-mingw (Clang, C++20)
- CMake
- Ninja

```bash
powershell -ExecutionPolicy Bypass -File tools\build.ps1
```

Das erzeugt ein startfertiges Build in `build\Release\bin\`. Mit `-Debug` entsteht ein Debug-Build, mit `-Clean` ein Neuaufbau.

```bash
powershell -ExecutionPolicy Bypass -File tools\package.ps1
```

Das erzeugt das Release-Paket:

- `Release\LIMINAL V5\`: Spielordner mit Programmen, Daten, Shadern, Icon, Lizenzhinweisen, `LIESMICH.txt`
- `Release\LIMINAL_V5.zip`

Test der Weltgenerierung gegen V4:

```bash
build\Release\tests\worldgen_test.exe data\levels\level0.json tests\data\v4_reference.json
```

Die Referenzdaten erzeugt `tools\v4_reference\dump_ref.py` aus dem unveränderten V4-Ordner (nur lesend).

---

## Hinweis: Signatur, SmartScreen und Smart App Control

Die Programme sind **nicht digital signiert**. Deshalb gilt auf anderen PCs:

- **SmartScreen** fragt beim ersten Start nach („Weitere Informationen“ → „Trotzdem ausführen“).
- **Smart App Control** (Windows 11) blockiert unsignierte Programme, wenn es aktiv ist.

Für eine Weitergabe an andere empfiehlt sich eine Code-Signatur, z. B. über **Azure Trusted Signing** oder ein Code-Signing-Zertifikat. Beide EXE-Dateien werden dann mit `signtool` signiert. Der Starter erkennt eine Blockade und erklärt sie.

---

## Fehlerbehebung

| Problem | Lösung |
|---|---|
| Starter meldet fehlende Dateien | Den kompletten Ordner behalten bzw. neu entpacken. Für den Desktop eine Verknüpfung auf `LIMINAL.exe` anlegen, nicht die EXE herauskopieren. |
| „Direct3D 11 nicht verfügbar“ / sehr langsam | Neuesten Grafiktreiber von NVIDIA, AMD oder Intel installieren. Ohne Grafikkarte ist nur der Software-Modus möglich (`--warp`). |
| Ruckeln | Grafikqualität *Mittel* oder *Niedrig* wählen, interne Auflösung senken oder Taste **L**. |
| Zu dunkel / zu hell | Grafik & Anzeige → Helligkeit. |
| Maus reagiert nicht | Das Spielfenster muss aktiv sein. Mit geöffnetem Menü ist die Maus frei. |
| Einstellungen kaputt | `%APPDATA%\LIMINAL\settings_v5.json` löschen. Beim nächsten Start werden Standardwerte angelegt. |
| Absturz | Fehlerbericht in `%LOCALAPPDATA%\LIMINAL\crash\` und Protokoll `liminal.log`. Der letzte Autosave bleibt erhalten. |
