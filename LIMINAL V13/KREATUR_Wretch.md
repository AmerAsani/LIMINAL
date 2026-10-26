# Kreatur: Wretch (Entity 15)

Quelle: [backrooms-wiki.wikidot.com/entity-15](https://backrooms-wiki.wikidot.com/entity-15), Revision 54, gelesen am 26.10.2026 (die Seite ist als „wird überarbeitet“ markiert); dazu die Ebenenseiten von Level 1, 2 und 3.
- Geschrieben von Stretchsterz und etoisle.
- Lizenz CC BY-SA 3.0.
- Zusammenfassung in eigenen Worten; kein Bild der Seite verwendet.

## Was die Seite sagt

**Gestalt:**
- Rotbraune, trockene, schuppige Haut mit Löchern und Pusteln, aus denen ein dicker, rotbrauner Saft sickert.
- Blutunterlaufene Augen; im dritten Stadium lösen sich die Lider auf – die Augen bleiben offen und rollen.
- Zähne und Nägel fallen im dritten Stadium aus und wachsen an anderen Stellen nach, in ungewöhnlicher Zahl und Größe.
- Haar fällt im zweiten Stadium aus. Oft bis auf die Knochen abgemagert – dahinter große Kraft.
- Kleidung und Habe bleiben oft am Körper, meist stark beschädigt.
- Im dritten Stadium läuft ständig brauner Schlamm aus Mund und Öffnungen.

**Der „Wretched Cycle“:** Hunger, Durst und Schlafmangel verwandeln einen Menschen über Wochen bis Monate. Stadium 1: Juckreiz, Wesensänderung. Stadium 2: Haut und Muskeln lösen sich, Löcher und Pusteln; Sprache nur noch Bitten um Wasser, Essen, Hilfe; Mandelwasser hilft. Stadium 3: Lippen und Lider lösen sich, Schlamm; nicht mehr zu retten.

**Verhalten:**
- Meist wenig klug; selten menschliches Verhalten bis hin zu Werkzeugen und Waffen.
- Übermenschlich stark (brechen durch Wände), manche schwächer.
- Von schlurfend langsam bis schneller als ein Mensch.
- In Gruppen wie ein Schwarmgeist.
- Gurgeln und Fauchen: tiefe, langsame Laute heißen meist ein trägerer, hohe ein klügerer oder schnellerer Wretch.
- Halten sich im Schatten, wo Menschen zusammenkommen.

**Ausweg (Seite):** essen, trinken, schlafen; am Laut die Gefahr erkennen; den Schlamm meiden; einen im dritten Stadium nicht retten wollen.

**Nicht gesagt:** ob Licht oder Lärm sie anziehen; wie sie angreifen.

**Ebenen:** Level 1 (aggressive, untote Menschengestalten), Level 2 (fast überall; griffen 2014 die Survivors an), Level 3 (selten, behalten den Körper, rennen, tragen Waffen, sprechen in Bruchstücken).

## Umsetzung

**Modell** (`wretch.py`, Zweibeiner-Baukasten, 1,73 m): ausgezehrt (Rippen als Grate unter der Haut, eingefallener Bauch, knotige Knie, eingefallene Wangen); rotbraune Haut (Pusteln und Löcher in der Verschiebung, Schuppen); große Augäpfel ohne Lider; Mund ohne Lippen mit zu vielen, schiefen Zähnen, dazu nachgewachsene Zähne durch die Wange und unter dem Kinn; brauner Schlamm aus dem Mund, Saftbahnen über den Körper; zerrissene Hose und Hemdreste.

**Bewegung** (Haltung „Wretch“): gebeugt, der Kopf hängt vor (Hals 0,55 rad); schlurft, das rechte Bein zieht nach (Zehen schleifen, Becken sackt ein); ruckartige Kopfbewegungen; rasselnder Atem; Sprint weit vorgebeugt mit schlackernden Armen; Flehen halb aufgerichtet mit vorgestreckten, zitternden Händen; Gurgeln mit zitterndem Kiefer.

**Verhalten** (`thinkWretch`, Level-JSON `presence.wretch`: `mode` shambler/sprinter/mixed, `chance` = Anteil im zweiten Stadium):
- Erscheint bevorzugt im Dunkeln.
- Sieht (18 m, wenn man Licht hat, sonst 7 m) und hört (Sprinten 16 m bzw. 24 m beim schnellen; Gehen 6 m).
- Träge: jagt mit 1,35 m/s, Schläge 34 Health, tiefes Gurgeln.
- Schnell: jagt in Stößen – 4–6 s mit 4,9 m/s, dann 2–3 s nach Luft ringend (die Gelegenheit zu entkommen); Schläge 26 Health; hohes Fauchen.
- Lässt ab, wenn er einen 7 s (schnell: 4 s) weder sieht noch hört, oder über 34 m.
- Zweites Stadium: fleht (Laute), folgt langsam auf 3 m, greift nie an.

**Klang:** `wretch_gurgle_0..2` (tief, nass), `wretch_snarl_0..2` (hoch, rau), `wretch_plead_0..2` (heiser, zwei, drei Silben, fallend, rasselnd einatmen).

**Eigene Auslegung:** Sehen und Hören wie die anderen Jäger; Schaden statt sofortigem Tod (ein Unterschied zu Smiler, Hound, Bündel); Sprint in Stößen; die Zahlen.

**Nicht umgesetzt:** Heilung mit Mandelwasser; Ansteckung durch den Schlamm; Mutationen (Flügel, zusätzliche Glieder, die Masse aus Augen und Mündern); Waffen der Level-3-Wretches; Horden mit Schwarmgeist.

## Gleichgewicht

| Wert | Zahl |
|---|---|
| Auftritt | Level 1 ~260 s (gemischt), Level 2 ~240 s (gemischt, 30 % flehend), Level 3 ~300 s (schnell); erster nach ~90 s |
| Schaden | 34 (träge) / 26 (schnell) je Schlag × Schwierigkeit (stalker_hit/35); Pause 1,8 / 1,3 s |
| Tempo | 1,35 m/s; schnell 4,9 m/s in Stößen (Spieler: gehen 3, sprinten 5,7 m/s) |

## Kritikrunden

Bilder: `docs/screens/wretch/`.

| Runde | Befund | Änderung |
|---|---|---|
| Blender | Rippen als riesige Stäbe wie ein Akkordeon. | Rippen als dünne Grate auf der Brustkorb-Ellipse; Zähne an falschen Stellen ergänzt (Wiki). |
| 1 (`r1_bewegung_zustaende.jpg`) | Haltung gebeugt, Kopf vor, Schlurfen und Sprint lesbar; Flehen und Zugreifen deutlich. | – |
| 2 (`r2_level2.jpg`) | Im Lampenlicht: lidlose Augen, Zähne, Schlamm – sehr unheimlich; Treffer mit roten Rändern. Die Hose wirkt im nahen Licht weiß (wie bei allen Stoffen, siehe Faceling). | – |
| 3 (`r3_level3_schnell.jpg`) | Schnell, im Dunkeln zuerst nur Umriss mit hellen Augen, dann im Licht greifend. Abgenommen. | – |

## Tests (`gameplay_test`)

Level 2: er sieht einen und jagt; der träge gurgelt tief; ein schwerer Schlag, nicht sofort tödlich; im zweiten Stadium fleht er und schlägt nicht. Level 3: der schnelle faucht hoch.
