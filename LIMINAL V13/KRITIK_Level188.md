# Kritikrunden Level 188 (Abnahmeschranke d)

Laufweg `python tools/qa/walkway.py level188 --round rN --rooms 15 --seed 3` (7 erreichbare Räume, E-036), Bögen
`docs/screens/level188/wegbilder_rN_*.jpg`. Wesen ab Runde 3 `python tools/qa/creatureshots.py level188 --round rN`,
Bögen `docs/screens/level188/wesen_rN_*.jpg` (Einzelbilder `build/qa/creatures/level188/rN/`). Grundlage: Wikidot Level 188 (Rev. 69).

## Runde 1 (Struktur, Ausstattung V13.2)

Laufweg 214 m durch alle 6 Räume, 54 Bilder.

Befunde:
1. **Hof wie im Wiki:** Atrium mit Mittelpfad, Birkenkreis aus Holzplanken, Fensterwände, darunter erleuchtete Fenster mit Gestalten.
2. **Fehler: Treppe ohne Kopfhöhe** (Bilder 019, 024, 039, 044).
   - Die Treppe stieg im 3 m hohen weißen Gang auf 3 m. Oben ragte der Kopf über die Wände; man sah den Hof und schwarzen Himmel.
   - Ursache: Die Engine legt Treppen in den unteren Raum, und dessen Decke bleibt gleich.
3. **Werkzeug:** Der Laufweg verlangte 15 Räume, die Ebene hat aber nur 6. Er lief deshalb im Kreis (6,8 km, 1 696 Bilder).

Änderungen:
- Die Treppe steigt jetzt in einem eigenen, 6 m hohen Treppenhaus zu einem Treppenabsatz auf 2,7 m. Die verschlossene Tür (Wiki) liegt am Absatz.
- `walkway.py` verlangt höchstens die erreichbaren Räume.

## Runde 2 (gleicher Weg)

207 m durch 7 Räume, 43 Bilder (`wegbilder_r2_*`). Nachweis: Die Treppe liegt im hohen Treppenhaus (020–023), oben bleibt Kopfhöhe, kein Blick nach draußen. Keine weiteren Befunde.

## Runde 3 (Abnahme: Laufweg und alle Wesen, Basis eac0140)

Laufweg `python tools/qa/walkway.py level188 --round r3 --rooms 15 --seed 3`: 207 m durch alle 7 Räume, 52 Bilder
(`wegbilder_r3_1.jpg`, `_2.jpg`, `wegkarte_r3.png`; Raumfolge Hof, Gang, Zelle 2, Gang, Treppenhaus, Absatz, Treppenhaus, Gang,
Zelle 3, Gang, Zelle 1 …). Wesen: neu die Tabelle für Level 188 in `tools/qa/creatureshots.py level188 --round r3` (10 Läufe:
Fensterwesen fern/Lampe/nah/Arm, Gestalt im Hof, Faceling im rosa Kleid, Skin-Stealer, Horcher mit Posen, Auftritt aus der
Zelle, Wurzelpistole mit Griff, Schuss, Keimlingen, Spuren; dazu was seit E-048 auf allen Ebenen wirkt: Stromausfall im Gang,
Beil in der Faust; Bereiche nach LOOK_188), je im Licht der Ebene und mit Lampe; Bögen `wesen_r3_1.jpg` … `_5.jpg`.

Befunde:
1. **Laufweg**: Hof nach LOOK_188 – weiße Fensterwände, Moos, Plattenweg, Birkenkreis, Tisch und Grammophon (000–010); der
   Gang grell und weiß, Mahagonitüren, leere Zellen, Treppenhaus mit Kopfhöhe (019–023). Das Moos liest sich von weitem als
   Rasen – so will es die Seite (man hielt es für Kunstrasen, es lebt); nah mit Lampe Polster (pistole 6).
2. **Fehler: grünes Kreuz an der Decke** (014, 035, 049; `zones_9`, `hand_1`): die Leuchte des Sicherungspunkts „Weißer Gang“
   hing genau unter einer Röhre (beide bei y0 + 4,5). Die Röhre lief quer durch den grünen Kasten – Geometrie in Geometrie und
   von weitem ein Apothekenkreuz, also doch ein Schild in einer schilderfreien Ebene.
3. **Horcher** (horcher 1–10): im Gang gut lesbar – das rosige Ohr vor dem bleichen, gebückten Leib, Ankündigung mit der Faust
   über dem Ohr (horcher 4), Licht auf dem Ohr (horcher 5). Schlecht: die Haut ist nicht kreideweiß, sondern cremegelb mit
   Wolken und Sprenkeln; nah mit Lampe (horcher 10) wie Marmor oder Tapioka – kein Riss zu sehen (bekannter Restmangel).
