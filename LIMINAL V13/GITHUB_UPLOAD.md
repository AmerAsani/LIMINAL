# GitHub-Upload

Repository: https://github.com/AmerAsani/LIMINAL (öffentlich, `origin`). Auftrag des Nutzers vom 02.11.2026: V11 und V12
öffentlich wie V1–V10; V13 erst, wenn V13.3 vollständig fertig ist (ersetzt E-025, siehe E-043).

## Stand 02.11.2026

| Ref | Commit | Inhalt |
|---|---|---|
| `main` | `3952886` | V1–V10, neues README (Bildleiste, Funktionen, Download, Steuerung, Bau, Versionen V1–V13, Lizenzen), `LICENSES.md`, `ATTRIBUTION.md`, `licenses/`, `docs/HISTORY.md`, `docs/readme/` |
| `liminal-v11` | `67ccfe8` | V11 + Wurzeldateien (README ohne „privat“, Lizenzen) |
| `liminal-v12` | `1cd40a1` | V12 + Wurzeldateien |
| Tag `v11` | → `42529a2` | Stand V11 vom 26.10.2026 (Tag unverändert) |
| Tag `v12` | → `60557d7` | Stand V12 vom 26.10.2026 (Tag unverändert) |

Geprüft mit `git ls-remote origin` nach jedem Push. **Nicht hochgeladen:** `liminal-v13`, Tags `v13`, alle Agenten-Branches.
`main` enthält keinen Commit von `liminal-v13` (`git merge-base --is-ancestor` geprüft; einziger neuer Commit auf `main`
ist `3952886` mit Elternteil `c701337`).

## Prüfung vor dem ersten Push

- **Geheimnisse, persönliche Pfade, Adresse:** alle 918 neuen Dateien (204 MB) von `liminal-v11`/`liminal-v12`
  durchsucht (Token-Muster GitHub/AWS/Slack/Google/Anthropic/OpenAI, private Schlüssel, Zugangsdaten in URLs,
  `C:\Users\…`, Temp-Pfade, E-Mail). Treffer: nur `publicKeyToken` in zwei Windows-Manifesten (öffentliches
  Microsoft-Token, kein Geheimnis). Werkzeug: `audit_push.py` (Arbeitsordner der Sitzung).
- **Größen:** keine Datei über 20 MB; Zips von V5–V10 liegen wie bisher in Git LFS.
- **LFS:** `git lfs push origin main liminal-v11 liminal-v12` statt `--all`, damit keine LFS-Objekte aus V13- oder
  Agenten-Branches hochgehen (an den Spitzen gleiche 9 Objekte).
- **Lizenzen:** Wiki CC BY-SA 3.0 mit Namensnennung je Seite und Änderungsvermerk (`ATTRIBUTION.md`), ambientCG CC0
  (Liste in `LIMINAL V12/LICENSES.md`), Lizenztexte llvm-mingw/LLVM (Apache 2.0 mit LLVM Exceptions), mingw-w64-Laufzeit
  und Inter (OFL 1.1) neu unter `licenses/`. Offener Punkt aus `LICENSE_REVIEW.md`: **die Lizenz des Spiels ist nicht
  gewählt** – Entscheidung des Eigentümers, in `LICENSES.md` offen benannt.
- **Wiki-Texte und Bilder:** keine Wiki-Texte oder -Bilder im Repository (V11/V12-Dokumente in eigenen Worten).
- **Fotos:** außer den CC0-Materialtexturen keine fremden Fotos, keine Bilder realer Personen.

## Repository-Seite

- Beschreibung: „LIMINAL 🟨🚪 – a first-person Backrooms exploration and horror game for Windows, built on its own
  C++20 / Direct3D 11 engine. Levels and entities based on the Backrooms Wiki (CC BY-SA 3.0). V1–V12 here, V13 coming.“
- Themen: backrooms, horror-game, first-person, windows, cpp, directx, directx11, game-engine, liminal-space.
- Gesetzt über den angemeldeten Chrome (kein `gh`, keine Tokens gelesen).

## Releases

| Release | Stand | Paket |
|---|---|---|
| v11 „LIMINAL V11“ | **veröffentlicht** | `LIMINAL_V11.zip`, 2,1 MB, sha256 `2394a79c…694f`; probegestartet |
| v12 „LIMINAL V12“ | **Entwurf** | `LIMINAL_V12.zip`, 157 MB, liegt unter `C:\Users\AmerA\Downloads\LIMINAL\Release\` (nicht im Repository); probegestartet |

Das Browser-Werkzeug lädt höchstens 10 MB je Datei hoch; das V12-Paket muss der Nutzer in den Entwurf ziehen und
„Publish release“ klicken. Die Pakete enthalten `LIMINAL.exe`, `data`, `shaders`, `assets`, `LIESMICH.txt` und `licenses/`,
gebaut außerhalb der eingefrorenen Ordner (V12-Manifest danach: 25 550 Dateien, 0 verändert).

## Bilder im README (02.11.2026)

Neu aufgenommen in 1920×1080 aus den eingefrorenen V12- und V11-Builds (eigener Benutzerordner, bis zu 7 Kandidaten je Ebene), je
Bild einzeln angesehen. Gewählt (JPG q85, je 83–229 KB, zusammen 1,5 MB, `docs/readme/`):

| Bild | Warum |
|---|---|
| `v12_level0_lobby` | gelbe Räume, Rasterleuchten, Tiefe – das Grundmotiv |
| `v12_level1_habitable_zone` | Betonhalle mit Pfeilern und Farbbändern, weite Sicht |
| `v12_level2_pipe_dreams` | Tunnel mit Tanks, Rohren und Gleis, starke Fluchtlinie |
| `v12_level3_electrical_station` | Ziegelgang mit Engelschild und Rohren |
| `v12_level5_terror_hotel` | Hotelhalle mit Kronleuchter und Damast |
| `v12_level5_3_library` | Bibliothekssaal mit Säulen und Regalen |
| `v12_level903_metro` | Bahnsteig mit Zug und Leuchtbändern |
| `v11_level1_parking` | Parkdeck aus V11, kühles Licht |
| `v11_level5_hotel` | endloser Hotelgang aus V11, warmes Licht |

Verworfen: Level 188 (Häuser oben abgeschnitten, viel leere Wiese), Level 37 (halbes Bild nahe Wand), Level 36
(verschmierte Spiegelungen am Glas, unscharfer Boden), dazu alle dunklen oder wandnahen Kandidaten. Die alten Bilder
(V10-Übersicht und fünf über GitHub hochgeladene 1280×720-Bilder älterer Versionen) sind aus dem README entfernt. Im Repository lagen keine
Bilddateien außerhalb der Versionsordner; die Bilder in den Versionsordnern bleiben (Versionen werden nie verändert).

## Offen

- V12-Paket an den Release-Entwurf hängen und veröffentlichen (Nutzer, siehe oben).
- Pull Request #2 („Liminal v11“, `liminal-v11` → `main`) ist offen, angelegt unter dem Konto des Nutzers; nicht von mir
  zusammengeführt oder geschlossen.
- Nach Abschluss von V13.3: `liminal-v13` und Tag `v13.3` hochladen, README mit V13-Bildern (12–16, je Ebene eines,
  Hand mit Taschenlampe, drei Wesen, Wasser/Spiegelung, Koop) und Koop-Anleitung fertigstellen.
