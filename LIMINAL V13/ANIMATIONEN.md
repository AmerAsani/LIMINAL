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

## Clump (Bündel)

| Zustand | Bewegung | Wiki-Bezug |
|---|---|---|
| Ruhe | Der Kern hebt und senkt sich (Atem). Unruhig: ab und zu greift eine Hand ein Stück neu. Der längste Arm liegt lang ausgestreckt am Boden, den Ellbogen zum Boden (vorher stand er wie eine Stelze). | „längstes Glied 8–10 Fuß“ (Entity 5) |
| Ziehen / Eilen | Hand über Hand, der Kern folgt ruckweise und schaukelt, die Beine schleifen | „bewegt sich, indem es über den Boden wischt“ |
| Lauschen | Erstarrt (keine neuen Griffe). Der Kern steht 22 cm höher und neigt sich zum Geräusch, alle Ellbogen flach, die Finger tippen in schnellen Stößen. Im Spiel: beim Suchen im Wechsel mit dem Tasten. | blind, jagt nach Gehör (Level 1) |
| Licht | **keine Reaktion** – blind | „eyeless“ (Level 1) |
| Tasten | Arme fegen suchend hin und her | – |
| Ausholen | Der längste Arm hebt sich hoch hinter den Kern (1–1,5 s Ankündigung) | „streckt das längste Glied, um zu greifen“ |
| Zugriff | Der Arm schnellt zum Ziel, der Kern bäumt sich auf (Maul nach vorn) | Griff ab etwa 2,4 m |
| Schacht / Abstieg | Der Kern liegt über der Decke, der Köderarm hängt heraus, die Finger zucken; Abstieg: fällt und landet | klettert Wände, nutzt Lüftungen |

| Runde | Befund | Änderung |
|---|---|---|
| 0 (vorher) | Langer Arm steht in Ruhe wie eine Stelze; Lauschen und Licht wie Ruhe. Die Vorführung „Ausholen“ setzte das falsche Feld. Schacht unter der 6-m-Decke außerhalb des Bilds. | – |
| 1 | Ausholen und Zugriff deutlich, Lauschen hebt den Kern; der lange Arm steht weiter steil (Ellbogen nach oben). | Ellbogen des langen Arms zum Boden; lauschend alle Ellbogen flach, Kern höher |
| 2 | Langer Arm liegt; Lauschen klar anders als Ruhe; Licht ohne Wirkung. | Prüfung unter niedriger Decke (Level 0) |
| 3 | Alle Zustände einschließlich Schacht und Abstieg sichtbar und unterscheidbar. Abgenommen. | – |

## Gestalt

Die Gestalt verändert sich nie, solange man sie ansieht (Level 0: unbestätigte dunkle Gestalten, das Gefühl, verfolgt zu werden). Jede Bewegung geschieht also ungesehen und zeigt sich erst beim nächsten Blick. Für die Bildfolgen ist die Vorführung davon ausgenommen; im Spiel bleibt sie im Blick erstarrt.

| Zustand | Bewegung | Wiki-Bezug |
|---|---|---|
| Ruhe | Atem in Brust und Schultern, Gewicht wandert langsam von Hüfte zu Hüfte, Fasern treiben | – |
| Wahrnehmen | Vorgebeugt (der Oberkörper wird verschoben, nicht gedreht: die Arme haben keine eigenen Knochen und hängen so weiter senkrecht), Schultern hoch, Kopf vorgeschoben und schief, ganz zum Beobachter gewandt. Im Spiel, solange man wegsieht: beim nächsten Blick steht sie so da. | verfolgt einen, Skopophobie |
| Licht | erstarrt (wie bei jedem Blick) | – |
| Gehen, Rennen, Angriff | gibt es nicht: sie wechselt den Ort nur ungesehen und greift nie an | unbestätigt, daher harmlos |

