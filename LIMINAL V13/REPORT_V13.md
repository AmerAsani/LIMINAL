# LIMINAL V13 – Abschlussbericht

Stand 26.10.2026, Branch `liminal-v13`. Auftrag: „LIMINAL – V13: Finger-Fix, Kreaturen-Neubau, Texturen, Bugfixing, Qualitätsupdate, Multiplayer (nur wenn nachweislich sicher)“.
Dieser Bericht trennt, was fertig und geprüft ist, von dem, was offen ist. Was nur angedeutet ist, steht nicht als fertig da.

---

## 1. Ergebnis in Kürze

- **Hand und Taschenlampe (Teil 1):** neue linke Hand mit Skelett (21 Knochen), gelöstem Griff um den Schaft, Daumen auf dem Schalter; Schaltfolge mit Daumendruck; Lampe und Lichtkegel immer deckungsgleich. Vier Kritikrunden (`HAND_ANALYSE.md`).
- **Kreaturen (Teil 2):** alle sechs Wesen – Hound, Smiler, Clump, Gestalt, Deathmoth, Nest – neu in Blender gebaut, mit dem Skelett des Spiels, je 4–7 Kritikrunden, Vorher/Nachher aus denselben Kameras (Übersicht `docs/screens/creatures/vorher_nachher_alle.jpg`) (`KREATUREN_ANALYSE.md`, `KREATUR_<Name>.md`).
- **Texturen (Teil 3):** Audit aller zehn Ebenen, Kachelbruch im Shader für Texturen ohne Raster, Leistung und Grafikspeicher gemessen (`TEXTUR_AUDIT.md`).
- **Qualität und Lore (Teil 4):** alle Ebenen aus festen Kameras mit und ohne Lampe, Wesen in ihren Ebenen, Karten neu erzeugt und geprüft, Abgleich mit den aktuellen Wikidot-Seiten (`QUALITAET_LEVEL.md`).
- **Fehlersuche (Teil 5):** Smoke-Test aller Ebenen und Menüs, Speichern/Laden, fünf Auflösungen, Bildzeiten, Speicher (`BUGLISTE.md`). Zwei Fehler behoben (B-003 Sicherheit, B-004 Protokoll), zwei offen und gering (B-001, B-002).
- **Mehrspieler (Teil 6):** bleibt aus und ist nicht erreichbar; Bedrohungsmodell, Entscheidung und geprüfte Protokoll-Prüfschicht (`MULTIPLAYER_*.md`). Kein Netzwerkcode im Spiel.

## 2. Abschlusskriterien

