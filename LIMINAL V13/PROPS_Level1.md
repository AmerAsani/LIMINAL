# PROPS Level 1 – Die Lagerdecks

## Teil A: Modelle aus Code

Die Ausstattung, die bis V13.2 in `src/props/prop_library.cpp` aus Quadern und Zylindern entstand, ist jetzt in
Blender gebaut: `tools/blender/models/models_l1_code.py`, exportiert nach `data/models/<name>[@N].glb`. Ein Modell
aus einer Datei überdeckt das gleichnamige Code-Modell. Gebaut sind **alle** Varianten, die der Code kennt (auch die,
die keine Karte setzt), damit keine Variante auf Variante 0 zurückfällt – dazu für den neuen Rohrverlauf der Karten
`pipe_elbow`, `pipe_wall` und `pipe_drop`. Insgesamt **14 Klassen, 40 Modelle**.

**Werkzeug** im Modul: `Kit` sammelt Geometrie je Material (ein Objekt je Material), Drehkörper (`lathe`),
Rohrführung mit Gehrung und parallel transportierten Rahmen (`sweep`), Profile (`prism`), Kästen mit Rundung (`bbox`),
Atlas-Flächen für Etiketten, Schilder und Schablonenschrift (`cell`, `text`), unregelmäßige Flecken und Laufspuren
(`blob`, `streak`, `tube_patch`), Rohrmäntel mit Material je Viereck (`tube_zones`). Gekrümmte Flächen tragen
analytische Normalen – ein Rostfleck auf einem Rohr erzeugt keine Schattierungskante, Knicke im Profil werden hart.

**Drei Befunde aus der Engine**, die für alle künftigen Modelle gelten:

1. Die Ausstattung wird mit **Rückseiten-Culling** gezeichnet (`render/renderer.cpp`, `rsCullBack`). Einseitige
   Flächen (Lamellen, Kartonlaschen, Folie, Abziehbilder) verschwinden von hinten. Abziehbilder müssen zur richtigen
   Seite zeigen, dünne Bleche brauchen Vorder- und Rückseite (`poly(n=…, two=True)`).
2. Der Import **spiegelt y** (Blender +y = Spiel −y, `props/glb_import.cpp`). Für gerichtete Modelle (Rohrbogen)
   entscheidend; Schrift erscheint nicht gespiegelt.
3. **Schablonenschrift** (`look=stencil`) schneidet alles unter 45 % der Materialhelligkeit weg, mindestens aber
   unter 0,0225 (linear): Druckfarben dunkler als sRGB ≈ 0,18 verschwinden ganz; Zeichen unter ≈ 1 cm fallen in hohen
   Mip-Stufen weg. Druck daher sRGB 0,19–0,22 und ≥ 1,2 cm.

### Klassen

Maße als Grenzen im Spiel (x vorn, y rechts, z oben; Ursprung wie im Code). Dreiecke vorher aus `propdump`
(Code-Modell), nachher aus dem Export. Bögen: `docs/screens/props/level1/<runde>/<name>[_vN].jpg`.

