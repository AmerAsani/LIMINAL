# Ausstattung Level 3 „Electrical Station“ (V13.3 Teil 1)

Alle Objektklassen von Level 3, die keiner anderen Ebene gehören, in Blender neu (`tools/blender/models/models_l3.py`).
Grundlage sind `LEVELSPEC_3.md` und `LOOK_3.md` (Wikidot Level 3). Das Wiki beschreibt eine alte elektrische Anlage aus
Ziegelgängen:
- weiße, rostige Gitter;
- Rohre mit schwarzer Flüssigkeit und Schlammlachen;
- Generatorräume, Fertigungsstraßen mit Förderbändern, Fenstern mit rotem Ziegel dahinter und Gitterrosttreppen;
- Kesselräume;
- Sanktuarien mit griechisch-römischen Säulen, einer großen Engelsstatue auf einem Sockel mit unentzifferten Zeichen
  und Bildern vom Engel (manche übermalt);
- Schädel, Teile von Zügen, verwitterte Frauenstatuen ohne Kopf und Arme, weiße Tuchbahnen, ein Aufzug nach Level 5.

**Bauweise**
- Eigener Geometrie-Kern im Modul (`Kit`: Geometrie je Material in einem bmesh). Bausteine: Kasten mit Fase,
  Zylinder, Drehkörper (auch Teilbögen), Profil entlang eines Pfads (rotationsminimierende Rahmen), Prisma, Pfade mit
  Biegeradius, Catmull-Rom-Pfade, Profile mit Gehrung (Bilderrahmen, Sockelgesimse).
- Normteile: Sechskantschrauben und Muttern, Nieten, Schlitzschrauben, Vorschweißflansche mit Dichtung, Handräder,
  Schweißraupen, Bügelschellen, Profilschienen.
- Gebrauchsspuren sind eigene Flächen 0,2–1,5 mm über der Oberfläche: Rost, Abplatzer, Läufer, Öl, Staub, Ruß,
  Kalk, Griffspuren, Kratzer. Auf Zylindern liegen sie als gewölbte Flecken (`cdecal`, `cpoly`).
- Schrift und Zeichen kommen aus dem Zeichenatlas des Spiels (`look=sign`/`stencil` mit UV). Beispiele:
  „UV-3“/„NV-1“/„F1–F3“ an den Schränken, „0/I“ am Hauptschalter, Skalen „A/V/KW“, Etagenziffern am Aufzug,
  „AGGREGAT 2“, Schablone „L3-07“, Warndreieck, Zettel.
- Skulpturen (Engel, Torso, Schädel) entstehen mit dem Distanzfeld-Werkzeug der Wesen (`creatures/sdf.py`, OpenVDB):
  - anatomische Teile und eigene Felder: das Gewand als Loft mit Falten, der Flügel als gehauene Platte,
    Bruchflächen als raue Halbräume;
  - Verwitterung durch Poren und Unebenheit;
  - danach Reduktion; Materialien nach Höhlung (Schmutz in Falten, Staub auf Oberseiten).
- Tücher entstehen mit der Stoffsimulation von Blender: Das Tuch fällt über verdeckte Möbel oder hängt an einer
  Latte.
- Rauheit und Metallizität sind je Material über `r=`/`m=` gesetzt. Stein ist matt (r 1,3–1,5), Lack halbmatt,
  Edelstahl teilmetallisch (m 0,45), damit er unter der Lampe nicht schwarz wird.

## Klassen

Maße: Tiefe (x) × Breite (y) × Höhe (z) in m, Variante 0. Dreiecke je Variante. „Stück“ = Anzahl in der Karte nach
dem Merge (1ab6819). Bögen: `docs/screens/props/level3/vorher/<name>.jpg` und `.../final/<name>[_vN].jpg`.
Nahansichten für Skulpturen und hohe Objekte: `.../final/<name>_nah.jpg`.

