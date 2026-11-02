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

**Nicht umgesetzt:** die Fallen und die menschliche Klugheit der Level-3-Ratten (ein eigenes Fallensystem); die Größe „wie Wasserschweine“ in Level 3 (ein Skelett für alle); Ratten als Nahrung. (Das Fressen aus Fässern ist seit V13.3 umgesetzt, siehe unten.)

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

## V13.3: Level 2 – an Fässern und Essensresten

Quellen: [Level 2](https://backrooms-wiki.wikidot.com/level-2) Rev. 93 (Abschnitt Entities/Death Rats, Fußnote zu den Fässern) und [Entity 24](https://backrooms-wiki.wikidot.com/entity-24) Rev. 25, beide CC BY-SA 3.0, eigene Worte.

**Was die Seite sagt:** Auf Level 2 praktisch keine Gefahr; die Ratten fressen die Flüssigkeiten in den Fässern (Wasser, Öl, ätzender Schleim, Almond Water und anderes – welche, sagt die Seite nicht) und zurückgelassene Lebensmittel; haarlos, wohl wegen der Hitze.

**Umsetzung (vorher „teilweise“, das Fressen fehlte):**
- **Karte** (`tools/levels/level2.py`, eigener Zufallsgenerator am Ende): Fassgruppen (`barrel@0..2`) und an manchen Essensreste mit einer Lache – Modelle `food_scraps` (aufgerissene Dose, ausgelaufene Bohnen, Folie) und `food_scraps@1` (angenagter Brotkanten mit Schimmel, Kekse, fettiges Papier) aus `tools/blender/models/models_l2_traces.py`.
- **Verhalten** (`src/gameplay/presence_rat_l2.cpp`, aus `thinkRat` aufgerufen, nur `mode "bald"`):
  - Auftritt der Gruppe an einer Futterstelle 8–20 m Weg vom Spieler, außer Sicht.
  - Jede Ratte sucht nach einer Pause die nächste Futterstelle im Umkreis von 9 m (nicht, solange man näher als 5 m ist), läuft hin (Wegefeld, das letzte Stück gerade), **klettert am Fass hinauf** (0,6 s im Bogen, aufgerichtet) und **trinkt am großen Spund** – die Schnauze über dem Spund, je Fassvariante gemessen: Kunststoffkappe, offenes Loch ohne Stopfen, heraussickerndes Öl – 8–20 s, Schlecken (`rat_lap_0..2`); oder **frisst an den Resten** (Nagen, `rat_gnaw_0..2`); ist der Deckel besetzt, schleckt sie die Lache am Fuß. Ab und zu hebt sie den Kopf und wittert.
  - Kommt man näher als 3 m (rennend 6 m) oder leuchtet sie an: vom Fass springt sie herunter (0,35 s), quiekt und stiebt davon – nur zu Zielen, deren Weg kaum länger als die Luftlinie ist und die weiter vom Spieler wegliegen; gibt es keines (Kammer mit nur einer Tür hinter dem Spieler), drückt sie sich in eine nahe Ecke. Außer Sicht verschwindet sie in eine Ritze. Bedrängt beißt sie wie sonst (6 Punkte).
- **Klang:** `rat_lap`, `rat_gnaw` als Ereignis `Snip` mit Rolle Rat (ohne Wirkung in der Sitzung, nur hörbar) – das Protokoll bleibt unverändert.
- **Mehrspieler:** Host rechnet; die Puppe bekommt Lage (auch die Höhe auf dem Deckel), Blick, `jaw`, `perceive` über NetState.
- **Interpretation:** welche Flüssigkeit sie trinken (das, was am Spund austritt), das Klettern, die Flucht in die Ecke.

**Wow-Check:** Im Lampenlicht sitzt eine nackte, rosige Ratte auf dem Fassdeckel, die Schnauze im offenen Spundloch, und schleckt – man hört das Schlecken, bevor man sie sieht; leuchtet man hin, springen sie quiekend herunter und drücken sich in die Ecken. Ehrlich: widerlich-lebendig, die Haut selbst bleibt etwas zu sauber (Restmangel).

### Kritikrunden (Level 2)

Bilder: `docs/screens/creatures/l2_ratte/`.

| Runde | Befund | Änderung |
|---|---|---|
| 1 (`r1_bogen.jpg`) | Ratten an den Fässern, eine auf dem Deckel (`r1_deckel.png`); die Ratte an den Resten fraß neben der Dose statt an den ausgelaufenen Bohnen (Fresspunkt in der Mitte des Modells). Auf `r1_flucht.png` läuft eine Ratte auf die Lampe zu – damals übersehen, erst in Runde 3 erkannt. | Fresspunkt dort, wo das Essbare liegt (Bohnen, Brotkanten), Annäherung bis 16 cm. |
| 2 (`r2_bogen.jpg`) | Sie fressen an Dose und Brot, eine trinkt auf dem Deckel (`r2_deckel.png`). | – (weiter mit Licht und Flucht) |
| 3 (`r3_bogen.jpg`) | In der Kammer mit nur einer Tür rannte eine aufgeschreckte Ratte am Spieler vorbei zur Tür (`r3_uebersicht_a..c.jpg`): das Fluchtziel lag hinter der Wand, der Weg dorthin durch die Tür. | Fluchtziele nur mit kurzem Weg (höchstens 1,6-fache Luftlinie) und weiter vom Spieler weg; sonst eine nahe Ecke seitlich. |
| 4 (`r4_bogen.jpg`) | Eine springt vom Fass (`r4_sprung.jpg`), die anderen huschen an der Wand entlang in die Ecken (`r4_ecke_links.jpg`, `r4_ecke_rechts.jpg`); niemand läuft mehr auf den Spieler zu. Restmangel aus der Nähe: auf dem Deckel saß sie mitten auf dem Deckel, nicht am Spund. | Schnauze über dem großen Spund (Lage aus `barrel@0..2.glb` gemessen), sie dreht sich oben zum Spund. |
| 5 (`r5_bogen.jpg`) | Die Schnauze steckt an der roten Kappe (`r5_spund_nah.jpg`), der Schwanz hängt über den Rand; am Boden die Nachbarin an der Dose. Versuch: fahlere, fleckige Hautfarbe – kaum ein Unterschied, verworfen. Abgenommen. | – |

### Tests (`gameplay_test`, „V13.3 Teil 4: Level 2“)

Fässer und Reste in der Karte; Ratten in Level 2; sie trinken und fressen (Laute), eine klettert auf den Deckel; dabei harmlos; kommt man, quieken und fliehen sie. Kammer mit nur einer Tür: auf dem Deckel die Schnauze am Spund; angeleuchtet quieken sie und fliehen nicht am Spieler vorbei zur Tür (mit der alten Fluchtregel schlägt die Prüfung fehl).
