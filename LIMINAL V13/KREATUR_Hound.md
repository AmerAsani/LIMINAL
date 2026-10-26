# Kreatur: Hound (Entity 8)

Quelle: [backrooms-wiki.wikidot.com/entity-8](https://backrooms-wiki.wikidot.com/entity-8), gelesen am 26.10.2026.
- Autor: Palico22; Neufassung: 1000dumplings.
- Lizenz CC BY-SA 3.0.
- Zusammenfassung in eigenen Worten; kein Bild der Seite verwendet.

## Was die Seite sagt

**Gestalt:**
- Menschenähnlich, aber mit fremdartigem Körperbau.
- Arme und Beine sind für den Gang auf allen vieren gebaut; das gibt ihm etwas Hundeartiges.
- Wirkt kränklich und abgemagert, hat aber beim Angriff große Kraft.
- Langes schwarzes Haar wächst am Kopf.
- Ein sehr großes Maul voller scharfer Zähne, oft vom Haar verdeckt.
- Scharfe Krallen.

**Laut:** Knurren kündigt Gefahr an.

**Verhalten:**
- Feindselig, sobald er einen Menschen sieht; wird aggressiv, wenn jemand aggressiv auftritt.
- Direkter Blickkontakt schüchtert ihn für kurze Zeit ein.
- Nicht sofort fliehen, nicht zähmen.

**Nicht beschrieben:** Haut, Augen, Größe, Farbe des Körpers.

## Umsetzung (eigene Entscheidungen, wo die Seite schweigt)

**Silhouette:**
- Ein Mensch, der auf allen vieren läuft.
- Die Schultern stehen höher als die Hüfte, weil die Arme lang sind. Der Rücken fällt nach hinten ab.
- Der Kopf hängt tief und vorgestreckt unter der Schulterlinie, das Haar fällt senkrecht davor.
- Von der Seite unverwechselbar: kein Hund, kein Affe, sondern ein menschlicher Rumpf in falscher Haltung.

**Anatomie:**
- Abgemagert: Rippen und Wirbelsäule unter straffer Haut; Schulterblätter stehen beim Gehen heraus.
- Becken- und Hüftknochen spitz, Bauch eingefallen.
- Muskeln sehnig und lang, keine Masse: Arme als Stelzen mit Sehnen, Knie und Ellbogen knotig.
- Hände: lange Finger mit Krallen, auf den Knöcheln aufgesetzt.
- Füße: menschlich, auf den Zehenballen; die Ferse steht hoch.

**Kopf:**
- Ein schmaler Schädel; das Maul reicht weit zurück bis unter die Ohren, die Lippen sind zurückgezogen.
- Unregelmäßige, schmale, spitze Zähne.
- Das Haar fällt vom Scheitel in schweren, nassen Strähnen bis unter das Kinn und deckt den Großteil des Gesichts.
- Augen nur als schwacher Schimmer zwischen den Strähnen (eigene Entscheidung, Seite schweigt).

**Haut:**
- Fahl, grau-gelblich, an Gelenken und über Knochen gerötet und rissig.
- Stellenweise schorfig (eigene Entscheidung: kränklich).
- Matt, an Lippen und Maul feucht.

**Haar:** schwarz, schwer, strähnig, nass glänzend. Nur am Kopf (laut Seite) und als Mähne den Nacken hinab.

**Bewegung:**
- Kreuzgang mit Gewichtsverlagerung: die Schultern rollen, der Kopf pendelt gegen.
- Galopp mit Streckung.
- Pirschen: geduckt, Ellbogen hoch.
- Knurren mit Zittern des Kiefers, Atmen sichtbar in den Flanken.

**Abgrenzung zu den anderen Wesen:** einziges vierbeiniges, menschliches Wesen; Haut statt Rauch oder Chitin.

## Prüfkriterien (Kritikrunden)

- Silhouette von der Seite und von vorn sofort als „Mensch auf allen vieren“ lesbar, nicht als Hund aus Rohren.
- Rippen, Schulterblätter und Wirbel sichtbar, aber unter Haut, nicht als Stäbe.
- Gelenke knotig; Hände mit Krallen, die den Boden berühren; Füße auf den Ballen.
- Haar deckt das Gesicht, im Lampenlicht nass glänzend.
- In Bewegung: keine Durchdringungen, keine Knicke an Schultern und Hüfte.

## Kritikrunden (Spielkamera und Vorführkamera)

Bilder: `docs/screens/creatures/hound/` (vorher: `docs/screens/creatures/vorher/hound_*`).

| Runde | Befund | Änderung |
|---|---|---|
| 1 | Im Spiel fast schwarz; Konturen mit Stufen; Rippenmuster flimmerte. | Umlaufsinn aus OpenVDB umgedreht (die Höhlung war überall 1); Toleranz beim Übertragen ins VDB-Gitter auf 1e-6 (Stufen); Frequenzen der Verschiebung unter die Gittergrenze. |
| 2 | Gesicht zu lang, Maulspalt zu schmal; Hände wie Paddel; Haar auch über den ganzen Rücken (die Seite nennt Haar nur am Kopf). | Kürzeres Gesicht, Maul bis unter die Wangen, Zähne auf einem Bogen; Hände mit Mittelhandgraten und gebogenen Fingern mit Krallen; Rückenhaar nur noch den Nacken hinab. |
| 3 | Strähnen zu gleichmäßig; Zähne weiß und gleich; Oberschenkel zu dünn; Augenschimmer zu hell. | 22 Strähnen je Bündel; Zähne ungleich und vergilbt; Oberschenkel kräftiger; Augenschimmer gedämpft. |
| 4 | Strähnen dick wie Seile (1,7 cm). | 34 dünnere Strähnen je Bündel (Grund 1,1 cm, zur Spitze auslaufend); Haarnetz 1,6 mm. |
| 5 | Das freie Auge las sich als Glubschauge. | Auge kleiner und tiefer, schweres Ober- und Unterlid über der halben Iris. |
| 6 | Mensch auf allen vieren, abgemagert, Räude, nasse Strähnen vor dem Gesicht, halb geschlossene Augen. Abgenommen. | – |

**Bleibt offen:** Aus der Nähe wirkt die vordere Zahnreihe noch etwas wie ein Gebiss.