| Klasse | Var. | Befestigung | Maße | Dreiecke vorher | Dreiecke nachher | Stück | Mat. | Was das Modell jetzt hat |
|---|---|---|---|---|---|---|---|---|
| angel_statue | 1 | Boden | 1.56 x 1.56 x 3.53 | 13012 | 35905 | 1 | 8 | siehe unten (Engel) |
| bars_w2 / bars_w3 | 1 / 1 | Durchbruch | 0.09 x 2.30 / 3.30 x 4.2 | 800 / 1052 | 9480 / 13812 | 3 / 2 | 5 | Rundstäbe 30 mm durch liegende, gelochte Flachstahlriegel, Schweißraupen (sichtbare Seiten), ein ausgebeulter Stab, Rostmanschetten am Fuß, Rostläufer unter den Nähten, Abplatzer, Staub auf den Riegeln; reichen in Boden, Decke (4,05 m) und Laibungen |
| boiler | 1 | Boden | 3.20 x 1.78 x 3.25 | 496 (Code) | 21014 | 3 | 24 | Flammrohrkessel: drei Schüsse mit Überlappung, Rund- und Längsnähte mit Nieten, gewölbte Böden, Stirnplatte mit Feuer- und Aschentür (Scharniere, Riegel, Luftschieber), Wasserstandsglas, Manometer am Siphon, Dampfdom mit Absperrventil, gedämmte Dampfleitung bis zur Decke, Hebelsicherheitsventil, Mannloch, Speise- und Abschlammleitung, gemauerte Sättel; Ruß, Kalk, Rost |
| cable_tray | 4 | Decke | 1.06 x 0.43 x 0.35 | 144 (Code) | 1463/1495/1236/2016 | 786 | 14 | Kabelleiter (C-Holme mit Langlöchern, Sprossen alle 25 cm), 11 Kabel in festen Lagen, zwischen den Sprossen durchhängend, oben Staub; Stoßlasche, Traverse (Gewindestangen, Profilschiene, Muttern). v1 Binder und aufgeschnittene Isolierung, v2 durchgerutschte Schlaufe ohne Hänger, v3 Abzweigdose an der Decke |
| canvas_drape_a | 1 | Boden | 2.23 x 0.99 x 1.08 | 2676 | 13524 | 2 | 3 | Leinen über Schreibtisch mit Karton und Stuhl (Stoffsimulation), Tisch- und Stuhlbeine sichtbar |
| canvas_drape_b | 1 | Boden | 2.75 x 1.23 x 1.58 | 2676 | 16696 | 1 | 2 | Leinen über hohem Schrank und Tisch, Saum liegt auf |
| canvas_wall | 1 | Wand | 0.40 x 3.16 x 2.80 | 960 | 14320 | 2 | 4 | Bahn an genagelter Latte gerafft, fällt in Falten, liegt unten auf, feuchter Saum |
| ceiling_globe | 1 | Decke | 0.30 x 0.30 x 0.61 | 640 (Code) | 3465 | 8 | 5 | Bronzerosette, Kette mit Gliedern, Galerie mit drei Rändelschrauben, Opalglaskugel mit Hals (leuchtet), unten Staub und Insekten |
| column_greek | 2 | Boden | 1.24 x 1.24 x 7.90 | 3952 | 12240/11958 | 12 | 3 | ionisch: Plinthe, attische Basis, Schaft mit Schwellung und 24 Kanneluren mit Stegen, Trommelfugen, Perlstab, Eierstab mit Pfeilen, Polster mit Gurt, Voluten mit Auge, Abakus, Kämpfer; Schmutzsaum, Abplatzer. v1: Trommel versetzt, Volute abgebrochen, Riss |
| conveyor | 2 | Boden | 5.81 x 0.92 x 1.24 | 1340 | 7538/5102 | 18 | 18 | v0 Gurtförderer (Trommeln mit Stehlagern, Trag- und Rückführrollen, Getriebemotor, gelber Kettenschutz, Reißleine mit Not-Halt, Kartons mit Laschen, Band, Etikett); v1 Rollenbahn mit Endanschlag, Lattenkiste, Jutesäcken, Karton |
| elevator_door | 1 | Durchbruch | 1.06 x 2.12 x 2.72 | 400 | 1803 | 1 (+ Level 5) | 15 | Edelstahlzarge mit Profil, ausgekleidete Laibung, Schwellen mit Führungsnut, Teleskopblätter in den Taschen (Gummikante sichtbar), Zeigerwerk mit Etagenziffern, Richtungspfeile, Ruftableau mit Leuchtringen, Zettel mit Klebeband |
| generator | 2 | Boden | 3.25 x 1.82 x 2.28 | 1056 | 14540/14658 | 16 | 17 | v0 Dieselaggregat (Kühler, Block, Ventildeckel, Einspritzleitungen, Krümmer, Wellenbalg, Schalldämpfer, Generator mit Rippen, Klemmenkasten, Kabel, Schalttafel mit ausgebautem Instrument); v1 Umformersatz der 1930er (Grundbett, Lagerböcke mit Ölern, Lochblechschutz, offener Kollektor mit Bürstenbrücke, Feldsteller, Instrumententafel) |
| grate_stair | 1 | Boden an Wand | 8.74 x 1.46 x 5.18 | 684 | 15520 | 1 | 8 | U-Wangen, Gitterroststufen (Trag- und Querstäbe, Stirnbleche mit Schrauben, gelbe Trittkante), Podest auf Trägern und vier Stützen, Geländer mit Knieleiste und Fußleiste, Wandhandlauf auf Konsolen |
| greek_torso | 1 | Boden | 0.55 x 0.61 x 1.61 | 1060 | 13000 | 2 | 4 | Frauenstatue ohne Kopf und Arme: Chiton am Oberkörper, Himation mit tiefen Falten, Knie des Spielbeins, Zehen, Bruchflächen an Hals und Armstümpfen, Ausbruch unten, Poren, Schmutz in Falten |
| painting_angel | 2 | Wand | 0.05 x 1.13 x 1.48 | 482 | 2087/1568 | 16 | 28 | Rahmen mit Profil und Gehrung (v0 Blattgold mit Poliment und Eckornamenten, v1 Nussbaum), wellige Leinwand, gemalt in Schichten (Lichtschein bzw. Nachthimmel mit Sternen, Flügel mit Federreihen, Gewand mit Faltenschatten, Mantel blau bzw. rot, Hände, Gesicht, Nimbus aus Gold), v1 mit Lilie; Firnis am Rand nachgedunkelt, Craquelé, Fehlstellen, Riss |
| painting_angel_scribbled | 2 | Wand | 0.05 x 1.13 x 1.48 | 650 | 2231/1672 | 15 | 29 | wie oben, Gesicht übermalt (LEVELSPEC_3): v0 breite schwarze Pinselstriche mit Läufern, v1 rotbrauner Fleck mit Läufern |
| pipe_black | 2 | Wand (Karte: Boden) | 0.66 x 5.82 x 2.30 | 696 | 12535/8000 | 25 | 19 | v0 DN150/DN100 auf Wandkonsolen mit Bügelschellen, Flansche, Absperrschieber mit Handrad, Bögen in die Wand mit Mauerkragen, Leck mit Läufen, Tropfen und Lache, Wandlauf, Fließpfeil auf gelber Binde; v1 Gussleitung DN200 auf Rohrböcken (Bogen in den Boden), Inspektions-T mit Blindflansch, Manometer, dünne Leitung darüber |
| sign_warning | 1 | Wand | 0.02 x 0.41 x 0.52 | 215 | 1370 | 6 | 8 | Emailleblech mit Wulstrand, umgebogene Ecke, schwarzer Rahmen, Auge im Dreieck mit Wimpern, Schmutz unten, Schrauben mit Abplatzern und Rostläufern, Rostlauf auf der Wand |
| skull | 1 | Boden | 0.23 x 0.14 x 0.18 | 884 | 7999 | 2 | 4 | Hirnschale mit Naht, Stirn, tiefe Augenhöhlen, Nasenöffnung, Jochbögen, Schläfengruben, Warzenfortsätze, Ober- und Unterkiefer mit Zähnen (einer fehlt), Poren; Knochen vergilbt, Schmutz in den Höhlen |
| sludge_pool | 1 | Boden | 2.58 x 1.91 x 0.02 | 40 | 2406 | 3 | 3 | drei Lachen mit gewölbter Oberfläche, Blasen, matter Haut, Kruste mit Rissen, Spritzer |
| switch_cabinet | 3 | Boden an Wand | 0.55 x 1.28 x 2.00 | 366 (Code) | 2662/2666/3516 | 26 | 27 | v0 grauer Zweitürer (Instrumente, Hauptschalter, Meldeleuchten, eine leuchtet, Beschriftung, Warnzeichen); v1 schmaler Eintürer auf Winkelfüßen, ockeroliv, großer Drehschalter, ein Instrument fehlt, Kabel von unten; v2 blaugrauer Dreitürer mit Lüftungshaube, Sichtfenstern, Leuchtmelderreihe und Not-Aus. Alle: Spalte mit Dichtung, Scharniere, Schwenkhebel, Kiemen, Erdungsleitung grün-gelb, Abplatzer, Rost unten, Griffspuren, Kratzer, Staub |
| train_bogie | 1 | Boden | 2.76 x 1.66 x 0.87 | 1724 | 13772 | 1 | 5 | Meterspur: Radsätze mit Spurkranz, Lauffläche und gewölbter Scheibe, Achslager mit Deckel, genietete Seitenrahmen mit Gurtplatten und Achshaltern, Wiege mit Drehpfanne, Schraubenfedern, Bremsbalken und -klötze; durchgerostet |
| window_brick | 3 | Wand | 0.16 x 1.80 x 1.35 | 588 | 2494/2866/3828 | 33 | 17 | Stahlsprossenfenster vor zugemauerter Öffnung (Ziegel im Verband, Fugen, Ausblühungen), Scheiben heil/gesprungen/zerbrochen/fehlend, Kitt, Griff, Betonsims; v1 Scheiben von innen gekalkt, v2 mit Schutzgitter aus Drahtgeflecht |

