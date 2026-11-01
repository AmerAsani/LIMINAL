# Kreatur: Plague Goblins (Entity 25, Level 1)

Quellen (nur backrooms-wiki.wikidot.com, Lizenz jeweils CC BY-SA 3.0):
- [entity-25](https://backrooms-wiki.wikidot.com/entity-25) „Plague Goblins“ von The Dragon King, Revision 28 (zuletzt bearbeitet 28.07.2025), gelesen am 01.11.2026 (ganze Seite einschließlich der aufklappbaren Abschnitte über die Kasten und die Lager).
- [level-1](https://backrooms-wiki.wikidot.com/level-1), Revision 83, gelesen am 27.10.2026: Fußnote im Abschnitt über weitere Wesen.
- Zusammenfassung in eigenen Worten; kein Bild der Seiten verwendet, keine wörtlichen Zitate.

## Was die Seiten sagen

**Level 1 (Fußnote):** kleine Wesen, die man mit einem Messer oder einer Axt verscheuchen kann.

**Entity 25:**
- Benannt nach ihrer Ähnlichkeit mit mittelalterlichen Pestärzten. Klein, scheu, Mischwesen aus Katze und Vogel, huschen durch die Backrooms. Gelegenheitsdiebe, die in Gruppen sammeln; meist harmlos, aber für Alleinreisende eine echte Plage.
- Sie sammeln alles, was herumliegt: Essen, Holz- und Metallreste, Vorräte, manchmal Knochen und Teile toter Wanderer und Wesen. Was sie in den Umhang stecken, verschwindet spurlos – gleich wie groß. Ist eine Gegend leer, ziehen sie weiter.
- Allein fliehen sie beim Anblick eines Menschen oder Wesens. In Gruppen („swindle“, 9 bis 27) bestehlen sie gern Unaufmerksame, besonders Alleinreisende, Schwache und Alte.
- Eine Bande erscheint an einem Eintrittspunkt, abseits feindlicher Wesen. Drei Kasten:
  - **Späher:** dunkle Ledermasken mit Glasöffnungen über den Augen; ziehen sich bei Gefahr zurück und warnen die anderen – ein Hinterhalt kann folgen.
  - **Henchmen:** die Lastträger und Sammler, die meisten der Bande; glatte hellbraune oder weiße Masken ohne Merkmale.
  - **Torwächterinnen:** nur erwachsene Weibchen; breitkrempiger Hut, feineres Cape, reich verzierte Maske, meist ein Silberstock mit einer Sanduhr an der Spitze. Damit öffnen sie ein Portal, durch das die Bande kommt und geht.
- Feste Lager („wards“, 100–500 Tiere, Zelte aus Gesammeltem) sind schwer befestigt und mit Fallen gesichert; wer dort stiehlt, wird übel zugerichtet oder getötet.
- Laute: heiseres Krächzen, Kreischen, Schnattern; auf gefährlichen Ebenen Handzeichen. Sie verstehen menschliche Sprache, manche ahmen sie mäßig nach; manche Wanderer handeln mit ihnen und werden mit Vorräten und Warnungen belohnt.
- Sie meiden andere Wesen (ihre Nähe zeigt eine sichere Gegend an), sind aber Beute – vor allem der Hounds, die schnell genug sind. Darum fürchten und hassen sie Hounds; manche Banden jagen sie mit Fallen und Waffen.
- Körper: katzengroß, halb zweibeinig, 3,6–7,7 kg; Weibchen größer. Weiche, schwarze, fellartige Federn am ganzen Körper; geschuppte Glieder mit kräftigen, geschickten Krallen (klettern, halten, sich wehren). Unter der Maske ein Krähenschnabel voller rasiermesserscharfer Sägezähne. Riesige, stark spiegelnde Augen mit hellgelbem Schimmer im Dunkeln. Zwei lange, dünne Ohren mit Pinseln.
- Innen wie ein Vogel (Luftsäcke, Kropf, Muskelmagen, hohle Knochen): sehr schnelle Läufer, gemessen bis 84 km/h, und sehr wendig. Fressen alles Organische; gegen seelische Anomalien der meisten Ebenen unempfindlich.
- Ratschläge: Vorräte immer dicht bei sich halten; bei Anzeichen eines Lagers ohne Tauschware umkehren; Mandelwasser und Essen tauschen, wenn sie kommen; in der Enge schreien und mit den Armen fuchteln (sehr schreckhaft); hält der Angriff an, genügt es, einen zu verletzen oder zu töten – die Bande zieht sich zurück und kommt nicht wieder. Nicht: Vorräte offen liegen lassen, ein Lager angreifen, einem lebenden Goblin Maske oder Umhang abnehmen, sich mit plötzlichen Bewegungen oder unangekündigtem Lärm nähern, ihnen beschädigte Überreste ihrer Art zeigen, ihnen Waffen geben, sie verspotten.
- **Krankheitsträger?** Nein: „Plague“ steht nur für die Pestarztmasken; ihr Fleisch gilt sogar als sauber. Darum gibt es keine Übertragung (der Auftrag: nur falls die Seite es sagt).

## Umsetzung

**Modell** (`tools/blender/creatures/goblin_parts.py` mit `goblin.py`, `goblin_scout.py`, `goblin_gate.py`, `goblin_portal.py`; ein Skelett `anim::gb`, vier Varianten, `data/models/ent_goblin*_l0..l2.glb`, 48 000/15 000/5 000 Dreiecke, Torwächterin 60 000):
- *Körper:* Vogelbecken, Bauch, Brust mit Kiel, aufgeplusterte Halskrause; Federfell (dachziegelartige Zellen, Strähnen). Ohren wie beim Karakal: breit am Ansatz, spitz, innen nackte Haut, langer schwarzer Pinsel. Gefiederter Katzenschwanz mit Federfächer.
- *Glieder:* befiederte Oberarme und Schenkel („Hosen“ bis über die Ferse), geschuppte Unterarme, Läufe und Zehen; drei lange Finger und ein Daumen, drei Zehen vorn und eine hinten, alle mit gebogenen Hornkrallen.
- *Gesicht (unter der Maske):* kahl – grau-rosa, runzlige Haut, wo die Maske die Federn abgerieben hat (Auslegung); schwere Ober- und Unterlider halb über riesigen Bernsteinaugen mit senkrechter Schlitzpupille (die Katze, Auslegung); Krähenschnabel, seitlich zusammengedrückt, mit Hakenspitze und eigenem Unterkiefer; Sägezähne oben 9, unten 8 je Seite, schräg nach außen über den Schnabelrand (Säge wie beim Gänsesäger, Auslegung); roter Rachen, Zunge.
- *Masken* (am eigenen Knochen, Scharnier an der Stirn, ein Riemen unter dem Kinn): Gesichtsschale (hinten offen) und langer, schlanker, leicht gebogener Schnabel, der den echten Schnabel samt Zähnen ganz umschließt. Henchmen: glatt, hellbraunes gewachstes Leder, verschmutzt, zwei schmale Schlitze, durch die die Augen schimmern. Späher: dunkles Leder mit Längsnaht, runde Glasaugen in Messingfassung, Nieten. Torwächterin: Ochsenblut-Lack mit eingeschnittenen Ranken, Messingringe um den Schnabel, gefasste Augenöffnungen, Perlen auf dem First, Messingranken auf den Wangen.
- *Umhang:* sechs Bahnen (je Strang zwei Knochen), je Bahn drei gegeneinander versetzte Streifen (runde Falten), vorn offen; bis zur Hüfte, darunter die Vogelbeine; grobes, fast schwarzes Tuch, speckig, der Saum zerrissen (Fetzen und Löcher). Zurückgeschlagene Kapuze auf den Schultern. Torwächterin: tiefer reichendes Samtcape mit Schulterkragen und hohem Kragen.
- *Henchmen:* ein verschnürtes Bündel auf dem Rücken – zwei lange Knochen, ein Rohrstück, ein Lumpenbausch (die Seite: sie sammeln Holz- und Metallreste und Knochen toter Wanderer).
- *Torwächterin:* breitkrempiger Filzhut mit Band; Silberstock mit Zwinge und Ringen, oben die Sanduhr (zwei Glaskolben, Deckel, drei Säulen, glimmender Sand).
- *Beute:* eine Flasche in der linken Klaue (verschwindet beim Verstauen).
- *Portal* (Variante 3): ein aufrechter, mandelförmiger Riss über dem Boden (1,25 × 0,9 m), innen glänzend schwarz mit wirbelnden Schlieren, der Rand unregelmäßig, glühend wie rieselnder Sand, mit abstehenden Splittern; sechs Sandströme kreisen am Rand.

**Skelett und Bewegung** (`src/anim/creature_goblin.cpp`, `gb::makeSkeleton`, `CreatureAnimator::updateGoblin`, 50 Knochen):
- *Gehen:* kurze, schnelle Schritte auf den Zehen (Ferse hoch, Knie vorn); der Kopf bleibt im Raum stehen, bis er 4,5 cm zurückliegt, dann ruckt er vor (Tauben, Krähen). Henchmen immer gebückt (Lastträger), Späher geduckt, die Torwächterin aufrecht mit Stock (die Spitze setzt im Schritt auf).
- *Huschen:* auf allen vieren (die Arme werden Vorderbeine), Galopp mit Flugphase; Umhang und Schwanz fliegen nach hinten. Die Torwächterin huscht nie, sie läuft aufrecht.
- *Ruhe:* Kopfrucke (Sakkaden), einzeln zuckende Ohren, Krallen reiben aneinander; beim Suchen picken sie auf allen vieren am Boden.
- *Wahrnehmen:* erstarrt, Ohren steil, der Kopf schnellt herum (bis 2,1 rad zur Seite, Eule) und legt sich schief.
- *Licht:* der Arm auf der Lichtseite schirmt die Augen ab, duckt sich, der Kopf weicht aus.
- *Wühlen (Diebstahl):* hängt am Rücken des Opfers, die Füße krallen sich in den Stoff, die Krallen graben im Wechsel.
- *Beute:* hält sie hoch, dann verschwindet sie im Umhang.
- *Drohen:* richtet sich auf den Zehen auf, Rumpf steil, Hals vorgestreckt; die Maske fällt um den Kinnriemen nach unten weg und hängt vor der Brust – frei liegen das kahle Gesicht, die Augen und der aufgerissene Schnabel mit den Sägezähnen; die Arme spannen den Umhang seitlich auf, die Krallen erhoben; der Kopf stößt im Takt vor (zischender Vogel), der Körper wiegt sich. Die Torwächterin hebt nur den Stock.
- *Angriff:* ducken (0,27 s), Sprung im Bogen bis vor das Gesicht des Opfers (Kopf etwa 1,2 m hoch), Krallen voran, Biss, zurückfallen und weghuschen.
- *Tod:* kippt auf die Seite, Glieder zucken abklingend, die Maske rutscht ab und bleibt daneben liegen.
- *Portal:* springen heraus (wachsen aus der Mitte) bzw. hinein (schrumpfen zur Mitte); die Torwächterin hebt beim Öffnen und Schließen den Stock.

**Verhalten** (`src/gameplay/presence_goblin.cpp`, `Presence::Role::Goblin` = 17, Kin `goblin`, Level-JSON `presence.goblin`):
- *Ankunft:* auf dem Weg 24–38 m vom Spieler, außer Sicht (Sichtprüfung auf drei Höhen), mit Platz zum Herausspringen öffnet die Torwächterin das Portal. Klang vor Sicht: rieselnder Sand, ein Glaston, Krächzen. Die Bande springt einzeln heraus (Torwächterin zuerst).
- *Spähen:* die beiden Späher pirschen bis etwa 12 m Weg heran und spähen aus Deckung; Henchmen suchen am Portal (picken, heben Liegendes auf – auch Funde, die dann fort sind). Sieht man einen Späher länger an oder leuchtet ihn an, schreit er und rennt zurück – die Bande ist gewarnt, es folgt der Hinterhalt (Stehlen).
- *Stehlen:* nur wenn das Ziel allein ist und etwas Brauchbares trägt (Mandelwasser, Essen, Batterien – nie Sicherungen, nie das Beil). Zwei Diebe nähern sich, erstarren aber, solange man sie ansieht; die anderen umkreisen in 5–10 m und spähen. Hinter dem Rücken springt einer auf den Rucksack: es zerrt und raschelt (Ankündigung, Meldung „Etwas zerrt an deinem Rucksack!“), 1,2 s – wer sich umdreht, rennt, springt oder das Beil schwingt, schüttelt ihn ab. Sonst nimmt er ein Stück, hält es hoch, verstaut es und flieht zum Portal. Nur einer zugleich am Rücken.
- *Bedrängen:* ein Geschwächter (Gesundheit unter 45; je Bande mit Wahrscheinlichkeit `chance` = 0,6) oder wer einen Dieb mit Beute in die Enge treibt (unter 1,6 m, länger als 1,5 s). Ankündigung: die Masken fallen, Fauchen der Bande (1,4 s); dann springen bis zu drei nacheinander zu, jeder Sprung mit Kreischen und Ducken vorher, Biss 5 (× Schwierigkeit). Höchstens 9,4 s und nie bis zum Tod: unter 22 Gesundheit lassen sie ab (Auslegung: sie wollen die Vorräte, nicht das Leben).
- *Abzug:* zurück durch das Portal, mit Beute im Lauf; die Torwächterin wartet am Portal, hebt den Stock und geht zuletzt, dann schließt es. Wer keinen Weg zurück findet, flieht anders und verschwindet ungesehen.
- *Handel:* legt man Mandelwasser oder Essen (Energieriegel) hin (Ablegen, bis 9 m vom Spieler), holt es einer: die Bande wird freundlich (kein Diebstahl, kein Angriff), die Torwächterin tritt vor, bietet mit vorgestreckter Klaue an, versucht ein Wort („Hal-lo?“) und legt ein Geschenk hin (Batterie); die Späher warnen mit einem Schrei vor feindlichen Wesen bis 25 m. Nach 50 s ziehen sie ab.
- *Hounds:* ist einer näher als 30 m am Portal oder 25 m am Spieler, flieht die ganze Bande.
- *Einzeln verscheuchen* („schreien und fuchteln“, Auslegung): wer auf einen zurennt (Sprint, über 2 m/s auf ihn zu, unter 5 m) oder ihn länger als 0,7 s aus unter 6 m anleuchtet, verscheucht ihn für 2,5–4,5 s.
- *Beil* (`goblinSwing`): ein Treffer bis 1,8 m im Blickkegel (±44°) tötet einen Goblin – Todesschrei, was er im Umhang trug, fällt heraus; die Bande flieht und kommt in dieser Ebene nicht wieder (die Seite). Ein Schwung ohne Treffer verscheucht, wer bis 6 m davor steht (±60°).
- *Augen:* im Dunkeln schimmern sie (Grundleuchten 0,75), im hellen Raum schwach; im Lichtkegel leuchten sie auf (Rückstrahlen), dicht davor gedämpft.

**Das Beil** (`data/items.json` „hatchet“, Netz `makeHatchet` in `src/render/meshes.cpp`, Materialien `FM_ITEM_HATCHET_*`, Symbol in `texture_gen.cpp`): 42 cm Eschenstiel, ovaler Querschnitt, Knauf, geschmiedeter Kopf mit Bart und blanker Schneide. In Level 1 in den Funden (`spawn_rules.item.table` „hatchet“: 0,18; `supply_items` 0,06). Benutzen schwingt es (Geste `Body::Gesture::Swing`: über die Schulter ausholen, Hieb nach vorn unten), es wird nicht verbraucht. Hält man das Beil gewählt, kommen Diebe von vorn nicht näher als etwa 3 m (sie erstarren).

**Klang** (`src/audio/synth_goblin.cpp`): Krallentipper und Umhang (Schritte), Schnattern (Schnabelklappern unter der Maske, gehauchte Silben), heiseres Krähen-„Kraa“, Warnschrei in zwei Stößen, Wühlen am Rucksack (Stoff, Reißverschluss, Krallen auf Plastik), Verstauen (Rascheln, dumpfer Schlag, fallender gedämpfter Ton), Fauchen mit ratterndem Schnabel (1,4 s), Kreischen beim Sprung, Portal (Sand, Glaston, Einatmen bzw. Zusammenfallen), Todesschrei mit Aufschlag, Nachahmen („Hal-lo?“), Beil (Zischen, Treffer).

**Mehrspieler:** die Bande denkt nur beim Host. Die Puppen beim Gast bekommen Rolle, Kaste (Variante), Zustand und die Kanäle über `NetState` (fly = auf allen vieren bzw. Öffnung des Portals, growl = Drohen/Maske, strike = Sprung, bait = Beute, reach, jaw, talk, wave, plea, shiver, perceive, tilt, lean, glow); die Größe je Goblin folgt aus der Netzkennung (gleich bei Host und Gast). Diebstahl, Aufheben, Verlorenes und Geschenke rechnet der Host in der Sitzung (`src/gameplay/session_goblin.cpp`) auch für die Gäste (deren Inventar, Meldungen über `netPersonalOut`); das Beil eines Gasts geht als Eingabe an den Host (`pawnUse`). Gestohlen wird nur von Alleinstehenden – wer zusammenbleibt, wird nur beobachtet.

## Interpretation (nicht auf der Seite)
- Das Portal als mandelförmiger Riss mit Sand (die Seite sagt nicht, wie es aussieht).
- Kahles Gesicht unter der Maske, Schlitzpupillen, Sägezähne über den Schnabelrand, Knochenbündel der Henchmen.
- Diebstahl am Rücken (Rucksack), die Ankündigung durch Zerren, das Abschütteln.
- Sie lassen von Geschwächten ab, bevor diese sterben; der Sprung bis vor das Gesicht.
- Das Geschenk beim Handel ist eine Batterie; die Warnung als Schrei.
- Tempo im Spiel höchstens 6,5 m/s statt 84 km/h (sie sollen sichtbar bleiben; so sind sie trotzdem schneller als jeder Sprint).
- Schreien und Fuchteln: das Spiel hat keinen Ruf – Zurennen und Anleuchten übernehmen das.
- Hound-Teile (Passierschein der Seite) gibt es im Spiel nicht.

## Zahlen
| | Henchman | Späher | Torwächterin |
|---|---|---|---|
| Größe (Scheitel) | 0,50–0,61 m (× 0,86–1,06) | 0,55–0,59 m | 0,68–0,72 m (× 1,18–1,24), mit Hut 0,8 m |
| Tempo | Schleichen 1,4, Gehen 1,0–2,4, Huschen 4,5–6,5 m/s | wie Henchmen | Gehen 1,0–1,6, eilig 3,2 m/s |
| Aufgabe | Dieb (2), umkreisen, sammeln | spähen, warnen | Portal, Befehle, Handel |

Bande 9–12 (Level 1; die Seite 9–27), Auftritt alle 260 s (× 0,7–1,3), Portal 24–38 m Weg. Sehen: „angesehen“ ab Sichtstufe 0,3; Licht bis 9 m. Diebstahl unter 1,05 m hinter dem Spieler (über 1,9 rad aus dem Blick), 1,2 s. Bedrängen: Fauchen 1,4 s, Sprung 0,9 s, Biss 5, Pause 1,4–2,2 s, höchstens drei zugleich.

## Gleichgewicht
- Andere Wesen unverändert; die Goblins fliehen vor ihnen (Hound) bzw. warnen vor ihnen.
- Der Verlust ist begrenzt: höchstens zwei Diebstähle je Bande (danach Abzug), nie Sicherungen oder das Beil. Wer zurückschlägt, bekommt das Gestohlene zurück (es fällt aus dem Umhang).
- Bedrängen nur bei Schwäche oder Provokation, nie tödlich; jeder Biss angekündigt.
- `supply_balance_test` unverändert grün (die alten Ebenen kennen das Beil nicht); die Funde in Level 1 bleiben 8–12, ein Teil davon ist jetzt ein Beil.

## Wow-Check
„IIIH, WAS IST DAS FÜR EIN VIEH?“ – ehrlich je Runde: Runde 1–2 „albern“ (Karottenmaske, Fühler-Ohren, die unter dem Hut wie erhobene Arme aussahen, Vogelscheuchen-Arme, Kegel bis zum Boden). Runde 3–4 „ganz okay, eher niedlich“ (im Raum eine Armee kleiner Spielfiguren; beim Drohen geduckt wie Eulen). Ab Runde 5: ja, aus der Nähe – wenn die Maske fällt und das kahle Gesicht mit den Bernsteinaugen und dem aufgerissenen Sägeschnabel sich aufrichtet, die Krallen erhoben (`r5_i_gesicht_schraeg.jpg`, `r7_i_gesicht_schraeg.jpg`); die Bande, die sich vor dem Riss sammelt und in Schritten heranschiebt, wirkt durch die Zahl. Aus der Ferne in hellen Räumen bleiben sie kleine dunkle Gestalten – das Grauen kommt dort aus dem Klang, dem Zerren am Rucksack und den Augen im Dunkeln.

## Kritikrunden
Bilder: `docs/screens/creatures/plague_goblins/` (Vorführungen `rN_a…n`, Szenen `rN_s1…s7`).
1. **Runde 1** – schlecht: Henchman-Maske dick und knollig wie eine Karotte, fleckig; Ohren lang und dünn wie Fühler; der Umhang ein Kegel bis zum Boden; beim Drohen ragt die Maske wie ein Horn, die Arme wie eine Vogelscheuche; die Augen flache, leuchtende Comic-Scheiben; das Portal ein regelmäßiger Feuerring. Ändert: Maske, Ohren, Umhang, Drohen, Augen, Portal neu. Bilder: `r1_a_henchman_nah.jpg`, `r1_e_maske_hoch.jpg`, `r1_d_torwaechterin.jpg`, `r1_h_portal.jpg`.
2. **Runde 2** – schlecht: die Ohren der Torwächterin durch die Krempe gesteckt lesen sich wie erhobene Arme; Maske hochgeklappt noch immer ein Horn; Capebahnen wie Bretter; die Torwächterin grell rot wie Spielzeug; im Dunkeln keine Augen. Ändert: kurze, breite Karakal-Ohren mit Pinsel und nackter Innenseite, kahles Gesicht mit Lidern, schlanker Lederschnabel, Ohren der Torwächterin unter den Hut, Ochsenblut, Augenschimmer im Dunkeln. Bilder: `r2_a_henchman_nah.jpg`, `r2_d_torwaechterin.jpg`, `r2_j_torwaechterin_nah.jpg`.
3. **Runde 3** – schlecht: das Portal blieb in der Szene unsichtbar und die Augen dunkel – Zustand, Größe und Leuchten gingen beim Host nie an die Animation (nur bei Puppen und Vorführungen); im Raum eine Armee gleicher grauer Kegel mit weißen Masken (Spielzeug). Ändert: Übergabe an die Animation nach jedem Denken; Umhang fast schwarz und speckig, Maske hellbraun und verschmutzt, Henchmen gebückt, Picken auf allen vieren; Falten statt Bretter. Bilder: `r3_s1_portal_auftritt.jpg`, `r3_s3_bande_lampe.jpg`.
4. **Runde 4** – schlecht: hochgeklappt wirkt die Maske jetzt wie ein Zauberhut; beim Drohen sitzen sie geduckt wie kleine Eulen, der Schnabel mit den Zähnen ist aus 2–3 m nicht zu sehen; aus der Nähe im Lampenlicht leuchten die Augen wie Kürbislaternen. Ändert: die Maske fällt um den Kinnriemen nach unten weg; aufgerichtetes Drohen mit gespreiztem Umhang und erhobenen Krallen, Kopfstoßen; Sägezähne größer und schräg über den Rand, heller Rachen, mehr kahle Haut; Augenschimmer nah gedämpft; Sprung bis vor das Gesicht. Bilder: `r4_s1_portal_auftritt.jpg`, `r4_s7_abzug.jpg`, `r4_e_maske_hoch.jpg`.
5. **Runde 5** – besser: das Gesicht wirkt (aufgerichtet, Sägeschnabel, kahle Haut, Krallen); schlecht: aus der Ferne noch gleiche aufrechte Figuren. Ändert: Bündel aus Knochen, Rohr und Lumpen auf dem Rücken der Henchmen (Silhouette, die Seite: sie sammeln Knochen toter Wanderer), Picken die halbe Zeit, Haltung je Goblin verschieden. Bilder: `r5_i_gesicht_schraeg.jpg`, `r5_e_maske_hoch.jpg`, `r5_s6_angriff.jpg`.
6. **Runde 6** – schlecht: der knielange Umhang bildet noch Glocken wie Figurinen, die Beine sieht man nicht. Ändert: Cape bis zur Hüfte, darunter die Vogelbeine; Torwächterin mit längerem Samtcape. Bilder: `r6_l_henchman_hinten.jpg` (Bündel), `r6_n_drohen_ich.jpg`, `r6_s3_bande_lampe.jpg`.
7. **Runde 7** – besser: gebückte Gestalten mit Bündel und Vogelbeinen, das Drohen mit offenem Gesicht wirkt; schlecht: die größeren Zähne stachen unten durch den Maskenschnabel (Torwächterin). Ändert: Maskenschnabel unten tiefer (Prüfbilder `r7b_*`). Bilder: `r7_i_gesicht_schraeg.jpg`, `r7_b_henchman_seite.jpg`, `r7b_j_torwaechterin_nah.jpg`, `r7b_o_torwaechterin_seite.jpg`.

## Tests (`gameplay_test`, `testV133PlagueGoblins`)
Level 1 kennt sie (Banden ab 9), Level 2 nicht; das Beil ist ein eigener Gegenstand und liegt in mindestens 8 von 16 Läufen in den Funden von Level 1. Erzwungene Bande: 9–12 Körper, eine Torwächterin, zwei Späher, Portal außer Sicht (über 12 m), Klang vor Sicht; alle springen aus dem offenen Portal; Netzzustand (Rolle 17, Kasten, Portal). Wer zur Bande sieht, wird nicht bestohlen; von hinten zerrt es am Rucksack (Ankündigung), mindestens 1 s später ist genau ein Stück fort, ohne Schaden. Beil: ein Schwung auf 3,5 m verscheucht (Kreischen, kein Treffer), das Beil wird nicht verbraucht; ein Treffer auf 1,1 m tötet einen Goblin, Todesschrei, die Bande flieht durch das Portal und kommt in 120 s nicht wieder. Geschwächter: Masken und Fauchen, der erste Biss frühestens 1,3 s danach und nach einem Kreischen, es wird gebissen, aber nicht bis zum Tod, danach Abzug. Handel: hingelegtes Mandelwasser wird genommen, Geschenk (Batterie), kein Diebstahl und kein Angriff. Ein Hound in der Nähe: die Bande flieht.

## Restmängel
- Aus der Ferne in hell beleuchteten Hallen lesen sie sich noch als kleine, dunkle Figuren; der Schrecken trägt dort über Klang und Verhalten, nicht über die Silhouette.
- Das Ablegen von Mandelwasser zum Handeln wird im Spiel nicht erklärt (nur über die Seite und Notizen erfahrbar).
- Die Ego-Hand zeigt das Beil nur während des Schwungs (es wird nicht dauerhaft getragen).
- Das Wühlen am Rücken ist in der Ego-Sicht nur zu hören und zu lesen; sichtbar ist der Goblin am Rücken nur für Mitspieler.
- Lager („wards“) mit Fallen sind nicht umgesetzt (die Karte von Level 1 hat dafür keinen Ort).

## Wünsche an die Karte
- Ein kleiner Raum oder eine Nische in Level 1 als Lager einer Bande (Zelte aus Gesammeltem, Fallen) – würde die „wards“ der Seite möglich machen.
