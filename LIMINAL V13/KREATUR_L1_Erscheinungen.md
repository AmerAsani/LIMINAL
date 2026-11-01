# Erscheinungen von Level 1: Garden-Verwandlung und die Spuren an der Zimmertür

Quellen (Wikidot, CC BY-SA 3.0, eigene Worte, kein Bild der Seite):
- [level-1](https://backrooms-wiki.wikidot.com/level-1) Rev. 83 (07.10.2026; Praetor3005 und DivineAtlas, Konzept EnderMitten), Abschnitte Sectors (Garden), Wesen und Erzählung; gelesen am 27.10.2026 (`scratchpad/v13/wiki/level_1.md` Abschnitte 6–7).
- Weitere Erscheinungen der Seite: Crawler-Pilz → `KREATUR_Crawler.md`, Gemälde der Spielleiterin → `KREATUR_GameMaster.md`. Ausgenommen (Auftrag): rein menschliche Gruppen und Erreger ohne Körper; Ouroboros-Arbeiter und Plague Goblins baut ein späterer Zweig.

Code: `gameplay/phenomena.*`, `gameplay/session_phenomena.cpp`; Modelle `tools/blender/models/models_l1_phenomena.py`; Klänge `audio/synth_phenomena.cpp`; Bild `shaders/post.hlsl` (Garden-Rand), `shaders/mesh_gbuffer.hlsl` (`skinMarks`: Ranken und Rinde auf der eigenen Hand).

## A. Garden – die Verwandlung in eine Pflanze

**Was die Seite sagt:** Ein üppig überwucherter Sektor; Wesen nennt sie dort keine. Wer etwa eine Stunde bleibt, beginnt sich in eine Pflanze zu verwandeln.

**Umsetzung**
- **Garden-Uhr** je Spieler: zählt die Zeit in Räumen des Typs `garden` (Zone der Karte, zwei Hallen 62 × 45 m). Draußen läuft sie mit einem Drittel der Zeit zurück (wer geht, erholt sich langsam).
- **Gestufte Zeichen**: nach 3 min „Erde unter den Fingernägeln …“, leises Rascheln dicht am Ohr, die ersten dunklen Triebe winden sich um Handgelenk und Finger (Viewmodel), Blätter wachsen vom Rand der Sicht herein; nach 6 min Ranken überall, langsamer (×0,9); nach 10 min **Verwandlung**: die Haut wird bleich-grünlich und verholzt zu Rinde, Holz knarrt, der Klang wird dumpf, das Tempo sinkt bis ×0,55, der Blätterrand schließt sich; nach 15 min Tod mit eigener Ursache `garten` („DU BIST EINE PFLANZE“). Verlassen meldet „Weg vom Grün … langsam lässt das Ziehen nach.“, ganz erholt fallen die welken Ranken ab.
- **Spur der Verwandelten**: in jeder Garden-Halle eine Gestalt aus Rinde zwischen den Farnen (`plant_wanderer@0`: kniend, Kopf gesenkt, ein Arm ist zum Ast mit Blättern geworden, der andere wurzelt im Boden; `@1`: stehend, die Beine zum Stamm verwachsen, beide Arme zur Krone erhoben, ein Trieb wächst aus dem offenen Mund). Knorren, Aststummel, Moos und Efeu am Kopf, Augenhöhlen und Mund in der Rinde, ein Schuh, den die Wurzeln sprengen, Fetzen einer roten Jacke, Risse im Estrich.

**Interpretation:** „Etwa eine Stunde“ ist im Spiel verdichtet (Zeichen ab 3 min, Verwandlung ab 10 min, Tod nach 15 min) – sonst erlebte sie kein Spieler; der Sektor selbst ist in wenigen Minuten durchquert, wer trödelt, merkt es rechtzeitig. Die Gestalten sind abgeleitet: was aus denen wurde, die blieben.

**Zahlen:** Uhr 180 / 360 / 600 / 900 s; Erholung ×0,33; Verstand zusätzlich 0,02 / 0,05 / 0,12 %/s; Tempo ×0,9 (Ranken), ×0,8 → 0,55 (Verwandlung); Klang dumpf 0,3–0,6; Haut: Ranken ab 144 s, Rinde ab 600 s (Shader ab 0,5).

**Klang:** `garden_rustle` (trockene Blätter, in Wellen), `garden_creak` (Holz knarrt, Stick-Slip mit tiefen Formanten).

**Wow-Check:** Zwischen den Farnen kniet jemand aus Rinde, den Arm zum Ast gestreckt – und dann siehst du die feinen Triebe an deinem eigenen Handgelenk.

## B. Die Spuren an der Zimmertür (Erzählung der Seite)

**Was die Seite sagt:** In der Erzählung kam nachts ein dunkles, stilles Wesen mit klatschenden Schritten an die Schlafraumtür; zurück blieben Krallenspuren und eine zähe Flüssigkeit.

**Umsetzung**
- **Ort**: ein Zimmer in den Gängen mit nur einer Tür (Karte: Spawnpunkt `trace`/`claws` 0,6 m hinter der Schwelle).
- **Spuren** (Blender): an der Wand neben der Tür, Gangseite, drei Sätze tiefer **Krallenrillen** (graue Rinne im Putz, dunkle Mitte, gezackte helle Ränder, abgeplatzte Brocken, Putz am Boden) und **schwarze, zähe Flüssigkeit** entlang der Krallen mit Läufern und tropfenförmigen Enden, eine Lache am Fuß der Wand (`claw_marks`); an der Schwelle eine Spur breiter, flacher **Abdrücke mit drei Krallen**, dazwischen Tropfen und gezogene Fäden (`goo_trail`).
- **Klang**: Steht man draußen in bis zu 9 m, kommt im Mittel alle 110 s (×0,6–1,4) eine Folge **hinter der Tür**: fünf klatschende, nasse Schritte nähern sich von hinten im Zimmer, dann zwei Mal Krallen über Blech und Putz, schwerer feuchter Atem dicht an der Tür – Stille. Im Zimmer selbst hört man nichts (dort ist nie etwas).

**Interpretation:** Die Seite nennt das Wesen nicht; es bleibt unsichtbar und ungefährlich – nur Spur und Klang, damit die Erzählung im Spiel spürbar wird, ohne ein Wesen zu erfinden.

**Klang:** `slap_step` (flacher nasser Fuß: dumpfer Aufsatz, klatschendes Wasser, saugendes Ablösen), `claw_scrape` (vier Krallen, hohes Stick-Slip-Kreischen, bröckelnder Putz), `wet_breath` (Einatmen mit Rasseln, Ausatmen durch Flüssigkeit).

**Wow-Check:** Klatsch. Klatsch. Hinter der Tür nähert sich etwas Nasses, dann kreischen Krallen über das Blech – und an der Wand daneben kleben die schwarzen Läufer von gestern.

## Mehrspieler
Garden-Uhr und Tod rechnet der Host (Tod des Gasts mit Rolle −3 über das bestehende Todesereignis, der Gast zeigt „DU BIST EINE PFLANZE“); Haut, Blätterrand und Klang rechnet der Gast für sich aus seiner Lage. Die Folge hinter der Tür hört jeder Spieler für sich (kein Spielzustand). Protokoll unverändert.

## Gleichgewicht
Beide ohne direkten Schaden außer der Garden-Uhr; der Garden bleibt durchquerbar (Zeichen nach 3 min, Tod erst nach 15 min, Erholung draußen).

## Kritikrunden
Bilder: `docs/screens/creatures/l1_erscheinungen/` (`r<N>_garden*`, `r<N>_tuer*`).

| Runde | Befund | Änderung |
|---|---|---|
| 1 (`r1_tuerspuren.jpg`, `r1_garden.jpg`) | Spuren nicht im Bild (Blick in das Zimmer statt auf die Wand daneben); Garden-Bild von der Einladung verdeckt. | Eigene Kamera auf die Wand, Reihenfolge der Aufnahmen. |
| 2 (`r2_tuerspuren.jpg`, `r2_garden_arm.jpg`, `r2_garden_figur.jpg`) | Krallen als gleichförmige schwarze Striche, Schleimspur im Dunkeln unsichtbar; Garden-Rand ein pixeliges Tarnmuster; die Gestalt liest sich als Wurzelknäuel. | Rand aus echten Blattformen (Polarkoordinaten, Mittelrippe, dahinter Laub); Schultern und größerer Kopf. |
| 3 (`r3_garden_rand.jpg`, `r3_tuerspuren_lampe.jpg`) | Blätter flache, zu grüne Dreiecke, Rand ein perfektes Oval; Krallen ohne helle Ränder. | Drei Blattlagen, gebogen, dunkler, Rand gewellt; Krallen mit grauer Rinne und dunkler Mitte. |
| 4 (`r4_garden_figur_lampe.jpg`, `r4_garden_rand.jpg`) | Gestalt jetzt als kniender Mensch aus Rinde lesbar, Kopf ein blasses Ei; Ranken an der Hand neongrün, Rinde wie Nadelstreifen. | Ranken dunkel und matt, Rinde unregelmäßig; Kopf mit Knorren, Moos, Efeu, dunklerer Rinde. |
| 5 (`r5_tuerspuren_lampe.jpg`, `r5_tuer_schleim_lampe.jpg`) | Krallen und Läufer tragen; die Abdrücke mit Krallen führen sichtbar die Schwelle entlang. Ranken erst ab 10 min sichtbar (zu spät). | Ranken schon ab den ersten Zeichen (Hautwert bis zur Verwandlung 0 → 0,5). |
| 6 (`r6_garden_arm_450.jpg`, `r6_garden_arm_640.jpg`, `r6_garden_arm_850.jpg`, `r6_garden_kopf.jpg`) | Verlauf an der Hand klar: Triebe, dann bleich-grün mit Rinde. Die zwei Aststummel am Kopf wirkten wie Hasenohren. | Stummel seitlich und nach hinten. |
| 7 (`r7h_garden_kopf.jpg`) | Kopf ohne „Ohren“, die Gestalt bleibt menschlich. | Abgenommen. |

## Tests (`gameplay_test`, „V13.3: Erscheinungen in Level 1“)
Garden-Uhr zählt im Sektor; Zeichen nach 3 min (Meldung), Ranken (Haut, langsamer), Verwandlung (Rand > 0,6, Tempo < 0,75); draußen geht die Uhr in 10 s um 2,5–4,5 s zurück; nach 15 min Tod (Ursache `garten`, Rolle −3). Tür: von draußen genau fünf Schritte, zwei Mal Krallen, ein Atem; im Zimmer still.

## Restmängel
- Die Gestalten haben keine Animation (sie sind Ausstattung, keine Figur).
- Das Wesen hinter der Tür bleibt unsichtbar – bewusst (Interpretation oben).
