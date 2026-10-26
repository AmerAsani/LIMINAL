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
