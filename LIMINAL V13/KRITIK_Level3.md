# Kritikrunden Level 3 (Abnahmeschranke d)

Je Runde Laufweg (`python tools/qa/walkway.py level3 --round rN --rooms 18 --seed 3`), Bögen `docs/screens/level3/wegbilder_rN_*.jpg`.
Grundlage: `LEVELSPEC_3.md` (Wikidot Level 3).

## Runde 1 (Struktur nach dem Umbau, Ausstattung V13.2)

Laufweg 201 m durch 27 Räume, 51 Bilder. Befunde:
1. **Ziegelgänge wie im Wiki:** roter Hauptgang mit schwarzen Rohren an einer Wand, Verteilern und Engelsbildern; Netze
   in grauem, hellem und braunem Ziegel.
2. **Neue Gangnetze über lange Strecken unlesbar** (009–011, 014–015, 022–027, 032–034, 050): Röhren nur alle 5 m in
   engen Gängen, davon 25 % tot; das Wiki will „sehr dunkle Bereiche“, aber nicht ganze Netze ohne Licht.
3. **Graue Netze kahl** (020–046): ohne Rohre und Kästen auf langen Stücken.
Änderung: Röhren in allen Gängen alle 4 m, 20 % tot (dunkle Bereiche bleiben durch tote Gruppen und Kammern).
Punkt 3 wird mit der neuen Ausstattung (Agent Level 3) in Runde 2 geprüft.

## Runde 2 (mit der neuen Ausstattung: 24 Klassen, 38 Modelle)

Laufweg 201 m durch 27 Räume, 51 Bilder (`wegbilder_r2_*`). Befunde:
1. **Gänge lesen sich als Level 3:** schwarze Rohre mit Ventilen und Flanschen an einer Wand, Kabelleitern unter der Decke, Engelsbilder, Verteiler, Gitterfenster. Die grauen Netze sind seit Runde 1 hell genug (020–035).
2. **Zwei dunkle Strecken im roten Hauptgang** (009–011, 014–015): tote Röhren, im Wiki ausdrücklich „sehr dunkle Bereiche“. Bleibt so.
3. **Aufzug nach Level 5:** Das Aufzugstor ist für 1,4 m gebaut, die Öffnung war 1 m; die Laibungen steckten in der Wand (Restmangel aus `PROPS_Level3.md`).

Änderung: Die Öffnung ist jetzt 2 m breit, das Tor auf 1,35 skaliert wie in Level 5.

## Runde 3 (Abnahme: Laufweg und alle Wesen, Basis d1954b8)

Laufweg `python tools/qa/walkway.py level3 --round r3 --rooms 15 --seed 3`: 201 m durch 27 Räume, 51 Bilder
(`wegbilder_r3_1.jpg`, `_2.jpg`, `wegkarte_r3.png`). Wesen: `tools/qa/creatureshots.py level3 --round r3` – neu die
Tabelle für Level 3 (13 Läufe: großes und kleines Bündel, Rudel, Faceling, Skin-Stealer, Wretch, Death Rats mit Falle,
Schwarm, Praetorianer, Smiler, Red Knight, purpurne Wesen), je im Licht der Ebene und mit Lampe; Bögen
`wesen_r3_1.jpg` … `_5.jpg`.

Befunde:
1. **Großes, bewaffnetes Bündel** (`wesen_r3_1`, bigclump 1–7): an der Decke im Halbdunkel stark, aber im hellen Gang
   vor dem Gitter und nah im Lampenlicht glatt und gleichmäßig glänzend wie eine Puppe. Die lebenden Arme hatten eine
   eigene, stumpfe Farbe, aber die Haut des Hound (eine Fleckung für alles); die toten Glieder und der Kern trugen die
   Töne des kleinen Bündels (Rauheit 0,42 – Kunststoffglanz). In der Nahaufnahme stach das Rohr in die Kamera.
2. **Mottenschwarm im Gesicht** (moth 4–5): nur dunkle Flächen mit rotem Rand. Ursache: Die Motten sitzen 6–10 cm vor dem
   Auge, die Lampe in der Hand 35 cm vor dem Körper – sie leuchtet von den Motten weg, ihre der Kamera zugewandte Seite
   bekommt nur das Grundlicht der Ebene (0,035). Flügel, Beine und Staub sind nicht zu erkennen.