| Klasse | Varianten | Befestigung | Grenzen nachher (m) | Dreiecke vorher → nachher | Materialien (Auswahl) |
|---|---|---|---|---|---|
| `batten` | 0 an, 1 tot (dunkle Röhrenenden) | Decke, hängt an 2 Seilen 0,35 m tiefer | x −0,76…0,82 (Kabel), y ±0,07, z −0,45…0 | 172 → 353 / 407 | Wanne, Endkappen, Diffusor in 3 Leuchtzonen, Edelstahlklammern, verzinkte Bügel, Kabel, Insekten, Staub, Typschild |
| `ceiling_vent` | 0 weit offen, 1 schief an einem Scharnier, Lamelle fehlt | Decke (Öffnung 0,6 × 0,6) | ±0,36, z −0,57…0 | 244 → 436 / 428 | Gehrungsrahmen mit Laibung, Schacht, Lamellen beidseitig, Scharniere, Schrauben (eine fehlt), Russhof |
| `chair` | 0 steht, 1 umgekippt | Boden | ±0,23, z 0…0,94 | 104 → 801 | Buche (Fototextur), konische gefaste Beine, Zarge, Streben, gebogene Lehnenbretter, Gleiter/Filz, abgegriffene Sitzkante, schmutzige Füße |
| `desk_crt` | 0 aufgeräumt, 1 Ordner/2. Stapel/Stuhl gedreht, 2 ohne Stuhl, Schublade offen, Papier am Boden, Tasse umgekippt | Boden | x −0,35…0,95 (v2 0,43), y ±0,70, z 0…1,13 | 418 / 418 / 238 → 3127 / 3219 / 1951 | Resopal-Platte mit Umleimer, Stahlgestell, Sichtblende, Schubladenblock, Röhrenmonitor (gewölbte Scheibe, Lüftungsschlitze, Schalter, Kabel), Tastatur mit einzelnen Tasten, Papiere, Tasse, Kaffeeringe, Drehstuhl mit Rollen |
| `gauge` | 0 schwarz, 1 Edelstahl (roter Bereich), 2 Messing alt (Zeiger am Anschlag), 3 schwarz mit Wasserrand | Wand (Rückseite 2 cm vor der Wand) | x −0,10…0,06, ±0,08 | 172 → 731 / 695 / 738 / 739 | Gehäuse mit Bajonettring, Zifferblatt mit 51 Teilstrichen, Ziffern, rotem Bereich, Zeiger mit Gegengewicht, Nabe, Anschlag, Sechskant, Gewinde, Rosette, Abplatzer, Rost, Fettfilm; leicht schief |
| `highbay` | – | Decke (Pendel) | ±0,27, z −1,00…0 | 224 → 1584 | Deckendose, Kugelgelenk, Pendelrohr mit Muffe, Gussgehäuse mit 10 Rippen, Klemmkasten, Reflektor (Emaille außen mit Staub, facettiertes Aluminium innen), Rollrand, Klemmring, gewölbtes Glas, Fangseil |
| `pipe_run` | 0 Stahl mit Schelle, 1 gedämmt mit Schelle, 2 Stahl mit Muffe, 3 gedämmt aufgerissen | 1-m-Stück, Achse im Ursprung, Stange bis z 0,75 | x ±0,50, r 0,045 / 0,070 | 52 → 228 / 314 / 384 / 182 | verzinkt, Anlauf, Flugrost, Rost an Schelle/Gewinde, Temperguss-Muffe, Alu-Mantel mit Überlappung, Längsnaht, Niete, Mineralwolle, Staub, Tropfen |
| `pipe_elbow` (neu) | 0/1 nach +y, 2/3 nach unten | Anfang im Ursprung | 0,5 × 0,5 | – → 444 / 388 | Temperguss-Bogen mit Wulsten, Gewinderost; Segmentbogen mit Nähten |
| `pipe_wall` (neu) | 0 Stahl, 1 gedämmt | Wand, sitzt über dem ersten `pipe_run` | x 0…0,26, r 0,10 | – → 1084 / 376 | Rosette, Flanschpaar DN 80, 4 Schrauben, Dichtung, Rost, Feuchte- und Rostfahne an der Wand |
| `pipe_drop` (neu) | 0/1 mit Wandschelle, 2/3 frei | senkrecht, z ±0,5 | Wand bei x −0,15 | – → 466 / 580 / 120 / 234 | wie `pipe_run`, Wandschelle mit Gewindestange und Wandteller |
| `power_box` | – | Wand (Rückseite x = 0) | x 0…0,24, y ±0,31, z −0,60…1,60 | 98 → 1351 | Stahlblech RAL 7035, Tür mit Scharnieren und Vierkantverschlüssen, Warnschild, Schildhalter, Kiemen, Verschraubungen, Panzerrohr mit Muffe und Abstandsschelle bis zur Abzweigdose, Leitungen an Clips in die Wand, abgeschnittenes Kabel mit Kupferadern, Rostläufer, Abplatzer, Staub |
| `rack` | 0 Kartonlager, 1 gewickelt, 2 ausgeräumt/offen, 3 Wasserschaden, 4 Kisten, 5 hoch gestapelt, 6 gemischt, 7 fast leer; v ≥ 4 mit Endrahmen, Anfahrschutz, Lastschild | Boden (Doppelreihe 2,7 × 2,4 m) | x −1,42…1,30 (v ≥ 4: 1,42), y ±1,21, z 0…5,40 | 1496…2436 → 3114…5456 (Mittel ≈ 4300) | C-Pfosten mit Lochung, Zickzack-Aussteifung, Fußplatten mit Ankern, Stufentraversen mit Einhängeplatten, Sicherungen, Fachetiketten, Schablonennummern, Staplerkratzer, Europaletten, Kartons, Stretchfolie in Lagen, Holzkisten, Flachkarton-Bündel |
| `safety_light` | – | Decke | ±0,19 × ±0,08, z −0,14…0 | 112 → 333 | vergilbtes Gehäuse, grüner Diffusor mit Endstücken, Ladeanzeige, Prüfknopf, Notausgang-Piktogramm beidseitig, Schrauben, Typschild, Insekten |
| `side_table` | 0 leer, 1 mit Tischlampe (Level 5) | Boden | ±0,30, z 0…0,66 (v1 1,15) | 164 → 1148 / 1968 | Nussbaum (Fototextur), profilierte Platte, abgeriebene Kante, gedrechselte Säule, drei geschwungene Füße mit Messingschuhen, Wasserringe, Kratzer; Lampe: Messingfuß, Faltenschirm innen/außen, Spinne, Kabel |

