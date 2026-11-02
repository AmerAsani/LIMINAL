# Kreatur: Der Gastgeber (Entity 18), Level 5

Quellen: [Entity 18 – „The Beast of Level 5“](https://backrooms-wiki.wikidot.com/entity-18) Rev. 54 (13.08.2026, Autor ratscrapz, Idee
eines unbekannten Fandom-Autors) und [Level 5](https://backrooms-wiki.wikidot.com/level-5) Rev. 184 (07.08.2026; der Schlussabsatz in
seiner Stimme, von ratscrapz), beide Wikidot, **CC BY-SA 3.0**. Gelesen 02.11.2026. Eigene Worte, keine Zitate, keine Bilder. Im Spiel
heißt er nicht – „eine ruhige Männerstimme“; der Wiki-Name „Beast“ kommt nicht vor (E-039).

## Was die Seiten sagen

- **Entity 18:** Die Seite ist zweigeteilt: zuerst spricht das Wesen selbst zum Leser, dann warnt ein früheres Opfer, das einen Handel
  mit ihm schloss. Es gibt sich als höflicher, beruhigender Gastgeber und Geschäftspartner, nennt das Hotel sein Reich und mag es nicht,
  „das Biest“ genannt zu werden. Es behauptet, alles zu sehen, was Besucher tun; es riecht Angst und nutzt sie. Es erscheint nur denen,
  die es wählt – wer es bemerkt, den hat es schon gefunden. Es benutzt die Bildnisse und Muster der Wände („Lügen“).
- **Das Angebot:** Der Besucher werde erwartet; Hilfe gegen „ein paar Dinge“, ein Geschäft unter vier Augen; man werde nie wieder Angst
  haben; man solle ihm die Hand geben und nicht mehr zweifeln.
- **Die Warnung:** Wer bleibt und den Handel schließt, kommt nicht mehr los und zerbricht; man fühlt sich unfähig, den Handel zu
  beenden. Rat: die Ebene sofort verlassen, der Stimme nicht zuhören, die Bildnisse an den Wänden nicht ansehen.
- **Aussehen:** nicht im Text beschrieben (nur Bildnachweise).
- **Level 5:** der Schlussabsatz – eine Einladung in sein Büro zum Plaudern, man müsse nicht weitergehen. Wo das Büro liegt: Seite sagt
  nichts.

## Umsetzung

**Ohne eigene Rolle** (Weltobjekt, `src/gameplay/hotel_l5.*`, `session_hotel.cpp`): Stimme, Bildnisse, Büro, Hand.

**Das Büro** (Karte, `tools/levels/level5.py`, eigener Zufall am Ende): ein leeres Zimmer am Hauptgang, weit vom Start; darin ein
Schreibtisch aus Mahagoni zur Tür (grüne Lederauflage, Bankerlampe mit grünem Glasschirm, ein Vertrag mit rotem Siegel und Füllfeder,
Tintenfass, Kerzenständer-Telefon, Kontobuch; `e18_desk`), dahinter ein hoher Ohrensessel aus Ochsenblutleder, **von der Tür abgewandt**
(`e18_wingchair`), an der Rückwand ein Bildnis eines Mannes im Anzug, dessen Gesicht bis auf die Leinwand abgekratzt ist (`e18_portrait`).
Spawnpunkt `trace`/`office`; Halt-Regeln in `mapkit.py` und `gameplay_test` (mountOf).

**Ablauf** (je Besuch der Ebene einmal):
1. **Er wählt** – frühestens nach 260 s auf der Ebene den Spieler mit der meisten Angst (kleinster Verstand unter 70; nach der doppelten
   Zeit auch ohne), der in einem Gang höchstens 45 m vom Büro steht. Nur dieser hört und sieht ihn.
2. **Einladung** – die drei nächsten Bildnisse bekommen nasse, echte Augen, die dem Spieler folgen (`e18_eye`); die Stimme spricht aus dem
   nächsten Bildnis (Untertitel, eigene Worte): er habe ihn erwartet; sein Hotel mache ihm Angst, das müsse nicht sein; er solle in sein
   Büro kommen, er müsse nicht weitergehen. Im Büro geht die Bankerlampe an (grün-warmes Licht in der dunklen Tür).
   Wer die Bildnisse ansieht, dem zehrt es am Verstand (1/s), und die Stimme sagt, die Bilder lögen.
3. **Im Büro** (≤ 2,4 m vor dem Schreibtisch) – „Setzen Sie sich …“; aus dem abgewandten Sessel hebt sich ein Arm im Nadelstreifenärmel über
   die Lehne, viel zu lang (drei Glieder, zwei Ellbogen), legt sich über den Tisch, und eine wachsbleiche Hand mit zu langen Fingern und
   Siegelring hebt sich zum Handschlag; „Geben Sie mir die Hand …“. Benutzen: **„Die Hand nehmen“**.
4. **Der Handel** – „Abgemacht.“ Der Verstand ist voll, und 180 s lang sinkt er nicht (nie wieder Angst); dafür nimmt er **ein paar Dinge**
   (zwei der wertvollsten Gegenstände, nie Sicherungen oder das Beil). Die Bildnisse sehen weiter zu.
5. **Die Rechnung** – nach 180 s, ganz nah am Ohr: „Wir sind noch nicht fertig miteinander.“ Der Verstand fällt auf höchstens 20 – der Handel
   lässt einen nicht los.
6. **Ablehnen** – wer nicht kommt (90 s), das Büro verlässt (6 s) oder sich 50 m entfernt: „Sie gehen? Wie schade …“, −8 Verstand, die Lampe
   erlischt, er kommt nicht wieder.

