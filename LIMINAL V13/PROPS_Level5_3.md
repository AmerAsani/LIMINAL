# Ausstattung Level 5.3 „Promethei Bibliotheca“ (V13.3 Teil 1)

Alle 16 Objektklassen von Level 5.3 in Blender neu (`tools/blender/models/models_l53.py`). Grundlage sind
`LEVELSPEC_5.3.md` und `LOOK_5.3.md` (Wikidot Level 5.3): eine Bibliothek, älter als das Hotel, mit Wänden aus Büchern bis
unter die Decke, Kerzenlicht auf Leder und Gold, Säulen aus porösem Tuff, eisernen Kronleuchtern wie die Gitter von
Level 3, Gemälden im Stil der Renaissance, Wappen an den Decken, Inschrift „PROMETHEI BIBLIOTHECA“, sehr alten Holztüren
und abgeblätterter Tapete im Eingangsgang.

**Bauweise**
- Eigener Geometrie-Kern `Geo` im Modul: Flächen mit eigenen Normalen je Ecke (ohne Verschweißen), Profil entlang eines
  Wegs mit Gehrung (`sweep`), Drehkörper (`lathe`, auch mit Material je Facette), Rohre mit mitgeführtem Rahmen (`tube`),
  Quader mit Fase (`cbox`), Schrift aus dem Zeichenatlas des Spiels (`_atlas_text`).
- **Bücher** sind einzelne Bände. Damit 939 Regale unter dem Budget bleiben, hat jedes Buch nur die sichtbaren Flächen:
  - Rücken mit runder Wölbung über Normalen je Ecke, ohne zusätzliche Dreiecke;
  - Verzierung als Flächen 0,8 mm davor: Bünde, Goldlinien an Kopf und Fuß, Titelschilder (rot, schwarz, grün),
    Papierschilder mit Signatur, abgewetzte Kapitale;
  - von den Seiten nur die Stufen zum Nachbarn (L-förmig, wenn der Nachbar niedriger oder zurückgesetzt ist);
  - Schnitt oben nur bis 2,2 m über dem jeweiligen Geschossboden (darüber unsichtbar). Schnitt in Creme, staubig, rot
    gesprenkelt oder vergoldet.
- **Buchgruppen-Bausteine:** mehrbändige Werke (3–9 gleiche Bände, einzelne fehlen oder sind herausgezogen),
  gemischte Bände, liegende Stapel, schief lehnende Bücher (liegen auf der unteren Kante, lehnen am Nachbarn), Lücken.
  Format je Fach: unten Folianten, oben Duodez; 16 Einbände (Leder, Leinen, Pergament), verblichene Rücken,
  Halbleder mit Marmorpapier.
- Gebrauchsspuren als eigene Flächen: Staub auf den Fachböden und Schnitten, abgegriffene Kanten in Griffhöhe,
  Patina in den Kehlen der Bronze, Rost am weißen Eisen, Craquelé und Fehlstellen auf den Gemälden, Kleisterflecken
  und Wasserränder an der Tapete, Poren und Ausbrüche am Tuff, Haarrisse im Marmor.
- Rauheit und Metallizität je Material über `r=`/`m=`:
  - Holz `r` 1,6–3,0 (die Holztextur ist sehr glatt, mit `r` 0,85 spiegelten die Fachböden hell);
  - Marmor `r` 2,5;
  - Blattgold `r` 0,45, `m` 0,85; Bronze `r` 0,55, Patina `r` 0,9;
  - Leder `r` 0,8, Leinen 1,15.
- Die Inschrift ist eine eigene Zeichnung in römischer Kapitalis: erhabene vergoldete Grate mit Serifen, Wechsel von
  dicken und dünnen Strichen. Es wird keine Schriftdatei benutzt, die frühere Fassung nutzte Inter.

## Klassen

Maße: Tiefe (x) × Breite (y) × Höhe (z) in m, Variante 0. „Stück“ = Anzahl in der Karte. Dreiecke je Variante.
Bögen: `docs/screens/props/level5_3/vorher/<name>.jpg` und `.../final/<name>[_vN].jpg`, Nahansichten
`.../final/<name>_nah.jpg`. Übersichten: `docs/screens/level5_3/props_uebersicht_vorher.jpg` und `..._final.jpg`.

