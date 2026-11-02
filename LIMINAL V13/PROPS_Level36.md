# Ausstattung Level 36 „Timeless Airport“ (V13.3 Teil 1)

Alle Objektklassen von Level 36 in Blender neu (`tools/blender/models/models_l36.py`), dazu die fünf bisher fehlenden
Modelle der ergänzten Wiki-Bereiche: Sicherheitskontrolle (`xray_scanner`, `security_gate`, `tray_stack`, alles außer
Betrieb) und Lounge (`lounge_chair`, `lounge_table`). Grundlage sind `LEVELSPEC_36.md` und `LOOK_36.md` (Wikidot
Level 36): ein Flughafen um vier Uhr morgens, für immer – hell, ordentlich, sauber, und gerade deshalb fremd. Alles
funktioniert und nichts geht: Tafeln zählen, Bänder stehen, Uhren haben keine Zeiger, Schalter sind geschlossen.

**Bauweise**
- Eigener Geometrie-Kern im Modul: Teile je Material in einem bmesh (`Kit`), Bausteine Kasten mit Fase, Prisma nach
  Umriss, Ringe-Loft, Drehkörper, Profil entlang Pfad (Handlauf, Schürze, Keder, Gurte, Reißverschlüsse), Platten und
  Flecken (0,3–1,5 mm über der Fläche), Linsenkopfschrauben, Lochraster.
- `Kit.finish()` teilt alle Flächen selbst in Dreiecke und entfernt flächenlose Dreiecke und Splitter (Höhe < 0,02 mm):
  keine entarteten Dreiecke mehr im Export (vorher u. a. am Ausgangsschild).
- Schrift ist Geometrie aus Inter (wie bisher, keine Atlas-Schrift in Level 36), bereinigt; Bildzeichen (Flugzeug, Koffer,
  Pfeil, Zug, Läufer, Mann/Frau, Herzschrittmacher) als eigene Vielecke.
- Gepäck als Baukasten für Band, Fundsachen und C32: Hartschalenkoffer (Rillen, Reißverschluss rundum, Griffe,
  Teleskopgriff, vier Doppelrollen, Anhänger mit Papierkarte), Stoffkoffer (Paspel, Vortasche mit Reißverschluss,
  Gurtgriffe, Rollen, Füße), Reisetasche (flacher Boden, Endpaspel, Gurte, Griffe, loser Schultergurt), Rucksack
  (Fronttasche, Reißverschlussbögen, Träger), Karton (Laschenfuge, Klebeband, Schnur, Etikett, teils feucht).
- Rauheit/Metall je Material (`r=`/`m=`): gebürsteter Edelstahl r 0,62 m 0,85, matter Edelstahl r 1,0, Chrom r 0,35
  m 0,95, eloxiertes Alu r 0,85, Gummi r 1,25 (abgerieben 1,05), Hartschale r 0,55–0,65, Stoff r 1,35–1,45, Leder r 0,75,
  Schichtstoff (Kunststoff) r 0,8, Holz r 1,8 (mit 0,85 spiegelte die Holzstruktur hell), Glas r 0,25.
- Sauberkeit nach LOOK_36: keine Lack-Fotostrukturen mit Abplatzern (Runde 1 verworfen), Gebrauchsspuren nur als
  Schuhabrieb am Sockel, Laufspur auf Paletten, Gummiabrieb, Kalkrand am Brunnen, Staub am Rollgitter.
- Glas ist im Spiel undurchsichtig: Schaufenster ohne Scheibe (Dichtungen, Glasleiste, Spiegelstreifen), sonst getöntes
  Glas (Balustrade, Türfenster), satiniertes Glas als Kunststoff.

## Klassen

Maße: Tiefe (x) × Breite (y) × Höhe (z) in m, Variante 0. Dreiecke je Variante. „Stück“ = Anzahl in der Karte.
„Mat.“ = Materialien im Modell. Bögen: `docs/screens/props/level36/vorher/<name>.jpg` und
`docs/screens/props/level36/final/<name>[_vN].jpg` (neue Klassen nur final).

