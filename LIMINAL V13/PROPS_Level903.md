# Ausstattung Level 903 „The Metro“ (V13.3 Teil 1)

Alle Objektklassen der Ebene in Blender neu (`tools/blender/models/models_l903.py`, dort auch der Wagen von Level 37,
siehe `PROPS_Level37.md`). Grundlage `LEVELSPEC_903.md`/`LOOK_903.md` (Wikidot Level 903: endlose U-Bahn-Station,
graue Ziegel, zwei Gleise, links hält ab und zu ein Wagen, rechts ein ruhender Zug über die ganze Länge, alte Wagen als
Unterkünfte und Handelsposten, Treppen immer versperrt oder zugemauert, lange dünne Leuchtröhren, Leuchtschilder in
unbekannter Schrift).

**Bauweise:** eigener Geometrie-Kern im Modul (Prismen und Profile mit Materialien je Fläche, Drehkörper mit
Sichtseite aus der Profilrichtung, Rohre mit Gehrung, Lofts, Rechteck-Schnitt eines Querschnitts für Stirnwände,
Platzieren mit Spiegelung und umgedrehtem Umlaufsinn). Der Wagenkasten ist aus seinem Querschnitt gebaut (Schürze,
Seitenwand mit Einzug über der Brüstung, Dachrundung); Seitenwand und Stirnwände bestehen aus Bändern um echte
Öffnungen (Türen, Fenster), Laibungen sind eigene Flächen. Gebrauchsspuren sind eigene Flächen 1–6 mm vor der
Oberfläche (Bremsstaub, Schmutzfahnen, Wasserläufe, Abplatzer, Rost, Kaugummi, Staub), Holz und Ziegel sind leicht
verformt. Erfundene Schrift: ein fester Zeichensatz aus 18 Zeichen (Stamm + 1–3 Anhänge), keine echten Buchstaben;
Bildzeichen statt Zielnamen (LOOK „keine echte Schrift“). Rauheit und Metall je Material über `r=`/`m=`.

**Engine-Grenzen, die das Design bestimmen:** Glas ist deckend (keine Durchsicht). Die Hauptscheiben der Wagenfenster
sind deshalb heruntergelassen (offen, Oberkante der abgesenkten Scheibe an der Brüstung sichtbar); Spiegelung zeigen die
Kippfenster oben, die Türscheiben und die Stirnfenster. Im ruhenden Zug sind alle Scheiben dunkles, glänzendes Glas
(LOOK: „dunkle Fenster“); im bewohnten Wagen ist die Hauptscheibe offen und die Vorhänge hängen dahinter.

## Klassen

Maße: Tiefe (x) × Breite (y) × Höhe (z) in m, aus dem GLB. Dreiecke je Variante; vorher = V12-Modell. „Stück“ = Anzahl
in der Karte je Variante. Bögen: `docs/screens/props/level903/vorher/<name>.jpg` und `.../final/<name>[_vN].jpg`;
Übersichten `docs/screens/level903/props_uebersicht_<runde>.jpg`; an Ort und Stelle
`docs/screens/level903/props_insitu_vorher.jpg` und `props_insitu_final.jpg`.

