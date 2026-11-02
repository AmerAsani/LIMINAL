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
| v12 „LIMINAL V12“ | **veröffentlicht** (02.11.2026, „Latest“) | `LIMINAL_V12.zip`, 157 MB, in 18 Teilen `LIMINAL_V12.zip.001`–`.018` (je 8,58 MB, letzter 4,07 MB); Gesamtdatei sha256 `08927be46a7c6e17e4a8f1ffd854faf96854f8077b9ca36eb6538f1e24f0b166` |

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

## Offen

- Lizenz des Spiels: wählt der Nutzer (CC BY-SA 3.0 oder GPLv3); dann in einem Zug LICENSE-Datei, README,
  `LICENSES.md` und Release-Beschreibungen.
- Nach Abschluss von V13.3: `liminal-v13` und Tag `v13.3` hochladen – Quelltext mit `third_party` (libdatachannel,
  libjuice, Mbed TLS, usrsctp) und `tools/lobby`, Lizenztexte dazu; Bau-Test aus frischem Klon; Release mit Link zum
  Quelltext und Bausatz; README mit V13-Bildern (12–16, je Ebene eines, Hand mit Taschenlampe, drei Wesen,
  Wasser/Spiegelung, Koop) und Koop-Anleitung (koop.json, Lobby) fertigstellen.
