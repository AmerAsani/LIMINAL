# Kreatur: Smiler (Entity 3)

Quelle: [backrooms-wiki.wikidot.com/entity-3](https://backrooms-wiki.wikidot.com/entity-3), gelesen am 26.10.2026.
- Autor: Palico22; Neufassung: Stretchsterz.
- Lizenz CC BY-SA 3.0.
- Eigene Worte; kein Bild der Seite verwendet.

## Was die Seite sagt

**Erscheinung:**
- Erkennbar an Augen und Zähnen, die im Dunkeln spiegeln und leuchten.
- Ein langes Lächeln mit vielen spitzen Zähnen und weiß leuchtende Augen.
- Der Körper ist unbekannt; vielleicht mit bloßem Auge nicht sichtbar.
- Unbestätigt: fremdartige, unnatürlich geknickte Glieder.

**Ort:** auf den meisten Ebenen, vor allem in dunklen Bereichen.

**Verhalten:**
- Feindselig. Licht zieht ihn an; er verfolgt Lichtträger.
- Er greift an, wenn jemand in Panik zurückweicht oder Lärm macht.
- Rat: Blickkontakt halten, langsam weggehen, leise sein, Licht aus. Nicht rennen, solange er nicht jagt.

**Laute:** keine beschrieben.

## Umsetzung

**Grundsatz:** Fast nur Augen und Zähne sind zu sehen. Der Körper ist dunkle, matte Masse an der Grenze des Sichtbaren. Kein Comic-Gesicht.

**Gesicht:**
- **Keine Symbole.** Die Augen sind keine Kreise, sondern feuchte, spiegelnde Augäpfel in tiefen Höhlen. Leicht ungleich groß und ungleich hoch; das Weiß leuchtet, ohne Iris.
- Das Lächeln ist kein Bogen. Ein Maul in einem Schädel:
  - Ober- und Unterkiefer mit zurückgezogenen, dunklen, nassen Lippen;
  - auf jeder Seite reicht es weiter nach hinten als die andere;
  - der Mundwinkel steigt links höher.
- **Zähne:**
  - viele, schmal und spitz, ungleich lang;
  - einige schief, einige abgebrochen, Lücken;
  - eine zweite, kleinere Reihe dahinter;
  - sie leuchten matt und ungleich.
- Der Kopf ist nur im Lampenlicht als dunkler, glatter Schädel zu erahnen.

**Körper:**
- Dunkle Masse, die sich nach unten verliert, als Rauch mit Rand.
- ~~Andeutungen langer, geknickter Glieder~~ – in Runde 2 gestrichen (E-021): im Licht lasen sie sich als Strichmännchen-Arme. Statt dessen steigen dünne Rauchzungen aus dem Hinterkopf.
- Kein vollständiger Körper, keine Beine.

**Material:**
- Kopf und Masse schwarz und matt, mit leichtem Glanz an Kanten.
- Lippen und Zahnfleisch dunkel und nass.
- Zähne und Augen leuchten.

**Bewegung:**
- Schweben knapp über dem Boden.
- Der Kopf dreht sich langsam zum Spieler und neigt sich.
- Das Lächeln öffnet sich weiter; Lidschlag. Die Masse zieht nach.

**Abgrenzung:** einziges Wesen ohne festen Körper; das Gesicht trägt alles.

## Prüfkriterien

- Im Dunkeln: zwei ungleiche, leuchtende Augen und eine ungleichmäßige Zahnreihe – unheimlich, nicht lustig.
- Im Lampenlicht: ein Schädel mit Maul, Lippen und Zahnfleisch erkennbar; der Körper als dunkle, zerfasernde Masse.
- Aus der Nähe: Zähne einzeln, ungleich, mit Tiefe (zwei Reihen), keine flache Scheibe.

## Kritikrunden (Spielkamera und Vorführkamera)

Bilder: `docs/screens/creatures/smiler/` (vorher: `docs/screens/creatures/vorher/smiler_*`). „feste Kameras“ = dieselben Kameras wie vorher (nah, mittel, fern, Seite, bewegt, Lampe, dunkel); „nah“ = Vorführkamera (vorn, Kopf, Seite, hinten, oben, unten, Lauf, Lampe).

| Runde | Befund | Änderung |
|---|---|---|
| 1 | Im Spiel sah der Smiler unverändert aus. Das Protokoll zeigte `Quelle prozedural`: Die neuen glb lagen nicht im Laufverzeichnis. | Bauskript kopiert die Daten nach jedem Export; Prüfung der Quelle im Protokoll vor jeder Bewertung. |
| 2 | Erste echte Fassung wirkte wie ein Comic: glänzendes Ei als Kopf, riesige ovale Augen, dünner Lächel-Strich, Arme wie ein Strichmännchen. | Kopf aus Rauchmaterial, eigene nasse Lippen, Maul weiter geöffnet, größere Zähne, schmalere Augen, Arme gestrichen. |
| 3 | Noch zu nah am bekannten Bild: Grinsen symmetrisch, Zähne zu regelmäßig. | Schiefe Mundlinie (links deutlich höher), Lücken (13 %), mehr lange Nadeln, Zähne stärker geneigt. |
| 4 | Augen als flache ovale Scheiben; Zahnfleisch als Perlenschnur; Maulspalt mit Zacken; eine Rauchzunge neben dem Kopf las sich als Ohr oder erhobene Hand. | Formteil `tube` für Lippen, Zahnfleisch und Spalt; Mandelaugen mit spitzen Winkeln; Lippen laufen zu den Winkeln aus; Zungen dünner; Hinterkopf mit groben Beulen. |
| 5 | Im Dunkeln und im Lampenlicht stimmig: ungleiche Mandelaugen, nasses Zahnfleisch, zwei Reihen ungleicher Zähne. Abgenommen. | – |

**Bleibt offen:**
- Im hellen Licht ist der Kopf von hinten noch recht rund.
- Aus manchen Winkeln liest sich eine Rauchzunge kurz als erhobene Hand. Im Dunkeln, wo der Smiler lebt, ist das nicht zu sehen.

**V13.2 (Modellier-Analyse):** Im hellen Raum las sich der Kopf als glatte, glänzende Kugel. Schädeldecke und Seiten sind jetzt uneben, porig und runzlig; Augen- und Mundpartie bleiben glatter. Vorher/Nachher aus 1 m: `docs/screens/analyse_v13_2/smiler_vorher_nachher_nah.jpg`. Im Dunkeln (wo er im Spiel erscheint) bleiben nur Augen und Lächeln zu sehen.

## V13.3: Leuchtstab als Köder (Level 1)

**Quellen** (Wikidot, CC BY-SA 3.0, eigene Worte, keine Bilder): [level-1](https://backrooms-wiki.wikidot.com/level-1) Rev. 83 (07.10.2026, Praetor3005 und DivineAtlas), Wesenliste; [entity-3](https://backrooms-wiki.wikidot.com/entity-3) Rev. 83 (07.08.2026); [level-3](https://backrooms-wiki.wikidot.com/level-3) Rev. 18 (26.09.2026, Natedagreat563).

**Was die Seiten sagen:** Smiler werden von Licht angezogen und – sobald sie Beute entdeckt haben – auch von Lärm (Level 1). Rat: ruhig bleiben, das Licht ausschalten oder die Lichtquelle als Köder wegwerfen (Level 1); eine Lichtquelle zum Smiler werfen lenkt ihn ab und erlaubt die Flucht (Entity 3). Auf Level 3 wirkt der Trick kaum: die Smiler zucken, halten an, überlegen kurz und bleiben beim Wanderer. Welche Lichtquelle, sagen die Seiten nicht (nur Level 3 nennt die Taschenlampe).

**Umsetzung**
- **Leuchtstab** (`data/items.json`: `glowstick`, Stapel 6): Fundstück, das Wanderer vor einem liegen ließen – auf den Ebenen mit Smilern (`finds.glowstick`/`glowstick_dark` in `level1/2/3.json`, eher in dunklen Räumen; je Lauf etwa 4). Auswählen und **F**: die Hand knickt ihn vor der Brust (Glas knackt, er leuchtet auf, Schütteln), holt über die Schulter aus und wirft (Loslassen nach 0,62 s; `Body::Gesture::Throw`). **Q** legt ihn ungeknickt ab.
- **Flug** (`gameplay/lure.cpp`): 8 m/s in Blickrichtung plus 1,7 m/s nach oben und der Schwung des Laufens; Schwerkraft, wenig Luftwiderstand, Überschlag um die Querachse (8–15 rad/s); prallt von Wand (Rückprall 32 %) und Decke ab, springt auf dem Boden kurz auf, gleitet längs seiner Achse aus (4,2 m/s²) und rollt quer weniger gebremst (1,9 m/s²), fällt über Kanten, schwimmt auf Wasser. Liegt flach (Achse waagrecht, 8,5 mm über dem Boden).
- **Modell** (`render/mesh_glowstick.cpp`): 15 cm Hülle mit gerundetem Ende, Kappe mit Rille und Öse; drei Stufen: ungeknickt (blasser, glänzender Kunststoff), hell (grün leuchtend), matt (gegen Ende), danach erloschene Hülle (liegt noch 4 min). Eigenes Symbol im Inventar.
- **Licht** (`app.cpp`): grünes Punktlicht nach allen Seiten, Stärke und Reichweite (2,5 + 4,5 m) folgen der Leuchtkraft; die zwei nächsten Stäbe werfen Schatten; in der Hand 40 % (sonst überstrahlt er Arm und Bild). Leuchtdauer 60 s: in 0,5 s ganz hell, dann langsam schwächer, zuletzt rasch dunkel.
- **Smiler** (`gameplay/presence_lure.cpp`, `baitSmiler`): Er geht zum **stärkeren Licht**. Die eingeschaltete Lampe zählt 1, ein frischer Stab 1,6 – beides geteilt durch (1 + Abstand/6 m), vom Smiler aus gemessen. Er bemerkt Stäbe bis 22 m (über 6 m nur mit Sichtlinie; nah sieht er den Schein um die Ecke). Hat ihn ein Stab, wendet er sich ab (`Lured`: Klang und Hinweis), eine laufende Ankündigung bricht ab; er gleitet mit 1,7 m/s durch das Dunkel zum Stab (am Rand von Raumlicht bleibt er stehen) und hängt bis auf 0,75 m darüber: die obere Masse senkt sich und neigt sich zum Licht, das Gesicht folgt weiter hinab als sonst (Animationskanal `bait`), das Lächeln öffnet sich. Solange der Stab leuchtet, greift er nicht an und rückt nicht vor. **Lärm oder Nähe holen ihn zurück:** Rennen oder lauter Lärm in 5 m, harte Aufpralle (über 0,5) in 5 m oder ein Spieler näher als 2 m – dann beachtet er diesen Stab nicht mehr. Erlischt der Stab und ist man weit weg (über 12 m oder außer Sicht), hat er inzwischen das Interesse verloren und vergeht.
- **Level 3** (`presence.smiler_bait: "hesitate"`): er zuckt, hält 2,6 s an, sieht zum Stab – und wendet sich wieder dem Spieler zu.
- **HUD und Klang:** beim ersten Stab im Inventar ein Hinweis; beim Abwenden „Der Smiler wendet sich dem Licht zu – jetzt ruhig weg.“ (Level 3: „…stutzt nur…“). Klänge (`audio/synth_lure.cpp`): `glow_crack` (Kunststoff, Glasampulle, Schütteln), `glow_throw`, `glow_land_0..2` (hohler Kunststoff, Nachspringen, Rollen), `glow_splash`, `smiler_lured` (sein Dröhnen biegt ab und sinkt, nasses Zähneklicken). Der Aufprall des Stabs ist leise (Lärm 0,35, unter der Schwelle des Smilers).
- **Mehrspieler:** Der Gast spielt Geste und Knacken selbst und schickt beim Loslassen nur die Eingabe „Platz benutzen“; der Host wirft aus der Hand seiner Figur, rechnet Flug und Smiler und meldet den Stab allen (`Dropped glowstick_lit`); beim Gast fliegt er mit demselben Wurf aus der Hand des Werfers (nur Bild). Die Puppe eines gelockten Smilers sieht beim Gast zum nächsten Stab (Kanal `bait`). Protokoll unverändert.

**Interpretation:** Leuchtstab statt Taschenlampe (die Seiten nennen keine bestimmte Lichtquelle außer auf Level 3); „stärkeres Licht“ als Vergleich der Leuchtkraft mit Abstandsgewicht; dass Lärm und Nähe den Köder brechen (Level 1: nach einer Sichtung zieht auch Lärm an); Leuchtdauer, Wurf- und Rollwerte.

**Nicht umgesetzt: geworfene Taschenlampe.** Technisch nicht sauber: im Mehrspieler bräuchte der Gast eine neue Aktionsart (Protokolländerung ausgeschlossen); dazu käme der Zustand „Lampe liegt dort“ in Spielstand, Körper und Lichtsystem. Der Leuchtstab deckt den Rat des Wikis ab.

**Gleichgewicht:** Die Stäbe kommen über eigene Fundstellen (Hash je Raum), nicht über die Vorratstabellen – Mandelwasser, Energieriegel, Batterien und die Spawnpläne bleiben unverändert (`spawn_rules` nicht angefasst, `supply_balance_test` grün). Ein Stab kauft 60 s, nur bei Dunkelheit, nur wenn die eigene Lampe aus oder weit weg ist und man leise bleibt; auf Level 3 fast wertlos.

**Wow-Check:** Ein grünes Licht schlittert klappernd durch die schwarze Halle, und das Grinsen, das eben noch dir galt, senkt sich langsam darüber, die Masse beugt sich wie ein Tier über Futter – in dem Moment ist der Smiler zum ersten Mal unheimlich *und* überlistbar. Ehrlich: Wow für die Szene (`r9_smiler_seite.jpg`); offen bleibt, dass er, wenn der Stab direkt unter ihm liegt, sich in Blickrichtung statt genau darüber neigt.

### Kritikrunden (Leuchtstab)

Bilder: `docs/screens/creatures/smiler_koeder/` (je Runde `r<N>_…`; Hand, Flug, Boden nah/mittel/fern, mit Lampe, Smiler vorher/Ego/nah/Seite/mittel/mit Lampe).

| Runde | Befund | Änderung |
|---|---|---|
| 1 (`r1_hand.jpg`, `r1_flug.jpg`, `r1_boden_nah.jpg`) | Das Licht flutet die ganze Halle grün, Decke und Wände; der Stab glüht weiß; in der Hand überstrahlt er das Bild. Der Smiler erschien im Skript nicht. | Licht 1,3 → 0,8, Reichweite 8,5 → 7 m, in der Hand 40 %, Leuchten der Hülle 4,2 → 2,4. |
| 2 (`r2_boden_mittel.jpg`, `r2_hand.jpg`) | Lichtfleck jetzt etwa 2 m, glaubhaft. Der Wurf startete aus 2,4 m: Standort war ein Kistenstapel. Das Flugbild zeigte die Ego-Sicht; der Smiler griff vor dem Wurf zu. | Standort am Boden der Halle, Skriptzeiten. |
| 3–5 (`r3_smiler_ego.jpg`, `r5_boden_fern.jpg`, `r5_boden_lampe.jpg`) | Ein Wrangler (dann Jungtiere an der Wand) tötete den Spieler vor der Szene – die Bilder zeigten den Todesschirm. Fern: kleiner grüner Fleck in der dunklen Halle – stark; mit Lampe bleibt er sichtbar, aber unauffällig. | Skriptbefehle `grace` und `lurecam`; anderer Standort; Kamera erst nach einem Bild umschalten. |
| 6 (`r6_flug.jpg`, `r6_smiler_ego.jpg`) | Flug sichtbar: grüne Spur unter der Decke, das Licht wandert mit. Der Smiler folgt (Protokoll: „wendet sich dem Leuchtstab zu, 8 m“), steht aber steif wie ein Pfahl neben dem Licht. Kern der Hülle noch fast weiß. | Hülle 2,4 → 1,6 (grüner Kern); Kamera vom Smiler aus hinter den Stab (`lurecam 1000 …`). |
| 7 (`r7_smiler_nah.jpg`) | Das Grinsen über dem Lichtfleck wirkt – aber der Kopf sieht geradeaus, der Körper reagiert nicht auf das Licht. | Kanal `bait` im Animator: obere Masse 0,36 m tiefer und 0,4 m zum Licht, Gesicht bis 57° hinab. |
| 8–9 (`r9_smiler_seite.jpg`, `r9_smiler_mittel.jpg`, `r9_smiler_lampe.jpg`) | Von der Seite beugt sich die Masse über das Licht, das Grinsen hängt schräg darüber – das Bild trägt. Mit eingeschalteter Lampe aus 9 m bleibt er beim Stab (der Stab ist stärker). Er hält aber 1,1 m Abstand. | Abstand zum Stab 1,1 → 0,75 m. |
| 10 (`r10_smiler_seite.jpg`) | Der Stab fiel ihm direkt vor die Füße (zu ihm hin geworfen): er bleibt darüber; die Neigung zeigt dann in seine Blickrichtung statt genau über das Licht. Abgenommen, Rest offen. | – |

### Tests (`gameplay_test`, „V13.3: Leuchtstab und Ansprechen“)
Fundstellen (Level 1, 2, 3 je 4, Level 0 keine, auf freiem Boden); F knickt (aus dem Inventar, Geste), beim Loslassen fliegt er, Wurfbogen, liegt flach nach etwa 10 m, leuchtet, eigenes Licht; Level 1 im Stromausfall bei eingeschalteter Lampe: der Smiler wendet sich dem Stab zu, geht von 11,7 auf 1,4 m an ihn heran, der abgewandte Spieler überlebt; wer neben ihm rennt, holt ihn zurück; mit der Einstellung von Level 3 stutzt er nur und wendet sich wieder dem Spieler zu.

## V13.3: Level 3 – Angriff ohne Anlass

**Quelle** (Wikidot, CC BY-SA 3.0, eigene Worte, keine Bilder): [level-3](https://backrooms-wiki.wikidot.com/level-3) Rev. 18 (26.09.2026, Natedagreat563), Abschnitt zu den Wesen der Ebene, Smiler.

**Was die Seite sagt:** Die Smiler von Level 3 haben – so die verbreitete Annahme der Forscher – Lider und Lippen entwickelt und verbergen damit Augen und Zähne; ganz dunkel werden sie dabei nicht, nur viel schwächer. Sie dienen Wesenverbänden als Fallen und Wachen. Viele Forscher nehmen deshalb an, dass sie **unter allen Umständen angreifen** – anders als die Smiler anderswo, die nur gereizt oder erschreckt zugreifen. Rat: im Zweifel ruhig und leise umkehren, nicht darauf eingehen. Als letztes Mittel nennt die Seite eine Blitzlicht-Granate aus Base Gamma (gibt es im Spiel nicht).

**Umsetzung** (`gameplay/presence_smiler_l3.cpp`, Level-JSON `presence.smiler_unprovoked: 0.35`; auf allen anderen Ebenen 0 – dort unverändert):
- Beim ersten Denken wird je Smiler gewürfelt: etwa jeder dritte greift **ohne Anlass** an. Er wartet 9–22 s, und zwar nur, solange der Wanderer 4–15 m entfernt ist (näher wäre es unfair, weiter merkt man es nicht). Stillstehen und Hinsehen – der Rat für die gewöhnliche Panik – ändert daran nichts. Stutzt er gerade am Leuchtstab, wartet er.
- **Ankündigung (2,2 s, unbeweglich):** Lider und Lippen pressen sich erst zu – das Gesicht wird für 0,4 s fast dunkel –, dann reißen sie auf: Augen und Zähne in voller Helligkeit, das Lächeln weit offen, das Gesicht zittert. Eigener Klang `smiler_peel` (`audio/synth_smiler_l3.cpp`): dumpfes, feuchtes Quetschen, klebriges Ablösen mit einem Riss, dichtes Zähneklappern, darüber ein dünner Ton, der von 1,3 auf 3,4 kHz steigt und schwebt, scharfes Einziehen von Luft durch die Zähne. Er klingt bewusst anders als `smiler_open` (Panik), damit man sofort weiß: diesmal hilft Stillstehen nicht. Dazu der Hinweis „Lider und Lippen reißen auf – er kommt ohne Anlass. Licht aus und raus aus seiner Bahn!“.
- **Ansturm:** 4,2 m/s, höchstens 6 s. Drei Auswege, alle aus dem Wiki abgeleitet:
  - **Wegrennen** – der Sprint ist schneller (5,7 m/s); wächst der Abstand über 13 m, lässt er ab.
  - **Licht aus** – das Licht zieht ihn an (Entity 3). Ist die Lampe aus und steht der Wanderer im Dunkeln, verliert er ihn und stürmt zu der Stelle, an der er ihn zuletzt im Licht sah; wer dort nicht mehr steht (1,6 m), ist sicher – er lässt ab (das Lächeln verklingt) und vergeht. Geht die Lampe in 9 m wieder an, hat er ihn wieder.
  - **Raumlicht** – wie immer: tritt er auf eine beleuchtete Kachel, erlischt er.
- Wer stehen bleibt oder die Lampe anlässt und nicht wegläuft, wird erreicht – der Zugriff ist tödlich wie bei jedem Smiler.
- **Mehrspieler:** Der Host rechnet; die Puppe beim Gast bekommt Leuchten und Kiefer über die vorhandenen Kanäle, die Ankündigung als Figurereignis (Stärke 2). Protokoll unverändert.

**Begründung der Fairness:** „Ohne Anlass“ heißt hier: kein Auslöser durch den Spieler – nicht: ohne Warnung. Die Ankündigung ist länger als die der Panik (2,2 statt 1,5 s), sichtbar (erst dunkel, dann grell) und hörbar anders; es gibt zwei Auswege, die jeder Spieler schon kennt (Licht aus, weg), und einen dritten (Raumlicht). Er kommt nie aus nächster Nähe (über 4 m) und nur bei einem Drittel der Smiler – die Ebene bleibt spielbar, aber kein Smiler auf Level 3 ist mehr „sicher“, nur weil man stillsteht.

**Interpretation:** Würfelanteil, Wartezeit, Abstandsfenster, Ankündigungsdauer, Ansturmdauer, die Abbruchregeln und der Klang.

**Gleichgewicht:** Keine Änderung an Vorräten, Spawnplänen oder anderen Wesen; nur Smiler auf Ebenen mit `smiler_unprovoked > 0` (nur Level 3).

**Tests** (`gameplay_test`, „V13.3: Level 3 – Smiler ohne Anlass“; Halle Aquila mit der Einstellung von Level 3): Level 3 setzt den Wert, Level 1 nicht; still stehen und hinsehen – nach 20 s reißen Lider und Lippen auf, frühestens 2,2 s danach der tödliche Zugriff (gemessen 3,9 s); Licht aus und zur Seite treten – er stürmt zur letzten Stelle (Stufe 6), verfehlt und lässt ab; wegrennen – er lässt ab; Level 1: 30 s still stehen und hinsehen, kein Angriff ohne Anlass.
