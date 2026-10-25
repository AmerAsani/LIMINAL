# LIMINAL V11 – Änderungen gegenüber V10

V11 ist eine Überarbeitung von Grund auf: **zehn feste Ebenen** als eine Reise von Level 0 bis Level 37, Kreaturen nach `ART_DIRECTION.md` (das Lächeln neu, der Hund überarbeitet, das Bündel neu), tödliche Begegnungen mit Wiedereinstieg am letzten sicheren Ort, Wasser, eigene Klänge für jede Ebene. V10 bleibt unverändert in seinem Ordner (geprüft: alle 18 880 Dateien bitgenau); V11 hat eigene Spielstände (`saves_v11`), Einstellungen (`settings_v11.json`) und Protokolle.

## Qualitätsüberarbeitung (25.10.2026)

Anlass war die Rückmeldung zu V11: Die Welt wirkte blockig, Räume waren eng und gleichförmig, Steckdosen und Lüftungsgitter schwebten, in Level 0 lag zu viel herum, Kreaturen, Taschenlampe, Oberfläche und Klang sollten besser werden, und ein Mehrspielermodus war zu prüfen. Der in der Nachricht erwähnte Screenshot war **nicht** beigefügt; alles wurde nach der schriftlichen Beschreibung bearbeitet. V10 blieb unverändert (Prüfung des Manifests am Ende dieser Runde). Diese Runde wurde nicht mehr abgeschlossen: Die Arbeit ging in V12 weiter; die Kreaturen wurden in dieser Runde nur angesehen, nicht geändert.

### Halt der Ausstattung (alle Ebenen)
- **Regeln** (`tools/levels/mapkit.py`): Jedes Objekt hat eine Montageart, also Wand, Boden oder Decke.
  - Wandobjekte (Steckdose, Lüftungsgitter, Bild, Schaltkasten, Schild, Tür …) nur an echten Wandflächen (`wall_faces`, `wall_ok`): dahinter Wand, davor Luft, nicht über einem Durchgang ohne Sturz.
  - Bodenobjekte stehen auf dem Raumboden oder einem Block, Deckenobjekte hängen an der Decke.
- **Prüfung:** `validate()` bricht das Erzeugen einer Karte bei Verstößen ab. Zusätzlich prüft `gameplay_test` alle **2930** Objekte in der echten Kachelwelt: vorher **731** ohne Halt, jetzt **0**.
- **Durchgänge:** Offene Durchgänge sind immer raumhoch. Wo ein Schild darüber hängen soll, ist es jetzt ein breiter Durchgang mit Sturz (Level 1).

### Level 0 nach dem Wiki neu gebaut (Wikidot Revision 50)
- Gegliederte Räume, Flure und Durchbrüche, 2,85–3,15 m hoch, dazu ein langer Flur:
  - Säulenfeld;
  - Bogenräume mit eigenem Bogenmodell;
  - Lochfeld mit schwarzen Gruben (wer fällt, kommt am letzten sicheren Ort wieder zu sich);
  - Dunkelzone mit knöcheltiefem Wasser;
  - rote Räume;
  - ein vergilbtes Zimmer;
  - Naht nach Level 1.
- Keine Türen, keine Möbel. Steckdosen und Lüftungsgitter sitzen an echten Wandflächen.
- Nur noch 4–6 Funde (Mandelwasser, selten eine Batterie), an Wänden und in Ecken statt mitten im Raum. Notizen hängen an Wänden. Die Gestalt steht an Flurenden.
- Prüfliste mit Abweichungen: `LEVEL_CHECKLIST.md`.

### Räume, Licht und Leuchten
- **Leuchten je Zone** (`"fixture"`):
  - Röhrenleuchten in Gängen und Kammern (Level 1, 2, 3, Wartungsflure im Hotel, Level 7), hängende Röhren in Level 3 – statt überall gleicher Rasterpanels;
  - im Hotel Kronleuchter, Wandleuchten und Deckenkugeln.
- **Andachtsräume in Level 3:** 5,6 m hoch, vier kannelierte Säulen, Kronleuchter, Statue.
- **Hotelhalle:** stark möbliert, mit Bücherregalen, Schränken, Gemälden, Sitzgruppen und zusätzlichen Wandleuchten.
- **Lichtplanung** setzt keine Leuchten mehr über hohe Blöcke. Gruben haben eigene, schwarze Wände.

### Funde
- **Level 0** auf 4–6 verringert. Die übrigen Ebenen wurden einzeln geprüft und nur dort geändert, wo Funde unglaubwürdig lagen:
  - auf dem Schreibtisch (Level 1);
  - im Stahlregal (Level 2, 3);
  - auf Nachttischen, in Regalen und auf Tischchen (Level 5, nichts mehr auf dem Boden);
  - auf Couchtisch und Regal (Level 7);
  - auf der Truhe (Level 19).
- Funde liegen in der Höhe ihres Platzes (`generator.cpp`).
- **Zählung je Lauf** (Seeds 3/4242): L0 4, L1 8–10, L2 8–9, L3 9, L4 15–16 (Wiki: reichlich Mandelwasser), L5 11–14, L6 4, L7 2, L19 8–10, L37 0.

### Neue Modelle (im Code erzeugt)
Bogen, Bücherregal (4 Varianten), Couchtisch, Stahlregal (2), Deckenkugel, Säule (Höhe als Variante), Deckenlüftung, Röhrenleuchte (frei oder hängend).

