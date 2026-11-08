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
| v12 „LIMINAL V12“ | **veröffentlicht** (02.11.2026) | `LIMINAL_V12.zip`, 157 MB, in 18 Teilen `LIMINAL_V12.zip.001`–`.018` (je 8,58 MB, letzter 4,07 MB); Gesamtdatei sha256 `08927be46a7c6e17e4a8f1ffd854faf96854f8077b9ca36eb6538f1e24f0b166` |
| v13.3 „LIMINAL V13.3“ | **veröffentlicht** (08.11.2026, „Latest“) | `LIMINAL_V13.zip`, 263 MB, in 30 Teilen `LIMINAL_V13.zip.001`–`.030`; sha256 `a1bd446d1f5a538796caad2b1f39e51a77308a56792177e4f025c6d99d388fa5` |

Die Pakete enthalten `LIMINAL.exe`, `data`, `shaders`, `assets`, `LIESMICH.txt` und `licenses/`, gebaut außerhalb der
eingefrorenen Ordner (V12-Manifest danach: 25 550 Dateien, 0 verändert). Beide Release-Beschreibungen enthalten den Link
zum Quelltext des Branches und den Bau mit `tools\build.ps1`.

**Weg zum V12-Release:**
1. Bevorzugter Weg a): GitHub CLI 2.103.3 für den Nutzer installiert (`winget install GitHub.cli --scope user`, ohne
   Admin), `gh auth login --web` mit Geräte-Code im angemeldeten Chrome eingegeben und „Authorize GitHub CLI“ bestätigt.
   GitHub verlangte danach das Passwort („Confirm access“); das gibt Claude nicht ein. Der Nutzer bekam eine Zeile, der
   Code lief nach 15 Minuten ab (`context deadline exceeded`). `gh` bleibt installiert, ist aber nicht angemeldet; kein
   Token wurde gelesen oder gespeichert.
2. Weg b): Paket binär in 18 Teile unter 10 MB geteilt, alle über den Browser an den Entwurf gehängt, Anleitung zum
   Zusammensetzen (`copy /b LIMINAL_V12.zip.0* LIMINAL_V12.zip` oder 7-Zip) und Prüfsumme in die Beschreibung, dann
   veröffentlicht. Den Befehl vorher lokal geprüft (gleiche Prüfsumme).

**Kontrolle als Fremder (ohne Anmeldung, 02.11.2026):**

| Release | Seite (nicht angemeldeter Browser) | Download (curl ohne Anmeldedaten) | Prüfsumme | Start |
|---|---|---|---|---|
| v11 | sichtbar, 3 Assets | 2 100 911 Byte | gleich (`2394a79c…694f`) | 90 s ohne ERROR |
| v12 | sichtbar, „Latest“, 20 Assets | 18/18 Teile, mit `copy /b` zusammengesetzt | gleich (`08927be4…b166`) | 150 s ohne ERROR, Texturen geladen |

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

## Abschluss 02.11.2026 (Auftrag „GitHub Abschluss“)

- **Pull Request #2** („Liminal v11“, `liminal-v11` → `main`): geschlossen, nicht zusammengeführt, ohne Kommentar.
  `git ls-remote`: `main` danach unverändert (`3952886`). Wahrscheinlich entstand er durch einen Klick von mir neben das
  Zahnrad „About“, der das Banner „Compare & pull request“ traf; Banner werden nicht mehr angeklickt, es werden keine
  Pull Requests geöffnet.
- **Einstellungen:** nur gelesen. Settings → Rules → Rulesets: „You haven't created any rulesets“; Settings → Branches
  leitet auf Rulesets um (keine Branch-Regeln). Kein Ruleset angelegt, nichts gelöscht. Den Tab „New branch ruleset“
  hatte ich nicht in meiner Tab-Gruppe; möglich ist derselbe Fehlklick in die Bannerzeile („Protect this branch“).
  Geändert wurden nur Beschreibung, Themen und Releases.
- **Quelltext leicht findbar:** README auf `main` mit Abschnitt „Source code“ (Tabelle V1–V10 auf `main`, V11/V12 mit
  direkten Links auf die Ordner im Branch, V13 folgt, Klon-Befehl); Download-Zeilen auf die Releases (`main` jetzt
  `4b4f5fc`).
- **Bau-Test als Fremder:** frischer Klon nur des Branches (`git -c credential.helper= clone --branch liminal-v1x
  --single-branch`) in einen leeren Ordner, `tools\build.ps1`, Start:

  | Version | Klon | Bau | Start |
  |---|---|---|---|
  | V11 | 206 MB | rc 0 in 126 s (portable Werkzeugkette lädt sich selbst) | 150 s ohne ERROR |
  | V12 | 565 MB | rc 0 in 86 s; 185 Modelle wie im Original | 150 s ohne ERROR |

  Einzige Warnungen: fehlende Einstellungsdateien beim allerersten Start (normal). Keine Funde, nichts zu beheben;
  Versionsordner unverändert (V12-Manifest 0 verändert).

## V13-Upload (08.11.2026)

