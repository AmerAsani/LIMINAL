# Ausstattung Level 5 „Terror Hotel“ (V13.3 Teil 1)

Alle Objektklassen von Level 5 in Blender neu (`tools/blender/models/models_l5.py`), dazu die bisherigen Code-Modelle
Sessel, Sofa, Kleiderschrank und Bücherregal (`src/props/prop_library.cpp`, jetzt von den Dateien überdeckt).
Grundlage sind `LEVELSPEC_5.md` und `LOOK_5.md` (Wikidot Level 5): ein Grandhotel der 1920er–30er, das niemand mehr
betritt, aber jemand noch putzt:
- Main Hall mit mahagoniroter Damasttapete, Nussbaum, Kerzenwandleuchtern, Sofas, Sesseln, Bücherregalen;
- Türen mit goldenen Plaketten, deren Nummern keinem Muster folgen;
- Aufzüge mit Kupfergitter;
- Beverly Room mit großem Kronleuchter, kleinem verziertem Tisch mit Getränken und unfertiger Mahjong-Partie,
  Grammophon und Silberschild an den Eingängen;
- Boiler Room: eng, heiß, große alte Maschinen, Rohre, Ventile, rauchende Öfen.

Die Hotelbereiche bleiben sauber (Wiki: der Schmutz verschwindet). Gebrauchsspuren sind dort Abnutzung: abgesessener
Samt, abgegriffenes Messing und Holz, blinde Stellen im Spiegel. Ruß, Rost, Fett und Abplatzer gibt es nur im Kesselraum.

**Bauweise**
- Eigener Geometrie-Kern im Modul (Geometrie je Material in einem bmesh). Bausteine:
  - Kasten mit Fase, Drehkörper, Prisma, Platte nach Umriss;
  - Profil entlang eines Pfads mit Gehrung (rotationsminimierende Rahmen) für Leisten, Gesimse, Rahmen, Keder;
  - Rohr entlang Bézierpfad mit Verjüngung (Leuchterarme, Tonarm, Schnecken).
- **Polster** als verformte, abgerundete Kissen (`g_cushion`): Bausch, durchgesessene Kuhle mit leichten Falten,
  Rautenheftung mit Knöpfen und Faltenlinien, Keder entlang der Nähte, abgesessene Stellen als eigene Flecken.
- **Normteile:** Sechskantmuttern, Nieten, Flanschpaare mit Dichtung und Schrauben, Handräder nach dem
  Ventilrad-Maßstab (Oberteil mit Stopfbuchse, Spindel mit Gewinde, Nabe mit Mutter, geschwungene Speichen, Kranz mit
  Griffbuckeln und leichter Unrundheit, Lackabplatzer, Rost am Sockel, Fett in den Gängen).
- Flecken liegen 0,6–2 mm über der Oberfläche: eben (Ruß, Abrieb), auf Zylindern gewölbt (Rost, Abplatzer,
  Rostläufer), auf gewölbten Böden mit Normale aus dem Drehprofil.
- Holz: Fotostruktur `wood_walnut_floor` mit Tönung je Bauteil (Rahmen, Füllung, Leisten, Furnier). Die prozedurale
  Maserung (Runde r1/r2) wirkte gezeichnet und hatte Äste.
- Schrift als Geometrie: Inter für „The Beverly Room“. Ziffern sind DejaVu Sans Mono, denn bei Inter füllte die
  Umwandlung das Loch der „4“ (LICENSES.md).
- Rauheit und Metallizität je Material über `r=`/`m=`:
  - Samt matt (r 1,25), abgesessen glatter (r 1,0);
  - Schellack seidenmatt (r 0,55–0,7);
  - Messing poliert (r 0,55, m 0,95), angelaufen (r 0,95), abgegriffen blank (r 0,35);
  - Kesselmantel halbmatter Lack (r 1,15, m 0,1), Stein und Ruß stumpf (r 1,3–1,5).

## Klassen