### Oberfläche
- **Ruhigere Palette:** gebrochenes Weiß, ein warmer Akzent nur für Auswahl, aktive Werte und Fortschritt, Paneele als dunkles Glas mit Haarlinie. **Keine gelben Auswahlbalken mehr**, sondern eine helle Zeile mit schmaler Akzentmarke.
- **Hauptmenü ohne Kasten:** Wortmarke und Liste links auf einem Verlauf, der Raum bleibt sichtbar. Versionsangabe „LIMINAL V11“ (vorher stand dort noch V10).
- **HUD:**
  - Health und Sanity als schmale Balken unten links; sie treten zurück, solange sie voll sind und sich nicht ändern.
  - Die Lampe erscheint daneben nur, wenn sie an oder schwach ist.
  - Die Schnellleiste ist kleiner, nur mit Inhalt sichtbar und nach Änderungen kurz deutlich.
  - Die Ausdauer ist eine dünne Linie.
  - Interaktion knapp unter dem Fadenkreuz mit Tastenkappe; Meldungen als ruhige Zeilen; Zone klein unten links; Minimap kleiner, mit feinem Ring.
- **Todesschirm:** ein dunkles Band statt eines roten Kastens. Inventar, Pause, Einstellungen und Ladebildschirm folgen denselben Regeln.
- **Debug getrennt:** Seed und Schwierigkeit erscheinen beim Start nicht mehr als Meldung (Protokoll und Pausenmenü). Der Screenshot-Pfad steht nur im Protokoll. Die Technikanzeige bleibt auf F3.

### Taschenlampe und Hand (Ego-Perspektive)
- **Vorher** lag die Lampe längs im Unterarm in der Hand. Zu sehen waren nur eine Pinzettenhand und ein dunkler Knubbel.
- **Jetzt ein Griff von oben:** Die Hand liegt auf dem Schaft, die Finger schließen sich quer darum, der Daumen sitzt hinten, das Glas tritt an der Kleinfingerseite aus. Der Ellbogen ist seitlich, das Handgelenk bleibt fast gerade.
- **Lichtkegel = Achse des Modells**, aus der tatsächlich erreichten Hand gerechnet.
- **An Wänden** zieht die Hand die Lampe zurück und senkt sie. In der Enge kippt die Lampe nach unten. Das Licht beginnt nie hinter der Wandfläche.
- **Kamera:** Die Sprint-Vorlage schiebt sie nicht mehr in Wände. Das Problem bestand schon in V10.
- **Licht:**
  - Kein Kontaktschatten mehr für die Lampe; Hand und Lampenkopf erzeugten eine gezackte dunkle Scheibe im Kegel.
  - Breiteres, schwaches Streulicht wie bei echten Reflektoren.
- **Geprüft:** stehen, nach unten sehen, gehen, rennen, drehen, ausschalten (Level 1 und 3).

### Klang
- **Schallausbreitung** (`gameplay/sound_field`): Klänge hinter Wänden kommen durch Türen und um Ecken. Richtung ist die letzte sichtbare Stelle des Wegs, Dämpfung und Verzögerung folgen der Weglänge, je stärker der Schall abknickt, desto dumpfer.
  - Ohne offenen Weg geht er durch die Wand und ist stark gedämpft. Vorher reichte eine Wand für nur −3,5 dB.
  - Das gilt für Kreaturen, Ereignisse, Ausstattung und ferne Klänge.
- **Stille als Mittel:** Während einer Begegnung treten die Klangbetten zurück. Danach bleibt es neun Sekunden fast still, und ferne Einzelklänge pausieren.

### Mehrspieler
Machbarkeit und Bedrohungsanalyse in `MULTIPLAYER_ASSESSMENT.md`. **Nicht gebaut**: Die nötige Sicherheit lässt sich hier nicht nachweisen (keine Tests über ein echtes Netz, keine Lasttests, keine externe Prüfung). Das Spiel baut weiterhin keine Netzverbindung auf.

### Texte und Lizenz
- Der Wiki-Ortsname „Manila“ ist aus dem Spiel entfernt (jetzt „Vergilbtes Zimmer“, `LICENSE_REVIEW.md` Abschnitt 6).
- Wer in eine Grube fällt, liest „Du fällst ins Bodenlose – und kommst bei „…“ wieder zu dir.“ statt der veralteten Meldung „Prototyp: Ziel ist noch nicht gebaut“.
- Neue Dokumente: `LEVEL_CHECKLIST.md`, `MULTIPLAYER_ASSESSMENT.md`.

### Tests
- `gameplay_test`: **349** Prüfungen, 0 Fehler. Neu sind:
  - Vorräte je Ebene;
  - Halt der Ausstattung;
  - Taschenlampe im Griff (14);
  - Schallausbreitung (7).
- `audio_test` 106, `worldgen_test` 29 658, `supply_balance_test` ohne Fehler.

## Die Reise: zehn Ebenen (`LEVEL_SELECTION.md`)
Jede Ebene ist eine feste Karte (Entwurf in `tools/levels/levelN.py`, erzeugt nach `data/maps/levelN.json`). Was ein Lauf darin verteilt (Gegenstände, Notizen, Kisten, Dekoration, Kreaturen), wählt er aus festen Punkten (`WORLD_GENERATION.md`).

