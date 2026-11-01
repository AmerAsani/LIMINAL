# Ausstattung Level 2 „Pipe Dreams“ (V13.3 Teil 1)

Alle Objektklassen der Ebene in Blender neu (`tools/blender/models/models_l2.py`), Grundlage `LEVELSPEC_2.md`/`LOOK_2.md`
(Wikidot Level 2: Wartungstunnel mit Maschinen und Rohren aus mehreren Epochen, verschlossene Türen in Weiß/Grau/Creme/
Sperrholz/Holz, Fenster-Tür mit Schwarz dahinter, Fässer, Plattformwagen, Werkzeug, Leitern, Kabeltrommeln, Klappstühle,
verfallenes B.N.T.G.-Gleis mit Trichterwagen, verlassene Macchina-Bereiche mit Werkzeug und Teilen).

**Bauweise:** eigener Geometrie-Kern im Modul (Drehkörper, gezogene Profile mit Gehrung, Prismen, Fillets) und Normteile
(Sechskantschrauben/Muttern, Vorschweißflansche, Flanschverbindungen, Absperrschieber mit Ventilrad, Manometer,
Rundstahlbügel, C-Schienen). Gebrauchsspuren sind eigene Flächen mit 1–4 mm Abstand (Rost, Abplatzer, Läufer, Öl, Staub,
Etiketten, Klebeband, Schablonen), Dellen sind Verformungen. Ein Objekt je Material. Rauheit/Metallizität je Material über
`r=`/`m=`; Lacktexturen `painted_metal_white/grey` dort, wo Rostinseln gewollt sind (Kessel, Verteiler, Türen, Fass v1).

## Klassen

Maße: Tiefe (x) × Breite (y) × Höhe (z) in m. Dreiecke je Variante. „Stück“ = Anzahl in der aktuellen Karte (nach dem
Umbau auf das 15-m-Tunnelnetz). Bögen: `docs/screens/props/level2/vorher/<name>.jpg` und `.../final/<name>.jpg`
(Varianten `_v1` …).