| Klasse | Var. | Befestigung | Maße | Dreiecke vorher | nachher | Stück | Mat. | Was das Modell jetzt hat |
|---|---|---|---|---|---|---|---|---|
| bookcase_aisle | 3 | Wand | 0.52 x 1.01 x 3.16 | 816 | 1866/1874/1732 | 501 | 63 | Regal der Gänge: Rückwand aus Brettern mit Fugen, halbe Pilaster mit Fase und Fuge (der Nachbar ergänzt), Fachböden mit gerundeter Nase, Sockel mit Fußprofil, Kranz mit Zahnschnitt und Karnies, Messingrahmen mit Signaturkarte; sieben Reihen Bücher (s. o.), Staub auf den Kanten, Fach 3 mit Ablage für Funde |
| bookcase_aisle_b | 3 | Wand | 0.52 x 1.01 x 3.16 | 804 | 1766/1834/1858 | 438 | 62 | wie oben, unordentlich: weniger Werke, mehr Stapel, lehnende Bücher und Lücken |
| bookcase_end | 1 | Wand | 0.07 x 1.04 x 3.16 | 444 | 302 | 4 | 7 | Stirnseite: zwei erhabene Kassetten mit Fase und Profilleiste, Kantenleisten mit Fuge, Sockel- und Kranzprofil, Messingrosette, Messingschild mit Signatur „C-VII“ (Atlas), abgegriffene Kanten |
| books_fallen | 3 | Boden | 0.88 x 1.10 x 0.09 | 888 | 282/258/306 | 15 | 21 | 5–7 Bände mit Kapital, Schnitt zwischen den Deckeln und Goldlinien, einer schräg auf einem anderen; ein Buch aufgeschlagen mit gewölbten Seiten und Satzspiegel, eines mit dem Gesicht nach unten (Dach); lose gewellte Blätter |
| candelabrum | 2 | Boden | 0.52 x 0.51 x 1.89 | 2712 | 2943/2157 | 107 | 8 | Bronze: drei geschwungene Beine mit Volute und Ballenfuß, gedrehter Baluster mit Kehlen (Patina), Arme mit Zierranken, Tropfschalen, Tüllen; Kerzen unterschiedlich weit herabgebrannt mit Wachsnasen, Docht und zweischaliger Flamme. v1 drei Kerzen |
| ceiling_arms | 2 | Decke | 1.60 x 1.60 x 0.03 | 197 | 1294/1280 | 90 | 13 | Stuckfeld mit profiliertem Ring (hängt 3 cm herab) und Goldkehle, Eckrosetten, Lorbeerkranz mit Zweig, Schild mit Goldrand: v0 geviert mit Sparren und Sternen, v1 gespalten mit Turm; Krone, Schriftband, abgeplatzte Farbe |
| chandelier_iron | 1 | Decke | 2.04 x 2.04 x 2.06 | 4936 | 8070 | 4 | 9 | weiß gestrichenes Eisen: Baldachin, Kette mit Gliedern, gedrehter Schaft mit Zapfen, zwei Reifen mit S-Armen, 20 Kerzen mit Tropfschalen, Rost in unregelmäßigen Flecken |
| column_tuff | 1 | Boden | 0.96 x 0.96 x 10.20 | 1616 | 3870 | 12 | 3 | toskanisch: Plinthe mit Fase und Ausbrüchen, attischer Wulst, Schaft aus acht Trommeln mit Schwellung, Fugen und leichtem Versatz, Hals mit Astragal, Echinus, Abakus; Poren, Schmutzsaum unten |
| door_old_open | 1 | Durchbruch | 0.87 x 1.00 x 2.25 | 1380 | 924 | 2 | 6 | Eichenzarge mit Fase; Blatt aus fünf Bohlen mit Fugen, Rissen und morscher Unterkante; Querriegel und Strebe hinten, geschmiedete Bänder mit Lilienenden und Nagelköpfen, Rostränder, Ringzug mit Rosette, Schlüsselschild, Kloben |
| inscription_bibliotheca | 1 | Wand | 0.08 x 3.44 x 0.57 | 9132 | 1302 | 2 | 6 | Granitrahmen mit Profil und Gehrung, Marmortafel mit Fase und Patina unten, Haarrisse, Rosetten, Buchstaben als vergoldete Grate (römische Kapitalis) |
| library_wall | 3 | Wand | 1.22 x 1.00 x 9.32 | 3064 | 7136/6830/7088 | 111 | 68 | drei Regalgeschosse wie oben, Galerien bei 3 und 6 m: Nussbaumboden, Stirnbrett mit Goldleiste, Konsole mit Karnies und geschnitzter Schnecke, Geländer (Vierkantstäbe mit Bund, C-Schnecken mit eingerollten Enden), Messinghandlauf; Abschlussgesims mit Zahnschnitt |
| library_wall_lamp | 1 | Wand | 1.22 x 1.00 x 9.32 | 4320 | 8938 | 38 | 72 | wie library_wall, dazu dreiarmige Kerzenhalter aus Messing an beiden Handläufen |
| library_wall_upper | 1 | Wand | 1.22 x 1.00 x 6.85 | 2416 | 5186 | 18 | 60 | zwei Geschosse ab der ersten Galerie (über Durchgängen) |
| painting_renaissance_a | 2 | Wand | 0.08 x 1.80 x 1.25 | 566 | 1170/978 | 26 | 155 | Rahmen mit Profil und Gehrung, Blattgold auf rotem Poliment, Eckpalmetten (v1 Nussbaum, stärker nachgedunkelt); Rundtempel mit Säulen und Laterne, Zypressen, Pinien, ferne Stadt, Himmel im Verlauf, Wolken; fünf Gestalten mit Gewand, Faltenschatten, Mantel, Gürtel, Händen, Gesicht mit Augen, Bärten, Rolle und Buch; Firnis, Craquelé, Fehlstellen |
| painting_renaissance_b | 2 | Wand | 0.08 x 1.80 x 1.25 | 642 | 1006/814 | 36 | 109 | Arkade mit drei Bögen und Pilastern (Licht und Schatten), Landschaft im mittleren Bogen, Gestalt mit Buch auf dem Thron, vier Zuhörer, Schachbrettboden in Fluchtperspektive |
| wallpaper_peel | 1 | Wand | 0.11 x 0.63 x 0.97 | 108 | 232 | 8 | 10 | Fehlstelle mit grauem Putz, Rissen und Lattenwerk, weiße Reißkante, Wasserrand mit Läufen; die Bahn hat sich oben gelöst und hängt nach vorn über (Rückseite mit Kleisterflecken, unten eingerollt), Locken an den Rändern |