| Ebene | Titel | Was sie ausmacht | weiter |
|---|---|---|---|
| 0 | Die Vorzimmer | gelbe Tapete, Säulenfeld, Bögen, Dunkelzone, rote Räume; nur die Gestalt | Naht nach 1 |
| 1 | Das Lagerdeck | drei Parkdecks, Hochregale, Labyrinth; Stromausfall, Lächeln, Hund, selten das Bündel | Naht nach 2, Bodenloch nach 19 |
| 2 | Die Rohrgänge | Versorgungstunnel, Felder an eigenen Stromkreisen, Dampf; Lächeln, Bündel | Naht nach 3 |
| 3 | Das Umspannwerk | Ziegelgänge, Schalträume, Transformatorhallen; Hund, Lächeln, Bündel | Aufzug nach 4 (braucht eine Sicherung) |
| 4 | Das stille Stockwerk | leere Büros, Abdrücke im Teppich, Fenster mit Regen; keine Angriffe | Treppe nach 5 |
| 5 | Das Hotel | Halle, Ballsaal, Flure mit Zimmernummern, Kesselraum; Motten-Nester, selten der Hund | Treppe nach 6, Tür nach 19 |
| 6 | Das Dunkel | völlig dunkel, die Lampe gibt kein Licht, man tastet; nur ein Rauschen weist den Weg | Treppe nach 7 |
| 7 | Das Meer unter der Decke | ein Zimmer in der Decke, darunter Meer; die Tiefe ruft etwas herauf | tauchen nach 37 |
| 19 | Der Dachboden | Nebenweg: Dielen, Dämmwolle, Glühen durch die Ritzen erholt den Verstand | Tür nach 5 |
| 37 | Die Becken | weiße Fliesen, warmes Wasser, keine Gefahr | das letzte Becken: Abspann |

- **Übergänge** mit Ankern: Naht, Treppe, Tür, Aufzug (verbraucht die Sicherung), Bodenloch (fallen), Tauchen (unter 7 m), Ende. Die Ankunft steht am Gegenstück; im ersten Bild löst nichts aus.
- **Ein Lauf, ein Seed:** Jede Ebene leitet ihren Seed aus dem Lauf ab. Was man aufgehoben hat und welche Notiz wo lag, merkt sich der Lauf je Ebene (`level_memory`), auch über Rückwege.
- **Wasser:** waten (langsamer), schwimmen (wohin man blickt, Sprungtaste hinauf), hinausklettern an Kanten bis 1,8 m, unter Wasser dumpfer Klang und Nebel, 30 s Luft. Im Meer von Level 7 kostet die Kälte Gesundheit; wer zu lange über der Tiefe bleibt, weckt etwas (Rumpeln, Sog, Zugriff). Licht nach unten hält es kurz fern.
- **Raumtypen** wirken: rote Räume zehren am Verstand, das Glühen im Dachboden gibt ihn zurück, Hitze kostet Ausdauer.
- **Das Ende:** Wer in das letzte Becken von Level 37 taucht, dessen Licht von unten kommt, ist angekommen. Ein eigener Abspann („Angekommen“) mit dem Laufbuch.

## Kreaturen (`ART_DIRECTION.md` 5.2, `ENTITY_BEHAVIOR.md`)
- **Das Lächeln, neu modelliert:** kein schwarzer Klumpen mehr, sondern eine Säule tieferer Dunkelheit. Sie verdeckt den Hintergrund nur teilweise und hat einen rauchigen Rand (gerastert ausgedünnt; TAA mischt ihn über viele Bilder). Das Gesicht sitzt in 1,7–2,0 m Höhe und ist breiter als ein Menschengesicht:
  - zwei schmale, mandelförmige Schlitze aus kaltem Weiß, leicht ungleich;
  - Lider, die von oben **und** unten schließen (Lidschlag, enger, wenn das Lächeln aufgeht);
  - ein 58 cm breites, ungleich gebogenes Lächeln mit 83 nadelförmigen, unterschiedlich langen, teils abgebrochenen Zähnen, die ineinandergreifen und ungleich leuchten.
  - Das Leuchten pulsiert wie Atem, die Zähne zittern kaum merklich. Die Lampe macht den Körper nicht grau: der Glanz ist so dunkel wie die Farbe.
- **Der Hund, überarbeitet:**
  - nasses, strähniges Haar statt Drahtbürste: flache Strähnen, die zur Spitze hin schmaler werden und ausfransen;
  - die Stirnsträhne fällt über das Gesicht, die Mähne liegt auf dem Rücken und fällt über die Flanken;
  - Haarknochen werden vom Kopf weitergetragen, so dass Strähnen beim Schwingen nicht mehr in „Sprossen“ zerreißen.
- **Das Bündel, neu** (`ENTITY_BEHAVIOR.md` 2.3):
  - Körper: eine Masse aus verwachsenen Rumpfteilen und halb versunkenen toten Gliedern, sieben menschliche Arme (der längste 2,6 m) in verschiedenen Hauttönen, zwei Beine, die nachschleifen, unten ein Maul mit zwei Zahnreihen; Haut feucht glänzend, der Kern in Blutergussfarben.
  - Es haust in Deckenschächten (Lager der Karten von Level 1–3); ein Arm hängt als Köder heraus, die Finger zucken.
  - Es **hört nur**. Lärm lässt es lauschen und an Rohre klopfen. Weiterer Lärm, und es fällt heraus und zieht sich Hand über Hand, ruckartig, zum **Ort des letzten Geräuschs**.
  - Es tastet und kehrt nach 20 s Stille in den Schacht zurück.
  - Wer in Armlänge Lärm macht, sieht den langen Arm ausholen (1–1,5 s). Der Griff zielt auf die gehörte Stelle: Wer leise von dort weggeht, entgeht ihm; wer weiter Lärm macht, wird gegriffen.
  - Klänge: Klopfen, Klatschen der Hände, Schleifen, Ausholen, Griff.