4. **Auftritt** (auftritt 1–3): das Ohr schiebt sich aus der Zellentür, dann der Leib – aber im grellen Licht ohne jedes
   Halbdunkel; die Seite gibt die grellen Gänge vor, der Schreck kommt nur aus dem Klang.
5. **Wurzelpistole** (pistole 1–10): aus 1,2 m von oben ein kleiner schwarzer Körper mit sechs blassen Wurzeln – noch ein
   Seestern; aus mehr als 3 m nichts als ein dunkler Punkt im Rasen (pistole 9, rechts unten). Der Griff im Ego-Bild, Schuss,
   Keimlinge gut. **Kasten der Gabe** (pistole 10): von oben ein leerer Bilderrahmen – der Deckel war um 109° nach hinten gekippt
   und lag dabei im Boden (Drehsinn), die Wände 12 mm dünn um ein helles Moosbett bis an den Rand. **Werkzeug**: Bild 8 (Pistole
   im Riss) stand in der Wand des Gangs (schwarze Fläche).
6. **Fensterwesen** (window 1–5): schwarzer Umriss vor der verschwommenen Landschaft, winkt; mit Lampe bleibt er schwarz (ein
   Schatten, Seite); unter 1,9 m fährt der Arm heraus, roter Rand (Zugriff). **Gestalt** im Hof (gestalt 1–3): dunkle Figur auf
   dem Moos, steht still, solange man hinsieht – gut.
7. **Faceling im rosa Kleid, Skin-Stealer** (dress, skinstealer): Werkzeugfehler – `near` stellte die Kamera im 3 m breiten
   Gang an die Wand (Kopf abgeschnitten), dann tötete sie den Spieler und die Vorführung lag unter dem roten Schleier.
8. **E-048**: Stromausfall im Gang (blackout 1–3) – die Leuchtflächen der Röhren erlöschen, nur die grüne Leuchte bleibt (Notlicht);
   mit Lampe der Kegel. Beil (hand 1–3): der Stiel liegt in der Faust; im Schwung wirken die Finger flach.
9. **Bereiche** (zones 1–12): Eingangsfenster warm, Plattenweg, Birkenkreis mit Fuge des Deckels, Tisch und Grammophon,
   Glasgalerie, Oberlicht, Tür zum Gang, Treppenhaus, Zelle – nach LOOK_188. Werkzeug: `zones_11` sah auf die Wand des Absatzes.

Änderungen:
- **Sicherungspunkt „Weißer Gang“**: Leuchte und Punkt 1,5 m nach Norden in die Lücke zwischen zwei Röhren (`level188.py`, am
  Ende, ohne Zufall; Karte neu erzeugt, Vergleich: nur diese beiden Zahlen).
- **Horcher-Haut**: eigene Texturebene „kreide“ (`EL_CHALK`, `texture_entity.cpp`; Mikroebenen ab Platz 110): trockene,
  kalkweiße Haut in Schollen gesprungen wie eingetrockneter Lehm (4 cm), Risse verschieden weit, darin rohes, nasses Fleisch,
  Schollenränder aufgeworfen und entzündet, einzelne Schollen abgeplatzt, feine Risse nur stellenweise; Kalk matt, Fleisch glänzt.
  `FM_ENT_HR_SKIN` → `ent(EL_CHALK, ML_PORES)`.
- **Auftritt im Halbdunkel** (Auslegung, `presence_listener.cpp`): beim ersten Heraustreten je Besuch (später in 2 von 5 Fällen)
  stottern die Röhren der Gänge und fallen 7 s aus – erst die Tür hinter dir, dann das Dunkel, dann das Ohr im Lampenkegel. Er
  ist blind; das Dunkel nimmt nur dem Spieler das Licht. Neue Prüfung im `gameplay_test`.
- **Wurzelpistole** (`e39_bed_moss`): um sie ist das Moos ausgezehrt (Auslegung: sie zieht Eisen und Blut aus dem Boden) – ein
  unregelmäßiger Fleck aus totem, braunem Moos (Radius um 0,6 m), nach außen trocken und in Zungen wieder grün, darin ein
  verzweigtes Geflecht blasser Wurzeln, die stellenweise unter das Moos tauchen.