Maße: Tiefe (x) × Breite (y) × Höhe (z) in m, Variante 0. Dreiecke je Variante. „Stück“ = Anzahl in der Karte.
„Mat.“ = Materialien im Modell. Bögen: `docs/screens/props/level5/vorher/<name>.jpg` und
`docs/screens/props/level5/final/<name>[_vN].jpg`.

| Klasse | Var. | Befestigung | Maße | Dreiecke vorher | Dreiecke nachher | Stück | Mat. | Was das Modell jetzt hat |
|---|---|---|---|---|---|---|---|---|
| armchair | 2 | Boden | 0.86 x 1.02 x 0.98 | 176 (Code) | 12164 | 4 | 6 | Clubsessel: Rollarmlehnen mit Stirnplatte und Keder, Rautenheftung mit 18 Knöpfen, durchgesessenes Sitzkissen mit Keder oben und unten, abgesessene Kuhle und Armauflagen, gedrechselte Kugelfüße mit Messinghülsen. v0 Weinrot, v1 Gold (LOOK_5) |
| sofa | 2 | Boden | 0.94 x 2.15 x 0.91 | 304 (Code) | 20516 | 3 | 6 | Chesterfield: wie der Sessel, 42 Knöpfe, drei unterschiedlich durchgesessene Kissen, sechs Füße. v0 Weinrot, v1 Gold |
| wardrobe | 2 | Boden | 0.67 x 1.19 x 2.03 | 208 (Code) | 2192/2281 | 4 | 5/7 | Sockel und Kranz mit Profil (Gehrung), Seiten mit abgeplatteten Füllungen, zwei Türen mit Füllungen und Kehlleisten, Schublade mit Bügelgriffen, Knöpfe, Bänder, Schlüsselschild mit Schlüsselloch, abgegriffenes Holz. v1 dunkler, linke Tür mit Spiegel und blinden Stellen |
| bookcase | 4 | Boden (Ursprung hinten) | 0.37 x 0.98 x 2.00 | 1040 (Code) | 5764/4984/5252/4740 | 3 | 14 | Sockel und Kranz mit Profil, Perlstab an den Vorderkanten, Rückwand mit Fugen, Böden mit Vorleimer. Bücher mit gerundetem Rücken, Schnitt, Bünden und Goldschild in acht Leder-/Leinenfarben, Lücken, angelehnte Bücher, liegende Stapel, Buchstütze. Varianten: andere Belegung und Füllgrad |
| side_table | +2 | Boden | 0.56 x 0.56 x 0.66 | 164/212 (Code) | 594/1314 | 19 | 4/6 | v0/v1 baut models_l1_code (rund, mit Lampe). Neu v2: eckiger Teetisch mit Profilkante, Zarge, Schublade mit Messingknopf, Ablage, Füße. v3: dazu Messingleuchter mit Seidenschirm. Platte genau 0,66 m (Funde) |
| hotel_bed | 1 | Boden | 2.19 x 1.76 x 1.25 | 2556 | 9114 | 4 | 6 | Kopfteil mit Bogen, Abdeckleiste (Sweep) und Wurzelfurnier mit Perlstab, Fußteil mit Füllung und Kehlleiste, gedrechselte Pfosten mit Knäufen, Zargen mit Profil, Matratze, Laken, Tagesdecke mit nach unten wachsendem Faltenwurf und Borte, Umschlag, zwei angelehnte Kissen |
| hotel_door_a–f | 6 × 4 | Wand | 0.11 x 1.13 x 2.22 (c, f: 1.23 x 2.33) | 1520–6052 | 1544–4352 | 66 | 7–9 | Bekleidung mit Profil und Sockelklötzen, Schattenfuge, Blatt mit abgeplatteten Füllungen und Kehlleisten. Bauart je Buchstabe: a 4, b 6, c 2, d 5 liegende, e 2 hohe, f 4 Füllungen; c und f mit Verdachung; b und e Mahagoni. Langschild mit Drücker (blank) und Schlüsselloch, drei Bänder, abgegriffenes Holz am Schild. Nummernschild je Bauart oval, eckig oder mit Spitzen, mit Schräubchen und schwarzen Ziffern. v1 Trittblech, v2 Hängeschild „DO NOT DISTURB“, v3 Trittblech und Schlüssel im Schloss. 24 Nummern ohne Muster (s. u.) |
| placard_beverly | 1 | Wand | 0.01 x 0.52 x 0.12 | 11152 | 3456 | 3 | 2 | Neusilberplatte mit Fase und erhabener Randlinie, Schrift „The Beverly Room“ schwarz ausgelegt, vier Linsenkopfschrauben mit Schlitz; keine entarteten Dreiecke mehr |
| wainscot | 1 | Wand | 0.04 x 1.00 x 1.02 | 768 | 178 | 855 | 3 | Sockelleiste und Brüstungsleiste mit echtem Profil (Wulst, Kehle), Rahmen, abgeplattete Füllung mit Kehlleiste. Ebene: 152 000 statt 656 000 Dreiecke |
| cornice | 1 | Wand (Decke) | 0.20 x 1.00 x 0.26 | 96 | 292 | 962 | 2 | Stuck mit Karnies, Hängeplatte, Zahnschnitt (nur sichtbare Flächen), Viertelstab; vergoldeter Rundstab |
| candle_sconce | 3 | Wand | 0.17 x 0.33 x 0.30 | 588 | 1452/1760/2268 | 217 (+7 in 5.3) | 4–6 | Ovale Rosette mit angelaufenem Rand, Eichelzapfen, Knauf, geschwungene, verjüngte Arme mit Zierschnecken und Manschetten, Tropfteller mit Tülle, Kerzenhülsen mit Wachsnase, Flammenbirnen. v1 dritter Arm mit höherer Kerze, v2 mit gefalteten Stoffschirmchen (leuchten durch) |
| painting_portrait | 3 | Wand | 0.07 x 0.88 x 1.08 | 556 | 1700/1817/720 | 35 | 26–29 | Rahmen mit Profil und Gehrung (v0/v1 Blattgold mit Eckrosetten, Akanthus und Muschel; v2 Nussbaum), Leinwand, gemalt in Schichten: Grund, weicher Schein, Gewand mit Falten und Schulterlicht, Gesicht mit Schatten und Lichtern, Ohren, Augen mit Iris und Glanzlicht, nachgedunkelter Firnisrand, Craquelé, Messingschildchen. v0 Herr, v1 Dame mit Bubikopf und Perlen, v2 alter Mann mit Bart |
| window_bright | 1 | Wand | 0.45 x 1.55 x 1.85 | 588 | 3812 | 1 | 11 | Tiefe Laibung mit hellem Licht dahinter (Weg nach Level 188), Blendrahmen, zwei Schiebeflügel mit Sprossen (12 Scheiben, unterschiedlich spiegelnd), Riegel, Hebegriffe, Fensterbank mit Profil, Bekleidung, Samtvorhänge mit Raffhaltern, Vorhangstange mit Knäufen |
| chandelier_grand | 2 | Decke | 1.47 x 1.40 x 1.62 | 6472 | 12580/10412 | 6 | 5 | Baldachin, Kette aus Gliedern, gedrechselter Schaft mit Vase und Schale, geschwungene Arme mit Zierschnecken und Ring, Kerzen, Kristall: Perlgirlanden, Behangtropfen, Prismen, Kugel. v0 zehn und sechs Arme, v1 acht Arme und Korb aus drei Prismenkränzen |
| lantern_green | 1 | Decke | 0.31 x 0.27 x 1.01 | 376 | 2184 | 9 (+8 in 5.3) | 4 | Rosette, Kette, sechseckige Haube mit Rippen, Lüftungskranz und Knauf, Rahmen mit sechs Stegen, grüne Scheiben (leuchten), Birne, Bodenschale mit Zapfen |
| elevator_cage | 1 | Durchbruch | 0.18 x 1.44 x 2.56 | 652 | 3682 | 2 | 7 | Kupferpilaster mit Kanneluren (Grünspan), Kapitell und Sockel, Sturz mit Rosettenfries und Gesims, halbrunde Anzeige mit Skala und Zeiger, geschlossenes Scherengitter (U-Stäbe, Kreuzlaschen mit Nieten, Laufschienen, Griff), Messingschwelle mit Rillen, Rufknöpfe. Rundes ist vorab in y gestaucht, weil die Karte um 1,35 streckt |
| mahjong_table | 1 | Boden | 0.90 x 0.90 x 1.06 | 2368 | 10994 | 1 | 17 | Platte mit Randprofil und Filz, Zarge mit vier Schubkästchen und Messingknöpfen, gedrechselte Beine, Stegkreuz. Vier Mauern aus zweilagigen Steinen (eine angebrochen), zwei Hände auf Bänkchen, 22 offene Steine mit Zeichen, Würfel. Whiskygläser mit Eis, Cocktailglas mit Olive, Karaffe, Sektflasche mit Folie, Aschenbecher mit Zigarette |
| gramophone | 1 | Boden | 0.84 x 0.60 x 1.53 | 2056 | 4620 | 1 (+1 in 188) | 12 | Schränkchen mit Profilsockel und bespannten Türen, Laufwerkskasten, Plattenteller mit Filz, Schellackplatte mit Rillen und Etikett, Schwanenhals-Tonarm mit Schalldose (Glimmer) und Nadel, Blumentrichter mit Blattnähten und gerolltem Rand, Kurbel mit Holzgriff, Bremse, Drehzahlknopf |
| boiler_tank | 1 | Boden an Wand | 2.56 x 1.98 x 3.80 | 3736 | 25538 | 5 | 20 | Flammrohrkessel nach dem Rohr- und Ventilrad-Maßstab, Einzelheiten unten |
| furnace | 1 | Boden an Wand | 2.37 x 2.53 x 3.80 | 524 | 8929 | 2 | 11 | Ziegelkörper mit Winkeleisen und Zugstangen (Muttern), Deckplatte, genietete Feuertür mit Scharnieren, Riegel und Schauloch, Glut im Spalt, Aschentür, Sturzeisen. Rauchrohr aus drei genieteten Schüssen mit Flanschen, Drosselklappe und Deckenkragen. Rußfahne in drei Tönen, Ascheberg mit Kohle, angelehnte Schaufel |
| metro_car_end | 1 | Boden | 6.21 x 2.65 x 2.70 | 1340 | 11564 | 1 | 13 | Edelstahlkasten mit Sicken, Zierstreifen, Fenster mit gerundetem Gummirahmen, gewölbtes Dach mit Lüfterhauben und Regenrinne, Stirnwände mit Übergangstür, Griffstangen und Nummer „903“. Offene Schiebetür mit Gummikanten und Rillen-Trittblech. Innen gepolsterte Längsbänke, Haltestangen, Handläufe mit Schlaufen, Lichtbänder, Werberahmen, Rillengummiboden |

