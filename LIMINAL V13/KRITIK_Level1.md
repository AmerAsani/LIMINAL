# Kritikrunden Level 1 (Abnahmeschranke d)

Je Runde: Laufweg durch die ganze Ebene (`python tools/qa/walkway.py level1 --round rN --rooms 18 --seed 3`, 836 m durch
18 Räume, 209 Bilder alle 4 m; Bögen `docs/screens/level1/wegbilder_rN_1..6.jpg`, Karte `wegkarte_rN.png`), Befunde,
Änderungen, Nachweis in der nächsten Runde. Ausstattungs-Runden je Objektklasse stehen in `PROPS_Level1*.md`.

## Runde 1 (nach Zusammenführung aller Level-1-Ausstattung)

Befunde:
1. **Gothic-Hallen leer und flach** (Bilder 036–052): Säulenreihen tragen nichts, die Decke ist eine glatte Fläche; vom
   Wiki-Merkmal „geschwungene Bauweise mit Bögen" bleiben nur die Durchgangsbögen.
2. **Gild-Wände kahl** (063–068, 112–117, 133–139): lange zweifarbige Wandflächen ohne jedes Teil.
3. **Garden fast schwarz** (141–152): Pflanzen und Bäume ohne erkennbares Licht, grüner Nebel schluckt die Leuchten.
4. **Werkzeug**: viele Bilder zeigen aus 0,5–2 m eine Wand (Kurven des Wegs; Blick nur in Laufrichtung).
5. Gut: Aquila (Stützen, Leuchtbänder, Pfützen mit Spiegelung, Kisten), Ouroboros (Baustelle mit Böcken, Leitern,
   Baustrahlern), Übergänge mit Bögen, Gänge und Zimmer.

Änderungen:
1. Neues Modell `arcade_gothic` (Blender, 2 576 Dreiecke): Segmentbogen zwischen zwei Säulen einer Reihe, Kämpfer auf
   Halsring-Höhe (4,5 m), Scheitel 5,45 m, Zwickel bis zur Decke, Brettschalung, Archivolte, Schlussstein; hängt an der
   Decke (Halt-Regel Decke). 2 Reihen je Halle, Bögen zwischen allen benachbarten Säulen.
2. Gild: Rohr unter der Decke entlang aller Wände, dazu Verteiler, Kästen, Lüftungsgitter und Steckdosen in
   unregelmäßigen Abständen (je Wandkachel 18 %).
3. Garden: Leuchtbänder dichter (6 m statt 7 m), heller (0,85 statt 0,75), weniger tote (25 % statt 35 %).
4. `walkway.py`: steht in Laufrichtung eine Wand näher als 3 m, blickt die Kamera in die freieste Richtung bis ±75°.

## Runde 2

Nachweis: Gothic mit Arkaden (Bilder 036–050) – die Hallen lesen sich jetzt als geschwungene Säulenhallen; Gild-Wände mit
Verteilern und Kästen (112, 113, 121, 136, 137). Neue Befunde:
1. **Garden immer noch zu dunkel** (141–143): Nebeldichte 0,05 schluckt das Licht nach wenigen Metern.
2. **Werkzeug**: Wände in 3–5 m Abstand füllen noch ganze Bilder (Gild 133–139).

Änderungen: Garden Nebeldichte 0,05 → 0,03, Grundlicht 0,045 → 0,065 (bleibt die dunkelste Hallen-Zone);
`walkway.py` Schwelle 3 → 6 m.

## Runde 3

Nachweis: Garden lesbar (Bilder 144–152: Birken, Farne, Moos, Ranken im grünlichen Licht), Gothic-Arkaden aus allen
Winkeln (162–173), Ouroboros-Baustelle unverändert gut. Neue Befunde:
1. **Messfehler im Werkzeug** (Bilder 005, 022 zeigen die Wand aus 0,4 m): `maplib.Grid` ließ sichtbare Blöcke durch,
   also auch die freistehenden 4,2-m-Wandscheiben in Aquila. Das betraf Laufweg-Blick **und die Sichtachsen-Metrik**.
   Behoben: Begehbarkeit sperrt verdeckte Blöcke, Gruben und sichtbare Blöcke ab 1,5 m (niedrigere sind Podeste wie der
   Bahnsteig von Level 903); Sicht auf Augenhöhe sperren Wände, Stützen und Blöcke ab 1,5 m.
2. Korrigierte Messung aller Ebenen: nur Level 1 verletzt eine Grenze (Sichtachse P90 79,7 m < 85 m) – die Wandscheiben
   waren 12–18 m lang. Gekürzt auf 6 m: Median 36,8 m, P90 86,2 m, Raummetrik aller zehn Ebenen grün; Ähnlichkeit und
   Schilder unverändert grün.