| Klasse | Var. | Befestigung | Maße | Dreiecke vorher | Dreiecke nachher | Stück | Mat. | Was das Modell jetzt hat |
|---|---|---|---|---|---|---|---|---|
| carousel_36 | 1 | Boden | 11.99 x 3.72 x 1.92 | 1360 | 19844 | 2 | 26 | Plattenband in Stadionform: 144 überlappende, schräge Gummilamellen (Geraden und Fächer in den Bögen) mit Fase, dunkler Unterbau, Edelstahlschürze mit Deckkante, Sockel, Gummi-Prallleiste, 20 Fugen mit Schrauben, Gummiabrieb; Mittelinsel mit Firstabdeckung, Stoßfugen und Gummiführung; Einlauf mit Haube, Wangen und Gummischürze aus Streifen (leicht verschwenkt); Not-Aus an beiden Seiten; Mast mit Anzeige „METRO 903“, Zug-Bildzeichen, „GEPAECK“; sechs Gepäckstücke |
| luggage_pile | 3 | Boden | 3.10 x 0.80 x 2.20 | 2004 | 21416/17679/15964 | 8 | 25–27 | Fundsachen-Regal: verzinkte Winkelstützen mit Lochung (beide Seiten), gekantete Fachböden mit Schrauben, Kreuzverbände, Fußplatten, Etiketten, Schild FUNDSACHEN; Koffer, Taschen, Rucksäcke, Kartons mit Anhängern. v1 mit Lücken, v2 mehr Kartons |
| checkin_counter | 3 | Boden | 2.56 x 3.00 x 2.30 | 603 | 3831/4437/4081 | 18 | 22–27 | Tresen mit Ablage (runde Kante), Alu-Band, Edelstahl-Trittblech, Arbeitsplatte, Rechner, Bildschirm, Tastatur, Bordkarten- und Anhängerdrucker; Gepäckwaage mit Rippenband, Rolle, schräger Führung, Gummivorhang, Gewichtsanzeige „0.0 kg“, Aufkleber MAX 32 kg; Trennwand aus Schichtstoff und satiniertem Glas; Nummernschild „36 CHECK-IN“ am Mast; Sammelband hinter der Reihe (deckt den Block, Stoßfugen zum Nachbarn). v0 offen (Kundenanzeige QX 0903), v1 geschlossen (Anzeigen aus, Aufsteller GESCHLOSSEN, Gurtpfosten), v2 Anzeige „- - - -“, vergessener Koffer auf dem Sammelband |
| departure_board | 1 | Wand | 0.13 x 4.10 x 2.17 | 8168 | 7624 | 3 | 13 | Gehäuse mit Wandschienen und Lüftungsschlitzen, Alu-Rahmen, vier Bildschirme mit Stoßfugen, Kopfband, Spaltenköpfe, Zeilenbänder, Flüge wie bisher (QX 0903 BOARDING 1000 grün), stehende Uhrzeit 04:00, Statusleuchte, Leitungsstutzen |
| bta_booth | 1 | Boden | 3.10 x 2.02 x 3.51 | 428 | 10519 | 1 | 20 | Tresen mit runden Enden, Sockel, Platte mit Kante, violettes Leuchtband und Leuchtfeld mit Schriftzug BACKROOMS TRAVEL AGENCY, Klingel, Prospektständer mit Heften, Bildschirm, Formulare, Stifte; Sideboard und Lagerschrank (decken den Block), Rückwand mit Profilen, Füßen, Zielliste (LEVEL 11/186/903/36.1), Rückseite mit Fugen und Revisionsklappe; Leuchtkasten BTA beidseitig auf Masten |
| travelator | 1 | Boden | 11.92 x 1.40 x 1.04 | 1500 | 6958 | 3 | 13 | Palettenband mit Längsrillen, Palettenfugen und gelben Randmarken, Kammplatten mit 50 Zähnen, Flurplatten mit Rillen und Schrauben, Sockelblech mit Bürstenleiste, schräges Innendeck, Außenverkleidung mit Fugen, getönte Glasbalustrade mit Rundung und 26 Klemmen, Gummihandlauf mit Umlenkung an beiden Enden, Not-Aus, Fahrtrichtungsanzeige, Laufspuren |
| travelator_stopped | 1 | Boden | 12.58 x 1.40 x 1.04 | 1524 | 9258 | 1 | 13 | wie travelator; zwei Paletten fehlen (Kette, Rollen, Welle sichtbar), rote Anzeigen an beiden Enden, Warnaufsteller mit X und AUSSER BETRIEB |
| sign_36_a / sign_36_b | 1 / 1 | Decke | 0.17 x 3.00 x 1.26 | 2150 | 2208 | 17 / 6 | 8 | Alu-Strangprofil mit runden Kanten, Endkappen mit Schrauben, beidseitige Leuchtfelder gelb auf Anthrazit, Pfeile außen, Flugzeug und Koffer, Trennstrich, Gewindestangen mit Rosetten, Abhängern, Spannschloss, Zuleitung. a: Gates und Gepäck auf beiden Seiten in derselben Welt-Richtung; b: genau umgekehrt (widersprüchlich) |
| sign_36_empty | 1 | Decke | 0.21 x 3.02 x 1.27 | 148 | 854 | 1 | 9 | gleiches Gehäuse, Felder leer, eine Abdeckung herausgebrochen (Splitter, tote Röhren mit Fassungen), eine Stange abgerissen, Kasten hängt schief, Kabel lose |
| exit_sign_36 | 1 | Wand | 0.07 x 1.20 x 0.43 | 220 | 354 | 1 | 5 | Alu-Gehäuse mit runden Kanten auf Wandwinkeln, grünes Leuchtfeld, weißer Läufer, Tür, EXIT, Pfeil, Leitungsrohr mit Dose; keine entarteten Dreiecke mehr |
| storefront_a/b/c | 1 je | Wand | 0.98 x 6.00 x 3.80 | 5822/5357/5418 | 4167/4696/6247 | 5/2/2 | 22–26 | Pfeiler mit Fuge, Sockel mit Trittblech, Pfosten-Riegel-Fassade aus Strangprofil mit Glasnut und Dichtungen, Schaufenster ohne undurchsichtige Scheibe (Spiegelstreifen), Rahmentür mit Stoßgriffen, Mittelriegel, Trittblech, Bodenschiene, Türschließer, Aufkleber „00:00 - 00:00“, Rollladenkasten mit Führungsschienen, Leuchtkasten mit erhabenen Buchstaben, Innenlicht. a Zeitungen: schräge Fächer mit Zeitschriften (Titelband), Zeitungsstapel mit Band, Kasse; b Kaffee: Theke mit Glasvitrine und Gebäck, Siebträgermaschine, Becherstapel, Mühle, Preistafel „--,--“, Stehtisch; c Mode: Kleiderstange mit Bügeln und Kleidung (Schultern, Ärmel, Knopfleiste), Regal mit Stapeln, Schuhe, Spiegel |
| storefront_closed | 1 | Wand | 1.01 x 6.00 x 3.80 | 408 | 4549 | 4 | 18 | wie oben ohne Auslage; Rollgitter aus 38 Lamellen mit Lochreihen, Endleiste mit Griffmulden, Vorhängeschloss, Zettel GESCHLOSSEN mit Klebeband, Staub unten |
| seats_row | 3 | Boden | 0.66 x 3.17 x 0.94 | 972 | 6958/6864/6931 | 24 | 9–11 | Traverse aus Rechteckrohr mit Endkappen, zwei Alu-Guss-Füße mit Gleitern, vier Sitzschalen aus gebogenem Lochblech (Lochung auf Sitz und Lehne), Halter, Polster (blau, teils abgewetzt), fünf Armlehnen aus Guss mit PU-Auflage und Abrieb, Tischplatte (genau 0,65 m) auf Kragarm. Tisch jetzt auf der Seite, auf der die Karte die Funde ablegt. v1 ein Polster fehlt, Zeitung; v2 zwei Sitze ohne Polster, aufgerissenes Polster, Jacke über der Lehne |
| gate_counter | 1 | Boden | 0.82 x 2.10 x 1.44 | 144 | 1570 | 5 | 13 | Pult mit runden Ecken, Sockel, Alu-Band, Leuchtfeld BOARDING, Ablage, Bildschirm, Tastatur, Bordkartenleser mit rotem Fenster, Mikrofon am Schwanenhals (Durchsagen), Telefon, Papier, Schuhabrieb |
| gate_sign_c30–c34 | 1 je | Wand | 0.11 x 2.00 x 0.80 | 481–569 | 552–616 | 1 je | 6 | Gehäuse mit runden Kanten auf Wandwinkeln, Alu-Rahmen, gelbes Feld mit Flugzeug und GATE, Nummer gelb auf Anthrazit, Schrauben |
| gate_door_closed | 1 | Wand | 0.16 x 1.90 x 2.74 | 372 | 1990 | 4 | 12 | Stahlzarge mit Falz und Dichtung, zwei Flügel mit Drahtglasfenstern (getönt), Panikstangen, Bänder, Obentürschließer, Trittbleche, Kartenleser, rote Statusleuchte im Gehäuse, Schild |
| gate_door_boarding | 1 | Wand | 0.16 x 1.90 x 2.74 | 360 | 1322 | 1 | 11 | Zarge, offene Flügel in der Laibung, helles Inneres der Brücke mit Bodenstreifen, Schwelle, Kartenleser grün, grüne Statusleuchte |
| clock_36 / clock_36_b | 1 / 1 | Wand | 0.08 x 0.64 x 0.64 | 456 / 480 | 1056 / 1074 | 3 / 2 | 3 / 4 | Bahnhofsuhr: Gehäuse als Drehkörper mit Wulst, Zifferblatt mit 60 Strichen, Mittelachse; b mit stehenden Zeigern und roter Kelle; ohne b fehlen die Zeiger |
| water_fountain | 1 | Wand | 0.46 x 0.58 x 1.00 | 228 | 828 | 2 | 7 | Rückplatte mit Schrauben, Becken als Loft (Rand, Mulde zum Ablauf), Ablaufsieb, Sprudler mit Bügel, Druckknopf, gerundete Verkleidung mit Lüftung, Sockel, Kalkrand, Wasserrest |
| restroom_sign | 1 | Wand | 0.06 x 0.90 x 0.50 | 244 | 585 | 2 | 3 | Platte mit runden Kanten auf vier Abstandshaltern, Bildzeichen Mann und Frau, Trennstrich, WC, Pfeil |
| camp_c32 | 1 | Boden | 4.41 x 3.86 x 0.58 | 2008 | 17834 | 1 | 26 | vier Decken (Höhenfeld mit Falten, zusammengeschobener Stelle, unregelmäßigem Rand, umgeschlagener Kante, zwei mit Borte), Isomatten (ausgebreitet mit Rillen, zwei gerollt mit Riemen), Kissen, Reisetasche, Rucksäcke, Koffer, Pappbecher mit Deckel und Manschette, umgefallene Becher mit Fleck, Thermoskanne, Flasche, Verpackungen, Zeitung, Pappschild 1784XBA8JLP / -> 186 |
| xray_scanner | 2 | Boden | 5.00 x 1.96 x 1.45 | – (fehlte) | 4340/4340 | 3 | 16 | Rollenbahn vorn auf Beinen, Gerät mit gerundetem Gehäuse, Tunnel mit Bleivorhang an beiden Enden, Gurtband, Absperrband rot-weiß, Lüftungsschlitze, Warnzeichen, Zettel AUSSER BETRIEB, Bedienpult mit Not-Aus, Schlüsselschalter, Tasten; Monitorplatz (aus), Rollenbahn hinten mit Endanschlag, Ablagetische mit Wannen. v0/v1: gleiche Maschine, quer versetzt, damit sie ihren Kollisionsblock genau deckt |
| security_gate | 1 | Boden | 0.66 x 1.69 x 2.26 | – (fehlte) | 2175 | 3 | 14 | zwei Säulen mit runden Kanten, Innenabdeckung mit LED-Zonenleisten (aus), Alu-Stirnkanten, Fugen, Rampen, Herzschrittmacher-Warnzeichen, Querbalken mit Anzeige, Ampel, Lautsprechergitter, Absperrband, Zettel, Kabelbrücke |
| tray_stack | 1 | Boden | 0.68 x 0.48 x 0.56 | – (fehlte) | 2148 | 3 | 5 | Rollwagen mit vier Lenkrollen, zwölf ineinandergesetzte Wannen (offene Schale mit Rand), leicht versetzt, vergessener Gürtel mit Schnalle |
| lounge_chair | 2 | Boden | 0.84 x 0.82 x 0.83 | – (fehlte) | 1057/1057 | 24 | 5 | Kufengestell aus Chromrohr mit Gleitern, Sitzkörper, gewölbtes Sitzkissen mit Keder, Rückenkissen mit Steppnähten, Armlehnen mit Keder, abgesessene Stellen. v0 Leder braun, v1 Stoff blau |
| lounge_table | 1 | Boden | 0.64 x 0.64 x 0.60 | – (fehlte) | 750 | 12 | 7 | Tellerfuß und Säule aus Chrom, Holzplatte mit Kante, Zeitschrift, Pappbecher |