3. **Falle der Death Rats** (rat 3–5): der blanke Draht ist im Spiel 1–2 Pixel breit (zwei Litzen, im 2-m-Gang gut 4 mm)
   und außerhalb des Lampenflecks unsichtbar. Die rote Leuchte sitzt an der Wand, nicht im Weg. Wer nicht auf das Knistern
   achtet, läuft blind hinein – unfair.
4. **Faceling, entwickelt** (faceling 2): Kopf glatt wie eine Schaufensterpuppe, der Bart ein glänzender schwarzer Klotz
   (drei Ellipsoide im geteilten, nassen Haar) – liest sich als Maulkorb.
5. **Skin-Stealer im Mantel** im Lampenlicht hellgrau wie ein Kittel (der Stoff ist dunkel, die Lampe auf 3 m überstrahlt
   ihn); im Dunkeln eine schwarze Gestalt mit Kapuze – gut.
6. **Gut**: Red Knight (läuft in Panik vorbei, Umhang, Hieb in die Luft, violettes Glimmen an der Wand); die purpurnen
   Wesen (Skin-Stealer leuchtet im dunklen Paranoia-Gang, Hound mit Schrift); Praetorianer (acht Beine, Aufbäumen);
   Wretch (Krustengesicht, Waffe, Angriff mit Ankündigung); Rudel (lauert flach, kaum zu sehen – gewollt).
7. **Laufweg**: der rote Hauptgang (0–80 m) liest sich als Level 3 (schwarze Rohre, Kabelleitern, Verteiler, Engelsbild,
   Gitter). Die Gangnetze (80–200 m, Bilder 020–049) sind dagegen kahl: Ziegel, die Kabelleiter unter der Decke, sonst
   fast nichts. Das Wiki: Elektrokästen, Kabel und alte Geräte überall. Ursache: die Netzstücke sind 5 m lang, die
   Ausstattung setzte nur alle 6–7 Kacheln etwas, das Rohrmodell (5,8 m) passt in kein Stück.
8. **Werkzeug**: das kleine Bündel kam nicht (am Start kein Lager in der Nähe); der Smiler kam nicht (am Start ist
   Bereich 0, und der erzwungene Ausfall fand kein Dunkel in 15–27 m); Faceling aus 5 m nur ein Punkt am Gangende.

Änderungen:
- **Haut des großen Bündels** (neue Texturebene `EL_BUNDLE` „buendel“, `src/render/texture_entity.cpp`): verschmolzene
  Menschenhaut über überdehnten Muskeln – fahl-gelblich mit gestauter Röte, Totenflecken (blauviolett), alte Blutergüsse,
  Dehnungsstreifen längs der Glieder, Poren und Stoppeln, wulstige Narbennähte (wo Glieder verwachsen sind), eingetrocknetes
  Blut in Spritzern und nach unten gezogenen Schlieren, schwarzer Schmier aus den Rohren, nässende Bläschen; nass glänzt
  sie nur fleckweise (Rauheit 0,15 gegen 0,8 trocken). Eigene Materialien auch für die toten Glieder und den Kern
  (`FM_ENT_CL_DEAD_A/B`, `FM_ENT_CL_CORE`; `clump_l3.py`), beide Modelle neu gebaut. Konstanten: Mikroebenen ab Platz 109,
  116 Plätze (`texture_entity.hpp`, `gbuffer.hlsli`, `mesh_gbuffer.hlsl`).
- **Motten im Gesicht** (`app.cpp`; `presence_moth_l3.cpp` `mothFaceCount`): die Lampe streut von Hand, Ärmel und den
  Flügeln vor der Linse zurück – weiches Fülllicht unter dem Kinn (ohne Schatten, nur solange Motten im Gesicht sitzen;
  ohne Lampe nur ein Hauch, denn Licht aus ist die Abwehr). Dazu Flügelstaub: ein zweiter Durchgang des Staubs in einem
  1,4-m-Würfel um die Kamera ab 10 cm (`renderer.cpp`, `particles.hlsl`: naher Abstand und Größe als Parameter).
- **Falle** (`rat_trap.py`, neu gebaut): drei abisolierte Adern zum Seil verdrillt (im 2-m-Gang etwa 1 cm), dazu zwölf
  Fetzen der abgenagten Isolierung (weiß und rot), die am Draht hängen – so bauen Nager; im Lampenlicht entsteht eine
  Reihe heller Flecken quer durch den Gang.
- **Faceling, entwickelt** (`faceling_evo.py`, neu gebaut): eigene Haut (stumpf, grauer, mehr Staub) und eigenes Haar
  (stumpf, verfilzt, graubraun) statt der geteilten Materialien (`FM_ENT_FC_SKIN/HAIR`); die anderen Facelings bleiben.
