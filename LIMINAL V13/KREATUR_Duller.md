# Kreatur: Duller (Entity 6)

Quelle: [backrooms-wiki.wikidot.com/entity-6](https://backrooms-wiki.wikidot.com/entity-6), Revision 52 (letzte Bearbeitung 07.08.2026), gelesen am 27.10.2026; dazu [Level 1](https://backrooms-wiki.wikidot.com/level-1), Revision 83 (07.10.2026).
- Entity 6: ursprünglich ein anonymer Fandom-/Reddit-Nutzer, überarbeitet von Stretchsterz. Level 1: Praetor3005 und DivineAtlas.
- Lizenz CC BY-SA 3.0.
- Zusammenfassung in eigenen Worten; kein Bild der Seite verwendet.

## Was die Seite sagt

**Gestalt:**
- Menschenähnlich, groß, dunkelgrau, zerbrechlich wirkender, sehr dürrer Knochenbau.
- Kein Gesicht, keine Ohren, überhaupt keine Gesichtszüge.
- Lange Arme, die sich sehr weit ausdehnen können – so erreichen sie Beute auf der anderen Seite einer Wand.
- Wackliger Stand, unnatürlicher Gang; viel stärker, als er aussieht; rennt sehr schnell, selbst mit Lasten, die doppelt so schwer sind wie er.

**Verhalten:**
- Flieht meist vor allem, was ihm nahekommt – und gilt trotzdem als feindselig.
- Jagd: Er steht in einem Flur, das Opfer im Flur hinter der Wand; er schiebt den Arm durch die Wand, packt und zieht das Opfer zu sich. Was danach geschieht, weiß man nicht.
- Bemerkt man ihn außerhalb dieser Jagd, rennt er lautlos davon. Läuft man auf ihn zu, flieht er.
- Nimmt manchmal scheinbar zufällige Dinge mit.
- Meidet Mandelwasser (Brunnen und Gegenstände damit).

**Level 1:** einer der häufigen Bewohner; verdächtige Geräusche an Wänden (Kratzen, Schaben, Rascheln) heißen: Abstand zu den Wänden halten.

**Nicht gesagt:** Sinne, Licht- und Lärmreaktion, Größe in Metern, wie er tötet, Laute (außer dem Schaben an der Wand).

## Umsetzung

**Modell** (`tools/blender/creatures/duller.py`, Skelett `anim/creature_duller.cpp`, 2,19 m, 59 Knochen):
- Ausgemergelt: Brustkorb mit Rippengraten, Brustbein, Schlüsselbeine, Schulterblätter und Dornfortsätze treten hervor, eingefallener Bauch, Beckenkämme; knotige Knie und Knöchel, lange Zehen.
- Kopf: ein länglicher Schädel ohne Gesicht und Ohren; die Haut ist über allem zugewachsen und spannt sich straff über die Knochen – Brauenwulst, tiefe Mulden, wo Augen wären, Wangenknochen, eingefallene Schläfen und Wangen, schmaler Kiefer, eine kaum sichtbare Falte, wo ein Mund wäre; keine Öffnung (**Interpretation**: „kein Gesicht“ als zugewachsen statt glatt wie ein Ei).
- Arme: Oberarm und Unterarm, der Unterarm aus **sieben ineinander steckenden Gliedern** (jedes wie ein langgezogener Fingerknochen: Schaft mit Knochenkante und zwei Sehnen, am Ende zwei Gelenkknorren mit Ringfalten). Eingefahren liegen sie im Unterarm (die Hände hängen bis unter die Knie), ausgefahren reicht der Arm 3,4 m weit – vielgliedrig und peitschendünn. Große, flache Hände, Finger bis 24 cm, ohne Nägel. (**Interpretation** der „sehr weit ausdehnbaren“ Arme; das Spiel verformt Knochen nur starr – Strecken wäre nicht möglich.)
- Material `FM_ENT_DULLERSKIN`: dunkelgrau, trocken, stumpf (Rauheit ×1,5), Haarrisse; kein rötliches Durchscheinen.
- **Bruchstelle** (gehört zum Wesen, sitzt auf der Wandseite des Opfers): fünf Brocken, die zusammen die Wölbung bilden (Kuppel 0,28 m, 10 cm), haarfeine verzweigte Risse mit Abplatzern, ein Rand aus gebrochenem Beton mit freiliegenden Kieseln um ein unregelmäßiges dunkles Loch. Material `FM_ENT_CONCRETE` (frischer Bruch, hell, stumpf).

**Bewegung** (`CreatureAnimator::updateDuller`):
- Ruhe: tief gebeugt, der Kopf hängt vor, er wiegt sich ungleichmäßig; der gesichtslose Kopf „lauscht“ schief, zuckt.
- Gang: wacklig – das Becken schwankt weit, die Füße setzen fast über Kreuz, die Schrittlänge schwankt von Schritt zu Schritt, alle 1,4–4,2 s knickt ein Knie ein (Becken sackt, Rumpf ruckt zur Seite), der Kopf pendelt locker nach; die langen Arme schlenkern.
- Wahrnehmen: der Kopf schnellt herum (steife Feder), er erstarrt.
- Jagen (Schaben): beide Hände an der eigenen Wandseite, die Finger schaben, er lehnt sich an die Wand, Kopf schief an der Wand.
- Angriff: der rechte Arm richtet sich auf die Bruchstelle, die Glieder gleiten heraus und folgen dem Bogen von der Wand zum Opfer; die Hand ist erst gespreizt und schließt sich (Griff). Reicht er nicht, tastet die Hand suchend hin und her.
- Rückzug/Flucht: weit vorgebeugt, lange Schritte, die Arme schleifen nach hinten; lautlos.
- Bruchstelle: die Wölbung wächst (zittert beim Schaben), Risse erscheinen; beim Durchbruch fliegen die Brocken heraus, prallen auf und bleiben liegen; ungesehen heilt die Wand (Level 1: was sich verändert, verschwindet, wenn man nicht hinsieht).

**Verhalten** (`gameplay/presence_duller.cpp`, Level-JSON `presence.duller`: `every` mittlerer Abstand der Auftritte):
- Erscheint ungesehen 12–30 m Weg entfernt und lauert ungesehen 5–14 m vom Spieler (nie im Blickfeld).
- Hört den Spieler durch die Wände: Weg oder Luftlinie × 1,6 < Lärm × 18 m (rennen 18 m, gehen 8 m, schleichen 2,7 m); geworfene Dinge ebenso.
- Hinterhalt: steht der Spieler höchstens 1,25 m vor einer Wand (1–3 Kacheln dick, ±0,38 m daneben keine Tür), hinter der ein Platz ohne Sicht liegt, geht der Duller dorthin (bis 17 m Weg) und folgt dem Spieler die Wand entlang.
- Ankündigung: 3,0 s Schaben in der Wand neben dem Spieler (Klang kommt aus der Wand), ab 1,4 s wölbt sich der Putz, zweimal Knacken.
- Durchbruch, wenn der Spieler noch vor der Wand und näher als 4 m ist; wer dann waagrecht höchstens 1,75 m von der Bruchstelle steht, wird gepackt und zur Wand gezogen (**tödlich**). Sonst tastet der Arm 1,6 s und gleitet zurück; Pause 22–36 s.
- Gesehen (der Körper deutlich im Blick): 0,35 s Erstarren, dann lautlose Flucht mit 6 m/s; ungesehen und 12 m weg verschwindet er.
- Mandelwasser in der Hand (gewählter Platz): er bricht nicht durch, weicht zurück, 60 s Pause.

**Klang** (`audio/synth_duller.cpp`): `duller_scrape_0..3` (Finger und Nägel über Beton, raschelnde Haut, Knöchel, dumpf in der Wand), `duller_bulge_0..1` (Ächzen des Betons, Knistern, rieselnde Krümel), `duller_burst` (Schlag, Brocken prasseln, Haut über Bruchkanten, sieben Glieder rasten ein), `duller_retract`, `duller_step_0..3` (weiche nackte Sohlen, Gelenkknacken), `duller_grab` (Schreck: Druckstoß, Finger schließen sich, Schleifen durch den Beton). **Eigene Stimme**: er hat keinen Mund – seine Stimme ist das trockene Knacken der Gelenke, das beim Ausfahren in schneller Folge klickt.

**Mehrspieler:** KI beim Host; die Puppe beim Gast bekommt Lage, Zustand und die Kanäle reach (Ausfahren), bait (Wölbung), shiver (Bruchstelle sichtbar), hang (Abstand zur Wandfläche / 4 m), up (Wandnormale), jaw (Griff), alert (Schaben), perceive, growl (Flucht) – die Bruchstelle rechnet die Animation daraus.

**Nicht umgesetzt:** Diebstahl von Gegenständen; Mandelwasser-Brunnen (gibt es im Spiel nicht); was mit Gepackten geschieht (das Spiel zählt es als Tod).

## Gleichgewicht

| Wert | Zahl |
|---|---|
| Auftritt | Level 1, im Mittel alle 150 s (±30 %, × Pause der Schwierigkeit), einer zur Zeit |
| Hören | Lärm × 18 m (Weg oder Luftlinie × 1,6) |
| Ankündigung | 3,0 s Schaben, Wölbung ab 1,4 s |
| Reichweite | 1,75 m waagrecht von der Bruchstelle (Spieler höchstens 1,25 m vor der Wand, damit er anläuft) |
| Schaden | Zugriff tödlich (wie Smiler, Hound, Bündel) |
| Tempo | lauernd 1,4 m/s, anschleichend 1,6–2,4 m/s, Flucht 6,0 m/s (Spieler: gehen 3, sprinten 5,7) |

## Wow-Check

„Man steht in einem Gang, hinter der Wand schabt es, der Putz neben einem wölbt sich und reißt – dann bricht ein grauer Arm aus zu vielen Gliedern hervor und greift zu; wer ihn später doch einmal sieht, sieht ein gesichtsloses, schädelhaftes Gerippe, das auf Spinnenbeinen lautlos davonrennt.“ Ehrliche Antwort: **ja** für Durchbruch und Flucht; der Körper im vollen Licht der Lampe wirkt bleich (Belichtung, siehe Restmängel).

## Kritikrunden

Bilder: `docs/screens/creatures/duller/` (Blender-Vorschau `blender_r1.jpg`, `blender_endstand.jpg`).

| Runde | Bilder | Was ist schlecht | Was ändert sich |
|---|---|---|---|
| 1 | `r1_koerper_und_wand.jpg` (Halle fern/nah, Wand mittel, Arm nah) | Fehler: die Teile der Bruchstelle stehen ohne Wand vor der Brust (Scheibe mit Rissen). Körper wie eine Gliederpuppe, Kopf ein glattes Ei. Risse wie schwarze Spinnenbeine, Brocken schwarz (das Steinmaterial nutzt die Horn-Textur), der Arm ein glattes Rohr, die Hand winzig. | Ohne Bruchstelle liegen ihre Teile unter dem Boden. Neues Material `FM_ENT_CONCRETE` (heller Bruch, Zahnschmelz-Textur grau); Risse haarfein, verzweigt, ungleich lang, mit Abplatzern; unregelmäßiges Loch; Glieder als Fingerknochen (Schaft, Knochenkante, Sehnen, zwei Gelenkknorren); Hand 13 cm, Finger bis 24 cm; tiefere Haltung. |
| 2 | `r2_wand_beton.jpg` (Wand mittel, Wölbung, Durchbruch, Arm nah) | Wölbung jetzt lesbar, Risse gut; Beton zu weiß und sauber (wie ein Teller), Arm noch wie ein Teleskop-Rohr, glänzt. | Beton dunkler und stumpfer (Rauheit ×1,9), Haut stumpf (×1,5), Knoten größer, Schaft dünner (Taille 0,66). |
| 3 | `r3_woelbung_arm.jpg` (Wölbung, Risse, Durchbruch, Arm fern, Körper im Raum dahinter) | Wölbung und Risse überzeugen, Beton passt zur Wand; die Ringfalten an jedem Glied lesen sich wie Rohrmuffen; der Arm ist zu gerade. | Ringfalten weg; ovaler Schaft, schräge Adern, Längsfalten, Knochenhöcker; der ausgestreckte Arm windet sich (Welle zur Hand). |
| 4 | `r4_durchbruch_flucht.jpg` (Wölbung, Durchbruch mit Brocken, Arm nah, fern den Gang hinab; Schaben und Flucht im Raum dahinter) | Der Durchbruch ist der Schreckmoment (Brocken fliegen, prallen auf, der vielgliedrige Arm schießt heraus); die Flucht auf langen Spinnenbeinen ist unheimlich. Feine Formen gehen in der Vereinfachung verloren (Bruchteile nahmen zu viele Dreiecke). | Stufe 0 von 70 000 auf 90 000 Dreiecke, Gewichte: Körper 1,3, Glieder 1,5, Risse 0,25, Beton 0,4, Loch 0,1. |
| 5 | `r5_stromausfall_lampe_zugriff.jpg` (Stromausfall mit Lampe: Schaben, Wölbung, Durchbruch, Arm; Zugriff aus der Ich-Sicht) | Im Lichtkegel funktioniert die Szene (Geräusch vor Sicht, dann die Wölbung im Lichtkreis). Der Zugriff ist nur Rot und Dunkel – die Hand ist kaum zu erkennen (wie bei den anderen Wesen wird der Schirm schwarz). | Unverändert (gleiches Schreckbild wie die übrigen Wesen); unter Restmängel. |
| 6 | `r6_koerper_vorher.jpg` (Halle: vorn, Kopf nah, hinten, Gang, Flucht) | Zwischen Brustkorb und Becken klafft eine Lücke (die Taille reichte nicht ins Becken), die Schultern sind Kugeln, der Kopf ein schwarzes Ei – Gliederpuppe statt Gerippe. Gang und Flucht sind gut. | Taille bis ins Becken, Wirbelsäule trägt sie; Kapuzenmuskel; Kopf als zugewachsener Schädel (Brauenwulst, tiefe Augenmulden, Wangenknochen, eingefallene Schläfen und Wangen, schmaler Kiefer – keine Öffnung). Haut etwas dunkler (0,5). |
| 7 | `r7_endstand.jpg` (Lampe nah, Halle mittel, Kopf, Wand mittel und schräg, Flucht) | Gerippe und Schädel ohne Gesicht wirken im Lampenlicht verstörend; die Rippen etwas zu regelmäßig; im vollen Lampenlicht bleich statt dunkelgrau (Belichtung). | Abgenommen; Restmängel unten. |

## Restmängel

- ~~Im nahen Lampenlicht wirkt die dunkelgraue Haut bleich~~ – behoben bei der Zusammenführung (Hauptstrang): Grundfarbe 0,50 → 0,34 (`FM_ENT_DULLERSKIN`); im Hallenlicht jetzt dunkelgrau wie im Wiki, Bild `docs/screens/creatures/duller/r8_hauptstrang_farbe.jpg`.
- Die Rippen sind zu gleichmäßig; Staub beim Durchbruch fehlt (es gibt keine Teilchen für Wesen).
- Auf der Seite des Dullers bekommt die Wand kein Loch (man sähe es nur dort, wo er steht – und dort flieht er).
- Diebstahl von Gegenständen nicht umgesetzt.

## Tests (`gameplay_test`, `testV133DullerArms`)

Level 1 (Gang mit Krankenzimmer dahinter): ein Duller steht hinter der Wand und schabt; er ist nicht zu sehen; zuerst nur Schaben (kein Zugriff); wer weggeht, entkommt; die Wand bricht auf, und wer bleibt, wird gepackt (tödlich); im Raum dahinter gesehen flieht er – lautlos und ohne anzugreifen; mit Mandelwasser in der Hand bricht er nicht durch; lauernd sucht er sich die Wand, hinter der der Spieler steht.