| Klasse | Var. | Befestigung | Maße | Dreiecke vorher | Dreiecke nachher | Stück | Materialien |
|---|---|---|---|---|---|---|---|
| barrel | 3 | Boden | 0.59 x 0.59 x 0.89 | 280 | 4206/4256/4218 | 3 | 18: Lack blau/rostrot/schwarz, Etikett, Kappe, Kennband, Rost, Rostfahne, Öl, Staub … |
| cable_reel | 1 | Boden | 0.60 x 0.35 x 0.60 | 420 | 3288 | 1 | 10: Trommelholz, Fugen, Kabel, Stecker, Schablone, Anker … |
| chamfer_26 | 1 | Boden (Ecke) | 1.00 x 1.00 x 2.60 | 8 | 1032 | 24 | 8: Beton (concrete_chalky) hell/nass/Schmutz/Salz, Konus, Rost … |
| door_level3 | 1 | Durchbruch | 1.01 x 0.98 x 2.52 | 396 | 1736 | 1 | 12: Blatt, Zarge, Beschlag, Bänder, Pinsel-„3“, Spuren … |
| door_plywood | 1 | Wand | 0.09 x 1.06 x 2.10 | 644 | 2332 | 15 | 14: Sperrholz, Lagen hell/dunkel, nass, Bohle, Schloss, Sprühfarbe … |
| door_steel_cream | 1 | Wand | 0.13 x 1.02 x 2.10 | 1064 | 1714 | 7 | 15: Blatt, Zarge, Beschlag, Sprühfarbe, Zettel, Klebeband … |
| door_steel_grey | 1 | Wand | 0.13 x 1.02 x 2.10 | 1184 | 1482 | 14 | 17: Blatt, Zarge, Schließer, Trittblech, Blechschild … |
| door_steel_white | 1 | Wand | 0.13 x 1.02 x 2.10 | 1065 | 1426 | 4 | 17: Blatt, Zarge, Schließer, Warn- und Verbotsschild … |
| door_window | 1 | Wand | 0.13 x 1.02 x 2.10 | 944 | 1320 | 9 | 14: Blatt, Glasleisten, schwarzes Glas, Kratzer … |
| door_wood_old | 1 | Wand | 0.14 x 1.05 x 2.10 | 1292 | 2416 | 14 | 15: Altanstrich, vergrautes/faules Holz, Langbänder, Kastenschloss, Emailleschild … |
| folding_chair | 1 | Boden | 0.44 x 0.41 x 0.86 | 328 | 1492 | 7 | 6: Rahmen, Sitz/Lehne, Kappen, Niete, Rost |
| hopper_wagon | 1 | Boden | 2.17 x 1.37 x 1.35 | 348 | 6780 | 1 | 7: Rost (Trichter), Rahmen, Farbreste, Schotter, Steine … |
| junction_box | 1 | Wand | 0.23 x 0.62 x 1.74 | 336 | 1792 | 271 | 13: Lack (Textur), verzinkt, Warnzeichen, Typenschild, Kreppband … |
| mach_cabinet | 1 | Boden (Wand) | 0.57 x 1.01 x 2.17 | 956 | 3056 | 196 | 23: Lack, Sockel, Kunststoff, Meldeleuchten, Skalen, Ruß … |
| mach_pump | 1 | Boden (Wand) | 0.88 x 1.50 x 1.69 | 1752 | 9014 | 3 | 23: Motor, Gehäuse, Grundrahmen, Druckrohr, Schutzblech, Ölauge … |
| mach_tank | 1 | Boden (Wand) | 1.13 x 1.18 x 2.55 | 2624 | 6896 | 6 | 18: Kessellack (Textur), Schweißnähte, Rohr, Schieber, Messing … |
| pallet | 4 | Boden | 1.21 x 0.83 x 0.15 | 352 | 3154/5956/3140/6414 | 6 | 17: Holz (vier Töne), Brandstempel, Karton (trocken/innen/nass/Rand), Klebeband … |
| pipe_bundle_a | 1 | Boden (Wand, oben) | 0.32 x 5.00 x 0.48 | 976 | 7168 | 49 | 16: Rohre grau/rot/gelb, Flansche, Schieber, Kompensator, Manometer … |
| pipe_bundle_b | 1 | Boden (Wand, oben) | 0.28 x 5.00 x 1.12 | 732 | 5606 | 51 | 15: Alu-Mantel, Mineralwolle, Reparaturschelle, Kugelhahn … |
| pipe_riser | 1 | Boden (Wand) | 0.37 x 0.46 x 2.11 | 820 | 4190 | 195 | 16: Lack (Textur), Kennringe, Alu-Mantel, Wolle, Schieber … |
| rail_track | 1 | Boden | 5.06 x 1.07 x 0.16 | 936 | 7048 | 22 | 9: Schiene (Rost), Schwellen, faule Enden, Schotterbett, Steine … |
| stepladder | 1 | Boden | 0.70 x 0.46 x 1.63 | 312 | 2346 | 0* | 9: Alu, Kunststoff, Gummi, Aufkleber, Farbspritzer |
| toolbox | 1 | Boden | 0.50 x 0.24 x 0.29 | 308 | 1410 | 2 | 7: roter Lack (Textur), Stahl, Fett, Rost |
| trolley_flatbed | 1 | Boden | 1.34 x 0.77 x 0.95 | 756 | 4432 | 0* | 15: Lack, Sperrholz, Reifen, Zahnrad, Getriebe, Rohrstück … |
| tube_white / _orange / _dead | je 1 | Decke | 1.63 x 0.06 x 0.14 | 276 | 596 | 312/89/80 | 10: Leiste, Kappen, Fassungen, Glas leuchtend/tot, geschwärzte Enden, Alu-Kappen … |
| violet_lamp | 1 | Wand | 0.05 x 0.18 x 0.13 | 120 | 650 | 24 | 8: Gehäuse, Prismenscheibe violett, LED, Staub … |
| macchina_parts (neu) | 1 | Boden (0,6 m vor Wand, +x zur Wand) | 0.79 x 1.50 x 1.07 | – | 6548 | 4 | 18: Werkbank (Guss, Holz), Messing, Kupfer, Getriebe, Schraubstock, Ölkanne … |
| macchina_tools (neu) | 1 | Boden (0,6 m vor Wand, +x zur Wand) | 0.89 x 1.22 x 1.92 | – | 4388 | 2 | 16: Schrank, Lochplatte, Schattenumrisse, Werkzeug, Griffe … |
| pipe_run (geprüft) | 4 | Decke | 1.00 x 0.10 x 0.80 | – | 228/314/384/182 | 320 | Modell aus `models_l1_code.py` (Level 1), unverändert |
| pipe_wall (geprüft) | 2 | Wand | 0.26 x 0.37 x 0.59 | – | 1084/376 | 16 | Modell aus `models_l1_code.py` (Level 1), unverändert |

