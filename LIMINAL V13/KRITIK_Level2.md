# Kritikrunden Level 2 (Abnahmeschranke d)

Je Runde: Laufweg (`python tools/qa/walkway.py level2 --round rN --rooms 18 --seed 3`), Bögen
`docs/screens/level2/wegbilder_rN_*.jpg`, Karte `wegkarte_rN.png`; Befunde, Änderungen, Nachweis in der nächsten Runde.
Grundlage: `LEVELSPEC_2.md` (Wikidot Level 2). Ab Runde 4 (Abnahme): `--rooms 15 --seed 3` und alle Wesen der Ebene in ihren Räumen
(`python tools/qa/creatureshots.py level2 --round rN`, Bögen `wesen_rN_*.jpg`, Einzelbilder `build/qa/creatures/level2/rN/`).

## Runde 1 (Struktur nach dem Umbau, Ausstattung noch V13.2 plus Macchina-Platzhalter)

Laufweg 201 m durch 25 Räume, 51 Bilder.

Befunde:
1. **Wartungstunnel wie im Wiki:** kreidiger, schlammbrauner Beton, Maschinen und Rohrbündel an einer oder beiden Wänden, viele verschlossene Türen in Weiß, Grau und Creme sowie aus Holz, Warnschilder, Röhren in Weiß und Orange.
2. **Brownout schwarz** (Bilder 018–028): Röhren sichtbar, aber kein Licht auf Wänden und Boden.
   - Das Wiki beschreibt manche Bereiche als dunkel. Ein Brownout ist aber gedimmt, nicht aus.
   - Violette Notlichter (Wiki: seit dem Ausfall vereinzelt) fehlen dort.
3. **Werkzeug:** In 1–2 m engen Tunneln zeigen Bilder an Biegungen die Wand. Das liegt am Querschnitt, nicht an einem Fehler.

Änderungen:
- Brownout-Röhren 0,35 → 0,45, tote Gruppen 55 % → 45 %, Flackern bleibt.
- Violette Notlampen in Brownout-Kreuzungen mit 50 % statt 12 %.

Nachtrag Runde 1 (Prüfung):
- Nach der Änderung meldete die Raummetrik 3 und dann 58 verstellte Tunnel. Ursache war ein veralteter Ausstattungs-Abzug (`build/qa/props_level2.json` von 16:22, die Karte von 17:45).
- `room_metrics.py` erzeugt den Abzug jetzt selbst neu, wenn die Karte neuer ist. Mit frischem Abzug: 0 von 181 verstellt.
- Zusätzlich setzt `furnish_tunnel` in Tunneln unter 3 m keine Tür mehr gegenüber einer Maschine.

## Runde 2 (mit der neuen Ausstattung: 30 Klassen, Varianten der Maschinen)

Laufweg 208 m durch 28 Räume, 52 Bilder (`wegbilder_r2_*`). Befunde:
1. **Maschinen verschiedener Epochen gemischt**, wie im Wiki: genietete Schalttafel, beiges Steuerpult, grauer Schrank, Verteiler aus Gusseisen, Bakelit und Kunststoff; Rohrbündel über 2,13 m (001–015, 029–035).
2. **Dunkle Strecken** (018–027): ausgefallene Leuchtengruppen und ein Leerraum, wie im Wiki (Reihenschaltung, lichtlose Leerräume).
3. **Brownout:** Noch zu dunkel geplant; die Zone selbst liegt nicht auf diesem Weg.

Änderung: Brownout-Grundlicht 0,02 → 0,045, Nebel 0,06 → 0,035, Röhren 0,45 → 0,55, tote Gruppen 45 % → 35 %.

## Runde 3 (gleicher Weg, `wegbilder_r3_*`)

Nachweis: Die dunklen Strecken liegen in den Räumen 10, 129, 53, 139 und 105 (Beton-Tunnel ohne Leuchten, tote Gruppen) und im Leerraum. Die Zonenprüfung zeigt: kein Brownout-Raum auf dem Weg. Keine weitere Änderung an der Struktur. Die Runden 4 und 5 folgen mit den Level-2-Wesen.