- **Kasten der Gabe** (`e39_case`): tief (11 cm, Wand 18 mm), dunkle Innenwand, Moosbett 4 cm unter dem Rand mit Polstern, die
  Mulde als dunkler, blutfleckiger Samt in Pistolenform mit Moosrand; Deckel 98° offen (Drehsinn richtig), von einer Messingstrebe
  gehalten, innen der Abdruck der Pistole; Messingecken oben und unten, Schließblech und offener Riegel.
- **Werkzeug**: Faceling und Skin-Stealer erst als Vorführung im Gang (Licht der Ebene, Lampe, nah), dann der echte Auftritt
  mit nachgeführter Kamera (`face faceling|skinstealer|listener` neu in `app.cpp`); Bild 8 der Pistole aus dem Gang, dazu die
  Pistole aus 4 und 7 m und der Kasten aus 2 m; Auftritt mit Lampe im Dunkel und nach dem Licht; `zones_11` zur Tür des Absatzes.

## Runde 4 (alle Wesen `wesen_r4_1.jpg` … `_6.jpg`, 10 Läufe; Laufweg unverändert)

Befunde:
1. **Sicherungspunkt**: die grüne Leuchte hängt als eigener Kasten zwischen zwei Röhren (`zones_9`, `hand_1`) – kein Kreuz mehr.
2. **Horcher-Haut** (horcher 1–10): jetzt Schollen mit Rissen, in den Rissen rotes Fleisch, einzelne abgeplatzte Stellen –
   rissig, nicht mehr marmoriert. Schlecht: die Schollen (4 cm) sind am dünnen Leib so groß wie ein Glied breit, und jeder Riss
   hat denselben roten Rand – nah ein gleichmäßiges Netz wie Krakelee-Porzellan (horcher 10), von weitem ein Muster.
3. **Auftritt im Halbdunkel** (auftritt 1–5): die Tür geht auf, die Röhren stottern und gehen aus, im Dunkel schiebt sich der
   bleiche Leib aus der Zelle (auftritt 3), mit Lampe Ohr und Faust (auftritt 4) – der Schreck, der vorher fehlte. Schlecht:
   der 24 m lange Gang zählt als „großer Raum“ (`Haunt::powerCut`, Welle Feld um Feld wie im Parkdeck): das Dunkel kam als Welle,
   dauerte fast 15 s statt 7 (auftritt 5 noch dunkel), und während der Welle blieben Treppenhaus und Zellen hell. In den
   Posen-Bildern (horcher 2–10, erzwungenes Heraustreten) leuchteten dabei die Zellen durch die Oberkante der Gangwand –
   ein Sägezahn aus hellen Dreiecken unter der Decke (Licht der Nachbarräume durch die Wandkante, sonst im Hellen unsichtbar).
4. **Wurzelpistole** (pistole 1–13): der ausgezehrte Fleck mit dem Wurzelgeflecht macht die Stelle aus 1–2 m sofort lesbar,
   die sechs Wurzeln der Pistole gehen im Geflecht auf (kein Seestern mehr). Schlecht: der Fleck ist eine Fläche aus Ringen mit
   Material je Viereck – flache Farben, eckige Ränder, wie ein ausgeschnittenes Papier (pistole 4, 9, 10); aus 4 und 7 m
   (pistole 11, 12) ein kleiner, dunkler Fleck.
5. **Kasten der Gabe** (pistole 10, 13): von oben ein offener Holzkasten mit stehendem Deckel, Moosbett, dunkelroter Mulde in
   Pistolenform und Messingecken – kein Rahmen mehr. Modellprüfung: der Samt hatte 5,7 % entartete Dreiecke (Fase auf 4 mm).
6. **Faceling im rosa Kleid** (dress 1–4): im Gang eine gesichtslose Frau im rosa Kleid, still, 3,4 m – im grellen Licht
   unheimlich gerade weil nichts verdeckt ist; mit Lampe gleich. **Skin-Stealer** (skinstealer 1–3): wahre Gestalt, fahlgelb
   mit dunklen Flecken und Noppen, eingesunkene Augen. Werkzeug: der echte Auftritt (dress 5–8, skinstealer 4–6) war nicht im
   Bild – sie erscheint in einer Zelle (Protokoll: 61,5/34,5) und geht dort umher, bis sie jemanden sieht; die Kamera (`face`)
   sah auf die Zellenwand.
7. **Unverändert gut**: Fensterwesen (window 1–5), Gestalt (gestalt 1–3), Stromausfall der Halluzination (blackout 1–3),
   Beil (hand 1–3); `zones_11` zeigt jetzt die verschlossene Tür des Treppenabsatzes.