Die Studiokabine zeigt die Tür im Durchbruch kaum, weil der Durchbruch dort 2 m tief ist. Darum gibt es zusätzlich
Bilder in der echten Ebene: `docs/screens/props/level5_3/r3/door_old_open_ebene.jpg`.

## Varianten und Karte

Varianten der häufigen Klassen: Regale je 3, Regalwand 3, Kandelaber 2, heruntergefallene Bücher 3, Wappen 2, beide
Gemälde je 2.

**Karte** (`tools/levels/level5_3.py`):
- Am Ende wird je Objekt `variant = vrng.randrange(n)` gezogen, mit eigenem Zufallsgenerator (Seed 53031).
- Die Anordnung bleibt unverändert. Vergleich vorher/nachher: 1481 Objekte, 0 mit anderer Lage, Räume, Türen und
  übrige Felder gleich.
- Verteilung:
  - bookcase_aisle 172/158/171, bookcase_aisle_b 141/130/167, library_wall 34/42/35;
  - candelabrum 53/54, books_fallen 5/5/5, ceiling_arms 39/51;
  - painting_a 15/11, painting_b 24/12.

## Runden

Studio-Bögen mit `tools/qa/propsheet.py`. Dazu eine Hülle (`scratchpad/v13/l53_sheet.py`), die nur Befestigung und
Vorkommen für die Fenster von Level 188 ersetzt. Nahansichten im selben Studio mit Kamera in Modellkoordinaten
(`scratchpad/v13/l53_detail.py`, je Ansicht Licht der Ebene und streifende Lampe). Prüfbögen mit
`tools/qa/reviewsheet.py`.

### vorher (V12-Fassung)
- **Regale und Regalwand**: Buchblöcke als Quader mit projizierter Rücken-Textur, unter der Lampe flach, von der Seite
  verschmiert; Böden und Seiten glatte Quader. Galerie aus Quadern, Stäbe als Zylinder, Zierbögen als Tori; entartete
  Dreiecke in allen drei Regalwänden.
- **Stirnbrett**: Brett mit zwei Quaderfüllungen.
- **Bücher**: sieben Quader mit Papierquader.
- **Kandelaber**: dünne Zylinder, Kugeln als Knäufe, Beine als Quader.
- **Wappen**: flache Farbflächen ohne Stuck, Lorbeer als Rauten.
- **Kronleuchter**: Tori und Zylinder, Rost als eine Kugel, keine Kette.
- **Säule**: Zylinder und Kegel, glatt, ohne Fugen.
- **Tür**: fünf Brettquader, drei Bandquader, Torus als Ring.
- **Inschrift**: serifenlose Schrift (Inter), flache Tafel.
- **Gemälde**: Farbflächen, `painting_renaissance_a` mit umgeklappten Normalen; Rahmen aus vier Quadern.
- **Tapete**: Fleck und flacher Streifen, im Spiel kaum zu sehen.