## Runde 4 (Abnahme: Laufweg und alle Wesen, Basis 3d282a6)

Laufweg `python tools/qa/walkway.py level2 --round r4 --rooms 15 --seed 3`: 208 m durch 28 Räume, 52 Bilder
(`wegbilder_r4_1.jpg`, `_2.jpg`, `wegkarte_r4.png`). Neu für die Wesen: `tools/qa/creatureshots.py` – je Wesen ein Lauf
in seinem Raum der Ebene (erzwungen über `rand`, `presence`, `near`, `pack`; Spuren der Skin-Stealer aus dem
Ausstattungs-Abzug), erst im Licht der Ebene, dann mit Lampe; Bögen `wesen_r4_1.jpg` … `_5.jpg` (17 Wesen und Spuren).

Befunde:
1. **Photoshop** (`wesen_r4_1.jpg`, 1–5): Das Gesicht ist ein gezeichnetes Comic-Gesicht. Die Augen sind Scheiben in
   Flächenfarben, das Maul ist ein rosa Oval mit weißen Dreiecken. Der Körper ist ein roter Kasten mit gemalten Adern.
   Es gibt kein Fleischfoto, nur Vektorgrafik. Der Röhrenmonitor zeigt in der Mitte ein schwarzes Oval.
2. **Automat** (Cannibal Cuisine, 6–9): Hände und Waren lesen sich. Das Gehäuse ist aber glatt wie neu: blauer Lack ohne
   jede Spur. In der verdreckten Sackgasse wirkt er wie ein Spielzeug.
3. **Macchina-Werkstatt**: schwarz, auch auf dem Lampenbild.
4. **Laufweg**: Zwei Strecken sind völlig schwarz: Bilder 017–027 (68–108 m, 40 m am Stück) und 040–044 (160–176 m).
   Dort liegen tote Röhrengruppen und Beton-Tunnel ohne Leuchten. Das Wiki kennt dunkle Bereiche. LOOK_2 nennt aber für
   solche Stellen das violette Notlicht („brennt auch im Stromausfall“), und davon gibt es in diesen Strecken keines. Nur
   12 % der Kreuzungen haben eines.
5. Lesbar in Silhouette und Größe: das Hound-Rudel (bleich, schwarz verklebte Glieder), die Death Rats an den Fässern,
   die Netze der Nguithr'xurh, das Fenster am Gangende (Umriss winkt vor Hügeln im Tageslicht), der Wretch und der
   Swindlebird (blauer Vogel am Tunnelende).
6. **Werkzeug**:
   - Die Lampe braucht über eine Sekunde, bis sie voll leuchtet. Die „mit Lampe“-Bilder nach 12 Bildern zeigten kein
     Lampenlicht – daher auch Befund 3.
   - Jerry und das Fenster entstehen nur in geladenen Gegenden. Vom Start aus wurden sie nicht erzwungen
     („kein Rohr in der Karte“, „kein Gangende“).
   - Der Smiler kommt erst 7–10 s nach dem Stromausfall.
   - Die freie Kamera der Spuren stand im eigenen Körper.

Änderungen:
- **Photoshop neu** (Wikidot Entity 104: Fotos von Fleisch auf allen Seiten, Gesicht aus zwei Augenfotos und dem Foto
  eines aufgerissenen Pitbull-Mauls). Die Fotos sind jetzt echte Aufnahmen gebauter Szenen in Blender/Cycles
  (`tools/textures/ps_photos_render.py`):
  - acht Fleischszenen: marmoriertes Steak in der Styroporschale, Hackfleisch auf Metzgerpapier, Bauchspeck, Leber in
    Blut, Beinscheibe mit Mark, Darm auf Edelstahl, Rinderzunge, Haut mit Gänsehaut;
  - zwei menschliche Augen mit Lidern, Wimpern und Äderchen, die in verschiedene Richtungen sehen;
  - das Maul: Fell, schwarze Lefzen, gefleckter Gaumen, Eck- und Schneidezähne mit Zahnstein, Zunge, Speichelfäden.