Änderungen:
- **Kreidehaut**: Schollen 2,6 cm, die meisten Risse eng und nur dunkel (trockener Spalt), weit und rot mit entzündetem Rand nur
  in Flecken, wo die Haut aufklafft; Haarrisse grau; abgeplatzte Schollen nur in diesen Flecken.
- **Dunkel ohne Welle**: `Haunt::powerCut(…, wave)` – der Horcher schickt die Dauer negativ (der ganze Stromkreis auf einmal,
  7 s); `Session` und Koop (`netPowerCut`, Vorzeichen der übertragenen Dauer) reichen das durch, Protokoll unverändert, alle
  anderen Aufrufer behalten die Welle.
- **Moosfleck**: 230 flache Moospolster wie der Buckel am Griff, innen tot und braun, nach außen trocken, am Rand einzelne grüne
  und trockene Polster, die in den Rasen auslaufen; das Wurzelgeflecht liegt darüber und taucht zwischen den Polstern ab.
- **Kasten**: Samt 6 mm mit kleiner Fase (Modellprüfung 0 Fehler).
- **Werkzeug**: echter Auftritt von Faceling und Skin-Stealer mit `near` in ihrer Zelle.

## Runde 5 (alle Wesen `wesen_r5_1.jpg` … `_6.jpg`, Laufweg `wegbilder_r5_*`: 207 m durch 7 Räume, 52 Bilder)

Befunde:
1. **Laufweg**: wie Runde 3, an der Decke des Gangs (014, 035) jetzt ein grüner Kasten neben der Röhre – kein Kreuz. Hof,
   Gang, Zellen, Treppenhaus, Absatz nach LOOK_188; keine neuen Mängel.
2. **Auftritt im Halbdunkel** (auftritt 1–5): Tür, dann gehen die Röhren aller Gänge auf einmal aus (kein Sägezahn mehr an der
   Wandkante, das Treppenhaus ist mit dunkel), im Dunkel schiebt sich der Leib aus der Zelle (auftritt 3), mit Lampe Ohr und
   Faust (auftritt 4), nach 7 s stottert das Licht zurück (auftritt 5). In den Posen-Bildern (horcher 1–4) steht er im dunklen
   Gang – die erste Begegnung hat jetzt Halbdunkel, Klang vor Sicht und eine Lichtreaktion.
3. **Kreidehaut** (horcher 9–10, nah mit Lampe): kalkweiß mit feinen, dunklen Rissen, rote Spalten nur stellenweise – liest sich
   als gesprungene, trockene Haut, nicht mehr als Marmor und nicht mehr als Porzellannetz. Von weitem (horcher 7) kreideweiß
   wie die Gänge, das rosige Ohr setzt sich ab. Abgenommen.
4. **Wurzelpistole** (pistole 4): der Fleck ist jetzt aus Polstern, sein Rand läuft in den Rasen aus – nah gut. Schlecht: das
   tote Moos ist mit der grünen Moostextur nur ein dunkles Oliv; aus 4 und 7 m (pistole 11, 12) trennt es sich kaum vom Schatten
   im Rasen. **Kasten** (pistole 10, 13): offener Kasten, abgenommen. **Pistole im Riss** (pistole 8): blasse Wurzeln in den
   Rissen des weißen Bodens, gut.
5. **Faceling im rosa Kleid** (dress 5–8): der echte Auftritt in ihrer Zelle – sie sieht einen, kommt, das leere Gesicht füllt
   das Bild (dress 7, roter Rand), dann der tödliche Zugriff (dress 8). Werkzeug: `near` stellte die Kamera in der Zelle an die
   Wand (Kopf abgeschnitten, dress 5–6), ebenso beim Skin-Stealer im Gang (skinstealer 4–6).
6. **Unverändert gut**: Fensterwesen (window 3), Gestalt (gestalt 1), Stromausfall (blackout 2), Faceling-Vorführung (dress 3),
   Skin-Stealer-Vorführung (skinstealer 3), Sicherungspunkt (`zones_9`).

Änderungen:
- **Moosfleck**: totes Moos ohne Textur als helles, stumpfes Strohbraun (linear 0,21/0,145/0,06; heller als der Rasen und eine
  andere Farbe), trockenes Moos bleibt mit Textur.
- **Werkzeug**: `near <wesen> <abstand> <seite> 1` sucht erst den größten Abstand über alle Winkel (im 3 m breiten Gang und in
  der Zelle stand die Kamera sonst an der Wand); `face faceling|skinstealer` zielt auf den Kopf (1,5 m).