- **Gangnetze** (`tools/levels/level3.py`, eigener Zufall 3304 ganz am Ende – alle 1298 Objekte davor und die
  purpurnen Wesen bleiben, geprüft): 80 Wandteile ab 0,9 m Höhe – 45 Verteiler, 20 Schaltkästen mit Panzerrohr zur Decke
  (`power_box`), 15 Engelsbilder; Abstand zu Ausstattung, Wesen-Plätzen und Spuren; kein Gang wird enger.
- **Werkzeug**: kleines Bündel erst am Lager im breiten Gang, Smiler im Hauptgang über `presence encounter`, Faceling
  zusätzlich als Vorführung nah, Nahaufnahme des Bündels von der Seite.

## Runde 4 (`wesen_r4_1.jpg` … `_3.jpg`: die geänderten Wesen; Laufweg `wegbilder_r4_*`, 201 m durch 27 Räume)

Befunde:
1. **Falle**: jetzt lesbar – im Lampenlicht eine Linie mit weißen und roten Fetzen quer durch den Gang, im Dunkeln aus
   2,6 m noch eine Reihe heller Punkte unter der roten Leuchte (rat 3–6). Fair: man sieht sie, bevor man sie berührt.
2. **Motten im Gesicht** (moth 4–5): Flügelunterseiten mit Binden und Zickzackmuster, Saum und der behaarte Leib sind zu
   sehen, das Bild ist nicht mehr schwarz. Der Flügelstaub (moth 3) liest sich als große weiße Flocken wie Schnee.
3. **Großes Bündel**: nah (bigclump 6) fleckig, schmutzig, blutig, mit Narbennähten – keine Puppe mehr. Ganz nah (7)
   aber ein Muster aus dunklen, gewundenen Linien wie Würmer (das Gratrauschen der feinen Falten). Im hellen Gang aus
   3 m (4–5, Vergleich `bc4_cmp`) leichte Totenflecken, aber noch zu blass und glatt: die Fleckung ist aus der Ferne zu
   schwach, die Formen sind glatte Würste ohne Unebenheit.
4. **Faceling**: die eigene Haut ist stumpf statt glänzend, der Bart aber bleibt ein Klotz (nun matt).
5. **Kleines Bündel**: am Lager erschienen, aber die Kamera sah auf die Wand – es sitzt im Deckenschacht.
6. **Smiler**: im Stromausfall ein kleines Gesicht aus Augen und Zähnen im dunklen Gang vor dem hellen Raum dahinter;
   mit Lampe nicht heller (er verbirgt sie). Gut.
7. **Laufweg**: in den Netzen jetzt Verteiler, Schaltkästen mit Panzerrohr und Bilder (024, 026, 037, 042, 044), aber
   weiter kein Rohr – das Wiki nennt die Rohre mit schwarzer Flüssigkeit als Kennzeichen der Ebene.

Änderungen:
- **Haut des Bündels**: dunkler, Totenflecken größer und kräftiger, Schmier häufiger; Beulen unter der Haut (Zysten,
  Muskelknoten – Höhe 0,7 mm je Zelle, 14 je Kachel), stärkere Grobform; die feinen Falten als gerichtete, flache Querfalten
  statt Gratlinien.
- **Flügelstaub** kleiner und schwächer (Größe 2,2 → 1,5, Helligkeit 0,55 → 0,4).
- **Faceling-Bart**: statt drei Ellipsoiden 150 einzelne, verfilzte Strähnen an Backen und Kinn (1–3,4 cm, um den offenen
  Mund frei, kahle Stellen), `faceling_evo.py` neu gebaut.
- **Kleines Bündel**: die Kamera blickt zum Lager an der Decke hinauf; dazu die Vorführung mit Lampe.
- **Gangnetze**: Rohrstrang auf 82 % (4,8 m statt 5,8 m, auch der Durchmesser – DN 125 statt 150) passt auf die
  durchgehende 5-m-Wand eines Netzstücks, die Bögen gehen in die Wand. Je Netzstück höchstens einer (60 %), an einer Wand;
  Wandteile prüfen jetzt den Abstand längs derselben Wand. Ergebnis: 57 Rohrstränge, 24 Verteiler, 15 Schaltkästen,
  5 Bilder; Raummetrik grün (Gänge 2,0 m, 0 von 69 verstellt).
