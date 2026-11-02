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

## E-030 Koop (Beta): Umsetzung und Freigabe (26.10.2026)
- **Architektur:** Listen-Server, hostautoritativ (`MULTIPLAYER_ARCHITEKTUR.md`). Transport ENet 1.3.18 in `liminal_net.dll`, Handshake mit Beweis des Schlüssels, AES-256-GCM über CNG. Das Spiel lädt die DLL erst beim Hosten oder Beitreten.
- **Gast:** sendet nur Eingaben und Absichten; seine Figur sagt er voraus und gleicht sie mit dem Host ab (weich unter 2 m, sonst Sprung). Mitspieler und Wesen sieht er 100 ms zurück interpoliert.
- **Wesen:** Jedes richtet sich nach dem nächsten lebenden Spieler (Sicht, Lärm, Weg). Die Regie wechselt alle 30 s den Bezugsspieler. Was im Blick irgendeines Spielers erschiene, wird verworfen. Beim Gast sind die Wesen Puppen mit derselben Animation.
- **Gruppe:**
  - Ebenenwechsel, wenn alle Lebenden im Ausgangsbereich stehen, sonst nach 60 s.
  - Tod: zusehen, nach 20 s am Sicherungspunkt zurück; alle tot: Neubeginn.
  - Vorräte ab der nächsten Ebene × (1 + 0,6 (n − 1)), deterministisch im Generator.
  - Der Host speichert einen eigenen Koop-Stand mit den Gästen nach Namen.
- **Verworfen:**
  - ein eigener dedizierter Server (zu groß für 2–4 Freunde);
  - Zustände vom Gast (Bedrohungsmodell T1);
  - Sprachübertragung;
  - Vermittlung über fremde Server (Datenschutz).
- **Freigabe:** Beta, weil die Tests bestehen (Auftrag). Offen: Fuzzing über Stunden und eine Durchsicht durch Dritte (`MULTIPLAYER_ENTSCHEIDUNG.md`).

