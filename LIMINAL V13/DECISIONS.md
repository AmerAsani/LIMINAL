# LIMINAL V12 – Entscheidungen

Arbeitsmodus: autonom, ohne Rückfragen (Auftrag vom 25.10.2026, „Autonomes Neudesign“). Jede Entscheidung steht hier mit Datum und Grund. Gilt eine Unklarheit, wird die Variante mit höherer Qualität und besserer Lore-Treue gewählt.

## E-001 Arbeitsort und Versionen (25.10.2026)
- V12 liegt in `LIMINAL V12`, kopiert aus V11 (ohne den alten Build-Ordner), auf dem Git-Branch `liminal-v12`. Commit nach jedem abgeschlossenen Teilschritt, kein Push.
- **V11 und V10 bleiben unverändert.**
  - V10: Manifest-Vergleich am 25.10.2026 23:20, 18 880 Dateien, 0 Unterschiede.
  - V11: Beim Einfrieren wurde ein Manifest erzeugt (Scratchpad `v12/v11_frozen.txt`, 18 687 Dateien); am Ende wird erneut verglichen.
  - V11 ist im Repository nicht eingecheckt (das war schon vorher so) und wird von V12 aus nicht eingecheckt.
- V12 hat eigene Spielstände (`saves_v12`), Einstellungen (`settings_v12.json`, beim ersten Start aus V11 übernommen) und Protokolle (`liminal_v12.log`).

## E-002 Audit und Entscheidung je System (25.10.2026)

| System | Befund in V11 | Entscheidung | Grund |
|---|---|---|---|
| Engine (C++20, D3D11, Deferred, TAA, GTAO, SSR, Volumetrie) | stabil, schnell (GPU ~4,5 ms bei 1600 × 900) | **behalten** | Kein Engine-Wechsel ohne zwingenden Grund (`ENGINE_DECISION.md`). Die Darstellung ist nicht der Engpass, sondern Geometrie, Materialien und Modelle. |
| Weltstruktur (Kachelwelt, 1-m-Raster) | Kollision, Navigation, Schatten, Schallweg und Licht beruhen darauf. Sie erzwingt rechte Winkel, flache Decken je Raum und kistenartige Räume. | **umbauen** | Bleibt als Gerüst für Kollision und Simulation. Darüber kommt eine **Schicht aus Detailmodellen** (glTF aus Blender): Sockel, Rahmen, Fasen, Unterzüge, Leuchtenkörper, Pfeiler, Bögen, Kassetten. Schräge oder runde Wände werden als Modell gezeigt, Kollision über unsichtbare Kacheln (gibt es schon: `hidden`). Ein kompletter Ersatz der Kachelwelt hieße, Kollision, Licht, Schatten, Navigation und Schallweg neu zu schreiben – in dieser Arbeit nicht in hoher Qualität machbar. |
| Level-Aufbau (`mapkit.py`, feste Karten, Prüfungen) | gut: entworfen, geprüft (Wände, Erreichbarkeit, Halt) | **behalten, erweitern** | Neue Level-Skripte je ausgewähltem Level; mapkit bekommt Detailmodelle und neue Prüfungen. Die alten Level 1–37 sind nur Referenz. |
| Materialien (prozedurale Texturen, 43 Ebenen) | erkennbar synthetisch, teils flächig | **ersetzen**, wo es besser wird | CC0-PBR-Texturen (ambientCG) als neue Ebenen: Albedo, Normalen, Rauheit, AO. Ein Importweg (`data/textures/<name>/`) speist sie in denselben Texturblock. Prozedurale Ebenen bleiben nur, wo sie überzeugen (z. B. Schilder-Atlas). |
| Beleuchtung | physikalisch brauchbar, Leuchten je Raum geplant | **behalten**, je Level neu abstimmen | Lichtrhythmus kommt aus `LOOK_<Nr>.md`. |
| Kreaturen (aus Code-Primitiven geformt) | trotz Arbeit erkennbar aus Grundkörpern | **ersetzen** | Organische Formen entstehen in Blender: Skin-Modifier, Subdivision, Displacement, Remesh, UV, Normal-Map-Bake. Für menschenähnliche Körper kommt das MakeHuman-Basismodell (CC0) als Grundlage in Frage. Ausgabe als glTF. Nur Wesen, die das Wiki für das jeweilige Level belegt. |
| Viewmodel (Hand, Arm, Lampe) | Griff in V11 verbessert; die Hand ist aber ein Skelett-Modell mit einem Fingerglied | **ersetzen** | Hand und Lampe als in Blender gestelltes Modell (Faust um den Schaft), am Handgelenk befestigt; Unterarm aus dem Rig. Kegel = Modellachse (Logik aus V11). |
| Oberfläche | in V11 überarbeitet (Palette, Listen, HUD) | **umbauen** | Weiterentwickeln, nicht nochmals von vorn; bei mehreren Auflösungen prüfen, Notizen und Einstellungen einbeziehen. |
| Audio (Synthese, FDN-Hall, Kopfschatten, Schallweg, Begrenzer) | technisch stark | **behalten**, Klanglandschaft je neuem Level | Eigene Schichten je Level, Stille bewusst. |
| Mehrspieler | – | **nicht bauen** | Auftrag; nur `MULTIPLAYER.md`. |

## E-003 Modell- und Asset-Pipeline (25.10.2026)
- **Blender 4.5.14 LTS** (portable ZIP von download.blender.org, 398 661 046 Byte). SHA-256 `b9533d2397ac1984db4466fb23a7a4649391cca93f6e84209f9bcc60d071c8b9`, geprüft gegen die offizielle `.sha256`-Datei.
  - Liegt in `.toolchain/blender/` (nicht im Repository, wie der Compiler) und läuft im Hintergrundmodus (`blender -b --python …`).
  - Lizenz: GPL; betrifft nur das Werkzeug, nicht die damit erzeugten Modelle.
- **Format:** glTF 2.0 binär (`.glb`). Gelesen von einem **eigenen kleinen Loader** im Spiel, ohne fremden Code, damit keine zusätzliche Lizenz entsteht.
- **Texturen:** ambientCG, Lizenz CC0 1.0 („All ambientCG assets are provided under the Creative Commons CC0 1.0 Universal License“, docs.ambientcg.com/license, abgerufen 25.10.2026). Jede verwendete Textur kommt mit ID, URL, Abrufdatum und Prüfsumme nach `LICENSES.md`.
- **Menschliche Grundform** (Hand, menschenähnliche Wesen): MakeHuman-Assets, CC0 laut FAQ von MakeHuman („The asset license is CC0“, abgerufen 25.10.2026). Nur Basismodell und Ziele, keine Fremd-Assets von Dritten.
- Erzeugte Modelle liegen als `.glb` unter `data/models/`. Die Blender-Skripte, die sie bauen, liegen unter `tools/blender/` und sind reproduzierbar.