Dreiecke der Ausstattung in der Ebene: vorher 0,18 Mio. (ohne die fünf fehlenden Klassen), jetzt 0,72 Mio.
(Summe Stück × Dreiecke laut `propdump`) – weit unter 4 Mio.

## Varianten und Karte

- Varianten der häufigen Klassen: Check-in 3, Sitzreihe 3, Fundsachen 3, Loungesessel 2. Gewählt am Ende von
  `tools/levels/level36.py` mit eigenem Zufall (`random.Random(3636)`) nach allen Platzierungen – die Anordnung bleibt
  gleich (172 Objekte, 0 Objekte außer der Variante verändert, geprüft gegen die alte Karte).
- Durchleuchtungsband: Variante nach der Lage seines Blocks. Die Karte setzt die drei Blöcke auf ganze Kacheln
  (`int(y_) - 1 … int(y_) + 1`), das Modell aber auf `y_`; bei den Spuren 1 und 3 liegt der Block 0,5 m versetzt. v0 ist
  0,5 m quer verschoben, v1 mittig – jedes Band deckt seinen Block genau (x −2…+3 m, y 2 m).
- Blöcke gedeckt: Check-in-Reihe (Block 2 m hinter den Schaltern → Sammelband), BTA-Stand (Block hinter der Rückwand →
  Sideboard und Lagerschrank), Gepäckband, Fundsachen, Laufbänder, Gate-Schalter, Sitzreihen, Röntgenbänder.