## E-031 Neue Wesen: Auslegung der Wikidot-Seiten (26.10.2026)
- **Grundlage:** Entity 2 (Rev. 36), 9 (Rev. 39), 10 (Rev. 61), 15 (Rev. 54, „wird überarbeitet“), 24 (Rev. 25, „wird überarbeitet“) und die Ebenenseiten (`ENTITY_ABGLEICH.md`), am 26.10.2026 neu gelesen. Was die Seiten nicht sagen, ist hier und in den Briefings `KREATUR_<Name>.md` als **eigene Auslegung** markiert.
- **Skin-Stealer – zwei Gestalten:** Die Seite sagt, ein Skin-Stealer in Menschenhaut sei nicht von einem Menschen zu unterscheiden (die Noppen ziehen die Haut passend, heilen Schnitte und Nähte). Ein erster Entwurf mit sichtbaren Hautlappen und verrutschtem Gesicht widersprach dem und wurde verworfen. Jetzt: **wahre Gestalt** (blassgelb, Noppen, eingesunkene weiße Augen; Level 5 und 188) und **getarnt** (ein Mensch in abgetragener Kleidung; Level 1 und 3, wo die Ebenenseiten Getarnte nennen). Was ihn verrät, ist das Verhalten (spricht kaum, wiederholt Worte), nicht das Aussehen.
- **Skin-Stealer – Größe 2,05 m:** Die Seite sagt nur „groß“. 2,3 m ragten über die Türstürze der Ebenen (2,2 m, B-014).
- **Skin-Stealer – Hunger:** satt harmlos, gereizt (Lampe lange im Gesicht, zu nah, anrennen) tödlich; hungrig (Anteil je Ebene, `chance`) folgt er dem, der allein ist, spricht nach, hält 1,4 s inne und greift an. **Ausweg (Wiki: langsam und ruhig weggehen):** wer in diesem Innehalten ruhig zurückweicht, den lässt er ziehen; wer rennt, reizt ihn. Im Mehrspieler sucht er Einzelne (kein Mitspieler näher als 6 m).
- **Faceling – vier Verhaltensweisen nach den Ebenenseiten:** Level 903 friedlich (winken – Entity 9: sie winken, wen sie kennen), Level 2 in Gruppen und reizbar (Entity 9: kleine Störungen, Bedrängen, Angriffe reizen; dann Ausbrüche, schwächer als Menschen – leichter Schaden, man entkommt leicht; die Gruppe reagiert mit), Level 3 feindselig mit Steinwerkzeugen (Ebenenseite), kurzsichtig und schwerhörig (Entity 9: kleine, kurzsichtige Augen, Ohrenschmalz) – sie bemerken einen erst auf 5 m, Level 188 im rosa Kleid tödlich in den Gängen (Ebenenseite). **Eigene Auslegung:** die Lampe lange im Gesicht zählt als Störung; das rosa Kleid lässt ab, sobald man die Gänge verlässt.
- **Facelings sind verschieden** (Entity 9: Haar, Haut, Kleidung, Körperbau, Alter): drei gewöhnliche Varianten (Mann grau, Frau mit langem Haar und dunklerer Haut, älterer schwerer Mann), dazu „entwickelt“ und „rosa Kleid“.
- **Wretch:** Die Seite nennt keine Auslöser (Licht, Lärm). **Eigene Auslegung:** sie sehen und hören wie die anderen Jäger des Spiels (Sicht 18 m im Licht, 7 m im Dunkeln; Rennen 16–24 m). Zwei Arten nach der Seite (der Laut verrät die Gefahr): träge (tief, langsam) und schnell (hoch; „schneller als ein Mensch“ – in Stößen von 4–6 s, dazwischen 2–3 s nach Luft ringend, damit man entkommen kann). Schläge mit übermenschlicher Kraft: 26–34 Health je Schlag (nicht sofort tödlich – im Unterschied zu Smiler, Hound, Bündel). Zweites Stadium: fleht, greift nie an. Heilung mit Mandelwasser (Wiki) ist **nicht umgesetzt**. Waffen der Level-3-Wretches (Ebenenseite) **nicht umgesetzt** (kein Modell).
- **Death Rat:** Hausratte mit langem Körper (Modell 32 cm Rumpf und Kopf), schwarzes Fell, braune Hörner mit rot glühenden Spitzen; haarlos in Level 2 (Entity 24). Meist harmlos; „Reaktion unbekannt“ (Wiki) → **eigene Auslegung:** sie stieben davon, bedrängt beißt eine (6 Health). Das „Screaming Event“ (Ratten fallen über andere Wesen her, das Licht flackert, bis es vorbei ist) als Ereignis außer Sicht in Level 5. Die Fallen und die menschliche Klugheit der Level-3-Ratten (Ebenenseite) sind **nicht umgesetzt** (eigenes Fallensystem nötig).
- **Fensterwesen:** in drei dunklen Fenstern (ohne Jalousie) im Erdgeschoss des Hofs von Level 188; einmal je Ebene gesetzt, bleiben dort (Wiki: bleiben, bis sie vergehen). Lockt mit Flüstern (Wiki: tröstend) und Handzeichen; unter 1,9 m vor der Öffnung fährt ein Arm heraus (Wiki: 1–2 m), nach 0,7 s zieht er hinein (tödlich) – wer sofort zurücktritt, entkommt. **Level 2** (Ebenenseite: Fensterwesen am Gangende) ist **nicht umgesetzt**: die Rohrgänge haben keine Fenster; ein eigenes Fenster an Gangenden wäre ein neues Ausstattungsstück (Rang 2 bleibt dafür offen).
- **Regie:** je Wesen eine Gruppe zur Zeit, außer Sicht, 14–30 m Weg (weiter, wenn die erlaubten Räume weiter liegen), erster Auftritt nach 25–150 s; Abstand je Ebene (`every`) mal Schwierigkeit (`stalker_pause`). Schaden der Schläge mal `stalker_hit / 35`.

## E-032 Arm der Lampenhand: Kette, Handgelenk, gedrehte Faust, Ärmel (V13.3, 27.10.2026)
- **Befund:** siehe `HAND_ANALYSE.md`, Abschnitt „V13.3: Arm“. Der Ärmel war die Körperform plus 2,2 cm rundum (Unterarm 11–12 cm dick); mit dem Daumen genau oben verlangte das Handmodell einen Ellbogen 16 cm über der Schulter.
- **Entscheidung:**
  - Unterarm 27 cm (`figures.cpp`), Oberarm bleibt 29,9 cm.
  - Handgelenk im Handmodell 30° statt 14° gestreckt (übliche Stellung im Kraftgriff). Finger, Nägel und Lampe bleiben punktgleich.
  - Im Ego-Bild ist die Faust 43° um die Lampenachse gedreht (Daumen nach außen, leicht von unten gehalten), Griff 18 cm unter und 13 cm links vom Auge. Der Ellbogen hängt dann auf statt über Schulterhöhe, der Unterarm steigt zur Hand. Die Haltung von außen (E-028) bleibt.
  - Ärmel als eigener Schlauch entlang der Armkette mit dem Querschnitt einer Jacke, Raffung, Bündchen mit gerundeter Kante und dunkler Öffnung, Falten in der Armbeuge und Stau über dem Bündchen, Ober-/Unterarm-Gewichte an der Ebene des Ellbogens (±4,5 cm). Stufe 0 der Jacke feiner vernetzt (6,5 mm, 40 % der Dreiecke).