**Kessel im Einzelnen:**
- drei Schüsse mit Überlappung, Rundnähte mit 56 Nieten, Längslaschen mit Doppelreihe;
- gewölbter Stirnboden mit Nietring, Feuertür (Rahmen, Glutspalt, Luftschieber, Scharniere, Riegel), Aschenfall im
  Mauerwerk;
- Mannloch mit zehn Muttern, Dampfdom mit Flansch und zwölf Muttern;
- Absperrventil mit Handrad (Ventilrad-Maßstab), gedämmte Dampfleitung mit Spannbändern bis zur Decke;
- Hebelsicherheitsventil mit Gewicht, Manometer am Siphon (Skala, rote Marke, Zeiger, Glas);
- Wasserstandsglas mit zwei Hähnen, Schutzstäben und Ablass;
- Speiseleitung mit Flanschen und Handrad in den Boden, Abschlammventil mit Handrad;
- Typenschild „KESSEL 3  12 ATU  1928“;
- Ruß über der Feuertür, Rostläufer unter den Nähten, Abplatzer, Rost am Dom, Ruß auf der Dämmung, Fett an den
  Spindeln.

**Dreiecke der Ebene** (alle 2264 Objekte, propdump nach dem Merge): **1,77 Mio.**, davon 1,49 Mio. für die eigenen
Klassen. Vorher 1,28 Mio. Größte Posten:
- Kerzenleuchter 217 × 1,5–2,3k = 0,39 Mio.;
- Gesims 962 × 292 = 0,28 Mio.;
- Vertäfelung 855 × 178 = 0,15 Mio. (vorher 0,66 Mio.).