- **Lärm als Reiz:** still 0, leise gehen 0,15 (neue Taste **C**), gehen 0,45, rennen 1,0, Landung 0,8, Pfütze und Wasser +0,4, geworfene Dinge am Aufprallort 0,6. Das Bündel hört etwa Lautstärke × 22 m Weg. Etwas werfen (**Q**) lockt es weg.
- **Alle Kreaturen** halten die Richtwerte für Dreiecke ein: Lächeln 120 k / 18 k / 3,5 k, Hund 148 k / 37 k / 6,9 k, Bündel 142 k / 40 k / 6,6 k.

## Entscheidungen (Dokumente im V11-Ordner)
- `ENGINE_DECISION.md`: weiter mit der eigenen Engine (statt Unreal oder Unity), mit Begründung aus den offiziellen Unterlagen.
- `LICENSE_REVIEW.md`, `ATTRIBUTION.md`: Weg B, eigene Auslegung. Im Spiel stehen keine Wiki-Namen, -Texte oder -Bilder. Es gibt keine fremden Dateien.
- `WORLD_GENERATION.md`: halb-prozedural. Wände, Räume, Türen und feste Objekte sind je Ebene fest. Gegenstände, Kisten, Dekoration und Kreaturen wählt jeder Lauf aus festen Punkten.
- `LEVEL_SELECTION.md`, `ART_DIRECTION.md`, `AUDIO_DESIGN.md`, `ENTITY_BEHAVIOR.md`.

## Level 1 „Das Lagerdeck“ (feste Karte)
- **Aufbau:** Drei Parkdecks auf drei Höhen mit Rampen, Lagerboxen, zwei Lagerhallen, ein Gang-Labyrinth mit Kammern, weiße Wartungsflure mit eigenem Strom, ein Bodenloch nach Level 19 und ein Rohrgang nach Level 2. Die Karte entsteht aus `tools/levels/level1.py` und liegt in `data/maps/level1.json`.
- **Ausstattung (neues Modul `src/props`, alle Modelle im Programm erzeugt):**
  - Leuchtbänder und Hallenstrahler.
  - Notausgangsschilder mit eigener Batterie: sie brennen im Stromausfall weiter.
  - Fahrbahnmarkierung, Stützennummern, Wagen in vier Lacken.
  - Hochregale mit Paletten, Kartons und gewickelter Ladung.
  - Rolltore, Schreibtisch mit Röhrenmonitor, Bett, Bilder, Lager, Schaltkästen, Rohre, Pfützen.
  - Am Bodenloch: Bruchkanten mit Bewehrung, Betonbrocken, Absperrschranken.
- **Kollision für Ausstattung:** Wagen und Hochregale stehen auf unsichtbaren Blöcken (`TF_HIDDEN`). Kollision, Schatten und Wege gelten, gezeichnet wird nur das Modell.
- **Das Bodenloch** ist 7,4 m tief und glüht von unten. Dafür gibt es eine neue Lichtart, die nach oben strahlt und an keinem Stromkreis hängt.
- **Übergänge der Karte** werden ausgelöst: hineinfallen oder durchgehen. Die Ziele sind im Prototyp noch nicht gebaut. Ein Hinweis erscheint, und nach einem Sturz geht es zurück zum Anfang.
- **Notizen** liegen an den Punkten der Karte. Die ersten beiden beschreiben die Regeln von Lächeln und Hund (Fairness F5).

## Klang
- **Neuer Mischer:**
  - Abstand 1/d, Laufzeit- und Pegelunterschied zwischen den Ohren, Kopfschatten, Luftdämpfung, dumpf hinter Wänden.
  - Nachhall als Netz aus acht Verzögerungen plus Frühreflexionen aus den Raummaßen.
  - Gruppen, Ducking, Begrenzer, 96 Stimmen, Varianten ohne Wiederholung.
- **324 neu erzeugte Klänge** (`AUDIO_DESIGN.md`, Abschnitt 6). Es gibt keine fremden Dateien.
- **Klanglandschaft je Ebene** aus der Level-Datei:
  - Klangbetten je Raumtyp, Ereignisse, Dauerquellen an Stellen der Ausstattung, Nachhall je Raumtyp.
  - Schritte in Pfützen.
  - Alle Ereignisse des Spiels sind auf die neuen Klänge umgestellt. Unbekannte Namen meldet das Protokoll.

## Die Begegnung: Stromausfall und Lächeln (`ENTITY_BEHAVIOR.md`, Abschnitt 4)
- **Der Stromausfall kommt als Welle.** Schütze fallen Feld um Feld vom fernen Eck her, Licht und Brummen sterben mit.
- **Danach erscheint im Dunkeln das Lächeln**, außer Sicht und mindestens 15 m Weg entfernt.
  - Blickkontakt hält es fest.
  - Wer in seiner Nähe rennt, bekommt zuerst eine Ankündigung von 1,5 s: es öffnet sich, der Ton steigt. Wer dann stehen bleibt und hinsieht, beruhigt es.
