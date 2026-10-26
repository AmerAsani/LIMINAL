# LIMINAL V12 – Bericht zum autonomen Neudesign (10 Level)

Stand: 26.10.2026, Branch `liminal-v12` (19 Commits seit `main`). Grundlage: der Auftrag „LIMINAL – Autonomes Neudesign: 10 Level, maximale Qualität“.
Dieser Bericht trennt, was fertig und geprüft ist, von dem, was fehlt oder vereinfacht wurde. Nichts hier ist als fertig gemeldet, was nur angedeutet ist.

---

## 1. Ergebnis in Kürze

- **Zehn Ebenen**, jede mit `LEVELSPEC_<Nr>.md` (Wiki-Fakten in eigenen Worten, Ergänzungen, Konflikte) und `LOOK_<Nr>.md` (Bildsprache).
  - Jede hat eine feste Karte aus einem Python-Skript, eigene Blender-Modelle, Klang, Funde auf Flächen, Wesen nach dem Wiki und eine Qualitätsschleife mit festen Kameras.
  - Die Bilder liegen in `docs/screens/<level>/`.
- **Die Reise:**
  - 0 → 1 → 2 → 3 → 5;
  - von 5 nach 5.3 (Gang) und 188 (Fenster), jeweils mit Rückweg;
  - von 5 mit dem Metro-Wagen nach 903;
  - 903 → 37 (mit Rückweg) und 903 → 36;
  - das Gate C33 in 36 ist das Ende (Abspann).
  - Jeder Übergang hat im Ziel sein Gegenstück; der Test prüft das.
- **Oberfläche neu** („Beschilderung“), geprüft bei fünf Auflösungen (`docs/screens/ui/`).
- **Dokumente:** `DECISIONS.md` (E-001–E-018), `PROGRESS.md`, `LICENSES.md`, `MULTIPLAYER.md`, dieser Bericht.

## 2. Abschlusskriterien

| Kriterium | Stand | Beleg |
|---|---|---|
| Alle 10 Level spielbar und optisch unterscheidbar | **erfüllt** – siehe Einschränkungen in Abschnitt 4 | `docs/screens/level0 … level36` (15–21 Bilder je Level) |
| Objektvalidierung grün | **erfüllt** | gameplay_test „Halt der Ausstattung“: alle 10 Karten, 0 Objekte ohne Halt (Level 5: 2253, 5.3: 1480, 903: 553 …); `mapkit.validate` beim Erzeugen jeder Karte ohne Fehler |
| Build läuft | **erfüllt** | `cmake --build build/Release`; Tests: gameplay **351/0**, audio **106/0**, worldgen (V4-Referenz) **29 658/0**, supply_balance **OK** |
| Keine Änderung an V10 | **erfüllt** | `git status -- "LIMINAL V10"`: keine Änderungen |
| V11 bleibt unverändert | **erfüllt** | Einfrier-Manifest: 18 687 Dateien geprüft (SHA-256), 0 verändert, 0 fehlend, 0 neu |
| Externe Assets mit Quelle, Autor, Lizenz | **erfüllt** | `LICENSES.md` (ambientCG CC0 mit Lock und Prüfsummen, Wikidot-Seiten mit Fassung und Autoren, Inter OFL, Blender); offene Rechtsfragen dort Abschnitt 8 |
| Kein Mehrspieler | **erfüllt** | `MULTIPLAYER.md`, kein Netzwerkcode |

## 3. Die Level

| Level | Im Spiel | Kern | QA-Runden | Entscheidung |
|---|---|---|---|---|
| 0 „Threshold“ | Die Vorzimmer | 326 Räume, Zickzack-Flur, Säulenfeld, Bögen, Lochfeld, Dunkelzone mit Wasser, rote Räume; Summen schwillt an; nur die Gestalt | 4 | E-006 |
| 1 „The Habitable Zone“ | Die Lagerdecks | Hallen Aquila/Gothic/Gild, Gänge mit Räumen, Rohrgang; Smiler, Hound, Clump, Stromausfall | 3 | E-009 |
| 2 „Abandoned Utility Halls“ | Die Versorgungsgänge | Tunnelnetz mit Maschinen, Röhren in Reihenschaltung, Gleistunnel, Tür „3“ | 3 | E-010 |
| 3 „Electrical Station“ | Das Umspannwerk | Ziegelgänge, Gitter, Sanktuarium mit Engel, Fertigungsstraße, Aufzug | 3 | E-011 |
| 5 „Terror Hotel“ | Das Hotel | Damast, Beverly Room mit Mahjong-Tisch, 58 Zimmer, Kesselraum mit Kesseln und Öfen, Metro-Halt; Deathmoths mit Nestern, seltene Hounds | 4 | E-012 |
| 5.3 „Promethei Bibliotheca“ | – | Halle mit drei Geschossen Regalen, Tuffsäulen, Kandelaber, Gänge in alle Richtungen, Leere hinter Gittern, alte Holztür zurück | 4 | E-013 |
| 188 „The Courtyard of Windows“ | Der Hof der Fenster | 30-m-Hof, 144 Fenster, Moos, Fünfeckweg, Birkenkreis, Umrisse in Fenstern, weißer Gang | 3 | E-014 |
| 903 „The Metro“ | Die Metro | 196-m-Station, ruhender Zug, fahrender Wagen mit Zielen (Bildzeichen), zugemauerte Treppen | 3 | E-015 |
| 37 „Sublimity“ | – | 338 geflutete Fliesenräume, Säulenhallen, Gruben mit Treppen ins Tiefe, Licht aus seltsamen Winkeln | 3 | E-016 |
| 36 „Timeless Airport“ | – | Gepäckband, Check-in, Rundgang mit Laufbändern, Sackgasse hinter EXIT, Ladenzeile, Gates C30–C34, Ende an C33 | 3 + Schlussaufnahme | E-017 |