- **Abwägung:**
  - Mit Daumen oben gibt es keine Haltung, in der der Ellbogen tiefer liegt und die Faust im Bild bleibt (gerechnet: Ellbogen 20 cm unter der Schulter → Faust rund 50° unter dem Blick, das Bild reicht 31°).
  - Mehr Ellenabknickung (über 33°) wäre am Rand der Beweglichkeit.
  - Die Lampe nicht genau in Blickrichtung zu halten, hätte Licht und Wesen-Reaktionen verändert.
  - Die Drehung um die Lampenachse ändert nur die Lage der ganzen Faust, nicht den Griff. Rollen um 90° und mehr (Handfläche nach oben) wurde verworfen.
- **Verworfen:**
  - eine 2 cm tiefe Öffnung im Bündchen (Wand 3–4 mm, im Netz zerfranst);
  - Beugefalten mit 3,4 mm Tiefe (zu viele umgeklappte Dreiecke bei 160° Beugung, Test B-001);
  - eine Mischzone am Ellbogen von ±3 cm (48 umgeklappte Dreiecke von außen, Grenze 46).

## E-033 Koop: Verbindung ohne Router-Einstellungen über WebRTC-Datenkanäle (27.10.2026)
- **Auftrag:** Jeder Spieler soll ohne Portfreigabe beitreten können. Keine eigene Kryptografie, kein eigenes Transportprotokoll, wenn es Bewährtes gibt. Recherche: `MULTIPLAYER_RECHERCHE.md`.
- **Gewählt:**
  - libdatachannel 0.24.6 (WebRTC-Datenkanäle) mit libjuice 1.7.4 (ICE, STUN, TURN), Mbed TLS 3.6.7 LTS (DTLS 1.2) und usrsctp (SCTP);
  - dazu ein kleiner eigener Lobby-Server (Beitrittscodes, Signalisierung) mit dem STUN/TURN-Server aus libjuice.
  - Alles Netz bleibt in `liminal_net.dll` und im Lobby-Programm.
- **Warum:**
  - NAT-Durchdringung mit automatischem Relay-Rückfall.
  - Verschlüsselung (DTLS mit Vorwärtsgeheimnis) und Echtheit der Gegenstelle (Zertifikats-Fingerabdruck) macht die Bibliothek.
  - Ohne Konto und ohne Kosten nutzbar; Relay und Lobby lassen sich selbst betreiben.
  - Baut mit llvm-mingw. Zuerst mit einem kleinen Probeprogramm geprüft (direkt, nur Relay, WebSocket); heute deckt das `tests/net_transport_test.cpp` ab.
- **Verworfen:**
  - Steam Networking Sockets und EOS: Konto, SDK nur mit Konto; Spieler bräuchten Steam bzw. ein Epic-Produkt. Die DLL-Schnittstelle lässt Steam später als zweiten Weg zu.
  - GameNetworkingSockets (Open Source): ohne eigene Zertifizierungsstelle keine Authentisierung; dazu Protobuf mit Abseil und bekannte Linkprobleme mit LLVM.
  - netcode/yojimbo: keine NAT-Durchdringung; Connect-Tokens brauchen ein eigenes Backend.
  - Photon und PlayFab Party: Konto, laufende Kosten, aller Verkehr bei einem Dritten.
  - ENet mit eigenem Handshake und AES (V13.2): eigene Kryptografie, keine NAT-Durchdringung. **ENet, der Handshake und die AES-Schicht sind entfernt.**
