# Kreatur: Cannibal Cuisine (Entity 36), Level 2

Quellen: [Entity 36 – „Cannibal Cuisines“](https://backrooms-wiki.wikidot.com/entity-36) Rev. 13 (Autor Gasaludo) und
[Level 2](https://backrooms-wiki.wikidot.com/level-2) Rev. 93 (Abschnitt Entities; u/M654z, 1000dumplings, Boring Talking X_X), beide
Backrooms-Wiki auf Wikidot, CC BY-SA 3.0. Alles in eigenen Worten, keine Bilder und keine Zitate des Wikis. Im Spiel heißt er nur
„Automat“; Herstellerschild und Karteikarte tragen eigene Wörter statt der Namen der Seite (LICENSE_REVIEW: möglichst wenige
wiki-eigene Namen im Bild).

## Was die Seite sagt

- **Gestalt:** ein anomaler Verkaufsautomat, etwa 1,83 × 1,22 × 0,76 m, oft im Stil bekannter Marken; zwei Tastenfelder (Ziffern,
  Buchstaben). Statt Spiralen schiebt je Fach eine abgetrennte, skelettierte Menschenhand die Ware vor, bis sie fällt. Oben und unten je
  eine Klappe, aus denen metallisch überzogene Skelettarme (bis etwa 1,37 m) kommen; die unteren dienen als Beine. Innen ein Lager mit
  armlosen Leichen, an einer eine Karteikarte. Hinten ein Herstellerschild.
- **Ware:** gratis, alles aus Menschen (Fleisch in Riegelpapier, Chips aus Knochenmark, Blut in Flaschen, Herz, Zähne, Ohren …).
  Sie macht sofort süchtig: Süchtige bleiben beim Automaten, anderes Essen und Mandelwasser verdaut der Körper nicht mehr, Hunger und
  Durst wachsen; Aufhören ist bisher immer gescheitert.
- **Gefahr:** Wer ihn oder ein Opfer angreift, den schlagen und kratzen die Arme; er verfolgt ihn auf den unteren Armen.
- **Klang:** Die Leichen flüstern mit ihren Stimmbändern; Süchtige hören stattdessen Weinen in der Stimme eines geliebten Menschen.
- **Level 2:** recht häufig, meist am Ende von Sackgassen oder an Wänden ohne Maschinen und Rohre; im Stromausfall war ihr Licht eine
  Wegmarke. Für die Ebene gilt er als nicht gefährlich.

## Umsetzung

**Modell** (`tools/blender/models/models_l2_rand.py`, `cc_body@0..2`, rund 75 000 Dreiecke, drei Lackierungen ohne echte Marken):
Gehäuse, Fenster ohne Scheibe (nur die Nut), fünf Reihen à vier Fächer mit Ware (Riegel, Chips, Flasche, Dose, Herz, Zähne, Ohren,
Keule, Hautstreifen) und je einer Knochenhand, die ihre Ware umgreift; blutige Handabdrücke auf der vergilbten Rückwand, Blut läuft
von den Fachkanten; Tastenfelder, „NO COINS / NO CARDS“, Ausgabefach „PUSH“; hinter den Klappen Hohlräume mit Schulterpfannen, unten
das Gesicht einer armlosen Leiche mit Karteikarte; Herstellerschild hinten (eigene Wörter, ein Auge als Zeichen).
Bewegliche Teile: `cc_flap_top@0..2` (leuchtendes Kopfschild), `cc_flap_bot`, Arm aus `cc_humerus`, `cc_forearm`, `cc_hand@0`
(Kralle) / `@1` (gespreizt, als Fuß), `cc_ware` (auch das Item). Materialien: Lack, Chrom, Knochen (Rauheit 1,2), Überzug (Metall
0,6), Fleisch nass (0,22), Fett 0,3, Papier, Karton, Glow für Kopfschild und Röhre.

**Verhalten** (`src/gameplay/apparitions_l2.cpp`, ohne Rolle; Sitzung `session_apparitions.cpp`; Level-JSON `apparitions.cuisine`):
- 6 Automaten (`tools/levels/level2.py`): 4 an Sackgassen (nicht neben den Fensterwesen, 24 m zueinander), 2 an kahlen Kammerwänden;
  der Automat steht fest auf einem unsichtbaren Block (Kollision, Schatten, Wege); der Spawnpunkt liegt 0,64 m davor (Tag `o`).
- **Licht:** eigenes Licht (warm, mit der Farbe des Kopfschilds, 7,5 m), das im Stromausfall anbleibt – Wegmarke im Dunkeln.
- **Klang vor Sicht:** Brummen (Kompressor, Röhre, darunter Murmeln) als Schleife, alle 7–15 s Flüstern der Leichen in 6 m.
- **Taste (E):** „Automat: eine Taste drücken“ – zwei Tasten, die Knochenhand schiebt die Ware durch das leere Fach, sie kippt über
  die Kante und fällt ins Ausgabefach (2,6 s); die Ware kommt ins Inventar, gratis; 3 je Automat, dann „Die Fächer sind leer“.
- **Ware essen:** +40 % Verstand (Rausch). Ab dann **süchtig** (auch über Ebenen und im Spielstand): Mandelwasser und Riegel würgt man
  wieder hoch (wirken nicht); mehr als 40 m vom nächsten Automaten zehrt der Entzug (bis +0,06 %/s Verstandsverlust nach 30 s);
  nahe einem Automaten hört man statt des Flüsterns Schluchzen in einer warmen, vertrauten Stimme.
- **Angriff (Beil):** Klappen klappern, das Kopfschild flackert (0,6 s Warnung); dann gehen die Klappen auf, die unteren Arme stemmen
  ihn hoch (+0,12 m) und schieben ihn bis 0,6 m vor, die oberen schlagen abwechselnd alle 0,85 s: 30 % Gesundheit, wer bis 1,6 m
  vor seiner Vorderseite steht (Arm 1,37 m, Hand); nach 5,6 s (jeder weitere Treffer verlängert) ziehen sich die Arme zurück.
- Todesart „automat“: „D E R   A U T O M A T   H A T   D I C H“, Hinweis: nicht angreifen, bei Klappern zurück.

**Klang** (`src/audio/synth_l2_rand.cpp`): `cc_hum` (Schleife), `cc_whisper` (gedämpfte Flüstersilben dreier Stimmen),
`cc_sob` (Schluchzen, warme Stimme, Einatmer), `cc_keys`, `cc_hand` (Knochen schaben über Blech, Knöchel klacken), `cc_drop`
(weich auf Blech, Papier, Klappe pendelt), `cc_rattle`, `cc_flap`, `cc_creak` (Knochen unter Last), `cc_strike` (Schwung, Aufschlag).

**Mehrspieler:** Der Host entscheidet Ware, Zorn und Treffer für alle (Gast: E → `netAction(1, Kennzahl)` → `pawnInteract` →
`useApparition`); Bild und Klang beim Gast über das Weltereignis Dropped `l2rand` (Ausgabe, Zorn). Treffer und Tod der Gäste setzt der
Host (Rolle −5 → `netDie`). Die Sucht führt der Host je Spieler (Verbrauch in `pawnUse`), der Gast zusätzlich für sein Klangbild.

**Skript:** `rand cuisine <abstand> [winkel] [nr]`, `randstate cc_vend|cc_anger|cc_lurch|cc_arm <wert>`, `randstate hold 1`.

## Interpretation (begründet)

- **Gefahr nur bei Angriff:** Level 2 nennt ihn ungefährlich; die Entity-Seite lässt ihn nur bei Angriff zuschlagen. Der einzige Angriff
  des Spielers ist das Beil – also löst das Beil den Zorn aus. Den Fall „jemand erklärt einem Süchtigen, wie er funktioniert“ gibt es
  ohne Gespräche nicht.
- **Verfolgung:** Die Seite lässt ihn bis etwa 400 m verfolgen. In den engen Sackgassen steht er fest (Block), deshalb nur ein Ausfall
  bis 0,6 m auf den unteren Armen. Ein laufender Automat wäre eine eigene Rolle mit Wegen und Kollision – bewusst nicht gebaut.
- **Sucht als Spielregel:** „verdaut nichts anderes“ → Mandelwasser/Riegel wirken nicht; „bleibt beim Automaten“ → Entzug in der
  Ferne; das Weinen statt Flüstern steht so auf der Seite. Das Vergessen aller Erinnerungen an den Automaten ist nicht dargestellt.
- **Nachschub alle 12 h:** länger dauert kein Lauf – 3 Waren je Automat, kein Nachschub.
- **Rückseite prüfen:** Das Schild gibt es, aber er steht mit dem Rücken an der Wand (wie Automaten stehen); siehe Restmängel.

## Zahlen und Gleichgewicht

| Wert | |
|---|---|
| Automaten | 6 (4 Sackgassen, 2 Wände) |
| Licht | 7,5 m, im Stromausfall an |
| Ware | 3 je Automat, +40 % Verstand |
| Entzug | ab 40 m, bis +0,06 %/s nach 30 s |
| Warnung | 0,6 s Klappern/Flackern |
| Hieb | 30 % alle 0,85 s, bis 1,6 m vor dem Gehäuse (Arm 1,37 m) |
| Zorn | 5,6 s, +4 s je weiterem Treffer |

Die Ware ist eine echte Versuchung (+40 % statt +30 % eines Riegels), kostet aber jede spätere Heilung durch Mandelwasser – wer sie
nimmt, muss zurück zu den Automaten (18 Waren in der ganzen Ebene). Der Zorn tötet nur, wer drei Hiebe lang stehen bleibt.

## Wow-Check

„Ein ganz normaler Snackautomat am Ende des Gangs, der einzige Lichtfleck im Stromausfall – und aus der Nähe umklammern Knochenfinger
jede Packung, Blut läuft von den Fachkanten, und wer zuschlägt, sieht ihn auf Knochenarmen aufstehen.“ Ehrliche Antwort aus der Nähe
und im Zorn: „IIIH“; aus der Ferne bewusst unauffällig (so beschreibt ihn die Seite).

## Kritikrunden (Bilder `docs/screens/creatures/l2_randwesen/`)

**Runde 1** – `r1_cc_bogen.jpg`, `r1_cc_nah.jpg`, `r1_cc_zorn2.jpg`. *Schlecht:* Der Spieler stand im Bild; aus der Nähe ein sauberer
Automat, die Knochenhände dünn wie Zweige; im Zorn ragen die Arme dünn wie Insektenbeine seitlich hoch. *Änderung:* Kamera ohne
Spieler, Arme dicker (Radien ×1,5), Krallen größer (×2,7), Hände greifen die Ware (1,15, gekrümmt), Handabdrücke auf der Rückwand.

**Runde 2** – `r2_cc_bogen.jpg`, `r2_cc_nah.jpg`, `r2_cc_zorn3.jpg`. *Gut:* Die Knochenfinger um jede Packung sind aus der Nähe sofort
zu erkennen, die Arme greifen nach vorn. *Schlecht:* Die Rückwand überstrahlt (Abdrücke unsichtbar, sahen wie Sonnenräder aus); die
oberen Arme stehen in der Ruhe hoch wie Fühler. *Änderung:* Rückwand vergilbt und schwächer (0,3), echte Handform, Blut läuft von den
Fachkanten, ruhender Arm auf Brusthöhe.

**Runde 3** – `r3_cc_bogen.jpg`, `r3_cc_zorn3.jpg`, `r3_cc_ausgabe2.jpg`. *Gut:* Blutläufe an den Fächern; im Zorn greifen beide oberen
Arme mit Krallen nach vorn, die unteren stemmen ihn hoch; die Ausgabe (Ware fällt, liegt im Fach) ist zu sehen. *Schlecht:* Fleisch
und Fett matt wie Knete. *Änderung:* Fleisch nass (0,22), Fett 0,3.

**Runde 4** – `r4_cc_bogen.jpg`, `r4_cc_nah.jpg`. *Gut:* glänzende Ware zwischen den Fingern. *Schlecht:* Herstellerschild und Karte
trugen die Namen der Seite. *Änderung:* eigene Wörter (LICENSE_REVIEW).

**Runde 5 (aus den Augen des Spielers, mit Anzeige)** – `r5_pov_bogen.jpg`, `r5_pov_cc_taste.jpg`, `r5_pov_cc_stromausfall.jpg`,
`r5_pov_cc_zorn.jpg`. *Gut:* Die Taste ist erkennbar („Automat: eine Taste drücken“); im Stromausfall ist der Automat das einzige Licht
im Tunnel – genau die Wegmarke der Seite; im Zorn greifen die Arme in die Kamera. Abnahme.

## Tests (`gameplay_test`, `testV133L2Rand`)

Plätze in der Karte; alle Plätze besetzt; fest (Block unter der Grundfläche); leuchtet; Flüstern in der Nähe; drei Waren gratis, dann
leer (Tasten, Hand, Fall); Ware: Rausch, dann süchtig; Mandelwasser wirkt nicht mehr; Entzug in 107 m Entfernung; Sucht im Spielstand;
Zorn: Warnung ohne Hieb, dann Hiebe, außer Reichweite kein Treffer, Rückzug; Koop: Ausgabe und Zorn beim Gast.

## Restmängel

- Das Herstellerschild hinten ist im Spiel nicht zu sehen (der Automat steht an der Wand). Die Seite rät, die Rückseite zu prüfen –
  dafür bräuchte die Karte frei stehende Automaten (Wunsch an die Karte).
- Kein Verfolgen über die Sackgasse hinaus (siehe Interpretation), keine süchtigen Wanderer am Automaten (Menschen-NPCs gibt es nicht).
- Bottled Lightning (einzige Schwäche laut Seite) gibt es im Spiel nicht; der Automat ist nicht zerstörbar.
