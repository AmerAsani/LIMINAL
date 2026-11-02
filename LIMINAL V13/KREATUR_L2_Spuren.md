# Kreatur: Spuren der Skin-Stealer in Level 2 (Entity 10)

Quellen: [Level 2](https://backrooms-wiki.wikidot.com/level-2) Rev. 93 (Abschnitte Entities/Skin-Stealers und Temperatur) und [Entity 10](https://backrooms-wiki.wikidot.com/entity-10) Rev. 61, beide CC BY-SA 3.0; eigene Worte, keine Bilder, keine Zitate. Das lebende Wesen: `KREATUR_SkinStealer.md`.

## Was die Seite sagt

- **Level 2:** Vor dem großen Stromausfall waren die Skin-Stealer hier äußerst gefährlich, von der Hitze bestimmter Bereiche gereizt und fast verwildert; sie durchstreiften die Gänge nach Nahrung. Die Hitze ließ die getragene, verwesende Menschenhaut schmelzen. Nach dem Stromausfall – keine Menschen mehr – verließen sie die Ebene oder verhungerten, denn sie fressen nur Menschen. Seither sind sie nicht zurückgekehrt.
- **Entity 10:** Sie reißen Menschen die Haut ab und ziehen sie sich über; feine Saugnoppen halten sie fest und pumpen Blut hinein, so dass sie warm wirkt. Nach etwa einem Tag ist die Haut verbraucht. Unter der Haut: groß, blassgelb, eingesunkene weiße Augen.

## Umsetzung: kein Wesen, nur Spuren

Laut Seite gibt es auf Level 2 keine Skin-Stealer mehr (`presence.skinstealer` bleibt aus). Was sie hinterlassen haben, liegt in der Ebene – **Interpretation**: abgelegte Häute, Kratzspuren, ihr Lager.

**Modelle** (`tools/blender/models/models_l2_traces.py`, Signed-Distance-Formen in Blender, `data/models/`):
- `skin_shed` – leere Menschenhaut, plattgedrückt und zerknittert: entlang des Rückgrats der Riss, durch den das Wesen ausstieg (Ränder eingerollt, innen verglast vom klaren Blut), die Kopfhaut umgeschlagen – das leere Gesicht sieht nach oben, verfilztes Haar; ein Arm am Körper, einer weggestreckt mit offener Hand und Nägeln, die rechte Wade liegt umgeschlagen auf dem Oberschenkel; Totenflecken, eingetrocknete Lache.
- `skin_shed@1` – in den heißen Tunneln **geschmolzen** (die Seite: die Hitze ließ die Haut schmelzen): zu einer Lache zerlaufen, wenige breite Zungen, Fließspuren, angeschmorte Reste der Haut.
- `skin_hung` – eine Haut an einem rostigen Haken in der Wand, an der Kopfhaut aufgehängt wie ein Mantel: das Gesicht vom Gewicht lang gezogen (Augen und Mund zu Schlitzen), die Schultern fallen ohne Knochen ab, der leere Leib hängt in Falten und staucht sich unten, die Ärmel hängen frei, Finger baumeln; Haar um den Haken gewickelt, Blutspur an der Wand.
- `skin_scratches` – tiefe Kratzer von Krallen in Putz und Beton, in Gruppen zu viert, drei Töne (Rand, Flanke, Grund), Putzbrocken am Boden.
- `skin_lair` – die verlassene Lagerstelle in einer Ecke: eine Mulde aus hingeworfener, zerrissener Kleidung der Opfer (Jacken, Hosen, Hemd, Decke), darin ein Stapel gefalteter Häute (die oberste mit Gesicht), ringsum abgenagte Menschenknochen (Oberschenkel, Rippen, Wirbel, ein Unterkiefer, Fingerknochen – sie fressen nur Menschen), Habe der Opfer (Turnschuh, aufgerissener Rucksack, leere Flasche, Taschenlampe), Flecken aus getrocknetem klarem Blut, alles verstaubt.

**Karte** (`tools/levels/level2.py`, eigener Zufallsgenerator nach allen anderen Platzierungen, die Anordnung bleibt gleich): die Lagerstelle in einer Ecke einer kühlen Kammer weit vom Start (Kammer 35–46/70–81), dort die aufgehängte Haut und Kratzspuren; zwei abgestreifte Häute in kühlen Gängen, eine geschmolzene in den heißen Tunneln, Kratzspuren in der Hitze (insgesamt 5). Halt-Regeln in `tools/levels/mapkit.py` (Boden, Wand), `gameplay_test` (`mountOf`), `room_metrics --check` grün.

**Klang, Mehrspieler:** keine – die Spuren sind Ausstattung (feste Objekte der Karte, bei Host und Gast gleich).

**Wow-Check:** Im Lampenkegel hängt an einem Haken eine leere Menschenhaut mit langgezogenem Gesicht und Schlitzaugen über einem Haufen Kleider und abgenagter Knochen; im Gang davor liegt eine zweite, aufgeschlitzt entlang des Rückens. Ehrlich: beklemmend – man begreift, was hier gewohnt hat, ohne es je zu sehen.

## Gleichgewicht

Keine Wirkung auf das Spiel (keine Gefahr, kein Verstand); die Spuren erzählen. Sie liegen abseits der Hauptwege, damit sie gefunden und nicht vorgeführt werden.

## Kritikrunden

Bilder: `docs/screens/creatures/l2_spuren/` – je Runde Studiobögen aus `tools/qa/propsheet.py` (`rN_studio/`: 50 cm und 2 m im Licht der Ebene und nur mit Lampe, je vier Winkel), Kurzbogen `rN_bogen.jpg`, Übersicht `rN_uebersicht.jpg`; in der Ebene `r6_ebene_*.jpg`. Die Befunde der Runden 1–4 sind aus den Bögen und den Runden-Kommentaren in `models_l2_traces.py` nachvollzogen (Vorgängerarbeit), 5 und 6 neu.

| Runde | Befund | Änderung |
|---|---|---|
| 1 (`r1_bogen.jpg`) | Die geschmolzene Haut wie ein Keks, die aufgehängte wie ein Lebkuchenmann (Pappaufsteller), Kratzer kaum zu sehen, das Lager ein Ring wie ein Spielzeugkringel. | Neuentwurf: gespreizte Haut mit Riss am Rücken und Haar, geschmolzene Lache, Haut am Haken mit Falten, tiefere Kratzer, Lager als Mulde aus Kleidern. |
| 2 (`r2_bogen.jpg`) | Haut cremig-rosa mit Flecken wie ein Tarnmuster; Falten als Sinusrippen, am Haken Wellen wie ein Vorhang; eingerollte Rissränder als Klötze; Haar wie ein schwarzer Besen; Fließspuren in Ringen wie eine Zielscheibe. | Dunkler und entsättigt, große weiche Flecken; Falten aus gestreckten, unregelmäßigen Graten; Innenseite nur, wo sie zum Riss zeigt; Haar fein in verklebten Strähnen; Fließspuren unregelmäßig. |
| 3 (`r3_bogen.jpg`) | Alles flach wie Pappe, Ränder glatt wie ausgeschnitten; die geschmolzene Haut ein Stern wie ein Comic-Klecks; Ärmel mit dem Leib verschmolzen; Kratzer als schwarze Linie mit weißem Saum; Kleider wie Schnittmuster. | Mehr Relief (hohe Beulen, gestauchte Falten), ausgefranste, eingerissene Ränder; wenige breite, ungleiche Zungen; Ärmel frei; Kratzer in drei Tönen (Rand, Flanke, Grund); Kleider mit Volumen. |
| 4 (`r4_bogen.jpg`) | Im Licht der Ebene zu hell; die geschmolzene Haut bleich; das Dosenetikett grell, Brot und Papier zu hell. | Haut dunkler, geschmolzene Reste angeschmort, Etikett ausgeblichen, Brot und Papier dunkler. |
| 5 (`r5_bogen.jpg`) | Die Änderungen sind kaum zu sehen (gemessen: 2–4 Helligkeitsstufen von 255) – kein Fehler, aber kein Gewinn. Hauptmangel jetzt: die aufgehängte Haut hat gerade Schultern auf 1,31 m – der Umriss eines stehenden Menschen, wieder ein Pappaufsteller. | Ohne Knochen fallen die Schultern vom Hals schräg ab (Mantel am Haken), die Ärmel hängen tiefer und näher am Leib, schräge Zugfalten vom Hals zu den Schultern. |
| 6 (`r6_bogen.jpg`, `r6_ebene_bogen.jpg`) | Die Haut hängt leer am Haken, das lang gezogene Gesicht unter dem Haken (`r6_ebene_haken_nah.jpg`); in der kühlen Kammer liest sich die Szene mit dem Lager in der Ecke (`r6_ebene_lager.jpg`) und den Kratzern (`r6_ebene_kratzer.jpg`); die abgestreifte Haut im Gang und die geschmolzene im heißen Tunnel liegen glaubhaft (`r6_ebene_haut.jpg`, `r6_ebene_geschmolzen.jpg`). Abgenommen. | – |

**Restmängel:** Aus Spielerhöhe bleibt die liegende Haut ein flacher Umriss am Boden (eine Höhenfeld-Haut kann nicht übereinander liegen; die umgeschlagene Wade ist die einzige zweite Lage). Die Kleider des Lagers sind im Lampenlicht noch recht bunt.

## Tests (`gameplay_test`, „V13.3 Teil 4: Level 2“)

Spuren in der Karte (ein Lager, mindestens zwei abgestreifte Häute, eine aufgehängte, mindestens drei Kratzspuren); eine geschmolzene Haut liegt in den heißen Tunneln; in Level 2 kein lebender Skin-Stealer. Halt-Regeln der Modelle (Boden, Wand) über `mountOf`.