## E-004 Levelauswahl (25./26.10.2026)
- 21 Kandidatenseiten vollständig gelesen (`LEVELAUSWAHL.md`).
- Gewählt: **0, 1, 2, 3, 5, 5.3, 188, 903, 37, 36**. Ersatz in dieser Reihenfolge: 6.1, 141, 115, 7.
- Seiten mit „outdated“ oder „in rewrite“ wurden gemieden, wo es gleichwertige stabile gab (6, 7, 15, 21, 33, 34, 54).
- Reise: 0 → 1 → 2 → 3 → 5. Von 5 aus Abzweige nach 5.3 und 188, jeweils mit Rückweg. Dann ein Metro-Wagen nach 903, von dort nach 37 und 36. Die Gate-Tür in 36 ist das Ende.
- Nicht belegte Verbindungen nutzen den belegten Mechanismus des Metro-Wagens und sind als eigene Ergänzung markiert.

## E-005 Pipeline umgesetzt (26.10.2026)
- **Texturen:**
  - `tools/textures/fetch_ambientcg.py` lädt die Auswahl (`selection.json`, 2K-JPG) über die offizielle API, jede Datei in einen eigenen Ordner, mit SHA-256 im Lock (`ambientcg_lock.json`).
  - `process.py` macht daraus kachelbare 1024-px-Texturen in `data/textures/` (Albedo JPG, DirectX-Normalen PNG, AO/Rauheit/Metall/Höhe PNG) und `textures.json` mit der realen Größe aus den ambientCG-Maßen.
  - Im Spiel belegen sie bis zu 48 eigene Ebenen (`L_IMP0` …). Zonen und Raumtypen wählen sie mit `wall_tex`, `floor_tex`, `ceil_tex`; Modellteile mit `tex=` im Materialnamen.
- **Modelle:** Blender-Skripte unter `tools/blender/` (`common.py` mit Materialkonvention, `build_models.py` baut alle oder einzelne Modelle) schreiben `data/models/<name>.glb`.
  - Das Spiel liest sie mit einem eigenen glTF-Leser (`props/glb_import`) und meldet sie unter dem Dateinamen als Ausstattung an.
  - Ein gleichnamiges Code-Modell wird dabei überdeckt.
- **Koordinaten:** Das Spiel ist linkshändig (x vorn, y rechts, z oben), Blender und glTF sind rechtshändig. Der Leser spiegelt y und dreht den Umlaufsinn, damit ein Modell im Spiel so aussieht wie in Blender. Abgesichert durch `gameplay_test` (Abschnitt „Modelle aus Blender“).

## E-006 Level 0 (26.10.2026)
- **Quelle:** Wikidot „Level 0“, Revision 50 (`LEVELSPEC_0.md`), Bild in `LOOK_0.md`. Vier Runden Qualitätsschleife (`docs/screens/level0/`).
- **Wände:** Raufaser (Wallpaper001A, CC0) in stumpfem Senfgelb statt eines Chevron-Musters. Begründung: Die Seite nennt nur eine kränklich gelbe Tapete (siehe `LEVELSPEC_0.md`), kein Muster. Ein Muster wäre eine fremde Deutung.
- **Langer Flur:** ein Zickzack über drei Spuren statt einer geraden Röhre. So sieht man nie weiter als einen Abschnitt, und der Flur wirkt nicht wie aus einem anderen Spiel.
- **Säulenfeld:** raumhohe, tapezierte Blöcke im 4-m-Raster, versetzt, einige fehlen, einige doppelt breit. Das Wiki nennt Stützen im Gitter, die durch das Verschieben ungleichmäßig wurden.
- **Rasterdecke:** 0,6-m-Raster mit 0,6 × 1,2-m-Leuchten aus Blender (Prismen, Rahmen); etwa jede neunte ist tot, etwa jeder zwölfte Raum flackert. Dazu Luftauslässe und ein Winkelprofil an der Wand.
- **Feuchtigkeitsrand:** ein bündiger Verlauf in acht Stufen statt vorstehender Bänder. Neue Bandarten im Generator: 0 = Leiste, 1 = Anstrich, 2 = Deckenleiste.
- **Dunkelzone:** Boden 25 cm tiefer, knöcheltief Flüssigkeit, keine Leuchten und kein Summen (Wiki). Die Taschenlampe ist dort die einzige Lichtquelle.
- **Klang:**
  - Das Summen aller Leuchten schwillt in Abständen von 70–180 s für etwa 6 s auf das 2,4-Fache an (`hum_surge`, Wiki: das Summen schwillt manchmal an).
  - Flüstern und ferne Stimmen sowie Kratzen hinter den Wänden sind belegt.
  - Klopfen und Türen entfallen: Level 0 hat laut Wiki keine Türen.
- **Funde:** Mandelwasser (Wiki) und selten Batterien. Die Batterien sind eine eigene Ergänzung, weil die Dunkelzonen ohne Licht nicht begehbar wären. Energieriegel entfallen in Level 0.
- **Noch offen:** Die dunkle Gestalt bekommt ihr Modell in der Phase Wesen; Hand und Lampe folgen in der Phase Viewmodel. Beides wird für Level 0 danach erneut aufgenommen.

## E-007 Viewmodel: Hand und Taschenlampe aus Blender (26.10.2026)
- **Vorher (V11):** Die Hand war Teil der prozeduralen Spielerfigur: ein Fingerglied für alle Finger, klumpig, die Lampe ein glatter Zylinder. Bild: `docs/screens/viewmodel/00_vorher_v11.jpg`.
- **Jetzt:** eigene Modelle, kein fremdes Asset (`tools/blender/models/models_vm.py`).
  - **Lampe:** Drehkörper mit Endkappe, Gummiring, gerillter Griffzone, Gummischalter, Hals, Kopf mit Kühlrillen, Metallring und Glas. Maße und Lichtpunkt wie in V7, damit Halt und Lichtquelle gleich bleiben.
  - **Hand:** aus Ellipsoiden und Röhren nach Handmaßen gebaut, durch ein Voxel-Remesh vereinigt und geglättet, mit Nägeln.
- **Griff:** Kraftgriff. Die Lampe liegt schräg in der Hohlhand (56° zur Handachse), der Daumen oben am Schaft, das Glas tritt an der Daumenseite aus.
  - Ein kleiner Löser sucht die Fingerwinkel so, dass die Finger am Schaft anliegen.
  - Das Handgelenk ist um 33° zur Elle abgespreizt und um 15° gestreckt. Dadurch steht der Unterarm 27° zur Lampe und kommt von außen links.
  - Begründung: Mit dem Daumen oben und einem Unterarm in Lampenrichtung wäre die Lampe anatomisch nicht nach vorn zu halten. Der Griff muss also von der Seite kommen.
