# Kreatur: Growler (Entity 130)

Quelle: [backrooms-wiki.wikidot.com/trimmed:entity-130](https://backrooms-wiki.wikidot.com/trimmed:entity-130), Revision 67 (zuletzt bearbeitet 13.05.2025), gelesen am 27.10.2026. Die Seite liegt im Trimmed-Bereich (archiviert im Mass Trimming Project); `entity-130` selbst existiert nicht, Level 1, 2 und 5 verlinken genau auf diese Seite.
- Geschrieben von CementSandwich (Bild auf der Seite: Aether48).
- Lizenz CC BY-SA 3.0.
- Dazu die Ebenenseiten: Level 1 (Rev. 83), Level 2 (Rev. 93), Level 5 (Rev. 184), alle CC BY-SA 3.0.
- Zusammenfassung in eigenen Worten; kein Bild der Seite verwendet, keine wörtlichen Zitate.

## Was die Seite sagt

**Gestalt:**
- Riesig, massig, grob menschenähnlich, ständig vornübergebeugt (der tentakelartigen Glieder wegen).
- Der Körper ist eine Masse bunter Pfeifenreiniger. Größe 4 bis 15 Fuß (1,2 bis 4,6 m).
- Kurze Beine, lange Arme; alle Glieder enden in einem Gewirr aus Draht und Pfeifenreinigern, das hinter ihm schleift.
- Am Rumpf viele kurze, spitze Drahtfortsätze wie kleine Rippen; dazu peitschenartige Ranken.
- Die Brust lässt sich öffnen. Völlig blind, keine Organe außer einem zentralen Nervensystem.
- Level 1: aus Kabeln, sehr stark, wenig intelligent; der „Kopf“ ist eine Stelle am Brustkorb, an der die Kabel auseinanderlaufen.
- Level 2: Junge sind sehr klein; ihre Glieder sind leuchtend bunt und selbst in völliger Dunkelheit zu sehen.

**Verhalten:**
- Wandert ziellos; orientiert sich mit einer Art Echoortung. Nimmt er Beute wahr, kommt er schnell.
- Angriff: telepathisches „Knurren“ (weißes Rauschen) verwirrt und erschreckt – große verursachen aus der Nähe Hirnschäden; er torkelt heran, Hunderte der Drahtrippen fahren aus und spießen auf, die Ranken schlagen zu; danach öffnet er die Brust, nimmt die Beute auf und wächst.
- Tödlich. Treffen zwei zusammen, verschmelzen sie zu einem doppelt so großen und klügeren Wesen.
- Beim Betreten einer neuen Ebene ist sein einfaches Gehirn überfordert – oft stirbt er am Schock.
- Level 2: Junge jagen Death Rats, Deathmoths und andere kleine Beute; in engen Gängen verschmelzen sie versehentlich, bis einer nicht mehr hindurchpasst und verhungert; solche Leichen versperren manchmal den Weg (die Pfeifenreiniger lösen sich erst Stunden danach). Leichen sind häufig.
- Sie senden WLAN aus und laden elektronische Geräte auf (abtrünnige Gruppen nutzten gefangene als Ladestation – nicht empfohlen).

**Klang:** das telepathische Knurren/Rauschen in 10 bis 80 m Umkreis; daran erkennt man ihn.

**Ausweg (Seite):** meiden, schnell weglaufen; sich nicht auf das Rauschen konzentrieren; gestellt: Büschel herausreißen, auf den Kopf zielen; Level 1: schwere Gegenstände auf den Kopf werfen (verwirrt ihn), dann fliehen.

**Nicht gesagt:** Bewegungsgeräusche, Reaktion auf Licht (er ist blind), Tempo, Zahlen.

**Ebenen:** Level 1 (häufig), Level 2 (halbwegs häufig, Leichen häufig), Level 5 (nur genannt).

## Umsetzung

**Modell** (`tools/blender/creatures/growler.py`, Skelett `gw` mit 47 Knochen, 2,1 m gebeugt): ein schwarzer, verfilzter Drahtkern (nur Form) – darüber über 700 einzelne Pfeifenreiniger am Rumpf, je ein Strang als Rohrkette, der über die Oberfläche läuft (quer, schräg, längs – gewickelt, durcheinander), weitere an Armen, Beinen, Händen und Füßen, in vier verblichenen, verdreckten Farben (Magenta, Türkis, Gelb, Grün; helle Grundebene mit Faser-Mikroebene, matt – Magenta und Türkis leuchten bei den Jungen) und dazwischen schwarze, glänzende Kabel. Die Enden stehen ab und zeigen den blanken Draht; Hunderte blanke Drahtspitzen stehen aus der Masse. Kein Kopf: auf der Brust ein Knoten – ein dunkles Loch, aus dem Kabel und blanke Drähte sternförmig laufen. Zwei Brustklappen aus wirrem Geflecht über einer Höhle mit dunklem Gewirr und einem blassen Nervenstrang (das einzige Organ). Sechs Bündel Drahtrippen (in Ruhe 10–20 cm Stummel), drei geflochtene Peitschenranken am Buckel. Arme bis zum Boden, an Händen und Füßen schleifendes Gewirr. Materialien `FM_ENT_PIPE_A..D`, `FM_ENT_WIRE` (blank, metallisch), Kabel `ent_darkskin`, Kern `ent_hair`.

**Bewegung** (`src/anim/creature_growler.cpp`):
- *Ruhe*: wiegt sich langsam, die Fasern zucken, eine Ranke hebt sich tastend; die Ranken hängen über den Rücken.
- *Wahrnehmen*: bleibt stehen, der Knoten dreht und zittert (Klicks), der Oberkörper wendet sich zum Geräusch, die Ranken heben sich.
- *Jagen*: weit vorgebeugt, Arme vor, torkelnd (Rollen von Seite zu Seite, ungleich schnelle Schritte).
- *Ausholen* (Ankündigung, 1,1 s): richtet sich auf, Arme weit, die Drahtrippen sträuben sich zitternd, die Ranken steigen.
- *Angriff*: Satz nach vorn, die Arme umschlingen, die Rippen fahren 28 cm aus, die Ranken peitschen über den Rücken nach vorn; beim Zugriff öffnet sich die Brust.
- *Rückzug/Verwirrt*: taumelt, schlägt um sich, der Knoten dreht durch, dann zieht er ab.
- *Tod*: sackt vornüber zusammen, Glieder flach ausgebreitet, die Ranken schlaff (Leiche in Level 2).
- Das Gewirr an Händen und Füßen und die Ranken sind Ketten mit Trägheit (schleifen am Boden). Größe über den Maßstab je Knochen (neu: `CreaturePose::s`, `FigureRig` skaliert die Knochen).

**Verhalten** (`src/gameplay/presence_growler.cpp`, Level-JSON `presence.growler`):
- *Blind*: Licht ist ihm gleich. Er klickt (alle 2,5–4 s, jagend alle 0,6–1 s); jedes Klicken ortet Bewegung bis 13 m (jung 7 m, mit der Größe mehr) bei freier Bahn; wer still steht, wird nur bis 3,2 m gefunden. Rennen hört er (Lärm × 22 m Weg, jung × 14).
- *Rauschen*: 10 bis 80 m Umkreis nach der Größe (10 + 50 × (Maßstab − 0,5)), durch Wände; beim Jagen voll, sonst 55 %; wer ihn ansieht, dem wird es um 30 % stärker („nicht auf das Rauschen konzentrieren“). Bild: Bildrauschen wie zwischen Sendern, Farbsäume, Entsättigung, das Bild schwankt; Klang: Rauschen im Kopf (ohne Richtung). Aus der Nähe (6 m) zehrt es am Verstand, große (ab Maßstab 0,9) schaden dem Kopf (bis 1,2 Health/s).
- *Jagd*: torkelt mit 3,0 m/s (verschmolzen 3,4, jung 1,9; ruckartig 72–128 %) zur letzten Ortung; ohne neue Ortung 6 s: er verliert einen (dumm).
- *Angriff*: nah (2,3 m) holt er 1,1 s aus (Ankündigung: Klang und Haltung), dann der Stoß – ausgewachsen tödlich (Zugriff mit Kamera), jung ein Treffer (14 Health), danach lässt er ab. Nach einem Zugriff wächst er.
- *Geworfener Gegenstand*: trifft ihn ein Gegenstand (Aufprall am Körper), ist er 5–7 s verwirrt (Level 1); landet einer bis 14 m weit, geht er dem Aufprall nach.
- *Verschmelzen*: treffen sich zwei (beide nicht jagend), nimmt der größere den kleineren in 3 s auf (der kleinere schrumpft in ihn hinein); neue Größe = Kubikwurzel der Volumensumme × 1,08 (zwei gleiche: × 1,36).
- *Level 2*: Junge (Maßstab 0,45–0,58), zu zweit oder dritt, leuchten im Dunkeln (Glühen 2,6), jagen Death Rats (nehmen sie auf, wachsen ein wenig), verschmelzen; wird einer zu groß für einen Gang (Maßstab > 1,1, Deckenhöhe < 2,15 × Maßstab), steckt er fest, zappelt und verhungert nach 70–100 s; Leichen liegen in den Gängen (Anteil `chance`), wer hindurch will, watet langsam durch das Gewirr (Tempo × 0,45).
- *WLAN*: in der Nähe (Rauschen > 0,25) lädt sich die Lampe auf (3 % je Sekunde × Stärke).

**Klang** (`src/audio/synth_growler.cpp`): `growler_static` (Schleife: wandernder Bandpass wie zwischen Sendern, Knistern, pulsierende Störung, tiefes rauhes Drahtbrummen), `growler_click_0..3` (metallische Klickfolgen, schneller werdend), `growler_drag_0..2` (dumpfer, weicher Aufschlag, Draht kratzt über Beton, Rascheln), `growler_surge` (Ausholen: Rauschen schwillt, Draht kreischt unter Spannung), `growler_impale`, `growler_merge` (Draht verdreht sich knarrend, zwei Brummen schweben), `growler_collapse`.

**Mehrspieler:** die KI läuft beim Host; die Puppe beim Gast bekommt Größe (Kanal `fly`), Leuchten (`glow`), Haltung (`alert`, `perceive`, `crouch` = Ausholen, `reach` = Stoß, `strike` = Ranken, `jaw` = Brust, `shiver` = verwirrt, `hang` = Leiche) und Zustand; das Rauschen rechnet der Gast aus der Puppe selbst.

**Interpretation** (die Seite sagt es nicht): die Zahlen (Sinne, Tempo, Schaden); dass Stillstehen vor der Echoortung schützt; dass ein geworfener Gegenstand am Körper (nicht nur genau am Kopf) zählt; dass ihn das Ansehen stärker rauschen lässt; die Klicks als Klang der Echoortung; die Lampe, die sich auflädt; die Wachstumsformel.

**Nicht umgesetzt:** Tod durch Schock beim Ebenenwechsel (Wesen folgen nicht durch Ausgänge); Büschel herausreißen (das Spiel hat keinen Nahkampf); Hirnschaden als bleibende Folge; Größen über 1,8 (4,6 m) – die Gänge sind 2,8 m hoch.

## Gleichgewicht

| Wert | Zahl |
|---|---|
| Auftritt | Level 1 ~330 s (einzeln, ausgewachsen), Level 2 ~210 s (2–3 Junge; Leichen alle 60–120 s mit 50 %), Level 5 ~450 s |
| Sinne | Echoortung 13 m (jung 7 m) bei Bewegung, 3,2 m im Stillstand; Lärm × 22 m Weg |
| Tempo | Wandern 0,65 m/s, Jagd 3,0 m/s (gehen 3, sprinten 5,7 m/s) |
| Schaden | ausgewachsen: Stoß tödlich nach 1,1 s Ankündigung; jung: 14 Health; Rauschen bis 1,2 Health/s (nah, groß) |
| Ausweg | stehen bleiben (er findet einen nur nah), wegrennen (er verliert einen nach 6 s), Gegenstand werfen (5–7 s verwirrt) |

## Wow-Check

„Ein gebückter, grob menschlicher Haufen aus verdreckten bunten Pfeifenreinigern und schwarzen Kabeln, ohne Kopf – nur ein Loch in der Brust, aus dem blanke Drähte stehen –, der im Halbdunkel klickend auf einen zutorkelt, während das Bild zu rauschen beginnt.“

Ehrliche Antwort je Runde: Blender-Runde und Runde 1 „ganz okay“ bis „albern“ (Wollmonster) – deshalb neu gebaut (Runden 2–4). Ab Runde 4: ja – das schwarze Loch mit den Drähten wirkt wie ein Maul ohne Gesicht, die Masse liest sich als Draht, das Rauschen macht ihn grau und nah; im Lampenlicht und als leuchtendes Junges im Dunkeln ist er unverwechselbar.

## Kritikrunden

Bilder: `docs/screens/creatures/growler/`.

| Runde | Was ist schlecht | Was ändert sich | Bilder |
|---|---|---|---|
| Blender | Wenige, dicke Stränge auf einem schwarzen Kern: eine schwarze Puppe mit Neonschnur, nicht eine Masse aus Pfeifenreinigern; der Knoten sieht aus wie eine Zielscheibe (Auge), die Klappen wie ein Grill, die Beine schwarz. | Dreimal so viele Stränge (Radius 2,1 cm), Knoten als verdrehtes Knäuel, Klappen schief und ausgefranst. | `r0_blender.jpg`, `r0b_blender_dichter.jpg` |
| 1 (Level 1) | Dicht, aber wie Wolle: satte Farben, weich, „Teppich-Monster“ statt Grauen; die Beine noch schwarz; die Ranken stehen wie Hörner; die Drahtrippen sieht man in Ruhe nicht. Das Rauschen wirkt (`r1_rauschen.jpg`: entsättigt, Farbsäume, Korn) – und macht ihn grauer und unheimlicher. | Stränge dünner (1,4 cm), doppelt so viele, verblichene, verdreckte Farben, Beine dicht umwickelt, 160 blanke Drahtspitzen, Rippen länger (10–20 cm Stummel, ausgefahren 28 cm mehr), Ranken hängen in Ruhe und heben sich erst beim Lauschen/Jagen (Wahrnehmen wird sichtbar). | `r1_bogen.jpg`, `r1_a_nah.jpg`, `r1_j_lampe.jpg`, `r1_rauschen.jpg` |
| 2 | Liest sich jetzt als Draht- und Pfeifenreinigermasse; im Lampenlicht stark. Aber: die Brustklappen wirken noch wie ein Korb; der Knoten ist kein Blickfang; Junge in Level 2 im Dunkeln kaum zu sehen (`c_jung_dunkel`). Verschmolzen im Gang (`d_gross`) füllt er den Tunnel – gut. | Schwarze, glänzende Kabel zwischen den Strängen (Level 1: aus Kabeln), Klappen dichter und wirr, Knoten als dunkles Loch mit 46 blanken Drähten, Junge leuchten 2,6-fach. | `r2_bogen.jpg`, `r2_a_nah.jpg`, `r2_j_lampe.jpg`, `r2_level2_jung_gross.jpg` |
| 3 | Kabel und Loch wirken: dunkler, bedrohlicher, ein Blickfang auf der Brust (`r3_h_knoten`), die offene Brust zeigt Höhle und Nervenstrang (`r3_g2_brust_nah`). Junge leuchten immer noch nicht – Ursache: der Shader blendet Leuchten bei dunklen Texturen aus (die Pelz-Ebene ist zu dunkel). Die Klappen bleiben als senkrechte Gitterstäbe erkennbar. | Pfeifenreiniger auf die helle Zahn-Ebene (Fasern aus der Mikroebene, Rauheit × 2,6, Tönung umgerechnet): Junge leuchten jetzt neonfarben im Dunkeln (`r3_jung_leuchtet_dunkel.jpg`). Klappen aus denselben gewickelten Strängen wie der Rumpf. | `r3_bogen.jpg`, `r3_h_knoten.jpg`, `r3_g2_brust_nah.jpg`, `r3_jung_leuchtet_dunkel.jpg` |
| 4 | Klappen fügen sich ein (kein Korb), Fasern aus der Nähe wie echte Pfeifenreiniger (`r4_g2_brust_nah`). Jagdfolge im Spiel (`r4_jagd_rauschen_zugriff.jpg`): Rauschen ab 11 m, Annähern, Stoß, Zugriff mit Kamera ins Gewirr – stark. Schwäche: das Ausholen dauert nur 1,1 s und ist in der Folge kaum zu sehen; das Junge leuchtet auch im Hellen grell. | Belassen (die Ankündigung hört man deutlich: Rauschen schwillt, Draht kreischt). | `r4_bogen.jpg`, `r4_jagd_rauschen_zugriff.jpg`, `r4_b_rauschen_5m.jpg`, `r4_e_stoss.jpg`, `r4_f_zugriff.jpg` |
| 5 | Level 2: Junge leuchtend bunt im Dunkeln, ein verschmolzener füllt den Gang (`r5_level2_jung_gross.jpg`). Die Leiche steht noch halb gebückt – liest sich nicht als tot. | Totpose: Becken tiefer, Rumpf 1,42 rad vornüber, Hüfte 0,95 rad. | `r5_level2_leiche_jung.jpg`, `r5_level2_jung_gross.jpg` |
| 6 | Die Leiche ist zusammengesunken (`r6_leiche_liegt.jpg`); im engen Gang ein Hindernis aus Draht. Liegt sie nicht ganz flach (das Gewirr trägt), passt das zur Seite (Pfeifenreiniger lösen sich erst Stunden später). Abgenommen. | – | `r6_leiche_liegt.jpg` |

## Tests (`gameplay_test`, `testV133GrowlerNguithr`)

Level 1: erscheint ausgewachsen (Maßstab 0,85–1,2); wer still in 9 m steht, wird auch im Lampenlicht nicht gefunden (blind); er klickt; das Rauschen ist schon auf Abstand da; wer sich bewegt, wird geortet und getötet, das Ausholen kommt mindestens 1 s vorher; ein geworfener Gegenstand verwirrt ihn, dann entkommt man. Level 2: Junge klein und zu mehreren, zwei verschmelzen, einer weniger, der andere wächst; ein großer schadet aus der Nähe mit dem Rauschen; durch eine Leiche nur langsam.

## Restmängel

- Das Ausholen ist kurz (1,1 s) und optisch schwach – die Ankündigung trägt vor allem der Klang.
- Leichen-Pose nicht ganz flach; Ferne Stufe (L2) mit 21 000 Dreiecken (viele Drahtspitzen lassen sich schlecht vereinfachen).
- Verschmelzen ist nur an zwei Jungen in Level 2 ausgelegt; ein Growler folgt nicht durch Ausgänge (kein Schock-Tod).