**Bildzeiten** am Start jeder Ebene (1600 × 900, Qualität hoch; Entwicklungsrechner mit GeForce RTX 5070 Laptop):

| Level | GPU (ms) |
|---|---|
| 0 | 3,2 |
| 1 | 4,5 |
| 2 | 2,9 |
| 3 | 1,7 |
| 5 | 3,9 |
| 5.3 | 4,1 |
| 188 | 6,4 |
| 903 | 4,8 |
| 37 | 4,4 |
| 36 | 6,8 |

Die Spiellogik braucht unter 0,1 ms. Gemessen wurde ein Bild am Startpunkt, nicht über einen ganzen Lauf; auf schwächerer Grafik ist das nicht geprüft.

## 4. Was fehlt oder vereinfacht ist (ehrlich)

**Wesen**
- Die Wesen sind die prozeduralen Modelle aus V10/V11, keine neuen Blender-Modelle mit Skelett (E-008: zurückgestellt).
- **Fensterwesen (188):** nur Umrisse in Fenstern und die Gestalt aus Level 0; sie tötet nicht, obwohl das Wiki sie feindlich nennt.
- **Facelings (903), Skin-Stealer (188), Bewohner und Gruppen** (Hotel, Bibliothek, Metro, Flughafen): nicht dargestellt; es gibt keine Menschen im Spiel.
- **Library Mind (5.3):** nur als fallende Bücher (Klang) und als langsamer Verlust an Verstand; kein Handel mit Erinnerungen.

**Bau**
- **5.3:** Das Wiki nennt sieben Stockwerke; begehbar sind drei Höhen, weil die Engine je Kachel nur einen Boden kennt (technische Grenze).
- **903:** 100 Meilen Länge nicht darstellbar; der Wagen steht immer bereit (keine Wartezeit), die Fahrt selbst wird nicht gezeigt.
- **188:** Das Eingangsfenster verschwindet nicht (sonst gäbe es keinen Rückweg); der Raum verändert sich nicht.
- **36:** Ausschnitt eines Terminals statt 26; Laufbänder stehen (keine bewegten Böden in der Engine); Zeitdehnung nur über Uhren und Verstand.
- **37:** Durchsichtiges Wasser kann die Engine nicht; flaches Wasser ist deshalb helles Türkis.

**Oberfläche**
- Titel- und Pausenmenü, Ankunftstafel, Abspann, HUD-Beschriftungen, Hilfe und Tafeln sind neu gestaltet.
- Inventar, Karte, Notizblatt und Todesbildschirm haben nur das neue Thema (Farben, Kanten) bekommen, ihr Aufbau stammt aus V11.

**Prüfung**
- Ein vollständiger Durchlauf von Level 0 bis zum Gate wurde **nicht** am Stück gespielt.
- Geprüft sind:
  - alle Übergänge und ihre Gegenstücke (Test);
  - Kollision und Halt aller Objekte (Test);
  - Schwimmen und Ertrinken (Test in Level 37);
  - einzelne Wechsel per Skript (5 → 188, Ende in 36);
  - jeder Level mit festen Kameras.

**Recht**
- Die Lizenzfrage für das ganze Spiel (ShareAlike laut Licensing Guide des Wikis) ist **nicht** entschieden.
- Es gibt keine Markenrecherche; `OFL.txt` (Inter) fehlt noch im Paket. Siehe `LICENSES.md` Abschnitt 8.

## 5. Gefundene und behobene Fehler der Engine (Auswahl)

- **Lichtsuche je Kachel:** Sie sah nur 8-m-Felder. Große Leuchten brachen hart ab und machten Flecken (Ballsaal) → Suchweite nach Radius.
- **Treppenstufen** hatten keine Bodentextur → jetzt mit Textur (Holztreppen in 5.3).
- **Wasser:**
  - nur eine dunkle Fläche → Varianten (flach, tief);
  - in Durchgängen fehlte Wasser → jetzt auch dort.
- **Höhenfeld:** Unsichtbare Blöcke unter 1,2 m warfen Schatten → zählen als Boden.
- **Werkzeug:** `mapkit` erkennt hohe Blöcke als tragende Wand (frei stehende Regale). Viele Platzierungsfehler fing die Prüfung während des Baus ab (Regale auf Treppen, Objekte in Freihaltezonen vor Türen, Wandteile in Öffnungen).

## 6. Bauen und Starten

```
export PATH="$PWD/.toolchain/llvm-mingw/bin:$PWD/.toolchain/cmake/bin:$PWD/.toolchain/ninja:$PATH"
cmake --build build/Release
cd build/Release/bin
./LiminalGame.exe                       # Hauptmenü
./LiminalGame.exe --level level5_3      # direkt in eine Ebene
../tests/gameplay_test.exe data         # 351 Prüfungen
```

- Karten neu erzeugen: `python tools/levels/<level>.py`.
- Modelle neu bauen: `.toolchain/blender/blender-4.5.14-windows-x64/blender.exe -b --factory-startup --python tools/blender/build_models.py -- <namen>`.
- Texturen neu aufbereiten: `python tools/textures/process.py`.

## 7. Empfohlene nächste Schritte

1. Einen vollständigen Durchlauf 0 → 36 spielen und Laufwege, Funde und Verstand im Zusammenhang abstimmen.
2. Lizenzentscheidung (CC BY-SA 3.0 oder GPLv3 für das Spiel), `OFL.txt` ins Paket, Markenrecherche.
3. Wesen mit Skelett aus Blender (zuerst der Hound), danach Fensterwesen und Facelings.
4. Inventar, Karte und Notizblatt im Aufbau an die neue Oberfläche angleichen.