\* stepladder und trolley_flatbed stehen in der Liste `LOOSE` der Karte, wurden im aktuellen Zufallslauf aber nicht gesetzt;
Bögen mit Ersatzeintrag im Abzug.

**Dreiecke der Ebene:** Ausstattung gesamt 3,53 Mio. (`build/qa/props_level2.json`, 1837 Objekte, alle Klassen), vorher
0,88 Mio. Die vielfach gesetzten Klassen sind bewusst sparsam: Verteilerkasten 271 × 1,8k, Schaltschrank 196 × 3,1k,
Steigleitung 195 × 4,2k, Rohrbündel 100 × 5,6–7,2k, Röhren 481 × 0,6k.

**Übersichten je Runde:** `docs/screens/level2/props_uebersicht_vorher.jpg`, `…_r1.jpg` … `…_r5.jpg`,
`…_final.jpg`; kompakte Prüfbögen (zwei Nahansichten je Klasse, 50 cm im Licht und mit streifender Lampe):
`docs/screens/level2/props_pruefbogen_r2.jpg` … `…_r5.jpg`, `…_final.jpg`.

## Runden

### Vorher (V12-/Code-Fassung)
- barrel (Code): Mantel ohne Außenseite (Normalen falsch), keine Sicken/Falze. Unbrauchbar.
- cable_reel: zwei Scheiben und ein Zylinder. Keine Wicklung, keine Bolzen, kein Kabelende.
- chamfer_26: nackter Keil.
- door_*: saubere Innentüren ohne Zargenprofil, Dichtung oder Spuren. Sperrholz mit Sinus-Maserung. Holztür mit
  moderner Klinke.
- folding_chair, toolbox, stepladder, trolley_flatbed: Klötzchen und Stangen.
- hopper_wagon: Pyramidenstumpf auf einem Quader.
- junction_box, mach_cabinet: Quader ohne Beschläge.
- mach_pump: Motor mit umlaufenden Rippen (falsch), Rohre geknickt. mach_tank: Zylinder mit drei Ringen.
- pallet: keine Europalette. Kartons glatte Quader.
- pipe_bundle_a/b, pipe_riser: glatte Zylinder, Flansche als Scheiben, keine Schrauben, Handrad eine Scheibe.
- rail_track: Schienen aus drei Quadern.
- tube_*: flacher Kasten mit Leuchtfläche. violet_lamp: überstrahlter Kasten.

### Neubau und Testbögen (vor r1)
- Fass: Rost per Flächenzuordnung ergab Pixel-Tarnmuster. Ersetzt durch Flecken und Bänder mit unregelmäßigem Rand.
- Fass v0: Lacktextur zeigte die Würfelprojektion als senkrechte Naht. Jetzt schlichter Lack.
- Palette: Holzlook „wood“ zeigte Furnierfugen quer über die Bretter. Vergleich von vier Looks, gewählt wurde
  „concrete“ (sägerau).