**Klang** (`synth_hotel.cpp`): eine ruhige, warme Männerstimme (f0 um 100 Hz), nah und trocken, sanft fallende Satzmelodie; dahinter ganz
leise ein Grammophon (Knistern, gedämpfte Töne); die Rechnung geflüstert.

**Mehrspieler:** Der Host wählt, schaltet die Phasen und rechnet Verstand und Dinge (auch für Gäste) – als Weltereignis mit der Nummer des
Gewählten. Stimme, Untertitel, Augen in den Bildnissen, Lampe und Arm gibt es nur auf dem Rechner des Gewählten (er erscheint nur ihm).
Den Handschlag des Gasts meldet ein Interact mit fester Kennzahl (`l5host:hand`), der Host prüft Abstand und Phase. Protokoll unverändert.

**Skript:** `hotel portrait|office <abstand> [winkel]`, `hotelforce host <phase>`, `hotelstate host_on|host_after|host_arm|host_lamp|fear_end <wert>`.

## Interpretation (begründet)

- **Gestalt:** Der Text beschreibt ihn nicht; gezeigt wird nur, was die Seite trägt – Stimme, Bildnisse, Büro, Hand. Der zu lange Arm
  im Anzug aus dem abgewandten Sessel ist die Auslegung von „Gastgeber“ und „Biest“ zugleich.
- **„Ein paar Dinge“** sind Gegenstände aus dem Inventar; **„nie wieder Angst“** ist eine Zeit ohne Verstandsverlust; **„kommt nicht mehr
  los, zerbricht“** ist die Rechnung (Verstand ≤ 20).
- **Er riecht Angst:** er wählt den Spieler mit dem kleinsten Verstand.
- **Büro:** ein leeres Zimmer am Hauptgang (die Seite nennt keinen Ort).

## Zahlen

| Wert | |
|---|---|
| Wählt | ab 260 s, Verstand < 70 (ab 520 s ohne), im Gang ≤ 45 m vom Büro |
| Einladung | 3 Sätze, 5 s Abstand; Bildnisse mit Augen: die 3 nächsten (≤ 16 m); Ansehen: −1 Verstand/s |
| Hand | ≤ 2,4 m vor dem Tisch; Arm in ~3 s ausgestreckt |
| Handel | Verstand 100, 180 s furchtlos, 2 Dinge; danach Verstand ≤ 20 |
| Ablehnen | 90 s / 6 s außerhalb / 50 m: −8 Verstand |

## Wow-Check

„IIIH, was ist das für ein Vieh?“ – Eine freundliche Männerstimme spricht dich aus einem Bildnis an, das dich mit echten, nassen Augen
ansieht; im Büro hebt sich aus einem abgewandten Sessel ein viel zu langer Arm im Anzugärmel über den Tisch und bietet dir eine
wachsbleiche Hand – und wer sie nimmt, hat keine Angst mehr, bis die Rechnung kommt. Ehrliche Antwort: unheimlich durch Stimme, Augen und
Handel; der Ärmel liest sich aus der Nähe noch wie ein Rohr (Restmängel).

## Kritikrunden

Bilder: `docs/screens/creatures/gastgeber/` (Studio `rN_host_bogen.jpg`, Ablauf `rN_hostflow_bogen.jpg`).

**Runde 1** – `r1_host_bogen.jpg`. *Schlecht:* Der Arm ist ein dünner schwarzer Stab, die Hand ein weißer Klumpen; das abgekratzte
Gesicht im Bildnis liest sich wie eine Vase; das Büro ist durch die Deckenleuchte hell. *Änderung:* dicker Ärmel mit Ellbogen, Hand
1,3-fach.

**Runde 2** – `r2_host_bogen.jpg`. *Schlecht:* „Ofenrohr“ – gerade, gleichmäßig, Ringe; die Hand Plastik. *Änderung:* Arm über die Lehne
gehoben, zwei Ellbogen, auf den Tisch gelegt, die Hand hebt sich; Hand als Distanzfeld (Knöchel, zu lange Finger, Siegelring), 1,5-fach.

**Runde 3** – `r3_host_bogen.jpg`. *Gut:* Lampe, Vertrag, Telefon, abgewandter Sessel. *Schlecht:* im Ablauf noch nicht geprüft.

**Runde 4 (Ablauf)** – `r4_hostflow_bogen.jpg`. *Gut:* Er wählt, die Stimme spricht mit Untertitel aus dem Bildnis, im Büro die
nächsten Sätze. *Schlecht:* Der Arm erschien erst nach Sekunden – die Bilder waren zu früh, der Handschlag ging ins Leere.
*Änderung:* Ablaufzeiten der Bilder.

**Runde 5 (Abnahme)** – `r5_hostflow_bogen.jpg`, `r5_host_bogen.jpg`; beste Ansichten `r5_hostflow_a_stimme.jpg`, `r5_hostflow_c_hand.jpg`
(„Die Hand nehmen“), `r5_hostflow_d_handschlag.jpg` (Handel), `r5_host_b_buero_tuer.jpg`. Ablauf vollständig: Wahl, Stimme, Büro, Hand,
Handel. Abgenommen mit Restmängeln.

## Tests (`gameplay_test`, `testV133Level5Hotel`)

Büro in der Karte (Schreibtisch, Sessel, Bildnis); er wählt den mit der meisten Angst und bittet ins Büro (Untertitel); im Büro die Hand;
Handschlag – Verstand voll, Dinge genommen; furchtlos; die Rechnung (Verstand ≤ 20).

## Restmängel

- Der Ärmel liest sich aus der Nähe noch wie ein Rohr; die Augen in den Bildnissen sind aus mehr als 3 m kaum zu sehen.
- Das Büro ist durch die Deckenleuchte des leeren Zimmers (Karte, nicht verschoben) heller als gewünscht; die grüne Lampe wirkt erst nah.