- **Eigener Code bleibt klein:**
  - die Lobby: Räume, Codes, Weiterreichen der Beschreibungen, Raten, Sperren – keine Kryptografie;
  - eine Rahmung in der DLL: Hallo mit Version und Geheimteil des Codes, Ping für die Verbindungsqualität, Nutzdaten;
  - Spielprotokoll, Autorität und Prüfung wie bisher.
- **Bausteine statt eigener Kryptografie:**
  - Der Beitrittscode besteht aus Raumnummer und Geheimteil. Die Lobby sieht nur die Raumnummer. Das Geheimteil prüft der Host erst im DTLS-Kanal (Passwortprüfung über einen authentisierten Kanal).
  - Die lange Form des Codes enthält zusätzlich 60 Bit des Zertifikats-Fingerabdrucks des Hosts (Pinning). Der Gast prüft ihn, bevor er etwas sendet.
  - Mit dem langen Code kann weder eine kompromittierte Lobby noch ein Angreifer im lokalen Netz den Host vortäuschen. Mit dem kurzen Code vertraut der Gast der Lobby (Restrisiko, `MULTIPLAYER_BEDROHUNGSMODELL.md`, Abschnitt 9.5). Im Modus „Direkt“ ist der lange Code Pflicht.
- **Einstellungen am Bau von Mbed TLS** (`src/netdll/config/liminal_mbedtls_config.h`): DTLS-SRTP an (ohne baut libdatachannel nicht), Threads an (PSA-Zustand), Sockets aus.
- **Quellen unverändert:** Fehlende Includes in libdatachannel (`std::transform` ohne `<algorithm>`) werden beim Bau über `-include algorithm` ergänzt, nicht im Quelltext.
- **Nachträge während der Umsetzung (27.10.2026):**
  - **Verbindung zur Lobby über WinHTTP statt über den WebSocket-Client von libdatachannel.**
    - Dieser schaltet unter Windows die TLS-Prüfung ab („not supported on Windows“). Belegt im Test: Ein fremdes Zertifikat wurde angenommen.
    - WinHTTP prüft mit Schannel gegen die Stammzertifikate von Windows; zusätzlich ist Pinning möglich (`lobby_cert`).
    - `liminal_net.dll` importiert dafür `winhttp.dll`; `tools/check_no_network.py` erlaubt das nur für die DLL.
  - **Gegenmaßnahme zu `VERIFY_OPTIONAL`** (libdatachannel mit Mbed TLS): Die DLL verlangt einen von DTLS geprüften Fingerabdruck, der zur Beschreibung passt.
  - **Unzuverlässiger Kanal mit Lebensdauer 150 ms statt „0 Wiederholungen“:**
    - SCTP staut sonst veraltete Momentaufnahmen hinter seiner Staukontrolle. Gemessen im schlechten Netz (128 kbit/s): 6,6 s Laufzeit und 19 m Abweichung.
    - Mit Lebensdauer verfällt Altes; die Laufzeit bleibt bei 0,35 s.
  - **Laufzeit für die Anzeige aus SCTP;** der eigene Ping enthält die Wartezeit bis zum nächsten Bild. Der Ping zählt nur den Verlust.
  - **SCTP für Echtzeit:** Wiederholung höchstens nach 2 s, 12 Versuche (statt 10 s und 5).
    - Anlass: Im ersten 60-min-Langlauf über das gestörte Relay trennte die Stille-Erkennung nach 37 min einen Gast; SCTP hatte über 10 s nichts gesendet.
    - Ergebnis des Langlaufs danach: `MULTIPLAYER_TESTS.md`, Abschnitt 7.8.
  - **Stand beim Beitritt:** Der Host schickt nach dem Willkommen alle aufgenommenen und abgelegten Dinge der Ebene; der Gast merkt sich gemeldete Funde, die er noch nicht geladen hat.
    - Anlass: Im zweiten Langlauf wich der geteilte Stand eines Gastes dauerhaft ab (Fund in einem beim Gast nicht geladenen Sektor), und ein später Gast sah schon aufgenommene Funde.
    - Verworfen: die ganze Welt im Willkommen übertragen (größer, und die Welt entsteht ohnehin aus dem Seed).
  - **Absagen mit 1 s Nachlauf,** sonst kam die Absage beim Gast nicht an (er meldete „kein Weg“ statt „falscher Code“).
  - **Wer wegen ungültiger Nachrichten getrennt wird, behält keinen Platz** (vorher 120 s belegt).
  - **Obergrenze `kRoles` = 40** im Spielprotokoll (Format gleich). Unbekannte Rollen zeigt der Gast nicht an.