- Sitzreihe: Der Tisch lag bisher auf der Seite, die die Karte als Block führt; die Funde (Ablage bei +1,6 m) lagen
  daneben in der Luft. Das Modell ist jetzt gespiegelt gebaut – Tisch dort, wo Block-Lücke und Funde sind.

## Runden

Bögen je Runde unter `docs/screens/props/level36/<runde>/`, Übersichten `docs/screens/level36/props_uebersicht_<runde>.jpg`.
Die Kontaktbögen laufen mit dem Studio aus `propsheet.py` (Spieler wird in jede Kabine gestellt).

### vorher (alte V12-Fassung)
- Fast alles Quader und Zylinder ohne Fasen: Gepäckband als Kasten mit Mittelwand, Koffer als Klötze; Check-in als weißer
  Kasten mit orange Platte und schwarzer Platte als Trennwand; Laufband als Trog mit hellgrüner Glaswand; Sitzreihe aus
  Brettern; Gate-Schalter ein Kasten; Rollgitter aus Brettern; Uhren mit flachen Strichen.
- Ladenfronten: helle, undurchsichtige Scheibe vor der ganzen Auslage.
- Ausgangsschild mit entarteten Dreiecken (Schrift).
- Sitzreihe: Tisch auf der falschen Seite (s. o.).
- Sicherheitskontrolle und Lounge: keine Modelle (in Runde 1 der Ebene nur Blöcke zu sehen).

