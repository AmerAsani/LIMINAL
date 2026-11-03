# Kritikrunden Level 5.3 (Abnahmeschranke d)

Laufweg `python tools/qa/walkway.py level5_3 --round rN --rooms 15 --seed 3`, Bögen `docs/screens/level5_3/wegbilder_rN_*.jpg`;
Wesen ab Runde 2 `python tools/qa/creatureshots.py level5_3 --round rN`, Bögen `docs/screens/level5_3/wesen_rN_*.jpg`
(Einzelbilder `build/qa/creatures/level5_3/rN/`).
Grundlage: `LEVELSPEC_5.3.md` (Wikidot Level 5.3), `LOOK_5.3.md`, die Briefings `KREATUR_Gentleman.md`,
`KREATUR_Ambassadors.md`, `KREATUR_Macchina.md` (Abschnitt Level 5.3).

## Runde 1 (Struktur, Ausstattung V13.2)

Laufweg 202 m durch 20 Räume, 51 Bilder.

Befunde:
- **Eingangsflur und große Halle wie im Wiki:** Damast, Täfelung, Kerzenleuchter; Regale in drei Geschossen, Tuffsäulen, Kandelaber, eiserne Kronleuchter.
- **Regale in den Gängen schwarz** (022–050): Kandelaber standen nur an den Kreuzungen; die Buchrücken der Gänge waren nicht zu sehen. Das Wiki nennt nur einen älteren Gang im Dunkeln.

Änderung: In hellen Gängen steht alle 6 m ein Kandelaber, abwechselnd an beiden Seiten (`dress_aisles`). Die alten Gänge bleiben dunkel. Raummetrik weiter grün (0 von 173 Räumen verstellt).

## Runde 2 (Abnahme: Laufweg und alle Wesen, Basis 87560b2)

Laufweg `walkway.py level5_3 --round r2 --rooms 15 --seed 3`: 202 m durch 20 Räume, 51 Bilder (`wegbilder_r2_1.jpg`,
`_2.jpg`, `wegkarte_r2.png`). Die Ebene hat 173 Räume, der Weg erreicht 15 Räume schon nach etwa 120 m und geht bis 200 m
weiter (E-036 greift nicht). Wesen: neu die Tabelle für Level 5.3 in `tools/qa/creatureshots.py` (7 Läufe): der Hausherr fern,
mittel, nah im Licht der Ebene und mit Lampe (`gentleman`), Haltungen der Szene (`gentpose`: Verbeugung, Lesen, Wurf,
Ansprache), Wahrnehmen von hinten, Ankündigung, Griff (`gentnotice`), der Besuch, wie er im Spiel abläuft, mit Untertiteln
(`gentflow`), die Störung von hinten (`gentdisturb`), das Wissen der Bibliothek auf der Galerie mit und ohne Lampe (`lore`:
Folio der roten Lichter am Lesepult, Karteikarten auf dem Katalogschrank) und das Lesen (`loreread`). Bögen
`wesen_r2_1.jpg` … `_4.jpg`.

Befunde:
1. **Laufweg** nach LOOK_5.3: Eingangsgang mit Damast, Täfelung, Wandleuchtern und grüner Laterne (000–004); am Gangende die
   alte Tür im Halbdunkel (005); die große Halle mit drei Geschossen Regalen, Tuffsäulen, Kandelabern, Holztreppe und Wappen
   an der Decke (006–019); die Gänge mit Marmor an den Kreuzungen, Kandelaber alle 6 m (020–050), die alten Gänge dunkler.
   Auf dem Boden der Halle liegt ein Haufen herabgefallener Bücher (013, Library Mind) – gut. Keine elektrischen Leuchten,
   keine Notausgangsschilder, keine Personen. Keine Änderung.
2. **Hausherr, Frack** (gentleman 3–6, gentnotice 2): ohne Nadelstreifen (bekannte Schwäche); im Lampenlicht wird der Frack
   grau und glänzt wie Kunststoff (gentleman 6, gentnotice 2 von hinten: ein silbergrauer Overall). Die Revers sind aus dem
   Stoff des Fracks und verschwinden – von vorn ein schwarzer Sack mit einem runden Loch, in dem die Weste als leuchtend roter
   Ballon steht, oben ein weißes Ei (Hemd).