| Klasse | Var. | Befestigung | Maße | Dreiecke vorher | Dreiecke nachher | Stück | Materialien |
|---|---|---|---|---|---|---|---|
| metro_car_903 | 1 | Boden (Wagenboden, PLAT + 0,01) | 30,03 × 5,00 × 3,76 (Wagen 19,4 × 3,6; Gleis 30 m) | 3411 | 59106 (Wagen ~40 000, Gleis ~19 000) | 1 | 56: Aluminium blank/matt, Rot, Dichtung, Glas, Staubglas, Innen creme/grau, Boden, Sitz, Edelstahl, Lichtband, Anzeige, Unterboden, Drehgestell, Rad, Lauffläche, Feder, Bremsstaub, Schotter … |
| metro_dormant_a | 2 | Wand (Zugseite) | 0,16 × 6,00 × 3,18 | 320 | 1717 / 1816 | 9 / 5 | 13–16: Lack alt, Streifen blau, Dichtung, Glas, Staubglas, Rost, Roststreifen, Bremsstaub, Ablaufschmutz, Aufkleber … |
| metro_dormant_b | 2 | Wand | 0,16 × 6,00 × 3,18 | 332 | 1747 / 1815 | 4 / 11 | wie a (Sprosse, gesprungene Türscheibe) |
| metro_dormant_coupling | 1 | Wand | 0,74 × 1,00 × 3,12 | 96 | 572 | 10 | 8: Lack, Faltenbalg, Unterboden, Ruß, Kupplung, Bolzen, Schläuche, Kabel |
| metro_dormant_home | 1 | Wand | 0,94 × 6,00 × 3,18 | 884 | 3412 | 1 | 31: wie a, dazu Vorhang/Falte, Licht im Spalt, Decke, Topf, Blätter, Kistenholz, Karton, Klebeband, Fußmatte, Schild, Glühbirne … |
| platform_edge | 3 | Boden (Kante an der Bahnsteigwand) | 0,99 × 0,66 × 0,15 | 304 | 192 / 215 / 220 | 204 / 130 / 58 | 3–6: Kantenstein, Abrieb, Noppenplatte (neu/matt/ersetzt), Fuge, Kaugummi |
| sign_903_a | 1 | Decke | 0,15 × 2,20 × 0,99 | 676 | 480 | 3 | 8: Gehäuse, Leuchtfläche, Linienfeld, Ring, Schrift, Insekten, Gewindestangen, Kabel |
| sign_903_b | 1 | Decke | 0,16 × 2,20 × 0,99 | 604 | 566 | 3 | 8: wie a, Bildzeichen Wagen statt Pfeil |
| sign_903_wall | 2 | Wand | 0,03 × 4,06 × 0,72 | 372 | 288 / 339 | 5 / 3 | 6–7: Emaille blau/weiß, Abplatzer, Rahmen, Bolzen, Rostfahnen, Schmutzfilm (v1) |
| stairs_barricaded | 1 | Boden | 6,31 × 2,64 × 4,30 | 320 | 6390 | 2 | 19: Fliese, Abdeckstein, Terrazzo, Kantenschutz, Gummi, Staub, Handlauf, Ziegel (3), Mörtel, Bretter (3), Pfosten, Nägel, Papier, Band |
| track_metro | 2 | Boden (Gleisbett) | 6,00 × 5,00 × 0,55 | 240 | 9162 / 9407 | 14 / 13 | 15–18: Schotter (3 Töne), Steine (2), Schwelle, Rippenplatte, Spannklemme, Schiene, Schienenkopf, Stromschiene, Abdeckung, Isolator, Kabelkanal; v1 Papier, Dose, Ölfleck |
| tube_long | 3 | Decke | 3,00 × 0,07 × 0,11 | 72 | 560 / 560 / 560 | 53 / 19 / 4 | 7–8: Aluprofil, Seile, Vorschaltgerät, Fassungen, Röhre (neu/alt/tot), Endkappen, geschwärzte Enden |

**Budget:** Ausstattung der Ebene vorher 153 000 Dreiecke, nachher etwa 511 000 (Gleis 250 000, Bahnsteigkante 80 000,
Wagen 59 000, ruhender Zug 61 000, Röhren 43 000, Treppen 13 000) – weit unter 4 Mio. Die Bahnsteigkante ist trotz
echter Noppen sparsamer als vorher (192–220 statt 304 Dreiecke, 80 000 statt 119 000 für 392 Stück): Noppenfeld als
„Eierkarton“ (vier Dreiecke je Noppe), flach schattiert.

**Maße und Befestigung gegenüber V12:**
- `platform_edge` war spiegelverkehrt: Die Kante lag 1 m im Bahnsteig (Blender +y ist im Spiel -y). Jetzt liegt der
  Kantenstein an der Bahnsteigwand und steht 3 cm über (Grenzen lo/hi in y gespiegelt, gleiche Größe).
- `metro_car_903` trägt das Gleis unter sich (x -14,5 … 15,5): Die Karte lässt die Gleisstücke 86–116 aus, der Wagen
  schwebte über dem Hallenboden. Schwellenraster wie `track_metro` (Stoß auf Stoß bei 86 und 116). Die Gleismitte liegt
  unter der Wagenmitte; `track_metro` hat deshalb seine Gleismitte bei y = -0,5 (Welt y = 5,0 wie der Wagen).