- Kessel: Rostband als Streifen sichtbar. Ursache: Sehnen tauchten in den facettierten Mantel. Rostringe haben jetzt
  einen Versatz nach Facettenhöhe.
- Flanschpaar und Schieber: Flansche zeigten in die falsche Richtung. Korrigiert.
- Türen: Schilder lagen hinter den geprägten Kassetten. Jetzt liegen sie auf der Kassette, Griffspur kleiner.
- Schaltschrank: Glas ist im Spiel deckend. Deshalb dunkles Glas ohne Einbaugeräte dahinter.

### Runde r1 (alle 33 Bögen)
- Budget: 3,7 Mio. allein für Level 2. Flansche 14 statt 18 Segmente, Pumpe, Kessel und Steigleitung sparsamer.
- cable_reel: Holz grauer, Kabel entsättigt.
- chamfer_26: Läufer breiter und weicher.
- door_steel_grey: zu türkis, Tönung neutraler.
- folding_chair: Lehne wirkte schwebend. Lehne breiter, mit Laschen und Nieten.
- hopper_wagon: Schotter wirkte als Platte. Jetzt buckelig, dunkel, mit Steinen.
- rail_track: Bett heller, Rand ausgefranst, größere Steine.
- toolbox: Rot dunkler. trolley_flatbed: Boden dunkler, mehr Öl.

### Merge liminal-v13 (Tunnelnetz 15 m, Brownout, Macchina) und Runde r2
- Neue Stückzahlen: Verteiler 271, Schaltschrank 196, Steigleitung 195, Röhren 481. Damit 4,99 Mio. Dreiecke.
- Gesenkt auf 1,8k / 3,1k / 4,0k / 0,6k Dreiecke. Rohrbündel A/B auf 8,5k / 5,5k.
- Mittel dafür: weniger Schrauben, keine U-Bügel-Muttern, Ventilrad 4–5 Speichen, Schieber ohne eigene Flansche an der
  Steigleitung.
- macchina_parts und macchina_tools neu gebaut (die Karte setzte sie ohne Modell).
- Befund Macchina: Werkbankplatte und Ablage zu hell, Staubflecken weiß. Werkzeugschrank mit Lacktextur sah aus wie
  Tarnmuster. Säge ragte seitlich aus der Lochplatte.
  - Änderung: Holz dunkler, Staub dezent, Schrank schlicht, Lochplatte Hartfaser, Säge und Zangen neu gesetzt.
- Sperrholztür: Schrauben sparsamer (4,1k → 2,3k).
- pipe_run/pipe_wall (Level-1-Modelle) passen in die Hallen: grau bzw. gedämmt, Hänger, Wanddurchführung mit
  Rostfahne. Keine Änderung, weil Level 1 dieselben Modelle nutzt.

### Runde r3
- door_plywood: dunkle Flecken lasen sich als Löcher. Jetzt gealtertes Holz mit weichen Wasserläufern von oben.
- mach_cabinet: Seitenwände leer. Jetzt Lüftungskiemen und Rostfahne.
- pipe_bundle_b: Alu-Mantel zu sauber. Jetzt Staubbänder oben, innerhalb der Modullänge.

### Runde r4
- r4 lief beim ersten Versuch nur teilweise durch (Bilder fehlten). Wiederholt.
- mach_cabinet: unter Licht fast weiß. Lack dunkler, Rostfahne breiter und gedeckt.
- pipe_riser: Dämmungsriss wirkte wie Klebeband. Wolle gedeckt, dunkle Kante.
- door_plywood: Läufer heller.

### Runde r5
- Stichprobe im echten Level (`spot.py`, Spieler ausgeblendet): Die „3“ über der Tür nach Level 3 ist gut lesbar. Pumpe
  und Gleis/Wagen sitzen richtig.
- **Hauptbefund:** Die Karte setzt Schaltschränke und Steigleitungen unter die Rohrbündel (107 bzw. 100 von 100
  Bündeln). Dadurch liefen Bündelrohre, die tiefe Gussleitung und die Deckenrohre der Schränke durcheinander.