- **Der Zugriff ist tödlich, mit einem echten Schreck** (Entscheidung vom 25.10.2026):
  - Die Kamera reißt zum Gesicht herum, und das Gesicht fährt bis vor die Linse. Dazu Zittern, Zoom, ein weißer Blitz und ein eigener Schrei je Kreatur.
  - Nach 1 s kommt der harte Schnitt ins Schwarz, nach 2,4 s der Todesschirm.
- **Wiedereinstieg am letzten Sicherungspunkt:**
  - 12 Punkte auf Level 1, gesetzt nach festen Regeln: am Eingang jedes Bereichs, hell auch im Stromausfall, fern von Kreaturen. Grüne Sicherheitsleuchten zeigen sie.
  - Vorbeigehen speichert.
  - Nach dem Tod: volle Gesundheit, Inventar bleibt, 90 s Schonzeit.
- Auf Level 1 kommt das Lächeln nur mit dem Stromausfall.

## Tests
- `gameplay_test`: 312 Prüfungen, 0 Fehler. Neu sind:
  - die Begegnung auf der Karte von Level 1: Ankündigung, Beruhigen, tödlicher Zugriff mit Schreck;
  - Wiedereinstieg und Sicherungspunkte;
  - die Kollision von Wagen, Hochregalen und dem Bodenloch;
  - die Skelette der Kreaturen (höchstens 64 Knochen, die Augen und Lider des Lächelns);
  - leise gehen (knapp halbes Tempo, kein Sprint);
  - das Bündel auf Level 2:
    - lauern, still stehen bleibt unbemerkt;
    - ein Wurf lockt es aus dem Schacht zum Aufprall, am still stehenden Spieler vorbei;
    - Lärm in Armlänge: Ausholen; leise weg: der Griff geht ins Leere; weiter Lärm: tödlich;
  - die Reise: jeder Ausgang hat im Ziel sein Gegenstück, das Ende liegt in Level 37;
  - Wasser: im Meer von Level 7 schwimmen, zu lange unten ertrinken.
- `audio_test`: 106 Prüfungen, 0 Fehler.
- `worldgen_test`: 29 658 Prüfungen, 0 Fehler (alte Welt bitgenau).
- `supply_balance_test`: OK.
- **Neue Skriptbefehle für Tests:**
  - `pos x y [blick] [neigung]`;
  - `hold <Taste> <Bilder>`;
  - `presence encounter|smiler|hound|clump`, `face <kreatur>`, `figure <kreatur> [abstand] [drehung]`;
  - `give <item>`, `key <taste>`;
  - `audioinfo`.

## Bekannte Grenzen
- **Kreaturen:**
  - Die Motten und die Gestalt sind noch die Modelle aus V10 (die Gestalt hat jetzt den Rauchrand des Lächelns).
  - Das Bündel ist auf Level 3 nicht größer als auf Level 2.
  - Seine Hände prüfen beim Kriechen keine Wände, in engen Gängen greifen sie manchmal in die Wand.
  - „Etwas in der Tiefe“ (Level 7) ist nur zu hören, nie zu sehen.
- **Regeln:**
  - Die eingeschaltet abgelegte Lampe als Köder fehlt.
  - Der Hund hört weiterhin nur Rennen und nahes Gehen, nicht das ganze Lärm-Modell.
- **Level 6:** Wie hell die Wände in Armlänge erscheinen, hängt von der automatischen Belichtung ab. Frontal vor einer Wand ist es heller als „schwach“.
- **Hotel:** Die Kronleuchter sind für 6 m hohe Hallen etwas klein.
- **Karte:** Das Gang-Labyrinth von Level 1 ist noch zu regelmäßig.
- **Klang:** Ob die Klänge gut klingen, ist nur gemessen, nicht gehört (`tests/sound_export.exe` schreibt sie als WAV). Um Ecken gebeugt wird der Schall seit der Qualitätsüberarbeitung (oben).
- **Abspann:** Den Fahrstuhl-Ausgang aus V9 gibt es auf den festen Karten nicht mehr; sein Abspanntext bleibt für alte Welten erhalten.

---

# LIMINAL V10 – Änderungen gegenüber V9

V9 hat der Welt ein Ziel gegeben. Der Gegner war aber ein schwarzes Strichmännchen, und die Halluzinationen waren Abziehbilder. V10 ersetzt sie durch **Entities aus den Backrooms-Wikis**: Jede lebt in dem Bereich, in den sie laut den Quellen gehört, sieht aus, wie sie beschrieben wird, und verhält sich so. Die Welt selbst ist unverändert: gleiche Seeds, gleiche Räume, bitgenau.

> Eine erste V10-Fassung hatte stattdessen erfundene Figuren (der Angestellte, der Beobachter, der Andere, später Pfahl, Laterne, Kriecher, Abgewandte). Sie sind vollständig entfernt, samt Modellen, Texturen und Klängen. Geblieben ist nur die Technik dahinter.