| Kriterium des Auftrags | Stand | Beleg |
|---|---|---|
| **Arbeitsweise:** keine Rückfragen, Entscheidungen dokumentiert | erfüllt | `DECISIONS.md` E-019 bis E-024 |
| V12 und V10 unverändert, Arbeit in eigenem Branch/Ordner | **erfüllt** | V12: Einfrier-Manifest geprüft – 25 550 Dateien, 0 verändert, 0 fehlend, 0 neu. V10: `git status -- "LIMINAL V10"` ohne Änderung. Branch `liminal-v13`, Ordner `LIMINAL V13` |
| Commit je Teilschritt, `PROGRESS.md` | erfüllt | Commits in Abschnitt 3, `PROGRESS.md` |
| Lizenzen externer Assets in `LICENSES.md`; Wiki nur in eigenen Worten; keine Wiki-Bilder, keine Texturen aus anderen Backrooms-Spielen | erfüllt | V13 fügt kein fremdes Material hinzu (`LICENSES.md`, Einleitung und Abschnitte 3 und 9); Briefings in eigenen Worten mit Quelle und CC BY-SA 3.0 |
| **Teil 1:** `HAND_ANALYSE.md` mit Nahaufnahmen; Hand mit 5 Fingern à 3 Gelenken, natürlicher Zylindergriff ohne Spreizen, Verdrehen, Eindringen; alle Zustände; Hand und Lampe als Einheit; mindestens 4 Runden aus Spieler- und Debug-Kamera; Ansatzwechsel | erfüllt | `HAND_ANALYSE.md` (Ursache, neuer Ansatz E-020, Runden 1–4), `docs/screens/viewmodel/v13_*`; Tests: Skelett mit mindestens 21 Gelenken, Posen, Geometrie < 2 mm |
| **Teil 2:** `KREATUREN_ANALYSE.md` (alter Pfad, Cache, Rückfall, Hashes, Laufzeitnachweis) | erfüllt | Abschnitte 1–2; Protokoll je Start: `Quelle Blender: ent_<x>_l0.glb (FNV …)` |
| Echter Neubau in Blender (Hintergrundmodus), Skelett, als Assets importiert | erfüllt | `tools/blender/creatures/`, `data/models/ent_*_l0..l2.glb` mit Skin |
| „Sculpt/Subdivision, Retopo, UVs“ | **anders gelöst** | Formen als Distanzfeld aus anatomischen Teilen (statt Pinsel-Sculpting ohne Sicht), Neuvernetzung mit OpenVDB und Reduktion (statt Retopo von Hand); keine UV-Abwicklung, weil die Wesen triplanar aus der Ruhepose texturiert werden – der UV-Kanal trägt Höhlung und Kante (E-021) |
| Eigene Animationen | **teilweise** | Die bestehende prozedurale Bewegung (Gang, Atmung, Gewichtsverlagerung, Kopf, Zustände) treibt die neuen Körper; im Gang geprüft. Neu geschrieben wurde sie nicht (Abschnitt 5) |
| `KREATUR_<Name>.md` je Wesen aus der Wiki-Seite in eigenen Worten | erfüllt | Hound, Smiler, Clump, Gestalt, Deathmoth (mit Nest) |
| Eigene Silhouetten, keine gemeinsame Grundanatomie, keine Comic-Gesichter, keine Blockformen, keine flachen Materialien | erfüllt | `KREATUREN_ANALYSE.md` Abschnitt 5; Smiler ausdrücklich gegen Comic-Wirkung überarbeitet (Runden 2–4) |
| Haut mit Durchscheinen, Feuchte, Rauheit, Detailnormalen | erfüllt | Wesen-Haut mit Durchscheinen (Shader), Höhlung und Kante je Punkt, nasse Varianten, Mikro-Ebenen der Wesen-Texturen |
| Vorher/Nachher aus denselben Kameras (nah, mittel, fern, Seite, Lampe, dunkel, bewegt); mindestens 4 Kritikrunden je Wesen | erfüllt | `docs/screens/creatures/vorher/`, `…/<wesen>/nachher_*`; Runden: Hound 6, Smiler 5, Clump 7, Gestalt 5, Motte 4, Nest 4 |
| **Teil 3:** `TEXTUR_AUDIT.md` je Ebene mit Bildern; PBR nur aus eigenen oder CC0/CC-BY-Quellen; Kachelbruch; Identität erhalten; Leistung und VRAM gemessen | erfüllt | `TEXTUR_AUDIT.md`, `docs/screens/texturen/`; E-022 |
| **Teil 4:** alle Ebenen mit festen Kameras, Lampe an/aus, Wesen; Validierung; Lore-Abgleich je Wiki-Seite | erfüllt | `QUALITAET_LEVEL.md`; Wikidot-Revisionen am 26.10. geprüft |
| **Teil 5:** Smoke-Tests (Ebenen, Spawn, Übergänge, Speichern/Laden, Menüs, Einstellungen, Inventar, Notizen, Lampe, Ausdauer, Wesen), Protokolle, Kollision, Audio, Speicher, Bildzeiten, Auflösungen; `BUGLISTE.md`, Abstürze zuerst | erfüllt, mit Einschränkung | `BUGLISTE.md` (Prüfungen); kein Absturz gefunden. Z-Fighting nur per Sichtprüfung der 142 Kamerabilder |
| **Teil 6:** Bedrohungsmodell zuerst; nur serverautoritativ, Client sendet nur Eingaben; Prüfung jeder Nachricht; Tests; einschalten nur wenn sauber, sonst aus und unerreichbar mit Entscheidung; Einzelspieler unbeeinträchtigt; nie „vollkommen sicher“ | erfüllt (Modus aus) | `MULTIPLAYER_BEDROHUNGSMODELL.md`, `MULTIPLAYER_ENTSCHEIDUNG.md`, `MULTIPLAYER_TESTS.md`; `tools/check_no_network.py`: kein Netzwerkcode im Spiel |
| **Abschluss:** `REPORT_V13.md` | erfüllt | dieses Dokument |