### Runde r1 (alle Klassen neu, 28 Modelle)
Übersicht `docs/screens/level5_3/props_uebersicht_r1.jpg`, Nahansichten `.../r1/*_nah.jpg`.
- **Regale**: Bücher einzeln gut lesbar. Fachböden glänzten hellgrau, weil die Holztextur fast glatt ist.
  Staubstreifen als hellgraue Leiste.
  - Änderung: Holz `r` 2,0–2,2, Staub dunkler und schmaler.
- **Wappen**: Stuckring unsichtbar (Profil falsch herum, die Ausstattung wird mit Rückseitenausblendung gezeichnet),
  das Schriftband kreuzt den Kranz wie ein Riss, Lorbeer als winzige Rauten.
  - Änderung: Ring umgedreht, Band unter den Schild, paarweise Lorbeerblätter am Zweig.
- **Tapete**: die Bahn verdeckt die Fehlstelle, ihre Vorderseite (Damast) verschwindet in der Wand.
  - Änderung: Neubau mit übergekippter Bahn (Rückseite sichtbar), grauer Putz mit Textur, Läufe des Wasserrands.
- **Gemälde**: Gestalten wie Lutscher (Kopf als Kreis, Körper als Trapez).
  - Änderung: Gestalt neu mit Schuhen, welligem Saum, Schattenseite, Falten mit Lichtkanten, Gürtel, Mantel, Arm mit
    Hand, Gesicht mit Schatten, Augen und Nase, Bärten; Himmel im Verlauf, Wolken, Pinien, ferne Stadt.
- **Marmor und Tür**: Marmortafel spiegelte, Eiche glänzte.
  - Änderung: `r` 2,5 bzw. 2,6–3,0.
- **Tür, Kandelaber oben, Kapitell**: von den Studiokameras nicht erfasst → Nahansichten ab r1.
- **Inschrift, Säule, Kronleuchter, Kandelaber, Bücher am Boden, Galerie**: gut, kein größerer Befund.

### Runde r2 (alle Bögen und Nahansichten)
- **Wappen**: Ring und Kranz sichtbar, Schriftband zu weiß.
  - Änderung: pergamentfarben.
- **Tapete**: Fehlstelle mit Latten, Reißkante, Bahn mit Kleisterflecken liest sich als abgelöste Tapete. Kein Befund.
- **Gemälde**: Gestalten mit Gesichtern, Bärten und Mänteln deutlich besser. Kein Befund.
- **Tür**: Die Nahansichten zeigen im 2 m tiefen Durchbruch des Studios nur Laibung.
  - Änderung: Kameras im Durchbruch und Bilder in der echten Ebene (r3).
- Ein Lauf von r2 kopierte die neuen Modelle nicht in den Spielordner (PATH-Fehler im Rundenskript). Er wurde
  verworfen; seine Nahansichten zeigten den Stand von r1 und sind dort einsortiert. Seither prüft das Skript vor jeder
  Runde, dass jede Datei kopiert ist.

### Runde r3 (alle Bögen, Nahansichten, Türen in der Ebene)
- **Tür in der Ebene** (`r3/door_old_open_ebene.jpg`): Das Blatt liegt geöffnet an der Laibung, Bohlen, Bänder und
  Ring sind unter der Lampe zu erkennen. Abgenommen.
- **Kronleuchter**: Rostfelder in gleichen Abständen wie Streifen.
  - Änderung: Rost in zufälligen Flecken (1–3 Segmente) und dunkle Rostpunkte.
- **Regale**: Funde liegen im Fach 3 bei x = 0,36.
  - Änderung: kein Buch in Fach 3 mehr vor 0,30 (auch keine herausgezogenen Bände).

### Runde r4 (alle Bögen und Nahansichten)
- **Stirnbrett**: zu schlicht neben den Regalen.
  - Änderung: Kantenleisten mit Fuge wie die Pilaster, Messingrosette am Fries.
- **Wappen**: Schriftband noch hell.
  - Änderung: gedeckter.
- Übrige Klassen ohne Befund.

