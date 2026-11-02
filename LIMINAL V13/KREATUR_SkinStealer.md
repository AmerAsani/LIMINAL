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

## V13.3: Ansprechen (Level 1)

**Quellen** (Wikidot, CC BY-SA 3.0, eigene Worte, keine Bilder): [level-1](https://backrooms-wiki.wikidot.com/level-1) Rev. 83 (07.10.2026, Praetor3005 und DivineAtlas), Wesenliste und Erzählabschnitt; [entity-10](https://backrooms-wiki.wikidot.com/entity-10) Rev. 61 (09.03.2025).

**Was die Seiten sagen:** Weiße Humanoide in der Haut ihrer Opfer; sie greifen meist erst nach einer ausreichend langen Interaktion an. Rat (Level 1): mit ihnen reden – sie sind wenig intelligent; ist klar, dass es kein Mensch ist, ruhig zurückweichen. Entity 10: sie sprechen nach, was sie gehört haben, verstehen aber nichts; wortkarge oder wirr redende „Menschen“ sind verdächtig. In der Erzählung von Level 1 antwortet die Gestalt knapp und mit einer sinnlos passenden Antwort, grüßt – und greift an.

**Umsetzung** (`gameplay/presence_lure.cpp`, `talkSkinStealer`; `gameplay/session_lure.cpp`)
- **Ansprechen:** die Interaktionstaste (**E**, in den Einstellungen belegbar), wenn kein Gegenstand im Blick ist und man eine menschenähnliche Gestalt (Skin-Stealer, Faceling, Wretch) in höchstens 6 m ansieht (Kopf oder Brust höchstens etwa 18° neben der Blickmitte, freie Sicht). Das HUD zeigt dann „E Ansprechen“ – für alle drei, damit der Hinweis nichts verrät. Der Spieler ruft einen von vier kurzen Sätzen („Hallo?“, „Ist da wer?“, „Hey … hallo?“, „Wer bist du?“; Untertitel), frühestens alle 2,4 s. Facelings und Wretches antworten nicht.
- **Die Antwort:** Er hält inne, sieht her und legt den Kopf schief (lauscht). Nach 1,1–2,1 s – zu spät – kommt der Satz zurück, auf eine von drei Arten: **falsch betont** (erste Silbe laut und hoch, die letzte fällt flach ab statt zu steigen, die erste zu lang, die letzte gekappt; zu glatt, ohne Atem; eine zweite, tiefere Stimme schwingt 30 ms später mit; dann die letzte Silbe noch einmal, zu hoch), **abgehackt** (Silben hart gekappt, die erste gestottert, Pausen am falschen Ort, flacher Ton mit einem Sprung nach oben) oder **verzögert** (gedehnt, die Stimme sinkt wie ein langsamer werdendes Band, leicht rau; danach ein feuchtes Schnalzen und Einatmen durch die Zähne). Synthetisch aus Lauten (Quelle-Filter-Stimme, Formanten je Vokal), keine Aufnahmen: `talk_call_0..3`, `skin_reply_0..11` (Satz × Art), `skin_shriek` in `audio/synth_lure.cpp`. Beim ersten Mal ein Hinweis: „Das ist kein Mensch. Ruhig zurückweichen – nicht rennen, nicht noch einmal ansprechen.“
- **Entlarvt** (Zustand 6): Er steht, sieht her, der Kopf kippt weit zur Seite (Kanal `tilt`), der Mund arbeitet nach (`talk`), er wird unruhiger (`alert`).
  - **Ruhig zurückweichen** (gehen oder schleichen) um 3,5 m über den Abstand bei der Antwort (oder auf über 11 m): er lässt ziehen – satt wandert er weiter, hungrig zieht er ab; 60 s Ruhe.
  - **Angriff** (mit Schrei, `skin_shriek` – die Tarnung fällt): Rennen in 14 m, näher als 1,6 m, länger als 9 s stehen bleiben (zu lange Interaktion), erneutes Ansprechen.
  - Rennen schon während der Pause vor der Antwort (in 12 m) löst den Angriff ebenso aus.
- **Lange Interaktion ohne Ansprechen:** Steht man insgesamt 26 s nah (unter 5 m, mit Sicht) bei einem satten Skin-Stealer, spricht er von selbst (nachgeahmte Silben) und steht danach wie entlarvt da – Zurückweichen hilft, Stehenbleiben nicht.
- **Bestehendes bleibt:** wahre Gestalt und Tarnung, Satt/Hungrig, Reizung durch die Lampe, Innehalten als Ankündigung, Zurückweichen im Innehalten.
- **Mehrspieler:** Der Gast spielt Ruf und Untertitel selbst und schickt nur die Eingabe „Interact“ mit der Nummer des Satzes (0–3 statt einer Kennzahl); der Host prüft, wen die Figur des Gasts ansieht, rechnet Antwort und Folgen. Antwort und Schrei hören alle in der Nähe (Ereignisse `Reply`, `Shriek`), den Ruf eines Spielers hören die anderen über das vorhandene Klang-Ereignis. Protokoll unverändert.

**Interpretation:** die drei Arten der verräterischen Antwort, die Zeiten und Abstände (Antwortpause, 3,5 m Zurückweichen, 9 s, 1,6 m, 26 s); dass auch Facelings und Wretches den Hinweis zeigen (fair: der Hinweis verrät nichts).

**Zahlen**

| Wert | Zahl |
|---|---|
| Ansprechen | bis 6 m (Hinweis), Blick ±18°, alle 2,4 s |
| Antwort | nach 1,1–2,1 s, 1–2,5 s lang |
| Ausweg | ruhig 3,5 m weiter zurück als bei der Antwort oder über 11 m |
| Angriff | Rennen < 14 m, Abstand < 1,6 m, > 9 s stehen, erneutes Ansprechen; ohne Ansprechen nach 26 s Nähe erst die Selbstentlarvung |

**Wow-Check:** Man ruft „Hallo?“ in den dunklen Gang, und nach einer Pause, die eine Spur zu lang ist, kommt dasselbe Wort zurück – zu glatt, falsch betont, aus zwei Kehlen –, während der Kopf zur Seite kippt, bis der Hals gebrochen aussieht. Ehrlich: Wow für Ton und Kopf; unter hartem Licht wirkt die Tarnhaut (Modell aus V13.2) wie eine Schaufensterpuppe.

### Kritikrunden (Ansprechen)

Bilder: `docs/screens/creatures/skinstealer_ansprechen/` (je Runde vorher, Ruf, Antwort, entlarvt nah/mittel; Spektrogramm der Stimmen).

| Runde | Befund | Änderung |
|---|---|---|
| 1 (`r1_antwort.jpg`) | Im dunklen Gang nicht zu beurteilen: Kopf und Mund verschwinden im Schwarz. Hinweis „E Ansprechen“ und Untertitel „Hallo?“ lesbar. | Fülllicht für die Bilder (`testlight eye`, zählt nicht als Lampe – sonst reizt sie ihn). |
| 2–3 (`r3_antwort.jpg`) | Kein Licht: erst hinter dem Kopf (dessen Schatten), dann gesetzt, bevor der Körper nach dem Versetzen nachgezogen war. In voller Größe sieht man die Kopfneigung schon im Dunkeln. | Licht vor den Kopf, nach `near` warten; Hinweis nach der ersten Antwort ergänzt. |
| 4 (`r4_entlarvt_nah.jpg`) | Entlarvt kippt der Kopf so weit, dass der Hals gebrochen wirkt – das trägt. Vorher und Ruf noch dunkel. | – (Kopfneigung bleibt). |
| 5 (`r5_vorher.jpg` … `r5_entlarvt_mittel.jpg`) | Ganze Folge lesbar: aufrecht – lauscht (Kopf beginnt zu kippen) – antwortet – steht schief da. Unter hartem Licht wirkt die Tarnhaut wie eine Puppe (Modell V13.2, nicht Teil dieser Aufgabe). | – |
| 6 (`r6_stimmen_spektrogramm.jpg`) | Ton: erste Fassung – Atemrauschen deckte die Stimme zu (Grundton nicht messbar), Zischlaute 4–5 dB lauter als die Vokale, Schwerpunkt bis 7,5 kHz. | Atem 0,16 → 0,035, Reibe- und Verschlusslaute auf Sprachniveau. Jetzt: Ruf steigt 120 → 175 Hz (Frage), Antwort 0 fällt 205 → 100 Hz und wiederholt die Silbe zu hoch, Antwort 1 zerhackt mit Tonsprung; Schwerpunkt 1,6–4,2 kHz. |
| 9 (`r9_entlarvt_nah.jpg`) | Endstand wie Runde 5. Ein Bild vom Angriff nach dem Rennen gelang im Skript nicht (Halten der Tasten bewegte den Spieler nicht) – das Verhalten belegt der Test. | – |

### Tests (`gameplay_test`, „V13.3: Leuchtstab und Ansprechen“)
Level 1: Hinweis im HUD beim Ansehen, getarnt; E ruft (ein Satz), er antwortet verräterisch und ist entlarvt (Zustand 6), die Antwort allein ist kein Angriff; wer rückwärts gehend zurückweicht, den lässt er ziehen (kein Schrei, kein Tod, auch 6 s danach); hungrig und entlarvt: wer umdreht und rennt, löst Schrei und Angriff aus und stirbt; erneutes Ansprechen nach der Entlarvung löst den Angriff aus.

## V13.3: Level 3 – getarnt im Mantel, einfache Sätze, Verwandlung

**Quellen** (Wikidot, CC BY-SA 3.0, eigene Worte, keine Bilder, keine Zitate):
- [level-3](https://backrooms-wiki.wikidot.com/level-3), Rev. 18 (26.09.2026; u/M654z, Neufassung Natedagreat563): Abschnitt der klugen Wesen/Skin-Stealers, Base Gamma und Vorfall GB-02.
- [entity-10](https://backrooms-wiki.wikidot.com/entity-10), Rev. 61 (09.03.2025; ursprünglich AndromedaMapping1, Neufassung 1000dumplings).
- [level-1](https://backrooms-wiki.wikidot.com/level-1), Rev. 83 (07.10.2026; Praetor3005 und DivineAtlas): Rat und Erzählung (die lose hängende Haut).

**Was die Seiten sagen:**
- Level 3: Die getragene Haut wird dort langsamer verdaut – die Tarnung hält **Tage** statt etwa eines Tages –, beginnt aber zu **faulen**. Sie wissen das und verhüllen sich mit viel Kleidung (Mäntel, Kapuzen, Masken, Sonnenbrillen); ein Überwachungsbild zeigt einen dünnen, vermummten Mann. Sie sprechen **Englisch etwa auf A2-Niveau**, können eigene Gedanken einfach ausdrücken und täuschen oft einen starken **Akzent** vor. Sie dienen Brigaden als Lockvögel; einer schlich sich in fremder Haut in Base Gamma ein (GB-02). Sicherster Test: das Blut (durchsichtig).
- Entity 10: Die Noppen halten die Haut fest und heilen Schnitte; satt friedlich, hungrig suchen sie Einzelne; sie sprechen selten und verstehen wenig; Rat: Leuten misstrauen, die kaum sprechen oder Wörter wiederholen; ruhig weggehen.
- Level 1: Sie greifen meist erst nach längerer Interaktion an; in der Erzählung hängt die Haut lose, als die Gestalt sich umdreht, und rutscht ab, als sie getroffen wird.
- **Nicht gesagt:** wie sie sich verwandeln; Licht und Lärm.

**Umsetzung**
- **Modelle** (gleiches Skelett, 2,05 m):
  - `ent_skinstealer_coat` (`skinstealer_coat.py`, Variante 2): der große, hagere Mann aus V13.2 in einem langen, dunklen Mantel (Schoß bis eine Handbreit über das Knie, vorn offen, Gehschlitz, Knopfleiste, Taschenklappen an der Hüfte, weite Stulpen), hochgeschlagener Kragen, Kapuze mit Schild über der Stirn, grauer Schal über Mund und Nase (am Kiefer – er bewegt sich, wenn er spricht), runde Sonnenbrille, dunkle Hose, Stiefel. Was von der Haut zu sehen ist – Wangenknochen und Schläfen um die Brille, Handrücken, Handgelenke –, ist fleckig grau-grün, rissig (faulend).
  - `ent_skinstealer_torn` (`skinstealer_torn.py`, Variante 3): die wahre Gestalt (V13.2: blassgelb, Saugnoppen, eingesunkene weiße Augen); die Menschenhaut ist am Rücken aufgeplatzt (aufgerollte Hautkanten entlang des Risses) und **bis zur Hüfte heruntergerutscht** – dort ein faltiger Wulst, aus dem **die leeren Arme der Haut** schlaff bis zu den Knien hängen; **das Gesicht des Opfers hängt kopfüber als leere Maske auf der Brust** (Augenlöcher, Nase, Lippen, Ohren, das kurze Haar hängt nach unten), am Kinn noch mit dem Hals verbunden; rohes Rot an allen Risskanten; Mantelfetzen hinten; **klare, nasse Tropfen** (durchsichtiges Blut) an Hals, Maske und Wulst.
- **Animation** (`anim/creature_kin_l3.cpp`): im Mantel ruhig die **Hände in den Manteltaschen** (keine Armbewegung); Lampe im Gesicht – die rechte Hand vor die Brille, der Kopf wendet sich ab; Verwandlung (`shiver`): er krümmt sich, **krallt beide Hände ins eigene Gesicht**, die Ellbogen weit, der Kopf ruckt, der Kiefer reißt weit auf; nach dem Reißen entfalten sich die Arme über den Kopf; wahre Gestalt wachsam: geduckt, die langen Hände vor dem Bauch (die Maske bleibt sichtbar).
- **Verhalten** (`gameplay/presence_kin_l3.cpp`, `thinkSkinL3`; Level-JSON `presence.skinstealer.mode = "coat"`; Zustände 0 geht umher, 7 begleitet – Phasen begleitet/lauscht/verwandelt sich –, 6 entlarvt und satt, 2 Angriff, 4 zieht ab):
  - **Er schließt sich an:** sieht er einen (bis 18 m), kommt er und **bleibt in der Nähe** – 3,2–5,5 m Abstand, nie rennend (1,5 m/s, weit weg oder wenn man rennt 2,6 m/s); wer ihm zu nahe kommt, vor dem tritt er höflich zurück. Rennt man weg, ruft er „Wait! Where you go?“.
  - **Er spricht:** zuerst ein Gruß, dann alle 9–15 s ein einfacher, leicht falscher Satz (8 Sätze, z. B. „Is safe here. You stay with me.“, „My name is Daniel. Is my name.“, „You have water? I am very thirst.“).
  - **Ansprechen** (dieselbe Taste E wie in Level 1, Level 1 unverändert): er hält inne, legt den Kopf schief und antwortet nach 1,0–1,6 s mit einem Satz („I come from camp. Is far.“ – ohne Ortsnamen des Wikis, `LICENSE_REVIEW.md` …). **Spricht man ihn innerhalb von 25 s ein zweites Mal an, kommt genau derselbe Satz mit genau derselben Melodie** (der Kopf zu schief) – **beim dritten Mal antwortet er nicht mehr: entlarvt**.
  - **Licht:** die Lampe im Gesicht (bis 8 m) – er wendet sich ab, die Hand vor der Brille; zusammen 2,4 s, und man sieht die faulende Haut: entlarvt.
  - **Lange Zeit:** hungrig hält er die Tarnung 95–130 s Begleitung, dann fällt sie von selbst; satt verabschiedet er sich nach 70–100 s („I go now. Goodbye, friend.“) und geht. Ist ein Mitspieler dabei (näher als 6 m), wartet er mit der Verwandlung (er sucht Einzelne).
  - **Verwandlung** (die Ankündigung, 2,4 s): Krümmen und Zucken, Hände ins Gesicht, Knacken der Gelenke; bei 1,5 s reißen Haut und Mantel (Modellwechsel, Schrei), er richtet sich auf. Danach: **hungrig Angriff** (4 m/s, tödlich unter 1,2 m – der Spieler sprintet 5,7 m/s); **satt** steht er in wahrer Gestalt da und sieht her (Zustand 6) – wer **ruhig 3,5 m zurückweicht**, den lässt er ziehen; Rennen, zu nah, zu lange stehen oder erneutes Ansprechen lösen den Angriff aus (wie in Level 1).
  - Verliert er einen (14 s ohne Sicht und über 18 m), zieht er ab.
- **Klang** (`audio/synth_kin_l3.cpp`, Formant-Synthese, keine Aufnahmen): `skin_say_0..7`, `skin_ans_0..3`, `skin_wait`, `skin_bye` – englische Laute mit **vorgetäuschtem Akzent** (gerolltes R, V statt W, Z/D statt TH, stimmloser Auslaut, Betonung auf der falschen Silbe: „go-ING“, „hu-MAN“, „Da-ni-EL“), zu gleichmäßig (Silbendauern angeglichen), tief und ohne Atem, großer Vokaltrakt (Formanten −7 %); darunter, kaum hörbar, eine zweite Kehle eine Oktave tiefer, 25 ms später; am Satzende ein feuchtes Schnalzen. `skin_tear`: Gelenke knacken, Haut dehnt sich nass, bei 1,45 s reißen Stoff und Haut, Knochen springen, es tropft. Eigenes Bewegungsgeräusch im Mantel: `skin_coat` (der schwere Stoff schwingt, das Stiefelleder knarrt; beim Gehen alle 1,0–1,6 s, bis 14 m). Untertitel mit dem englischen Satz; beim ersten Satz der Hinweis „Ein Fremder im Mantel spricht. Hier tarnen sich Skin-Stealer als Menschen – auf Wiederholungen achten, Abstand halten.“
- **Mehrspieler:** der Host rechnet (Ansprechen des Gasts kommt als Eingabe „Interact“ wie in Level 1); die Puppe beim Gast bekommt Variante und Kanäle (`talk`, `tilt`, `light`, `shiver`, `jaw`, `crouch`, `alert`, `reach`); **die Verwandlung tauscht beim Gast nur das Modell** (keine Neuanlage der Puppe). Die Sätze gehen als vorhandenes Klang-Ereignis mit Namen an alle Gäste – auch die genaue Wiederholung; Untertitel nach dem Namen. Protokoll unverändert.

**Interpretation:** Mantel, Schal und Brille als Auswahl aus der Liste der Seite; die Sätze und Fehler; dass das zweite Ansprechen die genaue Wiederholung bringt und das dritte entlarvt (aus „Wörter wiederholen“, „längere Interaktion“ und der klügeren Level-3-Form abgeleitet); die Lampe als Entlarvung (faulende Haut); die Zeiten (Tage → 95–130 s Begleitung); die Gestalt der Verwandlung (Haut rutscht ab, Gesicht als Maske – aus der Level-1-Erzählung abgeleitet); der Name „Daniel“.

| Wert | Zahl |
|---|---|
| Auftritt Level 3 | ~280 s (unverändert), 30 % hungrig |
| Begleiten | 3,2–5,5 m; 1,5 / 2,6 m/s; Satz alle 9–15 s (bis 12 m, in Sicht) |
| Ansprechen | Antwort nach 1,0–1,6 s; 2. Mal in 25 s: gleicher Satz; 3. Mal: Verwandlung |
| Licht | 2,4 s im Gesicht (bis 8 m; klingt mit 0,5/s ab) |
| Tarnung | hungrig 95–130 s, satt 70–100 s (dann Abschied) |
| Verwandlung | 2,4 s (Riss bei 1,5 s); Angriff 4 m/s, tödlich < 1,2 m; satt: 3,5 m ruhig zurück |

**Wow-Check:** Ein sehr großer Mann im Kapuzenmantel geht seit zwei Minuten neben dir her, die Hände in den Taschen, und sagt mit zu ruhiger Stimme „Is safe here. You stay with me.“ – fragt man ein zweites Mal, kommt genau derselbe Satz mit genau derselben Melodie; beim dritten Mal krallt er sich ins eigene Gesicht, und als er sich aufrichtet, hängt das Gesicht des Mannes kopfüber als leere Maske auf seiner Brust. Ehrlich: Wow für Begleiten, Wiederholung und Maske; die synthetische Stimme ist hörbar künstlich (gewollt zu glatt) und ohne Untertitel nur teilweise verständlich.

### Kritikrunden (Level 3)

Bilder: `docs/screens/creatures/l3_skinstealer/`.

| Runde | Befund | Änderung |
|---|---|---|
| 1 Blender (`r1_blender.jpg`, Zeilen 2–3) | Mantel wie ein aufgeblasener Anzug: zwei Wurst-Schöße um die Oberschenkel, Kapuze wie ein Taucherhelm. Zerrissen: ein Ring um die Hüfte mit runden Löchern (wie Augen), zwei steife Platten am Rücken wie Pappe, die Maske am Hals unkenntlich. | Neu gebaut: Schoß als ein weiter Rock am Becken; Zerrissen: Hautwulst um die Hüfte mit den leeren Armen der Haut, Maske groß mit Brauen, Nasenflügeln, Lippen, Ohren, Haar. |
| 2 Blender (`r2_blender_a.jpg`, `r2_blender_b.jpg`) | Mantel lesbar, aber zwischen Rumpf und Schoß eine offene Kante; Arme und Rumpf prall (Muskeln der wahren Gestalt unter dem Stoff). Die Haarkappe über der Maske als schwarze Scheibe. | Mantel über einen schlankeren Körper geformt, Schoß ab der Taille; Haar als Strähnen. |
| 3 Verwandlung live (`r3_verwandlung_live_tpose.jpg`) | Das Verhalten stimmt (Anschließen, Antwort, genaue Wiederholung, Verwandlung, Angriff), aber im Zucken spreizte er die Arme seitlich – wie die Ruhepose („T-Pose“); die Hände standen sichtbar vor dem Mantel. | Verwandlung: Hände ins eigene Gesicht, gekrümmt, dann über den Kopf entfalten; Taschen-Haltung an die Klappe der Tasche. |
| 4 Nah (`r4_posen.jpg`, `r4_nah_maske_taschen.jpg`) | Hände verschwinden in den Taschen, Hand vor der Brille im Licht, Hände im Gesicht – gut. Aber: die Knie stachen hellgrau durch den Schoß, ein heller Dreiecks-Revers wirkte fremd; die Maske (aufrecht) stand wie eine Säule, die rohen Ränder als Perlen wirkten wie ein Nietengürtel. | Schoß vorn weiter, Hose dunkel, Revers entfernt; **Maske kopfüber** (am Kinn hängend, Haar unten), rohe Ränder als durchgehende Wülste, Wulst mit Falten und Beulen, Hände der wachsamen Haltung tiefer. |
| 5 Endstand (`r5_mantel_taschen.jpg`, `r5_mantel_geht_dunkel.jpg`, `r5_antwort_1.jpg`, `r5_antwort_2_gleich.jpg`, `r5_verwandlung_gesicht.jpg`, `r5_verwandlung_entfaltet.jpg`, `r5_maske_kopfueber.jpg`, `r5_angriff.jpg`; Übersichten `r5_posen.jpg`, `r5_uebersicht_live.jpg`, `r5_untertitel_und_entfaltet.jpg`) | Im Dunkeln eine schwarze, sehr große Gestalt mit Kapuze – unheimlich, weil sie nichts tut; Untertitel und Hinweis lesbar; die Wiederholung desselben Satzes trägt; Hände ins Gesicht, dann die kopfüber hängende Maske – das ist der Moment. | – |
| Klang (`klang_r1_spektrogramm.jpg` → `klang_r2_spektrogramm.jpg` → `klang_r5_spektrogramm.jpg`) | Erste Fassung: Stimme dumpf (Schwerpunkt 600–1000 Hz), oberhalb 3,5 kHz nichts – wie durch ein Rohr. | Weniger Neigung, Bandbreiten, höhere Formanten parallel, Präsenz-Anhebung; jetzt Energie bis 8 kHz, Schwerpunkt 0,8–1,8 kHz, Grundton 100–130 Hz flach (gewollt), „Wait!“ höher und lauter. |

### Tests (`gameplay_test`, „V13.3: Level 3 – Wretch mit Waffe, Skin-Stealer im Mantel“)
Im Mantel (Variante 2); schließt sich an; bleibt 20 s in der Nähe, hält Abstand, greift nicht an; spricht Sätze; beim Gehen schwingt der Mantel (eigenes Geräusch); angesprochen antwortet er (noch getarnt); zweites Ansprechen: genau derselbe Satz; drittes: Verwandlung beginnt (Ankündigung, noch kein Angriff); Haut und Mantel reißen (Variante 3, Schrei); hungrig tötet er, wer stehen bleibt; die Lampe zu lange im Gesicht entlarvt; satt steht er in wahrer Gestalt, wer ruhig zurückweicht, den lässt er ziehen; hungrig hält er die Tarnung über 85 s und verwandelt sich dann; satt verabschiedet er sich nach über 60 s und geht. Das bisherige Grundverhalten (getarnt, hungrig: folgt, hält inne, tötet; Zurückweichen im Innehalten) wird jetzt in Level 1 geprüft; Ansprechen in Level 1 unverändert (eigene Prüfungen).