Voraussetzung erfüllt: alle zehn Ebenen abgenommen, `ENTITY_ABGLEICH` umgesetzt, Schlussprüfung mit `REPORT_V13_3.md` und
`SELBSTKONTROLLE.md` (ehrlich: Langlauf B-036 offen).

- **Prüfung vor dem Push:** 359 neue Commits, 7 546 neue Dateistände (1,62 GB), keine Datei über 15 MB, keine neuen
  LFS-Dateien; Suche nach Schlüsseln und Passwörtern (privater Schlüssel, `ghp_`, `github_pat_`, `AKIA`, Zuweisungen von
  `secret`/`password`/`token`/`api_key`) ohne Fund außer den PEM-Kopfzeilen als Zeichenketten in Mbed TLS
  (`pk_internal.h`). `PROMPT_V13_3.md` und `LIMINAL V11/` nicht im Branch.
- **Push nur benannter Refs, in drei Schritten** wegen der Größe (`9e1d96e` 0,89 GB, `bd460da` 1,55 GB, `3ffe707`),
  jeweils vorwärts; danach Tag `v13.3` (annotiert, auf `3ffe707`) und `784f0d4` (Wurzelordner des Branches: README,
  `LICENSES.md`, `ATTRIBUTION.md` mit den Wesen-Seiten von V13, `licenses/`). `git ls-remote`: `liminal-v13`, `v13.3`;
  `main` enthält `liminal-v13` nicht; Tags `v11`/`v12` unverändert. Kein Force-Push, kein Pull Request.
- **Release v13.3 „LIMINAL V13.3“** („Latest“): Paket `LIMINAL_V13.zip` (877 Dateien, 394,9 MB entpackt, 262 804 498 Byte)
  mit `LIMINAL.exe`, `liminal_net.dll`, `liminal_lobby.exe`, `koop.example.json`, `data`, `shaders`, `assets`,
  `LIESMICH.txt` (mit Koop-Kurzanleitung) und `licenses/` (LLVM, mingw-w64, Inter, libdatachannel, libjuice, usrsctp,
  plog, Mbed TLS, nlohmann/json, `THIRD_PARTY_NOTICES.txt`); gebaut außerhalb des Versionsordners. In 30 Teilen
  `LIMINAL_V13.zip.001`–`.030` (je 9 000 000 Byte, letzter 1 804 498) über den Browser angehängt, weil `gh` nicht angemeldet
  ist. SHA-256 `a1bd446d1f5a538796caad2b1f39e51a77308a56792177e4f025c6d99d388fa5`. Beschreibung mit Anleitung zum
  Zusammensetzen, Prüfsumme, Koop-Hinweisen samt ehrlichem Stand, Link zum Quelltext und Bau mit `tools\build.ps1`.
- **Kontrolle als Fremder:** API ohne Anmeldung: veröffentlicht, 30 Assets, Summe gleich der Paketgröße, `latest` = `v13.3`.
  Alle 30 Teile mit `curl` ohne Anmeldedaten geladen, mit `copy /b` zusammengesetzt: gleiche Prüfsumme. Das entpackte Paket
  startete mit eigenem Benutzerordner 60 s ohne ERROR (Hauptmenü, Texturen geladen).
- **Bau-Test aus frischem Klon** (`git -c credential.helper= clone --branch liminal-v13 --single-branch`, 59 s, 2,8 GB):
  `tools\build.ps1` rc 0 in 122 s (portable Werkzeugkette selbst geladen: llvm-mingw, CMake, Ninja), `LiminalGame.exe`, `liminal_net.dll` und `liminal_lobby.exe` gebaut (Quellen aus `third_party`, Lobby aus `tools/lobby`); gameplay_test im Klon 1020/0; Start 150 s ohne ERROR (einzige Warnung: Audiogerät des Rechners stumm geschaltet).
- **README auf `main`** (`072de77`): Bildleiste mit den 16 Bildern aus `docs/readme_v13/` (1920×1080, je unter 300 KB;
  Ordner `docs/readme/` jetzt 25 Bilder, 5,0 MB), Funktionen V13.3, Download-Zeile Release v13.3, Abschnitt „Co-op“
  (LAN, Lobby, `koop.json`-Felder, ehrlicher Stand), Quelltext- und Bauzeilen V13, Versionszeile V13, ältere Bilder
  V12/V11 als eigener Abschnitt. Auf GitHub geprüft: 25 Bilder, keines kaputt. `LICENSES.md` (Netzbibliotheken) und
  `ATTRIBUTION.md` (Wesen-Seiten V13) auch auf `main`.
- **Repository-Seite:** Beschreibung auf „… with co-op for up to four players … V1–V13 here, V13.3 latest.“ geändert
  (erlaubt: Beschreibung). Keine Einstellungen, Regeln, Sichtbarkeit oder Webhooks angefasst; offene Pull Requests: 0.

## Offen

- Lizenz des Spiels: wählt der Nutzer (CC BY-SA 3.0 oder GPLv3); dann in einem Zug LICENSE-Datei, README,
  `LICENSES.md` und Release-Beschreibungen.
- V13: erledigt (Abschnitt „V13-Upload“).
