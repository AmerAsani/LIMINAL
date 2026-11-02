# Kreatur: Fensterwesen / Windows (Entity 2)

Quelle: [backrooms-wiki.wikidot.com/entity-2](https://backrooms-wiki.wikidot.com/entity-2), Revision 36, gelesen am 26.10.2026 (Abgleich) und erneut beim Bau.
- Neufassung: DrAkimoto; ursprünglicher Eintrag eines anonymen Fandom-Nutzers; Idee: u/glassydude101 (Reddit).
- Lizenz CC BY-SA 3.0.
- Zusammenfassung in eigenen Worten; kein Bild der Seite verwendet.

## Was die Seite sagt

**Gestalt:**
- Ein Fensterwesen ist die körperliche Erscheinung eines räuberischen Gedankenraums. Es bildet sich auf fast jeder Fläche, meist in einem gewöhnlichen Fenster, und nimmt die Form an, die zur Ebene passt.
- Hinter der Scheibe liegt eine verschwommene, verzerrte Landschaft. Sie ist für jeden Betrachter anders und passt selten zu dem, was hinter der Wand wirklich liegt.
- Ein besetztes Fenster enthält einen dunklen, gesichtslosen Umriss eines Menschen. Zeugen meinen darin jemanden zu erkennen, den sie kennen.
- Fängt es jemanden, bildet es einen körperlichen Arm aus, der greift und hineinzieht.

**Verhalten:**
- Lockt mit Flüstern (oft als tröstlich beschrieben) und mit Handzeichen.
- Wer näher als etwa 1–2 m kommt, wird gegriffen und hineingezogen.
- Einmal entstanden, bleibt es an seinem Ort, bis es vergeht.

**Ausweg (Seite):** nicht nähern; ruhig gehen und sich der eigenen Wirklichkeit versichern; keinem plötzlichen Drang nachgeben.

**Ebenen (Abgleich):** Level 188 (der Hof der Fenster), Level 2 (am Gangende).

## Umsetzung

**Modell** (`tools/blender/creatures/window.py`, Skelett `wn`, `data/models/ent_window_l0..l2.glb`):
- Platte mit der Landschaft (eigene Texturebene „Ansicht“: Horizont, Hügel, Felder, Weg, Baumgruppen, verschwommen wie hinter nassem Glas). Sie liegt vor der dunklen Rückwand des Fensters der Ebene; Rahmen, Sprosse und Bank kommen vom Fenster von Level 188.
- Umriss: flache, verschmolzene Formen (Hüfte, Brust, schräge Schulterlinie, Hals, Kopf), schwarz und matt (Material „Umbra“).
- Zwei runde, körperliche Arme mit langen, dünnen Fingern (die Seite: ein körperlicher Arm).

**Bewegung** (`CreatureAnimator::updateWindow`):
- Der Umriss wiegt sich kaum merklich und atmet; er wendet den Kopf dem Betrachter zu und legt ihn schief.
- Locken: linker Oberarm seitlich, Unterarm aufrecht, die Hand winkt zu sich heran.
- Greifen: der Umriss lehnt sich aus dem Fenster, der rechte Arm schießt heraus und streckt sich bis gut 2 m (länger als ein Menschenarm).

**Verhalten** (`Presence::thinkWindow`, `placeWindows`):
- Level 188: drei Fensterwesen in dunklen Fenstern ohne Jalousie im Erdgeschoss, einmal je Ebene gesetzt (Level-JSON `presence.window.count`).
- Flüstern unter 11 m, je näher, desto öfter; jedes Flüstern zehrt am Verstand.
- Winken, sobald man es auf 14 m sieht.
- Unter 1,9 m vor der Öffnung fährt der Arm heraus (Ton „Sog“). Wer nach 0,7 s noch unter 2,3 m davor steht, wird hineingezogen (tödlich). Wer sofort zurücktritt, entkommt; danach 4 s Ruhe.

**Klang:** `window_whisper_0..3` (warm, langsam, nah), `window_pull`.

**Mehrspieler:** Der Host entscheidet, die Gäste sehen Winken und Greifen über die Gesten der Momentaufnahme (Protokoll v3).

**Eigene Auslegung:** die Abstände (Flüstern 11 m, Greifen 1,9 m, 0,7 s), drei Fenster, der Arm, der sich streckt; dass der Umriss sich zum Greifen herauslehnt.

**Nicht umgesetzt:**
- ~~Level 2 (Fensterwesen am Gangende)~~ – umgesetzt in V13.3, siehe Abschnitt „V13.3: Level 2 – das Fenster am Gangende“.
- „Neugeborene“ und „sterbende“ Fenster (die Seite: entstehen aus Fluchtwunsch, verschieben beim Vergehen Menschen aus der Wirklichkeit) – ohne Spielsystem für Wünsche und Verschiebungen.

## Gleichgewicht

| Wert | Zahl |
|---|---|
| Flüstern | unter 11 m, alle 1,3–5 s, Verstand −0,4 × Nähe |
| Winken | Sicht unter 14 m |
| Greifen | unter 1,9 m vor der Öffnung (seitlich ±1 m); tödlich nach 0,7 s, wenn noch unter 2,3 m |
| Ausweg | gehend (3 m/s) 2,1 m in 0,7 s – zurücktreten reicht |

## Kritikrunden (Spielkamera, Level 188)

Bilder: `docs/screens/fensterwesen/`.

| Runde | Befund | Änderung |
|---|---|---|
| 1 (`r1_umriss_unsichtbar.jpg`) | Landschaft und winkende Hand gut; der Umriss fast unsichtbar – das Rauchmaterial der Gestalt dünnt Ränder aus, bei einer flachen Form ist alles Rand (B-015). Der greifende Arm sah wie ein Stock aus. | Eigenes Material „Umbra“ (schwarz, matt, fest); Arme ruhig dichter am Körper. |
| 2 (`r2_umriss_sichtbar.jpg`) | Der Umriss steht im Fenster – unheimlich. Mängel: kantige „Schulterpolster“ (ein waagrechter Balken); der Arm fuhr zu langsam aus. | Schräge Schulterlinie und runde Kappen; Arm schneller (Feder 7). |
| 3 (`r3_schultern_griff.jpg`) | Schultern gut. Der Arm zielte auf das Auge – zur Kamera hin verkürzt, kaum zu sehen; der Umriss kippte beim Herauslehnen aus dem Bild. | Ziel die Brust (0,75 m unter dem Auge); weniger kippen, weiter heraus. |
| 4 (`r4_lehnt_heraus.jpg`) | Der Umriss lehnt sich heraus, beide Arme greifen nach unten aus dem Fenster; von der Seite deutlich ein Arm aus dem Fenster. Abgenommen. | – |

**Bleibt offen:** Der Umriss ist bewusst flach (ein Schatten hinter Glas); von der Seite gesehen ist er dünn.

## Tests (`gameplay_test`)

Level 188: drei Fensterwesen; auf 4 m Flüstern, nichts weiter; zu nah → der Arm fährt heraus; sofort zurück → entkommen; stehen bleiben → hineingezogen (tödlich).

## V13.3: Level 2 – das Fenster am Gangende

Quellen: [Level 2](https://backrooms-wiki.wikidot.com/level-2) Rev. 93 (Abschnitt Entities/Windows) und [Entity 2](https://backrooms-wiki.wikidot.com/entity-2) Rev. 36, beide CC BY-SA 3.0, eigene Worte.

**Was die Seite sagt:** Auf Level 2 relativ selten, meist am Ende von Gängen; man soll nicht hingehen. Im großen Stromausfall dienten die hellen Fenster als Orientierungsmarken, seit wieder Strom fließt, gelten sie nur noch als Gefahr. Entity 2: ein Fenster bildet sich auf fast jeder Fläche, lieber an Wänden, in der Form, die zur Umgebung passt, und bleibt, wo es entstanden ist; neue Fenster entstehen aus dem Wunsch, zu entkommen.

**Umsetzung (E-031 aufgelöst):** Die Rohrgänge haben keine Fenster – also bildet das Wesen sich selbst eines: ein altes Stahlfenster einer Fabrikhalle, eingemauert in die Betonstirnwand einer Sackgasse.
- **Modell** `tools/blender/creatures/window_end.py` → `data/models/ent_window_end_l0..l2.glb` (Variante 1 von Entity::Window, Skelett `ent_window`; L0 19 742 Dreiecke): Stahlkasten 11 cm tief, durchgerostet mit Resten von Lack, Nieten an Stirnblech und Sturz, vorspringende Bank mit Staub und toten Motten, Mörtelfuge rings um den Kasten; oben ein Kämpfer mit zwei kleinen Feldern (eines verschmutztes Drahtglas mit Einschlag, eines leer), das große untere Feld ohne Glas, Scherben in den Ecken. Dahinter wie in Level 188 die verschwommene Landschaft und der dunkle Umriss mit den Armen. Öffnung 1,10 m breit, 0,95–2,37 m über dem Boden (Tunnel 2,6 m).
- **Karte** (`tools/levels/level2.py`, eigener Zufallsgenerator am Ende): 9 Gangenden als Spawnpunkte `entity`/`window` (Stirnwand einer Sackgasse, Blick in den Gang).
- **Verhalten** (`src/gameplay/presence_window_l2.cpp`, Level-JSON `presence.window`: `mode "end"`, `count 1`, `every 300`, `chance 0.7`):
  - Entstehen nur ungesehen: kein Punkt der Fensterfläche im Blickfeld eines Spielers mit freier Sicht, 12–45 m Weg; höchstens eines zugleich. Auf der Flucht (ein Jäger in der Nähe) 2,5-mal, mit zerrüttetem Verstand (unter 45) 1,8-mal so oft – **Interpretation** des Fluchtwunschs der Seite.
  - Ankündigung beim Entstehen: Knacken von Glas und Rahmen, ein Zug Außenluft (`window_form`, 26 m). Danach in der Nähe alle 6–11 s ferner Wind und Vogelstimmen hinter Glas (`window_air_0..2`, 22 m) – man hört es, bevor man es sieht.
  - Dann wie in Level 188 (`thinkWindow`): Flüstern, Winken, unter 1,9 m fährt der Arm heraus, wer stehen bleibt, wird hineingezogen (tödlich), wer sofort zurücktritt, entkommt.
  - Vergehen: weiter als 70 m und 12 s ungesehen – fort, nie vor den Augen.
- **Mehrspieler:** Der Host rechnet; die Puppe beim Gast bekommt Rolle, Variante 1, Lage und die Gesten über die vorhandenen NetState-Kanäle; die Ankündigungen sind vorhandene Ereignisarten (`Burst`, Rolle Window). Protokoll unverändert.

**Wow-Check:** Am Ende eines dunklen Tunnels ein helles Fenster, wo vorher keines war, dahinter Hügel im Tageslicht und ein schwarzer Mensch, der winkt – und das Rauschen von Wind, wo es keinen Wind geben kann. Ehrlich: unheimlich, nicht „ganz okay“.

### Kritikrunden (Level 2, Spielkamera)

Bilder: `docs/screens/creatures/l2_fenster/` (je Runde fern 16 m, mittel 5 m, seitlich, nah mit Lampe, Greifen 1,4 m).

| Runde | Befund | Änderung |
|---|---|---|
| 1 (`r1_nah.png`, `r1_greift.png`) | Das Fenster wirkte wie ein dünner Bilderrahmen an der Wand; die Sprünge im Glas liefen kreuz und quer wie Kritzelei. | Kasten tiefer und breiter (11 cm, Stirnblech, Bank); ein Einschlag statt Kritzelei. |
| 2 (`r2_bogen.jpg`) | Der schwarz lackierte Kasten verschwand im Dunkel; der Einschlag las sich als Stern. | Kasten durchgerostet, Lack nur noch in Platten; Sprünge gezackt und ungleich lang. |
| 3 (`r3_bogen.jpg`) | Aus der Nähe ist der Rost sichtbar (`r3_greift.jpg`); im Gegenlicht bleibt der Kasten ein Schattenriss – richtig so. Mängel: der Sprung liest sich weiter als Sternsymbol (`r3_nah_lampe.jpg`); der Kopf des Umrisses ist aus 1,4 m kantig (der Umriss bekam zu wenige Dreiecke, der flache Schmutz zu viele). | Einschlag aus der Mitte gerückt, acht Strahlen laufen ungleich bis an den Rahmen, dazwischen unterbrochene Ringe; Dreiecke nach Sichtbarkeit (Umriss ×3, Arme ×2, Schmutz, Lack und Mörtel weniger), L0 18 000. |
| 4 (`r4_bogen.jpg`) | Der Kopf ist rund. Die neuen Sprünge waren dünner als ein Voxel und zerfielen in Punkte. | Sprünge 2–3 mm breit, Glas feiner gerastert (1,2 mm). |
| 5 (`r5_bogen.jpg`, `r5_nah_lampe.jpg`) | Der Einschlag liest sich als echter Bruch im Drahtglas; Kasten, Bank und Scherben aus der Nähe glaubhaft, von fern ein heller Fleck am Gangende. Abgenommen. | – |

### Tests (`gameplay_test`, „V13.3 Teil 4: Level 2“)

Gangenden in der Karte; zu Beginn kein Fenster; an einer Stirnwand im Blick entsteht keines; mit dem Rücken zur Wand bildet es sich ungesehen, in der Stirnwand (dahinter Beton), mit Ankündigung; es flüstert, Wind und Vögel hinter Glas, es bleibt; auf 1,5 m fährt der Arm heraus – sofort zurück: entkommen, stehen bleiben: hineingezogen.