- **Einbau:** Hand und Lampe sind ein starres Teil im Rahmen der Lampe.
  - Die IK des Arms zielt auf das Handgelenk des Modells. Der Ellbogen zeigt dorthin, wohin der Unterarm des Modells führt.
  - Die Hand gleitet bis 8 cm entlang der Lampenachse, bis der Oberarm den Ellbogen erreicht. Rollen um die Achse hilft nicht, weil die Schulter fast auf ihr liegt.
  - Abweichung Unterarm zu Modell im Test höchstens 3,5°. Die Hand des Skeletts schrumpft ins Handgelenk.
- **Nebenbefund:** Die Handgelenk-Kapsel der prozeduralen Figur reichte 5,6 cm in den Unterarm und schwenkte bei erhobenem Arm durch den Ärmel. Das war schon in V11 sichtbar. Sie ist jetzt 1,5 cm lang, denn das Bündchen endet 1 cm vor dem Handgelenk.
- **Grenzen:**
  - Die Hand ist ein gestelltes Modell ohne eigene Fingeranimation.
  - Die rechte Hand und der übrige Körper bleiben die prozedurale Figur aus V10/V11.
  - Im Strichmännchen-Modus gibt es keine Hand, wie bisher.

## E-008 Wesen: V11-Modelle bleiben, Zuordnung streng nach Wiki (26.10.2026)
- **Geprüft:** Aufnahmen der V11-Wesen in Level 0 (Gestalt, Smiler, Hound, Deathmoth samt Nest, Clump).
  - Die Wesen sind prozedural modelliert, haben ein eigenes Skelett und eine Haut-Textur. Sie sind animiert und schon in Spielmechanik, Klang und Jumpscare eingebunden.
  - Gestalt, Smiler und Hound tragen das, was das Wiki über sie sagt (Silhouette; Augen und Zähne im Dunkeln; menschenähnlich auf allen vieren).
- **Entscheidung:** Der Ersatz durch Blender-Modelle mit Skinning ist zurückgestellt.
  - Nötig wären ein Skin-Leser für glTF, eine Abbildung auf die Wesen-Skelette und neue Animationen.
  - Der Gewinn je Aufwand ist geringer als bei den neun noch offenen Leveln.
  - Wird es nachgeholt, dann zuerst für den Hound (die schwächste Silhouette).
- **Regel je Level:** Nur Wesen, die die Wiki-Seite des Levels nennt. Die Regie (`presence`) wird je Level darauf beschränkt.
  - Level 0: nur die dunkle Gestalt; sie greift nie an.
  - Nicht als Modell vorhandene Wesen (Facelings in 903, Fensterwesen in 188) bekommen eigene, einfache Lösungen. Was davon fehlt, steht ehrlich im Bericht.
- **Level 0:** Gestalt-Aufnahmen in `docs/screens/level0/13–15`.

## E-009 Level 1 (26.10.2026)
- **Quelle:** Wikidot „Level 1“, Revision 83 (`LEVELSPEC_1.md`), Bild in `LOOK_1.md`. Drei QA-Runden (`docs/screens/level1/`).
- **Neu statt V11:** Die V11-Karte (farbige Parkdecks, Rampen, Autos, Rolltore, Wartungsflure) enthielt Dinge, die die Seite nicht beschreibt. Ersetzt durch:
  - drei der genannten Hallen-Sektoren: Aquila (Ankunft, grau, Stützen, tropfende Rohre), Gothic (Bögen, runde Säulen), Gild (farbig, Regale, die meisten Kisten);
  - eine Reihe Vorräume und ein Gang-Labyrinth mit den belegten Raumarten: Büro mit Birne, Ziegelraum, Krankenzimmer mit Bett, Gummizelle mit Stuhl, Raum mit Bildern an Wänden und auf dem Boden;
  - einen Rohrgang nach Level 2, in dem Rohre und Messuhren zunehmen.
- **Nicht dargestellt:** Stützpunkte und Bewohner (M.E.G., B.N.T.G. u. a.) sowie drei Sektoren mit eigenen Regeln. Begründung in `LEVELSPEC_1.md` Abschnitt 3.
- **Neue Blender-Modelle:**
  - Ausstattung: Holzkisten (3), Haufen (2), farbige Stützenbänder, hohe Bögen, runde Säule, Schwingtüren.
  - Räume: Krankenbett, Polsterfeld, Glühbirne, Arbeitstisch.
  - Schilder und Bilder: grünes Notausgangsschild mit eigenem Piktogramm, Bilder (6 an der Wand, 3 angelehnt). Die Bilder sind eigene Farbflächen-Kompositionen.
- **Renderer:** Niedrige unsichtbare Kollisionsblöcke (unter 1,2 m, z. B. Kisten, Bett) zählen im Höhenfeld als Boden.
  - Vorher warfen sie einen harten Kastenschatten, der größer als das Modell war.
  - Hohe Blöcke (Regale, Säulen) werfen weiter Schatten.
- **Tests:** Die V11-Tests der Begegnungen (Smiler, Treffer, Sicherungspunkt) laufen jetzt in der Halle Aquila.
  - Statt der Autos und des Bodenlochs wird geprüft: Regal, Kisten und Säule haben Kollision.
  - Kisten auf Kisten gelten als gehalten.

## E-010 Level 2 (26.10.2026)
- **Quelle:** Wikidot „Level 2“, Revision 93 (`LEVELSPEC_2.md`), Bild in `LOOK_2.md`. Drei QA-Runden (`docs/screens/level2/`).
- **Neu statt V11:** Die V11-Gänge waren leere, rote Klinkergänge. Neu:
  - ein Tunnelnetz im 10-m-Raster (Wiki: Längen in Vielfachen von 5 m), an dem Wände entlang Maschinen und Rohrbündel stehen, meist an einer Seite;
  - verschlossene Türen in den Arten der Seite;
  - Röhren in Gruppen, von denen ganze Gruppen tot sind (Reihenschaltung);
  - violette Notlichter, ein Gleistunnel mit verrottender Schmalspur und entgleistem Wagen (B.N.T.G.);
  - Anlage, leeres Packlager, Bürogang als Schleife, Leerraum;
  - eine Stahltür mit „3“ nach Level 3.
- **45-Grad-Knicke:** In der Kachelwelt nicht als echte Diagonale möglich. An 28 Knicken schrägt ein Modell die Außenecke ab; Kollision passend (Ecke gesperrt). Siehe `LEVELSPEC_2.md` Abschnitt 2.
- **Clumps:** lauern in Deckenschächten an Kreuzungen (Wiki: Ecken und Kreuzungen), auch dort, wo das Netz in den Gleistunnel mündet.
- **Fehler gefunden und behoben:** Die Ziffer „3“ wirkte in Runde 1 gespiegelt. Tatsächlich war sie eine korrekte Segmentziffer; die Korrektur in Runde 2 spiegelte sie wirklich. In Runde 3 ist sie zurückgenommen und der Mittelstrich gekürzt, damit sie eindeutig als 3 lesbar ist.