| Runde | Befund | Änderung |
|---|---|---|
| 0 (vorher) | Vorführung im Blick erstarrt: alle Bilder gleich | Vorführung bewegt sich; Atem, Gewicht, Wahrnehmen |
| 1 | Unterschiede bei frontaler Kamera kaum sichtbar | deutlichere Haltung, Kamera von der Seite |
| 2 | Wahrnehmen erkennbar, aber der Hals geht nach hinten (Hohlkreuz statt Vorbeugen) | Kopf nach vorn |
| 3 | Lauernd vorgebeugt, aber eine Hand ragt hinter der Hüfte heraus (Arme ohne eigene Knochen drehen mit) | Verschieben statt Drehen |
| 4 | Vorgebeugt, Arme hängen, nur eine leichte Wölbung an der Hüfte. Abgenommen. | – |

## Deathmoth

| Zustand | Bewegung | Wiki-Bezug |
|---|---|---|
| Fliegen / Schweben | zwei Flügelpaare, das hintere später; Körper wippt gegen den Schlag, legt sich in Kurven | – |
| Sitzen | Flügel flach, Vorderflügel über den Hinterflügeln; ab und zu öffnen sie sich langsam; Hinterleib atmet; Fühler tasten | Weibchen bleiben in den Nestern |
| Aufwärmen | Flügel halb gehoben, deutliches Zittern (vorher 0,13 rad, im Bild nicht zu sehen) | – |
| Wahrnehmen | Fühler zeigen nach vorn zum Reiz, die Flügel heben sich etwas. Im Spiel: sitzend, wenn man näher als 5,5 m kommt. | – |
| Licht | Nase zur Lampe, schnellerer Schlag, Fühler vor, unruhiges Zucken hin und her | „helles Licht zieht sie an“ (Entity 4) |
| Stich | Sturz mit der Nase voran, Flügel flacher und nach hinten gelegt, Beine vorgestreckt, Hinterleib unter den Leib gekrümmt | Weibchen jagen in Schwärmen |

| Runde | Befund | Änderung |
|---|---|---|
| 0 (vorher) | Flug gut; Aufwärmen kaum sichtbar; Wahrnehmen wie Sitzen; Licht und Stich wie Schweben | – |
| 1 | Alle Zustände unterscheidbar (Ausschnitte `moth_r1_detail`) | Zucken am Licht; Flügel im Sturz nach hinten |
| 2 | Licht: hoher, schneller Schlag; Stich wie ein Wespenstoß | Gesamtprüfung |
| 3 | Sieben Zustände eindeutig, keine Durchdringung von Flügeln und Leib. Abgenommen. | – |

## Nest (V13.2)

| Zustand | Bewegung | Wiki-Bezug |
|---|---|---|
| Ruhe | Säcke im Kragen und im Flur atmen (etwa 4,8 s), der Schleier schwingt langsam an der Angel, die Wucherung pulsiert leicht, das Gelege zuckt ab und zu (Larven) | „semi-organisch“, Larven in den Wänden (Entity 4) |
| Lampe auf dem Nest | Atem und Schwingen schneller (etwa 2,1 s), größerer Ausschlag | helles Licht zieht die Motten an |
| Alarm (Schwarm fliegt) | wie im Licht, bleibt unruhig, bis der Schwarm zurück ist | – |

Weit weg (über 30 m) bleibt die Pose stehen. Prüfung über `figstate light`/`alert` an der Vorführung und im Spiel (`hiveat`). Bilder der Form: `docs/screens/nest/`.

## Neue Wesen (V13.2): Zweibeiner, Ratte, Fensterwesen

Neue Eingaben: `wave` (winken, locken), `talk` (sprechen: Kiefer in Silben), `plea` (flehen), `strike` (Phase eines Schlags). Sie reisen im Mehrspieler in der Momentaufnahme mit (Protokoll v3). Briefings: `KREATUR_Faceling.md`, `KREATUR_SkinStealer.md`, `KREATUR_Wretch.md`, `KREATUR_DeathRat.md`, `KREATUR_Fensterwesen.md`.

### Zweibeiner (Faceling, Skin-Stealer, Wretch) – `updateBiped`