## Recherche und Zuordnung
- **Quellen:** Backrooms Wiki auf Wikidot (Hauptquelle; Seiten zu Level 0, 1 und 5 sowie zu Entity 3, 4, 5, 8, 9, 10 und 15) und auf Fandom (Vergleich; Entity List, Level 0 und 1, Smilers, Deathmoths, Hounds). Eine eigene „Backrooms Foundation“-Seite wurde nicht gefunden.
- **Dokumentiert** in [docs/ENTITAETEN.md](docs/ENTITAETEN.md): was die Quellen je Entity sagen, wo sie sich widersprechen, welche Auslegung gewählt wurde und warum Clump, Skin-Stealer, Faceling und Wretch nicht umgesetzt sind.
- **Zuordnung:** Innenräume = Level 0 (keine bestätigten Entities, nur dunkle Gestalten). Tiefgeschoss = Level 1 (Stromausfälle, Smiler, Hound). Hotel = Level 5 (Deathmoths mit Nestern, selten Hound).

## Die Entities
- **Smiler** (Entity 3): Nur Augen und ein langes Lächeln voller spitzer Zähne leuchten. Der Körper ist eine formlose schwarze Rauchmasse ohne Beine. Lebt im Dunkeln, kommt bei Stromausfällen. Das Licht der Lampe lockt ihn. Blickkontakt hält ihn fest. Ungesehen kommt er durch das Dunkel näher, nie ins Licht. Rennen in seiner Nähe löst einen Angriff aus. Kehrt das Licht zurück, erlischt er.
- **Hound** (Entity 8): Ein Mensch auf allen vieren. Knochig, Hände mit Schwimmhäuten, pfotenartige Füße. Langes schwarzes Haar verdeckt Kopf und Gesicht und läuft als Mähne den Rücken entlang. Weiße Augen, großes Maul mit zwei Zahnreihen, räudige Haut, hinkt. Streift umher, hört Rennen, sieht das Licht. Hat er dich, knurrt und pirscht er. Blickkontakt schüchtert ihn eine Weile ein. Den Rücken zugekehrt oder rennend: Bellen, Angriff, danach Rückzug. Verliert er die Spur, winselt er.
- **Deathmoths** (Entity 4): Etwa 45 cm Spannweite, zwei gezackte, grau behaarte Flügelpaare mit Muster, brauner Brustpanzer, behaarter Hinterleib mit Nadelborsten, gefiederte Fühler, Rüssel, bedornte Beine. Fleischig-harzige Nester überwuchern die Türen dunkler Hotelzimmer. Die Motten sitzen reglos an Wand und Decke. Licht oder Nähe scheucht den Schwarm auf. Er umkreist die Lampe, einzelne Motten stechen zu. Ohne Licht kreisen sie um die Deckenleuchten und kehren zum Nest zurück, durch die Tür statt durch Wände.
- **Gestalt** (Level 0): Ein dunkler, formloser Umriss am Ende eines Gangs. Verändert sich nie im Blick, ist fort, sobald man wegsieht, und greift nie an.

## Fair, nicht billig
- Keine Entity erscheint im Blickfeld oder neben dir. Jeder Angriff kündigt sich an: Knurren und Bellen, das Dröhnen des Smilers (eigene Ergänzung, keine Stimme), das Flattern.
- „Bewegt sich nur ungesehen“ gilt nur beim Smiler (Blickkontakt) und bei der Gestalt. Hound und Motten bewegen sich offen.
- Nach dem ersten Treffer nennt eine Einblendung den Rat aus dem Wiki.

## Technik
- **Neu:** `anim/creature` (fünf Skelette), `anim/creature_anim` (Vierbeiner mit Hinken, Flug mit zwei Flügelpaaren, Rauchmasse, Haarketten mit Kollision), `render/entity_models` (Modelle), `render/moth_wing.hpp` (Flügelumriss für Netz und Textur), `render/texture_entity` (acht Materialebenen bis 4096 px plus vier Detailnormalen).
- **Geändert:** `gameplay/presence` (Regie je Bereich, Verhalten, Stromausfall, Schwarm mit Wegesuche), `gameplay/haunt` (echter Stromausfall auch ohne Halluzinationen), `audio/synth` (neue Klänge), Shader (Leuchtfarbe aus dem Material), Hilfe, Notizen, Einstellungen.
- **Spieler:** realistischer Körper (Kapuzenjacke, Jeans) oder das Strichmännchen. Die Animator-Korrektur bei der Landung des Fußes bleibt.
- **Tests:** `gameplay_test` 193 Prüfungen (neu: Hound, Smiler mit Stromausfall, Mottenschwarm mit Nest, Gestalt, Bereichsregel), `audio_test` 59, `worldgen_test` 29 658, `supply_balance_test` OK.

---

# LIMINAL V9 – Änderungen gegenüber V8

V8 hat aus den Leveln eine zusammenhängende Welt gemacht, aber in dieser Welt gab es nichts zu erreichen und nichts zu fürchten außer der eigenen Sanity. V9 gibt ihr **ein Ziel** und **einen Gegner**. Die Welt selbst ist unverändert: gleiche Seeds, gleiche Räume, bitgenau.

## Kurzfassung

- **Ein Weg hinaus:** In jeder Ebene hängen tief im Inneren Sicherungskästen. Mit einer Sicherung aus jeder Ebene (Büros, Tiefgeschoss, Hotel) fährt ein Aufzug weiter als je zuvor, ins Licht. Danach kommt ein Abspann mit dem Laufbuch.
- **Der Andere ist echt:** Eine große, gesichtslose Gestalt folgt dir. Sie bewegt sich nur, wenn du sie nicht siehst. Im Dunkeln hilft nur die Taschenlampe.
- **Hinweise:** Neue Notizen, die wichtigsten zuerst, dazu eine leise Einleitung, die Zielanzeige im Pausenmenü und Markierungen auf der Karte.