**Abweichungen der Grenzen** (alle begründet): `power_box` z bis 1,60 wie das Code-Modell, aber das Panzerrohr endet
nicht mehr in der Luft, sondern führt in eine Abzweigdose und in die Wand. `pipe_run` hat eine Gewindestange bis
0,75 m über der Achse (statt Halter bis 0,30) und verschwindet so in jeder Decke der Karte (Abstände 0,12–0,68 m).
`desk_crt@2` ragt 8 cm weiter vor (offene Schublade). Kartonlaschen bleiben innerhalb des Regals (Gasse frei).

### Kartonmaßstab (Regal)

Jeder Karton hat Außenlaschen mit Stoß entlang der langen Seite (leicht gewölbt oder eingesunken), Klebeband über den
Stoß und an den Stirnseiten herunter (braun, klar, selten rot), die Herstellernaht an einer Kante, Etikett (Atlas-Zettel)
und Schablonendruck zur Gasse, Dellen (eingedrückte Seite) und Kantenschäden (gestauchte Ecke), Papierfaser über den
Look `cardboard`. Zustände: **zugeklebt**, **offen** (Laschen beidseitig, Innenwände, Boden), **durchnässt** (dunkle
untere Hälfte mit unregelmäßiger Wasserlinie, gewölbte Seiten, eingesunkener Deckel, abgelöstes Band, nasse Palette,
schiefer Stapel), **gestapelt** (1, 2 oder 4 je Lage, bis 4 Lagen, versetzt), dazu Stretchfolie in Lagen und
Flachkarton-Bündel mit Umreifung.

## Rohrverlauf für die Karten (Spielkoordinaten)

