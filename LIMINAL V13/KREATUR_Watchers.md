# Kreatur: Watchers (verlinkt als Entity 38), Level 5

Quellen: [Level 5](https://backrooms-wiki.wikidot.com/level-5) Rev. 184 (07.08.2026; Abschnitt Entities: „Watchers“, verlinkt auf
`entity-38`; dazu die Erscheinungen „Gefühl, beobachtet zu werden“, „Türen öffnen und schließen sich von selbst“) und
[Level 43](https://backrooms-wiki.wikidot.com/level-43) Rev. 62 (17.09.2026), beide Wikidot, **CC BY-SA 3.0**. Eigene Worte. Im Spiel ohne
Namen („Du fühlst dich beobachtet.“, „Lange Finger reißen dir über den Rücken.“).

## Was die Seiten sagen – und was nicht

- **Eine eigene Seite gibt es nicht** (geprüft 27.10.2026, erneut für diesen Auftrag): `entity-38` ist heute „Humanoids“ (neu angelegt am
  27.06.2023, keine ältere Fassung in der Geschichte); nichts unter trimmed:, archived:, deleted:; kein Seitentitel mit „Watcher“; die
  Wiki-Suche findet nichts. „Watchers“ wird **nicht** mit den „Humanoids“ gleichgesetzt (kein Beleg).
- **Level 5:** nur der Name, unter den seltenen anderen Wesen.
- **Level 43:** Watchers stehen in einer Liste von Wesen der Personalflure: selten, aber sie handeln feindselig – Ausrüstung mitnehmen.
- **Level 5, Erscheinungen:** man fühlt sich beobachtet; Türen öffnen und schließen sich von selbst.

## Umsetzung (Auslegung – Vorschlag für DECISIONS: Level-5-Wesen ohne eigene Seite)

**Gedanke:** „Watcher“ – einer, der zusieht. Im Hotel: aus den Türöffnungen dunkler Zimmer. Selten, feindselig (Level 43), das Gefühl,
beobachtet zu werden (Level 5).

**Ohne eigene Rolle** (Weltobjekt, `src/gameplay/hotel_l5.*`). **Orte:** Türöffnungen dunkler und leerer Zimmer zum Gang (Spawnpunkte
`trace`/`watcher`, 31; je Tür eine Seite der Kante). Je Lauf drei, mindestens 12 m auseinander, nie an der Tür des Mottennests.

**Modell** (`models_l5_wesen.py`; Haut als Distanzfeld wie die Wesen mit Skelett, im Spiel mit dem festen Hautmaterial
`FM_ENT_ROTSKIN` – graugrün, wachsig –, der Leib mit `FM_ENT_DULLERSKIN`):
- `wa_head` – ein ausgezogenes, ausgezehrtes Menschengesicht (0,5 m mit Hals; im Spiel 1,2-fach): Stirn, Brauenwulst, Wangenknochen,
  eingefallene Wangen und Schläfen, langer Kiefer, angelegte Ohren, Falten und Poren; statt Nase und zwei Augen **fünf Höhlen untereinander
  in der Mitte des Gesichts**. `wa_face`: wunde, nasse Ränder der Höhlen, Schwärze darin, ein breiter, lippenloser Mund mit gelben,
  schiefen Zähnen, dünne nasse Strähnen.
- `wa_eye` – fünf Augen verschiedener Größe: gelblicher, nasser Augapfel mit roten Äderchen, graugrüne Iris, große Pupille; jedes folgt dem
  Spieler, sie blinzeln nacheinander (rollen nach oben).
- `wa_hand@0/@1` – Hände um die Kante der Öffnung: der Ballen liegt in der Laibung, vier lange, knotige Finger greifen um die Ecke und
  liegen flach auf der Gangwand, gelbe Nägel; eine Hand hoch neben dem Kopf, eine viel zu tief unten.
- `wa_body` – der ausgezehrte, viel zu lange Leib (Rippen unter der Haut, eingefallener Bauch, spitze Schultern), vorgebeugt im Dunkel der
  Öffnung; `wa_arm_a/b`, `wa_claw` – der lange Arm (0,55 + 0,62 m) mit gespreizter Hand beim Hieb.

**Verhalten:**
- **Lauern:** nur die Finger am Rahmen; der Leib ein Umriss im Dunkel.
- **Lugen:** sieht ein Spieler die Öffnung (≤ 14 m, freie Sicht) **nicht direkt** an, kommt der Kopf aus dem Dunkel an die Kante, schief
  gelegt, und dreht sich mit dem Blick zum Spieler; die Augen folgen ihm. Solange er lugt, zehrt es am Verstand (0,25/s, „Du fühlst dich
  beobachtet.“). Er ist größer als die Tür und duckt sich unter den Sturz.
- **Angesehen:** sieht der Spieler direkt hin (Licht oder nah) und hält den Blick 1,2 s, zieht sich der Kopf langsam zurück; nach 3 s ohne
  Blick lauert er wieder.
- **Hieb:** wer dicht vorbeigeht (< 1,6 m von der Tür) und ihm den Rücken zukehrt: **Ankündigung** – die Fingergelenke knacken
  nacheinander, die Finger greifen fester (0,8 s) –, dann schießt der lange Arm heraus (12 × Schwierigkeit, nie unter 10 Gesundheit).
  40 s Pause.
- **In die Tür treten:** er zieht sich zurück und lauert nach 30 s an einer anderen Tür.

**Klang** (`synth_hotel.cpp`): Knacken langer Fingergelenke in Folge (Ankündigung), Hieb (Luftzug, Stoff reißt, dumpfer Schlag).

**Mehrspieler:** Der Host entscheidet Lugen, Rückzug, Ankündigung, Hieb und Umzug und meldet jeden Zustand als Weltereignis; die Plätze je
Lauf sind beim Host und den Gästen gleich (Zufall aus dem Seed). Augen und Kopf folgen auf jedem Rechner dem nächsten Spieler. Protokoll
unverändert.

**Skript:** `hotel watcher <abstand> [winkel] [nr]`, `hotelforce watcher <nr>`, `hotelstate wa_peek|wa_arm|hold <wert>`.

## Zahlen

| Wert | |
|---|---|
| Je Lauf | 3 von 31 Türen, ≥ 12 m auseinander |
| Lugen | Spieler ≤ 14 m mit Sicht, schaut nicht direkt hin; Verstand 0,25/s bis 12 m |
| Rückzug | 1,2 s direkt angesehen |
| Hieb | < 1,6 m, Rücken zur Tür; Knacken 0,8 s; 12 × Schwierigkeit, nie unter 10; 40 s Pause |

## Wow-Check

„IIIH, was ist das für ein Vieh?“ – Aus dem Augenwinkel siehst du in einer dunklen Zimmertür lange, knotige Finger um den Rahmen und einen
Kopf, der sich um die Kante schiebt – ein ausgezehrtes Gesicht, in dem statt Nase und Augen fünf nasse Augen untereinander sitzen, die dir
alle folgen; siehst du hin, gleitet er langsam ins Dunkel zurück. Ehrliche Antwort: von vorn und aus dem Augenwinkel verstörend; von der
Seite liest sich der Kopf noch zu glatt (siehe Restmängel).

## Kritikrunden

Bilder: `docs/screens/creatures/watchers/`.

**Runde 1** – `r1_watch_bogen.jpg`. *Schlecht:* ein weißer, flacher Teller mit roten Punkten an der Wand, die Finger weiße Stummel, der
Arm weiße Stäbe – Spielzeug. *Änderung:* Leib im Dunkel der Öffnung, Kopf 1,2-fach, dunklere Haut, dickere Finger, der Kopf steht in
der Öffnung statt hinter der Wand.

**Runde 2** – `r2_watch_bogen.jpg`. *Schlecht:* Kopf wie eine Pille mit Ringen, Leib wie eine Fischgräte (weiße Rippen) – immer noch
Spielzeug. *Änderung:* Kopf, Hände, Leib und Arme als Distanzfeld (eingefallene Wangen, Knochenkanten, Falten, Poren; Rippen unter der
Haut).

**Runde 3** – `r3_watch_bogen.jpg`. *Schlecht:* immer noch eine glatte, helle Form ohne Gesicht – im Lampenlicht weiß. *Änderung:*
ausgezehrtes Menschengesicht mit Stirn, Brauenwulst, Wangenknochen, Kiefer und lippenlosem Mund mit Zähnen; dunklere Haut.

**Runde 4** – `r4_watch_bogen.jpg`. *Schlecht:* Haut im Lampenlicht noch hell, der Kopf liegt fast waagrecht und zeigt die Seite.
*Änderung:* Haut mit dem festen, graugrünen Hautmaterial der Wesen (Kopf/Hände/Arme) und dem dunklen für den Leib (eigenes Modell
`wa_face` für Ränder, Zähne, Haar); der Kopf dreht sich mit dem Blick zum Spieler, weniger schief.

**Runde 5 (Abnahme mit Restmangel)** – `r5_watch_bogen.jpg`; beste Ansichten `r5_watch_b_lampe_mittel.jpg`, `r5_watch_c_nah.jpg`,
`r5_watch_d_hieb.jpg`. Finger, Leib und Augenreihe lesen sich als Wesen; der Hieb mit dem langen Arm wirkt. Von der Seite bleibt der Kopf
eine lange, glatte Form.

## Tests (`gameplay_test`, `testV133Level5Hotel`)

Orte in der Karte; Watchers gesetzt; Hieb erst nach dem Knacken der Finger, Schaden, nie tödlich.

## Abnahme Level 5 (03.11.2026, `KRITIK_Level5.md` Runden 2–5)

Bilder: `docs/screens/level5/wesen_r2_*` … `wesen_r5_*` (Einzelbilder `build/qa/creatures/level5/rN/watcher_*`).
- **Runde 2**: aus dem Gang nur ein schmaler, heller Streifen mit fünf Punkten an der Laibung (Stab, Thermometer).
  *Änderung:* Lider wulsten um jede Höhle (im Profil eine Reihe Knoten), Unterkiefer vor, Wangenknochen spitz, lange Strähnen;
  beim Lugen kommt der Kopf bis knapp vor die Wandflucht und dreht das Gesicht bis 72° zum Spieler (`hotel_l5.cpp`).
- **Runde 3**: schief und weiter vorn, aber zu schmal (Flöte, Fernbedienung). *Änderung:* Kopf 1,3-fach breiter (`WA_WIDE`), Mund
  offen mit 18 längeren Zähnen, ein Vorhang aus 48 nassen Strähnen.
- **Runde 4**: nah ein Gesicht, der kahle Schädel aber wie eine Erdnuss. *Änderung:* nasses Haar über Scheitel und Hinterkopf,
  zehn Strähnen über den Wangen.
- **Runde 5**: ein langes, graues Menschengesicht mit nassem Haar, fünf Augen in wunden Wülsten und einem breiten Grinsen lugt
  schief um die Türkante. Abgenommen.

## Restmängel

- Im Schrägblick aus mehr als 6 m bleibt der Kopf ein heller Fleck an der Laibung; das Gesicht liest sich ab etwa 4 m.
- Die Tür selbst bewegt sich nicht (die Karte hat keine Türblätter in den Öffnungen); „Türen öffnen und schließen sich“ bleibt ein
  Bild der Ebene, nicht des Watchers.
- Ohne eigene Seite ist alles Auslegung; bei einer neuen Wiki-Seite neu prüfen.