- **Freigabe:** Das Beta-Etikett ist entfernt, weil die Bedingungen des Auftrags erfüllt sind (`MULTIPLAYER_ENTSCHEIDUNG.md`, V13.3). Der Modus ist damit nicht „vollkommen sicher“; Restrisiken: `MULTIPLAYER_BEDROHUNGSMODELL.md`, Abschnitt 9.5.

## E-034 Abnahme-Reihenfolge mit vorgezogener Asset-Arbeit (V13.3, 27.10.2026)

- **Auftrag:** Die Ebenen werden in der Reihenfolge 1, 2, 3, 0, 5, 5.3, 188, 903, 37, 36 abgenommen (Schranken a–g).
- **Entscheidung:**
  - Die Abnahmen (Kritikrunden der Ebene, Laufweg, Prüfungen, Aufwand) laufen streng in dieser Reihenfolge im Hauptstrang.
  - Ausstattung und Wesen späterer Ebenen dürfen vorher in eigenen Zweigen entstehen (höchstens drei Agenten gleichzeitig).
  - Eine Ebene gilt erst als abgenommen, wenn ihre eigenen Runden mit den zusammengeführten Assets gelaufen sind.
- **Grund:** Blender-Modelle und Wesen brauchen Stunden je Klasse. Nacheinander ließe sich das nicht schaffen. Die Reihenfolge bestimmt, was zuerst fertig und geprüft ist, nicht was zuerst begonnen wird.

## E-035 Raummetrik: Grenzen je Ebene und Messkorrektur (V13.3, 27.10.2026)

- **Grenzen:** Die Grenzen stehen in `tools/qa/metric_limits.json` und sind je Ebene aus dem Wiki-Strukturtyp abgeleitet (`STRUKTURTYP.md`).
  - Labyrinthe mit Gängen (Level 2, 3) haben kürzere Sichtachsen als Hallen.
  - Level 5 nimmt Kesselgänge und Aufzugskabinen von der Gangbreite aus (Wiki: eng). Level 188 erlaubt kein Notausgangsschild.
- **Korrektur (27.10.):** `maplib.Grid` zählte sichtbare Blöcke als begehbar und durchsichtig, also auch freistehende Wandscheiben.
  - Neu: Begehbarkeit sperren verdeckte Blöcke, Gruben und sichtbare Blöcke ab 1,5 m. Niedrigere Blöcke sind Podeste, z. B. der Bahnsteig in Level 903.
  - Sicht auf Augenhöhe sperren Wände, Stützen und Blöcke ab 1,5 m.
  - Nach der Korrektur verletzte nur Level 1 eine Grenze (Sichtachse P90 79,7 m < 85 m). Die Aquila-Wandscheiben wurden von 12–18 m auf 6 m gekürzt.
- **Grund:** Eine Grenze, die eine falsche Messung besteht, beweist nichts. Die Messung wurde korrigiert, nicht die Grenze gelockert.

## E-036 Laufweg-Schranke e) bei Ebenen mit wenigen Räumen (V13.3, 01.11.2026)

- **Auftrag:** Laufweg mit mindestens 200 m und mindestens 15 Räumen, alle paar Meter ein Bild.
- **Grenze:** Zwei Ebenen haben laut Wiki und Karte weniger als 15 Räume:
  - Level 188 „The Courtyard of Windows“ (Wikidot Rev. 69): ein Atrium mit Gängen am Rand. Im Spiel sind es 7 Räume: Hof, weißer Gang, drei Zellen, Treppenhaus, Treppenabsatz.
  - Level 903 „The Metro“ (Rev. 60): ein einziger, endlos gleicher Bahnsteig, 1 Raum (`STRUKTURTYP.md`).
- **Entscheidung:** Die Strecke von mindestens 200 m wird eingehalten. Der Weg besucht alle erreichbaren Räume, mehrfach und aus anderen Richtungen.
  - `walkway.py` verlangt nicht mehr Räume, als erreichbar sind.
  - Vorher lief der Planer bei Level 188 bis zur Schutzgrenze im Kreis: 6,8 km, 1 696 Bilder.
  - Level 903 läuft den Bahnsteig auf zwei Spuren hin und zurück.
  - Mehr Räume hinzuzufügen, nur um die Zahl zu erreichen, widerspräche dem Wiki-Strukturtyp. Teil 2 des Auftrags verbietet selbst ergänzte Zonen.