## E-011 Level 3 und Bereinigung der Levelliste (26.10.2026)
- **Quelle:** Wikidot „Level 3“, Revision 18 (`LEVELSPEC_3.md`), Bild in `LOOK_3.md`. Drei QA-Runden (`docs/screens/level3/`).
- **Neu:**
  - Ziegelgänge in wechselnden Farben (braun, rot, schwarzgrau, weiß) mit Rohren voll schwarzer Flüssigkeit und Schlammlachen;
  - weiße, rostige Gitter vor versiegelten, hellen Räumen mit Tischen und Engelsbildern;
  - Generatorräume, eine helle Fertigungsstraße mit Förderbändern und Fenstern auf roten Ziegel, Kesselräume, ein Ziegel-Labyrinth;
  - ein Sanktuarium mit Säulen und Engel auf Zeichen-Sockel (Zuflucht, Sicherungspunkt);
  - ein dunkler Paranoia-Gang mit Warnschildern und Halluzinationsklängen;
  - ein Aufzug nach Level 5.
- **Texturen:** Bricks089 (als „dunkelgraue Ziegel“ gewählt) erwies sich in Runde 1 als Bruchsteinmauer. Ersetzt durch Bricks038 (alte weiße Ziegel) und Bricks056 (schwarzgraue Ziegel), beide CC0 von ambientCG, im Lock mit Prüfsumme.
- **Engine:** Klangereignisse können auf Raumtypen beschränkt werden (`"only"`). So erklingen die Halluzinationen nur im Paranoia-Gang.
  - Weinen, Schreie und Lachen (Wiki) gibt es im Spiel noch nicht. Belegt sind Flüstern, Atmen, Stimmen und Klappern.
- **Levelliste:** Die V11-Level 4, 6 und 19 gehören nicht zur Auswahl (`LEVELAUSWAHL.md`) und sind aus V12 entfernt, samt ihrer Notizen; in V11 bleiben sie erhalten.
  - Level 7 bleibt vorerst, weil der Schwimmtest es braucht, bis Level 37 Wasser hat.
  - Solange Level 5 und 37 nicht neu gebaut sind, schlägt die Reiseprüfung fehl (Anker zu entfernten Leveln). Das ist absichtlich offen und steht in `PROGRESS.md`.

## E-012 Level 5 (26.10.2026)
- **Quelle:** Wikidot „Level 5“, Revision 184 (`LEVELSPEC_5.md`), Bild in `LOOK_5.md`. Vier QA-Runden (`docs/screens/level5/`).
- **Neu statt V11:** Die V11-Fassung war ein rot getöntes Gangsystem. Neu:
  - Aufzugshalle mit Marmor, Kronleuchter und drei Aufzügen (zwei mit Kupfergitter, einer modern; Wiki);
  - Hauptgang und Flügel mit Damasttapete über Nussbaumvertäfelung, Stuckgesims, Kerzenwandleuchtern, Bildnissen, Möbeln;
  - Zimmertüren mit Nummern ohne Muster; 42 % der Zimmer verschlossen, die übrigen dunkel, eingerichtet oder leer;
  - Beverly Room (42 × 34 m) mit Türen ringsum, Mahjong-Tisch unter dem großen Kronleuchter, Grammophon, Silberschild an den Eingängen;
  - Kesselraum mit Flammrohrkesseln (meist) und Öfen (einige), Rohren, Dampf und Lecks;
  - Übergänge: Gang nach 5.3, Fenster nach 188, Metro-Halt nach 903.
- **Tapete:** Kein CC0-Set bei ambientCG passt (mahagonirot mit Gold). Eigene, prozedurale Damasttapete (`tools/textures/generate.py`).
  - Runde 1 zeigte, dass das erste Ornament wie ein Insekt las. Neu gezeichnet: Ogee-Gitter mit Palmette, Kelch, Voluten und Rosetten.
- **Sicherungspunkte:** Im Hotel hängt eine Laterne mit grünem Glas statt des modernen Notlichtschilds. Das Grün bleibt das Zeichen; die Form passt in die 1930er.
- **Engine-Fehler behoben (`room.cpp`):** Die Lichtsuche für die Kachelhelligkeit sah nur Leuchten in den benachbarten 8-m-Feldern.
  - Folge: Leuchten mit großem Radius (Ballsaal, 11–16 m) brachen hart ab. Decke und Boden zeigten dunkle Flecken.
  - Jetzt richtet sich die Suchweite nach dem größten Radius im Raum.
  - V4-Referenz (worldgen 29 658) unverändert grün, weil alte Leuchten höchstens 8 m reichen.
- **Wesen:** Deathmoths mit Nestern an den Türen dunkler Zimmer (Wiki: häufigste Wesen, zentraler Stock), selten Hounds in den Gängen.
- **Offen:** Bewohner und Gruppen (Homely Hotel, Originals, Housekeeping), Tippen auf die Schulter, sich selbst öffnende Türen – nicht umgesetzt (`LEVELSPEC_5.md` Abschnitt 3).

## E-013 Level 5.3 (26.10.2026)
- **Quelle:** Wikidot „Level 5.3“, Revision 40 (`LEVELSPEC_5.3.md`), Bild in `LOOK_5.3.md`. Vier QA-Runden (`docs/screens/level5_3/`).
- **Aufbau:**
  - Ein alter Hotelgang mit abgelöster Tapete führt durch eine sehr alte Tür in die große Halle: drei Geschosse Regale mit Galerien, Tuffsäulen, Kandelaber, eiserne Kronleuchter, Wappen an der Decke.
  - Von dort gehen Gänge zwischen Regalen in alle Richtungen ab: Westflügel älter und dunkler, Ostflügel neuer.
  - Eine Holztreppe führt ins Obergeschoss (+3 m) mit frei stehenden Regalen und weiteren Gängen.
  - Ein Treppengang führt zu alten Gängen (+6 m) mit Wappen an der Decke.
  - Ein weißes Eisengitter steht vor einer Leere.
  - Eine Sackgasse endet an der alten Holztür zurück nach Level 5 (Anker `l53_l5` im Gang zu 5.3).
- **Stockwerke:** Das Wiki nennt sieben. Die Engine hat je Kachel einen Boden; begehbar sind drei Höhen, die Halle zeigt drei Geschosse Regale. Technische Grenze, in `LEVELSPEC_5.3.md` Abschnitt 3 benannt.
- **Library Mind:** ohne Gestalt (Wiki: kein Körper, keine Sprache).
  - Buchfall-Geräusche im Raum (`book_fall`, neu in `synth_levels.cpp`).
  - Heruntergefallene Bücher am Boden.
  - Langsamer Verlust an Verstand in allen Bibliotheksräumen (Wiki: Erinnerungen werden nach ein bis zwei Stunden genommen).