Nicht neu gebaut: `bars_w1` (in keiner Karte, mit gebaut), die Klassen anderer Ebenen in Level 3 (Röhren, Leuchten,
Maschinenschränke, Verteiler, Kisten, Tische …).

**Engel** (`angel_statue`, Wahrzeichen des Sanktuariums)
- Sockel (Netz): Plinthe mit Karnies, Würfel mit vertieften Feldern; darin 21 Zeichen je Seite, eingeschlagen und
  verschmutzt (erfunden, nicht lesbar). Gesims, Deckplatte mit Staub.
- Figur 2 m (Distanzfeld):
  - Gewand mit hoher Gürtung (Kordel) und Falten, die zum Saum tiefer werden; das Knie des Spielbeins zeichnet sich
    ab, der Saum liegt auf, Zehen schauen heraus; Rollkante am Halsausschnitt.
  - Arme in Ärmeln mit Hangfalte und hohler Öffnung; betende Hände mit einzelnen Fingern.
  - Kopf 22° geneigt: geschlossene Lider, gerade Nase, Lippen, Haar mit Mittelscheitel und Strähnenwellen.
  - Gesicht verwittert, nicht zerkratzt (LEVELSPEC_3).
- Flügel: je eine gewölbte, gehauene Steinplatte (Umriss eines gefalteten, erhobenen Flügels). Oben drei Reihen
  Deckfedern als Schuppen, nach innen größer; darunter Schwingen mit Furchen und Kiel, die von der Hand auffächern;
  innen senkrechte Armschwingen; ausgezackter Rand aus Federspitzen.