Gemeinsamer Ablauf, Haltung je Wesen (`BipedStyle`):
- **Gang:** zwei Beine im Wechsel (Schrittlänge 0,85 + 0,38 × Tempo, relativ zur Beinlänge). Beim Losgehen tritt der hintere Fuß zuerst; steht ein Fuß zu weit hinten, setzt er nach. Im Stand hebt sich die Ferse, sobald der Fuß hinter der Hüfte steht. Im Schwung rollt der Fuß ab. Das Becken hebt und senkt sich, schwingt zur Seite und dreht; die Schultern drehen gegen. Erreichen die Beine den Boden nicht, sinkt das Becken höchstens 7 cm – lieber hebt der Fuß ab.
- **Arme:** hängen senkrecht (auch bei gebeugtem Rumpf), schwingen gegengleich; beim Rennen gebeugt.
- **Kopf:** eigene Blickrichtung im Körperrahmen – bleibt aufrecht, auch wenn der Hals vorhängt. Blick, Wahrnehmen, Licht, Umsehen in Ruhe, Rucke, Lauschen (schief).

| Wesen | Haltung | Gesten |
|---|---|---|
| Faceling | menschlich, etwas zaghaft | winken (rechte Hand, Unterarm pendelt); im Licht die Hand vor das Gesicht (nur ruhig); Hieb mit dem Stein (über den Kopf ausholen, nach vorn unten); zugreifen |
| Skin-Stealer | lange Schritte, kaum Armschwung, Kopf dreht langsam, Rucke | sprechen (Kiefer in Silben); jagend weit vorgebeugt; mit beiden Händen greifen |
| Wretch | gebeugt (0,4 rad), Kopf hängt vor (0,55 rad), Knie gebeugt; das rechte Bein schleift, Becken sackt darauf ein; rasselnder Atem; Rucke | Sprint mit schlackernden Armen; flehen halb aufgerichtet, Hände vor, zitternd; gurgeln (Kiefer zittert) |

### Death Rat – `updateRat`

| Zustand | Bewegung |
|---|---|
| Ruhe | schneller Atem, die Nase schnüffelt in Stößen, der Kopf sieht sich um |
| Trab | diagonale Paare |
| Galopp | vorn und hinten je fast gleichzeitig, der Rücken beugt und streckt sich |
| Wittern | auf den Hinterbeinen aufgerichtet, Vorderpfoten vor der Brust, Nase hoch |
| Fressen | Kopf am Boden, Kiefer kaut |
| Beißen | Stoß nach vorn, Maul weit auf |
| Licht | geduckt, Kopf weg |
| Schwanz | hängt mit Trägheit am Becken, schleift, pendelt im Lauf |

### Fensterwesen – `updateWindow`

| Zustand | Bewegung |
|---|---|
| Ruhe | der Umriss wiegt sich kaum merklich, atmet, legt den Kopf schief |
| Blick | wendet den Kopf dem Betrachter zu |
| Locken | linker Unterarm aufrecht, die Hand winkt zu sich heran |
| Greifen | lehnt sich heraus, der rechte Arm schießt heraus und streckt sich bis gut 2 m |

### Kritikrunden der Bewegung

| Runde | Befund | Änderung |
|---|---|---|
| 1 (Faceling) | Beim Losgehen Spagat: der Standfuß blieb stehen, bis die Phase ihn rief; das Becken sank zu Boden (B-016). | Anlauf, Ausgleichsschritt, Ferse, Absenkung begrenzt (Messung: Becken schwankt 5–9 cm) |
| 2 (Faceling, Skin-Stealer) | Gehen, Rennen, Winken, Licht, Zugreifen, Schlag unterscheidbar (`docs/screens/faceling/bewegung_zustaende.jpg`, `docs/screens/skinstealer/bewegung_zustaende.jpg`). | – |
| 3 (Wretch, Ratte) | Wretch: alle Zustände lesbar. Ratte: im Galopp schlug der Schwanz senkrecht hoch. | Schwanz steifer |
| 4 (Fensterwesen, im Spiel) | siehe `KREATUR_Fensterwesen.md` (vier Runden) | – |