- `stairs_barricaded`: Stufen nicht mehr unter dem Bahnsteig (lo z 0 statt -1,2); die Barrikade steht 0,15 m vor der
  Treppe (lo x -0,21), damit sie nicht im Schatten des unsichtbaren Treppenblocks liegt.
- `metro_dormant_coupling` reicht 0,62 m hinter die Zugseite (in den unsichtbaren Zugblock), damit die Lücke zwischen
  den Wagen Tiefe hat; `metro_dormant_home` 0,32 m (Licht hinter den Vorhängen).
- Wandseite des fahrenden Wagens: Türen geschlossen (die Karte sperrt diese Seite; Türen öffnen nur zum Bahnsteig).

**Varianten** (Kartenskript `tools/levels/level903.py`, letzter Abschnitt: eigener Zufall nach allen Platzierungen,
Anordnung unverändert – geprüft: alle 553 Objekte gleich bis auf `variant`, Räume/Übergänge/Spawns gleich):
Bahnsteigkante gepflegt / abgetreten (Riss, Kaugummi, matte Platte) / ausgebessert (neue Platte, Abplatzer an der Nase);
Röhre neu / gealtert (geschwärzte Enden, wärmeres Licht) / eine Röhre tot; Gleis mit anderer Steinlage, Ölfleck und
Abfall; Stationstafel stärker verwittert (Rostlauf, Schmutzfilm, mehr Abplatzer); Zugabschnitte a und b je stärker
verschmutzt (Staub auf den Scheiben, Laufspuren, Aufkleberreste). a und b selbst unterscheiden sich in Fenstern (Kippflügel
gegenüber Mittelsprosse), Türscheibe und Aufklebern; Streifen und Lack sind gleich, weil a und b im selben Wagen
abwechseln.

## Was gebaut ist (je Klasse)

- **metro_car_903** (fahrender Wagen, 18 m): Kasten aus dem Querschnitt (Schürze, Seitenwand, Einzug über der
  Brüstung, Regenrinne, Dachrundung), Riffelung unten, roter Zierstreifen mit Abplatzern an den Türkanten, dünne Linie;
  je Seite drei Türportale mit Gummi, Führungsschienen oben/unten, Schwelle mit gelber Kante; Türblätter außen
  schiebend (Bahnsteigseite offen, Wandseite zu) mit Fenster, Dichtung, Gummilippe, Trittblech, Warnaufkleber,
  Griffspur, Griff innen; sechs Fenster je Seite mit gerundeten Ecken, Dichtung, Kippfenster (Glas, Rahmen, 9° gekippt,
  Staubkante), Kämpfer, Oberkante der abgesenkten Scheibe; Schmutzfahnen unter den Fensterecken, Wasserläufe von der
  Rinne, Bremsstaub unten. Stirnseiten: Wand aus dem Querschnitt mit Stirntür (Fenster, Griffe, Trittblech), zwei
  Stirnfenster, Scheibenwischer, Eckpfosten, Rammschutz mit Rippen, Kupplung (Zugeinrichtung, Schaft, Kopf, Kegel,
  Trichter, Kontaktkasten), Luftschläuche mit Hähnen, Scheinwerfer vorn an, Schlusslichter hinten an, Wagennummer in
  erfundenen Zeichen, Zielanzeige außen. Innen: Gummiboden mit Laufspuren, Längsbänke (Schale, Heizungsblende mit
  Schlitzen, Polster je Sitz, einzelne abgewetzt oder geflickt), Trennwände mit Haltestangen, Haltestangen über den
  Bänken mit Haltern und Halteschlaufen, Mittelstangen in den Türräumen, Decke mit zwei Lichtbändern und Lüftern,
  Werbekarten im Bogen, Brüstungsleiste, Zielanzeigen an beiden Enden (vorn Wellen = Level 37, hinten Flugzeug =
  Level 36, Statuszeile), Notbremsgriffe, Feuerlöscher. Unterboden: Langträger, Stromrichter mit Kühlrippen,
  Batteriekasten, zwei Luftbehälter mit Böden und Bändern, Bremswiderstand mit Lamellen, Kompressor, Kabelkanäle,
  Hauptluftleitung. Zwei Drehgestelle: Radsätze (Radprofil mit Spurkranz und Lauffläche, Achse), Achslager mit Deckel
  und Schrauben, Lenker, je zwei Schraubenfedern, Rahmen mit Hornführungen und Mittelabsenkung, Querträger, Luftfedern,
  Drehzapfen, Fahrmotoren, Getriebe, Klotzbremsen, Stromabnehmerbalken aus Holz mit Schleifschuh unter der
  Stromschiene, Kabel. Darunter das Gleis (siehe track_metro), unter dem Wagen vereinfachte Befestigungen.
