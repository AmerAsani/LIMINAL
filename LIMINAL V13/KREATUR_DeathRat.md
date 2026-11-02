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

## V13.3: Level 3 – groß wie ein Wasserschwein, Stromfallen, menschlich klug

**Quellen** (Wikidot, CC BY-SA 3.0, eigene Worte, keine Bilder, keine Zitate):
- [level-3](https://backrooms-wiki.wikidot.com/level-3), Rev. 18 (26.09.2026; u/M654z, Neufassung Natedagreat563), Abschnitt der klugen Wesen/Death Rats.
- [entity-24](https://backrooms-wiki.wikidot.com/entity-24), Rev. 25 (01.06.2025), als „wird neu geschrieben“ markiert.

**Was die Seiten sagen:**
- Level 3: Größe und Aussehen eines Wasserschweins; die Zähne künstlich geschärft – sie wetzen sie an den unzerstörbaren Gitterstäben. Sie bauen die ausgefeiltesten Fallen, die man dort kennt, immer mit primitiver Elektrik, und bauen (schlechter) Bomben, Drohnen und Gasgranaten nach; die Fallen wirken nicht immer, sind aber eine ernste Gefahr. Gefangene zeigen menschliche Klugheit; sie sprechen nicht, lesen und schreiben aber Englisch und reden über Text-zu-Sprache-Geräte. Bei Überfällen führen sie meist die Brigaden an. Körperlich schwach, wegen der Klugheit aber mit am gefährlichsten.
- Entity 24: schwarzes Fell, braune Hörner mit rot glühenden Spitzen (häufigste Form).
- **Nicht gesagt:** wie eine Falle gebaut ist (Auslöser, Draht, Wirkung); wie sie jagen; Licht und Lärm; Rückzug.

**Umsetzung (vorher „teilweise“: Hausratten, Fallen und Klugheit weggelassen)** – Varianten der Rolle Rat, keine neue Rolle:
- **Modell** `tools/blender/creatures/rat_l3.py` → `ent_rat_l3_l0..l2.glb` (Variante 2, Rattenskelett; L0 46 000 Dreiecke), eigener Entwurf, keine skalierte Hausratte: Fassleib mit hohem, rundem Hinterteil und schweren Keulen, Schulterbuckel, kein Schwanz; stumpfer Wasserschweinkopf (tiefe Schnauze, dunkles Nasenpolster oben, Augen und kleine Ohren hoch am Schädel). Grobes, verfilztes schwarzes Fell mit 340 abstehenden Grannenbüscheln (der Umriss franst aus), eine Wirbelreihe drückt durch. An den Flanken lange Hautspalten mit dunklem Schorf (zu schnell gewachsen – Auslegung). Vernarbt zurückgezogene Lippen: rotes Zahnfleisch und die **orangen Nagezähne liegen frei**, die Spitzen zu Meißeln geschliffen (helle Schlifffacetten, Schleifriefen). Zwei dicke, nach hinten gebogene Hörner mit Wachstumsringen, rot glühende Spitzen. Vorn **lange, nackte, graue Finger** mit dunklen Nägeln (sie bauen Fallen – Auslegung), hinten drei Hufzehen. **Links am Hals das Sprachgerät:** Blechkasten, Messinggitter des Lautsprechers, Drehknopf, rote Leuchte, zwei Kabelbinder, ein Kabel ins Fell. Materialien: Rattenfell, dunkle/graue Haut, Schorf (glänzend), Zahnfleisch, Zahnschmelz (Harz, glänzend) und Schliff (Zahn), Horn, Glut, Stahl, Messing, Gummi.
- **Falle** `tools/blender/creatures/rat_trap.py` → `ent_rat_trap_l0..l2.glb` (Variante 3): an der linken Wand auf einem Brett eine Motorradbatterie mit roter Polkappe, eine von Hand gewickelte Zündspule auf einer Holzspule, eine rostige Konservendose als Kondensator, ein Federkontakt (Unterbrecher wie beim Weidezaun), rote Leuchte auf verdrilltem Draht, Kabel mit Isolierband, abgenagte Litzen (Zahnspuren); an beiden Wänden Nägel mit Porzellanisolatoren; dazwischen **zwei verdrillte blanke Kupferlitzen in 18 cm Höhe** (fertiges Netz an den Schwanzknochen, das Spiel dehnt sie auf die Gangbreite und lässt sie 1–2 cm durchhängen). Funken (eigene Netze) am Unterbrecher und an beiden Isolatoren.
- **Animation** (`src/anim/creature_rat_l3.cpp`, Variante über `extra`): der Leib um 3,5 vergrößert (gut 1,2 m lang, Schulter ~0,6 m) und um 0,15 m gehoben, die Beine zusätzlich um 1,75 gestreckt (Maßstab je Knochen – das Modell hat dünne, kurze Beine). Schwerer Passgang mit seitlichem Rollen von Becken und Rücken, im Lauf rollender Galopp; langsam drehender, schwerer Kopf. Haltungen: **lauern** (tiefer, Kopf vor), **sitzen** auf den Keulen wie ein Wasserschwein (Vorderbeine gestreckt), **wetzen** (schnelles Mahlen), **drohen** (Brust aufgerichtet, Finger vor, Kopf hoch, Maul offen), **beißen** (Kopfstoß zum Ziel, Maul weit), Licht (wendet den Kopf ab). Spricht das Gerät, bleibt der Mund zu – nur der Hals zuckt. Falle: Wurzel quer zum Gang, Isolatoren an den Wänden (rechts um 180° gedreht), Draht mit Durchhang, der beim Brummen zittert; gerissen hängen beide Hälften zu Boden; die Funken werden über den Maßstab eingeblendet.
- **Verhalten** (`src/gameplay/presence_rat_l3.cpp`; Level-JSON `presence.rat.mode = "l3"`, Gruppe 2–3, `every 200`, `count 3` = höchstens drei Fallen; kein Schrei-Ereignis): eine **Brigade**, je Ratte ein Auftrag:
  - **wetzt** (Auftritt außer Sicht, 14–30 m, bevorzugt hinter Gittern): sitzt und wetzt die Zähne – das Kreischen von Zahn auf Stahl hört man bis 34 m, bevor man sie sieht. Beim Auftritt steht meist schon **eine Falle** vor dem Spieler (außer Sicht, auf seinem Weg).
  - bemerkt ihn (Sicht 16 m, mit Lampe 26 m; Rennen 22 m, Lärm 12 m): das **Sprachgerät spricht** (beim ersten Mal „WE SEE YOU.“, bei späteren Brigaden „YOU CAME BACK.“ / „WE REMEMBER YOU.“).
  - **lauert außerhalb der Lampe:** hält Abstand = Reichweite der Lampe (an 7,5 m, aus 3,5 m) + 1 m + was sie gelernt hat; bevorzugt dunkle Stellen außerhalb des Strahls, mit Sicht auf ihn; im Lichtkegel tritt sie **seitlich aus dem Strahl** (keine Flucht) und sagt „TURN OFF YOUR LIGHT.“; sonst „KEEP WALKING.“, „DO NOT RUN.“; setzt sich ab und zu hin.
  - **baut Fallen:** eine Ratte (nicht die Anführerin) geht ungesehen zu einer Gangstelle auf dem Weg des Spielers (vor ihm oder dort, wo er schon war – sie merken sich seinen Weg), hockt 8 s an der Wand (Wetzen, Draht, Klicken, Funken) und hinterlässt eine Falle; kommt er näher als 6 m und sieht sie, bricht sie ab.
  - **treibt zur Falle:** liegt eine scharfe Falle bis 16 m vor ihm, kommen sie von hinten (er zwischen Ratten und Falle), 3,8 m Abstand, links/Mitte/rechts gestaffelt, Zähneklappern, „KEEP WALKING.“ / „THIS WAY IS SAFE.“.
  - **droht** (näher als 2,2 m): richtet sich auf, Zähne frei, **Zischen und „STOP.“ – 1,1 s** –, dann **beißt** sie, wer noch unter 1,55 m steht (14 Health × Schwierigkeit); danach **weicht** sie 4,5 s ins Dunkel zurück (körperlich schwach) und lauert wieder.
  - **stürzt sich auf die Beute:** hängt der Spieler in einer Falle, rennen alle in 25 m hin („NOW.“), drohen kürzer (0,65 s, hörbar) und beißen, solange er gelähmt ist.
  - **Gedächtnis** (je Ebene, über Brigaden hinweg): 24 Wegpunkte des Spielers, wie oft ihn die Lampe getroffen hat (+0,6 m Abstand je Mal, bis +3 m), Fallen, über die er gesprungen ist (die nächste hinter eine Ecke; „CLEVER.“), Schläge, Beil (+1 m Abstand). Abzug nach 3–4 min oder weit weg ungesehen; die Fallen bleiben.
- **Falle** (Variante 3, `thinkRatTrap`): **Ankündigung** alle 2,5–4 s ein Knistern mit Funken (hörbar bis 14 m, beim ersten Mal der Hinweis), Brummen im Draht, die rote Leuchte blinkt. **Wer mit den Füßen darunter durchgeht** (nicht im Sprung darüber): Schlag 10 Health × Schwierigkeit, **Muskelkrampf** (Betäubung 0,85: langsam, kein Sprint, klingt nach 6 s ab), die Lichter im Raum flackern, die Ratten hören es. **Jede vierte versagt** (es zischt, der Draht reißt) – die Seite: nicht immer wirksam. Danach liegt sie zerrissen da, die Leuchte aus. Ausweg: darüber springen (die Ratten merken es sich), umkehren.
- **Klang** (`src/audio/synth_rat_l3.cpp`, alles synthetisch): das **Sprachgerät** – Relais klickt, Einschaltrauschen, dann eine Maschinenstimme (der Formant-Synthesizer aus `synth_kin_l3.cpp`, `kinSpeakMachine`: Grundton fest 94 Hz, Silben gleich lang, ohne Atem) durch einen winzigen Lautsprecher (Bandpass 380–3300 Hz, Membranresonanz, 8-kHz-Abtastung mit 6 Bit, Klirren, 50-Hz-Brumm), am Ende wieder das Relais (`rat_tts_0..11`, Untertitel auf Englisch). Wetzen an Stahl (Haft-Gleit-Kreischen mit Rohrresonanz, `rat_grind`), Zähneklappern, Zischen mit Knurren (Drohung), Biss, schwere gepolsterte Schritte mit Krallenklicks (`rat_step_big`), Fallenbau (Draht verdrillen, Klicken, Funke); Falle: Knistern mit anschwellendem 100-Hz-Brumm (`trap_crackle`), Schlag (Knall, Lichtbogen voller Obertöne, Knistern, Plopp der Batteriesicherung, `trap_zap`), Versagen (Spucken, Zischen, Draht reißt und sirrt).
- **Mehrspieler:** der Host rechnet (Brigade, Gedächtnis, Fallen, Auslösen für den jeweils nächsten Spieler); die Puppe beim Gast bekommt Rolle, Variante, Lage und die vorhandenen Kanäle (`crouch`, `plea`, `jaw`, `strike`, `reach`, `talk`, `light`, `perceive`; die Falle `hang` = gerissen, `strike` = Funke, `shiver` = Brummen) – die Isolatoren rechnet die Puppe aus der Karte nach. Klänge tragen Klangnummern (`gameplay/kin_l3.hpp`, `KC_RAT_*`, `KC_TRAP_*`), für die Gäste wird daraus das vorhandene Klang-Ereignis mit Namen (Untertitel und Hinweis hängen am Namen). Protokoll unverändert.

**Interpretation:** Bau und Wirkung der Falle (Weidezaun-Prinzip, Draht in 18 cm, Krampf als Betäubung, jede vierte versagt); das Bauen auf dem Weg des Spielers, das Treiben, das Lauern außerhalb der Lampe, das Gedächtnis; die Sätze des Geräts; die Hautspalten, die menschenähnlichen Finger, Lage und Bau des Geräts; dass sie die Gruppe „führen“, ist als Arbeitsteilung (Anführerin treibt, andere bauen) umgesetzt. Bomben, Drohnen und Gasgranaten sind nicht umgesetzt (Restmangel).

| Wert | Zahl |
|---|---|
| Auftritt Level 3 | Brigade 2–3, ~200 s (0,7–1,3 ×), eine zur Zeit, außer Sicht 14–30 m Weg, 60 % hinter Gittern |
| Größe | ~1,2 m lang, Schulter ~0,6 m (Leib × 3,5, Beine × 6,1 gegenüber der Hausratte) |
| Sinne | sieht 16 m (Lampe 26 m), hört Rennen 22 m, Lärm 12 m |
| Lauern | Lampe an 8,5 m, aus 4,5 m; + 0,6 m je Treffer der Lampe (bis + 3 m), + 1 m mit Beil |
| Drohen / Biss | ab < 2,2 m; 1,1 s Ankündigung; Biss < 1,55 m, 14 Health; danach 4–6 s Pause, 4,5 s Rückzug |
| Fallen | höchstens 3; Bau 8 s; Draht 18 cm, Gang 0,8–3,2 m; Knistern alle 2,5–4 s (14 m); Schlag 10 Health + Betäubung 0,85; 25 % versagen |

**Wow-Check:** Aus dem Gitterraum kreischt Zahn auf Stahl; dann sagt eine flache Maschinenstimme „WE SEE YOU.“, und am Rand des Lampenlichts sitzt ein schwarzes, borstiges Ding von der Größe eines Wasserschweins, die orangen Meißelzähne frei – es geht nicht weg, es wartet; ein paar Meter weiter knistert ein Draht. Ehrlich: Wow für Klang vor Sicht, die Maschinenstimme und das geduldige Warten am Rand des Lichts; die Falle wirkt nur im Lampenlicht (der Draht ist eine dünne Linie), und aus nächster Nähe ist der Kopf noch etwas glatt.

### Kritikrunden (Level 3)

Bilder: `docs/screens/creatures/l3_ratten/` (je Runde ein Ordner, Bogen `*_sheet.png`).

| Runde | Befund | Änderung |
|---|---|---|
| 1 (`r0/rat_sheet.png`) | Eine Wurst mit rosa Kastenkopf: rundum glatter Fellmantel, rosa Kreisflecken wie bei einem Spielzeug, der kahle Kopf ein Würfel; viel zu niedlich. | Kopf neu (Schädel, Brauen, Backen), Fell auch auf dem Kopf, Hautrisse als schmale Spalten, graue statt rosa Haut, Grannenbüschel, Leib × 3,5. |
| 2 (`r1/rat_sheet.png`) | Die Silhouette eines Wasserschweins steht; aber das Fell wirkt wie Gummi, die Spalten wie aufgeklebte Würstchen (lachsfarben, stehen vor), das Gesicht flach wie ein Bildschirm, die Augen weiße Knöpfe, Sitzen wie Kauern. | Schnauze rund und tief, kräftigere Fellstruktur und 300 Grannen, Schorf dunkel und vertieft, dunkle Augen mit Glanzpunkt, längere Zähne, größere Hörner, Sitzen aufrechter. |
| 3 (`r2/rat_sheet.png`, `r2/ratlive_sheet.png`, `r2/falle_sheet.png`) | Jetzt ein Vieh: Borsten, Ziegenhörner, freie Zähne mit rotem Zahnfleisch, das Gerät am Hals lesbar. Im Spiel lauert sie zu tief (Beine verschwinden, ein schwebender Laib); die Nase eine große graue Scheibe („Schwein“). Beim Auftritt stand keine Falle (Stellensuche lehnte alles ab: Raum 120, gesehen 19); der erste Satz fiel aus (Sprechpause noch vom Auftritt). | Lauern weniger tief; Stellensuche 6–34 m, 320 Versuche, Gründe im Protokoll; der erste Satz sofort. |
| 4 (`r3/falle_sheet.png`, `r4/falle_sheet.png`) | Der Draht war unsichtbar (0,2 mm); nach dem Verdicken spannte sich ein riesiges leuchtendes Segel quer durch den Gang: die Funken beider Isolatoren lagen in Ruhe 6 cm auseinander in einer Gruppe – ihre Punkte mischten die Knochen der linken und der rechten Wand. | Jeder Funke eine eigene Gruppe (ein Knochen je Punkt). |
| 5 (`r5/falle_sheet.png`, `r4/rat_sheet.png`) | Der Draht ist eine dünne Linie, die Funken blieben dunkel (die Leuchte dimmte das ganze Leuchten auf 25 %); die Nase noch grau. | Litzen doppelt so dick, beim Funken volles Leuchten; dunkles Nasenpolster. |
| 6 (`r6/rat_sheet.png`, `r6/falle_sheet.png`, `r6/ratlive_sheet.png`) | Endstand: nah ein borstiges Wasserschwein mit Meißelzähnen und Gerät; im Gang mit Lampe ist die Ratte jenseits des Lichts, ohne Lampe hockt sie wenige Meter entfernt im Dunkel; der Kupferdraht liegt als Linie quer im Lampenlicht, die rote Leuchte an der Wand. | – |
| 7 Abnahme Level 3 (`docs/screens/level3/abnahme_vn_falle.jpg`, `wesen_r3`–`r7`) | Der blanke Draht ist im Spiel 1–2 Pixel breit und außerhalb des Lampenflecks unsichtbar – wer das Knistern überhört, läuft blind hinein (unfair). | Drei abisolierte Adern zum Seil verdrillt (im 2-m-Gang etwa 1 cm) und zwölf Fetzen der abgenagten Isolierung (weiß, rot), die am Draht hängen: im Lampenlicht eine Reihe heller Flecken quer durch den Gang, im Dunkeln aus 2,6 m noch Punkte unter der roten Leuchte (`rat_trap.py`, neu gebaut). Schaden, Auslösen und Versagen unverändert. |

### Tests (`gameplay_test`, „V13.3: Level 3 – Death Rats …, Deathmoth-Schwarm mit Praetorianern“)
Klangnummern mit Namen (auch für Gäste), Untertitel des Geräts, Hinweise; Schaden bleibt Schaden; Level-JSON (Level 3 `l3`, Level 2 unverändert). Brigade aus 2–3 großen Ratten (Variante 2), außer Sicht; Wetzen vor Sicht oder schon bemerkt; sie bemerken den Spieler, das Gerät spricht; **mit Lampe halten sie mehr Abstand als ohne**; Gedächtnis (Begegnungen, Wegpunkte); Drohen (Zischen, „STOP.“) vor jedem Biss. Falle: Draht quer im Gang in 18 cm (0,8–3,3 m); Knistern; wer durchgeht, bekommt Schaden und Betäubung (Schlag oder Versagen); wer darüber springt, bleibt heil und sie merken es sich; die Brigade baut selbst eine Falle; Koop: Puppen der Ratten und Fallen tragen ihre Variante. Die Level-2-Prüfungen der Ratten bleiben grün.
