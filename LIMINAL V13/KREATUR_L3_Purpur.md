# Kreatur: Purpifizierte tote Wesen in den Ziegelwänden (Level 3)

Quelle: [Level 3 „Electrical Station“](https://backrooms-wiki.wikidot.com/level-3) Rev. 18 (zuletzt bearbeitet 26.09.2026; Abschnitte Anomalies/Purpification, Vorfall GB-03), CC BY-SA 3.0; eigene Worte, keine Bilder, keine Zitate. Notizen: `scratchpad/v13/wiki/ext_purple.md`, `level_3.md`.

## Was die Seite sagt

- Die Seite nennt das Phänomen „Purpification“. Selten – seit der Entdeckung der Ebene nur 17 Fälle – finden Forscher **tote Wesen, die zum Teil in die Wand genoclippt** stecken. Die Wesen und die **Wandstellen direkt um sie** sind **immer vollständig violett**; aus dem Körper strömt violette Energie über die Wände.
- Untersuchte Ziegel und Kadaver enthalten **reichlich Gammastrahlung** und eine **unbekannte, starke Energie**, die violettes Licht abgibt; ein herausgelöster Ziegel zeigt violette Schlieren im Inneren.
- Bekannte Fälle: ein **Skin-Stealer in einem dunkleren Bereich**; ein **Hound**, neben dem (rechts davon) eine **auf die Wand gemalte Botschaft** stand: Ein Schöpfer klagt, sein schönes Geschöpf so entstellt zu sehen, es sei zu Edlerem gemacht; er wünscht, es noch einmal mit echten Augen zu sehen und mit echter Hand zu berühren, und verspricht, Leib und Geist eines Tages zu heilen. Ob die Inschrift echt oder eine Falschspur ist, lässt die Seite offen.
- Den Vorgang selbst hat niemand beobachtet; Menschen wurden nie so gefunden. Ein **Verhalten, einen Klang oder eine Wirkung** der Toten nennt die Seite **nicht**.

## Umsetzung: keine Rolle, eine Erscheinung

Die Toten sind Ausstattung der Karte, ihr Wirken eine kleine eigene Einheit (`gameplay/purpification.*`, `session_purple.cpp`) – kein `Presence::Role`, kein `anim::Entity`, kein Netzverkehr.

- **Modelle** (`tools/blender/models/models_l3_purple.py`, Distanzfeld → OpenVDB-Netz, hinter der Wand und unter dem Boden abgeschnitten):
  - `purple_stealer` (57 600 Dreiecke): ein Skin-Stealer, der halb aus der Ziegelwand steigt – Kopf in den Nacken gegen die Ziegel gepresst, Kiefer aufgerissen mit Zähnen und Sehnenfäden, der linke Arm zieht sich an der Wand entlang, die langen Finger in die Fugen gekrallt; die rechte Hand kommt an falscher Stelle tief unten aus der Wand (Interpretation: der Noclip hat den Leib in der Wand verschoben). Noppen auf der Haut glimmen.
  - `purple_hound` (75 000 Dreiecke): ein abgemagerter Hound (Mensch auf allen vieren, Rippen und Wirbel unter straffer Haut), Kopf und Schultern aus der Wand gebrochen, eine Hand auf dem Beton, das Haar hängt in nassen, versteinerten Strähnen bis auf den Boden und liegt dort ausgebreitet. Rechts davon die gemalte Botschaft in eigenen Worten (Strichschrift, verlaufene Farbe).
  - Gemeinsam: violette Haut, matt in der Fläche, glasig auf Knochen und Wülsten (die Purpifizierung als Glasur), mit Rissen, aus denen Licht dringt; Augen, Schlund und die Naht zur Wand am hellsten. Wo der Körper in der Wand steckt, wird das Fleisch zu Mauerwerk (Ziegelplatten mit Fugen). Um den Körper ist die Wand in vier Stufen violett – mit der Textur der Wand und deckungsgleich, weil die Ausstattung ihre Texturen wie die Wände aus Weltkoordinaten projiziert –, vom glimmenden Kern bis zu einem blassen, ausgefransten Saum. Verzweigte Lichtbahnen laufen über die Ziegel (Interpretation von „strömt über die Wände“), herausgedrückte violette Ziegelbrocken, am Boden Bruchstücke mit leuchtenden Schlieren im Inneren (Wiki) und violetter Staub.
- **Platz** (`tools/levels/level3.py`, am Ende mit eigenem Zufall 3303 – die Anordnung der Ebene bleibt): je ein Fall, nur in Gängen mit der Ziegeltextur der Modelle. Der Skin-Stealer an der Nordwand des dunklen Paranoia-Gangs (Wiki: „dunklerer Bereich“), der Hound im Ziegelgang h2 mit durchgehender Wand für die Schrift. Halt-Regel Wand (`mapkit.py` `WALL_MOUNT`, `gameplay_test` `mountOf`). Sie leuchten mit eigener Energie (Tag `emergency`: kein Stromausfall, kein Flackern des Stromkreises).
- **Licht** (Wiki: violettes Leuchten): ein violettes Zusatzlicht vor jedem Körper (Reichweite 3–5 m), das langsam atmet (6,5 s) und gelegentlich anschwillt (Schwall, etwa alle 34 s).
- **Strahlung** (Wiki: Gammastrahlung; Wirkung Interpretation): dicht vor dem Körper zehrt sie am Verstand (bis 0,32 Prozentpunkte je Sekunde, fällt bis 3,6 m auf null, hinter der Wand nichts), dazu Bildrauschen und Farbsäume, beim Knistern ein Schub. Einmal je Lauf ein Hinweis („Kribbeln auf der Haut, Metall auf der Zunge …“).
- **Lampe** (Interpretation: die Energie zieht am Licht): Näher als 4,2 m stockt die Taschenlampe in kurzen Einbrüchen, je näher, desto tiefer. Wer die Lampe auf den Körper richtet, lässt die Glut anschwellen (Licht, Knistern und Schwall häufiger).
- **Botschaft**: Wer vor der Schrift neben dem Hound steht und hinsieht, liest sie als Untertitel (deutsch, eigene Worte).
- **Es sind Tote**: Sie bewegen sich nicht und greifen nicht an – die Seite beschreibt kein Verhalten. Die Pflicht-Animationen eines Wesens (Idle, Jagen, Angriff …) entfallen deshalb bewusst; „Lebendigkeit“ kommt nur aus Licht und Klang.
- **Mehrspieler**: Den Verstand der Gäste zieht der Host aus deren Lage ab (wie bei den anderen Gefahren); Licht, Klang, Lampe, Bild und Hinweise rechnet jeder Rechner für sich aus derselben Karte. Kein Spielzustand im Netz, Protokoll unverändert.

## Interpretation (gekennzeichnet)

Klang, Lampenstocken, das Anschwellen im Lichtkegel, die Wirkung der Strahlung auf den Verstand, die verschobene Hand des Skin-Stealers und das Fleisch, das zu Mauerwerk wird, stehen nicht auf der Seite. Sie folgen aus „starke unbekannte Energie“, „Gammastrahlung“ und „in die Wand genoclippt“.

## Zahlen

| Größe | Wert |
|---|---|
| Fälle in Level 3 | 2 (Wiki: selten) |
| Hörweite Brummen / Knistern | 13 m / 15 m |
| Strahlung spürbar | bis 3,6 m, Verstand −0,32 %/s dicht davor |
| Lampe stockt | bis 4,2 m |
| Botschaft lesbar | bis 3,4 m, Blick auf die Schrift |
| Schwall / Knacken im Mauerwerk | im Mittel alle 34 s / 48 s |

## Klang (`src/audio/synth_purple.cpp`, synthetisch)

- `purple_hum` (Schleife je Stelle): tiefes, schwebendes Brummen – bewusst nicht der Netzbrumm der Ebene –, darüber ein glasiges Singen aus eng benachbarten hohen Teiltönen.
- `purple_crackle_0..3`: Schübe trockener Knackse mit kurzem glasigem „Ping“ (Entladung in den Ziegeln).
- `purple_settle`: Mauerwerk knackt, eine Sehne knarrt unter Spannung, Ziegelkrümel rieseln.
- `purple_surge`: das Brummen schwillt an und steigt, das Knistern verdichtet sich.

Man hört die Stelle, bevor man sie sieht: das Singen ab 13 m, im dunklen Paranoia-Gang zuerst nur ein violetter Schimmer am Ende.

## Gleichgewicht

Keine Gefahr für das Leben. Wer verweilt, verliert Verstand (gemessen: 12 s dicht davor etwa 3,8 Prozentpunkte mehr als in 30 m Abstand); wer vorbeigeht, kaum. Die Lampe stockt nur in der Nähe – ein Schreck, keine Falle. Selten genug, dass jede Stelle ein Fund bleibt.

**Wow-Check:** Im stockdunklen Paranoia-Gang summt etwas Gläsernes; am Ende ein violetter Schimmer, dann der Skin-Stealer, halb aus den glühenden Ziegeln gekrochen, Kopf im Nacken, Kiefer offen, Finger in den Fugen – und die Lampe beginnt zu stocken. Ehrlich: Gänsehaut, man bleibt nicht stehen. Beim Hound im hellen Gang trägt die Botschaft daneben das Grauen – weniger Schock, mehr Beklemmung.

## Kritikrunden

Bilder: `docs/screens/creatures/l3_purpur/rN_*.jpg` (Skin-Stealer fern/mittel/nah/mit Lampe; Hound fern/mittel/nah/Gegenblick/Schrift/mit Lampe, alle in der Ebene). Die Befunde der Runden 1–3 sind aus den Bildern nachvollzogen (Vorgängerarbeit ohne eigene Notizen).

| Runde | Befund | Änderung |
|---|---|---|
| 1 (`r1_stealer_nah.jpg`, `r1_hound_nah.jpg`) | Leib glatt und hell wie eine Gummipuppe; weiße, dichte Lichtbahnen wie ein Stromschlag überstrahlen alles; Augen grell wie im Comic; das Haar des Hounds ein starrer Block. | Glasur-Risse und glimmende Noppen in der Haut, weniger und kürzere Lichtbahnen, Haar als einzelne nasse Strähnen bis auf den Boden. |
| 2 (`r2_stealer_nah.jpg`, `r2_hound_nah.jpg`) | Der Kopf des Skin-Stealers glüht noch wie eine Laterne; um den Hound kaum Spuren eines Durchbruchs; die Wand neben ihm fast grau. | Glutstellen kleiner (Augen, Schlund), Fleisch wird zu Mauerwerk, herausgedrückte Brocken, Bahnen weiter über die Wand, Bruchstücke am Boden. |
| 3 (`r3_stealer_nah.jpg`, `r3_hound_nah.jpg`, `r3_hound_gegen.jpg`) | Im hellen Gang ist der Fleck um den Hound nur lavendelgrau – die Seite sagt „immer ganz violett“; die Brocken grau wie fremder Schutt; helle Glasurflecken lassen die Haut wie Spielzeug wirken. | Ziegelstufen satt violett mit glimmendem Kern, Brocken aus derselben violetten Wand, Glasur dunkler (Glanz aus dem Licht, nicht aus heller Farbe). |
| 4 (`r4_hound_gegen.jpg`, `r4_stealer_nah.jpg`) | Die Wand ist jetzt violett – aber der Fleck bricht wie ein Aufkleber mit glattem Rand ab; der hell-lavendelfarbene Leib verschwimmt vor der nun leuchtenden Wand. | Vierte, blasse Außenstufe (die Farbe sickert aus), Rand ausgefranst (feines Rauschen, Ausleger entlang der Fugen, weniger Glättung); Haut dunkler, damit der Leib als Silhouette vor den glühenden Ziegeln steht. |
| 5 (`r5_hound_mittel.jpg`, `r5_stealer_nah.jpg`, `r5_stealer_fern.jpg`) | Klar besser: Kern, Mitte, Saum lesen sich als durchgefärbte Wand; der Skin-Stealer steht schwarz vor den glühenden Ziegeln. Neuer Befund aus `room_metrics --check`: Hound und das Rohrbündel gegenüber verengen den 2 m breiten Gang auf 0,72 m. | Platzregel in `level3.py`: gegenüber darf Bodenausstattung mit dem Körper den Gang nicht unter 0,9 m verengen; der Hound liegt nun 2 m weiter im selben Gang. |
| 6 (`r6_hound_mittel.jpg`, `r6_hound_schrift.jpg`, `r6_hound_fern.jpg`) | Am neuen Platz frei passierbar, die Schrift rechts gut lesbar, das Rohrbündel gegenüber weiter vorn; von fern nur ein schwacher violetter Schimmer im hellen Gang (gewollt: man hört ihn zuerst). | – |

**Restmängel:** Die Lichtbahnen sind flache Bänder auf der Wand – aus spitzem Winkel wirken sie dünn wie aufgemalt. Im hellen Gang des Hounds trägt das Zusatzlicht wenig; er wirkt dort mehr durch Fleck und Schrift als durch Leuchten. Der Fleckrand folgt dem Rauschen, nicht den einzelnen Ziegeln (die Lage der Ziegeltextur kennt das Modell nicht).

## Tests (`gameplay_test`, „V13.3: Level 3 – purpifizierte Wesen in der Wand“)

Level 0 hat keine; Level 3 genau zwei Stellen (Skin-Stealer, Hound), beide an einer Ziegelwand im Gang (dahinter Wand); Strahlung dicht davor stark, in 6 m und hinter der Wand null; violettes Licht vor dem Körper, aus 40 m keines; die Lampe stockt nur nahe der Stelle; angeleuchtet schwillt die Glut an; in der Sitzung nah Knistern und Verstandsverlust, fern nichts; Hinweise beim ersten Anblick und bei der Strahlung; die Botschaft lässt sich lesen; ein Brummen als Klangquelle je Stelle. Halt-Regeln über `mountOf`; `room_metrics --check level3` grün.