- **metro_dormant_a/b** (ruhender Zug, 6-m-Abschnitt): älterer Wagen aus eigenem Profil, grauer Lack, blauer Streifen
  und Linie (durchgehend), Zierleiste, Blechstöße mit Nietreihen, zwei Fenster mit gerundeten Ecken, Dichtung, dunklem
  Glas und Staubkante (a: Kippflügel, b: Mittelsprosse), Doppeltür mit Fenstern, Trittblech, Dichtung, Führung
  (b: Türscheibe gesprungen), Regenrinne mit Ablaufschmutz, Rostfahnen unter den Fensterecken, Rostinseln,
  Bremsstaub, verblasste Aufkleber mit erfundenen Zeichen, Wagennummer.
- **metro_dormant_coupling**: Wagenenden beider Wagen gerundet bis in die Stirnwände, Faltenbalg (Zickzack, 18
  Falten) mit Rahmen, dunkler Durchgang dahinter, Kurzkupplung mit Bolzen, zwei Luftschläuche und Steuerkabel im Bogen.
- **metro_dormant_home** (bewohnter Abschnitt, Wiki: Wagen als Unterkünfte und Handelsposten): wie a, aber
  Hauptscheiben offen, Vorhänge mit Falten und Blende, warmes Licht im Spalt, Kippflügel offen, Tür einen Spalt auf
  mit Licht und Decke; davor Pflanze im Tontopf (Bogenhanf-Blätter), zwei Lattenkisten, Karton mit Klebeband,
  Fußmatte, handgemaltes Schild an Drähten unter der Rinne, Kabel aus dem Türspalt am Wagen entlang, Glühbirne im
  Drahtkorb.
- **platform_edge**: Kantenstein mit gerundeter Nase (3 cm Überstand), Abrieb an der Kante, zwei gelbe
  Noppenplatten je Meter mit Fuge; Varianten siehe oben.
- **sign_903_a/b**: gefastes Aluprofil, Leuchtfläche beidseitig, Linienfeld mit Ring, erfundene Zeichen, Pfeil (a)
  oder Bildzeichen Wagen (b), Gewindestangen mit Deckenplatten und Muttern, Laschen, Kabelschlauch, tote Insekten.
- **sign_903_wall**: Emailleschild (glänzend) mit weißem Rand und Stationsnamen in erfundenen Zeichen (auf allen
  Tafeln gleich), L-Rahmen, sechs Abstandsbolzen, Abplatzer an Kanten und Ecken, Rostfahnen unter Bolzen.
- **stairs_barricaded**: Brüstungswände mit Fliesen und Abdeckstein bis zur Decke, Sockelschmutz, Terrazzostufen mit
  Kantenschutz und Gummieinlage, Staub in den Ecken, Handläufe auf Haltern mit Rosetten, oben eine Ziegelwand aus
  einzelnen, leicht verschobenen Steinen (drei Ziegelsorten, Mörtel quillt); unten Barrikade: zwei Pfosten mit
  Stahlwinkeln, neun Bretter unterschiedlicher Breite, leicht schief, mit je vier Nägeln, zwei Diagonalbretter,
  krummer Nagel, Zettel mit erfundenen Zeichen, Absperrband, Schutt hinter den Brettern.
