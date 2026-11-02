# LIMINAL

Ein Erkundungs- und Horrorspiel in den „Backrooms“ für Windows, in C++20 mit eigener Direct3D-11-Engine.
Übersicht, Bilder und alle Versionen: [README auf `main`](https://github.com/AmerAsani/LIMINAL/blob/main/README.md).

Jede Version liegt in einem eigenen Ordner (`LIMINAL V<n>`); ältere Versionen werden nie verändert, eine neue Version
beginnt als Kopie der vorigen.

## Versionen

| Version | Branch / Tag | Inhalt in Kürze |
|---|---|---|
| V1–V10 | `main` | Frühere Stände, spielfertige Pakete von V5–V10 unter `downloads/`. |
| V11 | `liminal-v11` / Tag `v11` | Zehn feste Ebenen als eine Reise von Level 0 bis Level 37, tödliche Begegnungen mit Wiedereinstieg, Wasser, eigene Klänge je Ebene. Bericht: `LIMINAL V11/CHANGELOG.md`. |
| V12 | `liminal-v12` / Tag `v12` | Neudesign der zehn Ebenen mit festen Karten aus Python-Skripten, Modellen aus Blender, CC0-Texturen (ambientCG), neuer Oberfläche. Bericht: `LIMINAL V12/REPORT.md`. |
| V13 | folgt | Wird veröffentlicht, sobald V13.3 vollständig fertig ist. |

## Bauen und starten

Voraussetzungen: Windows 10 oder 11 (64 Bit), Grafikkarte mit Direct3D 11, PowerShell. Das Bauskript lädt eine portable
Toolchain (llvm-mingw, CMake, Ninja) nach `.toolchain\` im Versionsordner und ändert nichts am System.

Im Ordner der gewünschten Version, z. B. `LIMINAL V12`:

```
powershell -ExecutionPolicy Bypass -File tools\build.ps1
build\Release\bin\LiminalGame.exe
```

Mit `--level level5` startet man direkt auf einer Ebene, mit `--seed N` mit festem Seed.

## Lizenzen und Quellen

- Übersicht: [`LICENSES.md`](LICENSES.md); Namensnennung des Backrooms-Wikis: [`ATTRIBUTION.md`](ATTRIBUTION.md);
  Lizenztexte der mitgelieferten Bibliotheken und der Schrift: [`licenses/`](licenses/).
- Die Ebenen und Wesen beruhen auf dem [Backrooms Wiki](https://backrooms-wiki.wikidot.com) (Wikidot, CC BY-SA 3.0),
  nur in eigenen Worten und eigener Gestaltung.
- Für das Spiel selbst ist noch keine Lizenz gewählt (Einzelheiten in `LICENSES.md`).