3. **Hausherr, Gesicht** (gentleman 5/6, gentnotice 4/5): gemalt ohne gemalte Schatten und ohne Wangenrot (bekannte Schwäche) –
   eine gleichmäßig gelbliche Fläche; das grobe Rissnetz macht sie aus 1,5 m zerknittert wie Papier, eher Mumie als
   Porzellanbildnis.
4. **Hausherr, Hand im Griff** (gentnotice 4/5): aus 38 cm rosig-orange wie Knetmasse; die Nägel weiße Kappen wie Verbände.
5. **Hausherr, Szene**: Verbeugung mit dem Arm zum Boden (gentpose 1), Ansprache mit halb geöffneten Armen, Kopf über die
   Schulter (gentnotice 1/2) – gut. Untertitel der Ansprache und der Stille kommen (gentflow 8/9), Zorn: Bücher am Regal
   (gentflow 11/12). Schlecht: aus 12 m (gentflow 4–10) ist er im Bild zu klein, um die Szene zu beurteilen; die
   Bücherhaufen des Zorns lesen sich als ein paar liegengelassene Bücher.
6. **Werkzeug**: Der Spieler stand beim Erscheinen am Start im Gang (0,4 m vor ihm): er bemerkte ihn sofort, die Störung war
   verbraucht, bevor er an seinem Platz stand (gentdisturb zeigt nur Umsehen). Das Folio ließ sich nicht lesen (Blick auf
   den Fuß des Pults), die Taste „Escape“ kennt das Skript nicht.
7. **Wissen der Bibliothek** (lore 1–5, loreread 3): Katalogschrank mit gezogener Schublade, Karteikarten und Text – gut;
   das Folio mit der roten Tafel am Lesepult – gut zu finden. LOOK_5.3 schließt Lesepulte aus („die Seite nennt keine“);
   Entscheidung siehe Runde 3.

Änderungen:
- **Nadelstreifen** im Shader (`mesh_gbuffer.hlsl` `pinstripes`, Material `kMatPinstripe`): Kreidestreifen 1,3 mm breit alle
  12,5 mm, senkrecht in der Ruhepose (an der Haut haftend), gewebt, über die Pixelbreite gemittelt (kein Flimmern aus der
  Ferne); Frack und Hose.
- **Gemaltes Gesicht** (`kMatFlagEntPaint`, `gentleman.py` `face_paint`, `build.py` Gruppe `paint`): je Punkt Rouge (zwei
  runde Flecken, zu deutlich wie bei einer Puppe), Umbra in den Augenhöhlen, neben der Nase, unter den Wangenknochen, unter
  Nase und Unterlippe, an Schläfen und Kinnlade, Bleiweiß auf Nasenrücken, Stirn, Kinn und Wangenknochen. Das grobe Rissnetz
  halb so hoch (die feinen Risse trägt die Mikroebene).
- **Frack**: Rauheit 1,0 → 1,6 (stumpf auch im Lampenlicht); **Revers aus schwarzer Seide** (eigenes Material
  `FM_ENT_GM_SILK`, glänzend), etwas breiter; Weste flacher und tiefer ochsenblutrot (0,20 → 0,15); Hemdbrust flacher.
- **Hand**: weniger Rot (wachsbleich), Nägel gelb wie Horn.
- **Zorn der Bibliothek** (`session_library.cpp`): vier Haufen entlang der Regalfront (ein großer, drei kleinere) statt zwei.
- **Werkzeug**: Spieler vor dem Erscheinen fort (50/100), Szene aus 6 m (59/101,5), Gang aus der Halle (63,5/101), Fund
  und Haufen am Ende; Folio mit Blick auf die Seite.
