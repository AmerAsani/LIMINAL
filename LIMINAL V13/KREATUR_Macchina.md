# Kreatur: Macchina (Entity 189), Level 2 – Reste

Quellen: [Entity 189 – „The Macchina“](https://backrooms-wiki.wikidot.com/entity-189) Rev. 27 (Autor Dr Bierre) und
[Level 2](https://backrooms-wiki.wikidot.com/level-2) Rev. 93 (Abschnitte Entities und Geschichte), beide Backrooms-Wiki auf Wikidot,
CC BY-SA 3.0. Alles in eigenen Worten, keine Bilder und keine Zitate. Im Spieltext fällt ihr Name nicht (die Werkstatt heißt in der
Karte wie bisher).

## Was die Seite sagt

- **Wer sie waren:** eine teils kybernetische Art, mit The Lost lose verwandt, Meister des Noclippens; Heimat Level 2, aus tierhaften
  Wesen entstanden. Etwa zweihundert Jahre ausgestorben; Level 2 nennt etwa 1850 als letzte Sichtung.
- **Anzüge:** lebenslang in großen, klobigen Metallanzügen mit unverstandener Technik; auf die Visiere schweißten sie menschenähnliche
  Muster, um für The Lost nicht fremd zu wirken. Ohne Anzug galt man nicht als lebendig; die wahre Gestalt abzubilden war tabu. Das
  Material rissen sie aus Level 2, bis kaum etwas übrig war; zuletzt starb jedes dritte Kind, weil kein Anzug da war.
- **Waffen, Schrift, Kunst:** Speere und Piken; eine Röhre für brennende Geschosse, zu teuer, nie benutzt; Schrift aus Zeichen wie
  Hieroglyphen, mit The Lost ein grobes Latein; große Wandtücher.
- **Ende:** Sie sprachen von „der großen Antwort“ und zeichneten Gestalten, die aus Lichtkugeln in Level 2 herabsteigen und nach ihnen
  greifen; kurz darauf verschwanden sie. Ruinen sind selten, keine Siedlung ist erhalten.

## Umsetzung

Kein lebendes Wesen – die Seite sagt, sie sind fort. In der Werkstatt (Zone `macchina`, schon mit `macchina_parts`/`macchina_tools`):

**Modelle** (`models_l2_rand.py`, Kartenobjekte aus `level2.py`, 2 Werkstätten):
- `macchina_suit` (11 000 Dreiecke): leerer Anzug, an die Wand gesunken; Rumpf aus Rohrblech, die Brust nach vorn aufgeklappt und
  **hohl** (Polsterreste, Kabel, sonst nichts); Arme und Beine aus Rohrstücken mit Flanschen, Handrädern und Kolben; Kuppelhelm, auf dem
  Visier ein Gesicht aus **Schweißperlen** mit gedeckten Anlauffarben – Brauen, ungleich hohe Augen mit gebohrten Löchern, Nase, Mund
  mit Zahnpunkten; schief und zu menschlich.
- `macchina_cradle`: unfertiges Kindergestell (Rippen aus Rohrringen, ein Ärmchen, kein Helm), daneben Zange und Zuschnitte.
- `macchina_tapestry`: Wandtuch – Lichtkugeln mit Strahlen, lange schwarze Gestalten steigen herab und greifen mit langen Fingern nach
  knienden Anzugträgern mit erhobenen Armen; Zeichen am Rand; verblasst, Stockflecken.
- `macchina_spear`: Pike mit eingeschlagener Klinge, davor die verrußte Brandröhre.
- `macchina_plate`: der Fund – eine Tafel aus flachgeklopftem, rostigem Rohrblech zu Füßen des Anzugs: Zeichen in Reihen, in der Mitte
  eine Lichtkugel mit herabsteigenden Gestalten, unten kniende Anzüge, am Rand eingeritzte Zeilen.

**Verhalten** (`apparitions_l2.cpp`/`session_apparitions.cpp`, Level-JSON `apparitions.macchina`):
- Der hohle Anzug klingt im Luftzug (`mc_hollow`, Schleife, 7 m); alle 18–40 s arbeitet das Blech (`mc_creak`).
- **Der erste Blick** (2,6 m, ansehen) erzählt den Fund: der leere, blank gescheuerte Anzug, das geschweißte Gesicht, das unfertige
  Gestell – „Ohne Anzug, heißt es, galten sie nicht als lebendig.“ (einmal je Spieler, im Spielstand).
- **Die Blechtafel** (E: „Blechtafel mit Zeichen ansehen“) kommt ins Notizbuch: Beschreibung der Zeichen und, später eingeritzt und
  übersetzt, was The Lost über sie wussten – die Anzüge, das Metall aus den Wänden, jedes dritte Kind, die große Antwort, danach niemand
  mehr (`data/notes.json`, mode `fund`: liegt nur dort, nie als Zufallsnotiz).

**Klang** (`synth_l2_rand.cpp`): `mc_hollow` (Rauschen durch zwei Resonanzen – Helm und Brustraum – an- und abschwellend),
`mc_creak` (Blechknarren, nachklirrende Platte, Hall).

**Mehrspieler:** Die Tafel ist eine gewöhnliche Notiz (Host entscheidet das Aufheben, wie alle Notizen); Klang und Meldung rechnet
jeder Rechner für sich aus derselben Karte.

**Skript:** `rand macchina <abstand> [winkel] [nr]`.

## Interpretation (begründet)

- Nur Reste, weil die Seite sie für verschwunden erklärt. Der Fund zeigt, was sie waren: ein leerer Anzug (ohne Anzug kein Leben), das
  unfertige Kindergestell (das dritte Kind), das Wandtuch (die große Antwort).
- Die Tafel ist eine Erfindung des Spiels (die Seite nennt Schrift aus Zeichen und das Latein mit The Lost); der Text ist eigener Wortlaut.
- Die blau pulsierenden Rohrlichter und die Tür mit der Macchina-Klinge (Weg zu Level 811) sind nicht gebaut (LEVELSPEC_2, Abschnitt 3).

## Wow-Check

„Im Lampenlicht sitzt ein Anzug aus Rohren an der Wand, die Brust offen und leer, und auf sein Visier hat jemand mit Schweißperlen ein
Menschengesicht gesetzt, damit es freundlich aussieht.“ Ehrliche Antwort: kein „IIIH“-Schreck, sondern ein stilles Grauen – passend zu
Wesen, die es nicht mehr gibt.

## Kritikrunden (Bilder `docs/screens/creatures/l2_randwesen/`)

**Runde 1** – `r1_mc_bogen.jpg`, `r1_mc_lampe.jpg`. *Schlecht:* fast alles schwarz (die Werkstatt ist dunkel); das Gesicht ein dünnes,
gemaltes Roboterlächeln; die Brust wirkt geschlossen; die Tafel hell wie Papier. *Änderung:* jede Ansicht mit Lampe; Gesicht aus
einzelnen Schweißperlen mit Anlauffarben, schief; Tafel aus dunklem Blech.

**Runde 2** – `r2_mc_bogen.jpg`, `r2_mc_nah.jpg`. *Gut:* Das Perlengesicht wirkt grob geschweißt und zu menschlich. *Schlecht:*
Anlauffarben leuchten violett; die Brust öffnet sich zur Seite statt nach vorn. *Änderung:* gedeckte Anlauffarben, kleinere Ringe;
Öffnung nach vorn.

**Runde 3** – `r3_mc_bogen.jpg`, `r3_mc_nah.jpg`, `r3_mc_wandtuch.jpg`. *Gut:* die leere, dunkle Brust; Wandtuch, Gestell und Pike im
Bild. *Schlecht:* Die Gestalten auf dem Tuch hängen nur, keine greift (die Seite: sie greifen nach ihnen); das Tuch zu sauber.

**Runde 4** – `r4_mc_bogen.jpg`, `r4_mc_wandtuch.jpg`. *Gut:* Arme weit ausgestreckt mit langen Fingern über den Knienden, mehr
Stockflecken.

**Runde 5 (aus den Augen des Spielers)** – `r5_pov_bogen.jpg`, `r5_pov_mc_lampe.jpg`. *Gut:* Mit der Taschenlampe steht der Anzug im
Kegel, die Meldung erzählt den Fund, die Tafel ist zu benutzen. Abnahme.

## Tests (`gameplay_test`, `testV133L2Rand`)

Anzug, Wandtuch, Kindergestell und Pike liegen in der Werkstatt (Zone `macchina`); der erste Blick erzählt den Fund (Meldung, Knarren);
die Tafel liegt nur beim Anzug (nie Zufallsnotiz).

## Restmängel

- Die Werkstatt ist ohne Lampe fast schwarz; mit der Lampe gut lesbar (gewollt).
- Weg zu Level 811 (blaue Rohrlichter, Klingentür) nicht gebaut.