### r1 – alle 34 Klassen neu
Befund ([Übersicht](docs/screens/level36/props_uebersicht_r1.jpg)):
- Lack-Fotostrukturen (`painted_metal_white/grey`) wirken verschmutzt, abgeplatzt, granitartig: BTA-Rückwand
  ([Bogen](docs/screens/props/level36/r1/bta_booth.jpg)), Gate-Schalter ([Bogen](docs/screens/props/level36/r1/gate_counter.jpg)),
  Gate-Tür, WC-Schild, Röntgen-Sockel, Check-in-Front – passt nicht zu „sauber, ordentlich“.
- Glas ist im Spiel undurchsichtig: Schaufenster verdecken die Auslage ganz
  ([Bogen](docs/screens/props/level36/r1/storefront_a.jpg)), Laufband-Balustrade wie eine hellgrüne Brüstung, Drahtglas wie
  helle Fliesen.
- Torsonde: graue Platten ohne Einzelheiten. C32: Decken flach und zu bunt. Gepäckband: Abrieb auf den Lamellen als
  weiße Flecken, Insel ein flacher Kasten. Loungesessel ohne Nähte.
- Gut: Abflugtafel, Uhren, Ausgangsschild, Gate-Schilder, Wegweiser, Fundsachen, Wannenstapel, Röntgenband.
- Prüfung: entartete Dreiecke (Decken, Bildzeichen, Strangprofile) und umgeklappte Normalen (Gürtel-Spirale).