- Travertin matt; Schmutz in Falten, Staub auf Oberseiten, Poren.

## Varianten und Karte

Häufig gesetzte Klassen haben jetzt mehrere Bauarten mit eigener Silhouette:
- Schaltschrank 3;
- Generator 2 (Diesel/Umformer);
- Fenster 3;
- schwarze Rohre 2;
- Förderband 2 (Gurt/Rollen);
- Säule 2;
- beide Engelsbilder je 2;
- Kabelleiter 4: war schon zufällig, alle Varianten mit gleichen Kabellagen an den Enden, damit Abschnitte
  aneinanderpassen.

**Karte** (`tools/levels/level3.py`):
- Nach allen Platzierungen wird je Objekt `variant = rng.randrange(n)` gezogen. Die Liste `VARIANTS` enthält die
  Klassen oben und die Varianten aus Level 2 (`mach_cabinet` 3, `junction_box` 4).
- Die Anordnung bleibt unverändert. Vergleich der Karte vorher/nachher: 1296 Objekte, 0 mit anderer Lage, Räume und
  Türen gleich.
- Verteilung:
  - Schrank 9/8/9, Generator 4/12, Fenster 7/14/12, Rohre 12/13, Förderband 10/8, Säule 7/5;
  - Bild 6/10, übermaltes Bild 7/8;
  - Maschinenschrank 14/11/6, Verteiler 4/9/4/2, Kabelleiter 194/198/209/185.

