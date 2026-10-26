# Kreatur: Death Rat (Entity 24)

Quelle: [backrooms-wiki.wikidot.com/entity-24](https://backrooms-wiki.wikidot.com/entity-24), Revision 25, gelesen am 26.10.2026 (die Seite ist als „wird überarbeitet“ markiert); dazu die Ebenenseiten von Level 2, 3 und 5.
- Ein anonymer Fandom/Reddit-Nutzer (laut Seite).
- Lizenz CC BY-SA 3.0.
- Zusammenfassung in eigenen Worten; kein Bild der Seite verwendet.

## Was die Seite sagt

**Gestalt:**
- Wie eine gewöhnliche Hausratte, mit langem Körper und schwarzem Fell.
- Auf dem Kopf braune, hornartige Auswüchse mit rot glühenden Spitzen.
- Es gibt Arten, die anders aussehen – etwa eine haarlose auf Level 2, wohl an die Hitze angepasst.

**Verhalten:**
- Meist harmlos (manche sogar essbar).
- Fressen männliche Deathmoths und Schimmel; die meisten jagen und töten auch andere Wesen.
- Manche Arten sind gefährlich und stellen Fallen – „klüger, als man denkt“.
- Das „Screaming Event“: auf vielen Ebenen kein menschlicher Schrei, sondern Ratten, die über ein anderes Wesen herfallen; das Licht flackert, bis es vorbei ist.

**Ausweg (Seite):** meiden, nicht reizen – wie sie reagieren, ist unbekannt.

**Ebenen (Ebenenseiten):** Level 2 (fast harmlos, fressen Flüssigkeiten aus Fässern und Essen), Level 3 (wie Wasserschweine mit geschärften Zähnen, bauen Fallen, menschlich klug), Level 5 (ohne Verhalten; die Entity-Seite: fressen männliche Motten).

## Umsetzung

**Modelle** (`rat.py`, `rat_bald.py`, Skelett `rt`: Rumpf in drei Abschnitten, Kopf mit Kiefer, vier Beine zu je drei Gliedern, Schwanz in vier Gliedern):
- Entworfen in 0,52 m, im Spiel auf 62 % verkleinert (Rumpf und Kopf 32 cm, Schwanz 26 cm) – eine große Hausratte mit langem Körper.
- Schwarzes Fell (Strähnen längs), braune, nach hinten gebogene Hörner mit rot glühenden Spitzen, kleine dunkle Augen, Schnurrhaare, nackter, geringelter Schwanz, rosa-graue Pfoten.
- Haarlos (Level 2): Haut mit Falten statt Fell.

**Bewegung** (`updateRat`): Trab in diagonalen Paaren; schnell Galopp in Sprüngen (der Rücken beugt und streckt sich); im Stand schneller Atem, die Nase schnüffelt in Stößen; Wittern: auf den Hinterbeinen aufgerichtet, Vorderpfoten vor der Brust; Fressen: Kopf am Boden, kauend; Beißen: Stoß nach vorn, Maul weit auf; im Licht geduckt, Kopf weg; der Schwanz hängt mit Trägheit am Becken und schleift.

**Verhalten** (`thinkRat`, Level-JSON `presence.rat`: `mode` bald, `chance` = Schrei-Ereignis):
- Gruppen von 2–4 (Level 3: 2–3) huschen umher, schnüffeln, fressen.
- Kommt man näher als 3 m (sprintend 6 m) oder leuchtet sie an: sie quieken und stieben davon, und verschwinden außer Sicht in eine Ritze.
- Bedrängt (Licht auf ihr unter 4 m, sehr nah) beißt eine zu: 6 Health.
- Level 5: das Schrei-Ereignis – alle 160–320 s fallen irgendwo außer Sicht (9–24 m) Ratten über ein Wesen her: Kreischen (`rat_scream`), das Licht dort flackert 3–5 s.

**Klang:** Quieken (`rat_squeak_0..3`, 2,6–6 kHz), Schrei-Ereignis (viele Stimmen, Rangeln, gedämpft hinter Wänden).

**Eigene Auslegung:** davonstieben und das Beißen, wenn man sie bedrängt (die Seite: Reaktion unbekannt); das Schrei-Ereignis außer Sicht; Größe.

**Nicht umgesetzt:** die Fallen und die menschliche Klugheit der Level-3-Ratten (ein eigenes Fallensystem); die Größe „wie Wasserschweine“ in Level 3 (ein Skelett für alle); das Fressen aus Fässern; Ratten als Nahrung.

## Kritikrunden

Bilder: `docs/screens/deathrat/`.

| Runde | Befund | Änderung |
|---|---|---|
| Blender | Gut lesbar als Ratte mit Hörnern; mit 0,52 m Rumpf aber zu groß für „gewöhnliche Hausratte“. | Auf 62 % (Skelett `kRatScale`, Modell `SCALE`). |
| 1 (`r1_bewegung_zustaende.jpg`) | Trab, Aufrichten, Fressen, Beißen lesbar; im Galopp schlug der Schwanz senkrecht hoch. | Schwanz steifer (15 statt 6), weniger Schwerkraft. |
| 2 (`r2_schwanz.jpg`, `r2_level2_haarlos.jpg`) | Schwanz bleibt unten. Level 2: kleine, rosige, haarlose Ratten im Gang, fliehen vor dem Spieler. | – |
| 3 (`r3_level5.jpg`) | Level 5: Ratten im Hotelflur, die Lampe treibt sie auseinander. Abgenommen. | – |

## Tests (`gameplay_test`)

Level 2: Ratten erscheinen, haarlos; nah: sie quiekt und stiebt davon. Level 5: Schrei-Ereignis mit Flackern.