Änderung: glatte Lacke ohne Struktur; Schaufenster ohne Scheibe mit Spiegelstreifen, Tür mit Mittelriegel, getöntes
Glas; Torsonde mit LED-Leisten, Stirnkanten, Fugen, Rampen, Warnzeichen, Ampel, Gitter; Decken stärker gefaltet, Wolle
gedeckt; grauer Gummiabrieb, Inselabdeckung mit First und Stoßfugen; Steppnähte und Keder am Sessel; Dreiteilung im
`Kit` mit Entfernen flächenloser Dreiecke und Splitter, Spirale ohne Nulldurchgang.

### r2
Befund ([Übersicht](docs/screens/level36/props_uebersicht_r2.jpg)):
- Klar besser: saubere Flächen, Auslagen sichtbar ([Zeitungen](docs/screens/props/level36/r2/storefront_a.jpg)),
  Laufband mit dunklem Glas, Drahtglas dunkel mit Gitter, Torsonde gegliedert.
- Mode: Kleidung wirkt wie flache Bretter ([Bogen](docs/screens/props/level36/r2/storefront_c.jpg)).
- BTA: Rückseite der Rückwand eine glatte dunkle Fläche.
- Spiegelstreifen etwas zu hell; Brunnenverkleidung ein Klotz; C32-Decken noch zu glatt
  ([Bogen](docs/screens/props/level36/r2/camp_c32.jpg)).

Änderung: Kleidung als Loft mit Hals, Schultern, Wellen, Ärmeln, Knopfleiste; BTA-Rückseite mit Fugen und
Revisionsklappe; Spiegelstreifen dunkler; Brunnenverkleidung mit gerundeter Front und Sockel; umgeschlagene Kanten an
zwei Decken.

### r3 (geänderte Klassen)
Befund ([Übersicht](docs/screens/level36/props_uebersicht_r3.jpg)):
- Kleidung hängt jetzt als Kleidung ([Bogen](docs/screens/props/level36/r3/storefront_c.jpg)), BTA-Rückseite gegliedert,
  Brunnen rund.
- C32: Decken sind immer noch Rechtecke mit leichtem Relief ([Bogen](docs/screens/props/level36/r3/camp_c32.jpg)).
- Laufband (Blender-Draufsicht, im Studio sieht die Kamera nicht hinein): Paletten, Kammzähne, Flurplatten, Handlauf und
  Not-Aus stimmen; Schmutzfleck auf der Flurplatte zu hart.