---

## Der Weg hinaus

- **Sicherungskästen** (`data/world.json` → `fuses`, Prefab `data/prefabs/fusebox.json`):
  - Wahrscheinlichkeit 1,2 % je Raum (keine Gänge, Decke ab 2,4 m). Nur in der **eigenen** Schicht einer Ebene und dort nur zwischen 22 % und 85 % ihrer Tiefe, also nie an den Nahtstellen.
  - Der Kasten sitzt an einer freien Wandstelle: keine Tür dahinter, keine Wandleuchte, Nachbarkacheln frei.
  - **Gemessen** (`layout_stats --world --radius 8`, 5 Seeds), nächster Kasten:

    | Ebene | nächster Kasten |
    |---|---|
    | Büros | 82–188 m |
    | Tiefgeschoss | 339–478 m |
    | Hotel | 832–975 m |

  - **Aussehen:**
    - Stahlkasten mit offener Tür und Warnschild „VORSICHT STARKSTROM“ (auch an der Wand darüber).
    - Kabelrohr bis in die Decke, verzinkte Montageplatte „HAUPTSICHERUNG“.
    - Bakelithalter mit Messingklemmen, Schmelzsicherung aus Porzellan („63A“), orange Kontrolllampe.
  - **Atmosphäre:**
    - Netzbrummen mit Knistern als räumliche Quelle (22 m).
    - Unruhiges oranges Licht mit seltenen Funken.
    - Hinweis „Irgendwo ganz in der Nähe brummt Starkstrom …“ ab 30 m.
  - **Karte:** Gesehene Kästen werden gemerkt und gespeichert, orange (geleert grau). Volle Kästen erscheinen auch als Marke am Kartenrand.
  - **E** nimmt die Sicherung heraus (eine je Ebene): Klemme, Funke, das Brummen bricht ab. Der Kasten bleibt leer, auch nach dem Laden (`picked`).
- **Sicherungen** sind Items (`fuse_office`, `fuse_cellar`, `fuse_hotel`) mit eigenem Modell und Symbol. Sie lassen sich ablegen, aber nicht „benutzen“.
- **Der Aufzug:**
  - In der ersten Kabine: „Unter dem Tastenfeld: drei leere Fassungen für Sicherungen. Daneben, eingeritzt: »RAUS«.“ Später meldet die Kabine, welche Sicherungen noch fehlen.
  - Mit allen drei: **„Sicherungen einsetzen“**. Die Fahrt dauert elf Sekunden, ohne Ortswechsel.
  - Unterwegs rüttelt die Kabine immer stärker, das Sichtfeld weitet sich, das Kabinenlicht wird bis zu achtmal heller, das Bild bleicht aus.
  - Ein eigener Klang: Der Antrieb läuft hoch, ein Akkord schwillt an.
  - Die Sicherungen werden erst am Ende verbraucht. Ein Autosave während der Fahrt kann also nichts kosten.
- **Abspann „DRAUSSEN“** (neuer Oberflächenmodus):
  - Weißblende, dann ein warm werdender heller Grund mit kurzem Text.
  - Danach das Laufbuch Zeile für Zeile: Name, Schwierigkeit, Zeit bis zum Entkommen, Weg, Räume, Ebenen, Übergänge, Notizen, Halluzinationen, Begegnungen mit dem Anderen, Seed. Dazu Wind und ferne Vögel.
  - Enter überspringt. Danach: **Weiter erkunden** (die Tür geht auf, Einblendung aus Weiß) oder **Zum Hauptmenü**. Der Stand wird gespeichert (`runlog.escaped`, `escape_time`).
- **Pausenmenü:** neuer Block „DER WEG HINAUS“ mit ✓/○ je Ebene und einem kurzen Ziel. Das Laufbuch hat eine dritte Zeile.

## Der Andere

- Modul `gameplay/stalker.cpp`, gehört zur Sitzung.
- **Seine Regel:** Er bewegt sich nur, wenn du ihn nicht siehst. Als gesehen gilt er, wenn mindestens ein Punkt (Kopf, Brust oder Knie) alle drei Bedingungen erfüllt:
  1. Er liegt höchstens etwa 50° neben deiner Blickrichtung.
  2. Die Sichtlinie ist frei.
  3. Er ist beleuchtet: Kachelhelligkeit ab 0,2 und kein Stromausfall in seinem Raum, oder er steht im Kegel der Taschenlampe (26°, 22 m).
- **Gesehen:** Er erstarrt mitten im Schritt (die Pose bleibt stehen). Beim ersten Blick je Jagd ertönt ein Stich, und die Sanity sinkt um 4 %.
  - Anstarren kostet zusätzlich 0,45 % Sanity pro Sekunde. Seine Nähe kostet bis zu 0,3 % pro Sekunde und entzieht dem Bild die Farbe.
- **Ungesehen:** Er folgt dem Gefälle eines Distanzfelds (Dijkstra über 81 × 81 geladene Kacheln, Stufen bis 0,65 m, schräg nur an freien Ecken vorbei). Ab 30 m Weg geht er mit 1,5-fachem Tempo.
  - Hörbar sind schwere, dumpfe Schritte (28 m), ein tiefes, atmendes Brummen an seiner Stelle (verdeckt hinter Wänden) und ab etwa 14 m dein Herzschlag.
