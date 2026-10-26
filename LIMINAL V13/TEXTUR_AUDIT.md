# LIMINAL V13 – Textur-Audit

Stand 26.10.2026. Grundlage: alle zehn Ebenen aus denselben festen Kameras vor und nach der Änderung (`docs/screens/texturen/<ebene>_vorher.jpg`, `…_nachher.jpg`; Ausschnitte nebeneinander in `vergleich_l01.jpg`, `vergleich_rest.jpg`, `vergleich_crop.jpg`), Lampenaufnahmen eingeschlossen. Entscheidung E-022 in `DECISIONS.md`.

## 1. Bestand

- **Quellen:** 37 PBR-Sätze von ambientCG (CC0 1.0) und 3 eigene, prozedural erzeugte Sätze (Damasttapete, Buchrücken, Tuffstein; `tools/textures/generate.py`). Jeder Satz hat Farbe, Normalen (DirectX), Rauheit, Umgebungsverdeckung, Metall und Höhe, zusammengefasst in `albedo.jpg`, `normal.png`, `orm.png` (1024 px). Herkunft, Lizenz und SHA-256 je Satz: `LICENSES.md` Abschnitt 2 und `tools/textures/ambientcg_lock.json`.
- **Keine Texturen aus anderen Backrooms-Spielen, keine Wiki-Bilder.** Geprüft: Alle Einträge in `data/textures/textures.json` sind entweder ambientCG-IDs mit Prüfsumme im Lock oder `eigen:`-Generatoren. V13 fügt keine neue Bildquelle hinzu.
- **Eigene Ebenen im Code** (`render/texture_gen.cpp`, prozedural): Putz, Teppich, Platten, Fliesen, Raster, Deckenplatten, Tapete, Ziegel, Paneele, Beton, Holz, Metall, Stoff u. a. – für Ausstattung und ältere Teile.

## 2. Je Ebene

`*` = Kachelbruch an (Abschnitt 3). Bilder: `docs/screens/texturen/`.

| Ebene | Wände / Böden / Decken (Satz) | Befund vorher | Nachher |
|---|---|---|---|
| 0 – Threshold | `wallpaper_threshold`, `carpet_threshold*`, `ceiling_office`; Nebenräume `carpet_red*`, `concrete_*`, `plaster_white*` | Identität stimmt (gelbe Tapete, feuchter Teppich, Rasterdecke). Der Teppich zeigt in großen Hallen ein wiederkehrendes Fleckenmuster im 2-m-Raster. | Teppich ohne erkennbares Raster; Tapete und Decke unverändert (Muster mit Raster). |
| 1 – Habitable Zone | `concrete_wall_smooth*`, `concrete_wall_old*`, `concrete_floor*`, `concrete_cast*`; Räume mit Ziegel, Fliesen, Holz | Parkhaus-Beton passt. An großen Wänden wiederholt sich ein waagerechter Schmutzstreifen genau alle 2,4 m. | Streifen gebrochen, kein Wiederholmuster; Ziegel, Fliesen, Holz unverändert. |
| 2 – Pipe Dreams | `concrete_chalky*`, `concrete_wall_old*`, Ziegel (`bricks_dirty_brown`, `bricks_dark_grey`) | Tunnelwände zeigen denselben großen Fleck alle 2 m, im Fluchtpunkt deutlich. | Flecken unregelmäßig verteilt; Ziegel unverändert. |
| 3 – Electrical Station | Ziegel (vier Sätze), `concrete_rough*`, `marble_beige*`, `metal_aluminum*` | Ziegel stark und eigenständig; Betonböden wiederholen sich in langen Gängen. | Böden ohne Raster; Ziegel unverändert. |
| 5 – Terror Hotel | `wallpaper_damask_red` (eigen), `carpet_red_pattern`, `wood_herringbone_dark`, `marble_white*` | Identität sehr stark (rote Damasttapete, Fischgrät, Kronleuchter). Muster sind beabsichtigt. | Marmor ohne Raster; Muster unverändert. |
| 5.3 – Bibliothek | `wallpaper_damask_red`, `marble_beige*`/`marble_white*`, Holz; Buchrücken (eigen) an den Regalen | Marmor groß genug. Buchrücken wiederholen sich aus der Nähe sichtbar (Regal neben Regal gleich). | Marmor ohne Raster. Buchrücken: offen (Abschnitt 5). |
| 36 – Flughafen | `carpet_blue_dark*`, `terrazzo_antique*`, `metal_aluminum*`, `plaster_white*`, `ceiling_office` | Terrazzo wiederholt sich in den langen Hallen kaum sichtbar (feines Korn); Teppich in den Wartezonen. | Kein Raster mehr; in der Ferne zeigt der Terrazzo an Übergängen zweier Versätze leichte Helligkeitswolken (gewollt weich, siehe `vergleich_crop.jpg`). |
| 37 – Poolrooms | `tiles_white_bath` | Fliesenraster gehört zur Ebene; Fugen sind gleichmäßig wie gewollt. | unverändert (Raster). |
| 188 – Hof der Fenster | `concrete_white*`, `moss*`; Wege `paving_*` | Weißer Putz und Rasen wiederholen sich auf den hohen Fassaden und dem Rasen. | Putz und Rasen ohne Raster; Pflaster unverändert. |
| 903 – Endstation | `bricks_grey_black` (Bahnsteig, graue Klinker wie im Wiki), `concrete_cast*`, `tiles_white` | Klinker und Fliesen mit gewolltem Raster; Beton an Decke und Wänden wiederholt sich wenig. | Klinker und Fliesen unverändert (Raster); Beton ohne Raster. |

## 3. Kachelbruch (E-022)