- **Texturen:** Buchrücken und Tuffstein gibt es nicht als CC0-Set mit der nötigen Ausrichtung. Beide eigen, prozedural (`generate.py`).
  - Die Buchreihen sind 0,375 m hoch. Deshalb liegen Fachböden und Raumböden der Bibliothek auf Vielfachen davon.
- **Engine:** Treppenstufen tragen jetzt die Textur des Bodens (vorher glatte, helle Farbe). V4-Referenz unverändert grün.
- **Werkzeug:** `mapkit.validate` erkennt hohe Blöcke als tragende Wand für Wandteile (frei stehende Regale). Der C++-Test prüft das ohnehin an der echten Kachelwelt.
- **Fehler gefunden und behoben:**
  - Runde 1: Die Regalwände der Halle lagen im Dunkeln. Neu: Kerzen an den Handläufen und Kandelaber vor den Regalen.
  - Grüne Sicherungslaternen färbten dunkle Gänge grün; ihr Licht ist jetzt schwach und fast weiß.
  - Regale auf den Treppenstufen und frei stehende Regale in der Freihaltezone vor Türen fielen in der Platzierungsprüfung auf und wurden verlegt.

## E-014 Level 188 (26.10.2026)
- **Quelle:** Wikidot „Level 188“, Revision 69 (`LEVELSPEC_188.md`), Bild in `LOOK_188.md`. Drei QA-Runden (`docs/screens/level188/`).
- **Aufbau:**
  - Ein 30 m hoher Hof aus weißem Beton mit Vorsprüngen und 144 Fenstern in sechs Reihen.
  - Moos, ein fünfeckiger Plattenweg mit vier Zuwegen zum Birkenholzkreis.
  - Ein Tisch mit Gegenständen und ein Grammophon (Musik als Warnung).
  - Glasgalerien und Stützen, Oberlicht.
  - Hinter einer Mahagonitür ein greller, weißer Gang mit leeren Zellen und einem Treppenhaus.
- **Fensterwesen:** Es gibt kein Modell. Eigene, einfache Lösung (wie in E-008 angekündigt):
  - Dunkle Umrisse in einigen schwach erleuchteten Fenstern.
  - Die Gestalt (Level 0) erscheint im Hof, verharrt, solange man hinsieht, und verschwindet beim Wegsehen.
  - Nicht tödlich, obwohl das Wiki sie feindlich nennt. Steht so im Bericht.
- **Rückweg:** Das Eingangsfenster bleibt bestehen (Wiki: es verschwindet nach zwei bis drei Minuten). Sonst gäbe es keinen Rückweg, weil Level 1.5 nicht zur Auswahl gehört.
- **Leistung:** 22 Oberlichtfelder, aber nur jedes zweite trägt ein Licht. Die Beleuchtung sank von 10,8 auf 5,3 ms GPU (1600 × 900, hohe Qualität).
- **Fehler gefunden und behoben:**
  - Runde 1: Die Scheibe vor den Fenstern war undurchsichtig und verdeckte Jalousien, Umrisse und das warme Eingangslicht. Die Scheibe ist entfernt; der Hintergrund selbst glänzt.
  - Sichtbare Blöcke zählen in mapkit als begehbares Podest; Fenster landeten dadurch in den Vorsprüngen. Eigene Blockprüfung in `level188.py`.

## E-015 Level 903 (26.10.2026)
- **Quelle:** Wikidot „Level 903“, Revision 60 (`LEVELSPEC_903.md`), Bild in `LOOK_903.md`. Drei QA-Runden (`docs/screens/level903/`).
- **Aufbau:** Eine Halle, 196 m lang.
  - Der Mittelsteig ist ein begehbarer Block 1,1 m über dem Gleisbett; links das offene Gleis.
  - Rechts der ruhende Zug als Block mit Wagenwänden als Wandteile.
  - Der fahrende Wagen steht am linken Gleis: Boden und Wände als Blöcke, Türen als Lücken in der Wand.
- **Ziele im Wagen:**
  - Das vordere Wagenende führt nach Level 37, das hintere nach Level 36. Bildzeichen (Wellen, Flugzeug) statt Schrift.
  - Level 36 nennt die Metro als Eingang; Level 37 als Ziel ist eigene Ergänzung (`LEVELAUSWAHL.md`).
- **Ohne Wartezeit:** Der Wagen steht immer da (Wiki: alle paar Stunden, zehn Minuten).
- **Zugemauerte Treppen:** Ursprung des Modells am Fuß der Treppe, dahinter sperrt ein Block. So bleibt die Bodenprüfung gültig.
- **Fehler gefunden und behoben:**
  - Runde 1: Bildzeichen zu klein, Fenster des bewohnten Wagens flächig orange, Bodentextur wie Bruchstein.
  - Runde 2: Vorhänge mit Falten und einem leuchtenden Spalt; Boden aus schwarzgrauen Ziegeln.

## E-016 Level 37 und Abschied von Level 7 (26.10.2026)
- **Quelle:** Wikidot „Level 37“, Revision 52 (`LEVELSPEC_37.md`), Bild in `LOOK_37.md`. Drei QA-Runden (`docs/screens/level37/`).
- **Neu statt V11:**
  - Level 37 ist nicht mehr das Ende (das liegt jetzt am Tor von Level 36).
  - Ankunft und Rückweg mit dem Metro-Wagen, der halb in einer Fliesenwand steht.
  - 338 Räume: Säulenhallen, Gruben mit Treppen ins tiefe Wasser (meist dunkel), Gänge, dunkle Bereiche, Licht aus seltsamen Winkeln.
- **Engine:**
  - Wasserflächen haben jetzt Varianten (`water_look`: 0 Meer, 1 flaches, 2 tiefes Becken). Flaches Wasser ist helles Türkis, weil die Engine keine durchsichtigen Flächen kennt.
  - In Durchgängen zwischen gefluteten Räumen liegt ebenfalls Wasser; vorher blieb in jeder Öffnung ein trockener Streifen.
  - V4-Referenz unverändert grün.
- **Level 7 entfernt:** Es gehört nicht zur Auswahl. Der Schwimm- und Ertrink-Test läuft jetzt in einer Grube von Level 37. Die Notiz von Level 7 ist entfernt; in V11 bleibt alles erhalten.
- **Test:** Die Reiseprüfung erwartet das Ende jetzt in Level 36.

## E-017 Level 36 – das Ende (26.10.2026)
- **Quelle:** Wikidot „Level 36“, Revision 33 (`LEVELSPEC_36.md`), Bild in `LOOK_36.md`. Drei QA-Runden und eine Schlussaufnahme (`docs/screens/level36/`).
- **Aufbau:**
  - Gepäckausgabe (Ankunft aus der Metro am Band), Check-in mit Schaltern, Abflugtafeln und BTA-Stand.
  - Rundgang mit Laufbändern und widersprüchlichen Wegweisern, eine Sackgasse hinter einem EXIT-Schild.
  - Ladenzeile, Gatehalle C mit fünf Gates.