## 3. Änderungen (Changelog)

| Commit | Inhalt |
|---|---|
| `8062126` | Aufbau: Kopie von V12, Version 13 (eigene Einstellungen, Spielstände, Protokolle), V12 eingefroren |
| `eba084a`, `e3261a1`, `92cd25f` | Teil 1: Hand als Käfig-Modell mit Skelett, gelöster Griff, Skinning aus glTF, Posen `grip`/`press`, Schaltfolge mit Daumendruck, Haut mit Durchscheinen, Ärmel mit Bündchen, Debug-Kamera `vmcam`, `vmdebug` |
| `b1ad5a5` | Teil 2: Analyse, Briefings, Import der Wesen aus Blender (glb mit Skin), Skelett-Export `--dump-skeletons`, Quellennachweis im Protokoll |
| `137ca9e` | Hound und Smiler neu (Distanzfeld-Werkzeug `tools/blender/creatures/`), Vorführkamera `figcam`, Material `ent_darkskin` |
| `e72ba4d`, `3285333` | Teil 6: Bedrohungsmodell, Entscheidung, Prüfschicht `src/net` mit Tests; Starter prüft den Herausgeber des Microsoft-Pakets (B-003) |
| `fcfaede` | Teil 3: Kachelbruch im Shader, Textur-Audit; `perf` meldet Grafik- und Arbeitsspeicher; B-004 |
| `fffc771` | Clump, Gestalt, Deathmoth, Nest neu; Materialien `ent_silhouette`, `ent_resin`; Werkzeug: Formteil `tube`, Netze zusammenführen, Gewichtung der Stufen |
| Abschluss-Commit | Motte Runde 3, Vorher/Nachher aller Wesen, Qualitätsupdate (`QUALITAET_LEVEL.md`), Bugliste, Lizenzen, dieser Bericht |

**Neu im Spiel (sichtbar):**
- Linke Hand an der Lampe: Finger geschlossen um den Schaft, der Daumen drückt den Schalter, Licht erst danach.
- Sechs neue Wesen (Bilder in `docs/screens/creatures/`).
- Weniger sichtbare Wiederholung auf Beton, Teppich, Putz, Marmor, Terrazzo und Moos.

**Neu für Entwicklung und Prüfung:** Skriptbefehle `vmcam`, `vmdebug`, `figcam`, `figclear`; `--dump-skeletons`; `perf` mit VRAM und RAM; `tools/check_no_network.py`; Werkzeug für Wesen in Blender.

## 4. Geprüfte Funktionen

| Bereich | Wie | Ergebnis |
|---|---|---|
| Spiellogik | `gameplay_test` | **357 / 0** (Übergänge mit Gegenstücken, Halt aller Objekte, Vorräte, Wesen, Hand und Lampe) |
| Weltgenerierung | `worldgen_test` gegen V4-Referenz | **29 658 / 0** |
| Vorräte | `supply_balance_test` | **OK** |
| Klang | `audio_test` | **106 / 0** |
| Mehrspieler-Prüfschicht | `net_protocol_test`, auch unter ASan/UBSan | **72 / 0**, 1 000 000 Fuzz-Runden |
| Kein Netzwerkcode | `tools/check_no_network.py` | **SAUBER** |
| Smoke-Test im Spiel | Hauptmenü mit allen Einstellungsseiten; je Ebene Laden, Gehen, Rennen, Lampe, Inventar, Karte, Pause, Notizbuch, Speichern, Laden | alle 10 Ebenen bestanden, Werte nach dem Laden richtig, keine Warnung im Protokoll |
| Auflösungen | 1280 × 720 bis 3440 × 1440, 4:3 und 21:9 | alle 25 Bilder in Ordnung |
| Bildzeiten | `perf` je Ebene | GPU 1,8–7,6 ms bei 1600 × 900 (60-Hz-Begrenzung) |
| Speicher | `perf` | VRAM 935–1012 MB, RAM 470–620 MB, Spitze beim Laden 3,1 GB |
| Karten | `tools/levels/<ebene>.py` | 10 / 10 bestanden, identisch |
| Lore | Wikidot-Seiten am 26.10.2026 | Revisionen unverändert, Abweichungen in `QUALITAET_LEVEL.md` |
| Kreaturen im Spiel | Protokoll `Quelle Blender` je Wesen; Nah-, Fern-, Lampen-, Dunkel- und Gangaufnahmen | alle sechs aus Blender geladen |