## Runden

Studio-Bögen mit `tools/qa/propsheet.py` (nur ein Zusatz in `scratchpad/l3sheet.py`: `pipe_black` steht in
`mapkit.FLOOR_MOUNT`, hängt im Spiel aber an der Wand und wird im Studio deshalb an die Wand gestellt). Nahansichten
(`scratchpad/detail.py`): gleiches Studio, Kamera in Modellkoordinaten, je Ansicht Licht der Ebene und streifende Lampe.

### Vorher (V12-/Code-Fassung)
Übersicht `docs/screens/level3/props_uebersicht_vorher.jpg`, Bögen `docs/screens/props/level3/vorher/`. Zuerst blieben
Kabinen jenseits von x ≈ 95 schwarz, weil die Welt nur um den Spieler lädt. Behelf: Stapel; später kam der Fix in
propsheet.py aus liminal-v13.
- **angel_statue**: Kegel mit Zylinderfalten als Gewand, Flügel als Bretter, Kugelkopf ohne Gesicht, Zeichen als
  Stäbchen und Ringe.
- **bars**: glatte Zylinder, einzelne Stäbe ganz rostbraun, Riegel als Leisten, keine Nähte.
- **boiler**: Zylinder ohne Böden (innen sichtbar), Klötze als Füße.
- **cable_tray**: Blechwanne, Kabel von unten unsichtbar.
- **canvas_***: Höhenfeld über schwarzem Kasten, Wandbahn ohne Befestigung.
- **ceiling_globe**: Leuchtkugel am Stab.
- **column_greek**: Schaft brauchbar, Kapitell aus Zylindern, keine Basisprofile.
- **conveyor**: Band als Platte, Kartons als Quader.
- **elevator_door**: Platten, Taster als Scheiben.
- **generator**: grüner Kasten mit Schlitzen.
- **grate_stair**: Stufen als Platten.
- **greek_torso**: Drehkörper.
- **paintings**: Farbflächen, Rahmen aus vier Kästen, Kritzel als Stäbchen.
- **pipe_black**: zwei glatte Rohre, Lauf als Stab, Lache als Sternpolygon.
- **sign_warning**: Quadrat mit Dreieck.
- **skull**: Kugel und Kasten.
- **sludge_pool**: flaches Sternpolygon.
- **switch_cabinet**: Kasten mit zwei Türen.
- **train_bogie**: Zylinderräder an Kästen.
- **window_brick**: deckendes Glas; die Ziegel lagen hinter der Wandfläche und waren unsichtbar.