- **Ende:**
  - Nur Gate C33 steigt ein; seine Tür ist der einzige Übergang der Art „end“.
  - Wiki: Jede Gate-Tür führt in 36.1. Die anderen Tore sind hier geschlossen, weil ihr Countdown nicht läuft (Wiki: Einstieg ab 1000, Schluss bei 0).
  - Die Reiseprüfung verlangt genau ein Ende, und zwar in Level 36.
- **Wartende an C32:** Es gibt keine Menschen. Ihre Habseligkeiten und ihr Stimmengewirr zeigen sie.
- **Klang:** Neue Durchsage `pa_announce`: Gong und eine eintönige, unverständliche Stimme, bandbegrenzt mit Hall.
- **Schrift in Blender:**
  - Für Schilder flach und mit geringer Kurvenauflösung. Die Abflugtafel sank von 89 480 auf 8 168 Dreiecke.
  - Zu grobe Kurven (Stufe 2) zerrissen Buchstaben; Stufe 4–5 ist lesbar.
- **Fehler gefunden und behoben:**
  - Abflugtafel mit vertauschten Spalten: Im Blick entlang −x liegt +y rechts.
  - BTA-Stand zur Wand gedreht; seine Schrift stand nur auf der Rückseite.
  - Uhr und Trinkbrunnen hingen in Öffnungen.

## E-018 Oberfläche V12 (26.10.2026)
- **Ziel (Auftrag):** Oberfläche komplett neu, geprüft bei mehreren Auflösungen, Debug getrennt.
- **Gestaltung „Beschilderung“:** Die Oberfläche spricht die Sprache der Orte, durch die man reist: Wegweiser, Stationstafeln, Gate-Schilder.
  - Tiefes Blauschwarz, Signalgelb als einziger Akzent, gerade Kanten, links eine Signalkante an jeder Tafel.
  - Kapitälchen mit Sperrsatz für Beschriftungen.
- **Neu:**
  - Titel- und Pausenmenü als Seitenleiste über dem laufenden Raum, mit Wortmarke und Liste.
  - Ein **Streckenplan** der Reise (0 → 1 → 2 → 3 → 5 → 903 → 36, Abzweige 5.3, 188, 37); im Pausenmenü ist die aktuelle Ebene markiert.
  - Quellenangabe des Wikis im Hauptmenü.
  - **Ankunftstafel** jeder Ebene: Nummer, Titel, Wiki-Seite mit Lizenz.
  - **Abspann am Gate** mit den zehn Ebenen und ihren Autoren im Wiki.
  - Deutsche Begriffe „Leben“ und „Verstand“ statt Health/Sanity in allen sichtbaren Texten.
  - Hilfe auf die Reise von V12 umgeschrieben (Ebenen, Übergänge, Metro-Ziele, Ende, Wesen je Ebene).
- **Geprüft** (`docs/screens/ui/`): 1280 × 720, 1920 × 1080, 2560 × 1440, 1024 × 768 (4:3), 3440 × 1440 (21:9).
  - Alle Maße skalieren mit der Höhe (Bezug 1080 p). Die Seitenleiste ist 34 % der Breite, mindestens 520 px bei 1080 p.
  - Bei 1024 × 768 rutschen Streckenplan und Laufbuch unter die Beschreibung (fließende Abstände).
- **Debug:** bleibt getrennt (F3/I), technische Begriffe dort unverändert.
- **Grenzen:**
  - Die Schrift kommt weiter aus den Windows-Schriften (Segoe UI, Consolas), zur Laufzeit gerastert, nicht mitgeliefert.
  - Inventar, Karte, Notizblatt und Todesbildschirm folgen dem neuen Thema (Farben, Kanten), behalten aber ihren Aufbau aus V11.

---

# V13

## E-019 Aufbau von V13 (26.10.2026)
- **Grundlage:** V12 (`e8481b6`, Ergebnis der letzten Nacht) wird nach `LIMINAL V13` kopiert, ohne Build-Ordner; neuer Branch `liminal-v13`.
- **V12 und V10 bleiben unverändert:**
  - V12 ist mit einem Einfrier-Manifest (Pfad, Größe, SHA-256 aller 25 550 Dateien) in `Sicherungen/` festgehalten und wird am Ende dagegen geprüft.
  - V10 wird über `git status` geprüft.
- **Version 13:** eigene Dateien für Einstellungen (`settings_v13.json`), Spielstände (`saves_v13`), Protokoll und Absturzberichte. V12-Einstellungen werden beim ersten Start nur gelesen, nie geschrieben.
- **Bauweg:** Der Aufruf von `tools/build.ps1` über `powershell -ExecutionPolicy Bypass` wurde in dieser Umgebung abgelehnt (Sicherheitsregel). Gebaut wird deshalb direkt mit CMake und Ninja aus `.toolchain/`; das Ergebnis ist dasselbe. Befehle in `PROGRESS.md` und im Bericht.
- **Arbeitsweise:** nach jedem Teilschritt ein Commit; Fortschritt in `PROGRESS.md`; V12-Fortschritt und -Bericht liegen in `docs/archiv/`.

## E-020 Hand und Taschenlampe neu (26.10.2026)
- **Befund:** siehe `HAND_ANALYSE.md`. Die Hand aus V12 war ein starres Netz aus vereinigten Ellipsoiden, ohne Skelett; das Verfahren selbst war die Ursache.
- **Methode:**
  - Box-Modelling als Viereck-Käfig mit Subdivision (Blender, `tools/blender/models/models_hand.py`).
  - Skelett mit 21 Knochen und einer Markierung (`elbow`); Gewichte beim Bau.
  - Griff wird gelöst (`hand_grip.py`), dann als Ruhepose übernommen; vorher Corrective Smooth, Trennen sich berührender Finger und Anlegen der Haut an den Schaft.
- **Posen** als glTF-Animationen: `grip` = Ruhe, `press` (Daumen auf dem Schalter).
- **Engine:**
  - Der glb-Leser kann jetzt Skins und Posen. Die Hand nutzt den Skinning-Pfad der Figuren.
  - Geometrie (Handgelenk, Unterarm, Handfläche) kommt zur Laufzeit aus dem Skelett, nicht aus kopierten Konstanten.
- **Haltung:**
  - Die Lampe liegt 52° zur Handachse schräg in der Hohlhand; das Handgelenk knickt 33° zur Elle ab und ist 14° gestreckt.
  - Die Handfläche zeigt zur Körpermitte, der Daumen liegt oben auf dem Schaft, der Schalter unter seiner Kuppe.