Level 5.3 (Leuchter, Laterne): 1,78 Mio. Level 188 (Grammophon): 0,15 Mio.

## Varianten und Karte

**Varianten** (`build_<name>__vN` → `data/models/<name>@N.glb`):
- Türen 6 Bauarten × 4 Varianten, jede Variante mit eigener Nummer;
- Kerzenleuchter 3, Bildnisse 3, Kronleuchter 2;
- Sofa, Sessel und Schrank je 2, Bücherregal 4;
- Beistelltisch +2.

**Zimmernummern ohne Muster (Wiki):** 24 Nummern mit ein bis vier Stellen, keine Folge, keine Wiederholung:

| Bauart | v0 | v1 | v2 | v3 |
|---|---|---|---|---|
| a | 117 | 1213 | 52 | 386 |
| b | 2041 | 7 | 469 | 3318 |
| c | 31 | 812 | 1607 | 94 |
| d | 608 | 2275 | 13 | 541 |
| e | 9 | 730 | 4102 | 68 |
| f | 343 | 1089 | 26 | 957 |

Vorher trugen 66 Türen nur 6 Nummern. Jetzt kommen 21 der 24 Nummern in der Karte vor.

**Karte** (`tools/levels/level5.py`):
- Nach allen Platzierungen zieht ein eigener Zufall (`random.Random(1505)`) je Objekt die Variante. Die Liste
  `VARIANTS` enthält die Klassen oben.
