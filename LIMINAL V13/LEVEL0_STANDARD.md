# Level-0-Standard (V13.3)

Stand 27.10.2026. Auftrag V13.3, Abschnitt 3: Level 0 bekam bisher den meisten Aufwand. Hier steht nur, was an Level 0
**gut** ist und deshalb für jede Ebene gilt. Mängel von Level 0 stehen nicht hier, sondern in `AUFWAND.md` und in der
Abnahme von Level 0 (eigene Runde nach Level 3).

Grundlage: Laufweg vorher (`docs/screens/level0/wegbilder_vorher_1.jpg`, `_2.jpg`, Karte `wegkarte_vorher.png`, 202 m
durch 25 Räume), feste Ansichten `docs/screens/level0/01…15`, `LOOK_0.md`, `LEVELSPEC_0.md`, Raummetrik
`docs/metrics/level0.json`.

## 1. Ein Leitbild, aus dem alles folgt

Level 0 hat einen Satz, an dem jede Entscheidung gemessen wird: ein Bürohinterhof, der nie fertig wurde. Wände, Boden,
Decke, Licht und Ausstattung erzählen dieselbe Geschichte (stumpfe Farbe, Feuchtigkeit von unten, kaltes Licht).
**Standard:** Jede Ebene hat in `LOOK_<N>.md` ein Leitbild in einem Satz, und jedes Objekt, jede Textur und jede Leuchte
lässt sich darauf zurückführen.

## 2. Wiederholung als Wirkung, nicht als Faulheit

Die Ödnis entsteht aus einer klaren Grundstruktur, die überall gleich bleibt (gegliederte Räume aus Wandscheiben,
Teppich, Rasterdecke), während sich nur Kleinigkeiten ändern: Breite der Durchbrüche, Versatz der Flure, tote Leuchten,
Feuchtigkeitsflecken. Kein Raum ist ein Wahrzeichen. Aehnlichkeit der Layouts gemessen: S = 0,89 (`tools/qa/similarity.py`).
**Standard:** Labyrinth-Ebenen haben eine durchgehende Grundstruktur; Variation nur im Kleinen; Zusatzzonen nur, wenn das
Wiki sie beschreibt (`STRUKTURTYP.md`).

## 3. Tiefe durch versetzte Öffnungen

Man sieht fast überall zwei bis drei Räume tief (Bilder 000, 002, 006, 021–026), aber nie durch eine gerade Röhre:
Durchbrüche liegen versetzt, Wandscheiben enden mitten im Raum, Stürze und Deckensprünge rahmen den Blick.
Sichtachsen: Median 22 m, 90-%-Wert 46 m.
**Standard:** Jede Ebene plant ihre Sichtachsen: lange Blicke, die sich staffeln, statt kurzer Kisten oder
endloser gerader Röhren. Mindestwerte je Ebene in `tools/qa/metric_limits.json`.

## 4. Glaubwürdige Maße

Decken 2,7–3,1 m (Büro mit Rasterdecke), Flure 1,8–2,6 m, Räume von Kammer bis Halle gemischt; Rasterdecke 0,6 m,
Leuchten 0,6 × 1,2 m exakt im Raster, Steckdosen über dem Sockel, Sockelleisten, Lüftungsgitter in echter Größe.
**Standard:** Maße kommen von echten Gebäuden (Bürobau, Tiefgarage, Hotel der 1930er, U-Bahnhof, Flughafen) und vom
Wiki; jede Ebene begründet ihre Zahlen in `STRUKTURTYP.md` und erfüllt den Raummetrik-Test.

## 5. Raumhierarchie ohne Raster

Nie zwei gleich große Räume nebeneinander; Kammern 3 × 4 m, Räume 6 × 9 m, einzelne Hallen 14 × 18 m, dazu das
Säulenfeld als weite Ausnahme. Höhenwechsel (+0,85 m Podest, −0,25 m Dunkelzone) gliedern zusätzlich.
**Standard:** Jede Ebene hat Groß, Mittel und Klein in einem erkennbaren Verhältnis (Kennwert „gross“ und
„max/Median“ der Raummetrik), keine gleichförmigen Zellen.

## 6. Licht mit Rhythmus

Leuchten in Reihen mit 2,4–3,6 m Abstand, nicht in jedem Feld; etwa jede neunte tot, jede zwölfte flackert; dazwischen
dunklere Zonen; kaltes, leicht grünes Weiß auf warmem Gelb. Bögen wärmer, Säulenfeld gleichmäßig, Dunkelzone ohne Licht.
**Standard:** Licht ist geplant (Rhythmus, Lücken, Farbe je Zone) und erzeugt Hell-Dunkel-Folgen entlang der Wege.

## 7. Material in Schichten

Raufaser mit Feuchtigkeitsrand (15–35 cm, dunkler), Teppich mit Wasserflecken, Sockel, leicht vergilbte Decke. Schmutz
nur dort, wo er entsteht (Ränder, Flecken), Flächen sonst sauber und stumpf.
**Standard:** Jede Fläche hat eine Grundschicht und eine Gebrauchsschicht an plausiblen Stellen; keine gleichmäßige
Verschmutzung, kein gleichmäßiger Glanz.

## 8. Sparsame, echte Details

Wenige Objektklassen, die aber überall stimmen: Rasterleuchte, Luftauslass, Wandgitter, Steckdose, Bogen mit Laibung.
Keine Möbel, keine Schilder, keine Raumzwecke – weil das Wiki keine nennt.
**Standard:** Ausstattung nur, wo das Wiki oder die Bauart sie begründet; jedes Stück ist ein echtes Modell
(`PROPS_<N>.md`, Kontaktbögen), nichts, was nicht dorthin gehört.

## 9. Wiki-Treue mit Begründung

Alle Teilbereiche von Level 0 stehen auf der Wikidot-Seite (Bögen, Säulen, Löcher, Dunkelzonen, rote Räume,
Manila-Raum); Abweichungen sind in `LEVELSPEC_0.md` begründet (feste Karte statt Umbau beim Wegsehen).
**Standard:** Jede Zone ist mit einer Stelle der Wikidot-Seite belegt (`STRUKTURTYP.md`, `BIOM_AUDIT.md`).

## 10. Prüfung wie bei Level 0 – für alle

Level 0 wurde aus festen Kameras vor und nach jeder Änderung verglichen, mit und ohne Lampe, über viele Runden.
**Standard:** Abnahmeschranke a–g des Auftrags für jede Ebene, mit gleich viel Aufwand (Eintrag in `AUFWAND.md`).