- **Abwägung:**
  - Ein Unterarm, der genau von links unten kommt, wäre nur mit nach oben gedrehter Handfläche möglich (unnatürlich).
  - Gewählt ist die natürliche Handhaltung; die Arm-IK gleicht über Gleiten entlang der Lampe aus (bis 4,5 cm vor).
  - Beim Blick steil nach unten weicht der Unterarm bis 11,5° vom Modell ab (Test: < 12°).
- **Schaltfolge:**
  - Die Lampe bleibt immer in der Hand; gesenkt folgt ihr Rahmen dem Unterarm.
  - Licht erst nach dem Daumendruck (Klick zugleich); aus: Druck, dann senken.
  - Folge: Das Licht kommt etwa 0,25 s nach dem Tastendruck statt sofort. Bewusst so (Lampe und Lichtkegel passen immer zusammen).
- **Spielerfigur:** In der Ego- und Schulteransicht wird ein Netz ohne die linke Hand gezeichnet (`_nh1`). Das Ärmelende ist gerundet.
- **Haut:** eigenes Material mit Durchscheinen (Licht-Shader, Glanzkanal des G-Buffers); Rötung an Knöcheln und Kuppen je Punkt.

## E-021 Kreaturen neu gebaut statt nachgestellt (26.10.2026)
- **Befund:** siehe `KREATUREN_ANALYSE.md`. Die Wesen waren Vereinigungen grober Grundkörper aus `render/entity_models.cpp`; kleine Parameteränderungen hätten am Verfahren nichts geändert (der Auftrag schließt sie ausdrücklich aus).
- **Methode:** Formen in Blender als Distanzfeld aus Dutzenden anatomischer Teile (`tools/blender/creatures/`), Hautdetail als Verschiebung, Vernetzung mit OpenVDB, drei Stufen durch Reduzieren, Export als glb mit Skin.
- **Skelett:** dieselben Knochen wie die prozedurale Bewegung des Spiels (`--dump-skeletons`). So bleiben Gang, Atmung und Zustandswechsel erhalten und treiben die neuen Körper an; die Bewegung wird je Wesen nachgeschärft.
- **Nachweis:** Das Spiel schreibt je Wesen ins Protokoll, woher das Netz stammt (`Quelle Blender: ent_<name>_l0.glb (FNV …)` oder `Quelle prozedural`). Fehlt eine Stufe, fällt es auf den alten Bau zurück und meldet das.
- **Abwägung gegen Sculpting von Hand:** Ohne Grafiktablett und ohne Sicht in Blender ist reproduzierbarer Code der verlässlichere Weg. Jede Form ist im Quelltext begründet und kann neu erzeugt werden.
- **Smiler ohne Arme:** Die angedeuteten Arme der ersten Fassung lasen sich im Licht als Strichmännchen-Arme. Die Seite nennt sie nur als unbestätigtes Gerücht; der Körper bleibt deshalb eine formlose Masse.
- **Neues Formteil `tube`:** Strecke mit ellipsenförmigem Querschnitt. Ketten aus einzelnen Ellipsoiden ergaben an Lippen und Maulspalt eine Perlenschnur.

## E-022 Texturen: Kachelbruch statt neuer Bildquellen (26.10.2026)
- **Befund:** `TEXTUR_AUDIT.md`. Die 40 PBR-Sätze (37 × ambientCG CC0, 3 eigene) treffen die Identität jeder Ebene; das sichtbarste Problem sind Wiederholungen großer Flecken und Streifen auf Beton, Teppich und Putz in großen Räumen.
- **Entscheidung:** Kein Austausch der Sätze (kein Gewinn an Identität, mehr Lizenzpflege), sondern Kachelbruch im Shader nach I. Quilez (Variante 3: acht Versätze, weich überblendet), nur für Texturen ohne Raster (`"antitile": 1` in `data/textures/textures.json`). Ziegel, Fliesen, Deckenplatten, Pflaster, Dielen, Tapeten und Buchrücken bleiben unberührt.
- **Abwägung:** Variante 3 braucht einen zusätzlichen Abruf je Kanal und keine neue Textur; die Sechseck-Variante (Heitz/Neyret) mischt besser, kostet aber drei Abrufe je Kanal und eine Vorverarbeitung jeder Textur. Leistung und Speicher: `TEXTUR_AUDIT.md` Abschnitt 4.
- **Messung neu:** Der Befehl `perf` meldet den belegten Grafikspeicher (DXGI 1.4) und den Arbeitsspeicher des Prozesses.

## E-023 Starter: nur von Microsoft signierte Pakete ausführen (26.10.2026)
- **Befund:** Der Starter (`LIMINAL.exe`) lädt auf alten Windows-Versionen ohne Universal C Runtime – nach Rückfrage – das Visual-C++-Paket von aka.ms und führt es mit Administratorrechten aus. Geprüft wurde nur, ob die Datei *irgendeine* gültige Signatur trägt.
- **Entscheidung:** Der Unterzeichner muss „Microsoft Corporation“ sein; die Zertifikatskette wird auf Widerruf geprüft. Schlägt das fehl, wird die Datei gelöscht und der Spieler auf den manuellen Download verwiesen.
- **Nachweis:** `MULTIPLAYER_TESTS.md` Abschnitt 4 (Microsoft-Datei angenommen, gültig signierte Fremddatei abgelehnt, unsignierte abgelehnt). `BUGLISTE.md` B-003.

## E-024 Mehrspieler bleibt aus (26.10.2026)
- **Entscheidung:** Kein Mehrspielermodus in V13, nicht erreichbar; kein Netzwerkcode im Spiel (`tools/check_no_network.py`).
- **Begründung:** `MULTIPLAYER_ENTSCHEIDUNG.md` – die Spielsysteme sind für einen Spieler gebaut; „nachweislich sicher“ ist ohne vollständige Umsetzung, verschlüsselten Netzweg, Prozesstests und Durchsicht nicht belegbar; bei Unklarheit gilt höhere Sicherheit und Stabilität.
- **Geliefert:** Bedrohungsmodell mit Architekturregeln, Prüfschicht `src/net` (vom Spiel nicht gebunden) mit 72 Prüfungen, 1 000 000 Fuzz-Runden, Sanitizer-Lauf (`MULTIPLAYER_TESTS.md`).