- Beistelltische mit Lampe (Licht im Zimmer) bekommen 1 oder 3, Tische ohne Lampe 0 oder 2.
- Die Anordnung bleibt unverändert. Vergleich der Karte vorher/nachher: 2256 Objekte, 0 mit anderer Lage oder anderem
  Modell, Räume, Türen und Spawnpunkte gleich.
- Verteilung:
  - Türen a 1/0/3/5, b 5/1/2/5, c 1/7/0/6, d 3/4/4/2, e 4/2/2/2, f 4/0/2/1;
  - Leuchter 78/70/69, Bildnisse 9/16/10, Kronleuchter 4/2;
  - Sessel 2/2, Sofa 2/1, Schrank 3/1, Regal 1/0/1/1, Beistelltisch 8/2/7/2.

## Runden

Studio-Bögen mit `tools/qa/propsheet.py`, kompakte Prüfbögen mit `tools/qa/reviewsheet.py`. Vor jeder Runde
Werkbank-Vorschauen in Blender (Scratchpad, nicht eingecheckt) und Nahaufnahmen einzelner Stellen (Nummernschild,
Drücker).

### Vorher (V12-/Code-Fassung)
Übersicht `docs/screens/level5/props_uebersicht_vorher.jpg`, Bögen `docs/screens/props/level5/vorher/` (25 Klassen).
Zuerst blieben die Kabinen jenseits von x ≈ 90 schwarz, weil die Welt nur um den Spieler lädt. Behelf: Gruppen zu
sieben Klassen; später kam der Fix in propsheet.py aus liminal-v13.
- **armchair / sofa** (Code): abgeschrägte Quader, einfarbig, keine Nähte. Der Sessel schwebte 12 cm über dem Boden
  (keine Füße).
- **bookcase** (Code): Kasten mit Brettern, Bücher als Quader in vier Farben.
- **wardrobe** (Code): zwei Kästen, Türen als Platten, Knauf als Zylinder.
- **boiler_tank**: schwarzer Zylinder mit Bändern statt Nietnähten, Boden als gestauchte Kugel, Handrad als Torus mit
  drei Stäben, keine Nabe, keine Spindel.
