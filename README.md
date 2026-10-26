# LIMINAL

Ein Erkundungs- und Horrorspiel in den „Backrooms“ für Windows, in C++20 mit eigener Direct3D-11-Engine. Man wandert durch Ebenen, die sich an den Beschreibungen des Backrooms-Wikis (Wikidot) orientieren, sammelt Vorräte, liest Notizen anderer Wanderer und weicht Wesen aus, die das Wiki beschreibt.

Dieses Repository sichert die Entwicklungsstände. Jede Version liegt in einem eigenen Ordner (`LIMINAL V<n>`); ältere Versionen werden nie verändert, eine neue Version beginnt als Kopie der vorigen.

## Versionen

| Version | Branch / Tag | Inhalt in Kürze |
|---|---|---|
| V1–V10 | `main` | Frühere Stände (seit langem öffentlich in diesem Repository). |
| V11 | `liminal-v11` / Tag `v11` | Zehn feste Ebenen als eine Reise von Level 0 bis Level 37, tödliche Begegnungen mit Wiedereinstieg, Wasser, eigene Klänge je Ebene. Bericht: `LIMINAL V11/CHANGELOG.md`. |
| V12 | `liminal-v12` / Tag `v12` | Autonomes Neudesign der zehn Ebenen mit festen Karten aus Python-Skripten, Modellen aus Blender, CC0-Texturen (ambientCG), neuer Oberfläche. Bericht: `LIMINAL V12/REPORT.md`. |
| V13 | `liminal-v13` / Tag `v13` | Neue Hand an der Taschenlampe, sechs Wesen neu gebaut, Kachelbruch der Texturen, Fehlersuche. Fortsetzung V13.2: eigene Bewegung aller Wesen, neues Mottennest, fünf weitere Wesen aus dem Wiki (Faceling, Fensterwesen, Skin-Stealer, Wretch, Death Rat), Koop-Mehrspieler für bis zu vier Spieler als **Beta** (verschlüsselt, Host entscheidet; keine Garantie vollständiger Sicherheit – siehe `MULTIPLAYER_*.md`). Berichte: `LIMINAL V13/REPORT_V13.md`, `LIMINAL V13/REPORT_V13_2.md`. |

## Voraussetzungen

- Windows 10 oder 11 (64 Bit), Grafikkarte mit Direct3D 11.
- Zum Bauen: PowerShell. Das Bauskript lädt eine portable Toolchain (llvm-mingw, CMake, Ninja) in `.toolchain\` des jeweiligen Versionsordners; es ändert nichts am System.
- Optional für die Modelle: Blender 4.5 (`tools\setup_blender.ps1` lädt eine portable Fassung nach `.toolchain\blender\`).

## Bauen und starten

Im Ordner der gewünschten Version, z. B. `LIMINAL V13`:

```
powershell -ExecutionPolicy Bypass -File tools\build.ps1
build\Release\bin\LiminalGame.exe
```

Mit `--level level5` startet man direkt auf einer Ebene, mit `--seed N` mit festem Seed. Tests stehen in der README des Versionsordners und im jeweiligen Bericht.

Fertige Spielpakete (ausführbare Dateien) liegen nicht im Repository.

## Lizenzen und Rechtslage

- Fremdes Material, Quellen und Lizenzen: `LICENSES.md` im Versionsordner (ab V12), dazu `ATTRIBUTION.md` und `LICENSE_REVIEW.md` (V11).
- Inhalte des Backrooms-Wikis (Wikidot) stehen unter CC BY-SA 3.0; im Projekt werden sie nur in eigenen Worten und mit Quellenangabe verwendet.
- **Die Lizenz des Spiels ist nicht geklärt.** Der Licensing Guide des Wikis knüpft Spiele, die auf Wiki-Inhalten aufbauen, an CC BY-SA 3.0 oder GPLv3. Die Entscheidung steht aus; dieses Repository ist deshalb privat zu halten. Nichts hier behauptet, die Rechtslage sei geklärt.
