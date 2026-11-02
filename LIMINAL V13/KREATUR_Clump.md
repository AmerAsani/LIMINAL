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

## V13.3: Level 3 – die große, bewaffnete Form

**Quellen** (Wikidot, CC BY-SA 3.0, eigene Worte, keine Bilder, keine Zitate):
- [level-3](https://backrooms-wiki.wikidot.com/level-3), Rev. 18 (26.09.2026; u/M654z, Neufassung Natedagreat563), Abschnitt der hochintelligenten Wesen/Clumps und Vorfall GB-01.
- [entity-5](https://backrooms-wiki.wikidot.com/entity-5), Rev. 38 (23.09.2025; ursprünglich Kekcroc, Neufassung TrailmixNCocoa), als veraltet markiert, zwei Fassungen.

**Was die Seiten sagen:**
- Level 3: Biologie wie ein gewöhnlicher Clump, aber deutlich größer – gestreckt bis etwa 8 ft (normal höchstens etwa 3 ft); die Muskeln viel größer und stärker; die Arme geordnet und koordiniert statt chaotisch. Sie tragen oft mehrere Waffen zugleich und führen sie mühelos (gefangene drückten sogar Abzüge); schneller, wendiger, klüger; typische Jagd: an der Decke klettern und sich an Hängendem entlangschwingen; blutrünstiger; **entwaffnet verstümmeln sie sich und legen blanken Knochen frei, um ihn als Waffe zu nutzen**; manche trainieren gezielt Muskeln. 19 von ihnen durchbrachen bei GB-01 die Holzwände einer Basis.
- Entity 5: keine Augen (Level 1) – jagt nach Gehör; still bleiben; beim Geräusch mehrerer rennender Menschen zieht es sich zurück; Augen und Ohren ragen aus der Masse, in der Mitte das Maul mit rasiermesserscharfen Zähnen.
- **Nicht gesagt:** welche Waffen, wie ein Angriff abläuft, eigene Laute, Licht.

**Umsetzung (vorher „teilweise“: nur das kleine Bündel)** – Varianten der Rolle Clump (keine neue Rolle, kein neues `anim::Entity`); das kleine Bündel in den Schächten bleibt unverändert; Level-JSON `presence.clump_l3` (`every 300`):
- **Modelle** (Bündelskelett, eigener Entwurf in `tools/blender/creatures/clump_l3.py`, im Spiel ×1,35): `ent_clump_l3` (Variante 1, bewaffnet) und `clump_l3_bare.py` → `ent_clump_l3_bare` (Variante 2, entwaffnet). Die Masse ein Knoten aus Muskeln – breite Stränge laufen vom Scheitel zu den sieben Schultern (geordnet wie Speichen), die toten Glieder liegen in drei Bändern um den Kern (geflochten, nicht geknäult). Sieben prall bemuskelte Arme (Bizeps, Trizeps, Unterarmbeuger, Deltamuskel, dicke Adern) in eigener, stumpfer, trockener Haut (`ent_cl_hide_a/b`). Drei Waffenarme: **Arm 1** – die Hand abgerissen, aus der zerfetzten Manschette ragen Speiche und Elle als gesplitterter Knochenspieß (Bruchflächen, Splitter, hängende Sehnen); **Arm 5** – der Unterarm längs aufgerissen (rohes Fleisch in der Furche), die Elle herausgebrochen als gekrümmte Knochensichel mit abgeschabter Schneide und Kerben; **Arm 3** – die Faust um ein rostiges Rohr mit drahtumwickeltem Griff und Winkelstück (Variante 1) bzw. abgerissen mit frischem Knochenspieß, mehr Sehnen und einem Hautlappen (Variante 2). Fünf Augen, zwei Ohren, ein breiteres Maul mit drei Zahnreihen. Materialien: frischer Knochen `ent_cl_bone`, rohes Fleisch `ent_cl_raw`, Rost `ent_ob_rust`, Draht `ent_wire`.
- **Animation** (`src/anim/creature_clump_l3.cpp`, Variante über `extra`): **geordnet** – vier Laufarme (0, 2, 4, 6) setzen zwei gegen zwei im Takt, der Kern gleitet ruhig; die Waffenarme tragen nicht: am Boden erhoben vor dem Kern wie bei einer Gottesanbeterin, **an der Decke** liegt der Kern dicht unter ihr, die Laufhände greifen nach oben, Waffenarme und Beine hängen herab und pendeln nach (das Maul zeigt nach unten). **Lauschen** (Finger tippen), **Ausholen** (Waffenarm hoch und zurück, Kern lehnt zurück), **Hieb** (nach vorn unten, der Kern kippt mit), **Riss** (Arm 5 packt den Unterarm von Arm 3, beide zittern, Arm 3 reißt nach außen). Hände werden im vergrößerten Bildraum aufgesetzt (kein Rutschen).
- **Verhalten** (`src/gameplay/presence_clump_l3.cpp`): erscheint außer Sicht an der Decke eines Gangs mit mindestens 2,45 m freier Höhe; **lauert** dort (Knochen klappern, ganz nah Atem durch die Zähne – Geräusch vor Sicht); hört es etwas, **lauscht** es, dann **jagt es an der Decke** (Hand über Hand, Staub rieselt) dorthin, wohin der Lärm *sich bewegt* (klug: es merkt sich die Richtung) und **lässt sich fallen**, wenn es darüber ist; am Boden **jagt es koordiniert** und lauscht im Wechsel. Ist der Lärm frisch und nah (≤ 2,8 m), **holt es aus**: Knochen schabt über Stein (1,1 s Ankündigung) – wer leise weggeht, entkommt, es sucht weiter; dann der **Hieb** zur Stelle des Lärms (Treffer nur, wo man noch steht: Rohr 24, Knochen 30 Health × Schwierigkeit, nie ein Sofort-Tod) oder daneben (Rohr klingt auf Beton). Ein Fehlschlag mit dem Rohr bricht es zur Hälfte – **entwaffnet reißt es sich den Knochen frei** (2,6 s, dabei hört es nichts: das Fluchtfenster; Modellwechsel zu Variante 2). Nach 20 s Stille oder wenn **zwei Spieler in der Nähe rennen** (Entity 5), zieht es sich an die Decke zurück und verschwindet.
- **Klang** (`src/audio/synth_hound_pack.cpp`, Klangnummern `KC_CL_*`): `cl3_ceiling` (Handflächen an Beton, Putzkörner rieseln), `cl3_bones` (Knochen an Knochen, das Rohr tickt), `cl3_breath` (zischend durch die Zähne ein, tief grollend aus – eigene Stimme), `cl3_drop`, `cl3_scrape` (Knochen schabt über Ziegel), `cl3_swing`, `cl3_hit`, `cl3_miss`, `cl3_snap` (das Rohr biegt sich, bricht, das Stück rollt weg), `cl3_tear` (Fleisch reißt, Knochen bricht frei, es blutet), `cl3_step` (schwere Handfläche). Einmalige Hinweise: an der Decke („blind – still sein“), beim Ausholen („leise zur Seite“).
- **Mehrspieler**: der Host rechnet (Gehör aller Spieler über die Wegfelder, das Zurückweichen vor mehreren Rennenden zählt alle Spieler); die Puppe bekommt Rolle, Variante (Modellwechsel bei Entwaffnung), `hang` (Decke), `strike` (Hieb), `plea` (Riss), `wave` (Waffenarm ×⅛). Protokoll unverändert.

**Interpretation (nicht aus dem Wiki):** welche Waffen (Rohr, Knochenspieß, Knochensichel), der Rohrbruch als Entwaffnung, die Mantis-Haltung, die Vorhersage des Lärms, die Zahlen, die Laute.

| Größe | ×1,35 des Bündels: Kern ~1 m, gestreckt ~2,4 m, längster Arm ~3,5 m |
|---|---|
| Sinne | blind; hört Lärm × 27 m Weg (Gehen 12 m, Schleichen 4 m, Rennen 27 m), Aufprall geworfener Dinge |
| Tempo | an der Decke 1,9 m/s, am Boden 2,1 m/s |
| Hieb | Ankündigung 1,1 s; Treffer ≤ 1,15 m von der Stelle des Lärms und ≤ 3 m; Rohr 24, Knochen 30 Health |
| Auftritt | ~300 s (±30 %), höchstens eines, 21–45 m Weg, außer Sicht, Decke ≥ 2,45 m |

**Gleichgewicht:** Es ist schwer, aber blind: wer schleicht oder stillsteht, wird nicht gefunden; das Schaben vor jedem Hieb lässt Zeit, zur Seite zu gehen; nach einem gebrochenen Rohr ist es 2,6 s taub. Zwei rennende Spieler vertreiben es (Koop).

**Wow-Check:** Über dem Gang klatschen Hände an Beton, Putz rieselt in den Lampenkegel – oben hängt ein Knoten aus Muskeln, sieben Arme gespreizt, unten pendeln ein rostiges Rohr und ein Spieß aus gesplittertem Knochen; dann fällt es herunter und schabt mit dem Knochen über die Ziegel. Ehrlich: an der Decke im Halbdunkel und wenn es fällt *IIIH*; ganz nah im vollen Lampenlicht etwas weniger (die glatte Haut der Arme wirkt dort puppenhaft – siehe „Bleibt offen“).

### Kritikrunden (Level 3)

Bilder: `docs/screens/creatures/l3_clump_gross/r1` … `r5` (je Runde ein Bogen `bogen_rN.jpg`; a–e Vorführung am Boden – vorn, Seite, nah an den Waffen, ausholend, entwaffnet; f–g an der Decke von unten; h–k in der Ebene – an der Decke dunkel und mit Lampe, gefallen, ausholend).

| Runde | Befund | Änderung |
|---|---|---|
| 1 (`r1/bogen_r1.jpg`) | Die Arme beherrschen alles – glatte, glänzende Rohre, der Kern verschwindet; die Laufhände standen am Boden senkrecht in die Luft (der Anfangswert „an der Decke“ setzte die Hände oben auf); ×1,5 sprengte die Gänge; Knochenspieß und Sichel kaum zu sehen; der Auftritt in 12 m scheiterte (Decke zu niedrig). | Der Anfang folgt der Eingabe; Maßstab 1,35; Muskelbäuche (Bizeps, Trizeps, Unterarmbeuger, Deltamuskel) und dicke Adern; eigene stumpfe, trockene Haut (`ent_cl_hide_a/b`); Knochenspieß 0,36 m und dicker, Sichel weiter ausladend, Rohr dicker und länger. |
| 2 (`r2/bogen_r2.jpg`) | Waffen lesbar (weißer Spieß, Rohr, c/d); an der Decke hängen Spieß und Rohr herab (f/g); aber der lange Arm faltete sich zu einem zwei Meter hohen Spinnenbein und alle Ellbogen standen hoch – wie beim kleinen Bündel, nicht „geordnet“; im Gang stand der Spieler vor einem Gitter. | Ellbogen nach außen und flach, an der Decke nach außen unten; der lange Arm liegt am Boden fast gestreckt nach vorn; Bildposition im Gang seitlich. |
| 3 (`r3/bogen_r3.jpg`) | Am Boden geordnet (der lange Arm liegt, b/e); in der Ebene stark: an der Decke über dem Gang ein Knoten aus Muskeln, darunter pendeln Rohr und Knochenspieß (h/i); gefallen und ausholend eine Masse praller Glieder (j/k). Im Lampenlicht liest sich die Haut wie Puppenhaut. | Haut dunkler (beide Töne). |
| 4 (`r4/bogen_r4.jpg`) | Kaum Unterschied; am Rohr fehlte der Rost (dieselbe Verschiebung des Rauschens wie beim Rudel). | Rostnarben am Rohr. |
| 5 (`r5/bogen_r5.jpg`) | Endstand: an der Decke im Halbdunkel das stärkste Bild (h/i), am Boden mit Waffen erhoben (c/d), entwaffnet mit zweitem Spieß (e). Abgenommen. | – |

**Bleibt offen:** Nah und im vollen Lampenlicht wirken die glatten Arme noch etwas puppenhaft (Haut ohne Schmutz und Blutspuren – das Material hat keine Flecken je Stelle); das Rohr verschwindet beim Entwaffnen mit dem Modellwechsel (es bricht und wird weggeschleudert – es liegt kein Stück am Boden).

### Tests (`gameplay_test`, „V13.3: Level 2/3 – … grosses bewaffnetes Buendel“)
Erscheint bewaffnet (Variante 1) und lauert an der Decke, solange es still ist; Schritte in der Nähe: es jagt an der Decke, lässt sich fallen (Aufschlag), vor jedem Hieb das Schaben der Knochen, ein Hieb tötet nicht; wer still steht, wird nicht gefunden; entwaffnet reißt es sich den Knochen frei (Variante 2, ein Riss-Klang); nach langer Stille zieht es sich zurück.