- **candle_sconce**: Messingplatte, gerade Zylinderarme, Kugelflammen.
- **chandelier_grand**: zwei Torusringe, Zylinderarme, Ikosaeder als Kristalle.
- **cornice**: brauchbares Profil ohne Gliederung, Perlstab als glatter Zylinder.
- **elevator_cage**: schräge Einzelstäbe ohne Verbindung, Anzeige als Fächer aus Dreiecken.
- **furnace**: Ziegelquader, Eisentür als Platte, Rauchrohr als glatter Zylinder.
- **gramophone**: Kästen, glatter Kegeltrichter, Tonarm als Stab, keine Kurbel.
- **hotel_bed**: Kästen, Tagesdecke als Quader ohne Fall und Falten.
- **hotel_door_a–f**:
  - flaches Blatt mit aufgesetzten Kästen, Kugelknauf, keine Bekleidung, kein Schlüsselloch, keine Bänder;
  - die Nummernplakette hatte entartete Dreiecke (Fase dicker als die Platte);
  - nur 6 Nummern für 66 Türen.
- **lantern_green**: Kegel auf Sechskantzylinder.
- **mahjong_table**: Säulentisch, Steine als Quader, Gläser als Zylinder.
- **metro_car_end**: Kastenwände, Fenster als Leuchtflächen, Inneres aus Quadern.
- **painting_portrait**: Rahmen aus vier Kästen, Figur aus wenigen flachen Polygonen.
- **placard_beverly**: 11 152 Dreiecke allein für die Schrift, Platte ohne Schrauben.
- **wainscot**: 768 Dreiecke je Meter (855 Stück = 656 000), Rahmen aus Kästen. Die Nussbaum-Dielentextur sah auf
  der Wand wie Fußboden aus.
- **window_bright**: Rahmen aus Kästen, keine Flügel, keine Bank.

### Neubau und Testbögen (vor r1)
- **Polster**:
  - Der erste Wurf hatte kaum sichtbare Heftung; Tiefe 2,2 → 4 cm, Faltenlinien zwischen den Knöpfen.
  - Abgesessene Stellen als Flächenmaterial ergaben Treppenkanten, jetzt eigene Flecken mit weichem Umriss.
  - Der Keder entlang der Rückenkante hatte 1,5 % gekippte Dreiecke: die Eckpunkte des Rasters lagen zu dicht.
    Jetzt Mindestabstand 12 mm.
- **Bett**: Die Tagesdecke griff auf freigegebene bmesh-Punkte zu (Absturz), die Kanten der Decke liefen in der
  falschen Richtung, das Kissen kippte nach vorn. Alles behoben.
- **Kessel**: Der Aschentür fehlte der Untergrund (schwebte vor dem Mantel), jetzt Aschenfall aus Mauerwerk. Mannloch
  und Sicherheitsventil überlagerten sich. Ruß saß hinter dem gewölbten Boden, jetzt auf dem Boden mit Normale aus dem
  Profil.
- **Kerzenleuchter**: 2,9–4,0k Dreiecke wären bei 217 Stück 0,7 Mio. gewesen; Segmente reduziert auf 1,5–2,3k.
- **Mahjong-Tisch**: 19k Dreiecke; Mauersteine ohne Fase, danach 11k.
- **Beistelltisch**: Die Varianten 0/1 baut models_l1_code (später in der Modulliste, überdeckt). Hier nur v2/v3.

### Runde r1 (alle 52 Bögen, mit Varianten)
Übersicht `docs/screens/level5/props_uebersicht_r1.jpg`, Beispiel `docs/screens/props/level5/r1/hotel_door_b_v2.jpg`.
- **Holz (Türen, Schrank, Vertäfelung, Tische)**: hell orange, gezeichnete Wellenmaserung mit dunklen Ästen, wie
  Kiefer.
  - Änderung: Tönung etwa halbiert.