| Modell | Varianten | Ursprung / Achse | Anschluss |
|---|---|---|---|
| `pipe_run` | 0 Stahl verzinkt r 0,045 mit Hängeschelle · 1 gedämmt (Alu-Mantel) r 0,070 mit Hängeschelle · 2 Stahl mit Temperguss-Muffe, ohne Schelle · 3 gedämmt, Mantel oben aufgerissen, ohne Schelle | Rohrachse durch den Ursprung, entlang +x von −0,5 bis +0,5 | Stücke im Abstand 1,0 m, **scale 1**, yaw 0 (entlang x) oder π/2 (entlang y). 0 und 2 bzw. 1 und 3 haben gleiche Enden und lassen sich mischen |
| `pipe_elbow` | 0 Stahl (Temperguss-Bogen) · 1 gedämmt (Segmentbogen) · 2/3 dieselben nach unten | Anfang im Ursprung, Rohr kommt entlang +x | 0/1: Ende bei (0,5; 0,5; 0) Richtung +y → nächstes `pipe_run` mit Mitte (0,5; 1,0), yaw + π/2. 2/3: Ende bei (0,5; 0; −0,5) Richtung −z → `pipe_drop` mit Mitte (0,5; 0; −1,0) |
| `pipe_wall` | 0 Stahl · 1 gedämmt | Wandfläche bei x = 0, Rohr entlang +x | enthält **kein eigenes Rohr**: sitzt wie in `level1.py` (`x0 + 0,004`, yaw 0 bzw. π) über dem ersten Stück des Laufs; Rosette r 0,085 (gedämmt 0,098), Flansch DN 80 (r 0,10) bei x 0,10–0,195 |
| `pipe_drop` | 0 Stahl mit Wandschelle · 1 gedämmt mit Wandschelle · 2 Stahl frei · 3 gedämmt frei | senkrecht, z −0,5 … +0,5, Achse durch den Ursprung | Stücke im Abstand 1,0 m übereinander; Wandschelle nach lokal −x, Wandfläche bei x = −0,15 (yaw so wählen, dass −x zur Wand zeigt); gedämmte Stücke überlappen unten (Wasser läuft ab) |

- **Höhe:** Die Gewindestange reicht bis z = +0,75 über der Achse: Rohrachse 0,12–0,75 m unter die Decke setzen.
- **Gestapelte Rohre** (Rohrgang, `j // R.h ≥ 1`): die untere Lage mit Variante 2/3 (ohne Schelle) setzen, sonst geht
  ihre Stange durch das obere Rohr.
- **Muster:** 0 mit 2 mischen (z. B. 0, 2, 0, 0, 2, …) → Schellen alle 1–2 m, Muffen dazwischen; 1 mit seltenem 3.
  Die Karte setzt derzeit nur 0/1 – das ist gültig (Schelle je Meter), wirkt aber gleichförmiger.
- **Wandabstand:** waagrechte Läufe frei (Halt über die Decke), empfohlen ≥ 0,15 m (Stahl) bzw. ≥ 0,20 m (gedämmt);
  Fallrohre genau 0,15 m. **Bündel:** Achsabstand ≥ 0,25 m; an Ecken braucht jedes Rohr seinen eigenen Bogen.
- **Offene Enden** haben keine Kappe – jeden Lauf an Wand, Bogen oder Fallrohr in den Boden enden lassen.
- Nachweis im Rohr-Studio (eigenes Skript, gleiche Kamera/Lampe wie die Bögen): `docs/screens/props/level1/<runde>/pipe_verlauf.jpg`.

## Bögen

- **Vorher:** `docs/screens/props/level1/vorher/*.jpg`, Übersicht `docs/screens/level_1/props_uebersicht_A_vorher.jpg`.
- **Final:** `docs/screens/props/level1/final/*.jpg`, Übersicht `docs/screens/level_1/props_uebersicht_A_final.jpg`,
  Prüfbogen `docs/screens/props/level1/final/pruefbogen.jpg`, Rohrverlauf `…/final/pipe_verlauf.jpg`.
- **Runden:** Prüfbogen je Runde `docs/screens/props/level1/r1…r5/pruefbogen.jpg` (je Klasse 50 cm im Licht und mit
  streifender Lampe), Rohrverlauf `r1/`, `r2/`, `r3/pipe_verlauf.jpg`.
