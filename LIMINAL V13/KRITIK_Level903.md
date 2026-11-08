# Kritikrunden Level 903 (Abnahmeschranke d)

Laufweg `python tools/qa/walkway.py level903 --round rN --seed 3` (ein Raum, Bahnsteig hin und zurück, E-036), Bögen
`docs/screens/level903/wegbilder_rN_*.jpg`. Grundlage: Wikidot Level 903 (Rev. 60).

## Runde 1 (Struktur, Ausstattung V13.2)

Laufweg 294 m, 74 Bilder.

Befunde:
- **Wie im Wiki:** Bahnsteig mit gelber Kante, ruhender Zug über die ganze Länge, fahrender Wagen, lange Röhren in zwei Reihen, Schilder, zugemauerte Treppen.
- **Dunkle Bahnsteigenden** (019–027): gewollt (`level903.py`: Röhren nur bis 22 m vor den Enden). Der endlose Bahnsteig verliert sich im Dunkeln; man sieht keine Endwand.
- **Keine Änderung.**

## Runde 2 (Abnahme, Vorgänger; Laufweg `wegbilder_r2_*`, Wesen `wesen_r2_1.jpg` … `_4.jpg`, Basis 2f0e0fa)

Laufweg 294 m, 74 Bilder, 1 Raum (E-036: die Karte ist nach Wiki ein einziger Bahnsteig). Neu: Tabelle für Level 903 in
`tools/qa/creatureshots.py` (Läufe metro, faceling, zug, blackout, hand, zones; Licht der Ebene und Lampe). Befunde aus den
Bögen und den Änderungen im WIP-Stand (24e48f0, vom Nutzer angehalten; in dieser Abnahme geprüft und übernommen):

Befunde:
1. **Treppe von hinten** (zones_4): offener Kasten – zwischen den Brüstungswänden der Stufenkörper aus Terrazzo und die
   Rückseite der Ziegelwand, wie ein Karton.
2. **Enden mit Lampe** (zones_7_lampe, zones_8_lampe): eine helle Fliesenwand; die Station endet sichtbar (Wiki: vermutlich
   mindestens 100 Meilen lang).
3. **Ruhender Zug** (zug_5, zug_6, faceling_4–7): der Ablaufschmutz unter der Regenrinne war ein durchgehendes helles Band
   bis 2,0 m mit gewellter Unterkante – vom Steig aus wie Schnee auf dem Dach oder eine Markise über die ganze Zuglänge.
4. **Unter dem Wagen** (Gleisbilder metro_7): die Stirnflächen des begehbaren Wagenbodens (Block der Karte) trugen die weißen
   Fliesen der Halle und standen hell unter der Kupplung; ebenso die Seiten der Türschwellen im Spalt neben den Türen.
5. **Ostende**: zwischen der letzten Kupplung (192,5) und der Stirnwand klaffte die Hallenwand hinter dem unsichtbaren
   Zugblock.
6. **Facelings** (faceling_1–3): die friedliche Gruppe stand im Gang des fahrenden Wagens, eine winkt – kein Fehler, aber den
   Steig zeigen die Bilder nicht. Vorführung (faceling_4–7) mit drei Varianten gut.
7. **Wagenseite im Dunkeln** (metro_2): fast schwarz bis auf die Tür; die Lampe trifft im Bild nur die Kante – nicht
   beurteilbar.

Änderungen (WIP, Modelle gebaut, Karte neu erzeugt):
- `dark_end_903` (neu): stumpfe, fast lichtlose Fläche 22 × 5,4 m an beiden Stirnwänden; Halt-Regel in `mapkit.py` und
  `gameplay_test` (`mountOf`).
- `stairs_barricaded`: Rückseite als Fliesenwand bis zur Decke mit Kantenschutz, Sockelschmutz, Wasserlauf, verschlossener
  Revisionstür (Zarge, Bänder, Drücker, Schloss, Lüftungsgitter, Warnschild in erfundenen Zeichen).
- `metro_dormant_*`: schmaler Schmutzrand unter der Rinne, darunter einzelne Läufer (gleiche Zufallsfolge).
- `metro_car_903`: Kopfquerträger mit Rippen, rußiges Blech bis ins Schotterbett, Bremsleitung, Kabelschleife, Rost vor den
  Stirnflächen des Blocks; Seitenbleche; Türschwellen dunkel.
- `level903.py` (am Ende, ohne Zufall): Dunkel an beiden Stirnwänden, ein letzter Zugabschnitt im Osten.

## Runde 3 (Laufweg `wegbilder_r3_*`: 294 m, 74 Bilder, 1 Raum; Wesen `wesen_r3_1.jpg` … `_5.jpg`, 6 Läufe)