## Runde 6 (Abnahme: alle Wesen `wesen_r6_1.jpg` … `_6.jpg`, 10 Läufe; Laufweg wie Runde 5)

Befunde:
1. **Fensterwesen** (window 1–5): dunkler Umriss vor der verschwommenen Landschaft, winkt (window 1–3), mit Lampe bleibt er ein
   Schatten (window 4), unter 1,9 m der Arm und der rote Rand des Zugriffs (window 5). Nah (1,7 m) wirkt der Arm steif wie das
   Glied einer Gliederpuppe – Restpunkt (ein Schatten nach der Seite; nur im Augenblick des Zugriffs so nah).
2. **Gestalt** (gestalt 1–3): steht weit hinten im Hof auf dem Moos, mit Lampe unverändert schwarz. Abgenommen.
3. **Faceling im rosa Kleid**: Vorführung im Gang (dress 1–4, Licht der Ebene, Lampe, nah: das leere Gesicht unter dem
   schwarzen Haar). Der echte Auftritt mit weitem Abstand (dress 5–8): sie steht in ihrer Zelle mit dem Gesicht zur Wand und
   rührt sich nicht, solange sie einen nicht sieht – der Zugriff selbst ist in Runde 5 belegt (dress 7–8). Abgenommen.
4. **Skin-Stealer** (skinstealer 1–6): Vorführung im Gang und echt in einer Zelle aus 4 m, mit Lampe – fahlgelbe Noppenhaut,
   eingesunkene Augen, gebückt an der Wand. Abgenommen.
5. **Horcher** (horcher 1–10, auftritt 1–5): Halbdunkel beim Heraustreten, Kreidehaut, Ankündigung, Licht auf dem Ohr wie in
   Runde 5. Abgenommen.
6. **Wurzelpistole** (pistole 1–13): der Fleck aus Polstern mit dem Wurzelgeflecht ist nah gut, aber das tote Moos ist mit
   0,21/0,145/0,06 dunkelbraun wie nasses Laub – aus 4 und 7 m (pistole 11, 12) noch kaum zu sehen.
7. **Stromausfall, Beil, Bereiche** (blackout 1–3, hand 1–3, zones 1–12): unverändert gut, `zones_9` ohne Kreuz.
8. **Modellprüfung der Ebene** (50 Modelle: Karte mit Varianten, Gaben, Wesen in allen Stufen, Funde): `ent_listener_l1/l2`
   und `ent_window_l2` mit Dreiecken gegen den Umlaufsinn (bis 9,4 % in einem Teil, nach dem Vereinfachen).

Änderungen:
- **Moosfleck**: Radius 0,8 m (330 Polster), mehr totes Moos, Farbe 0,46/0,34/0,15 (unter dem Licht ein stumpfes Rostbraun –
  passt zur Auslegung: sie zieht Eisen aus dem Boden).
- `repair_glb.py ent_listener_l1 ent_listener_l2 ent_window_l2` (41, 61 und 6 Dreiecke gedreht; Modellprüfung 50/0).

## Runde 7 (Zwischenprobe Wurzelpistole und Horcher-Stufen, `wesen_r7_1.jpg`, `_2.jpg`)

Befunde:
1. **Wurzelpistole**: aus 1,2 m ein rostbrauner, ausgezehrter Fleck mit blassem Wurzelgeflecht um die schwarze Pistole
   (pistole 1–4) – sie liest sich als Wesen, das den Boden auszehrt, nicht als Spielzeug im Rasen; aus 4 m (pistole 11) ein
   brauner Fleck mit hellen Adern neben dem Kasten, aus 7 m (pistole 12) noch ein kleiner brauner Fleck – man sieht, dass dort
   etwas ist. Nah wirken die Polster als einzelne Tupfen etwas gleichförmig (Restpunkt).
2. **Horcher-Stufen** nach `repair_glb` (horcher 7–8 aus 9 m): unverändert im Bild. Abgenommen.
3. **Kasten** (pistole 10, 13): offener Kasten, unverändert. Abgenommen.

Restpunkte (in `AUFWAND.md`): der Arm des Fensterwesens wirkt nah steif; die toten Moospolster sind nah gleichförmige Tupfen;
die Pistole selbst bleibt aus mehr als 3 m ein dunkler Punkt (der Fleck verrät sie); die Gänge sind außerhalb des Auftritts
grell (Seite).
