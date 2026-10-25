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

## E-004 Levelauswahl
Folgt in `LEVELAUSWAHL.md`. Quelle ausschließlich backrooms-wiki.wikidot.com, Einzelseiten vollständig gelesen.
