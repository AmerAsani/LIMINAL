# Kreatur: Clump / Bündel (Entity 5)

Quelle: [backrooms-wiki.wikidot.com/entity-5](https://backrooms-wiki.wikidot.com/entity-5), gelesen am 26.10.2026.
- Autor: Kekcroc; Neufassung: TrailmixNCocoa.
- Lizenz CC BY-SA 3.0.
- Eigene Worte; kein Bild der Seite verwendet.

## Was die Seite sagt

**Erscheinung:**
- Ein Bündel menschlicher Glieder verschiedener Länge und Muskulatur.
- Größe des Körpers rund 0,9–1,2 m; das längste Glied meist 2,4–3 m.
- In der Mitte ein Maul mit rasiermesserscharfen Zähnen, das sich erst zeigt, wenn Beute herangezogen ist.
- Berichtet: Augen und Ohren, die aus der Masse der Glieder hervortreten.
- Haut: in der aktuellen Fassung klebrig und taufeucht (in der alten Fassung sehr trocken).

**Bewegung:**
- Schlägt mit den Gliedern auf den Boden und zieht sich so voran.
- Schnell, stark und wendig: springt, klettert an Wänden, kriecht durch Schächte.

**Verhalten:**
- Ständig hungrig, sucht Wanderer.
- Packt bis etwa 2,4 m mit dem längsten Glied und zieht die Beute zum Maul.
- Nach dem Fressen gräbt es sich ein; nur das längste Glied ragt heraus.

**Laute:** keine beschrieben.

## Umsetzung

**Silhouette:**
- Ein Knäuel aus ineinander verwachsenen Armen und zwei Beinen.
- Ein langer Arm (2,6 m) ragt heraus; die anderen sind kürzer und stützen.
- Von der Seite ein flacher, breiter Haufen auf vielen Gliedern, kein Spinnenrumpf.

**Anatomie:**
- Jedes Glied ist ein echter menschlicher Arm oder ein Bein: Oberarm mit Bizeps und Trizeps, Ellbogen, Unterarm mit Sehnen, Handgelenk, Hand mit fünf Fingern und Nägeln, Knie, Wade, Fuß.
- Verschiedene Menschen: Hauttöne hell bis dunkel, Behaarung und Muskulatur verschieden.
- Die Glieder sind an der Wurzel verdreht und verwachsen.
- Die Haut ist zwischen ihnen gespannt und runzlig, mit Nähten und Narben.
- In der Mitte unten eine quer liegende Spalte mit Lippenwülsten und Zähnen (geschlossen meist verborgen).
- Zwei, drei Augen und ein Ohr sitzen schief in der Masse (Bericht der Seite).

**Haut:** klebrig-feucht (aktuelle Fassung); glänzend in den Falten, Blutergüsse an den Verwachsungen.

**Bewegung:**
- Hand-über-Hand-Ziehen; der Kern schleift und schaukelt; die Beine schleifen nach.
- Der Köderarm tastet.

**Abgrenzung:** einziges Wesen aus vielen Gliedern; menschliche Haut wie der Hound, aber eine völlig andere Grundform.

## Prüfkriterien

- Jedes Glied als Arm oder Bein mit Gelenken und Hand bzw. Fuß lesbar, keine Schläuche.
- Verschiedene Hauttöne und Muskeln je Glied.
- Kern verwachsen, mit Spalte, Augen, Ohr.
- Feucht glänzend im Licht.

## Kritikrunden (Vorschau in Blender und Spielkamera)

Bilder: `docs/screens/creatures/clump/` (vorher: `docs/screens/creatures/vorher/clump_*`). Modell: `tools/blender/creatures/clump.py` (Skelett des Spiels: Kern, sieben Arme, zwei Beine).

| Runde | Befund | Änderung |
|---|---|---|
| 1 (Blender) | Erste Fassung: Füllmasse als Kugel mit zehn halb versunkenen Gliedern, sieben lebende Arme mit Muskeln, Händen (fünf Finger, Nägel, Daumenballen), zwei nachschleifende Beine mit Füßen auf dem Rücken, drei schiefe Augen, ein Ohr, Maulspalte mit zwei Zahnreihen. Las sich als Kugel mit dünnen Gliederpuppen-Armen; die Nähte auf der Masse wirkten wie ein Fußballmuster. | Masse kleiner, doppelt so viele tote Glieder (dicker), Arme fleischiger, Hände 15 % größer, keine Zellnähte mehr. |
| 2 (Blender) | Ein Knäuel aus Gliedern – aber an den Schultern standen Kugeln aus der Füllmasse ab, Ellbogen wie Kugelgelenke, ein nackter Bauch unter dem Knäuel. | Schulterwülste weg, Unterarm flacher mit Oberarmspeichenmuskel, Elle und Griffelfortsatz; vier Glieder unten herum. |
| 3 (Blender) | Tote Glieder als Kegelketten lasen sich wie Würste. | Tote Glieder mit derselben Anatomie wie die lebenden: die Schulter steckt in der Masse, der Ellbogen liegt auf ihr, die Hand krallt sich hinein; Beine mit herausragendem Fuß; Muskeln weich verschmolzen. |
| 4 (Spiel) | Erstmals im Spiel: drei Hauttöne, feucht glänzend im Lampenlicht, Hände und Füße lesbar. Mängel: runde Gesäßformen an toten Armen; Augen und Ohr unter Gliedern begraben. | Deltamuskel nur an lebenden Armen; Augen und Ohr auf der äußersten Fläche in fleischigen Wülsten (Strahltest am fertigen Feld); Falten strahlen vom Maul aus. |
| 5 (Spiel) | Die Gesäßform blieb. | Schultern der lebenden Arme flacher und länger. |
| 6 (Spiel) | Unverändert – also nicht die Schultern. Ein Strahltest vom Kamerapunkt fand die Ursache: zwei tote Arme mit prallen Oberarmen nebeneinander. | Tote Glieder ausgezehrt (sehnig, flache Muskeln, Strangsehnen) und schmaler. |
| 7 (Spiel) | Ein Knäuel aus verwachsenen menschlichen Gliedern in drei Hauttönen: lebende Arme mit Händen, die sich aufstützen, tote Arme und Beine, die sich um die Masse legen und in sie krallen, Füße, die herausragen; feucht glänzend im Lampenlicht; Maul unten verborgen. Abgenommen. | – |

**Bleibt offen:**
- Die Gliedlängen kommen aus dem Skelett des Spiels (Arme bis 1,3 m, der Köder 2,6 m). Dadurch wirkt der Clump von weitem weiter wie ein Wesen auf vielen Beinen. Eine andere Grundhaltung hieße, die Bewegung in `anim/creature.cpp` neu zu entwerfen; nicht in V13.
- Die Augen sitzen in Wülsten zwischen den Gliedern und sind aus der Nähe zu sehen, aus der Ferne kaum.
