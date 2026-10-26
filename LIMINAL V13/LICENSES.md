# LIMINAL V13 – Lizenzen, Quellen, Namensnennung

Stand: 26.10.2026. **Das ist keine Rechtsberatung, und die Rechtslage ist nicht abschließend geklärt** (siehe Abschnitt 8).
Diese Datei listet jedes fremde Material, das in V13 steckt oder als Grundlage diente, mit Quelle, Autorinnen und Autoren und Lizenz.
**V13 fügt kein fremdes Material hinzu:** keine neuen Texturen, Modelle, Klänge oder Schriften. Neu sind eigene Modelle (Hand, sechs Wesen) und eigener Code (Abschnitte 3 und 9).
Ältere Überlegungen: `LICENSE_REVIEW.md`, `ATTRIBUTION.md` (beide aus V11).

---

## 1. Backrooms Wiki (Wikidot) – Grundlage der zehn Ebenen

- **Quelle:** https://backrooms-wiki.wikidot.com. Text unter **CC BY-SA 3.0** (Seitenfuß und Licensing Guide des Wikis).
- **Wie verwendet:**
  - Aus jeder Seite wurden Fakten über Bau, Licht, Klang, Wesen und Übergänge in **eigenen Worten** in `LEVELSPEC_<Nr>.md` festgehalten und danach umgesetzt.
  - Kein Text wurde kopiert. Kein Bild und kein Ton der Seiten wurde verwendet; diese haben eigene Lizenzen, die jede Seite in ihrem Lizenzkasten nennt.
  - Was LIMINAL ergänzt oder weglässt, steht je Level in Abschnitt 2 und 3 der LEVELSPEC.
- **Aus dem Wiki übernommene Namen und Kennungen** (Fakten, keine Texte):
  - Titel der Ebenen;
  - „The Beverly Room“ (Silberschild, Level 5), „Promethei Bibliotheca“ (Inschrift, Level 5.3);
  - „BTA“ (Stand, Level 36), Gate C32 und die Flugnummer 1784XBA8JLP (Level 36);
  - die Namen der Wesen.
- **Änderungsvermerk:** LIMINAL ist eine freie, veränderte Umsetzung. Maße, Anordnung, Modelle, Licht, Klänge und alle Texte im Spiel sind eigene Arbeit.