- Die Übersichten heißen `…_A_…`, damit sie neben denen von Teil B stehen können. Bis Runde 2 liefen die Bögen in
  Gruppen (Kabinen weit vom Spieler blieben schwarz), ab Runde 3 mit dem behobenen `propsheet.py` in einem Lauf;
  `pipe_run`-Bögen ab Runde 2 zeigen das 1-m-Stück der neuen Karte (davor: ein auf 16 m gedehntes Stück).

## Runden

**Vorher** (Code-Modelle): `batten` Kasten mit flachem Leuchtstreifen, 8 mm dicke Seile; `ceiling_vent` Rahmen aus
vier Quadern, flache Stege; `chair` Vierkantbeine, Brett als Lehne, grell orange; `desk_crt` Kasten-Monitor, Tastatur
als Leiste, Papier zeigt Atlas-Buchstaben, v0 = v1; `gauge` Scheibe ohne Teilung; `highbay` Kegel und Zylinder;
`pipe_run` ein Zylinder, den die Karte auf 12–116 m dehnte (Halter 1 m breit); `power_box` Quader mit Strich als Tür,
Rohr endet in der Luft; `rack` Kartons als glatte Kästen, Folie als graue Klötze, Varianten kaum unterscheidbar;
`safety_light` Quader mit grünen Streifen; `side_table` Zylinder-Platte, Bodenscheibe mit Lücken.

**Runde 1** (erste Blender-Fassung, Rohr-Studio): Formen deutlich besser. Grundbefund **Culling**: Lamellen,
Laschen, offene Kartonwände, Klebeband bei quer liegendem Stoß, Gabelöffnungen und Lochung auf einer Seite fehlten
von hinten. Rost (`metal_rusty_iron`) rot und als Streifen je Meter („Zuckerstange“), Rostfahnen an Wänden wie Blut;
Alu-Mantel unter der Lampe schwarz; Schaltkasten mit `PaintedMetal014` rot zerfressen; Holz grell orange; Folie weiße
Klötze; Kisten wie Kartons; Manometer-Fettfleck riesig. Dreiecke der Ebene 3,07 Mio. (neue Karte: 1406 Rohrstücke).
→ `poly(n=, two=)`, offene Kartons mit Innenwänden, Rost braun und sparsam, Wandfahnen matt, Lack ohne Fremdtextur.

**Zusammenführung** mit `liminal-v13`: Die Karte setzt Rohre jetzt aus 1-m-Stücken, `pipe_wall` über dem ersten
Stück → `pipe_wall` ohne eigenes Rohr umgebaut; Gewindestange 0,75 m (Abstände bis 0,68 m).

**Runde 2:** Lamellen und Laschen von allen Seiten, offene Kartons glaubwürdig, Folie in Lagen (unten/oben dicht,
dazwischen scheinen Kartons durch) liest sich als Stretchfolie, Kisten als Holz. Neu: Schablonenschrift unsichtbar
(Shader-Schwelle), Holz weiter orange (prozedurale Maserung), Alu-Mantel unter Lampe weiter schwarz (Textur voll
metallisch), Schaltkasten aus dem Bild (Rohr bis 2,75 m verschiebt den Ausschnitt).
→ Druckfarben heller, Fototextur `wood_walnut_floor` für Stuhl/Beistelltisch, Resopal-Platte am Schreibtisch, Alu
als `galvanized` m 0,25, Abzweigdose; Dreiecke gespart (Paletten ab 1,7 m vereinfacht, Lochung bis 1,5 m, Schelle
ohne Kontermutter, Leuchtband-Klammern ohne Stirnflächen).

**Runde 3:** Holz und Platte stimmig, Alu hell, Kasten im Bild, Kistenschrift sichtbar. Manometer-Ziffern weiter
unsichtbar (zu klein für die Schnittgrenze), Wandfahne an der Durchführung harter schwarzer Fleck, Fallrohr mit
dunklem Längsstreifen; Varianten ohne Kartenvorkommen (`chair@1`, `ceiling_vent@1`, `gauge@2/3`, `batten@1`) in
der Blender-Vorschau: Flecken auf `gauge@2/3` zu groß. → Ziffern 12,5 mm, Fahne zweistufig weich, Streifen dezent,
Notleuchte vergilbt, Flecken kleiner.