- **Atlas `data/textures/ps_photos`** (`tools/textures/ps_photos_atlas.py`): Die Fotos sind entwickelt wie Abzüge –
  Farbstich, Abfall des Blitzes, Korn, Datumsaufdruck, Papierrand mit eingesogenem Blut und blutigen Fingerabdrücken.
- **Der Klotz** ist eine Collage aus 84 Abzügen rings um den Klotz: schief, überlappend, um die Kanten gebogen, Ecken
  stehen ab und zeigen die Papierrückseite. Blut läuft unter den Abzügen hervor, am Fuß steht eine Lache.
- **Werkzeug**:
  - Die Lampe bekommt 90 Bilder Anlauf.
  - Für Jerry und das Fenster geht das Werkzeug erst hin (Kammer des Vogels; 20 m vor einem Gangende, Blick weg).
  - Der Smiler bekommt 12 s.
  - Die Spuren werden mit der Spielerkamera und deren Lampe aufgenommen.

## Runde 5 (`wesen_r5_1.jpg` … `_5.jpg`; Laufweg unverändert)

Nachweis: `docs/screens/creatures/l2_randwesen/r6_ps_vorher_nachher.jpg` – links Runde 4, rechts Runde 5. Der Klotz ist
jetzt mit Fotos zugeklebt: Fleisch, Innereien, Haut, ein echtes Hundemaul, zwei echte Augen. Aus 3 m liest sich jedes
Bild als Foto. Im Glitch verrutschen die Scheiben mitsamt den Abzügen.

Befunde:
1. **Photoshop**:
   - Zwischen den Augen flimmert ein Schachbrett. Eine abstehende Ecke eines Abzugs stach durch die Augenfotos.
   - Das Bild im Monitor sitzt bis 9 cm neben der Scheibe, deshalb schnitt das dunkle Glas ein Oval hinein. Ursache:
     Blender-y ist im Spiel gespiegelt, die Drehung des Monitors wirkt also andersherum.
   - Die Modellprüfung meldet für die unterste Scheibe (`ps_block@1`) 50 % Splitterdreiecke: Der Schnitt lag genau auf
     dem Fuß.
2. **Jerry** (Lampe nach 90 Bildern):
   - Mit Lampe gut: ein dunkler, halb gerupfter Ara mit ausgebreiteten Flügeln vor dem Wandbild der Followers; im Bann
     legt sich das Rot über das Bild.
   - Im Licht der Ebene ist er blau auf blauem Wandbild kaum zu sehen.
   - Das bleibt so: Der Raum ist bewusst dunkel, das Murmeln führt hin, und der Blick ist die Gefahr (KREATUR_Jerry).
3. **Macchina** mit Lampe: Der leere Anzug mit dem geschweißten Gesicht lehnt rostig an der Wand und ist gut lesbar.
   Ohne Lampe bleibt die Werkstatt fast schwarz. Das bleibt so, sie ist verlassen.
4. **Gut**:
   - Der Smiler (Rauchsäule mit Augen und Zähnen im Gangende).
   - Das Rudel aus der Nähe mit Lampe (Rippen, verklebtes schwarzes Haar).
   - Die Motten um die Lampe am Verteilerkasten.
   - Der Wretch im Lampenlicht.
   - Die Spuren: Haut am Haken mit langgezogenem Gesicht, Lager mit Kleidern und Knochen, geschmolzene Haut im heißen
     Tunnel, Kratzer zu viert.
5. **Werkzeug**: Beim Faceling steckt die Kamera im Körper, weil die Gruppe auf den Spieler zugeht.
6. Der Laufweg-Befund 4 (schwarze Strecken) ist noch offen.