- **Verfahren** (`shaders/gbuffer.hlsli`, `shadeSurface`): nach I. Quilez, „texture repetition“, Variante 3. Ein großflächiger Index aus der Variationstextur wählt je Stelle einen von acht Versätzen derselben Textur; zwischen zwei benachbarten Versätzen wird weich überblendet, gesteuert vom Helligkeitsunterschied (kein Brei an den Übergängen). Ableitungen kommen aus der unverschobenen Koordinate, die Mip-Wahl bleibt stabil.
- **Nur für Texturen ohne Raster** (`"antitile": 1` in `data/textures/textures.json`, für eigene Ebenen in `render/renderer.cpp`): Teppich, Beton, Putz, Marmor, Terrazzo, Moos, Metall, Leder, Tuff. Bei Ziegeln, Fliesen, Deckenplatten, Pflaster, Holzdielen, Tapeten und Buchrücken würde ein Versatz das Raster zerschneiden – dort bleibt es aus (ein Abruf wie bisher).
- **Kosten:** bei Ebenen mit Kachelbruch zwei Abrufe je Kanal statt einem (Farbe, Normalen, ORM) plus ein Abruf der Variationstextur. Messung in Abschnitt 4.
- **Bestehende Variation** (großflächige Tönung und Rauheit aus der Variationstextur) bleibt und wirkt zusätzlich.

## 4. Leistung und Grafikspeicher

Messung: `perf_sweep.ps1` (je Ebene am Start, 1600 × 900, Qualität 2, Zeitmessung der GPU), dieselbe Maschine, ohne andere Last, vorher (`bin_vorher`) und nachher.

| Ebene | G-Buffer vorher | G-Buffer nachher | Differenz | GPU gesamt vorher | GPU gesamt nachher | VRAM nachher |
|---|---|---|---|---|---|---|
| 0 | 0.29 ms | 0.46 ms | +0.17 ms | 2.03 ms | 3.32 ms | 957 MB |
| 1 | 0.35 ms | 0.53 ms | +0.18 ms | 4.57 ms | 4.78 ms | 947 MB |
| 2 | 0.41 ms | 0.57 ms | +0.16 ms | 2.93 ms | 3.17 ms | 959 MB |
| 3 | 0.29 ms | 0.38 ms | +0.09 ms | 2.74 ms | 2.93 ms | 947 MB |
| 5 | 0.48 ms | 0.59 ms | +0.11 ms | 3.96 ms | 4.00 ms | 973 MB |
| 5.3 | 0.69 ms | 0.71 ms | +0.02 ms | 4.03 ms | 3.99 ms | 1003 MB |
| 188 | 0.36 ms | 0.50 ms | +0.14 ms | 6.63 ms | 7.39 ms | 945 MB |
| 903 | 1.54 ms | 0.51 ms | -1.03 ms | 5.22 ms | 3.28 ms | 946 MB |
| 37 | 0.34 ms | 0.40 ms | +0.06 ms | 2.89 ms | 2.85 ms | 939 MB |
| 36 | 0.29 ms | 0.42 ms | +0.13 ms | 7.64 ms | 7.56 ms | 943 MB |

- **Ergebnis:** Der Kachelbruch kostet im G-Buffer-Durchgang 0,06–0,18 ms je Bild (1600 × 900); in Ebenen mit wenig Kachelbruch (5.3, 37) kaum etwas. Die gesamte GPU-Zeit bleibt mit 2,9–7,6 ms weit unter dem Bildbudget von 16,7 ms (60 Hz).
- **Streuung:** je Ebene eine Messung. Die Gesamtzeiten schwanken zwischen Läufen um bis zu ±1 ms (Taktstufen der Grafikkarte; Level 903 vorher: Ausreißer im G-Buffer mit 1,54 ms). Belastbar ist die Differenz im G-Buffer, wo der Kachelbruch allein wirkt.

Grafikspeicher (aus dem Protokoll: Texturen der Welt und der Wesen; dazu der belegte Grafikspeicher des Prozesses über DXGI 1.4, neu im Befehl `perf`):

| Posten | Größe | Änderung durch V13 |
|---|---|---|
| Welttexturen: 91 Ebenen à 1024 px, 9 Mip-Stufen (Farbe BC1, Normalen BC5, ORM unkomprimiert RGBA8) | 667 MB | keine (kein neuer Satz) |
| davon ORM (Verdeckung, Rauheit, Metall, Höhe) unkomprimiert | ≈ 485 MB | – |
| Texturen der Wesen (8 Ebenen, 2048 px bei Qualität 2) | 112 MB | keine |
| Belegter Grafikspeicher des Prozesses je Ebene (DXGI 1.4, neu in V13) | 939–1003 MB | – (vorher nicht messbar) |

- **Empfehlung (nicht umgesetzt):** ORM als BC5 + BC4 (Rauheit und Verdeckung / Höhe) oder BC7 würde rund 360 MB sparen. In V13 nicht geändert: Die Texturen sind unverändert, der Bedarf liegt unter 1 GB, und ein neuer Kompressor mit Qualitätsprüfung aller 91 Ebenen ist ein eigener Schritt.

## 5. Offen

- **Buchrücken (5.3):** Der eigene Satz ist ein Raster aus Regalfächern; aus der Nähe sieht man dasselbe Fach mehrfach. Ein Versatz je Fach (Kachelbruch mit Rasterschritt) oder zwei weitere Varianten würden helfen. Nicht in V13.
- **Teppich-Normalen bei Streiflicht:** Der Teppich von Level 0 zeigt bei flachem Blick weiter ein feines Längsmuster (Faserrichtung der Quelle). Gehört zur Quelle, kein Kachelfehler.