- Änderungen:
  - Alle Bündelleitungen liegen über 2,13 m, die Konsolen sitzen oben.
  - Die tiefe Gussleitung von A ist entfernt.
  - Steigleitungen enden mit Bogen und Wandrosette in der Wand unterhalb der Bündel (2,11 m statt 2,6 m).
  - Schaltschrank ohne Deckenrohre. Kabel laufen über das Dach in die Wand.
  - Dämmungsrisse sind als unregelmäßiges Loch mit Wolle ausgeführt.
- Nachweis: Stichprobe danach. Bündel über dem Schrank, Steigleitung biegt darunter in die Wand.

### Schlussrunde final
- Alle 39 Bögen angesehen: 33 eigene Klassen und Varianten, Macchina 2, pipe_run 2 (von 4 Varianten), pipe_wall 2.
  Keine neuen Fehler.
- Die Kabinen für Durchbruch und Ecke (door_level3, chamfer_26) zeigen im Studio vor allem Rückseiten. Geprüft wurde
  deshalb im Spiel (siehe r5).

## Prüfungen
- `python tools/qa/validate_models.py <alle eigenen Klassen>`: 41 Modelle, 0 Fehler. Gesamtlauf: 356 Modelle, 72 mit
  Fehlern. Die Fehler betreffen Wesen und andere Ebenen, dazu das ungenutzte V12-Modell `door_oversize`, das nicht neu
  gebaut wurde.
- `gameplay_test`: 525 Prüfungen, 0 Fehler. „Halt der Ausstattung“: level2 1507 Objekte, 0 ohne Halt; gesamt 10493, 0
  ohne Halt.
- `python tools/qa/room_metrics.py --check`: Raummetrik OK, Level 2 verstellt 0/181.

## Restmängel (ehrlich)
- **Kartenkollisionen**, nicht im Modell lösbar:
  - Die Deckenrohre der Verteilerkästen reichen bis 1,3 m über die Kastenmitte. Sie könnten Bündeln begegnen; in der
    aktuellen Karte gibt es dafür keinen Fall.
  - Bündel enden in der neuen Karte einzeln. Die Blindflansche an den Modulenden lesen sich als verschlossene
    Leitungsenden, nicht als durchlaufende Trasse.
- **Würfelprojektion** der Texturen: An Zylindern (Fass v1, Kessel, Steigleitung) bleibt bei Lacktexturen eine
  senkrechte Naht sichtbar. Bei Holz läuft die Maserung je nach Drehung quer zum Brett, darum wird für Holz meist der
  Look „concrete“ verwendet.
- **Glas ist deckend**: Ein Fenster mit Einbaugeräten dahinter war nicht möglich, das Schaltschrankfenster ist dunkles
  Glas.
- **Studiogrenzen**: Bei Wandobjekten, die die Karte als Bodenobjekte führt (Schrank, Pumpe, Kessel, Bündel,
  Steigleitung), steht im Studio keine Wand dahinter. Wandläufer schweben dort sichtbar in der Luft.
- **Stückzahl und Wiederholung**: Je Klasse gibt es eine Fassung. 271 gleiche Verteiler und 196 gleiche Schränke
  wiederholen sich sichtbar. Varianten wären besser, kosten aber Dreiecke.
- door_steel_grey ist durch die Lacktextur stark rostig, eher „rostig mit grauem Lack“ als „grau“.
- Die Tür nach Level 3 ist im Studio kaum zu beurteilen. Im Spiel wurde nur aus einer Richtung geprüft.

## Aufwand
- Etwa 13 Stunden Agentenzeit (27.10. ca. 9 h bis Runde r1, 01.11. ca. 4 h für Merge, Macchina, r2–r5, final, Tests).
- Runden: vorher, Testbögen, r1–r5, final (7 Kontaktbogen-Läufe über alle Klassen, dazu Einzeltests).