- **track_metro**: Schotterbett als Höhenfeld mit Steinbuckeln (Ölspur zwischen den Schienen, Rost an den Schienen,
  dunkle Inseln), Einzelsteine, Spannbetonschwellen (unter den Schienen breiter, Mitte eingeschnürt), Rippenplatten,
  W-Spannklemmen, Schwellenschrauben, Schienen UIC-54-Profil (Kopf blank, Steg und Fuß rostig), Stromschiene von unten
  bestrichen mit Abdeckung auf Isolatorböcken, Kabelkanal mit Deckeln und Grifflöchern an der Wand (ein Deckel
  verschoben).
- **tube_long**: Deckenrosetten, Seile mit Haltern, schmales Aluprofil mit Endkappen, Vorschaltgerät, vier
  Fassungen, zwei T8-Röhren mit Endkappen und geschwärzten Elektrodenenden.
## Runden

Je Runde: Kontaktbögen aller Klassen (`tools/qa/propsheet.py`, Studio), kompakter Prüfbogen 50 cm (`reviewsheet.py`) und
20 Bilder an Ort und Stelle in der echten Ebene (Wagen an der Tür, schräg, innen, Spalt, Ende; Zug lang/nah/bewohnt,
Kupplung; Kante, Gleis; Treppe; Röhren; Schilder; je eine Ansicht nur mit Lampe). Große Objekte (Wagen 18 m, Zug als
Wand) lassen sich im Studio nur grob beurteilen, darum zählen bei ihnen die Bilder an Ort und Stelle.

### Vorher (V12-Fassung)
- metro_car_903 / metro_car_37: Kasten aus Quadern, Fenster schwarze Rechtecke ohne Rahmen, Türblätter als Platten,
  Drehgestell ein schwarzer Quader mit Zylinderrädern, Stirnseiten leer (kein Führerstand, keine Lampen, keine
  Kupplung), Innenraum Kästen; unter dem Wagen kein Gleis (die Karte lässt 86–116 aus), in Level 37 schwebt das
  Fahrwerk über dem Wasser.
- metro_dormant_a/b: flache dunkle Platte, Fenster schwarze Flächen, Streifen und Aufkleber farbige Rechtecke, keine
  Tür- und Fensterrahmen.
- metro_dormant_coupling: sieben schwarze Latten.
- metro_dormant_home: Vorhänge als Streifen auf der Außenhaut, Schild flach, Pflanze aus Stäben.
- platform_edge: gelbe Platte mit zehn Zylindern und ein Betonstreifen; die Kante lag spiegelverkehrt 1 m im
  Bahnsteig (Blender +y ist im Spiel -y), 304 Dreiecke × 392 Stück = 119 000.
- sign_903_a/b: Leuchtkasten mit Strich-Quadern, Pendel als Fäden.
- sign_903_wall: blaue Platte mit Strich-Quadern.
- stairs_barricaded: Stufen liefen unter den Bahnsteig (bis -1,2 m), die „zugemauerte“ Treppe wirkte als Ziegelturm,
  vier Bretter vor einem Block.
- track_metro: Quader-Schwellen und Kastenschienen auf schwarzer Platte, kein Schotter.
- tube_long: Leuchtstab an einer Leiste.

### Runde r1 (erste Fassung aller Klassen)
- Ruhender Zug: Lack mit Textur `painted_metal_grey` wirkte als durchgerosteter Stahl (dunkel, orange Flecken) –
  Lack ohne Textur, Rost nur als Inseln und Fahnen.
- Bewohnter Wagen: Oberkante der Vorhänge als Zacken vor dem Licht – Vorhänge bis oben, Blende mit Falten davor.
- Gesprungene Türscheibe: Sprünge leuchteten weiß – dünner, Staubglas statt Emaille.
- Kupplung: Falten als schwarze Gitterstäbe – Balg als Zickzackfläche (18 Falten, dunkles Gummigrau).
- Wagen: Bremsstaub als regelmäßige braune Welle – unregelmäßig, flacher, an den Türen unter der Schwelle; Sitze fast
  schwarz (Teppichtextur) – Stoff ohne Textur, blau; Lichtbänder überstrahlt (1,4 → 0,8); Griffspur an der Tür als
  Raute – weicher Fleck.