### Neubau und Testbögen (vor r1)
Bögen `docs/screens/props/level3/dev/` (nicht eingecheckt).
- **Schaltschrank**: Mit Lacktextur `painted_metal_white` sah er aus wie ein Dalmatiner. Jetzt Lack ohne Textur,
  Gebrauchsspuren als Flecken.
- **Kabeltrasse**: Die Rinne verdeckte die Kabel von unten. Jetzt Kabelleiter; die Kabel hängen zwischen den Sprossen
  durch und sind von unten zu sehen.
- **Schlamm**: Die Kruste war zu breit und zu hell, die Haut fleckig.
- **Objektprüfung**: Entartete Dreiecke (Fasen dicker als Teile) beseitigt der Kit jetzt selbst. Gekippte Normalen
  am Lachenrand (Rand unter dem Boden) sind behoben.
- **Engel, erster Wurf**:
  - Kopf mit Kugelaugen und abstehenden Locken („Tentakeln“), Arme als Klotz, Flügel als Lamellen.
  - Kopf neu (Lider in Höhlen, Haarkappe mit Wellen statt Röhren).
  - Arme schlank mit Hangfalte.
  - Flügel als gehauene Platte mit Federrelief.
- **Torso**: Armstümpfe als Pilzhüte, Mantelrolle als Schlauch.
- **Schädel**: Wangenknochen als Kugeln.
- **Merge liminal-v13** (drei neue Ziegel-Gangnetze): 786 Kabeltrassen statt 337. Die Ausstattung der Ebene lag bei
  3,87 Mio. Dreiecken. Die Kabelleiter wurde verschlankt (2,3k → 1,2–2,0k: dünne Kabel mit 4 Seiten und 4 Punkten,
  Sprossen ohne Schrauben, Traverse ohne Scheiben), die Rohre ebenso. Danach 2,94 Mio.

### Runde r1 (alle 37 Bögen, mit Varianten)
Übersicht `docs/screens/level3/props_uebersicht_r1.jpg`.
- **Tücher**: Flecken- und Staubmaterial je Fläche ergab auf dem Simulationsraster ein Pixelmuster.
  - Änderung: Staub nur auf flachen Oberseiten ohne Rauschen.
- **Skulpturen**: Schmutzflecken mit Treppenkanten, auf dem Gewand ein Tarnmuster.
  - Änderung: Kontrast der Steinmaterialien gesenkt (0,62 → 0,74), Flecken seltener.
- **Rollenbahn v1**: Säcke flach wie Fladen.
  - Änderung: Säcke höher und gestaucht.
- **Drehgestell**: Seitenrahmen als glatte Platten.
  - Änderung: Gurtplatten und Nietreihen, kleinere Rostflecken.
- **Kabelleiter, Kessel, Generatoren, Schränke, Treppe, Säulen, Schild, Leuchte**: ohne größeren Befund. Die
  Varianten sind klar verschieden.

### Runde r2 (alle 37 Bögen)
Übersicht `docs/screens/level3/props_uebersicht_r2.jpg`.
- **Tücher**: Staubinseln weiterhin als Raster sichtbar.
  - Änderung: Tücher nur noch einfarbig, die Wandbahn mit feuchtem Saum.
- **Fenster v2**: Das Schutzgitter fehlte. Ursache: Die Schleifenvariable `v` überschrieb die Variantennummer.
  - Änderung: Variable umbenannt; Gitter jetzt da (3828 Dreiecke).
- **Bild v1**: Der Nachthimmel war fast schwarz.
  - Änderung: aufgehellt.
- **Gitter**: Rost aus 2 m kaum zu sehen.
  - Änderung: Rostmanschetten höher, Läufer breiter; nicht sichtbare Schweißraupen weggelassen.
- **Engel** (Nahansicht in Blender): Stirn zu hoch, Haaransatz zu weit hinten; Scheitel als Loch vorn.
  - Änderung: Haarkappe nach vorn, Scheitel nach hinten, Unterlippe kleiner.