- **Zugriff** (ungesehen, unter 0,95 m): Health sinkt um `stalker_hit`, Sanity um 20 %.
  - Ein Schlag im Bild: rote Ränder, Blende aus Schwarz.
  - Danach verschwindet er für 120–240 s × `stalker_pause` (bei niedriger Sanity kürzer).
  - Beim ersten Mal erklärt ein Hinweis die Regel. Bei 0 % Health: Todesbildschirm „ER HAT DICH ERWISCHT“.
- **Auftreten:**
  - Der erste Auftritt kommt nach 180 s, nach dem Laden nach 90 s.
  - Er erscheint 20–34 m Weg entfernt, ungesehen (auch nicht im Licht sichtbar), bevorzugt hinter dir.
  - Nach 150 s, bei über 52 m Abstand oder wenn er abgeschnitten ist, verliert er die Spur.
  - Bei Aufzugfahrten ist er sofort fort.
  - In der Demo und mit der Einstellung **„Verfolger“** (aus) erscheint er nicht.
- **Schwierigkeit** (`data/difficulty.json`):

  | | Tempo (m/s) | Pausen | Zugriff |
  |---|---|---|---|
  | Leicht | 3,2 | × 1,6 | −25 % |
  | Mittel | 4,0 | × 1,0 | −35 % |
  | Schwer | 4,7 | × 0,7 | −45 % |

- Er ist dieselbe gestreckte, gesichtslose Figur wie die Halluzination aus V7. Die Halluzination verschwindet, wenn man näher kommt. Er nicht.

## Hinweise und Oberfläche

- **Notizen** (`data/notes.json`): sechs neue für die Welt.
  - Vier haben Vorrang (`"priority"`): „Drei Sicherungen“, „Nicht wegsehen“, „Im Dunkeln“, „Er fährt nicht mit“. Man findet sie zuerst, solange sie nicht im Notizbuch stehen.
  - Dazu kommen „RAUS“ und „Orange“.
- **Einleitung:** Bei einem neuen Lauf erscheinen drei leise Zeilen nacheinander in der oberen Bildhälfte.
- **Hilfe:** neue Abschnitte „Der Weg hinaus“ und „Der Andere“.
- **Einstellung „Verfolger“** unter Grafik & Anzeige (`settings_v9.json`: `stalker`).
- **Laufbuch:** neue Felder `encounters`, `caught`, `fuses`, `escaped`, `escape_time`. Sie erscheinen im Pausenmenü, auf dem Todesbildschirm und im Abspann.

## Klang und Grafik

- **Acht neue prozedurale Klänge:** `fuse_buzz` (Schleife), `fuse_take`, `stalker_step`, `stalker_drone` (Schleife), `stalker_sting`, `stalker_attack`, `escape_ride`, `outside` (Schleife).
- **Neue Texturebene** `L_FUSE` (Atlas):
  - Kastenlack mit Kratzern und Rost
  - Warnschild
  - verzinkte Montageplatte mit Bakelit
  - Porzellan mit Aufdruck
  - Messing
- **Feste GPU-Materialien:** 32 statt 16, neu sind `FM_FUSEBOX`, `FM_FUSE_GLOW` (leuchtend) und `FM_ITEM_FUSE`. Die Materialbank fasst dafür 16 352 statt 16 368 Weltmaterialien.
- **Modelle:** `fusebox`, `fusebox_empty`, `fuse`, dazu das Inventarsymbol „fuse“.

## Start ohne Starter

- Im Paket ist `LIMINAL.exe` jetzt **direkt das Spiel** (eine Kopie von `LiminalGame.exe`).
- Den früheren Starter (`launcher/`) blockierte Smart App Control mit „Bösartige Binärreputation“ (Code Integrity, Ereignis 3077). Das ist ein Fehlalarm, ausgelöst durch die eingebaute Download- und Admin-Funktion für die Microsoft-Laufzeit. Das Spiel selbst wird zugelassen.
- Startfehler (z. B. ohne Direct3D 11) meldet das Spiel selbst in einem Fenster.
- Der Quellcode des Starters bleibt im Projekt.

## Versionen und Tests

- Neue Dateien:
  - `saves_v9`, Spielstandversion 9
  - `settings_v9.json` (beim ersten Start aus V8 übernommen, sonst V7 … V3)
  - `liminal_v9.log`, `benchmark_v9.txt`, `liminal_v9_*.dmp`
- **Alte Stände:** V8-Stände erscheinen mit „(V8)“ und laufen in derselben Welt weiter. V3–V7 werden wie bisher gelesen.
- **Tests:**
  - `gameplay_test`: 163 Prüfungen (44 neue für V9).
  - `worldgen_test`: 29 658 Prüfungen, bitgenau wie V8.
  - `audio_test` und `supply_balance_test` sind unverändert grün.
  - `layout_stats --world` zählt Sicherungskästen je Ebene und prüft mit `--check`, dass jede Ebene in jedem Seed einen hat.
- **Neue Skriptbefehle:** `stalker N`, `facestalker [versatz]`, `stalkerinfo`, `give item`, `health N`. `expect` kennt jetzt auch `Note` und `Ending`.