Änderungen:
- **Photoshop**:
  - Unter Augen und Maul stehen keine Ecken mehr ab.
  - Das rechte Auge liegt eine Lage über dem linken.
  - Monitorbild: Drehung gespiegelt (`apparitions_l2.cpp`), 0,95-fach.
  - Die Scheiben werden nicht mehr genau durch den Fuß geschnitten.
- **Automat**: Der Lack bekommt eine Textur mit Kratzern, Abplatzern und Schmutz (`painted_metal_white`/`_grey`). Die
  Textur braucht UV-Koordinaten (`_box_uv`), denn bewegliche Modelle tasten Texturen über UV ab; ohne UV wäre sie
  einfarbig.
- **Karte** (`tools/levels/level2.py`): In gut der Hälfte der toten Röhrengruppen hängt eine violette Notleuchte an der
  Wand, 12 Stück.
  - Gesetzt mit eigenem Zufall und ganz am Ende: Alle anderen 1885 Objekte bleiben gleich (geprüft).
  - Nur das eigene Leuchten, keine Raumleuchte. Das Kachellicht und damit das Dunkel für Smiler und Brownout bleibt
    unverändert.
  - `gameplay_test` 901/0 nach der Kartenänderung.
- **Werkzeug**: Das Faceling wird unmittelbar nach dem Hinstellen aufgenommen.

## Runde 6 (`wesen_r6_1.jpg`, `_2.jpg`: Photoshop, Automat, Macchina, Jerry, Faceling; Laufweg `wegbilder_r5_*`, gleicher Weg)

Nachweis:
- **Photoshop**: Das Flimmern zwischen den Augen ist weg. Das Monitorbild sitzt in der Scheibe: Fleisch mit einem Auge,
  rot, gestört (`docs/screens/creatures/l2_randwesen/r7_ps_bogen.jpg`, `r7_ps_nah.jpg`).
- **Automat**: Der Lack zeigt Abrieb an den Kanten und dunkle Flecken, er ist stumpf statt neu
  (`r6_cc_lack_vorher_nachher.jpg`).

Befunde:
1. **Laufweg** (`wegbilder_r5_1.jpg`): Die Notleuchten wirken kaum. In der 40-m-Strecke ist nur ein violetter Punkt zu
   sehen (Bild 018). Es wurden nur 12 gesetzt, weil der Mindestabstand von 0,9 m zu jedem Wandobjekt fast alle Wände
   sperrte. Dazu leuchten sie schwach (0,95) bei 13 cm Größe.
2. **Faceling**: Die Kamera steckt weiter im Körper, die Gruppe läuft sofort heran. Jerry und Macchina sind wie in
   Runde 5.
3. **Modellprüfung** für alle 211 Modelle der Ebene (Karte, Randwesen, Spuren, Wesen): 74 mit Befund.
   - Ausstattung: `jerry_perch` (96 entartete Dreiecke), `food_scraps` (8 Dreiecke mit gedrehtem Umlaufsinn).
   - Wesen: in den vereinfachten Stufen l1/l2 gedrehte Dreiecke an dünnen Teilen (Haar, Seide, Schnurrhaare, Fühler)
     und entartete Dreiecke im Gaumen von Wretch und Faceling.
   - Die Stufe 0 des Wranglers hat 160 000 Dreiecke, über der Grenze von 120 000.

Änderungen:
- **Notleuchte**:
  - Heller: Leuchten 0,95 → 1,8, Rippen 0,65 → 1,2. Weiter nur Eigenleuchten mit Schein, keine Raumleuchte.
  - Abstand je Wandobjekt: Türen, Kessel und andere Notleuchten 1,0 bis 1,6 m; Schränke, Steigleitungen und Kästen 0,6 m.
    Die Leuchte hängt mit 2,05 m über den Pumpen und unter den Rohrbündeln ab 2,13 m.
  - Gesetzt in 85 % der toten Gruppen: jetzt 18 von 23.