- **Beleg:** Raumzahlen aus `data/maps/level188.json` und `level903.json`, ausgegeben von `tools/qa/room_metrics.py` (Spalte „Raeume“).

## E-037 Zeilenenden (V13.3, 01.11.2026)

- **Befund:** 37 Quelldateien wurden im Lauf von V13 von CRLF auf LF umgestellt, `level2.py` umgekehrt.
  - Ursache: `sed -i` in Git Bash entfernt das CR; Python-Textschreiben unter Windows setzt es.
  - Die Umstellungen kamen im Hauptstrang und in Agenten-Zweigen vor.
  - `gameplay_test.cpp` kollidierte deshalb beim Zusammenführen des Wranglers als ganze Datei.
- **Entscheidung:**
  - Während Agenten-Zweige offen sind, wird nicht massenhaft umgestellt; das gäbe Konflikte in jedem Zweig. Jede Datei behält ihr heutiges Zeilenende.
  - Gemischte Dateien (DECISIONS.md, PROGRESS.md) sind wieder einheitlich CRLF.
  - Ab jetzt werden Dateien nur noch binär bearbeitet (Python `rb`/`wb`, Zeilenende der Datei beibehalten) oder mit dem Edit-Werkzeug. Nie mit `sed -i`.
- **Wirkung auf das Spiel:** keine; Compiler, Python und JSON-Leser nehmen beide Formen.

## E-038 Entity 99 „The Game Master“ in Level 1 (V13.3, 01.11.2026)

- **Wiki:**
  - Level 1 (Wikidot Rev. 83): Gemälde liegen kurz vor den Türen zu den Hallen auf dem Boden und verschwinden unbeobachtet. Eines mit einer Narrenfigur führt beim Hindurchgehen nach Level 389.
  - entity-99 (Rev. 39): Marionetten-Narr mit Glasbildschirm-Gesicht. Sie lockt Wanderer in Spiele, betrügt selbst und verliert schlecht. „Wer betrügt, fliegt raus.“
- **Grenze:** Level 389 gibt es im Spiel nicht. Eine eigene Ebene nur für diesen Übergang wäre eine selbst ergänzte Zone (Teil 2).
- **Entscheidung (Interpretation, gekennzeichnet in KREATUR_GameMaster.md):**
  - Zwölf Plätze vor den Hallentüren, je Lauf vier Gemälde. Unbeobachtet verschwinden sie nach etwa 7 s und tauchen vor einer anderen Tür wieder auf.
  - Ihr Gemälde hat ein echtes Glasbildschirm-Gesicht, das den Ausdruck nur unbeobachtet wechselt. Dazu rote Kugeln und rote Fäden, die nach oben steigen.
  - Wer es betritt, erstarrt. Ihr Gesicht füllt das Bild, sie lädt mit eigenen Sätzen ein, ohne Spiele aus ihrem Katalog und ohne Inhalte anderer Spiele. Dann wirft sie einen „wegen Betrugs“ hinaus.
  - Man steht danach an anderer Stelle in Level 1: mindestens 30 m entfernt, nie im Garden. Sperrzeit 240 s je Spieler.
  - Im Koop rechnet der Host; der Gast erhält die Lage als Positionskorrektur.
- **Grund:** Verhalten und Übergang bleiben nah am Wiki, ohne eine Ebene zu erfinden. Das Hinauswerfen ist das einzige, was das Wiki über den Ausgang aus ihrem Spiel sagt.

## E-039 Benannte Einzelfiguren und Level-2-Wesen (V13.3, 02.11.2026)

