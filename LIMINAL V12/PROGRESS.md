# LIMINAL V12 – Fortschritt

Wiedereinstieg: diese Datei zuerst lesen, dann `DECISIONS.md`.

## Fertig
- [x] V11 eingefroren (Manifest), V10 geprüft (0 Unterschiede), V12 aus V11 kopiert, Branch `liminal-v12`, Version 12, Build frisch, Tests grün (gameplay 349, audio 106, worldgen 29 658, supply ok). Commit `7b5d36a`.
- [x] Blender 4.5.14 LTS portabel in `.toolchain/blender/` (Prüfsumme geprüft), läuft im Hintergrundmodus.
- [x] Schritt 0: Audit und Entscheidungen je System (`DECISIONS.md` E-002, E-003).

- [x] Schritt 1: Levelauswahl (`LEVELAUSWAHL.md`). Gewählt: 0, 1, 2, 3, 5, 5.3, 188, 903, 37, 36. Commit `3b58e34`.
- [x] Pipeline Texturen: 37 CC0-Sets (ambientCG), Lock mit Prüfsummen, 48 Ebenen im Spiel, `wall_tex`/`floor_tex`/`ceil_tex`. Commit `a595e4e`.
- [x] Pipeline Modelle: Blender → `.glb` → eigener Leser, Spiegelung y, Test (357/0).
- [x] Level 0 (Umgebung, Licht, Funde, Klang):
  - `LEVELSPEC_0.md`, `LOOK_0.md`.
  - Neue Karte: 326 Räume, Zickzack-Flur, Säulenfeld, Bögen, Lochfeld, Dunkelzone mit Wasser, rote Räume, Naht zu Level 1.
  - Neun Blender-Modelle: Rasterleuchte an, aus und rot, Luftauslass, Bögen 2/3/4 m, Steckdose, Lüftungsgitter.
  - Klang: `hum_surge`, Flüstern, Kratzen, Tropfen; im Spiel gemessen ×2,40.
  - Vier QA-Runden, Bilder in `docs/screens/level0/`.
  - Platzierung: 1035 Objekte, 0 ohne Halt.
  - Wesen fehlen noch (siehe unten).
- [x] Viewmodel: Hand im Kraftgriff und Taschenlampe als eigene Blender-Modelle (`DECISIONS.md` E-007).
  - Arm-IK passt zum Handmodell (Test 365/0).
  - Kein Durchstoßen am Ärmel und an der Wand (35 cm geprüft).
  - Bilder: Ruhe, Gehen, Laufen, Drehen, Blick unten/oben, Wand, Dunkel in `docs/screens/viewmodel/`.

- [x] Level 1 „Die Lagerdecks“:
  - `LEVELSPEC_1.md`, `LOOK_1.md`.
  - Neue Karte: Aquila, Gothic, Gild, Vorräume, Gang-Labyrinth, Rohrgang nach Level 2.
  - 30 Blender-Modelle; Wesen Hound, Clump, Smiler; Klang mit Rohren und Tropfen.
  - Drei QA-Runden, Bilder in `docs/screens/level1/`.
  - Platzierung: 1451 Objekte, 0 ohne Halt; Tests 357/0. Entscheidung E-009.

- [x] Level 2 „Die Versorgungsgänge“:
  - `LEVELSPEC_2.md`, `LOOK_2.md`.
  - Tunnelnetz mit Maschinen, Rohren und verschlossenen Türen; Röhrengruppen in Reihenschaltung, violette Notlichter.
  - Gleistunnel, Anlage, Packlager, Bürogang als Schleife, Leerraum, Tür nach Level 3.
  - 28 Blender-Modelle; drei QA-Runden, Bilder in `docs/screens/level2/`.
  - Platzierung 0 ohne Halt; Tests 354/0. Entscheidung E-010.

- [x] Level 3 „Das Umspannwerk“:
  - `LEVELSPEC_3.md`, `LOOK_3.md`.
  - Ziegelgänge, Gitter, Generatorräume, Fertigungsstraße, Kesselräume, Sanktuarium mit Engel, Paranoia-Gang, Labyrinth, Aufzug nach 5.
  - 22 Blender-Modelle, 2 neue CC0-Texturen; drei QA-Runden, Bilder in `docs/screens/level3/`.
  - Platzierung 0 ohne Halt. Entscheidung E-011.
- [x] Level 4, 6, 19 aus V12 entfernt (nicht in der Auswahl).

- [x] Level 5 „Das Hotel“:
  - `LEVELSPEC_5.md`, `LOOK_5.md`.
  - Aufzugshalle, Hauptgang mit Flügeln, 58 Zimmer, Beverly Room, Kesselraum, Gang nach 5.3, Fenster nach 188, Metro-Halt nach 903.
  - Eigene Damasttapete; 21 Blender-Modelle (Türen mit Nummern, Aufzüge, Kronleuchter, Mahjong-Tisch, Grammophon, Kessel, Ofen, Bett, Gesims, Laterne, Bildnis, Metro-Wagen, Fenster).
  - Funde nur auf Beistelltischen (9 von 9). Vier QA-Runden, Bilder in `docs/screens/level5/`.
  - Platzierung: 2253 Objekte, 0 ohne Halt. Engine: Lichtsuche nach Radius. Entscheidung E-012.

- [x] Level 5.3 „Promethei Bibliotheca“:
  - `LEVELSPEC_5.3.md`, `LOOK_5.3.md`.
  - Eingangsgang, große Halle mit drei Geschossen Regalen, zwei Flügel mit Gängen, Obergeschoss, alte Gänge, Leere hinter Gittern, Ausgang zurück nach Level 5.
  - Eigene Texturen Buchrücken und Tuffstein; 16 Blender-Modelle; Buchfall-Klang.
  - Funde auf Regalböden (6 von 6). Vier QA-Runden, Bilder in `docs/screens/level5_3/`.
  - Platzierung: 1480 Objekte, 0 ohne Halt. Entscheidung E-013.
  - Notizen für Level 5 und 5.3 ergänzt.

## Als Nächstes
1. Wesen-Pipeline (gestellte oder gerigte Modelle aus Blender); zuerst die dunkle Gestalt von Level 0.
2. Level der Reihe nach (1, 2, 3, 5, 5.3, 188, 903, 37, 36): Spec → Look → Bau → Platzierung → Items → Wesen → Klang → Qualitätsschleife → Commit.
3. Oberfläche neu, geprüft bei mehreren Auflösungen.

## Bekannte Fehler / offen
- **Reiseprüfung rot (gameplay_test, 4 Fehler):**
  - Level 5 zeigt auf 188 und 903, die noch nicht gebaut sind.
  - Level 7 zeigt auf das entfernte Level 6.
  - Die Prüfung „9 Ausgänge, 1 Ende“ erwartet das Ende noch in Level 37 statt am Tor von Level 36.
  - Wird mit 188, 903, 37 und 36 behoben.
- Einmalig hing in V11 ein Testlauf beim Start (Protokoll endete nach dem Laden der Level). In vier Wiederholungen nicht wieder aufgetreten. Ursache unbekannt; beobachten.