**Runde 4:** Ziffern lesbar, Fahne weich. `side_table@1` im Studio von Level 5 geprüft (Faltenschirm, Kabel, Messing –
stimmig). Regal wirkt zu neu → Anfahrspuren am Pfostenfuß.

**Runde 5:** keine sichtbaren Mängel mehr in den Bögen; Grenzprüfung ergab Kartonlaschen bis 0,33 m in die Gasse und
bis 5,60 m Höhe → Laschen klappen innerhalb der Regalgrenzen (steiler, weiter außen oder nach innen gefallen).

**Final:** Bögen und Übersicht wie oben; Prüfungen siehe unten.

## Dreiecke

Ebene 1 (neue Karte, `propdump`): Ausstattung gesamt **2,71 Mio.**, davon Teil A **1,82 Mio.** (Regal 0,95 Mio. bei 221 Feldern,
Rohrstücke 0,38 Mio. bei 1406 Stück, Leuchtbänder 0,18 Mio. bei 502 Stück, Hallenstrahler 0,10 Mio., Rest 0,22 Mio.). Vorher (alte Karte,
Code-Modelle) war Teil A 0,55 Mio. bei 1,40 Mio. gesamt. Die anderen Klassen (Teil B) lagen beim Messen bei
0,88 Mio.; bleibt die Ebene unter 4 Mio., solange Teil B unter ≈ 1,3 Mio. zusätzlich bleibt.

## Prüfungen

- `python tools/qa/validate_models.py` für alle 40 Modelle: 40 geprüft, 0 mit Fehlern.
- `build/Release/tests/gameplay_test.exe data`: OK, 446 Prüfungen, 0 Fehler (Halt der Ausstattung: Level 1 1477 Objekte, 0 ohne Halt; alle Ebenen 8976, 0).

## Restmängel (ehrlich)

- `pipe_wall` ist nur zusammen mit einem `pipe_run` vollständig; im Einzelbogen hängt der Flansch frei an der Wand.
- Die Karte setzt nur `pipe_run` 0/1: eine Schelle je Meter, keine Muffen, keine Bögen/Fallrohre – wirkt gleichförmig
  (Muster und Varianten oben). Gestapelte Rohre im Rohrgang: Stange der unteren Lage geht durchs obere Rohr.
- Insekten im Leuchtband gehen im Bloom unter; die Leuchtzonen sind nur schwach zu sehen.
- Holz von Stuhl und Beistelltisch ist eine Dielen-Fototextur: auf der Sitzfläche sind Fugen zu ahnen.
- Kartons ohne echte Transparenz der Stretchfolie (Lagenfarben täuschen sie vor); Etiketten sind unleserliche
  Atlas-Zettel; Kartondruck nur in Schablonenschrift (Großbuchstaben, Ziffern).
- Feine Gewinde (Gewindestange, Manometeranschluss) sind nur als Ringe angedeutet.
- `rack` liegt mit ≈ 4300 Dreiecken je Feld (221 Felder ≈ 0,95 Mio.) am oberen Rand; weitere Einsparung ginge nur über
  sichtbare Vereinfachung der Kartons.
- `ceiling_vent`: der Schacht ist eine schwarze Fläche (die Decke hat kein Loch), Tiefe nur 2,5 cm Laibung.

## Aufwand

Wandzeit ≈ 10 h (27.10.2026, 06:15–16:35, mit Pausen durch die Nutzungsgrenze); Runden: vorher, r1–r5, final
(7 Bogenläufe à ≈ 15 min, dazu 6 Läufe des Rohr-Studios und eine Blender-Vorschau der Varianten ohne Kartenvorkommen).