- **Auftrag:** alle im Wikidot-Wiki dokumentierten Wesen; „bewusst weggelassen“ ist nicht erlaubt. `LICENSE_REVIEW.md` schloss benannte Einzelfiguren aus.
- **Entscheidung:**
  - Benannte Einzelfiguren (Jerry, später Samantha, Entity 18) werden als eigene Gestaltung gebaut; im Spieltext nie der Wiki-Name, keine Zitate, Quelle und Lizenz im Briefing (`LICENSE_REVIEW.md` ergänzt). Gruppen und Stützpunkte der Wikis (M.E.G., B.N.T.G., „Base …“) erscheinen nicht im Spiel (ein Satz des Skin-Stealers nannte „Base Gamma“ – ersetzt); die seit V12 sichtbaren Namen aus dem Wiki (Titel der Ebenen, „The Beverly Room“, „Promethei Bibliotheca“, Gate C32/C33) stehen mit Namensnennung in `LICENSES.md`.
  - Jerry (Entity 7, Rolle 18): Auslöser ist gegenseitiger Blickkontakt (Auslegung), Unterwerfung tödlich wie bei den anderen tödlichen Wesen; Krankheit und Menschenaugen als Körperhorror-Auslegung; Followers nur als Spuren.
  - Fensterwesen Level 2: bildet ungesehen ein eigenes Stahlfenster in der Stirnwand einer Sackgasse (E-031 aufgelöst).
  - Deathmoths Level 2 ohne Nest; nur Weibchen beißen (nach Ankündigung), Männchen friedlich.
  - Death Rats Level 2 trinken an der Spundöffnung der Fässer (Flüssigkeit nicht genannt: Auslegung). Fluchtregel in `thinkRat` für alle Ebenen: kein Fluchtziel mit Weg über dem 1,6-fachen der Luftlinie oder näher am Spieler (Fehlerbehebung).
  - Skin-Stealer in Level 2 nur als Spuren (laut Seite).
- **Grund:** Auftrag Teil 4; Belege in `KREATUR_Jerry.md`, `KREATUR_Fensterwesen.md`, `KREATUR_Deathmoth.md`, `KREATUR_DeathRat.md`, `KREATUR_L2_Spuren.md`.

## E-040 Level-3-Wesen sprechen Englisch (V13.3, 02.11.2026)

- **Entscheidung:** Sätze der Skin-Stealer und Fetzen der Wretches sind englisch (die Level-3-Seite nennt einfaches Englisch), mit englischen Untertiteln; die Oberfläche bleibt deutsch. Waffen, Locken, Entlarvung beim dritten Ansprechen und die Zeiten sind Auslegung und in den Briefings markiert.
- **Grund:** Seite Level 3 (Wikidot, Rev. 18); `KREATUR_Wretch.md`, `KREATUR_SkinStealer.md`.

## E-041 Schatten unsichtbarer Blöcke (V13.3, 02.11.2026)

- **Befund:** Modelle auf unsichtbaren Kollisionsblöcken lagen im Schatten ihres eigenen Blocks (Wagenseite Level 903/37 im Lampenlicht schwarz, Treppenblock, Rohre Level 2).
- **Entscheidung:** `lighting.hlsl` `traceShadow`: Zellen gleicher Höhe wie die Startzelle verdecken nicht, solange der Strahl noch nicht in einer freien Zelle war; Wände (fest) und andere Blöcke werfen weiter Schatten.
- **Beleg:** Gegenprobe 198 Ansichten in allen zehn Ebenen, alter gegen neuen Shader – nur Modelle auf unsichtbaren Blöcken heller, kein Licht durch Wände (`docs/screens/level903/schatten_*.jpg`).

## E-042 Audio-Fehler (V13.3 Teil 6, 02.11.2026)

- **Ursache und Behebung:** siehe `AUDIO_BUG.md` (B-017 Stimmen gestoppter, unhörbarer Schleifen nie frei; B-018 Unterläufe; B-019–B-021 Gerätewechsel, Öffnen, Dämpfung). Watchdog mit Stille-Erkennung am Master-Ausgang.
- **Grenze:** Echtes Umstecken, echter Formatwechsel, Bluetooth und Hören nicht geprüft (ein Gerät, Lautsprecher stumm); simuliert und dokumentiert.

## E-043 Öffentlicher Upload von V11 und V12 (02.11.2026)

- **Auftrag des Nutzers (ersetzt E-025 und den Auftrag „privates Repository“):** V11 und V12 öffentlich im bestehenden Repository `origin` wie V1–V10; V13 erst, wenn V13.3 vollständig fertig ist.
- **Entscheidung:**
  - Hochgeladen: `main`, `liminal-v11`, `liminal-v12`, Tags `v11`, `v12`; nicht hochgeladen: `liminal-v13`, Tag `v13`, Agenten-Branches.
  - LFS nur für diese drei Branches (`git lfs push origin main liminal-v11 liminal-v12` statt `--all`), damit nichts aus V13 hochgeht.
  - Wurzeldateien der Branches neu (README ohne „privat“, `LICENSES.md`, `ATTRIBUTION.md`, `licenses/`); Versionsordner unverändert; Tags bleiben auf den Ständen vom 26.10.
  - README auf `main` neu, Bilder neu in 1920×1080 aus V12/V11 aufgenommen (Auswahl und Gründe in `GITHUB_UPLOAD.md`).
  - Lizenz des Spiels nicht gewählt (Entscheidung des Eigentümers, `LICENSES.md`).