### Runde r3 (12 Bögen der geänderten Klassen, dazu Nahansichten im Spiel)
Übersicht `docs/screens/level3/props_uebersicht_r3.jpg`, Nahansicht `docs/screens/props/level3/r3/angel_statue_nah.jpg`.
- **Tücher, Wandbahn, Fenster v2, Bilder**: in Ordnung.
- **Engel** im Spiel:
  - Schmutz in Augen und Mund wirkt wie Verletzung, Lider liegen zu tief.
  - Auf der Brust eckige helle Flecken; die Brust wirkt glatt.
  - Änderungen:
    - Steinflecken beim Engel abgeschaltet; Schmutz erst ab größerer Höhlung (Kopf 0,66, Körper 0,55).
    - Lider 2 mm nach vorn.
    - Falten am Oberteil stärker.
- **Torso**: Schultern wie Puffärmel; Rillen am Bauch wie Schlitze.
  - Änderung: Schultern kleiner; Armstümpfe schräg nach außen mit Bruchfläche quer zur Armachse; Bauchrillen und
    Nabel entfernt.

### Runde r4 (Engel, Torso, Gitter; Nahansichten)
Übersicht `docs/screens/level3/props_uebersicht_r4.jpg`, Nahansichten
`docs/screens/props/level3/r4/angel_statue_nah.jpg`, `.../r4/greek_torso_nah.jpg`.
- **Engel**: Gesicht ruhig und hell, klassisch, keine Verletzungsflecken mehr. Die Zeichen im Sockel lesen sich als
  eingeschlagen.
- **Gitter**: Rost unten jetzt deutlich.
- **Torso**: Neben der Taille schwebten kleine Reste der Armstümpfe; der Bruch lag knapp außerhalb der Schnittkugel.
  - Änderung: Stümpfe kürzer und schräg; der Bruch liegt ganz in der Schnittkugel.

### Runde r5 (Torso)
Bogen `docs/screens/props/level3/r5/greek_torso.jpg`.
- Torso jetzt mit gebrochenen Oberarmen ohne Reste.
- Stein glänzte unter der Lampe (Marmor mit r 1,0).
  - Änderung: alle Steinmaterialien der Skulpturen matt (r 1,3–1,5).

### Schlussrunde final (alle 37 Bögen, Nahansichten)
Übersicht `docs/screens/level3/props_uebersicht_final.jpg`, Bögen `docs/screens/props/level3/final/`, Nahansichten
`.../final/<klasse>_nah.jpg` (Engel, Säule v0/v1, Torso, Schädel, übermalte Bilder v0/v1, Treppe).
- Keine Rückschritte gegenüber r2 bis r5. Die Varianten sind nebeneinander klar verschieden.
- **Engel**: Gesicht ruhig, Hände lesbar, Flügel mit gehauenen Federn, Zeichen eingeschlagen.
- **Säule**: Kapitell mit Voluten und Eierstab, Basis mit Wülsten und Kehle.
- **Treppe**: Die Schmutzflecken auf den Gitterroststufen wirkten wie Löcher. Danach entfernt; der Bogen
  `final/grate_stair.jpg` ist neu gerendert (die Nahansicht zeigt noch die Flecken).
- **Torso**: Die Armstümpfe sehen von vorn aus 50 cm noch wie kleine Kappen aus (Restmangel).

## Prüfungen
- `python tools/qa/validate_models.py` (alle eigenen Klassen mit Varianten): 38 Modelle, 0 Fehler.
- `gameplay_test`: 525 Prüfungen, 0 Fehler. „Halt der Ausstattung“: level3 497 Objekte, 0 ohne Halt; gesamt 10493,
  0 ohne Halt.
- `python tools/qa/room_metrics.py --check level3`: Raummetrik OK (Decke min/Median 2,60/2,90 m, Gang 2,0 m,
  verstellt 0/11).