Befunde:
1. **Laufweg**: die Treppe ist von Westen jetzt eine Fliesenwand mit Revisionstür (008–010) – ein Betriebsraum, kein Kasten
   mehr. Die Enden laufen ins Dunkel (019–028, 066–073), der Zug hat keine Schneekante mehr (040–065). Keine neuen Mängel.
2. **Enden mit Lampe** (zones_7/8/11_lampe): der Kegel verliert sich; aus 5,5 m bleibt eine schwache runde Aufhellung auf der
   Dunkelfläche (zones_11_lampe) – liest sich als ferne Wand im Dunst. Abgenommen.
3. **Rückseite der Treppe** (zones_4, zones_10): Fliesen, Kantenschutz, Revisionstür mit Warnschild; im Lampenlicht stark
   rostfleckig (zones_10_lampe) – passt zu einer verlassenen Station. Abgenommen.
4. **Metro-Wagen, Front vom Gleis mit Lampe** (metro_8_lampe): das Trittblech der Stirntür (Edelstahl) leuchtet als leeres
   weißes Rechteck, der Rammschutz (Unterboden mit Lacktextur `painted_metal_grey`) wird weiß mit schwarzen Flecken
   (Dalmatiner). Ohne Lampe (metro_8) dunkel und richtig. Unter dem Wagen (metro_8, metro_10): Kopfquerträger und Blech
   decken die Fliesen des Blocks ab – gut. Wagenseite im Licht der Ebene (metro_3, metro_9): hell, kein Eigenschatten.
5. **Facelings** (faceling_1–3): nur die Ziegelwand der Treppe bei 140 – `near … 6 0 1` stellte die Kamera hinter die
   Treppe, kein Faceling im Bild. Vorführung (faceling_4–7) unverändert gut.
6. **Ruhender Zug, bewohnter Wagen** (zug_1–6): Vorhänge mit warmem Spalt, Schild, Kiste, Läufer unter der Rinne – gut. Glas
   deckend dunkel (Restmangel, `PROPS_Level903.md`).
7. **Stromausfall** (blackout_1–3), **Beil** (hand_1–3): unverändert.

Änderungen:
- Unterboden (`under`) ohne Lacktextur, stumpf und rußig (r 1,45); Trittblech gebürstet (`alu_dull`). `metro_car_903` und
  `metro_car_37` neu gebaut (derselbe Wagen; in Level 37 nur das Aussehen, kein Spielablauf).
- Werkzeug: Faceling-Lauf mit fester Kamera auf dem freien Steig (124/11,5); zwei Bilder der Wagenseite nur im Lampenlicht
  (metro_11 frontal aus 2 m, metro_12 schräg entlang der Seite).

## Runde 4 (Wesen `wesen_r4_1.jpg` … `_5.jpg`, 6 Läufe; Laufweg wie Runde 3)

Befunde:
1. **Front mit Lampe** (metro_8_lampe): Rammschutz gleichmäßig dunkelgrau mit Rippen, Trittblech mattgrau – behoben.
2. **Wagenseite nur im Lampenlicht** (metro_11, metro_12; Ebenenlicht aus): die Seite bleibt schwarz, nur roter Streifen,
   Fensterrahmen, Tür und der Schatten der Hand auf dem Streifen sind hell. Also **kein Eigenschatten** des unsichtbaren
   Blocks (E-041 wirkt: der Streifen liegt in derselben Zelle und ist beleuchtet), sondern das Material: glattes Metall
   (`alu`, m 0,75, r 0,75) ohne Streuanteil – im Kegel einer Lampe am Auge kommt fast nichts zurück. In Level 903 gibt es im
   Spiel keinen Stromausfall (Seite: sicher), aber die Seite wirkt so im Lampenlicht stumpf schwarz statt metallisch (auch in
   Level 37, gleicher Wagen).
3. **Facelings** (faceling_1–3): auch von 124/11,5 kein Faceling im Bild. Das Protokoll zeigt den Grund: die Gruppe erscheint
   bei (99,5/5,5) – im fahrenden Wagen. Auf dem offenen Steig mit Sichtachsen bis 196 m ist der Wagen fast der einzige Ort
   außer Sicht (`hiddenSpawn`). Kein Fehler: so kommen sie mit dem Wagen an (Wiki: hinein nur mit einem Wagen); faceling_3
   zeigt eines im Wagen.
4. Übrige Läufe wie Runde 3.

Änderungen:
- `alu` (Wagenkasten): r 1,05, m 0,55 – gebürstet wie die Laibungen, breiter Schimmer im Lichtkegel; `metro_car_903` und
  `metro_car_37` neu gebaut.
- Werkzeug: Faceling-Lauf zeigt die Ankunft durch die offene Tür (faceling_1/2), nach 15 s den Steig (faceling_8) und das
  Winken (faceling_3).