- **Beleg:** `GITHUB_UPLOAD.md`, `git ls-remote origin`.

## E-044 Randwesen von Level 2 (V13.3, 02.11.2026)

- **Entscheidung:** Cannibal Cuisine, Swindlebird, Photoshop und Macchina als Weltobjekte mit Verhalten ohne eigene Rolle (`gameplay/apparitions_l2.*`, Muster der Level-1-Erscheinungen); Host rechnet, Protokoll unverändert.
  - Swindlebird: „unbestätigt“ als Spielregel – höchstens eine Sichtung je Lauf (50 %), nur ungesehen, verschwindet beim Ansehen, unter 8 m oder im Lampenlicht; keine Spur.
  - Photoshop: ohne Kamera im Spiel rettet nur die Gabe (Ablegen/Werfen); Box schrumpft sichtbar 5 s; höchstens einer je Lauf (60 %) hinter einem Röhrenmonitor.
  - Macchina: nur Reste in der Werkstatt (laut Seite seit etwa 1850 verschwunden); Blechtafel als Fundnotiz.
  - Cannibal Cuisine: gefährlich nur beim Angriff mit dem Beil; Sucht als Spielregel (Mandelwasser und Riegel wirken nicht, Entzug ab 40 m); 3 Waren je Automat.
  - Wiki-Namen nicht im Bild und nicht im Spieltext (E-039).
- **Beleg:** `KREATUR_CannibalCuisine.md`, `KREATUR_Swindlebird.md`, `KREATUR_Photoshop.md`, `KREATUR_Macchina.md`.

## E-045 Wesen von Level 3 vervollständigt (V13.3, 02.11.2026)

- **Purpifizierte Wesen:** Erscheinung ohne Rolle (`gameplay/purpification.*`); die Seite nennt kein Verhalten – die Toten bewegen sich nicht; violettes Licht, Brummen, Strahlung zehrt nah am Verstand, Lampe stockt (Auslegung). Zwei seltene Fälle (Skin-Stealer, Hound mit Botschaft).
- **Death Rats und Deathmoths:** nur Varianten (Rat 2 groß, Rat 3 Falle; Moth 3/4 gewöhnlich, Moth 5 Praetorianer mit viertem Beinpaar). Fallen wie ein Elektrozaun (Draht 18 cm, 10 Schaden und Krampf, jede vierte versagt); Klugheit als Arbeitsteilung, Gedächtnis je Ebene, Warten außerhalb des Lichts. Schwarmschaden 0,9 je Motte und Sekunde statt Sofort-Tod (Fairness).
- **Hound-Rudel und großes Bündel:** Varianten der Rollen Hound (1: Level 2, 2: Level 3) und Clump (1 bewaffnet, 2 entwaffnet). Rudel: immer nur einer springt, mit freier Sicht und Ankündigung; Ansehen bricht ab; Level 3 quält statt sofort zu töten. Großes Bündel blind, Ausholen 1,1 s hörbar, Hiebe nicht tödlich.
- **Smiler „ohne Anlass“** (Level 3, `presence.smiler_unprovoked` 0,35): Angriff ohne Auslöser durch den Spieler, aber 2,2 s angekündigt; bekannte Abwehr wirkt.
- **Red Knight (Entity 71):** Körper und Auftritt in Level 3 belegt (Vorfallbericht) – gebaut als nicht feindliche Erscheinung (Flucht in Panik, Hieb gegen das Unsichtbare, Riss), Variante 4 der Rolle Wretch, eigener Zufall.
- **Speicherfehler beim Zusammenführen:** B-025 bis B-028 (`BUGLISTE.md`), gefunden mit AddressSanitizer.
- **Beleg:** `KREATUR_L3_Purpur.md`, `KREATUR_DeathRat.md`, `KREATUR_Deathmoth.md`, `KREATUR_Hound.md`, `KREATUR_Clump.md`, `KREATUR_Smiler.md`, `KREATUR_RedKnight.md`.
