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

## V13.3: Level 2 – kleine Motten an den Röhren

Quellen: [Level 2](https://backrooms-wiki.wikidot.com/level-2) Rev. 93 (Abschnitt Entities/Deathmoths) und [Entity 4](https://backrooms-wiki.wikidot.com/entity-4) Rev. 6, beide CC BY-SA 3.0, eigene Worte.

**Was die Seite sagt:** Früher zahlreich um die Leuchtstoffröhren (das Licht lockte sie), im großen Stromausfall weggezogen oder von den Smilern gefressen; heute kleiner als anderswo (Nahrungsmangel, enge Gänge) und meist friedlich – man soll sie aber nicht aus der Nähe reizen. Entity 4: die Männchen sind friedliche Aasfresser mit einer nadelartigen Saugröhre und den besten Fühlern der Art, die Weibchen aggressiv mit Reißkiefern. Fressfeinde auf Level 2: die kleinen Nguithr'xurh über den Maschinen.

**Umsetzung (vorher „bewusst weggelassen“):**
- **Modell** `tools/blender/creatures/moth_male.py` → `ent_moth_male_l0..l2.glb` (Variante 1 von Entity::Moth, Skelett `ent_moth`, L0 25 984 Dreiecke): statt Mandibeln eine zur Uhrfeder eingerollte Saugröhre, schlanker Leib mit schütterem Haarsaum und Haltezangen, große doppelt gekämmte Fühler, staubiger, dünner Pelz (Hunger). Das Weibchen ist das vorhandene `ent_moth` (Variante 2). Kleiner macht es das Spiel: Männchen Maßstab 0,52 (etwa 23 cm Spannweite), Weibchen 0,66.
- **Verhalten** (`src/gameplay/presence_moth_l2.cpp`, Kin-Liste `KIN_MOTH`, Level-JSON `presence.moth`: Gruppe 3–6, `every 110`, `chance 0.35` = Anteil der Gruppen mit einem Weibchen, höchstens 2 Gruppen):
  - Auftritt an einer **brennenden** Leuchtstoffröhre der Karte (`tube_white`, `tube_orange`; tote Gruppen nicht), 8–34 m Weg, ungesehen; Röhren nahe einem Nguithr'xurh-Netz bevorzugt. Kein Nest.
  - Sitzen an der Decke neben der Röhre; vor dem Auffliegen zittern die Flügel (Aufwärmen). Fliegen um die Röhre, stoßen klickend ans Glas (`moth_tap_0..2`), kehren an ihren Platz zurück.
  - **Licht lockt:** im Lampenkegel bis 9 m oder Lampe näher als 4 m – sie kreisen 1 m vor der Lampe, tun nichts. Lampe 3 s aus oder weiter als 12 m: zurück. Röhre dunkel (Stromausfall): sie ziehen ab.
  - **Netz:** ein Männchen treibt manchmal in ein Netz der Nguithr'xurh nahe der Röhre (etwa alle 45 s je Männchen), zappelt, wird ungesehen eingesponnen (`Stridulate` der Spinne).
  - **Reizen** (Gruppe): Gesicht unter 0,9 m an den Motten (+1,0/s), Lampe aus unter 2,5 m auf sie (+0,8/s), Rennen oder Springen unter 1,5 m (+2,0/s), Abklingen 0,4/s; ab 2,5 gereizt: Auffliegen (`moth_takeoff`), nach 0,25 s Brummen (`moth_buzz` – die Ankündigung), sie schwirren 0,5 m vor dem Gesicht. **Nur das Weibchen beißt**, frühestens 1,4 s nach dem Brummen und nur, solange man unter 3 m an der Röhre bleibt: 3 Punkte (`moth_bite`), dann 3,5–5,5 s Abstand. Zurücktreten (4 m) und Licht senken beruhigt; Männchen-Gruppen beruhigen sich spätestens nach 6 s, mit Weibchen nach 14 s.
- **Klang:** Flattern als Schleife `moth_flutter` (vorhanden), `moth_tap`, `moth_buzz`, `moth_bite` (`src/audio/synth_l2.cpp`), Auffliegen `moth_takeoff`.
- **Mehrspieler:** Host rechnet; Puppen mit Variante, Lage, `fly`, `shiver`, `reach`, `light`, `perceive` über die vorhandenen NetState-Kanäle; Ereignisse sind vorhandene Arten (`Knock`, `Hiss`, `Hit`, `TakeOff` mit Rolle Moth). Protokoll unverändert.
- **Interpretation:** Zahlen der Reizung, der Biss (3 Punkte), dass nur das Weibchen beißt (Entity 4: Männchen friedlich), das Aufwärmzittern.

**Wow-Check:** Unter einer Röhre sitzen handtellergroße, pelzige Motten mit Augenflecken; die Lampe zieht sie heran, und wer zu nah kommt, hat plötzlich flatternde Flügel und Beine vor den Augen, dazu das Brummen. Ehrlich: eklig und unruhig, nicht „ganz okay“.

### Kritikrunden (Level 2)

Bilder: `docs/screens/creatures/l2_motten/`.

| Runde | Befund | Änderung |
|---|---|---|
| 1 (`r1_bogen.jpg`) | Die Gruppe saß neben der Röhre an der Decke statt an ihr (Auftritt auf Kacheln mit Lampenmarke, nicht an der Leuchte); aus Spielerhöhe winzige graue Flecken. | Auftritt an den Leuchtstoffröhren der Karte selbst; Modell des Männchens umbenannt (`ent_moth_male` – der Name `ent_moth_l2` kollidierte mit der Detailstufe 2 von `ent_moth`). |
| 2 (`r2_bogen.jpg`) | Die Motten sitzen an der Röhre; aus der Nähe (`r2_decke_nah.jpg`) Flügelmuster mit Augenflecken, Beine, Fühler; im Flug schlagen die Flügel sichtbar (`r2_flug.jpg`). Die Lampe lockt (`r2_lampe.jpg`). | – (Befund gut; weiter mit dem Reizen) |
| 3 (`r3_bogen.jpg`) | Gereizt kreisten sie um den Kopf – meist außerhalb des Blickfelds (`r3_gereizt_leer.jpg`): die Ankündigung war nur zu hören, ab und zu huschte ein Flügel durchs Bild. | Sie schwirren 0,5 m vor dem Gesicht (Kreismitte vor dem Auge). |
| 4 (`r4_bogen.jpg`) | Pelzige Motten mit gemusterten Flügeln und Beinen flattern vor den Augen (`r4_gereizt_c.jpg`) – bedrängend, gut. Restmangel: an der Decke sitzen sie starr wie Papierfetzen und fliegen ohne Vorzeichen los. | Vor dem Auffliegen zittern die Flügel 1,4 s (Kanal `shiver`, wie echte Nachtfalter die Flugmuskeln aufwärmen). |
| 5 (`r5_bogen.jpg`) | Um die Röhre unruhiges Kommen und Gehen, Flügel angehoben vor dem Start (`r5_roehre_b.jpg`); das Zittern selbst ist auf Standbildern kaum zu sehen und wird im Test über den Kanal geprüft. Abgenommen. | – |

### Tests (`gameplay_test`, „V13.3 Teil 4: Level 2“)

Gruppe an einer brennenden Röhre (Regie), meist Männchen, niemand sieht sie erscheinen, oben an der Decke; in Ruhe gelassen friedlich; jeder Start von der Decke mit Flügelzittern davor; das Licht lockt sie, um das Licht kreisend tun sie nichts; manche Gruppen mit Weibchen; aus der Nähe gereizt: Brummen, gereizte Männchen im Blickfeld vor dem Gesicht, das Weibchen beißt erst nach der Ankündigung, leicht; zurücktreten und Licht senken beruhigt.