### Runde r5 (alle Bögen und Nahansichten)
- **Stirnbrett** mit Kantenleisten und Rosette, **Wappen** mit gedecktem Band: abgenommen.
- Keine weiteren Änderungen in Level 5.3. Die Änderung dieser Runde betraf Level 188.

### Schlussrunde final (nach Merge von liminal-v13 a8afc3f, neu gebaut)
Alle 28 Bögen, Übersicht `docs/screens/level5_3/props_uebersicht_final.jpg`, Nahansichten `final/*_nah.jpg`.
Gegenüber r5 unverändert, keine neuen Befunde.

## Prüfungen
- `python tools/qa/validate_models.py` für alle eigenen Klassen beider Ebenen (mit Varianten): 47 Modelle, 0 Fehler.
  Im ganzen Ordner haben 77 von 496 Modellen Fehler, alle aus anderen Modulen (Wesen `ent_*`, `door_oversize` u. a.).
- `gameplay_test` (nach dem Merge): 647 Prüfungen, 0 Fehler. „Halt der Ausstattung“: level5_3 1480 Objekte, 0 ohne Halt.
- `audio_test` 106/0, `worldgen_test` 29 658/0, `supply_balance_test` OK, `net_protocol_test` 72/0,
  `net_transport_test` 90/0, `coop_test --quick` 209/0.
- `python tools/qa/room_metrics.py --check level5_3`: Raummetrik OK (Decke min/Median 3,00/7,40 m, Gang 3,0 m,
  verstellt 0/173).
- **Dreiecke der Ausstattung von Level 5.3** (alle 1481 Objekte, propdump): 3,51 Mio. (vorher 1,78 Mio.). Davon die
  eigenen Klassen 3,46 Mio. (vorher 1,73 Mio.).
  - Größte Posten: Regale 1,71 Mio. (939 Stück zu 1,7–1,9k), Regalwand mit Kerzen und oberer Wand 1,21 Mio.
- Karte: nur Varianten geändert (siehe oben).

## Restmängel (ehrlich)
- **Budget**: Die Ebene liegt mit 3,51 Mio. Dreiecken nahe an der Grenze von etwa 4 Mio. Die Regale machen fast die
  Hälfte aus.
- **Bücher**:
  - Rücken sind flach mit gewölbten Normalen. Im Umriss (von der Seite) ist die Rundung nicht zu sehen.
  - Bünde und Linien sind Flächen ohne Erhebung.
  - Titel gibt es nur als farbige Schilder, keine Schrift.
- **Varianten**: Im Gang wiederholen sich drei Varianten je Regaltyp. Wer genau hinsieht, findet nach einigen Metern
  dasselbe Fach wieder.
- **Gemälde**:
  - Sie bleiben bewusst einfache Farbflächenbilder (eigene Motive, kein Bildzitat).
  - Gesichter sind wenige Flächen, Verläufe sind Bänder.
  - Craquelé besteht nur aus einzelnen Linien.
- **Tür**: Im Studio nicht sinnvoll abzubilden (2 m tiefer Durchbruch). Beurteilt mit Bildern in der Ebene und einer
  Blender-Vorschau.
- **Wappen**: Das Schriftband ist ohne Inschrift.
- **Inschrift**: Die Grate sind scharfkantig, ohne eingetiefte Kerbe (keine Booleschen Operationen).
- **Kontaktbögen**: Die Kameras zielen auf die Mitte der Hülle. Kerzen, Kapitelle und Galerien sind nur in den
  Nahansichten beurteilt.
- **Maße**:
  - Die Konsole der Galerie reicht wie vorher 0,53 m unter die Galerie.
  - Die Tapete reicht mit der übergekippten Bahn 0,1 m tiefer als vorher.

## Aufwand
- 27.10.: etwa 2 Stunden (Toolchain, Bau, propdump, Kontaktbögen vorher, Hülle für die Studiokabinen).
- 01.11.: etwa 10 Stunden für beide Ebenen:
  - zwei Merges;
  - Geometrie-Kern und alle 30 Klassen mit Varianten;
  - Karte;
  - Runden r1–r5 und final mit Nahansichten und Bildern in der Ebene;
  - Prüfungen und Dokumente.
- Runden: vorher, r1, r2, r3, r4, r5, final (je alle Bögen beider Ebenen). Dazu Nahansichten ab r1, Bilder in der Ebene
  in r3 und etwa 5 Blender-Vorschauen. Ein Lauf von r2 wurde verworfen (Kopierfehler).
