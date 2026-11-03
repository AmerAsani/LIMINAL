# Kreatur: Photoshop (Entity 104), Level 2

Quellen: [Entity 104 – „Photoshop“](https://backrooms-wiki.wikidot.com/entity-104) Rev. 43 (Autor 23vincentrandall, Seite in
Überarbeitung) und [Level 2](https://backrooms-wiki.wikidot.com/level-2) Rev. 93 (Abschnitt Entities), beide Backrooms-Wiki auf
Wikidot, CC BY-SA 3.0. Alles in eigenen Worten, keine Bilder und keine Zitate. Im Spiel trägt er keinen Namen („ein Klotz neben dem
Rechner“).

## Was die Seite sagt

- **Gestalt:** ein Klotz neben einem Desktop-Rechner, etwa 2,36 m hoch; das Gesicht aus zwei Augenfotos und dem Foto eines weit
  aufgerissenen Pitbull-Mauls, völlig flach; der Körper ein Block mit Fleischfotos auf allen Seiten, blockig, mit Glitch.
- **Verhalten:** aus der Ferne harmlos; wer ihm näher als fünf Fuß kommt, um den legt sich eine große durchsichtige Box mit schwarzen
  Kanten – was durch ihre Wände geht, wird „ausgeschnitten“; die Box schrumpft (eine Puppe wurde darin zerrissen). Wer ihn angreift,
  wird ausgeschnitten. Mit einer Kamera fotografiert, greift er nicht an. Legt man ihm etwas hin, holt ein großer, weißer, blutiger
  Arm es aus dem Bildschirm in den Rechner, danach sieht man es auf dem Monitor; wer ihm etwas gibt, den lässt er, er sucht seine Beute
  anderswo. Mit genug Gegenständen holt er sie dunkelrot gefärbt als Beine heraus und läuft.
- **Rat:** Abstand halten, fotografieren, etwas geben; nicht angreifen, nicht stehen bleiben.
- **Level 2:** keine Art, selten, vor allem in Räumen mit PCs; erstmals bei Office Space EL3A gesehen; seit dem Stromausfall läuft kaum
  Elektronik, deshalb ist er heute meist anderswo.

## Umsetzung

**Modell** (`models_l2_rand.py`, neu in der Abnahme von Level 2): Die Fotos sind echte Aufnahmen gebauter Szenen, keine
gemalten Flächen mehr.
- **Fotos** (`tools/textures/ps_photos_render.py`, Blender/Cycles; Fleisch, Haut und Fell mit Lichtstreuung, nass glänzend,
  Blitz an der Kamera, geringe Schärfentiefe):
  - acht Fleischszenen: marmoriertes Steak in der Styroporschale mit blutiger Saugeinlage, Hackfleisch auf Metzgerpapier,
    Bauchspeck, Leber in Blut, Beinscheibe mit Mark, Darm auf Edelstahl, Rinderzunge, Haut mit Gänsehaut;
  - zwei menschliche Augen (Lider mit Lidfalte und feuchtem Rand, Wimpern, Iris mit Fasern, Äderchen, Hornhaut mit dem
    Blitz), die in verschiedene Richtungen sehen;
  - das weit aufgerissene Maul eines Pitbulls: Fell, schwarze Lefzen, schwarz gefleckter Gaumen, Eck- und Schneidezähne
    mit Zahnstein, Zunge, Speichelfäden.
- **Atlas** `data/textures/ps_photos` (`tools/textures/ps_photos_atlas.py`, 2048 px): jedes Foto als Abzug mit Farbstich,
  Blitzabfall, Korn, manchmal Datumsaufdruck; weißer Papierrand, vergilbt, mit eingesogenem Blut und blutigen
  Fingerabdrücken; dazu das Monitorbild und die Rückseite des Fotopapiers.
- **Klotz** `ps_block@1..4` (0,9 × 0,9 × 2,36 m, vier waagrechte Scheiben für den Glitch, zusammen rund 10 000 Dreiecke):
  - Grund aus durchgeweichtem Kleister und Blut, die senkrechten Kanten gefast.
  - Darauf eine Collage aus 84 Abzügen rings um den Klotz, um die Kanten gebogen, schief und überlappend. Jeder liegt knapp
    über denen, die er deckt.
  - Gut jede vierte Ecke löst sich und biegt sich ab, mit Papierrückseite.
  - Oben fünf Abzüge, aus 19 Stellen läuft Blut unter den Abzügen hervor, am Fuß eine Lache.
  - Vorn das Gesicht, flach wie auf der Seite: zwei Augenfotos, etwas zu weit auseinander und verschieden schief, darunter
    groß das Maul.
- **Weitere Teile**:
  - `ps_screen`: das leuchtende Monitorbild aus dem Atlas – Fleisch, darin ein Auge, rot, mit Zeilen und Versatz.
  - `ps_arm_a/b`, `ps_hand@0/1`: weiß, blutig, Nägel.
  - `ps_edge`, `ps_handle`: Box und Anfasser wie beim Freistellwerkzeug.

**Verhalten** (`apparitions_l2.cpp`, ohne Rolle; Level-JSON `apparitions.photoshop`, `photoshop_chance` 0,6, `box_range` 1,52,
`box_time` 5):
- 6 mögliche Büros (`level2.py`): hinter dem Röhrenmonitor eines Schreibtischs, mit Blick wie der Monitor – über dem Bildschirm ragt
  das Fotogesicht auf. Je Lauf mit 60 % **einer** davon.
- **Aus der Ferne:** der Monitor zeigt sein Bild (Fleisch mit einem Auge, rotes Licht, 3,6 m), der Klotz glitcht in Stößen (Scheiben
  verrutschen, Bitrauschen); in 5 m fiept der Monitor; beim ersten Blick in 7,5 m ein Hinweis („Abstand halten“).
- **Näher als 5 Fuß** (1,52 m zur Grundfläche): eine Box mit schwarzen Kanten und Anfassern legt sich um den Spieler (2 × 2 × 2,15 m)
  und schrumpft in 5 s auf 0,4 m; Meldung „Nicht hindurchgehen. Gib ihm etwas: hinwerfen (Q)!“. Wer durch eine Wand der Box geht oder
  bleibt, bis sie zu ist, wird **ausgeschnitten** (Tod, „A U S G E S C H N I T T E N“).
- **Gabe:** Ein abgelegter oder geworfener Gegenstand in 2,3 m um den Klotz – der weiße, blutige Arm wächst aus dem Bildschirm, greift
  ihn (0,7 s), zieht ihn hinein (0,8 s); danach steht er flach im Bild des Monitors; die Box fällt, der Klotz glitcht heftig und ist fort.
- **Angriff mit dem Beil:** sofort die Box um den Angreifer.

**Klang** (`synth_l2_rand.cpp`): `ps_glitch` (Bitrauschen, Rechtecktöne, gebrochene Abtastrate), `ps_whine` (Röhrenmonitor: Fiepen,
Netzbrummen), `ps_box` (trockenes Klicken, steigender kalter Ton), `ps_crop` (Reißen, digitales Knirschen, harter Schnitt), `ps_arm`
(Scheibe beult sich, Glas knistert, nasser Klatscher), `ps_release` (fallender, grober Glitch).

**Mehrspieler:** Der Host entscheidet Box, Tod und Gabe (geworfene Dinge der Gäste liegen beim Host); die Gäste bekommen Box (mit
Opfer und Meldung an das Opfer), Gabe, Tod und Verschwinden als Weltereignis `l2rand`; der Tod eines Gastes kommt als Rolle −4.

**Skript:** `rand photoshop <abstand> [winkel] [nr]` (erzwingt ihn, nie näher als 2,3 m), `randstate ps_box|ps_fed|ps_glitch <wert>`.

## Interpretation (begründet)

- **Kamera:** Das Spiel hat keine Kamera; „fotografieren“ entfällt. Die zweite Rettung der Seite (etwas geben) trägt allein.
- **Hinwerfen statt Hinlegen:** Das Spiel kennt Ablegen und Werfen (Q); beides zählt, wenn das Ding nahe beim Klotz liegt.
- **Fair:** Die Box schrumpft sichtbar 5 s lang – Zeit für einen Griff ins Inventar. „Nicht stehen bleiben“ der Seite ist im Spiel
  „nicht warten“: Wer in der Box wartet, stirbt; wer geht, auch – nur die Gabe rettet.
- **Selten, Rechnerräume:** Büros mit Röhrenmonitor; nach der Gabe sucht er „anderswo“ – im Lauf kommt er nicht wieder.
- **Gegenstandsbeine:** nicht gebaut (die Seite beschreibt es nur als Möglichkeit mit vielen Gegenständen).

## Zahlen und Gleichgewicht

| Wert | |
|---|---|
| Vorkommen | 60 % je Lauf, einer von 6 Plätzen |
| Auslöser | 1,52 m zur Grundfläche (5 Fuß) |
| Box | 2 m → 0,4 m in 5 s, Kanten tödlich |
| Gabe | jeder Gegenstand in 2,3 m |
| Warnung | Glitch und Fiepen ab 5 m, Hinweis in 7,5 m |

Tödlich nur für den, der näher als zwei Schritte an den Schreibtisch tritt und dann nichts hergibt; er kostet einen Gegenstand.

## Wow-Check

„Hinter einem flackernden Röhrenmonitor ragt ein Klotz aus Fleischfotos auf, ein Hundemaul als Mund – ein Schritt zu viel, und eine
schwarze Auswahlbox rastet um dich ein und zieht sich zu, bis aus dem Bildschirm ein weißer, blutiger Arm nach deiner Flasche greift.“
Ehrliche Antwort: „IIIH“, jetzt schon vor der Box. Der Klotz ist mit echten Fotos von rohem Fleisch, Darm und Zunge
zugeklebt, aus denen ein echtes Hundemaul und zwei fremde Augen starren, blutig und schief geklebt; bewusst wie ein
Bastelprojekt aus der Hölle (so beschreibt ihn ein Zeuge auf der Seite). Bis Runde 5 lautete die Antwort für den Klotz
„Comic“ – deshalb neu gebaut.

## Kritikrunden (Bilder `docs/screens/creatures/l2_randwesen/`)

**Runde 1** – `r1_ps_bogen.jpg`, `r1_ps_mittel.jpg`, `r1_ps_arm1.jpg`. *Schlecht:* Der Klotz stand vor dem Monitor und sah zur Seite
(Bildschirm verdeckt); der Arm faltete sich beim Zurückziehen nach oben aus dem Monitor, die Hand hielt die Flasche groß vor dem
Bildschirm. *Änderung:* Platz hinter dem Monitor mit dessen Blickrichtung; Arm wächst mit der Reichweite aus dem Bildschirm, das
Genommene nur bis zum Bildschirm in der Hand.

**Runde 2** – `r2_ps_bogen.jpg`, `r2_ps_arm1.jpg`, `r2_ps_bild.jpg`. *Gut:* Monitor mit rotem Bild vor dem Klotz – eine starke
Bühne; Box liest sich als Auswahlrahmen. *Schlecht:* zurückgezogen blieb ein Stummel mit voller Hand; Fotos wirken gemalt.
*Änderung:* ganz zurückgezogen kein Arm, Hand wächst mit; Fell dichter in drei Tönen, nasse Lichter, Poren, Fotovignette.

**Runde 3** – `r3_ps_bogen.jpg`, `r3_ps_schirm_nachher.jpg`, `r3_ps_gesicht.jpg`. *Gut:* Nach der Gabe zeigt der Monitor flach die
Flasche – wie die Seite es sagt. *Schlecht:* Das Bild füllt die Scheibe nicht; Anfasser zu klein.

**Runde 4** – `r4_ps_bogen.jpg`, `r4_ps_box1.jpg`, `r4_ps_schirm_vorher.jpg`. *Änderung umgesetzt:* Bild 1,18-fach, Anfasser 1,4-fach,
Kanten 1,5-fach. *Gut:* Box mit deutlichen Ecken, Bild füllt den Schirm (ein dunkler Fleck links gehört zum Glitchbild).

**Runde 5 (aus den Augen des Spielers)** – `r5_pov_bogen.jpg`, `r5_pov_ps_box.jpg`. *Gut:* In der Box schneidet die schwarze Kante quer
durchs Bild, der Klotz starrt über den Monitor; der Hinweis erscheint beim ersten Blick. Abnahme.

**Runde 6 (Abnahme von Level 2, Wesenbogen `docs/screens/level2/wesen_r4_1.jpg`)** – *Schlecht:* Im Spiel neben den
anderen Wesen wirkt der Klotz wie ein gezeichnetes Comic-Gesicht:
- Augen und Maul sind aus Flächenfarben gelegt (Iris als Scheibe, Zähne als Dreiecke), der Körper ist ein roter Kasten
  mit gemalten Adern.
- Auf der Seite sind es Fotos – davon ist nichts zu sehen.
- Der Monitor zeigt in der Mitte ein schwarzes Oval.

*Änderung:* Neubau mit echten Fotos (siehe Modell): Szenen in Blender/Cycles, Atlas aus Abzügen, Collage um den Klotz.
Bilder: `r6_ps_vorher_nachher.jpg` (links vorher, rechts nachher), `r6_ps_fotos.jpg` (der Atlas).

**Runde 7 (`docs/screens/level2/wesen_r5_1.jpg`)** – *Gut:* Aus 3 m liest sich jedes Bild als Foto: ein echtes
Hundemaul mit Zähnen und Speichel, zwei Augen, die nicht zusammenpassen, ringsum rohes Fleisch und Darm. Im Glitch
verrutschen die Scheiben mitsamt den Abzügen.

*Schlecht:*
- Zwischen den Augen flimmert ein Schachbrett: Eine abstehende Ecke stach durch die Augenfotos.
- Das Monitorbild sitzt 9 cm neben der Scheibe, das dunkle Glas schneidet ein Oval hinein. Ursache: Die Drehung des
  Monitors ist im Spiel gespiegelt (Blender-y), deshalb lag es schon in den Runden 1–5 daneben.
- Die unterste Scheibe hat Splitterdreiecke (Schnitt genau durch den Fuß).

*Änderung:* Unter dem Gesicht keine abstehenden Ecken, das rechte Auge eine Lage höher, Monitorbild mit gespiegelter
Drehung (`apparitions_l2.cpp`), Schnitt 5 cm unter dem Fuß.

**Runde 8 (`r7_ps_bogen.jpg`: Licht der Ebene, Lampe, Box; `r7_ps_nah.jpg`)** – Kein Flimmern mehr. Das Monitorbild füllt
die Scheibe (Fleisch mit Auge, rot, gestört). Aus der Ferne steht in einem hellen Büro ein Klotz aus blutigen Fotos, aus
der Nähe sieht man Fingerabdrücke und Datumsaufdrucke. Die Modellprüfung ist grün. Abnahme.

## Tests (`gameplay_test`, `testV133L2Rand`)

Aus der Ferne harmlos; näher als fünf Fuß schließt die Box ein (Meldung an das Opfer); hingeworfen holt der Arm es in den Bildschirm,
die Box fällt, er ist fort; ohne Gabe ausgeschnitten (Tod „photoshop“, Knirschen); durch die Wand der Box gegangen: ausgeschnitten;
Koop: Box mit Opfer und Meldung, Gabe beim Gast.

## Restmängel

- Keine Kamera im Spiel (siehe Interpretation); keine Gegenstandsbeine.
- Die Fotos sind Aufnahmen gebauter 3D-Szenen, keine echten Fotografien. Aus unter 1 m ist das Maul als sehr gutes
  Modell erkennbar; ab Spielabstand (die Box greift bei 1,52 m) liest es sich als Foto.
- Bei Textur 1024 (Standard) wird der 2048-Atlas halbiert. Die Datumsaufdrucke sind dann aus der Nähe weich.
- Dinge, die aus der Box geworfen werden, werden nicht „ausgeschnitten“ (nur Spieler).