- **Polster**: Die Kuhle mit hellem Fleck und Faltenbuckeln wirkte wie ein Gesicht.
  - Änderung: Abrieb nur noch 10 % heller, Falten halb so hoch, Fleck vorn entfällt.
- **Ofen**: Ruß als schwarze Tarnflecken.
  - Änderung: kleinere, rundere Flecken.
- **Kessel**: Rückwand nach innen gewölbt, von hinten Loch im Mantel. Rostflecken als orange Rechtecke.
  - Änderung: Boden nach außen gewölbt; Rostläufer schmal, nach unten auslaufend, auf beiden Seiten; Dämmung dunkler
    mit Ruß.
- **Bildnisse**: Schein hinter dem Kopf als harte Ringe (Zielscheibe).
  - Änderung: vier, später sechs flache Stufen, dazu Lichter, Ohren, Faltenschatten, Craquelé.
- Kronleuchter, Laterne, Aufzug, Mahjong-Tisch, Grammophon, Bett: ohne Befund.

### Runde r2 (alle 52 Bögen)
Übersicht `docs/screens/level5/props_uebersicht_r2.jpg`, Beispiel `docs/screens/props/level5/r2/furnace.jpg`.
- **Holz**: dunkler und satter, aber die prozedurale Maserung zeigte weiter Wellenlinien und Äste (Nahansicht Tür).
  - Änderung: Fotostruktur `wood_walnut_floor` mit Tönung je Bauteil.
- **Ofen**: Ruß weiter schwarz. Ursache: sRGB 0,10 ist linear 0,01.
  - Änderung: drei Töne (Hauch 0,40/0,29/0,23, Mitte, Kern), Ränder weniger zackig.
- **Polster**: Abrieb jetzt dezent. Kein Befund mehr.
- **Kessel**: Rostläufer erzeugten 8 % gekippte Dreiecke (Zacken griffen über eine Zelle).
  - Änderung: Zacken höchstens eine Zelle, Umlaufsinn einmal je Fleck festgelegt.

### Runde r3 (42 Bögen der geänderten Klassen)
Übersicht `docs/screens/level5/props_uebersicht_r3.jpg`, Beispiel `docs/screens/props/level5/r3/hotel_door_c_v3.jpg`.
- **Holz**: wirkt jetzt wie poliertes Mahagoni bzw. Nussbaum. Furnierstreifen sichtbar, aber glaubhaft. Übernommen.
- **Ofen**: Rußfahne glaubhaft. Übernommen.
- **Türen**: Das Nummernschild war in den 2-m-Ansichten nicht zu sehen. Ursache: Es lag 5 mm vor dem Blatt, die
  Füllungen stehen 10 mm vor.
  - Änderung: Schild vor die Füllung (Blatt + 11,5 mm).

### Runde r4 (21 Türbögen)
Übersicht `docs/screens/level5/props_uebersicht_r4.jpg`, Beispiel `docs/screens/props/level5/r4/hotel_door_c_v3.jpg`.
- **Türen**: Schilder sichtbar. Die Nahaufnahme zeigte aber eine gefüllte „4“ (2041): Inter ist eine variable Schrift
  mit überlappenden Konturen. Höhere Kurvenauflösung half nicht.
  - Änderung: Ziffern in DejaVu Sans Mono (Blender-Datendateien).
- **Metro-Wagen**: Die Glasfenster wirkten milchig weiß. Ohne Durchsicht ist die Scheibe nur Spiegel.
  - Änderung: schwach leuchtende Fenster (beleuchteter Innenraum).

### Runde r5 (24 Bögen: Türen, Schild, Kessel, Metro)
Übersicht `docs/screens/level5/props_uebersicht_r5.jpg`.
- **Türen**: Ziffern sauber. Kein Befund mehr.
- **Metro-Wagen**: Fenster zu hell.
  - Änderung: Leuchtkraft 0,45 → 0,15.
- **Kessel, Schild**: kein Befund.