- Dabei gefunden (Restpunkt, nicht in dieser Abnahme): der glTF-Export schreibt v als 1 − v, das Spiel liest die zweite
  Texturkoordinate ungespiegelt – bei allen Wesen kommt die Kante invertiert an (fast überall 1: um ein Viertel heller,
  etwas glatter, echte Kanten dunkler). Alle Wesen sind mit diesem Stand abgestimmt; die Bemalung rechnet es ein
  (`build.py`). Vorschlag für DECISIONS.

## Runde 3 (`wesen_r3_1.jpg` … `_4.jpg`: der Hausherr nach den Änderungen, Besuch und Störung im Spiel; Laufweg unverändert)

Neu gebaut: `ent_gentleman_l0..l2` (Bemalung, Seidenrevers, Weste, Nägel), Shader, Materialien; Läufe `gentleman`,
`gentpose`, `gentnotice`, `gentflow`, `gentdisturb`, `loreread`.

Befunde:
1. **Nadelstreifen** (gentleman 3–6): da und senkrecht, auch an Hose und Ärmeln, sie haften beim Gehen am Stoff – aber zu
   hell und zu breit: aus 1,5 m ein Schlafanzug, mit Lampe ein hellgrauer Anzug mit weißen Streifen (gentleman 6,
   gentnotice 2).
2. **Gesicht** (gentleman 5, gentnotice 4/5, gentdisturb 3–5): jetzt ein bemaltes Puppengesicht – zwei runde Rouge-Flecken,
   Schatten in den Augenhöhlen und unter den Wangenknochen, helle Nase; im Griff von unten unheimlich. Das grobe Rissnetz
   lässt die Fläche noch wie Pappmaché wirken.
3. **Revers**: als glänzende Seidenstreifen neben der Weste zu sehen; oben stand nur die Spitze als schwarzer Punkt aus dem
   Frack – zwei Knöpfe auf der Brust.
4. **Weste**: dunkler und flacher, mit Goldknöpfen und Uhrkette eine Weste – unten noch ein Oval.
5. **Hand** (gentnotice 4/5): wachsbleich statt rosig; die Hornnägel sind im Lampenlicht schwarze Fingerhüte über den Kuppen.
6. **Besuch** aus 6 m (gentflow 3–13): Silhouette in der Tür des Gangs (14 s), Seufzer und Selbstgespräch mit Untertitel,
   Verbeugung, Lesen, Wurf, Ansprache, Stille, Abgang, Zorn mit Untertitel; das Notizbuch liegt 4,8 m hinter seinem Platz
   („Notizbuch aufheben“, gentflow 13); die vier Haufen lesen sich als Kaskade entlang der Regalfront (gentflow 14/15) – gut.
7. **Störung** (gentdisturb 1–9): Kopf über die Schulter („Ein Gast. Hier unten.“), Arm entfaltet sich („Erlauben Sie.“,
   Rat „Abstand!“), Griff („Vergessen Sie, was Sie hier gelesen haben.“), Revers („Wie Sie wünschen …“), Abgang, Zorn –
   vollständig, ohne Tod (Protokoll: bemerkt bei 2,8 m, Ankündigung 2,1 m, Griff 0,9 m; Anzeige danach Leben 97 – heilt
   nach –, Verstand 63).
8. **Folio**: wieder nicht gelesen – der Blick lag zwischen den Seiten, das Blatt liegt rechts (Werkzeug).
9. **Lesepult** (Befund 7 aus Runde 2): Entscheidung – das eine Pult bleibt als Ort des Wissens (E-047); `LOOK_5.3.md` nennt
   die Ausnahme (kein Lesetisch, kein zweites Pult).

Änderungen:
- Nadelstreifen 1,1 mm (statt 1,3), Kreide 0,15 (statt 0,23), Anteil 0,8; Frack-Grund 0,05 → 0,04.
- Gesicht: grobes Rissnetz noch einmal flacher (0,25/0,35 → 0,18/0,22 mm).
- Revers ohne die Spitze, oben weiter vorn und länger (bis an den Kragen).
- Weste vorn flach mit geradem Saum und zwei Spitzen (Kasten statt Ellipsoid), Knöpfe und Kette darauf.
- Nägel kleiner, auf dem Fingerrücken, die Kuppe frei; aus vergilbtem Elfenbein wie das Gesicht (ohne Bemalung).
- Werkzeug: Folio mit Blick auf das Blatt.
- Modellprüfung: die Stufen l1/l2 hatten nach dem Vereinfachen 33 bzw. 30 Dreiecke gegen den Umlaufsinn (Hemd, Hände,
  Gold, Schuppen) → `repair_glb.py`; danach 3/0.