- Bahnsteigkante: Noppen nicht zu sehen (glatt schattiert) – Facetten flach, 7,5 mm hoch: im Streiflicht klar.
- Treppe: Bretter zu orange – entsättigt; Ziegel zu bunt – überwiegend grau.
- Varianten eingeführt: Bahnsteigkante 3, Röhre 3, Gleis 2, Stationstafel 2, Zug a/b je 2 (Kartenskript am Ende,
  eigener Zufall, Anordnung unverändert).

### Runde r2
- Ruhender Zug: jetzt grau mit blauem Streifen, wirkt aber frisch und bläulich – Lack dunkler und neutraler,
  Ablaufschmutz unter der Regenrinne.
- Sitze: jetzt zu kräftig blau – gedämpft; Lichtbänder 0,8 → 0,55.
- Bremsstaub: in Level 37 (Wagen frei über dem Wasser) noch zu braun – dunkelgrau, Oberkante tiefer.
- Validierung: Scheibenwischer der vorderen Stirn ohne Dicke, Türgriffe der Stirntür mit 90-Grad-Knick (umgeklappte
  Normalen), Abdeckstein der Treppe überschnitt sich selbst, Bremsstaub des Zuges faltete sich – alle behoben.
- Lampenansichten an Ort und Stelle neu gesetzt (waren zu weit weg).

### Runde r3 (mit neu gebautem Spiel, Stand liminal-v13)
- Ruhender Zug: Ablaufschmutz unter der Rinne noch braun – neutrales Dunkelgrau.
- Lampenansichten an Ort und Stelle: Wagenseite (Level 903 und 37) und Bretter der Treppe blieben schwarz. Ursache: die
  unsichtbaren Kollisionsblöcke der Karte werfen Schatten (Wagenwände y 6–7 bzw. 7–8, Treppenblock ab x = 58); die
  Bretter standen 14 cm in diesem Block. Barrikade vor die Blockkante gesetzt (Pfosten mit Stahlwinkeln an den
  Stirnseiten der Brüstungen, Bretter, Diagonalen, Zettel und Band davor). Für die Wagenseite im Modell nicht lösbar.
- Außenanzeige der Stirn: Bildzeichen zu klein – um ein Viertel größer.
- gameplay_test 647 Prüfungen, 0 Fehler; Halt der Ausstattung level903 553/0, level37 2/0.

### Runde r4
- Barrikade: im Streiflicht jetzt hell, Bretter mit Maserung, Nägel, Zettel mit erfundenen Zeichen, Absperrband lesbar.
- Wagen innen: wirkte leer – Halteschlaufen (Riemen, Griffring) an den mittleren Haltestangen und Werbekarten im
  Bogen über den Fenstern (Rahmen, farbiger Grund, erfundene Zeichen, Bildform; Wiki: Werbung in unbekannten Sprachen,
  keine echten Marken).
- Übrige Klassen ohne neuen Befund (Bögen r3 und r4 gleich bis auf Barrikade, Ablaufschmutz und Anzeige).

### Runde r5
- Wagen innen: Halteschlaufen und Werbekarten wirken; die Griffringe hingen aber auf Augenhöhe (1,56–1,67 m) –
  Riemen kürzer, Ringe 1,67–1,77 m.
- Alle übrigen Klassen ohne neuen Befund; Validierung, Halt und Raummetrik grün.

### Schlussrunde final
- Bögen aller 20 Klassen/Varianten, Prüfbogen `docs/screens/level903/props_pruefbogen_final.jpg`, Übersicht
  `props_uebersicht_final.jpg`, an Ort und Stelle `props_insitu_final.jpg`. Keine neuen Befunde, die ich im Modell
  noch lösen kann (Restmängel unten).

Prüfbögen der Runden (50 cm im Licht, 50 cm von der Seite, 50 cm nur Lampe):
`docs/screens/level903/props_pruefbogen_r1.jpg` … `_r5.jpg`, `_final.jpg`; Übersichten
`docs/screens/level903/props_uebersicht_vorher.jpg`, `_r1.jpg` … `_r5.jpg`, `_final.jpg`.

## Prüfungen
- `python tools/qa/validate_models.py` mit allen 13 Klassen: 21 Modelle (mit Varianten), 0 Fehler. Gesamtlauf 487
  Modelle, 86 mit Fehlern – keines davon aus Level 903/37 (Wesen und andere Ebenen).