### Schlussrunde final (alle 52 Bögen)
Übersicht `docs/screens/level5/props_uebersicht_final.jpg`, Bögen `docs/screens/props/level5/final/`.
- Keine Rückschritte gegenüber r1–r5. Die Varianten sind nebeneinander verschieden: Bauart, Nummer, Trittblech,
  Hängeschild, Schlüssel; Bildnisse; Leuchter mit zwei, drei Armen oder Schirmen.
- Danach nur noch: Ziffern in DejaVu Sans Mono statt der eingebauten Schrift (Lizenz eindeutig; Bögen der Türen,
  des Kessels und des Wagens neu). Die Übersicht ist aus den Klassenbögen zusammengesetzt, weil der Teillauf sie
  sonst überschrieben hätte.

## Prüfungen
- `python tools/qa/validate_models.py` (alle eigenen Klassen mit Varianten): 58 Modelle, 0 Fehler.
- `build/tests/gameplay_test.exe data`:
  - Halt der Ausstattung: level5 2253 Objekte, 0 ohne Halt; alle Ebenen 10 506 Objekte, 0 ohne Halt.
  - Insgesamt 647 Prüfungen, 1 Fehler. Das ist der bekannte Ouroboros-Träger aus Level 1, nicht Teil dieser Arbeit.
- `python tools/qa/room_metrics.py --check level5`: Raummetrik OK (75 Räume, Decke 3,0 m, Gang min/Median
  3,0/3,5 m, verstellt 0/7).
- Dreiecke: 1,77 Mio. für Level 5 (unter 4 Mio.).
- Karte: nur Varianten geändert (siehe oben).

## Restmängel (ehrlich)
- **Bücherregal**: Die Karte stellt es wie Sofa und Sessel 0,55 m vor die Wand. Der Ursprung des Regals liegt aber
  hinten, deshalb steht es frei im Gang. Eine Änderung hätte die Anordnung verschoben (nicht erlaubt). Hinweis für die
  Karte: `level5.py`, Möbel in breiten Gängen, für `bookcase` den Abstand 0 nehmen.
- **Bildnisse**: Die Malerei ist flächig aus Polygonen (Modigliani-artig), keine echte Ölmalerei. Der Schein hinter dem
  Kopf zeigt noch schwache Stufen.
- **Holz**: Die Fotostruktur ist eine Dielentextur. Auf Füllungen erkennt man streifenweise Bretter (wie Furnier); die
  Maserung läuft wegen der Weltprojektion nicht mit dem Bauteil.
- **Metro-Wagen**: Die Fenster leuchten; den Innenraum sieht man nur durch die offene Tür. Der Wagen steht ohne
  Drehgestelle auf dem Boden der Nische (wie bisher, wegen des Einstiegs).
- **Maße**:
  - Ofen 2,37 × 2,53 m statt 1,92 × 2,04 m, wegen Ascheberg vorn und Schaufel an der Seite (keine Kollision, nur
    Grenzen);
  - Kessel 12 % breiter (Handrad der Speiseleitung);
  - Fenster 11 % breiter (Vorhänge);
  - Türen c/f 9 % breiter (Verdachung).
- **Polster**: Velours zeigt keinen Schimmer unter streifendem Licht (der Shader kennt keinen Glanzrand); die Rauheit
  ist matt gesetzt.
- **Beistelltisch v0/v1** gehören dem Modul der Level-1-Codemodelle. Wird dort etwas geändert, passen die Varianten
  2/3 stilistisch vielleicht nicht mehr dazu.
- **Mahjong-Steine**: Die Zeichen sind farbige Punkte, Striche und Balken, keine echten Schriftzeichen.

## Aufwand
- 27.10.: etwa 1,5 Stunden (Werkzeuge, Bau, Kontaktbögen vorher in Gruppen, Hilfen und erste Polster).
- 01.11.: etwa 8 Stunden:
  - Merge liminal-v13, alle Klassen neu, Varianten, Karte;
  - Runden r1–r5 und final;
  - Nahaufnahmen, Prüfungen, Dokument.
- Runden: vorher, r1, r2, r3, r4, r5, final (5 Kritikrunden und die Schlussrunde).
