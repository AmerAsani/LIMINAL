# Kreatur: The Game Master (Entity 99) – das Gemälde

Quellen (Wikidot, CC BY-SA 3.0, eigene Worte, kein Bild der Seiten):
- [entity-99](https://backrooms-wiki.wikidot.com/entity-99) „The Game Master“, Rev. 39 (11.08.2026); ursprünglich 1000dumplings, Neufassung Justazappyrat, Korrekturen mctoran.
- [level-1](https://backrooms-wiki.wikidot.com/level-1) Rev. 83 (07.10.2026, Praetor3005 und DivineAtlas): Gemälde am Boden, Durchgang zu Level 389.
- Gelesen am 27.10.2026 (`scratchpad/v13/wiki/txt/entity-99.txt`, `wiki/level_1.md` Abschnitte 6–7).
- Abgleich: [backrooms-remodeling-co](https://backrooms-wiki.wikidot.com/backrooms-remodeling-co) nur gelesen (die Ouroboros-Arbeiter baut ein späterer Zweig).

## Was die Seiten sagen

- Eine narrenhafte **Marionette** mit großer Macht über die Wirklichkeit in ihrem Spielsaal (Level 389). Das Wiki spricht von **ihr** (der Auftrag sagt „er“ – im Spiel gilt das Wiki: „die Spielleiterin“).
- Sie lockt Wanderer in ihre Ebene und fordert sie zu einem Spiel heraus; sprunghaft, Netzjargon, verliert schlecht, nennt sich regeltreu und **schummelt selbst**; ihre Spiele sind so gebaut, dass sie gewinnt.
- **Gesicht**: ein Glasbildschirm mit zwei roten X als Augen und einem Cartoon-Grinsen, andere Ausdrücke möglich. **Körper**: Flickwerk aus blauem und gelbem Stoff, mit Watte gefüllt; **Glieder aus Holz**; **rote Kugeln**, die quietschen, an den Spitzen der Narrenkappe, an den Schuhspitzen und in der Rumpfmitte; **rote Fäden** an Handgelenken, Knöcheln und anderen Stellen, deren Ursprung nicht zu finden ist – sie scheinen unendlich.
- Level 1: Gemälde liegen kurz vor den Türen zu den Hallen auf dem Boden und verschwinden, wenn niemand hinsieht (wie die Kisten); eines zeigt diese Narrenfigur und führt beim Hindurchgehen nach Level 389.

## Umsetzung (Level 1)

Kein neues Wesen mit eigener Rolle (Auftrag); die Spielleiterin erscheint **nur im Bild** und über ihre Einladung (`gameplay/phenomena.*`, `gameplay/session_phenomena.cpp`).

- **Gemälde am Boden**: 12 Plätze kurz vor den Türen zu den Hallen, auf der Seite außerhalb der Halle (Karte: Spawnpunkte `trace`/`painting`); je Lauf liegen 4 Bilder, eines davon ihres. Liegende Bilder `painting_floor@0..3` (Landschaft mit Haus, Stillleben, Abendmeer, Gang mit Tür – dieselbe Malerei wie die Wandbilder, flach gelegt, `models_l1.py: _painting(flat=True)`); gezeichnet als bewegliches Ausstattungsmodell `prop:<modell>@<variante>` (`render/prop_meshes.cpp`, Materialien aus der Bank der Welt).
- **Verschwinden unbeobachtet**: Sieht kein Spieler ein Bild (fünf Punkte, Blickkegel 1,15 rad, freie Sicht; unter 3,5 m gilt es immer als gesehen), kann es nach 7 s mit 12 %/s verschwinden und liegt dann vor einer anderen Tür, die gerade niemand sieht und die mindestens 8 m von jedem Spieler entfernt ist. Die gewöhnlichen Bilder wechseln dabei ihr Motiv.
- **Ihr Gemälde** (`painting_jester@0..2`, Blender): Bühne aus schwarzem Samt, rote Vorhänge, Schachbrettboden in Fluchtperspektive (die Seite: eine schachbrettgemusterte Tür); sie als schlaffe Marionette – Holzglieder mit Gelenkzapfen, Rauten aus blauem und gelbem Stoff, gezackter Kragen, Narrenkappe mit drei Zipfeln. **Echte Teile auf der Leinwand**: ein kleiner Glasbildschirm mit leuchtenden roten X und Mund, sechs rote Kugeln (Kappe, Schuhe, Rumpf), und **rote Fäden**, die aus dem liegenden Bild 2,45 m senkrecht ins Dunkel steigen. Drei Ausdrücke (Grinsen, schiefe Zickzacklinie, lachender Halbmond) – **das Gesicht wechselt, wenn niemand hinsieht** (alle 3–8 s). Im Dunkeln sieht man nur das rote Glimmen am Boden. In 10 m quietschen die Kugeln leise (alle 6–15 s).
- **Hindurchgehen**: Wer auf ihr Bild tritt, bleibt stehen; ihr Bildschirmgesicht füllt die Sicht (Nachbearbeitung `jesterFace`, dunkles Glas mit leisem Flimmern). Drei Sätze in eigenen Worten mit Kichern aus einem kleinen Lautsprecher: „EIN NEUER SPIELER! Endlich …“ – „Wir spielen. Du darfst dir das Spiel aussuchen … ach nein, doch ich.“ – „Geschummelt! Ich hab’s genau gesehen. RAUS mit dir!“ (schnarrender Fehlerton). Nach 6,2 s steht man **anderswo in Level 1** (9 Orte in Hallen und Gängen, nicht im Garden, mindestens 30 m entfernt), ihr Bild liegt dann vor einer anderen Tür, und 240 s lang lädt sie diesen Spieler nicht wieder ein.

## Interpretation (für DECISIONS)
Level 389 gibt es im Spiel nicht, und das Wiki beschreibt nur ihren Spielsaal, ihre Spiele und dass sie schummelt und schlecht verliert. Deshalb führt das Bild nicht in eine neue Ebene: sie lädt ein, fordert heraus, wirft dem Spieler Schummeln vor und **wirft ihn aus dem Spiel** – er steht woanders in Level 1. Keine Spielinhalte anderer Spiele, keine Spiele aus ihrem Katalog (die Seite nennt Abwandlungen bekannter Spiele – bewusst nicht nachgebaut). Das Gesicht wechselt nur ungesehen, weil die Bilder auf Level 1 nur ungesehen verschwinden.

## Zahlen

| Wert | Größe |
|---|---|
| Plätze / Bilder je Lauf | 12 / 4 (eines ihres) |
| Verschwinden | ab 7 s ungesehen, 12 %/s; Ziel ungesehen, ≥ 8 m von allen |
| Ausdruck | wechselt ungesehen alle 3–8 s |
| Einladung | 6,2 s (Sätze bei 0,4 / 2,2 / 4,1 s), stillstehen |
| Hinauswurf | Ort ≥ 30 m entfernt, Pause 240 s für diesen Spieler |
| Quietschen | in 10 m alle 6–15 s, leise |

## Klang (`audio/synth_phenomena.cpp`)
`jester_squeak` (Gummikugel, Luft durch ein Ventil), `jester_giggle` (hohe Stimme, Vokal i, Stöße; durch einen kleinen Lautsprecher: Bandpass, grobe Abtastung, Brummen), `jester_buzz` (zwei verstimmte Rechtecke – der Vorwurf des Schummelns).

## Gleichgewicht
Ungefährlich, aber verunsichernd: kein Schaden, nur Ortsverlust; das kann gut (weg von einem Wesen) oder schlecht (weg vom Weg) sein. Die Pause verhindert, dass man sie als Schnellreise missbraucht.

## Mehrspieler
Wohin ein Bild wandert, entscheidet der Host; er meldet es als Weltereignis `Dropped "painting"` (Kennzahl = Bild, Lage = neuer Platz, Variante und Winkel in z verschlüsselt) – der Gast legt dasselbe Bild an dieselbe Tür (`Phenomena::netMove`). Den Hinauswurf eines Gasts setzt der Host an dessen Figur; der Gast springt mit der nächsten Korrektur. Einladung, Gesicht und Sätze rechnet der Gast für sich. Protokoll unverändert.

## Wow-Check
Ein Bild am Boden vor der Hallentür, aus dem rote Fäden ins Dunkel steigen – du siehst weg, und als du wieder hinsiehst, grinst ein anderes Gesicht auf dem kleinen Bildschirm. Ehrliche Antwort: ja, das ist unheimlich.

## Kritikrunden
Bilder: `docs/screens/creatures/l1_erscheinungen/` (`r<N>_narr*`, `r<N>_gemaelde`, `r<N>_einladung*`).

| Runde | Befund | Änderung |
|---|---|---|
| 1 (`r1_gemaelde.jpg`, `r1_narr_lampe.jpg`, `r1_narr_dunkel.jpg`, `r1_einladung.jpg`) | Liegendes Bild vor der Tür liest sich; ihr Bild mit Fäden ist stark; im Dunkeln nur das rote Glimmen (gut). Einladung: „lachend“ ist nur ein Ring, Glas grau wie Rauschen; die folgenden Bilder waren noch von der Einladung verdeckt (Reihenfolge der Aufnahmen). | Aufnahmen nach der Einladung später; Mund und Glas überarbeiten. |
| 4 (`r4_narr_nah.jpg`) | Nah mit Lampe: Figur, Kugeln, Bildschirm und Fäden klar; Rahmen und Schachbrett tragen. | – |
| 5 (`r5_narr_dunkel_fern.jpg`) | Aus 3 m im Dunkeln fast unsichtbar, nur das Glimmen – gewollt (man findet es mit der Lampe). | – |
| 7 (`r7h_narr_einladung_satz2.jpg`, `r7h_narr_rausgeworfen.jpg`) | Mit HUD: Sätze erscheinen, nach dem Hinauswurf steht man in der Gothic-Halle, Meldung „Rausgeworfen …“. | – |
| 8 (`r8_einladung_1.jpg`, `r8_einladung_2.jpg`, `r8_einladung_3.jpg`) | Grinsen und lachender Halbmond gut, Glas jetzt dunkel; „verzogen“ hatte hart abgeschnittene Enden. | Verzogener Mund als schiefe Zickzacklinie mit runden Enden. |
| 9 (`r9_einladung_3.jpg`) | Zickzack liest sich als verzogenes, falsches Lächeln. | Abgenommen. |

## Tests (`gameplay_test`, „V13.3: Erscheinungen in Level 1“)
Vier Bilder, eines ihres, alle Plätze auf freiem Boden; angesehen verschwindet ein Bild 60 s lang nicht; ungesehen verschwindet es nach mehr als 7 s und liegt weit vom Betrachter; der Weg geht an die Gäste (Kennzahl), der Gast legt dasselbe Bild an dieselbe Tür (Motiv, Winkel); auf ihrem Bild: Sicht voll Gesicht, Stillstand; danach mehr als 20 m entfernt auf freiem Boden (nicht im Garden), Kichern und Quietschen, Meldung, ihr Bild woanders, keine sofortige zweite Einladung.

## Restmängel
- Ein später beitretender Gast sieht die Bilder bis zum nächsten Wechsel an ihren Anfangsplätzen (ohne Protokolländerung kein Gesamtstand).
- Die Spielleiterin selbst (Figur in Level 389) gibt es nicht – nur Bild, Gesicht und Stimme.