- **Faceling**: Vorführung im Gang (Licht der Ebene und Lampe), dazu die Gruppe aus 12 m.
- **Modelle**: `tools/qa/repair_glb.py` (neu) entfernt entartete Dreiecke und dreht den Umlaufsinn passend zur
  Vertexnormale. Es ändert nur die Indexliste; Punkte, Normalen und Gewichte bleiben. Wesen werden ohne
  Rückseitenkappung gezeichnet, im Bild ändert sich also nichts, die Daten stimmen danach. Angewandt auf `jerry_perch`,
  `food_scraps` und alle Wesen-Modelle der Ebene.
- **Wrangler**: Stufe 0 neu gebaut mit 118 000 statt 160 000 Dreiecken (`tools/blender/creatures/wrangler.py`, alle
  drei Formen).

## Runde 7 (Abnahme: alle Wesen `wesen_r7_1.jpg` … `_5.jpg`, Laufweg `wegbilder_r6_*`: 208 m durch 28 Räume, 52 Bilder)

Nachweis über alle Wesen der Ebene (Licht der Ebene und Lampe):
- **Photoshop**: Er steht hinter dem Röhrenmonitor im hellen Büro, ein Klotz aus blutigen Fotos. Der Monitor zeigt sein
  Bild, im Glitch verrutschen die Scheiben. In der Box legt sich das Rot über das Bild.
- **Automat**: In der Sackgasse das einzige Licht. Der Lack ist abgewetzt, die Knochenhände halten die Ware.
- **Swindlebird**: aus 9 m ein blauer Vogel am Tunnelende, aus 5 m als Papagei zu erkennen.
- **Macchina**: Die Werkstatt ist dunkel. Mit Lampe sieht man den rostigen Anzug mit geschweißtem Gesicht.
- **Jerry**: Mit Lampe ein dunkler Ara mit ausgebreiteten Flügeln vor dem Wandbild; der Bann färbt das Bild rot.
- **Fenster**: am Gangende, ein Umriss winkt vor Hügeln im Tageslicht.
- **Motten**: umschwirren die Lampe am Verteilerkasten.
- **Death Rats**: an den Fässern.
- **Rudel**: kreist mit Lampe ein.
- **Smiler**: Rauchsäule im Gangende.
- **Clump**: Bündel aus Gliedern in der Vorführung im Gang.
- **Nguithr'xurh**: Netzkugeln mit Spinnen über dem Kasten.
- **Wrangler**: Stufe 0 neu gebaut mit 118 000 Dreiecken, sieht aus wie vorher.
- **Faceling**: gesichtslose Gestalt im Gang (Vorführung).
- **Wretch**: im Lampenlicht.
- **Spuren**: Haut am Haken, Lager, geschmolzene Haut, Kratzer.

Die bereinigten Modelle (Ratten, Rudel, Motten, Netze, Wretch, Wrangler) sehen aus wie vorher; geändert hat sich nur
die Indexliste.

Laufweg: Die schwarze Strecke (Bilder 018–027) hat jetzt violette Punkte (018, 022, 025), klein und schwach, wie in
LOOK_2. Sie geben Richtung, ohne das Dunkel zu brechen. Sonst ist der Weg wie in Runde 4: kreidiger Beton, Maschinen
verschiedener Epochen, Röhren in Gruppen, Rohrbündel über Kopf, verschlossene Türen in vielen Farben.

Bleibt (bewusst):
- Jerry ist im Ebenenlicht blau auf Blau kaum zu sehen.
- Die Macchina-Werkstatt ist ohne Lampe schwarz.
- Der Growler ist aus 0,5 m ein bunter Knäuel. So hat ihn sein Briefing angelegt (junge Growler leuchten bunt).

Prüfungen: Raummetrik grün (Level 2: Decke ≥ 2,6 m, Gänge 2,0 m, Sichtachsen Median 17,8 m / P90 40,2 m, 0 von 181
verstellt), Ähnlichkeit S 0,812 (Ausreißer 17,9 %), Schilder 0 (Grenze 0), Modellprüfung 211 Modelle der Ebene 0 Fehler.
Abnahme der Ebene: siehe `AUFWAND.md`, Abschnitt „Level 2 – abgenommen“.
