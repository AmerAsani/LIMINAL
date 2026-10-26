# Kreatur: Deathmoth (Entity 4) und Nest

Quelle: [backrooms-wiki.wikidot.com/entity-4](https://backrooms-wiki.wikidot.com/entity-4), gelesen am 26.10.2026.
- Neufassung: DrAkimoto; ursprünglich Kekcroc (Fandom).
- Lizenz CC BY-SA 3.0.
- Eigene Worte; kein Bild der Seite verwendet.

## Was die Seite sagt

**Kasten und Größen:**
- Gewöhnliche Deathmoth (etwa 80 %): Männchen 30–35 cm Spannweite, 40–50 mm Körper; Weibchen 40–45 cm Spannweite, 60–70 mm Körper.
- Prätorianer (etwa 15 %): fast 1 m hoch, Flügel 60 cm, acht Beine mit Gift.
- Kaiserin: rund 3 m Spannweite.
- Larven, Eier.

**Gewöhnliche Weibchen:**
- Zwei Paar häutige Flügel mit Schuppen und feinen Fasern.
- Zwei Facettenaugen und zwei Punktaugen; gefiederte Fühler.
- Sechs gegliederte Beine mit Krallen.
- Kräftigere Brust und Hinterleib als die Männchen; verlängerte Mandibeln zum Reißen; ein einziehbarer Legebohrer.
- Eine ätzende Flüssigkeit.
- (Die Männchen haben stattdessen einen nadelartigen Rüssel.)

**Farben:** nicht beschrieben.

**Nest:**
- Halbe organische Bauten aus verdautem Material, Pflanzenfasern und abgesondertem Harz; innen feucht.
- Schichten 60–100 cm dick (Hauptnest). Das Harz konserviert.
- Säulen aus unverdaulichen Resten, mit derselben Masse überzogen.
- Kennzeichen: fleischige Wucherungen über Türen, Schwärme von Weibchen.

**Verhalten:**
- Helles Licht lockt sie an.
- Weibchen jagen im Schwarm; nahe am Nest feindseliger.
- Unter Stress rufen Lockstoffe weitere herbei.

**Laute:** nicht beschrieben.

## Umsetzung

**Welche Motte:** das gewöhnliche Weibchen (Spannweite 44 cm, Körper 68 mm), wie in V12.
- Neu gegenüber V12: Mandibeln und Legebohrer statt Rüssel und Schwanzborsten.
- Grund: Die Wikidot-Seite nennt beim Weibchen Mandibeln, den Rüssel nur beim Männchen.

**Silhouette:**
- Breite, leicht gezackte Vorderflügel, kleinere Hinterflügel.
- Ein kurzer, dicker, behaarter Körper.
- Gefiederte Fühler fast so lang wie der Körper.

**Anatomie:**
- Brust kräftig mit dichtem Haarpelz.
- Hinterleib in deutlichen Ringen mit Härchen; an der Spitze der Legebohrer (eingezogen).
- Kopf klein mit großen, gewölbten Facettenaugen (Facettenstruktur sichtbar), zwei Punktaugen dazwischen; kurze, gebogene Mandibeln.
- Beine dreigliedrig mit Dornen und Krallen.

**Flügel:**
- Dünne Platten mit Adern.
- Muster (eigene Wahl, Seite schweigt): staubiges Graubraun mit dunklen Binden, Augenfleck und hellem Saum.
- Schuppen als Mikrostruktur; am Rand leicht durchscheinend.

**Nest:**
- Schichtweise aufgebaute Masse über einem Türrahmen. Lagen wie Wachs und Harz, darin Fasern, Knochenstücke, Stoffreste.
- Bernsteinfarbenes Harz glänzt nass, an den Rändern tropfenförmig.
- Statt Blasen: Wulste der Schichten, Waben-Kammern (Gelege), Fäden zwischen Wand und Masse.

**Bewegung:** Flug mit zwei Flügelpaaren (wie V12), Sitzen mit angelegten Flügeln, Zittern vor dem Abflug.

**Abgrenzung:** einziges Insekt; Chitin, Schuppen und Haar statt Haut.

## Prüfkriterien

- Nah (unter 1 m): Facettenaugen, Fühlerkämme, Mandibeln, Beinglieder und Dornen erkennbar; Flügeladern und Muster.
- Im Lampenlicht: Flügel leicht durchscheinend, Körperhaar heller.
- Nest: Schichten, Harz, Reste, Gelege; keine Blasenwülste.

## Kritikrunden (Vorschau in Blender und Spielkamera)

Bilder: `docs/screens/creatures/moth/` und `docs/screens/creatures/nest/` (vorher: `docs/screens/creatures/vorher/moth_*`, `nest_*`). Modelle: `tools/blender/creatures/moth.py`, `nest.py`.

### Motte

| Runde | Befund | Änderung |
|---|---|---|
| 1 (Blender + Spiel) | Neu gebaut: Brust mit Haarbüscheln, Kopf mit gewölbten Facettenaugen (Facetten als Geometrie) und zwei Punktaugen, gekreuzte Mandibeln mit Innenzähnen, behaarte Taster, doppelt gekämmte Fühler, dicker Hinterleib mit Haarsäumen und eingezogenem Legebohrer, Beine mit Hüfte, Schenkel, Schiene mit Dornen und Spornen, fünfgliedrigem Fuß und Doppelkralle. Flügel als gewölbte Platten mit erhabenen Adern; der Umriss folgt genau `render/moth_wing.hpp` – im Spiel sitzt das Flügelmuster (Binden, Augenfleck, heller Saum) genau auf der neuen Form. Mängel: Flügel wie Fächer gefaltet, Hinterleib wie eine Raupe aus Ringen, Brustpelz stachelig; bei der Reduktion fraßen die großen Flügel den Großteil der Dreiecke, Augen und Pelz wurden grob. | Falten auf ein Drittel, Ringe weicher, Pelz dichter (700 Büschel), kürzer und nach hinten gekämmt, Haarsäume anliegend; neue Gewichtung der Stufen (`lod_weight`: Flügel 0,12, Fühler 2, Pelz 1,6, Augen 1,5). Nahkamera auf den Boden (die Motte sitzt dort). |
| 2 (Spiel) | Im Spiel richtig gerahmt: eine echte Motte – Muster mit Binden und Augenfleck auf den Flügeln, Fühler mit sichtbarem Kamm, Taster, Mandibeln, gegliederte Beine mit Dornen, pelzige Brust, im Lampenlicht hell. Mängel: die Facettenaugen wirkten wie Brillengläser in einer Fassung aus Kopfhaar; zwei dunkle Punkte oben bildeten mit den Augen ein Gesicht. | Augen weiter nach außen gewölbt und etwas größer, Punktaugen winzig. |
| 3 (Spiel) | Augen als dunkle, glänzende Kuppeln. Die beiden Punkte oben sind die Fühleransätze (anatomisch richtig). Abgenommen. | – |

**Bleibt offen:** Im Lampenlicht aus nächster Nähe wirkt der Brustpelz fast weiß (Pelztextur der Wesen); aus Spielerabstand stimmig.

### Nest

| Runde | Befund | Änderung |
|---|---|---|
| 1 (Blender) | Neu gebaut statt Blasenwülsten: Masse aus übereinander gegossenen Lagen über einem Türrahmen, am Sturz am dicksten, auf Wand und Boden ausgelaufen; darin Knochen mit Gelenkenden, Rippen, Stofffetzen, Pflanzenfasern; Wabe mit Eiern am inneren Pfosten; Harztropfen und -fäden; fleischige Wucherung über der Öffnung, zerrissene Haut darunter. Die Lagen sahen aus wie ein Stapel Teller. | Kern in den Pfosten, Lagen überlappen, verschmelzen und neigen sich. |
| 2 (Blender) | Organischer, aber die Haut unter dem Sturz wirkte wie gelochte Platten. | Risse statt runder Löcher, ausgefranste Ränder. |
| 3 (Spiel) | Im Spiel feucht, rotbraun, glänzend im Lampenlicht. Die Pfosten wirkten wie Schrauben (zu gleichmäßige Lippen), die Hautreste wie rechteckige Pappe. | Flachere Lippen, senkrechte Läufe über die Lagen, Hautreste als dunkle, zum Ende auslaufende Lappen (`ent_mouth`). |
| 4 (Spiel) | Herabhängende Lappen und Tropfen wie Harz und Fleisch, Lagen weicher. Abgenommen. | – |

**Bleibt offen:** Die Schichtung der Pfosten bleibt deutlich gleichmäßiger als gewachsene Masse. In der Ebene (an einer Tür zwischen Wänden) fällt das weniger auf als frei stehend in der Vorführung.


### Nest – Neuentwurf V13.2 (DECISIONS E-026, E-029)

Grundlage ist die Entity-4-Seite (Revision 6): Nester in vorhandener Architektur, Schichten aus verdautem Material, Pflanzenfasern und Harz, Säulen aus Resten, Eier in Säulen, Larven in den Wänden, Knochen rund um den Stock, „fleischige Wucherungen über Türen“ als Warnzeichen. Aufbau (`tools/blender/creatures/nest.py`):
- **Flur:** Wucherung quillt aus der oberen Ecke über den Sturz, läuft zur anderen Seite dünner aus. Flache Abläufe an den Pfosten, gewundene Adern auf der Wand, ein unregelmäßiger Krustenrand am Rahmen.
- **Laibung:** zusammenhängende Kruste, oben dicker. Eier in zwei versetzten Reihen halb in der Kruste, Harztropfen vom Sturz.
- **Schleier:** zerrissene Lage aus Fasern und Harz über der Öffnung, mit einem Einriss von unten. Darunter hängen Fäden mit Harzperlen zwischen den Seiten durch, einige gerissen.
- **Zimmer:** Kragen aus Lagen bis 0,9 m tief, oben fleischig mit atmenden Säcken. Zwei Säulen aus Resten mit herausragenden Knochen und Metall, Puppen in der Masse, Knochen vor der Tür.
- **Knochen und Animation:** neun Knochen (`ns` in `anim/creature.hpp`). Säcke atmen, der Schleier schwingt an der Angel, die Wucherung pulsiert, das Gelege zuckt; im Lampenlicht und bei Alarm schneller (`CreatureAnimator::updateNest`).

Bilder: `docs/screens/nest/` (vorher `v0_nest_sheet.jpg`; je Runde `nN_nest_sheet.jpg` mit festen Kameras nah, mittel, fern, schräg, innen dunkel, jeweils mit und ohne Lampe; Ausschnitte `nN_ml_crop` = Flur mit Lampe, `nN_il_crop` = Zimmer mit Lampe).

| Runde | Befund | Änderung |
|---|---|---|
| 0 (vorher, V13) | Vom Flur ohne Lampe unsichtbar, die Wucherung sitzt nur im Zimmer; rosa Kern, Bläschen | Neuentwurf nach Entity 4 |
| 1 | Vom Flur zu sehen. Rosa Wolke über dem Sturz, spitze Kegel wie Zähne (Maul), gekachelte Haut, rosa gestreifter Kragen | eigenes Material `ent_nestflesh`, erdige Textur, stumpfe Lappen, glatte Haut |
| 2 | Erdiger, keine Wolke mehr. Zahnreihe am unteren Hautrand (von innen ein Gebiss), Haut überstrahlt im Licht, Wucherung als symmetrische Schokoladenwolke, Abläufe als Kartoffelkette, Eier als freie Perlenschnüre | Haut durch Schleier aus Fasern ersetzt, Wulst asymmetrisch, Abläufe als flache Ströme, Eier in die Kruste, Adern flach und gewunden |
| 3 | Flur ohne Zähne, Fäden sichtbar. Von innen hängende Lappen wie Reißzähne, zwei Löcher im Schleier wie Augen, Laibungskruste ragt als Würste in den Flur, Krustenflecken wie Scheiben, Wulst wie ein Arm | eine Falte statt Lappen, nur ein Einriss, Kruste bleibt in der Laibung, Krustenrand am Rahmen, Textur des Nests auch für die Wucherung |
| 4 | Keine Zähne, kein Gesicht, Eier eingebettet. Von innen eine glatte Rolle wie eine Lippe, unter dem Sturz zwei helle Brotwülste, Wucherung glänzt | Falte in zwei versetzte, ungleiche Lagen; Wülste kleiner und eingebunden; matter |
| 5 | Unregelmäßige Kanten, keine Lippe, keine Wülste. Fäden mit Harzperlen und Gelege lesen sich als Nest. Abgenommen. | – |

**Bleibt offen:**
- Die Wucherung im Flur bleibt eine glatte, schwere Masse; eine feinere Schichtung bräuchte mehr Dreiecke, als die Detailstufe 0 (46 000) hergibt.
- Die Fernansicht (`fern`) zeigt das Nest nicht. Es liegt dort außerhalb des Bildes; die Schrägansicht aus etwa 5 m ersetzt sie.