- `gameplay_test` (Spiel neu gebaut auf Stand liminal-v13): 647 Prüfungen, 0 Fehler. „Halt der Ausstattung“: level903
  553 Objekte, 0 ohne Halt; level37 2, 0 ohne Halt; gesamt 10 506, 0 ohne Halt.
- `python tools/qa/room_metrics.py --check` (alle Ebenen): Raummetrik OK (903: 1 Raum, verstellt 0/0).
- Abschluss nach `git merge liminal-v13` (e8fcf28), neu gebaut: gameplay 647/0 (Halt gesamt 10 506, 0 ohne Halt),
  audio 106/0, worldgen 29 658/0, Koop 209/0, supply OK, net 72/0, Transport 90/0. Ein früherer Koop-Lauf meldete
  einmal 1 Fehler im Szenario „schlechtes Netz“ (Abweichung Host/Gast nach dem Anhalten); der Wiederholungslauf auf
  demselben Code war fehlerfrei – zeitabhängig, nicht durch Modelle oder Karte bedingt.
- Budget (propdump): Level 903 511 429 Dreiecke Ausstattung, Level 37 43 284.

## Restmängel (ehrlich)
- **Schatten der unsichtbaren Kollisionsblöcke:** Die Wagenwände der Karte (Level 903 y 6–7, Level 37 y 7–8) sind
  unsichtbare Blöcke, werfen aber Schatten. Die Außenseite des Wagens zur Bahnsteigseite bekommt deshalb kein direktes
  Licht der Röhren und keines der Taschenlampe, nur Umgebungslicht. Im Licht der Ebene sieht das stimmig aus, im
  Lampenlicht bleibt die Wagenseite dunkel. Ebenso liegen Stufen und Ziegelwand der Treppe im Treppenblock (nur
  Umgebungslicht); die Barrikade steht davor. Lösung nur in Karte oder Engine (Blöcke ohne Schatten).
- **Glas ist deckend:** Hauptscheiben des Wagens offen (heruntergelassen), damit der Innenraum sichtbar ist. Die
  Fenster des ruhenden Zuges sind dunkles Glas ohne Innenraum.
- **Gleis unter dem Wagen im Wagenmodell:** Die Karte lässt die Gleisstücke 86–116 aus; das Gleis gehört deshalb zum
  Modell `metro_car_903` (59 106 Dreiecke, davon ~19 000 Gleis; der Wagen allein liegt bei ~40 000). Würde die Karte
  später dort `track_metro` setzen, lägen zwei Gleise übereinander.
- **Studio:** Wagen (18–30 m) und Zugfassade sind im Studio nur grob zu beurteilen, die Lampenzeile des Wagens bleibt
  dort schwarz (Lampe zu weit). Beurteilt an Ort und Stelle.
- **Würfelprojektion:** Holzmaserung der Bretter läuft je nach Lage quer; Schotter hat keine eigene Textur
  (`concrete_rough` mit Höhenfeld und Einzelsteinen).
- **Noppen:** im Licht der Ebene kaum sichtbar, im Streiflicht klar (Raster 8 × 10 cm, 7,5 mm hoch, Eierkarton statt
  Kegelstümpfe – Dreiecksbudget; echte Noppenplatten sind dichter).
- **Ruhender Zug:** Dach und Innenraum fehlen (Fassade an der Blockkante; vom Bahnsteig nicht einsehbar). Über dem Zug
  sieht man die Hallenwand.
- Die Werbekarten und Aufkleber zeigen erfundene Zeichen und einfache Formen, keine Bilder.

## Aufwand
- Etwa 8 Stunden Agentenzeit (27.10. ca. 4 h: Analyse, Vorher-Bögen, Wagen und Gleis; 01.11. ca. 4 h: übrige
  Klassen, Varianten, Runden r1–r5 und final, Tests, Dokumente).
- Runden: vorher, r1–r5, final (7 Kontaktbogen-Läufe über alle Klassen, je mit 20 Bildern an Ort und Stelle in
  Level 903 und 6 in Level 37), dazu Blender-Vorschauen zwischen den Runden.
