# Kreatur: die Wurzelpistole (Entity 39) und die Spuren der „Gaben“ in Level 188

Quellen (Backrooms-Wiki auf Wikidot, **CC BY-SA 3.0**, gelesen 03.11.2026, eigene Worte, keine Zitate, keine Bilder):
[Level 188 „The Courtyard of Windows“](https://backrooms-wiki.wikidot.com/level-188) Rev. 69 (24.10.2026; Kai4C u. a.), Abschnitt
*Main Atrium* (Moos) – dort verlinkt das Wort „gifts“ auf [Entity 39](https://backrooms-wiki.wikidot.com/entity-39) Rev. 46
(02.11.2026; Autoren r a t i f und YourWeirdPlant; die Seite trägt den Hinweis, dass sie neu geschrieben wird). Den Kult nennt die
Ebenenseite **ohne Link**; eine eigene Seite unter `backrooms-wiki.wikidot.com/amor-incrementum` gibt es nicht (geprüft am 03.11.2026:
HTTP 404). Im Spiel fällt kein Wiki-Name (E-039): die Pistole hat keinen Namen, der Kult heißt im Probenzettel „die Gärtnerinnen“, ihre
Leiterin „ihre Vorsteherin“; „M.E.G.“ erscheint nicht.

## Was die Seiten sagen (eigene Worte)

- **Level 188 (Moos):** Besucher gaben Proben vom Grün zwischen den Wegen ab; die Untersuchung zeigte: kein Kunstrasen, lebendes Moos.
  Die Proben gingen an einen Kult, der sich herzlich bedankte und **Gaben** zurückschickte; dessen Priesterin schrieb, mit dem Opfer seien
  Bußlieder bezahlt. Mitglieder des Kults sind auf der Ebene nicht beschrieben.
- **Entity 39:** pflanzliche Wesen **in Gestalt von Schusswaffen** (häufig Pistolen und Sturmgewehre). Die Schale entsteht aus Mineralien und
  Metall im Boden; wo wenig Metall ist, bleibt sie **schwach und kann beim Schuss versagen**. Am Griff Wurzeln; wer sie mit viel Kraft
  **herauszieht**, dem schlingen sich die Wurzeln **um Hand und Unterarm**, sind einige Sekunden **eiskalt** und füllen sich mit **Blut**.
  Aus verdichtetem Blut entstehen in einer **verborgenen Kammer zwischen Griff und Magazin** Kugeln (Feuersalz statt Schießpulver). Die
  Pflanze regt die Bildung roter Blutkörperchen an und zieht die meisten zu sich; **leere Hülsen sickern aus dem Lauf**. Winzige Samen
  stecken in den Kugeln: ein Schuss kann **keimen** und durch fast jeden Boden außer Metall wurzeln (Nachkommen auf gepflasterten Böden,
  etwa in Level 1). Sie braucht viel Eisen und wurzelt tief; **unberührt welkt sie nach etwa zwei Wochen**. Gesunde spüren nach dem Gebrauch
  **leichte Müdigkeit**; man soll sie in einem **Kasten** aufbewahren, damit sie die Haut nicht berührt, nicht allein benutzen und keine
  abfeuern, die man nicht herausziehen kann. Von sich aus bewegt sie sich nicht.

## Auslegung (begründet)

Die „Gaben“ des Kults sind Wurzelpistolen, geschickt in Holzkästen mit Moos (die Seite: im Kasten aufbewahren). **Ein Kasten war offen**:
eine Gabe kam mit Besuchern in den Hof, ein Schuss keimte im **Moos**, einer in einem **Riss des Gangbodens** (gepflasterte Böden – wie die
Nachkommen in Level 1). Das Moos ist mager (der Probenzettel: kaum Eisen) – beide Pflanzen sind **verkümmert**: sie lassen sich nicht
herausziehen, und ihre schwache Schale **entlädt sich von selbst**, sobald die Kammer voll ist (die Seite: kann versagen). Daraus folgt das
Spiel: Wer eine anfasst, will sie herausziehen – sie sitzt fest, die Wurzeln greifen zu und trinken; man kann sich losreißen (Schmerz)
oder es geschehen lassen (Müdigkeit, und ein **Knall**, der durch Hof und Gänge hallt – der Horcher hört ihn). Der Schuss fällt kurz vor
ihr ins Moos und keimt. Danach welkt sie eine Weile (die Seite: welkt unberührt; hier verkürzt auf 150 s). Eine Waffe für den Spieler gibt
es nicht – Auftrag und Seite: keine, die man nicht herausziehen kann.

**Spuren des Kults** (Menschengruppe, auf der Ebene nicht anwesend): drei ausgestochene Moosproben mit Stäben und Fähnchen, der leere
Holzkasten der Gabe (innen Moos, die Mulde in Form der Pistole leer, abgerissene Wurzelfasern), der **Probenzettel** als Fund (eigene
Worte: Proben 1–3, lebendes Moos, mager; weitergegeben an die Gärtnerinnen, die sich mit Gaben bedankten; Lieder der Buße; ein Kasten war
offen; nichts anfassen, das aussieht wie eine Waffe – es hat Wurzeln und Durst).

## Umsetzung (Weltobjekt, keine Rolle – sie bewegt sich nicht von der Stelle)

| Teil | Datei |
|---|---|
| Ablauf (wittern, Griff, Züge, Schuss, Keimling, Losreißen, welk), Wurzeln (6 × 7 Glieder), Hülsentropfen, Mündungsblitz, Koop | `src/gameplay/courtyard_l188.hpp/.cpp` |
| Sitzung: Stelle „Herausziehen“ (Benutzen-Art 12), Schaden/Müdigkeit, Lärm, Fund, Weltereignis `l188`, eigene Hand | `src/gameplay/session_courtyard.cpp` |
| die eigene Hand greift hinunter und wird gehalten (`Body::holdHand`, `Gesture::Held`) | `src/gameplay/body.hpp/.cpp` |
| Modelle (Blender): `e39_gun`, `e39_root`, `e39_husk`, `e39_bed_moss`, `e39_bed_crack`, `e39_sprout`, `e39_samples`, `e39_case`, `e39_note` | `tools/blender/models/models_l188_gifts.py` |
| Klänge `e39_creak`, `e39_wrap`, `e39_suck`, `e39_bang`, `e39_drip`, `e39_wilt`, `e39_tear` | `src/audio/synth_listener.cpp` |
| Orte (Spawnpunkte `trace/e39`: `moss`, `floor`, `seed`, `note`), Kasten und Probenstiche | `tools/levels/level188.py` (am Ende ergänzt) |
| Probenzettel | `data/notes.json` („Probenzettel aus dem Hof“, mode `fund`) |
| Skriptbefehl `court <hold|grip|wrap|fire|wilt|recover|drip|sprouts> <wert>`, Licht | `src/app/app.cpp` |

**Ablauf.** In Ruhe steckt sie schief im aufgeworfenen Moos bzw. im gesprungenen Beton; ein Drittel der Wurzeln liegt frei, der Rest
taucht in den Boden (im Gang folgen sie den Rissen). Kommt jemand näher als 2,4 m, **wittert** sie: die Wurzeln zucken und kriechen ein
Stück auf ihn zu, der Lauf dreht sich bis 0,45 rad nach ihm, die Rinde knarrt alle 2,4–4,8 s. Alle 7–15 s **quillt ein Tropfen leerer
Hülsen** aus dem Lauf und fällt (leises Tropfen). **Anfassen** („Herausziehen“): die eigene Hand greift hinunter und wird festgehalten;
die Wurzeln steigen in 1 s vom Griffende zur Hand und winden sich spiralig um den Unterarm bis zum Ellbogen (Reiben, nasses Zuziehen);
„Eiskalt. Dann zieht es im Arm – sie trinkt.“; drei **Züge** (je 5 Schaden, roter Rand, Saugen und Puls), dabei pulsieren die Wurzeln.
Nach 3,9 s **entlädt sich die Kammer**: Knall mit langem Hall (Feuersalz – trocken, hell, nass nachplatzend), Mündungsblitz, Hülsen
spritzen, **Lärm** (Stärke 1,0 – der Horcher hört ihn bis 30 m), der Schuss keimt an einem Keimort 1,2–7 m vor dem Lauf (der Keimling
wächst in 60 s auf etwa 10 cm, höchstens 6). Die Wurzeln lassen los, die Hand ist frei; **müde** 45 s (Tempo 86 %). Dann **welk** (150 s):
schlaff, Lauf hängt; anfassen tut nichts. **Losreißen**: wer sich im Griff weiter als 1,7 m entfernt, reißt die Wurzeln aus dem Fleisch
(9 × Schwierigkeit Schaden, Reißen), kein Schuss, 90 s welk. **Nie tödlich** (mindestens 5 Lebenspunkte bleiben).

**Mehrspieler.** Der Host entscheidet Griff, Züge, Schuss, Keimort, Losreißen und Erholung und meldet sie als Weltereignis `Dropped "l188"`
(Kennzahl Art << 24 | Pistole << 16 | Spieler << 8 | Zusatz, Ort = Keimort); jeder Rechner rechnet Wurzeln, Klang, Blitz und Keimling.
Wittern, Knarren und Tropfen rechnet jeder Rechner selbst. Ein Gast fasst sie über die gewöhnliche Benutzen-Meldung an (Host:
`pawnInteract` → Benutzen-Art 12); Müdigkeit des Gastes rechnet der Host im `pawnTick`. Protokoll unverändert.

## Zahlen

| Wert | |
|---|---|
| Wittern | unter 2,4 m; Lauf dreht bis 0,45 rad; Knarren alle 2,4–4,8 s |
| Hülsen | alle 7–15 s ein Tropfen (quillt 0,6 s, fällt) |
| Griff | Wurzeln 1 s; Züge bei 1,45 / 2,25 / 3,05 s, je 5 Schaden (heilt nach); Schuss bei 3,9 s |
| Schuss | Lärm 1,0 an der Mündung; Keimort 1,2–7 m vor dem Lauf; Keimling 60 s bis ausgewachsen, höchstens 6 |
| Folgen | müde 45 s (Tempo 0,86); welk 150 s |
| Losreißen | über 1,7 m: 9 × Schwierigkeit Schaden, welk 90 s |

Gemessen im `gameplay_test`: drei Züge, tiefster Stand 8,6 unter dem Anfang (die Gesundheit heilt zwischen den Zügen nach), müde 0,86,
ein Keimling.

## Wow-Check

„IIIH, was ist das?“ – Im Moos steckt eine Pistole, deren Griff aus Rinde ist; als du dich bückst, drehen sich ihr Lauf und die
Wurzelspitzen nach dir. Du greifst zu, sie lässt sich nicht herausziehen, und blasse, fleischige Wurzeln kriechen dir in Spiralen den
Unterarm hinauf und ziehen im Takt deines Pulses – dann knallt es zwischen deinen Fingern, und irgendwo hinter den Türen fängt etwas an
zu laufen. Ehrliche Antwort: nicht „ganz okay“ – der Griff im Ego-Bild ist der Schreck; die Pistole selbst ist bewusst klein und still.

## Kritikrunden

Bilder: `docs/screens/creatures/wurzelpistole/` (Bögen aus `tools/qa/shots_l188.py`: nah/mittel, mit und ohne Lampe, Griff im Ego-Bild,
Schuss, welk mit Keimlingen, im Riss des Gangs, Spuren).

**Runde 1** – `r1.jpg`. *Schlecht:* die Pistole liest sich als **Spinne** (sechs lange, dünne, dunkle Wurzeln, beim Wittern wie Beine
aufgestellt); das Beet eine schwarze Scheibe; der Griff im Ego-Bild nur ein Knäuel am Boden (die Spirale zeigte zur Kamera). *Änderung:*
Wurzeln kürzer, am Griff dicker, zum Ende in den Boden, Wittern kriecht statt aufzustehen; Beet als aufgeworfener Moosbuckel; Spirale
länger.

**Runde 2** – `r2.jpg`. *Schlecht:* Beet weiter schwarz (Moos-Material dunkel gegenüber dem Boden des Hofs); Griff ohne sichtbaren Arm
unlesbar; im Gang sechs schwarze Strahlen (Risse) wie Spinnenbeine. *Änderung:* Moos-Albedo wie die Zone `hof`; **die eigene Hand greift
hinunter** (`Body::holdHand`) und die Wurzeln winden sich um den echten Unterarm; zwei lange, verzweigte Risse statt Strahlen, heller.

**Runde 3** – `r3.jpg`. *Gut:* **der Griff im Ego-Bild** – der eigene Arm reicht zur Pistole, die Wurzeln winden sich um den Unterarm;
Beet fügt sich ins Moos. *Schlecht:* in Ruhe noch ein Stern aus Strichen; im Gang Wurzeln quer zu den Rissen. *Änderung:* Wurzeln nach
einem Drittel unter dem Moos; im Riss folgen sie den zwei Rissen.

**Runde 4** – `r4.jpg`. *Gut:* Mündungsblitz sichtbar; im Gang liegen die Wurzeln in den Rissen. *Schlecht:* Wurzeln auf weißem Beton
schwarz (Holzebene); Keimlinge außerhalb des Bilds. *Änderung:* Wurzeln fleischig blass (kein Holz), Bild auf den Keimort.

**Runde 5** – `r5.jpg`. *Gut:* blasse Wurzeln lesen sich als Wurzeln, nicht als Spinne; Griff stark. *Schlecht:* Keimlinge grün auf grün
unsichtbar. *Änderung:* Keimling 1,5-fach, trockener Moosring ums Einschussloch, blasse Keimblätter.

**Runde 6 (Abnahme)** – `r6.jpg`; beste Ansichten Bild 4/5 (Griff im Ego-Bild, ohne und mit Lampe), 6 (Schuss), 7 (Keimlinge),
8 (im Riss des Gangs), 9/10 (Probenstiche, leerer Kasten). Abgenommen.

## Tests (`gameplay_test`, `testV133Level188`)

Zwei Pistolen (Moos, Riss), Keimorte, Probenzettel, Kasten und Probenstiche in der Karte; Modelle vorhanden; Probenzettel als Fund im Moos;
Wittern; Anfassen → Griff, eigene Hand gehalten; drei Züge; Knall, ein Keimling; müde, welk; Hand wieder frei; welk anfassen tut nichts;
Losreißen (Schaden, Reißen, kein Schuss); nie tödlich; Knall im Gang lockt den Horcher; Koop: Host meldet Griff, Züge, Schuss als
Weltereignis `l188`, beim Gast Schuss, Klang und Keimling.

## Restmängel

- Die Pistole ist klein (Pistolengröße, wie die Seite) und liest sich aus mehr als 3 m nur als dunkler Gegenstand im Moos; erst das
  Wittern (Knarren) und die Aufforderung „Herausziehen“ verraten sie.
- Der Kasten liest sich von oben eher als Rahmen (der offene Deckel liegt flach dahinter).
- Andere Formen der Seite (Sturmgewehre, seltenere Modelle) sind nicht gebaut – im mageren Moos des Hofs wächst nur die kleine Form
  (Auslegung).