Änderung: Decke neu – zusammengeschobene Stelle zieht das Tuch zusammen, stärkere Falten, unregelmäßiger Rand, Borte;
Schmutz an der Kammplatte als weicher Abrieb.

### r4 (geänderte Klassen)
Befund ([Übersicht](docs/screens/level36/props_uebersicht_r4.jpg)):
- Decken liegen jetzt geknüllt mit unregelmäßigem Rand und Borte ([Bogen](docs/screens/props/level36/r4/camp_c32.jpg)) –
  klar besser.
- Im Spiel (Stellenbilder Check-in und Sicherheitskontrolle): Schalterreihe mit durchgehendem Sammelband, Röntgenbänder
  und Torsonden stehen auf den Blöcken, keine Blockflächen sichtbar. Weiße Lackflächen zeigen im Spiel das
  Abplatzer-Muster des Looks `paint` (Gerätegehäuse, Schichtstoff).

Änderung: Schichtstoff und Gerätegehäuse als Kunststoff (`look=plastic`, r 0,8–0,95).

### r5 (alle Klassen)
Befund ([Übersicht](docs/screens/level36/props_uebersicht_r5.jpg)): alle Klassen stimmig; Uhren, Schilder, Tafel, Exit
und WC unter der streifenden Lampe ohne Plastikglanz; Gepäck, Regale, Röntgenband, Wannen, Sessel ruhig. Holzplatten
(Sitzreihen-Tisch, Lounge-Tisch, Ladenregale) zu orange und glänzend (Holzstruktur sehr glatt).

Änderung: Holz r 1,8 und etwas weniger gesättigt.

### final
[Übersicht](docs/screens/level36/props_uebersicht_final.jpg), Bögen `docs/screens/props/level36/final/`.

## Prüfungen

- `validate_models.py` für alle 42 Dateien der Ebene: 0 Fehler.
- `gameplay_test`: 647 Prüfungen, 0 Fehler; Halt der Ausstattung level36: 172 Objekte, 0 ohne Halt (gesamt 10 506, 0).
- `room_metrics.py level36 --check`: OK.
- Dreiecke Ausstattung Level 36: 718 607 (propdump nach final).
- Stellenbilder im Spiel (Check-in, Sicherheitskontrolle, `spotshots.py`): Modelle stehen auf den Blöcken.

## Restmängel (ehrlich)

- Glas ist undurchsichtig (Engine): Schaufenster haben deshalb keine Scheibe, nur Spiegelstreifen; die Laufband-
  Balustrade ist dunkles Glas, durch das man nicht hindurchsieht.
- Im Studio sieht keine Kamera von oben in das Laufband; Paletten, Kammplatten und das Loch im stehenden Band sind nur
  in der Blender-Vorschau und aus Augenhöhe im Spiel geprüft.
- Kleidung, Decken und Polster sind modellierte Formen ohne Stoffsimulation; die Decken wirken aus 50 cm etwas glatt.
- Zeitschriften sind farbige Platten mit Titelband, keine Titelbilder; Gebäck einfache Drehkörper.
- Lochblech der Sitze als dunkle Sechsecke auf der Fläche (keine echten Löcher); aus 2 m kaum zu sehen.
- Die Nummernschilder der Check-in-Schalter zeigen alle „36“ (Variante ändert den Zustand, nicht die Nummer).
- Die geöffneten Flügel der Boarding-Tür liegen nur als Kanten in der Laibung (das Modell darf nicht hinter die Wand).
- Loungesessel und -tisch sind mit gut 1000 Dreiecken eher schlicht.

## Aufwand

- Sitzung 1 (27.10.): Werkzeuge, vorher-Bögen, Gepäck, Gepäckband, Fundsachen, Check-in, Tafel, BTA – etwa 1 h.
- Sitzung 2 (01./02.11.): restliche Klassen, fünf neue Klassen, Varianten, Blöcke, Runden r1–r5 und final, Stellenbilder,
  Dokument – etwa 9 h (davon gut die Hälfte Laufzeit der Kontaktbögen).
- Runden: 5 Kritikrunden (r1, r2 voll; r3, r4 geänderte Klassen; r5 voll) und Schlussrunde final.
