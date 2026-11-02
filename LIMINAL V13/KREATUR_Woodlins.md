# Kreatur: Woodlins (verlinkt als Entity 45), Level 5

Quellen: [Level 5](https://backrooms-wiki.wikidot.com/level-5) Rev. 184 (07.08.2026; Abschnitt Entities: „Woodlins“, verlinkt auf
`entity-45`) und [Level 998](https://backrooms-wiki.wikidot.com/level-998) Rev. 179 (02.10.2026), beide Wikidot, **CC BY-SA 3.0**.
Eigene Worte. Im Spiel ohne Namen („Holz knarrt“, „Etwas beißt dir in den Knöchel“).

## Was die Seiten sagen – und was nicht

- **Eine eigene Seite gibt es nicht** (geprüft 27.10.2026, erneut für diesen Auftrag): `entity-45` ist heute „Chupavidas“ (neu angelegt
  am 08.09.2022, eine ältere Woodlins-Fassung steckt nicht in der Geschichte); nichts unter trimmed:, archived:, deleted:; kein
  Seitentitel mit „Woodlin“ unter rund 3185 Seiten; die Wiki-Suche findet nichts.
- **Level 5:** nur der Name, in der Liste der wenigen anderen Wesen neben den Deathmoths („andere Wesen eher selten“).
- **Level 998:** in den kalten Außenbereichen wurden Woodlins mit anderen „gewöhnlichen“ Wesen gesehen; dort verhalten sich alle
  untypisch – nicht offen feindselig, sie bemerken Wanderer kaum und ziehen fort. Daraus folgt nur: gewöhnlich bemerken sie Wanderer und
  verhalten sich anders – wie, sagt keine Seite.

## Umsetzung (alles Auslegung – Vorschlag fuer DECISIONS: Level-5-Wesen ohne eigene Seite)

**Gedanke:** Der Name („Wood-“) und das Hotel (Nussbaum, gedrechselte Möbel der 1920er, Klauen-und-Kugel-Füße) ergeben kleine Wesen aus
dunklem, gedrechseltem Holz, die in den Möbeln der Haupthalle hausen und reglos wie Schnitzwerk dasitzen, solange jemand hinsieht.
Das „gewöhnliche Verhalten“ (Level 998: das, was sie dort *nicht* tun): sie bemerken Wanderer und kommen näher.

**Ohne eigene Rolle** (Weltobjekt, `src/gameplay/hotel_l5.*`). **Orte:** vor jedem Sofa, Sessel, Regal und Beistelltisch der Gänge
und der Aufzugshalle (Spawnpunkte `trace`/`woodlin`, 15).

**Modell** (`wl_body`, `wl_jaw`, `wl_leg_a`, `wl_leg_b`; `models_l5_wesen.py`; im Spiel 1,45-fach, gut 0,55 m): Rumpf als gedrechselte
Spindel wie ein Treppendocke mit Ringen (abgegriffen, heller), Messingnagel im Scheitel; ein großer, geschnitzter Kopf mit Brauenwulst,
Nasengrat, Wangen und Astloch-Augen (nass glänzendes Schwarz darin); ein Riss klafft vom Scheitel über das Gesicht – rohes Holz an den
Rändern, **bleiches, nasses Fleisch darin**, dunkler Saft läuft heraus, Splitter stehen ab; der Unterkiefer grob geschnitzt mit einer
Reihe Holzsplitter als Zähne. Vier Glieder wie Stuhlbeine (gedrechselt, Kugelgelenke), unten **Klauen-und-Kugel-Füße**.

**Bewegung** (starre Kette in `hotel_l5.cpp`): reglos aufrecht wie eine Schnitzfigur (Hinterbeine gefaltet, Vorderbeine vor der Brust
wie gefaltete Hände); huschend kippt der Rumpf waagrecht, die Beine spreizen sich weit (diagonaler Takt) – ein Möbelstück, das wie eine
Spinne läuft; beim Biss stößt es den Kopf vor, der Kiefer klappt.

**Verhalten:**
- Eine Gruppe (3) zieht etwa alle 150 s in die Möbel eines Gangs, in dem ein Spieler ist (7–20 m, ungesehen).
- Wer hinsieht (Blickfeld, Licht oder nah), sieht nur Schnitzwerk – **reglos**. Ungesehen dreht es sich langsam zum Spieler (beim
  nächsten Blick sieht es einen an) und huscht nach 0,9–1,8 s ein Stück näher (höchstens 4 m je Satz) – hinter den Spieler. Wird es mitten
  im Lauf gesehen, erstarrt es auf dem Teppich. Man hört es vorher: Klacken von Holz auf Holz, eine rollende Kugel.
- Wer ihm lange den Rücken kehrt (ungesehen, näher als 1,4 m): **Ankündigung** – Holz knarrt dicht hinter dem Spieler (0,7 s,
  Meldung) –, dann ein **Biss in den Knöchel** (6 × Schwierigkeit, nie unter 10 Gesundheit; der Spieler ist 2,2 s langsamer). Umdrehen
  in der Ankündigung lässt es erstarren. Danach verschwinden alle im Holz; 2–3 Minuten Ruhe.
- Wer ganz nah herantritt und es ansieht (< 0,9 m), dem huscht es ins Möbel davon.

**Klang** (`synth_hotel.cpp`): Klacken gedrechselten Holzes (Knöchel auf Möbeln, eine Kugel rollt nach), Knarren eines trockenen Gelenks
(Ankündigung), Biss (Holz knackt, Splittern, ein nasser Laut).

**Mehrspieler:** Der Host entscheidet Auftreten, Sätze, Ankündigung und Biss und meldet jeden Zustand als Weltereignis (Kennzahl,
Zielpunkt); die Gäste bewegen die Figuren dorthin; Sehen zählt für alle Spieler (wer auch immer hinsieht, es erstarrt). Protokoll
unverändert.

**Skript:** `hotel woodlin <abstand> [winkel] [nr]`, `hotelforce woodlins <nr>`, `hotelstate hold|wood_t <wert>`.

## Zahlen

| Wert | |
|---|---|
| Gruppe | 3, alle ~150 s (0,6–1,4), 7–20 m, ungesehen |
| Sätze | nach 0,9–1,8 s ungesehen, ≤ 4 m, 1,9 m/s; Flucht 3,3 m/s |
| Biss | < 1,4 m und 0,5 s ungesehen; Knarren 0,7 s; 6 × Schwierigkeit, nie unter 10; 2,2 s langsamer |
| Gemessen (Test) | Rücken zu: 8 Sätze, Knarren nach 10,9 s, Biss nach 11,6 s |

## Wow-Check

„IIIH, was ist das für ein Vieh?“ – Neben dem Bücherregal steht eine geschnitzte Holzfigur, die du für Zierrat hältst; beim nächsten Blick
steht sie näher und sieht dich an, aus einem Riss in ihrem Gesicht quillt bleiches Fleisch – und als hinter dir Holz knarrt, ist es schon
zu spät. Ehrliche Antwort: das Unheimliche trägt das Verhalten (Statuen, die sich bewegen, wenn man wegsieht); das Modell allein liest
sich aus der Ferne als Holzmarionette – unheimlich, aber kein Schock (siehe Restmängel).

## Kritikrunden

Bilder: `docs/screens/creatures/woodlins/`.

**Runde 1** – `r1_wood_bogen.jpg`. *Schlecht:* nichts zu sehen – die erzwungene Gruppe saß an Möbeln außerhalb des Bildes.
*Änderung:* der erste genau am betrachteten Möbel (Skript).

**Runde 2** – `r2_wood_bogen.jpg`. *Schlecht:* 40 cm, dünn, dunkel – übersieht man; liest sich wie ein kleiner Roboter. *Änderung:*
1,45-fach (gut 0,55 m).

**Runde 3** – `r3_wood_bogen.jpg`. *Schlecht:* Spindelkörper und kleiner Kugelkopf – Marionette; das Gesicht mit dem Riss ist nicht zu
erkennen. *Änderung:* Kopf 1,4-fach (Riss nur noch im Gesicht, Fleisch und Saft größer), Kiefergelenk angepasst; ungesehen dreht es sich
zum Spieler.

**Runde 4** – `r4_wood_bogen.jpg`. *Schlecht:* immer noch von der Seite eine Holzfigur; Gesicht nur von vorn lesbar. *Änderung:* die
erzwungene sieht den Spieler an (Bilder); im Spiel dreht es sich ungesehen zu ihm.

**Runde 5 (Abnahme mit Restmangel)** – `r5_wood_bogen.jpg`; beste Ansichten `r5_wood_a_moebel.jpg`, `r5_wood_c_nah.jpg`. Ablauf
(erstarren, näherkommen, Knarren, Biss, Flucht ins Holz) im Test belegt; die Gestalt bleibt eher Marionette als Schock.

## Tests (`gameplay_test`, `testV133Level5Hotel`)

Orte in der Karte; Gruppe in den Möbeln; im Blick und hell 4 s reglos; mit dem Rücken zu ihnen kommen sie näher, vor dem Biss knarrt
Holz (≥ 0,6 s vorher); Biss leicht, nie tödlich; der Biss bremst.

## Restmängel

- Die Gestalt liest sich aus der Ferne als Holzmarionette; ein drastischerer Kopf (Fratze mit aufgerissenem Holz bis in die Brust,
  mehr Fleisch) wäre der nächste Schritt.
- Ohne eigene Seite ist alles Auslegung; sobald das Wiki eine Woodlins-Seite bekommt, muss das Verhalten danach geprüft werden.