## E-025 GitHub-Sicherung: nicht ins öffentliche Remote, lokal vollständig vorbereitet (26.10.2026)
- **Befund:** Das vorhandene Remote `origin` (`AmerAsani/LIMINAL`) ist **öffentlich** und enthält V1–V10. Der Auftrag verlangt für V11–V13 ein **privates** Repository, solange die Lizenzfrage offen ist. Die GitHub CLI ist nicht installiert; ein Repository anzulegen ginge nur mit einem Zugangstoken, und Zugangsdaten werden weder ausgelesen noch eingegeben. Der Name `liminal` ist zudem belegt (GitHub unterscheidet nicht nach Groß- und Kleinschreibung).
- **Entscheidung:** Kein Push nach `origin` (würde veröffentlichen), keine Änderung der Sichtbarkeit von `origin` (Kontoeinstellung, würde V1–V10 verbergen). Stattdessen alles lokal vorbereitet: V11 erstmals committet (Branch `liminal-v11`), README und `.gitignore` je Branch, Tags `v11`, `v12` (und `v13` am Ende), Geheimnis- und Größenprüfung. Die exakten Befehle für ein privates Repository `liminal-privat` stehen in `GITHUB_UPLOAD.md`.
- **Historie:** nur angehängt, nichts umgeschrieben. V10 bleibt Teil der gemeinsamen Historie (seit Langem öffentlich); er wird nicht neu hochgeladen.
- **LFS:** nur für ZIP-Dateien; die Assets aus der Historie in LFS zu verschieben, hieße umschreiben (ausgeschlossen), bei höchstens 6,8 MB je Datei auch unnötig.


## E-026 Mottennest: im Wiki belegt – Neuentwurf statt Entfernen (26.10.2026)
- **Prüfung:** Die Seite der Motte (Entity 4, Revision 6 vom 31.01.2026) beschreibt Nester ausführlich: in vorhandener Architektur, in Räumen mit wenigen Zugängen; Schichten aus verdautem Material, Pflanzenfasern und konservierendem Harz, 60–100 cm dick, feucht und halb organisch; Säulen aus unverdaulichen Resten (Knochen, Hartholz, Metall), mit demselben Material überzogen; Eier an der Auskleidung, in Säulen gestapelt; Larven fressen die obere Schicht und verpuppen sich in den Wänden; Skelettreste rund um den Stock; Warnzeichen „fleischige Wucherungen über Türen“ und Gruppen von Weibchen. Die Seite von Level 5 (Revision 184) nennt Nester „überall auf der Ebene“ und die Ebene einen der zentralen Stöcke, ohne Aussehen.
- **Entscheidung:** Das Nest bleibt und wird neu entworfen (Teil 1.6 des Auftrags), streng nach Entity 4: Wucherung über und in der Tür, Schichten, überzogene Säulen aus Resten, Eier in Säulen, Larven in der Wand, Knochen davor, Weibchen am Nest. Entfernen wäre nur bei fehlendem Beleg vorgesehen.

## E-027 Fehlende Wiki-Wesen: Reihenfolge und Umfang (26.10.2026)
- **Grundlage:** `ENTITY_ABGLEICH.md` (alle zehn Ebenenseiten, Entity-Übersicht und Entity-Seiten am 26.10.2026 auf Wikidot gelesen).
- **Reihenfolge:** Faceling (vier Ebenen, je Ebene anderes Verhalten), Fensterwesen (Level 188 als prägendes Merkmal, Level 2), Skin-Stealer (vier Ebenen), Wretch (drei), Death Rat (drei). Danach Duller, Nguithr'xurh, Growler, Arms, Wrangler und weitere.
- **Umfang je Wesen:** Briefing, eigenes Blender-Modell (keine fremde Grundanatomie), Rig, eigene Bewegung je Zustand, Verhalten nach Wiki, Klang, Gleichgewicht, Kritikrunden mit Bildern, Server-KI im Mehrspieler.
- **Bewusst weggelassen**, jeweils mit Grund in der Tabelle: Menschen und Gruppen; Krankheitserreger ohne Gestalt; unbestätigte oder ausgestorbene Wesen; Wesen, die nur in Erzählungen vorkommen; Wesen, deren Ebenenseite nur den Namen nennt.
- **Smiler:** Die Entity-3-Seite nennt neben Panik auch lauten Lärm als Auslöser. Ergänzt: Eine harte Landung oder ein Aufprall in seiner Nähe löst die Ankündigung aus. Stillstehen und Hinsehen beruhigt ihn wieder; die Ankündigung bleibt dafür mindestens 0,6 s sichtbar. Test in `gameplay_test`.

## E-028 B-001: Ruhepose, Dual Quaternions, eigene Lampenhaltung von außen (26.10.2026)
- **Befund:** Der Riss am Ellbogen hatte drei Ursachen: Ärmel und Rumpf verschmolzen in der Ruhepose (Arm 24 Grad, Brust mit 7,5 cm Rundung vernetzt); lineares Mischen der Knochen ließ Punkte an Ellbogen und Schulter umklappen; von außen hielt der Körper die Lampe in der Haltung für das Ego-Bild (Ellbogen über dem Kopf).
- **Entscheidung:** Ruhepose der Spielerfigur mit 54 Grad abgespreizten Armen; Dual-Quaternion-Skinning für Menschen im Shader (Bit 31 des Knochenversatzes; Wesen bleiben beim linearen Mischen, weil ihre Rahmen strecken können); eigene Lampenhaltung, sobald der Körper von außen gesehen wird (Schulteransicht; Mitspieler im Mehrspieler). Die Umrechnung Körper → Skelett ist jetzt eine eigene Funktion (`playerJointPose`), die Tests und Mitspieler nutzen.
- **Verworfen:** eine Gewichtsregel für die Ärmel (verschob die Faltung an die Schulternaht) und Korrektur-Blendshapes (das Netz entsteht prozedural; ein Blendshape müsste bei jeder Formänderung neu erzeugt werden – Dual Quaternions lösen das Umklappen ohne zusätzliche Daten).
- **Nachweis:** `BUGLISTE.md` Abschnitt „B-001: Nachweis“, Test in `gameplay_test`.

## E-029 Mottennest: Ergebnis des Neuentwurfs (26.10.2026)
- **Ergebnis:** Nach fünf Kritikrunden im Spiel (feste Kameras, mit und ohne Lampe) bleibt das Nest. Alle Teile stammen aus der Entity-4-Seite: Wucherung über der Tür, Schichten, Säulen aus Resten, Eier, Knochen.
- **Verworfen:**
  - eine gespannte Haut über der Öffnung (las sich als Leinwand, mit Fetzen als Zahnreihe);
  - herabhängende Einzellappen (Reißzähne);
  - kleine runde Löcher (Gesicht).
- **Gewählt:** ein zerrissener Schleier aus Fasern und Harz mit durchhängenden Fäden, eine geschichtete Kante im Zimmer, eine asymmetrische Wucherung im Flur und Eier halb in der Kruste.
- **Material:** `ent_nestflesh` nutzt die Nesttextur (Fasern, Adern, Harz), rotbraun getönt und matt. Harzflächen sind seltener und dunkler; Eier bekommen ein eigenes mattes Cremegelb (`ent_nestegg`).
- **Bewegung:** Das Nest ist jetzt ein Wesen mit neun Knochen; Licht und Alarm machen es unruhig (ANIMATIONEN.md).
