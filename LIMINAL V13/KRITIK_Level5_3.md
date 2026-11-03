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
