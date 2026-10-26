# Bewegung der Wesen (V13.2)

Die Wesen bewegen sich prozedural (`src/anim/creature_anim.cpp`). Jeder Zustand ist eigens für die Anatomie des Wesens geschrieben; es gibt keine gemeinsamen Bewegungsclips. Das Skelett stammt aus dem Bau in Blender (`tools/blender/creatures/`, gleiche Knochen wie die Bewegung).

## Neu für alle Wesen

- **Eingaben der Bewegung:**
  - `light`: im Lichtkegel der Lampe, mit Richtung `lightFrom`.
  - `perceive`: nimmt etwas wahr, mit Richtung `perceiveAt`.
  - Die KI in `presence.cpp` setzt beide. Lichtreaktion: Das Wesen zuckt zurück und gewöhnt sich nach einer Haltezeit; die Funktion heißt `lightReact`.
- **Skinning:** Dual Quaternions für alle Figuren (E-028). Die Knochenrahmen sind starr; Gelenke klappen bei starker Beugung nicht mehr ein.
- **Nachweis:** Bildfolgen je Zustand.
  - Skript `anim_series.ps1`, Level 36, Gepäckhalle.
  - Kamera um das Wesen (Skriptbefehl `figcam`), Zustand über `figstate <feld> <wert>`.
  - Je Zeile ein Zustand, je Spalte 8 Bilder später (0,13 s).
  - Bilder: `docs/screens/anim/<wesen>_r<n>.jpg`; r0 ist der Stand vor V13.2.

## Hound

| Zustand | Bewegung | Wiki-Bezug |
|---|---|---|
| Ruhe | Atem sichtbar (Brustkorb hebt sich, etwa 3 s). Gewicht wandert langsam von Seite zu Seite, ab und zu setzt er eine Pfote neu. Der Kopf sieht sich langsam um. | ausgemergelt, lauernd |
| Gehen | Kreuzgang, hinkt vorn links | – |
| Rennen | Galopp mit Flugphase. Kürzerer Sprung als zuvor: vorher überstreckten sich die Beine, im Sammeln fiel der Rumpf zu Boden. | „schnelle Jäger, wenn gestört“ (Level 2) |
| Wittern | Kopf hebt sich und streckt sich zum Reiz. Schnüffeln in Stößen (0,75 s). Linke Vorderpfote angehoben. Kiefer leicht offen. | Hounds hören und lauern (Level 3) |
| Licht | Zuckt zurück: Becken sinkt, Kopf eingezogen und abgewandt, die Pfote auf der Seite des Lichts vor den Augen, leichtes Zittern. Nach 1,5 s gewöhnt er sich. | eigene Ergänzung (dunkel lebendes Wesen); passt zu „Blickkontakt schüchtert kurz ein“ |
| Pirschen | geduckt, Kopf tief, knurrend | „Knurren heißt: er ist nah“ |
| Angriff | Sprung: Rumpf flach nach vorn, beide Vorderpfoten vorgestreckt und angehoben, Kopf vor, Maul offen. Der Hals wird kaum länger (vorher reckte sich nur der Hals). | „kräftige Angriffe“ |
| Übergang | Federn zwischen allen Haltungen (Ducken → Galopp usw.) | – |

Kritikrunden (Bilder `hound_r0` bis `hound_r3`):

| Runde | Befund | Änderung |
|---|---|---|
| 0 (vorher) | Ruhe starr. Wittern und Licht wie Ruhe (gab es nicht). Angriff: nur der Hals reckt sich zur Kamera hoch. Galopp: Beine überstreckt, Rumpf fällt im Sammeln zu Boden. | – |
| 1 | Ruhe lebt (Kopf sieht sich um). Wittern deutlich, Galopp ohne Einsacken. Licht kaum von Ruhe zu unterscheiden. Angriff: Rumpf steigt auf, Kopf knickt zur Schulter (Ziel lag seitlich; Sprung ging nach oben). Gelenke mit Dual Quaternions sauber. | Pfote vor die Augen, Becken sinkt; Sprung flach nach vorn, Pfoten vor; Vorführung frontal |
| 2 | Licht eindeutig (Pfote zum Gesicht), Angriff als Sprung erkennbar. Die Hand liegt aber halb hinter dem Haar, und der Hals streckt sich zu weit (Kopf außerhalb des Bilds). | Pfote vor die Augen, Hals kürzer, mehr Rumpf im Sprung |
| 3 | Alle acht Zustände unterscheidbar, keine Durchdringung von Pfote und Kopf, Gelenke ohne Knick. Abgenommen. | – |

## Smiler

| Zustand | Bewegung | Wiki-Bezug |
|---|---|---|
| Ruhe | Die Masse schwebt und hebt sich langsam wie Atem, das Gesicht treibt kaum merklich, unregelmäßiger Lidschlag, Zungen steigen wie Rauch. | Körper unklar, nur Augen und Lächeln |
| Gleiten | Masse zieht nach, Zungen wehen nach hinten | – |
| Wahrnehmen | Augen weit auf, das Gesicht dreht sich schnell zum Reiz, der Kopf legt sich kurz schief, die Masse neigt sich hin. Auslöser im Spiel: Lärm in der Nähe. | greift an bei lautem Lärm |
| Licht | Das Gesicht wendet sich zur Lampe, die Augen kneifen sich zusammen und leuchten heller (werfen das Licht zurück), die Masse neigt sich zum Licht, die Zungen werden unruhig. Keine Gewöhnung. | „angezogen von Licht“, „Augen reflektieren Licht“ (Entity 3) |
| Ankündigung | Das Lächeln öffnet sich weiter als zuvor (0,26 statt 0,17 rad), die Augen werden enger, die Masse richtet sich auf, die Zungen steigen. | – (Fairness: 1,5 s Zeit) |
| Zugriff | schnelles Gleiten, das Gesicht voran | – |

| Runde | Befund | Änderung |
|---|---|---|
| 0 (vorher) | Wahrnehmen und Licht wie Ruhe; Ankündigung im Bild kaum zu sehen | – |
| 1 | Wahrnehmen (Augen weit, Kopf dreht sich) und Licht (Augen eng, Gesicht zur Lampe) eindeutig, Ankündigung im Ausschnitt erkennbar. Ruhe noch starr. | Masse atmet, Gesicht treibt; Augen leuchten im Licht heller |
| 2 | Ruhe lebt; alle Zustände im hellen Saal unterscheidbar. | Prüfung im Dunkeln, wo er vorkommt |
| 3 (dunkel) | Nur Augen und Lächeln sichtbar, wie im Wiki; die Zustände sind allein am Leuchten unterscheidbar (zwei weite Augen; schmale Schlitze; breiteres Lächeln). Abgenommen. | – |