## 5. Bekannte Probleme

| Nr. | Problem | Schwere |
|---|---|---|
| B-001 | Kapuzenjacke reißt am stark gebeugten Ellbogen (Schulteransicht von der Seite) | gering |
| B-002 | Arbeitsspeicher beim Laden kurz 3,1 GB (alle Texturebenen unkomprimiert im Speicher) | gering |
| – | Bewegung der Wesen nicht auf die neue Anatomie abgestimmt (prozedurale Bewegung aus V11/V12) | Qualität |
| – | Clump: Gliedlängen aus dem Skelett lassen ihn von weitem weiter wie ein Wesen auf vielen Beinen wirken | Qualität |
| – | Nest: Schichtung der Pfosten gleichmäßiger als gewachsene Masse | Qualität |
| – | Smiler: im hellen Licht ist der Hinterkopf rund; eine Rauchzunge liest sich aus manchen Winkeln kurz als Hand | Qualität |
| – | Buchrücken in Level 5.3 wiederholen sich aus der Nähe | Qualität |
| – | ORM-Texturen unkomprimiert (≈ 485 MB VRAM), Kompression würde ≈ 360 MB sparen | Leistung |
| – | Rechte Hand (Gesten) weiter die prozedurale Hand | Qualität |
| – | Wesen, die die Wiki-Seiten zusätzlich nennen (Skin-Stealer, Facelings, Wrangler u. a.), gibt es nicht | Umfang |

## 6. Offene Fragen

1. **Lizenz des Spiels:** Der Licensing Guide des Wikis verlangt für Spiele, die auf Wiki-Inhalten aufbauen, CC BY-SA 3.0 oder GPLv3 mit Quellcode. Die Entscheidung liegt beim Projekt (`LICENSES.md` Abschnitt 8) – unverändert offen seit V12.
2. **Mehrspieler:** Soll ein späterer Modus gebaut werden? Wenn ja: eigener Server oder ein Spieler als Gastgeber, LAN oder Internet, welche Bibliothek für die Verschlüsselung (Bedingungen in `MULTIPLAYER_ENTSCHEIDUNG.md`).
3. **Bewegung der Wesen:** Soll die nächste Version die Animationen auf die neuen Körper abstimmen (eigene Bewegungen in Blender oder weiter prozedural)?
4. **Rechte Hand:** dasselbe Handmodell gespiegelt für die Gesten?

## 7. Befehle

```
# Bauen (aus dem Ordner LIMINAL V13; kopiert auch data/ nach build/Release/bin)
export PATH="$PWD/.toolchain/llvm-mingw/bin:$PWD/.toolchain/cmake/bin:$PWD/.toolchain/ninja:$PATH"
cmake --build build/Release

# Tests (aus build/Release/bin)
../tests/gameplay_test.exe data
../tests/audio_test.exe
../tests/supply_balance_test.exe data
../tests/worldgen_test.exe data/legacy/levels/level0.json ../../../tests/data/v4_reference.json
../tests/net_protocol_test.exe

# Kein Netzwerkcode im Spiel
python tools/check_no_network.py

# Ein Wesen neu bauen (danach cmake --build, sonst lädt das Spiel die alte Fassung)
.toolchain/blender/blender-4.5.14-windows-x64/blender.exe -b --factory-startup --python tools/blender/creatures/build.py -- <hound|smiler|clump|gestalt|moth|nest> --preview <ordner>

# Hand neu bauen
.toolchain/blender/blender-4.5.14-windows-x64/blender.exe -b --factory-startup --python tools/blender/build_models.py -- vm_hand_l vm_flashlight vm_flashlight_on

# Karten neu erzeugen und prüfen
python tools/levels/<ebene>.py
```