## Runde 4 (`wesen_r4_1.jpg` … `_3.jpg`: Hausherr nah und in den Haltungen, Folio, neu: Bereiche und Übergänge)

Läufe `gentleman`, `gentpose`, `gentnotice`, `loreread` und neu `zones` (Sicherungspunkte abseits des Laufwegs: vor der
Leere, Ostflügel zur Tür nach Level 5, alte Gänge, Treppe, Obergeschoss, Kronleuchter und Wappen der Halle).

Befunde:
1. **Nadelstreifen** (gentleman 3–6): fein und dunkel, aus 1,5 m ein Nadelstreifenfrack; mit Lampe grau mit feinen Streifen
   (schwarzer Stoff im Taschenlampenlicht) – gut.
2. **Weste**: als Kasten mit geradem Saum stand sie wie ein roter Ranzen vor dem Frack, breiter als die Öffnung, und schien
   von hinten durch den Rücken (gentnotice 1/2) – schlechter als in Runde 3.
3. **Revers**: ohne Spitze lang und glänzend, hängen aber mit runder Kuppe oben wie zwei schwarze Zungen über der Weste.
4. **Nägel** (gentnotice 4/5): klein, aber hell wie Pflaster auf dem Fingerrücken.
5. **Gesicht**: flacheres Rissnetz – das Rouge, die Schatten und die hellen Stellen tragen jetzt das Bild; aus 1,5 m bleibt
   der Firnis körnig (Mikroebene der Craquelure und Flecken der Zahnschmelz-Ebene) – Restpunkt.
6. **Folio** (loreread 1/2): auch mit Blick auf das Blatt kein „Folio lesen“. Ursache (Spielfehler, nicht Werkzeug): der
   unsichtbare Block des Lesepults ist 1,25 m hoch, `findTarget` prüft die Sichtlinie bis 1,2 m über dem Boden – das
   Folio war im Spiel von keinem Platz aus zu lesen (die Karteikarten mit 1,05 m schon).
7. **Bereiche** (zones 1–6): vor der Leere das weiße Eisengitter vor schwarzem Raum (mit Lampe hell); der Ostflügel endet an
   der alten Tür nach Level 5 im Kerzenlicht; die alten Gänge dunkel (nur mit Lampe Regale); Treppe mit Marmor; Obergeschoss
   mit Durchgang zur Galerie; in der Halle der weiße eiserne Kronleuchter und die Wappen an der Decke – nach LOOK_5.3, keine
   Schilder, keine elektrischen Leuchten. Keine Änderung.
8. **Szene in Haltungen** (gentpose 1–5): Verbeugung, Lesen, Ansprache gut; der Wurf ist als Haltung nur ein Stehen (das
   Buch fliegt im Ablauf, gentflow).

Änderungen:
- Weste zurück zur Form aus Runde 3 (flaches Ellipsoid mit Knöpfen und Kette).
- Revers als flaches Band (Querschnitt 4–4,5 mm, oben am Kragen 4,4 cm, unten 6,8 cm breit) knapp über dem Frack.
- Nägel noch kleiner und dünn (1,4 mm, feineres Gitter), sie liegen fast in der Haut.
- **Lesepult**: Block 1,25 → 1,15 m (`tools/levels/level5_3.py`, Karte neu erzeugt; Vergleich der Karten: nur diese Zahl
  geändert). Neue Prüfung im `gameplay_test`: beide Funde vom Platz des Lesers aus erreichbar – schlug mit der alten Karte
  fehl („Folio lesen“, 1,13 m), mit der neuen grün.
- `gm_notebook.glb`: 38 entartete Dreiecke (28,8 %) → `repair_glb.py`.