- **Dreiecke der Ausstattung von Level 3** (alle 1302 Objekte, propdump): 2,94 Mio. (vor dem Merge 0,31 Mio. für die
  eigenen Klassen; jetzt 2,54 Mio. für die eigenen Klassen). Größter Posten: Kabelleiter, 786 × 1,2–2,0k = 1,21 Mio.
- Karte: nur Varianten geändert (siehe oben).

## Restmängel (ehrlich)
- **Aufzugstür in Level 3**: Der Durchbruch in der Karte ist 1 m breit, das Modell für 1,4 m gebaut (passt in Level 5
  mit Skalierung 1,35 in einen 2-m-Durchbruch). In Level 3 liegen die Laibungsbleche in der Wand; zu sehen sind Sturz,
  Schwelle, Zarge und Anzeige. Im Studio (3-m-Durchbruch) stehen die geparkten Türblätter sichtbar frei.
- **Schwarze Rohre**: Die Karte führt `pipe_black` als Bodenobjekt; `propsheet.py` stellte es frei in die Kabine.
  Die Bögen entstehen mit einem Zusatz, der es an die Wand stellt.
- **Glas ist deckend**: Der Ziegel hinter dem Fenster ist nur in gesprungenen, zerbrochenen und fehlenden Scheiben zu
  sehen. Gelöst als „von außen zugemauertes Fenster“.
- **Kabelleiter**:
  - Alle Varianten führen die Kabel an den Enden in denselben Lagen, sonst passen die Abschnitte nicht aneinander.
    Die Vielfalt liegt in der Mitte (Schlaufe, Dose, Binder, Hänger).
  - Am Ende einer Trasse sind die Kabel offen geschnitten.
- **Skulpturen**:
  - Materialien sind je Fläche nach der Reduktion gesetzt. Schmutzränder haben Treppenkanten, darum ist der Kontrast
    niedrig gehalten.
  - Gesicht und Hände sind aus Grundformen zusammengesetzt, nicht frei modelliert.
  - Die Flügel sind eine Platte mit Relief; ihr Umriss ist polygonal.
- **Tücher**: ohne Flecken und Staub. Flächenweise Materialien zeigten das Simulationsraster.
- **Maße**: canvas_drape_a ist schmaler als vorher (Tuch liegt eng am Tisch, y ±0,5 statt ±0,85). Die Wandbahn ragt
  mit dem aufliegenden Saum 0,4 m in den Raum (vorher 0,07). Die Raummetrik bleibt grün.
- **Dreiecke**: angel_statue 35,9k (großes Einzelstück, unter 40k). bars_w3 13,8k (2 Stück, Grenze „mittel“ 12k
  überschritten). Die Kabelleiter macht 41 % der Ausstattung der Ebene aus.
- **Kontaktbögen**: Die Kameras zielen auf die Mitte der Hülle. Kapitell, Engelskopf und Treppenpodest sind nur in den
  Nahansichten beurteilt.
- Die Bilder sind gewollt einfache Farbflächenbilder (LEVELSPEC_3, eigene Motive); Pinselstruktur gibt es nur als
  Craquelé und Fehlstellen.
- Marmor (Säule, Torso) glänzt unter der streifenden Lampe noch etwas. Die Grenze des Schmutzsaums an der
  Säulenbasis verläuft als gestufte Linie.
- Die Armstümpfe des Torsos lesen sich von vorn als kleine Kappen, nicht als gebrochene Oberarme.

## Aufwand
- 27.10.: etwa 6 Stunden (Werkzeugkasten, erste 15 Klassen, Kontaktbögen vorher, Behelf für schwarze Kabinen).
- 01.11.: etwa 10 Stunden (Bilder, Tücher, Skulpturen, Merge, Varianten, Karte, Runden r1–r5 und final, Prüfungen,
  Dokument).
- Runden: vorher, Testbögen, r1, r2 (je alle 37 Bögen), r3–r5 (geänderte Klassen), final (alle 37); Nahansichten in
  r3, r4, final. Dazu etwa 25 Blender-Vorschauen der Skulpturen.
