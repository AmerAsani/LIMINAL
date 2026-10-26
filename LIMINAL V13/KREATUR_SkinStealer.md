# Kreatur: Skin-Stealer (Entity 10)

Quelle: [backrooms-wiki.wikidot.com/entity-10](https://backrooms-wiki.wikidot.com/entity-10), Revision 61, gelesen am 26.10.2026; dazu die Ebenenseiten von Level 1, 3, 5 und 188.
- Ursprünglich AndromedaMapping1; Neufassung 1000dumplings.
- Lizenz CC BY-SA 3.0.
- Zusammenfassung in eigenen Worten; kein Bild der Seite verwendet.

## Was die Seite sagt

**Gestalt:**
- Groß, menschenähnlich, sehr kräftig – reißt Menschen mit bloßen Händen auseinander.
- Blassgelbe Haut, eingesunkene, weiße Augen; die Außenhaut ist mit winzigen Noppen besetzt wie die Saugnäpfe eines Kraken.
- Das Blut ist durchsichtig (das sicherste Merkmal).
- Er zieht Opfern die Haut ab; die Noppen halten sie fest, ziehen sie passend, pumpen Blut hinein und heilen Schnitte und Nähte. Dann sieht er aus wie ein Mensch.

**Verhalten:**
- Satt: geht ziellos umher, nicht feindselig, solange man ihn nicht reizt.
- Hungrig: sucht Menschen, die allein unterwegs sind, und tötet sie mit bloßen Händen.
- Nach dem Fressen wird die Haut in etwa einem Tag verdaut; dann ist er wieder satt.
- Spricht, versteht aber nicht: wiederholt, was er hört, oft in mehreren Sprachen, um zu locken; spricht selten.

**Ausweg (Seite):** langsam und ruhig weggehen; nicht reizen; auf Menschen achten, die kaum sprechen oder Worte wiederholen.

**Ebenen (Ebenenseiten):** Level 1 (weiße Menschengestalten, die Haut tragen), Level 3 (tarnen sich als Menschen), Level 5 (ohne Verhalten), Level 188 (Bewohner der Gänge).

## Umsetzung

**Modelle** (Zweibeiner-Baukasten, Skelett `bp` mit eigenen Maßen, 2,05 m):
- `ent_skinstealer` – wahre Gestalt: kräftig, lange Glieder, große Hände mit langen Fingern, blassgelbe Haut mit eigener Texturebene „Blass“ (Noppen: Mulde mit hellem Rand, dicht an dicht, feucht), tiefe Augenhöhlen unter schwerem Brauenbogen, weiße Augen weit hinten, flache Nase, schmaler Mund ohne Lippen.
- `ent_skinstealer_skin` – getarnt: ein sehr großer, hagerer Mann mit gewöhnlichem Gesicht (Lider, Brauen, Nase, Lippen, Ohren), kurzem Haar, Hemd, Hose, Schuhe. **Kein sichtbares Merkmal** – die Seite sagt, man sieht keinen Unterschied.
- Erster Entwurf mit Hautlappen und verrutschter Maske verworfen (widersprach der Seite, E-031).

**Bewegung** (Haltung „Skin-Stealer“): lange Schritte, zu wenig Armschwung (er spielt einen Menschen), langsam drehender Kopf mit gelegentlichem Ruck; spricht er, bewegt sich der Kiefer in Silben; jagend weit vorgebeugt, greift mit beiden Händen.

**Verhalten** (`thinkSkinStealer`, Level-JSON `presence.skinstealer`: `mode` skin/true, `chance` = Anteil hungrig):
- Satt: geht umher; sieht er einen, bleibt er stehen und sieht her; getarnt spricht er ab und zu Worte nach.
- Reiz: Lampe im Gesicht (unter 10 m), näher als 2 m, Rennen dicht bei ihm. Ab 3 s Reiz: Angriff (4 m/s), tödlich.
- Hungrig und man ist allein: er folgt (1,15 m/s, wenn man ihn sieht; 2,3 m/s ungesehen) und spricht nach. Unter 6,5 m hält er 1,4 s inne, der Kiefer öffnet sich (Ankündigung). **Weicht man in dieser Zeit ruhig zurück** (mehr als 0,7 m), lässt er einen ziehen und folgt nur wieder (6 s Pause). Wer bleibt oder rennt, wird angegriffen.
- Im Mehrspieler sucht er Einzelne: steht ein Mitspieler näher als 6 m beim Ziel, gibt er sich harmlos.
- Gibt nach 90 s oder 25 s außer Sicht auf.

**Klang:** nachgesprochene Silben mit Mitlauten, flache, zu gleichmäßige Betonung, Wiederholungen, manchmal fragend (`skin_mimic_0..5`); schwere, barfüßige Schritte.

**Eigene Auslegung:** 2,05 m (die Türen sind 2,2 m hoch, B-014); Reizschwellen; das Innehalten als Ankündigung und das Zurückweichen als Ausweg; die Lampe als Reiz.

**Nicht umgesetzt:** das durchsichtige Blut (keine Verletzungen im Spiel); Sprechen in mehreren Sprachen (die Silben sind sprachlos); das Abziehen der Haut.

## Gleichgewicht

| Wert | Zahl |
|---|---|
| Auftritt | Level 1 alle ~300 s (getarnt, 25 % hungrig), Level 3 ~280 s (getarnt, 30 %), Level 5 ~360 s (wahr, 30 %), Level 188 ~320 s (wahr, 35 %, nur in den Gängen); erster frühestens nach 150 s |
| Folgen | 1,15 m/s gesehen, 2,3 m/s ungesehen (Spieler geht 3 m/s) |
| Angriff | 4 m/s (Spieler sprintet 5,7 m/s), tödlich unter 1,2 m |

## Kritikrunden

Bilder: `docs/screens/skinstealer/`.

| Runde | Befund | Änderung |
|---|---|---|
| Blender 1 | Rippen standen wie Stäbe ab; Glieder dünn wie Stelzen; die getragene Haut als Lappen widersprach der Seite. | Ohne Rippen, kräftiger; zwei Gestalten (wahr, getarnt). |
| Blender 2 | Sanduhr-Rumpf (Brust und Bauch als getrennte Kugeln): der Baukasten maß die Wülste in festen Metern. | Wülste und Muskeln wachsen mit Rumpf- und Gliedlänge mit. |
| 1 (`r1_level5_gereizt.jpg`) | Von der Lampe gereizt stürmt er mit vorgestreckten Händen – stimmig. In Tests reagierte er oft nicht auf die Lampe: Kopf über dem Türsturz (2,3 m), Lampe in der Wandkachel (B-013, B-014). | 2,05 m; Licht auch vom Auge, Brust als zweiter Punkt. |
| 2 (`r2_level3_getarnt.jpg`) | Getarnt: ein großer Mann im Dunkeln – wirkt wie ein Mensch. Hungrig tötete er aus 3,8 m in 2,5 s: kein Ausweg. | Innehalten 1,4 s; ruhiges Zurückweichen lässt ihn ablassen (Wiki: ruhig weggehen). |
| 3 (`r3_level5_205m.jpg`) | 2,05 m im Türrahmen, gereizt tödlich, im Zugriff die weißen Augen nah – stark. Abgenommen. | – |

## Tests (`gameplay_test`)

Level 5: wahre Gestalt; satt harmlos (ruhig dabeistehen); gereizt durch die Lampe tödlich. Level 3: getarnt; hungrig folgt er, spricht nach, hält inne, tötet; wer im Innehalten ruhig zurückweicht, den lässt er ziehen.