| Seite | Fassung | Autorinnen und Autoren laut Seite | Abgerufen |
|---|---|---|---|
| [Level 0 – „Threshold“](https://backrooms-wiki.wikidot.com/level-0) | Rev. 50, 06.10.2026 | Grundidee aus einem 4chan-Beitrag; bearbeitet von u/Deveyerr; Neufassungen DivineAtlas, Robert Goerman, DrAkimoto | 25./26.10.2026 |
| [Level 1 – „The Habitable Zone“](https://backrooms-wiki.wikidot.com/level-1) | Rev. 83, 07.10.2026 | Praetor3005, DivineAtlas; Idee EnderMitten; ursprünglich u/ThePizzaEater1000, Neufassung etoisle | 25./26.10.2026 |
| [Level 2 – „Abandoned Utility Halls“](https://backrooms-wiki.wikidot.com/level-2) | Rev. 93, 18.10.2026 | u/M654z; Neufassung 1000dumplings; Eigentümer Boring Talking; Zitierhinweis Greggita Mahayfaio | 25./26.10.2026 |
| [Level 3 – „Electrical Station“](https://backrooms-wiki.wikidot.com/level-3) | Rev. 18, 26.09.2026 | u/M654z; Neufassungen Natedagreat563 | 25./26.10.2026 |
| [Level 5 – „Terror Hotel“](https://backrooms-wiki.wikidot.com/level-5) | Rev. 184, 07.08.2026 | Discord13; Neufassung Stretchsterz; Beiträge ratscrapz | 25./26.10.2026 |
| [Level 5.3 – „Promethei Bibliotheca“](https://backrooms-wiki.wikidot.com/level-5-3) | Rev. 40, 24.08.2026 | Praetor3005 | 26.10.2026 |
| [Level 188 – „The Courtyard of Windows“](https://backrooms-wiki.wikidot.com/level-188) | Rev. 69, 24.10.2026 | 1000dumplings; Neufassung Kai4C; Dank an Alfarex, Mctoran, Praetor3005, Paint Palette, r a t i f | 26.10.2026 |
| [Level 903 – „The Metro“](https://backrooms-wiki.wikidot.com/level-903) | Rev. 60, 24.10.2026 | unbekannter Reddit-Nutzer; Neufassung Natedagreat563; Lizenzhinweis Natedagreat563 und Wesbot | 26.10.2026 |
| [Level 37 – „Sublimity“](https://backrooms-wiki.wikidot.com/level-37) | Rev. 52, 18.08.2026 | egglord; Kritik Sariastuff | 26.10.2026 |
| [Level 36 – „Timeless Airport“](https://backrooms-wiki.wikidot.com/level-36) | Rev. 33, 15.08.2026 | Endrew1; Dank an Praetor3005, KillerOreo52, Abdallah Amr | 26.10.2026 |
| [Entity 3 – Smilers](https://backrooms-wiki.wikidot.com/entity-3), [Entity 4 – Deathmoths](https://backrooms-wiki.wikidot.com/entity-4), [Entity 5 – Clump](https://backrooms-wiki.wikidot.com/entity-5), [Entity 8 – Hounds](https://backrooms-wiki.wikidot.com/entity-8) | Recherche V10/V11 (`docs/ENTITAETEN.md`) | siehe jeweilige Seite; Entity 5: Kekcroc, Neufassung TrailmixNCocoa | 25.10.2026 |
| [Licensing Guide](https://backrooms-wiki.wikidot.com/licensing-guide) | – | Wiki-Team | 25.10.2026 |

Fandom-Seiten wurden für V12 **nicht** verwendet (Auftrag: nur Wikidot).

## 2. Texturen

- **ambientCG:** alle unter **CC0 1.0** (https://docs.ambientcg.com/license/). Der Lock (`tools/textures/ambientcg_lock.json`) hält je Set Download-URL, Größe, SHA-256, Lizenz und Abrufdatum fest; `fetch_ambientcg.py` prüft die Prüfsummen. Namensnennung ist nicht verlangt, steht hier aber trotzdem.
- **Eigene Texturen:** prozedural erzeugt von `tools/textures/generate.py`, ohne fremde Vorlage: Damasttapete (Level 5), Buchrücken und Tuffstein (Level 5.3).

| Name im Spiel | Quelle | Lizenz | Herkunft | geplant für Level |
|---|---|---|---|---|
| `wallpaper_threshold` | [Wallpaper001A](https://ambientcg.com/view?id=Wallpaper001A) „Wallpaper 001 A“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 23d7add92ef4… | 0 |
| `wallpaper_threshold_mold` | [Wallpaper002C](https://ambientcg.com/view?id=Wallpaper002C) „Wallpaper 002 C“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 767a4cc16bef… | 0 |
| `carpet_threshold` | [Fabric028](https://ambientcg.com/view?id=Fabric028) „Fabric 028“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 be3c5a8a3431… | 0 |
| `ceiling_office` | [OfficeCeiling001](https://ambientcg.com/view?id=OfficeCeiling001) „Office Ceiling 001“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 c28085beb797… | 0 |
| `carpet_red` | [Carpet013](https://ambientcg.com/view?id=Carpet013) „Carpet 013“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 cad4ba111965… | 0 |
| `concrete_wall_smooth` | [Concrete046](https://ambientcg.com/view?id=Concrete046) „Concrete 046“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 597e009cb504… | 1, 188 |
| `concrete_wall_old` | [Concrete036](https://ambientcg.com/view?id=Concrete036) „Concrete 036“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 231c1f965748… | 1, 2 |
| `concrete_floor` | [Concrete047A](https://ambientcg.com/view?id=Concrete047A) „Concrete 047 A“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 70a9be4b8107… | 1 |
| `concrete_cast` | [Concrete028](https://ambientcg.com/view?id=Concrete028) „Concrete 028“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 90618c341fa6… | 1 |
| `bricks_dirty_brown` | [Bricks097](https://ambientcg.com/view?id=Bricks097) „Bricks 097“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 0c4ea4f9ba83… | 2 |
| `concrete_chalky` | [Concrete012](https://ambientcg.com/view?id=Concrete012) „Concrete 012“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 449936d94c11… | 2 |
| `metal_rusty_iron` | [Metal041B](https://ambientcg.com/view?id=Metal041B) „Metal 041 B“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 5930fb7ed48e… | 2, 3 |
| `painted_metal_white` | [PaintedMetal012](https://ambientcg.com/view?id=PaintedMetal012) „Painted Metal 012“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 e52b3753c473… | 2, 3 |
| `painted_metal_grey` | [PaintedMetal014](https://ambientcg.com/view?id=PaintedMetal014) „Painted Metal 014“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 ee4f38cdbb54… | 2 |
| `bricks_dark_grey` | [Bricks089](https://ambientcg.com/view?id=Bricks089) „Bricks 089“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 acc430a760c4… | 3 |
| `bricks_white_old` | [Bricks038](https://ambientcg.com/view?id=Bricks038) „Bricks 038“ | CC0-1.0 | ambientCG, abgerufen 2026-10-26, SHA-256 58d3de6c94e7… | 3 |
| `bricks_grey_black` | [Bricks056](https://ambientcg.com/view?id=Bricks056) „Bricks 056“ | CC0-1.0 | ambientCG, abgerufen 2026-10-26, SHA-256 aa9aa1151c9b… | 3, 2 |
| `wallpaper_damask_red` | eigen (damask) | – | eigenes Werk, `tools/textures/generate.py` | – |
| `books_spines` | eigen (books) | – | eigenes Werk, `tools/textures/generate.py` | – |
| `tuff_stone` | eigen (tuff) | – | eigenes Werk, `tools/textures/generate.py` | – |
| `bricks_brown_old` | [Bricks094](https://ambientcg.com/view?id=Bricks094) „Bricks 094“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 51be701039d6… | 3 |
| `bricks_red_factory` | [Bricks085](https://ambientcg.com/view?id=Bricks085) „Bricks 085“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 d27fbc435e45… | 3 |
| `concrete_rough` | [Concrete037](https://ambientcg.com/view?id=Concrete037) „Concrete 037“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 4f4fe9e6609f… | 3 |
| `wood_walnut_floor` | [WoodFloor046](https://ambientcg.com/view?id=WoodFloor046) „Wood Floor 046“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 88ee44280193… | 5, 5.3 |
| `marble_white` | [Marble021](https://ambientcg.com/view?id=Marble021) „Marble 021“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 13a661f0d2ad… | 5 |
| `marble_beige` | [Marble014](https://ambientcg.com/view?id=Marble014) „Marble 014“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 b03213a1f09a… | 5.3 |
| `carpet_red_pattern` | [Fabric026](https://ambientcg.com/view?id=Fabric026) „Fabric 026“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 d25e9cdd1727… | 5 |
| `leather_red` | [Leather011](https://ambientcg.com/view?id=Leather011) „Leather 011“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 c61346f350d1… | 5 |
| `travertine` | [Travertine009](https://ambientcg.com/view?id=Travertine009) „Travertine 009“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 927ca19b99e3… | 5.3 |
| `wood_herringbone_dark` | [WoodFloor014](https://ambientcg.com/view?id=WoodFloor014) „Wood Floor 014“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 a5c2c6ccef30… | 5.3 |
| `concrete_white` | [Concrete034](https://ambientcg.com/view?id=Concrete034) „Concrete 034“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 fafd7cd3aee2… | 188 |
| `moss` | [Moss002](https://ambientcg.com/view?id=Moss002) „Moss 002“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 03fe8868a5b5… | 188 |
| `wood_birch` | [WoodFloor066A](https://ambientcg.com/view?id=WoodFloor066A) „Wood Floor 066 A“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 5c20b58e4b87… | 188 |
| `paving_grey` | [PavingStones144](https://ambientcg.com/view?id=PavingStones144) „Paving Stones 144“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 fcbff7e4336e… | 188 |
| `paving_dark_blocks` | [PavingStones127](https://ambientcg.com/view?id=PavingStones127) „Paving Stones 127“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 b62840e6d57f… | 903 |
| `tiles_white` | [Tiles107](https://ambientcg.com/view?id=Tiles107) „Tiles 107“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 fa76887bb33a… | 37, 903 |
| `tiles_white_bath` | [Tiles036](https://ambientcg.com/view?id=Tiles036) „Tiles 036“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 548005dc451d… | 37 |
| `paint_stripes_yellow` | [PaintedMetal016](https://ambientcg.com/view?id=PaintedMetal016) „Painted Metal 016“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 df3458920716… | 903 |
| `carpet_blue_dark` | [Carpet012](https://ambientcg.com/view?id=Carpet012) „Carpet 012“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 476528d0deb9… | 36 |
| `terrazzo_antique` | [Terrazzo004](https://ambientcg.com/view?id=Terrazzo004) „Terrazzo 004“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 b781734c13ba… | 36 |
| `metal_aluminum` | [Metal050A](https://ambientcg.com/view?id=Metal050A) „Metal 050 A“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 c4d0ac0d0c87… | 36, 903 |
| `plaster_white` | [Plaster002](https://ambientcg.com/view?id=Plaster002) „Plaster 002“ | CC0-1.0 | ambientCG, abgerufen 2026-10-25, SHA-256 c364e72a39ea… | 36, 188 |

## 3. Modelle

- **Alle Modelle sind eigene Arbeit:** Python-Skripte für Blender in `tools/blender/models/models_*.py`, exportiert als `.glb` nach `data/models/`.
  - Es gibt keine heruntergeladenen Modelle.
  - V13: Die linke Hand (`models_hand.py`) und alle sechs Wesen (Hound, Smiler, Clump, Gestalt, Deathmoth, Nest) sind neu in Blender gebaut – eigene Skripte in `tools/blender/creatures/`, ohne Vorlage, ohne Scan, ohne fremdes Modell (E-021). Die Wesen folgen den Wikidot-Seiten nur in eigenen Worten (`KREATUR_<Name>.md`); es wurde kein Bild der Seiten und kein anderes Backrooms-Spiel als Vorlage benutzt.
  - Der alte prozedurale Bau der Wesen (aus V10/V11, `render/entity_models.cpp`) bleibt nur als Rückfall, falls eine `.glb` fehlt.
  - OpenVDB (in Blender enthalten, Mozilla Public License 2.0) dient beim Bau nur als Werkzeug zur Vernetzung; nichts davon liegt im Spiel.
- **Blender 4.5.14 LTS:** GPL-2.0-or-later, https://www.blender.org. Nur Werkzeug; nicht im Spielpaket, liegt unter `.toolchain/`.
  - Was man mit Blender erzeugt, gehört den Urhebern, nicht der Blender Foundation (Blender-Lizenzseite).

## 4. Schrift

- **Inter** (SIL Open Font License 1.1; Copyright 2016 The Inter Project Authors, https://rsms.me/inter/):
  - Liegt den Blender-Datendateien bei (`datafiles/fonts/Inter.woff2`).
  - Die Zimmernummern, das Beverly-Schild, die Inschrift der Bibliothek und alle Schilder und Tafeln in Level 36 wurden daraus als **Geometrie** erzeugt.
  - Die Schriftdatei selbst wird nicht mitgeliefert. Die OFL erlaubt die Einbettung in erzeugte Werke.
  - Der Lizenztext der OFL soll im Paket liegen; das ist noch offen (Abschnitt 8).
- **Oberfläche:** Segoe UI und Consolas werden zur Laufzeit aus den installierten Windows-Schriften gerastert und nicht mitgeliefert (wie in V11).

## 5. Klänge

Alle Klänge entstehen beim Start im Programmcode (`src/audio/synth_*.cpp`): Raumklang, Musik des Hotels, Durchsage, Buchfall, Schritte, Wesen. Es gibt keine Samples und keine Aufnahmen.

## 6. Texte im Spiel

Notizen (`data/notes.json`), Hilfe, Abspann und alle Beschriftungen sind selbst geschrieben. Fakten aus dem Wiki (siehe Abschnitt 1) stehen dort in eigenen Worten.

## 7. Software im Paket

| Bestandteil | Herkunft | Lizenz |
|---|---|---|
| llvm-mingw-Laufzeit (libc++, libc++abi, libunwind, compiler-rt), statisch gebunden | https://github.com/mstorsjo/llvm-mingw | Apache License 2.0 mit LLVM Exceptions |
| mingw-w64-Laufzeit | https://www.mingw-w64.org | ZPL / MIT-ähnlich |

## 8. Offene Rechtsfragen (nicht geklärt)

1. **ShareAlike für das ganze Spiel:**
   - Der Licensing Guide des Wikis verlangt, dass ein Spiel, das auf Wiki-Inhalten aufbaut, unter CC BY-SA 3.0 erscheint, oder alternativ unter GPLv3 mit vollständigem Quellcode, ohne DRM.
   - V12 nutzt die Seiten deutlich enger als V11 (Weg A in `LICENSE_REVIEW.md`). Die Entscheidung über die Lizenz des Spiels muss die Eigentümerin oder der Eigentümer des Projekts treffen; sie ist **nicht** getroffen.
2. **Marken:** „Backrooms“ und einzelne Namen wurden **nicht** auf Markenschutz geprüft (DPMA, EUIPO, USPTO).
3. **Inter als Geometrie:** Ob die Lizenztext-Pflicht der OFL auch für in Modelle umgewandelte Glyphen gilt, ist nicht geprüft. Vorsorglich sollte `OFL.txt` ins Paket.
4. **Namen und Kennungen aus dem Wiki** (Abschnitt 1): kurze Namen sind in der Regel nicht urheberrechtlich geschützt. Zusammen mit genauen Beschreibungen können sie aber eine Bearbeitung darstellen. Abgedeckt ist das nur durch die CC-Lizenz und diese Namensnennung.
5. **Bildnachweise der Wiki-Seiten:** Die Bilder wurden nicht verwendet. Ihre Lizenzen (teils CC BY-SA, CC BY 2.0) betreffen V12 deshalb nicht.

## 9. Verfahren aus Veröffentlichungen (kein fremder Code)

| Verfahren | Quelle | Verwendung |
|---|---|---|
| Kachelbruch, „texture repetition“, Variante 3 | Inigo Quilez, Artikel auf https://iquilezles.org/articles/texturerepetition/ | Idee nachprogrammiert in `shaders/gbuffer.hlsli` (E-022); kein Code übernommen |
| Abstandsfunktionen für Kapsel mit zwei Radien und Ellipsoid | Inigo Quilez, „distance functions“, https://iquilezles.org/articles/distfunctions/ | Formeln in `tools/blender/creatures/sdf.py` (Bau der Wesen, nicht im Spiel) |
