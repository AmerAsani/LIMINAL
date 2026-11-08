# Aufwand je Ebene (V13.3, Abnahmeschranke g)

Je Ebene: Runden, neu gebaute Assets, Bildpfade, gefundene/behobene Fehler, Zeitaufwand. Eine Ebene ohne vollständigen
Eintrag gilt nicht als abgenommen; ein Eintrag dünner als der von Level 0 ist ein Warnzeichen. Zeiten aus den Commit-Zeitpunkten
(Hauptstrang und Agenten-Zweige), gerundet. Stand wird bei jeder Abnahme fortgeschrieben.

Gemeinsame Vorarbeit für alle Ebenen (nicht einer Ebene zugerechnet): Werkzeuge `tools/qa/` (Kontaktbögen, Raummetrik,
Schilder, Ähnlichkeit, Laufweg, Smoke-Test, Kartenbild, Modellprüfung, Spot-Bilder, Prüfbogen), Engine-Erweiterungen
(freie Kamera, Testlampe, Weltlicht, Modellvarianten, Glanz je Material, Pfützen und Wasser, Deckenspiegelung), Wiki-Recherche
aller zehn Ebenen und ihrer Wesen (Wikidot, Notizen je Ebene), `LEVEL0_STANDARD.md`, `STRUKTURTYP.md`, `BIOM_AUDIT.md`.

## Level 1 – abgenommen (01.11.2026)

| Punkt | Stand |
|---|---|
| a) Strukturtyp, Raummetrik | Eintrag in `STRUKTURTYP.md`, BIOM_AUDIT (Sektoren Ouroboros, Garden, Fabled ergänzt). Decken ≥ 2,8 m; Sichtachsen Median 36,2 m, P90 87,0 m (korrigierte Messung, E-035); 15 große Räume; 0 von 44 Räumen verstellt; Ähnlichkeit S 0,823; Raummetrik grün |
| b) Ausstattung | Teil A (Code-Modelle, 22 Varianten, `PROPS_Level1.md`), Teil B (29 Klassen, `PROPS_Level1_B.md`), Sektoren (27 Klassen, 40 Modelle, `PROPS_Level1_Sektoren.md`), aus den Kritikrunden neu `arcade_gothic`, Erscheinungen 19 Modelle (`models_l1_phenomena.py`). Je mindestens 5 Runden mit Kontaktbögen |
| c) Wesen | alle Wesen der Level-1-Seite im Spiel (ENTITY_ABGLEICH). Neu in V13.3: Duller, Arms, Wrangler, Growler, Nguithr'xurh, Ouroboros-Arbeiter, Plague Goblins, Crawler, Spielleiterin (Entity 99, E-038), Garden-Verwandlung, Wesen an der Schlafraumtür, Leuchtstab-Köder (Smiler), Ansprechen (Skin-Stealer). Je KREATUR-Datei mit Wow-Check und mindestens 5 Runden. Ausnahmen nach Auftrag: Menschengruppen, Staphylococcus liminalis (Erreger ohne Körper) |
| d) Kritikrunden | 7 Ebenen-Runden (`KRITIK_Level1.md`), dazu die Runden der Ausstattung und der Wesen |
| e) Laufweg | 836 m durch 18 Räume, 209 Bilder alle 4 m (`docs/screens/level1/wegbilder_r1..r3_*.jpg`, `wegkarte_r*.png`); Fabled und Rohrgang 447 m durch 15 Räume (`wegbilder_r5..r7`); Gänge 236 m (`wegbilder_r4`) |
| f) Prüfungen | Objektvalidierung 0 Fehler; Halt-Test level1 0 ohne Halt; Schilder 6 (Grenze 12); alle Tests grün (gameplay 647/0, audio 106/0, coop --quick 209/0, net 72/0, worldgen 29658/0, supply OK). Smoke-Test 01.11. (`docs/metrics/smoke_level1.json`): GPU 4,34 ms (max 5,68), Spiel 0,74 ms, bis 1,90 Mio. Dreiecke, VRAM 1317 MB, RAM 949 MB (Spitze 1013 MB), 0 Fehler im Protokoll. Die Grafik-CPU von 15,7 ms ist die Wartezeit auf den 60-Hz-Takt: an allen 24 Stationen 15,6–16,5 ms, unabhängig von 0,38–1,90 Mio. Dreiecken; Spiel und Grafik ergeben zusammen 16,7 ms |
| g) Aufwand | Struktur und Messung 27.10. 06:00–06:22, 10:50–11:20, 17:00–17:40 (Hauptstrang). Ausstattung Teil B 27.10. 00:45–11:18, Teil A und Sektoren bis 17:09 (Agenten). Wasser und Pfützen 06:22–06:44 (gilt für alle Ebenen). Wesen (Agenten, Wandzeit laut Berichten): Duller/Arms etwa 3 h, Wrangler etwa 3 h, Growler/Nguithr'xurh etwa 4 h plus Neubau der Spinne 1,2 h, Leuchtstab/Ansprechen etwa 2 h, Erscheinungen/Ouroboros/Goblins 2,5 h (3 Agenten parallel), Träger- und Spät-Beitritt-Korrektur 0,3 h. Zusammenführen und Konflikte 01.11. etwa 1,5 h (Hauptstrang) |

Gefundene und behobene Fehler:
- Engine und Tests: Zonenlimit 16 der Engine (→ 24); Tests mit festen Koordinaten in Aquila (→ freie Stelle gesucht).
- Ausstattung und Karte:
  - Kontaktbögen schwarz bei fernen Kabinen (Streaming → Spieler in die Kabine);
  - Kisten über Pfützen-Spawns; Wesen-Spawns zu nah an Sicherungspunkten;
  - Säulen-Rohre in Öffnungen;
  - Wandteile vor später angelegten Türen.
- Werkzeuge: Messfehler `maplib` (sichtbare Blöcke, E-035); veraltete Ausstattungs-Abzüge in der Raummetrik.
- Wesen:
  - Duller-Haut zu hell;
  - Nguithr'xurh-Modell abgelehnt und neu gebaut (Walzenspinne 8+2);
  - Faceling-Test hing am Zufallsort;
  - Ouroboros-Träger pendelte am Wegefeld (nach jeder ersten Lieferung festgefahren);
  - Leuchtstab-Funde und offenes Mandelwasser beim Spät-Beitritt doppelt gezählt (Prüfsumme wich ab).
- Zusammenführen: Zeilenenden-Konflikt der Testdatei (E-037).

## Level 2 – abgenommen (02.11.2026)

| Punkt | Stand |
|---|---|
| a) Strukturtyp, Raummetrik | Eintrag in `STRUKTURTYP.md` (Labyrinth „Pipe Dreams“): Tunnelnetz im 15-m-Raster als Grundstruktur (Hallen 46 → 21 %, Tunnel 16 → 31 %), Brownout und Macchina-Werkstatt ergänzt (Wiki). Raummetrik grün: 302 Räume, Decke min/Median 2,60/2,70 m, Gänge 2,0 m, Fläche Median 40 m², Sichtachsen Median 17,8 m, P90 40,2 m (Grenzen 15/38 m), 4 große Räume, 0 von 181 Räumen verstellt. Ähnlichkeit S 0,812, Ausreißer 17,9 % (Grenzen 0,80 / 20 %) |
| b) Ausstattung | `PROPS_Level2.md`: 28 Klassen neu in Blender (die Röhren als eine Klasse in drei Fassungen), 2 Rohrklassen geprüft, 41 Modelle; Nachforderung Bauarten aus verschiedenen Epochen (Wiki) mit 8 weiteren Varianten, 49 Modelle. Kontaktbögen vorher, Testbögen, r1–r5, final (`docs/screens/props/level2/`), Prüfbögen und Übersichten `docs/screens/level2/props_*.jpg`. In der Abnahme neu: Automat mit Lacktextur, Notleuchte heller (siehe d) |
| c) Wesen | Alle Wesen der Level-2-Seite im Spiel (ENTITY_ABGLEICH, Abschnitt Level 2): Fenster am Gangende, Smiler (Stromausfall), Deathmoths (klein, an Röhren), Clump, Jerry, Hound-Rudel, Faceling, Spuren der Skin-Stealer, Wretch, Nguithr'xurh, Death Rats an Fässern, Cannibal Cuisine, Wrangler, Photoshop, Growler (Junge), Swindlebird, Reste der Macchina. Ausnahme nach Auftrag: Menschengruppen (Followers of Jerry nur als Spuren, Survivors, B.N.T.G.). Je KREATUR-Datei Wow-Check und mindestens 5 Kritikrunden für den Auftritt in Level 2 (Fenster 5, Motten 5, Ratten 5, Spuren 6, Jerry 8, Automat 6, Swindlebird 5, Photoshop 8, Macchina 5, Rudel 5) |
| d) Kritikrunden | 7 Ebenen-Runden (`KRITIK_Level2.md`): 1–3 Struktur und Ausstattung; 4–7 Abnahme mit allen Wesen in ihren Räumen (neues Werkzeug `tools/qa/creatureshots.py`, Licht der Ebene und Lampe; Bögen `docs/screens/level2/wesen_r4_*` … `wesen_r7_*`). Dazu die Runden der Ausstattung und der Wesen |
| e) Laufweg | `walkway.py level2 --rooms 15 --seed 3`: 208 m durch 28 Räume, 52 Bilder alle 4 m (`docs/screens/level2/wegbilder_r4_*`, `_r5_*`, `_r6_*`, Karten `wegkarte_r4..r6.png`); vorher r1 201 m/25 Räume, r2/r3 208 m/28 Räume |
| f) Prüfungen | Modellprüfung aller 211 Modelle der Ebene (Karte, Randwesen, Spuren, Wesen): 0 Fehler (vorher 74, siehe Fehler). Halt der Ausstattung (`gameplay_test`): level2 1583 Objekte mit Halt-Regel, 0 ohne Halt. Schilder 0 (Grenze 0). Raummetrik und Ähnlichkeit grün. Tests grün: gameplay 901/0, audio 121/0, coop --quick 209/0, net 72/0, worldgen 29658/0, supply OK. Smoke-Test 02.11. (`docs/metrics/smoke_level2.json`, 24 Stationen des Laufwegs): GPU 3,22 ms (max 4,02), Spiel 0,10 ms, bis 2,31 Mio. Dreiecke, VRAM 1295 MB, RAM 1079 MB (Spitze 1307 MB), 0 Fehler im Protokoll. Grafik-CPU 16,57 ms ist die Wartezeit auf den 60-Hz-Takt (wie Level 1) |
| g) Aufwand | Struktur 27.10. 16:00–16:22, Kritikrunde 1 17:42, Tür-Maschinen-Regel 17:47 (Hauptstrang). Ausstattung 27.10. 06:13–11:24 (vorher bis r1) und 01.11. 10:28–12:05 (Merge, Macchina-Werkbank, r2–r5, final, Nachforderung Varianten), laut PROPS_Level2 etwa 13 h Agentenzeit plus 2 h Varianten. Kritikrunden 2–3 01.11. 19:24–20:12. Wesen (Agenten): Fenster, Motten, Ratten, Spuren 01.11. 21:18 – 02.11. 07:57 (etwa 3 h Arbeit, mit Pause über Nacht); Jerry 02.11. 08:28–09:32; Randwesen (Automat, Swindlebird, Photoshop, Macchina) 02.11. 12:17–13:59; Hound-Rudel 15:40–16:57; Growler-Junge 01.11. 01:21–01:29; Zusammenführen 02.11. 12:17 und 18:21. Abnahme (diese Runde): 02.11. 18:22–20:45 (etwa 2,5 h), davon etwa 1 h Neubau Photoshop (Blender-Szenen, Atlas, Collage), 1 h Wesenbilder und Laufwege (4 Runden), 0,5 h Modellprüfung, Messungen und Tests |

**Neu gebaute Assets (Level 2):**
- **Ausstattung** (`tools/blender/models/models_l2.py`, Bögen `docs/screens/props/level2/`):
  - Fass (3 Lackierungen), Kabeltrommel, Schräge an Knicken;
  - Tür nach Level 3, Sperrholztür, Stahltüren creme, grau und weiß, Fenstertür, alte Brettertür;
  - Klappstuhl, Trichterwagen, Plattformwagen, Stehleiter, Werkzeugkasten, Palette (4);
  - Verteilerkasten (4 Bauarten, 1930er bis 1990er), Schaltschrank (3), Pumpe (2), Kessel (2);
  - Rohrbündel A und B, Steigleitung (2), Gleis;
  - Leuchtröhren weiß, orange und tot, Notleuchte, Macchina-Werkbank und -Werkzeugwand.
- **Wesen und Erscheinungen**:
  - `ent_window_end` (Fenster am Gangende), `ent_moth_male` (kleine Motten), `ent_rat_bald` (Ratten an Fässern),
    `ent_jerry` mit `jerry_mural`, `jerry_perch`, `jerry_poster` (2) und `jerry_shrine`;
  - Rudel-Hound `ent_hound_l2`;
  - Spuren `skin_shed` (2), `skin_hung`, `skin_scratches`, `skin_lair`;
  - Essensreste `food_scraps` (2);
  - Automat `cc_body` (3), `cc_flap_top` (3), `cc_flap_bot`, Arm und Hände `cc_humerus`, `cc_forearm`, `cc_hand` (2),
    `cc_ware`;
  - Swindlebird `sb_*` (10 Teile);
  - Photoshop `ps_block` (5), `ps_screen`, `ps_arm_a/b`, `ps_hand` (2), `ps_edge`, `ps_handle`;
  - Macchina `macchina_suit`, `macchina_cradle`, `macchina_tapestry`, `macchina_plate`, `macchina_spear`.
- **In der Abnahme neu oder neu gebaut**:
  - Photoshop-Fotos: 11 Szenen in Blender/Cycles (`tools/textures/ps_photos_render.py`), Atlas `data/textures/ps_photos`
    (`ps_photos_atlas.py`, Generator `ps_photos` in `selection.json`);
  - `ps_block@1..4` als Collage aus 84 Abzügen, `ps_screen` aus dem Atlas;
  - `cc_body`/`cc_flap_top` mit Lacktextur;
  - `violet_lamp` heller, 18 Notleuchten in toten Gruppen;
  - `ent_wrangler`, `_f`, `_fs` Stufe 0 neu;
  - 72 Modelle mit bereinigten Indexlisten (`tools/qa/repair_glb.py`).

**Bilder:**
- Laufweg `docs/screens/level2/wegbilder_r1..r6_*.jpg`, `wegkarte_r1..r6.png`.
- Wesen in der Ebene `docs/screens/level2/wesen_r4..r7_*.jpg` (Einzelbilder `build/qa/creatures/level2/r4..r7/`).
- Ausstattung `docs/screens/level2/props_*.jpg`, `docs/screens/props/level2/`.
- Wesen im Detail `docs/screens/creatures/l2_fenster/`, `l2_motten/`, `l2_ratte/`, `l2_spuren/`, `jerry/`,
  `l2_randwesen/` (Photoshop vorher/nachher `r6_ps_vorher_nachher.jpg`, Fotoatlas `r6_ps_fotos.jpg`, Abnahme
  `r7_ps_bogen.jpg`, `r7_ps_nah.jpg`, Automat `r6_cc_lack_vorher_nachher.jpg`) und `l3_rudel/` (Rudel Level 2, h–j).

Gefundene und behobene Fehler:
- **Struktur und Messung**:
  - Die Messung las eine alte Kartenkopie (→ `game.py` gleicht Karten ab).
  - Rohrbündel über Kopf zählten als Hindernis (→ Metrik ab 1 m Höhe ausgenommen).
  - Engstelle 0,92 m am Trichterwagen (→ Nordwand frei).
  - Der Brownout war schwarz statt gedimmt (→ Röhren 0,55, Grundlicht 0,045, violette Notlichter).
  - Die Raummetrik nutzte einen veralteten Ausstattungs-Abzug (→ erneuert ihn selbst).
  - In engen Tunneln stand eine Tür gegenüber einer Maschine (→ Regel in `furnish_tunnel`).
- **Ausstattung** (PROPS_Level2): Ventilräder, Rohre, Schränke und Türen in fünf Runden überarbeitet; Bauarten aus
  verschiedenen Epochen nachgefordert; Dreiecke der vielfach gesetzten Klassen gesenkt.
- **Photoshop**:
  - Gesicht und Körper waren Vektorgrafik wie ein Comic (→ Neubau aus Fotos).
  - Das Monitorbild saß seit Runde 1 bis 9 cm neben der Scheibe, weil die Drehung des Monitors im Spiel gespiegelt ist
    (→ `apparitions_l2.cpp`).
  - Abstehende Ecken stachen durch die Augenfotos und flimmerten.
  - Splitterdreiecke im Schnitt der untersten Scheibe.
- **Automat**: Das Gehäuse war glatt wie neu. Die Ursache: Bewegliche Modelle ohne UV können keine Textur zeigen
  (→ `_box_uv`).
- **Laufweg**: Bis zu 40 m tote Gruppen am Stück ohne jedes Notlicht (→ 18 Notleuchten, nur Eigenleuchten).
- **Modellprüfung der Ebene**:
  - `jerry_perch` mit 96 entarteten Dreiecken, `food_scraps` mit gedrehten Dreiecken.
  - 70 Wesen-Modelle mit gedrehtem Umlaufsinn oder entarteten Dreiecken nach dem Vereinfachen (→ `repair_glb.py`).
  - Wrangler-Stufe 0 mit 160 000 Dreiecken über der Grenze (→ 118 000).
- **Werkzeug**:
  - Die Lampe braucht über 1 s bis zur vollen Helligkeit; Bilder nach 0,2 s zeigten kein Lampenlicht.
  - Jerry und das Fenster lassen sich nur in geladenen Gegenden erzwingen.
  - Die freie Kamera stand im eigenen Körper.
  - Die Kamera steckte im heranlaufenden Faceling.
  - Alles in `creatureshots.py` behoben.
- **Wesen** (Agenten, Einzelheiten in den Briefings):
  - Fenster: zuerst ein dünner Bilderrahmen, dann ein schwarzer Kasten.
  - Spuren: Keks, Lebkuchenmann, Tarnmuster, Pappe.
  - Jerry: schwarzer Umriss, Plüsch.
  - Rudel: hockte wie eine Katze; ein Hound sprang unsichtbar um die Ecke.
  - Automat: Hände wie Zweige, Namen der Seite im Bild.

## Level 3 – abgenommen (02.11.2026)

| Punkt | Stand |
|---|---|
| a) Strukturtyp, Raummetrik | Eintrag in `STRUKTURTYP.md` (Mischform „Electrical Station“: Ziegelgänge mit Gittern als Grundstruktur, Sonderräume verstreut): drei Ziegel-Gangnetze (Gang/Kammer 60 von 71 → 191 von 207 Räumen), Büro entfernt, Paranoia-Gang 2 m breit und 2,6 m hoch; Fertigungsstraße, Generatorräume, Kesselräume, Sanktuarium, Räume hinter Gittern nach dem Wiki. Raummetrik grün: 207 Räume, Decke min/Median 2,60/2,90 m, Gänge 2,0 m, Fläche Median 25 m², Sichtachsen Median 23,5 m, P90 52,2 m, 8 große Räume, 0 von 69 Räumen verstellt. Ähnlichkeit S 0,798, Ausreißer 20,9 % (grün) |
| b) Ausstattung | `PROPS_Level3.md`: 24 Klassen neu in Blender, 38 Modelle (Runden vorher, Test, r1–r5, final; Bögen `docs/screens/props/level3/`, Übersichten `docs/screens/level3/props_uebersicht_*.jpg`). In der Abnahme neu: Ausstattung der Gangnetze (57 Rohrstränge auf 82 %, 24 Verteiler, 15 Schaltkästen mit Panzerrohr, 5 Engelsbilder; siehe d) |
| c) Wesen | Alle Wesen der Level-3-Seite im Spiel (ENTITY_ABGLEICH, Abschnitt Level 3): Hound-Rudel (lauern einzeln, jagen gemeinsam), Faceling entwickelt und bewaffnet, Wretch mit Waffen und Sprachfetzen, Deathmoth-Schwarm mit Praetorianern, Skin-Stealer im Mantel, Smiler (Stromausfall, Angriff ohne Anlass), großes bewaffnetes Bündel (und das kleine in den Schächten), Death Rats wie Wasserschweine mit Stromfallen, purpurne Wesen in den Wänden, Red Knight, Schattenfigur/Stimmen (Paranoia-Gang), Engel im Sanktuarium (Zuflucht). Ausnahme nach Auftrag: Menschengruppen (M.E.G./Base Gamma, B.N.T.G.). Je KREATUR-Datei Wow-Check und mindestens 5 Kritikrunden für Level 3 (Wretch und Skin-Stealer je 5 und Wow-Check, Purpur 6, Death Rats 6 + Abnahme, Deathmoths 5 + Abnahme, Rudel 5, großes Bündel 5 + Abnahme, Red Knight 5, Faceling 3 + Abnahme; der Smiler hat seine Bildrunden aus V13, sein Verhalten auf Level 3 (ohne Anlass) ist in den Abnahmerunden 3–7 im Bild) |
| d) Kritikrunden | 7 Ebenen-Runden (`KRITIK_Level3.md`): 1–2 Struktur und Ausstattung; 3–7 Abnahme mit allen Wesen in ihren Räumen (`tools/qa/creatureshots.py`, neue Tabelle für Level 3: 13 Läufe, Licht der Ebene und Lampe; Bögen `docs/screens/level3/wesen_r3_*` … `wesen_r7_*`), Orte der Ebene (`orte_r5.jpg`), Vorher/Nachher `abnahme_vn_*.jpg` |
| e) Laufweg | `walkway.py level3 --rooms 15 --seed 3`: 201 m durch 27 Räume, 51 Bilder alle 4 m (`docs/screens/level3/wegbilder_r3_*` … `_r7_*`, Karten `wegkarte_r3..r7.png`); vorher r1/r2 201 m durch 27 Räume |
| f) Prüfungen | Modellprüfung aller 122 Modelle der Ebene (Karte mit Varianten, Wesen in allen Stufen, purpurne Wesen, Leuchtstab): 0 Fehler (vorher 19, siehe Fehler). Halt der Ausstattung (`gameplay_test`): level3 600 Objekte mit Halt-Regel, 0 ohne Halt. Schilder: Notausgangsschilder 0 (Grenze 0), dazu 6 Warnschilder nach dem Wiki (Paranoia-Gang, Abgang nach Level 4). Raummetrik und Ähnlichkeit grün. Tests grün: gameplay 902/0, audio 121/0, coop --quick 209/0, net 72/0, worldgen 29658/0, supply OK. Smoke-Test 02.11. (`docs/metrics/smoke_level3.json`, 24 Stationen des Laufwegs): GPU 5,97 ms (max 15,74 in der Fertigungsstraße: 45 Hallenleuchten mit Schatten, „Licht“ 10–12,6 ms; Gegenprobe mit der Karte vor der Abnahme 15,21 ms), Spiel 0,08 ms, bis 2,15 Mio. Dreiecke, VRAM 1313 MB, RAM 1120 MB (Spitze 1313 MB), 0 Fehler im Protokoll. Grafik-CPU 16,53 ms ist die Wartezeit auf den 60-Hz-Takt (wie Level 1 und 2) |
| g) Aufwand | Struktur 27.10. 16:22–16:26, Kritikrunde 1 17:47 (Hauptstrang); Kritikrunde 2 02.11. 07:05. Ausstattung (Agent, laut PROPS_Level3) 27.10. etwa 6 h und 01.11. etwa 10 h (bis 19:02 mit Merge und Prüfungen). Wesen (Agenten, Commit-Zeiten): Wretch und Skin-Stealer im Mantel 02.11. 07:38–09:11; purpurne Wesen 09:48–12:48 (Pause durch das Nutzungslimit 12:17); Death Rats und Deathmoths 13:10–14:51; Rudel und großes Bündel 15:40–16:57; Smiler ohne Anlass 17:23; Red Knight 18:08; Wow-Check Skin-Stealer/Wretch 18:19; Zusammenführen und Speicherfehler (B-025…B-028, AddressSanitizer) 19:16–19:17. Abnahme (diese Runde): 02.11. 20:35–21:45 (etwa 1,2 h Wandzeit; Blender-Bauten, Wesenbilder, Laufwege und Tests liefen nebeneinander): Haut des Bündels (Texturebene, zwei Blender-Bauten zu je 8 min, eine Nachbesserung), Motten im Gesicht (Fülllicht, Staub, Plätze), Falle und Faceling (Blender, je unter 1 min), Gangnetze (Karte, Raummetrik), 5 Runden Wesenbilder (13 Läufe je volle Runde) und 5 Laufwege, Orte, Modellprüfung und Reparatur, Smoke-Test mit Gegenprobe, alle Tests |

**Neu gebaute Assets (Level 3):**
- **Ausstattung** (`tools/blender/models/models_l3.py`, Bögen `docs/screens/props/level3/`):
  - Ziegelgänge: `pipe_black` (2: DN150/DN100 mit Konsolen, Schellen, Flanschen, Schieber mit Handrad; Bündel), `cable_tray` (4),
    `sign_warning`, `sludge_pool`, `skull`, `canvas_wall`, `canvas_drape_a`, `canvas_drape_b`;
  - Gitter `bars_w2`, `bars_w3`; Fenster `window_brick` (3);
  - Generatorräume: `generator` (2: Dieselaggregat, Umformersatz der 1930er), `switch_cabinet` (3);
  - Fertigungsstraße: `conveyor` (2), `grate_stair`;
  - Kesselraum: `boiler` (Flammrohrkessel);
  - Sanktuarium: `angel_statue`, `column_greek` (2), `greek_torso`, `painting_angel` (2), `painting_angel_scribbled` (2), `ceiling_globe`;
  - `train_bogie`, `elevator_door`.
- **Wesen und Erscheinungen** (`tools/blender/creatures/`, Briefings `KREATUR_*.md`):
  - `ent_clump_l3` (bewaffnet: Knochenspieß, Knochensichel, Rohr) und `ent_clump_l3_bare` (entwaffnet);
  - `ent_hound_l3` (Brandnarben, Ruß; Rudel);
  - `ent_wretch_pipe`, `ent_wretch_board`, `ent_wretch_blade` (selbst gebaute Waffen), eigene Haut des Wretch (Texturebene `wretch`);
  - `ent_skinstealer_coat`, `ent_skinstealer_torn` (Mantel, zerrissen), wahre Gestalt mit neuer Haut (Wow-Check);
  - `ent_rat_l3` (wie ein Wasserschwein), `ent_rat_trap` (Stromfalle);
  - `ent_moth_praetorian` (acht Beine, Giftbeine), Mottenskelett mit viertem Beinpaar;
  - `ent_redknight` (verbeulte Platten, roter Umhang, Zweihänder);
  - `purple_stealer`, `purple_hound` (purpifizierte Wesen in der Wand, mit Botschaft);
  - Smiler ohne Anlass (Animation Lider/Lippen), Faceling entwickelt (`ent_faceling_evo`, V13.2).
- **In der Abnahme neu oder neu gebaut**:
  - Texturebene `buendel` (`EL_BUNDLE`) und Materialien `FM_ENT_CL_HIDE_A/B` (neu belegt), `FM_ENT_CL_DEAD_A/B`, `FM_ENT_CL_CORE`;
    `ent_clump_l3` und `ent_clump_l3_bare` neu gebaut;
  - `ent_rat_trap` neu gebaut (Seil aus drei Adern, zwölf Isolierfetzen);
  - `ent_faceling_evo` neu gebaut (Bart aus 150 Strähnen; `FM_ENT_FC_SKIN`, `FM_ENT_FC_HAIR`);
  - Fülllicht und Flügelstaub bei Motten im Gesicht (`app.cpp`, `renderer.cpp`, `particles.hlsl`), Plätze im Gesicht;
  - Gangnetze: 101 Wandteile (Karte, eigener Zufall 3304);
  - 24 Wesen-Stufen mit bereinigten Indexlisten (`repair_glb.py`).

**Bilder:**
- Laufweg `docs/screens/level3/wegbilder_r1..r7_*.jpg`, `wegkarte_r1..r7.png`.
- Wesen in der Ebene `docs/screens/level3/wesen_r3..r7_*.jpg` (Einzelbilder `build/qa/creatures/level3/r3..r7/`).
- Vorher/Nachher der Abnahme `docs/screens/level3/abnahme_vn_buendel.jpg`, `_motten_gesicht.jpg`, `_falle.jpg`, `_faceling.jpg`.
- Orte (Übergänge, Licht) `docs/screens/level3/orte_r5.jpg`; ältere Übersichten `01_ankunft.jpg` … `16_smiler.jpg`.
- Ausstattung `docs/screens/level3/props_uebersicht_*.jpg`, `docs/screens/props/level3/`.
- Wesen im Detail `docs/screens/creatures/l3_clump_gross/`, `l3_rudel/`, `l3_ratten/`, `l3_motten/`, `l3_purpur/`, `l3_redknight/`,
  `l3_skinstealer/`, `l3_wretch/`, `l3_wowcheck/`, `smiler/`.

Gefundene und behobene Fehler:
- **Struktur und Ausstattung** (Runden 1–2): Gangnetze über lange Strecken unlesbar (Röhren nur alle 5 m, 25 % tot → alle 4 m,
  20 % tot); Aufzugsöffnung 1 m für ein 1,4-m-Tor (→ 2 m, Tor 1,35); Ausstattung nach PROPS_Level3 (Tücher mit Pixelflecken,
  Fenster ohne Schutzgitter, Engel mit Flecken, Torso).
- **Gangnetze kahl** (Runde 3): Netzstücke sind 5 m lang, das Rohrmodell 5,8 m und die Ausstattung nur alle 6–7 Kacheln
  (→ Rohrstrang auf 82 %, Wandteile nach Abstand längs der Wand).
- **Großes Bündel puppenhaft glatt** (→ eigene Haut, Runden 3–5; ein Wurmmuster aus dem Gratrauschen in Runde 4 behoben).
- **Motten im Gesicht nur ein dunkles Bild**: die Lampe in der Hand leuchtet von den Motten weg (→ Fülllicht, Staub); danach
  füllten die Flügel das ganze Bild (→ Plätze weiter vor, Flügel halb angelegt); der Staub las sich als Schnee (→ kleiner).
- **Fallendraht kaum sichtbar** (1–2 Pixel; → Seil und Isolierfetzen).
- **Faceling-Bart als glänzender Klotz** (→ Strähnen, eigene Materialien).
- **Modellprüfung**: 19 von 122 Modellen der Ebene mit gedrehten oder entarteten Dreiecken (→ `repair_glb.py`, 0 Fehler).
- **Werkzeug**: kleines Bündel und Smiler kamen am Start nicht (kein Lager in der Nähe; Bereich 0 ohne Stromausfall) –
  `creatureshots.py` geht erst hin bzw. erzwingt die Welle im Hauptgang; die Nahaufnahme des Bündels stand im Rohr.
- **Wesen** (Agenten, Einzelheiten in den Briefings):
  - Speicherfehler beim Zusammenführen (B-025 bis B-028, AddressSanitizer);
  - Rudel: Narben spärlich, das Rauschen um 0 verteilt statt um 0,5 (Rand verschob sich nur gleichmäßig);
  - großes Bündel: der lange Arm faltete sich zum Spinnenbein, die Laufhände standen senkrecht in die Luft;
  - Red Knight im Dunkeln schwarz wie Stoff; Praetorianer mit einer grauen Kugel wie eine Clownsnase;
  - Mottenschwarm flog auf, griff aber nie an (Sitzzustand ignorierte den aufgescheuchten Schwarm);
  - purpurne Wesen: Gummipuppe mit Lichtbahnen wie ein Stromschlag, die Wand lavendelgrau statt violett.

## Level 0 – abgenommen (03.11.2026)

| Punkt | Stand |
|---|---|
| a) Strukturtyp, Raummetrik | Eintrag in `STRUKTURTYP.md` (Labyrinth „The Lobby“): Vorzimmer als Grundstruktur (65 %), dazu nur Raumtypen der Seite (Bögen, Säulenfeld, Löcher, Dunkelzone, rote Räume, Manila-Raum) und die Naht nach Level 1; Schilder 0. Strukturrunde 27.10.: Decken ≥ 2,7 m (Dunkelzone), breitere Durchbrüche (Sichtachsen Median 22,0 → 25,5 m), keine hervorgehobenen Raumnamen. In der Abnahme: Sicherungspunkte ohne Leuchte, „Flur“ statt „Der lange Flur“, Bogen-Durchgänge mit Teppich (siehe Fehler). Raummetrik grün: 315 Räume, Decke min/Median 2,70/2,90 m, Gänge 2,0 m, Fläche Median 30 m², Sichtachsen Median 25,5 m, P90 46,5 m, 4 große Räume, 0 Stützen unter 1 m zur Wand, 0 verstellt. Ähnlichkeit S 0,891, Ausreißer 14,2 % (120 Fenster; Grenzen in `metric_limits.json`) |
| b) Ausstattung | `PROPS_Level0.md`: 7 Klassen neu in Blender (`models_l0.py`: Rasterleuchte in 3 Fassungen, tote Rasterleuchte 2, rote Rasterleuchte 2, Luftauslass 3, Lüftungsgitter 2, Steckdose 3, Bogen `arch3` mit `arch2`/`arch4`), Tapete neu als Kachel aus 53-cm-Bahnen (`tools/textures/wallpaper_l0.py`, 2 Runden), 11 Fundstücke und Interaktionsobjekte neu (`models_items.py`). 5 Kritikrunden und Schlussrunde mit Kontaktbögen (vorher, r1–r5, final; `docs/screens/props/level0/`, `docs/screens/level0/props_uebersicht_*.jpg`, `docs/screens/props/items/`). In der Abnahme: Leuchtstab und Beil im Prüfstand (`itemsheet.py`), Beilkopf mit Metallanteil 0,6, die 7 Sicherheitsleuchten entfallen. Karte jetzt 1050 Objekte (vorher 1057) |
| c) Wesen | Die Seite nennt keine bestätigten Wesen (ENTITY_ABGLEICH, Abschnitt Level 0). Im Spiel: **Gestalt** (dunkle Figur am Gangende, ändert sich nie, solange man hinsieht, greift nie an; `KREATUR_Gestalt.md`, 5 Kritikrunden), **Halluzinationen** (Gestalt im Dunkeln nach einem Ausfall, Stromausfall des Raums, Phantomschritte, Flüstern, Kratzen, ferne Stimmen). Ausnahme nach Auftrag: andere Wanderer (Menschen) nur über ihre Notizen. In der Abnahme in vier Runden (2–5) im Spiel geprüft, mit Licht der Ebene und Lampe |
| d) Kritikrunden | 5 Ebenen-Runden (`KRITIK_Level0.md`): 1 Struktur (27.10.), 2–5 Abnahme mit Laufweg, allen Erscheinungen und Fundstücken. Werkzeug `tools/qa/creatureshots.py` um Level 0 erweitert (Gestalt, Halluzinationen, Fundstücke in der Hand, Notizen am Fundort, 9 Zonen-Ansichten); Bögen `docs/screens/level0/wesen_r2_*` … `wesen_r5_*`. Dazu die Runden der Ausstattung (r1–r5, final), der Tapete (2) und der Gestalt (5) |
| e) Laufweg | `walkway.py level0 --rooms 15 --seed 3`: 205 m durch 24 Räume, 52 Bilder alle 4 m (`docs/screens/level0/wegbilder_r2_*` … `_r5_*`, Karten `wegkarte_r2..r5.png`); vorher 202 m/25 Räume (`wegbilder_vorher_*`), Runde 1 212 m/25 Räume (mit `--rooms 25`) |
| f) Prüfungen | Modellprüfung aller 33 Modelle der Ebene (7 Kartenklassen in 16 Fassungen, Gestalt l0–l2, 11 Fundstücke, Lampe und Hand im Ego-Bild): 0 Fehler (vor der Abnahme 34 mit der Sicherheitsleuchte, ebenfalls 0). Wasserfläche, Pfützen, Leuchtstab und Beil sind Netze aus Code. Halt der Ausstattung (`gameplay_test`): level0 1050 Objekte mit Halt-Regel, 0 ohne Halt. Schilder 0 (Grenze 0), Sicherheitsleuchten mit Piktogramm 0 (neu gezählt, vorher 7). Raummetrik und Ähnlichkeit grün. Tests grün: gameplay 981/0, audio 121/0, coop --quick 209/0, net 72/0, worldgen 29658/0, supply OK (ein Lauf von coop meldete im Lauf mit simuliertem schlechtem Netz 6,4 m Ortsabweichung nach dem Anhalten, die Wiederholung 209/0; dieselbe Abweichung steht schon in `PROPS_Level0.md`, Netzcode). Smoke-Test 03.11. (`docs/metrics/smoke_level0.json`, 24 Stationen des Laufwegs): GPU 3,14 ms (max 3,80; am 27.10. 4,64/8,39), Spiel 0,06 ms, bis 0,53 Mio. Dreiecke, VRAM 982 MB, RAM 1134 MB (Spitze 1490 MB), 0 Fehler im Protokoll. Grafik-CPU 16,61 ms ist die Wartezeit auf den 60-Hz-Takt (wie Level 1 und 2). Klang: 450 s Spielzeit ohne „Klang fehlt“ |
| g) Aufwand | Gestalt (V13, mit Clump, Motte und Nest) 26.10. 07:52–11:01. `LEVEL0_STANDARD.md` 27.10. bis 06:07 (gemeinsame Vorarbeit). Ausstattung 27.10. 10:40–11:30 und 01.11. 13:00–20:00 (laut `PROPS_Level0.md` etwa 7,5 h: Bestand, Builder, Zusammenführung, Fundstücke in Blender, Prüfstand, Tapete, Runden). Struktur 27.10. bis 16:27, Kritikrunde 1 17:51 (Hauptstrang). Abnahme (Runden 2–5) 03.11. 08:10–09:25 (etwa 1,3 h): etwa 0,4 h Werkzeug (Level 0 in `creatureshots.py`, Leuchtstab und Beil in `itemsheet.py`, Leuchten in `signs.py`), 0,6 h Bilder, Prüfstände und Fehlersuche (Ausfall, Spiegelung, Bogen, Beil), 0,3 h Prüfungen, Messungen, Tests und Dokumente |

**Neu gebaute Assets (Level 0):**
- **Ausstattung** (`tools/blender/models/models_l0.py`, Bögen `docs/screens/props/level0/`):
  - Rasterleuchte `troffer` (3: Röhrenbänder, sterbende Röhre, Wasserfleck), `troffer_off` (2, eine mit gesprungener
    Scheibe), `troffer_red` (2);
  - Luftauslass `ceiling_diffuser` (3, Staubhof), Lüftungsgitter `vent_grille` (2), Steckdose `outlet` (3,
    Griffschmutz);
  - Bogen `arch3` mit Laibung und Kämpferprofil (`arch2`/`arch4` mitgebaut).
- **Tapete** `wallpaper_threshold`: Kachel 2,12 m aus vier Bahnen (`tools/textures/wallpaper_l0.py`, CC0-Grundlage).
- **Fundstücke** (`models_items.py`): `item_energy`, `item_almond`, `item_battery`, `item_note`, `item_fuse`,
  `item_fusebox`, `item_fusebox_empty`, `item_vending`, `item_exit_door`, `item_elev_frame`, `item_elev_door`.
- **Gestalt** `ent_gestalt_l0..l2` (V13, `tools/blender/creatures/gestalt.py`, Material `ent_silhouette`).
- **In der Abnahme:**
  - keine neuen Modelle;
  - geändert: Beil (Metallanteil des Kopfs, Schwung und Griff im Ego-Bild), Tür-Art `room_a`/`room_b` für die
    Bogen-Durchgänge, Leuchtflächen im Stromausfall und die Ersatzspiegelung der Decke (Shader).

**Bilder:**
- Laufweg `docs/screens/level0/wegbilder_vorher_*.jpg`, `wegbilder_r1..r5_*.jpg`, `wegkarte_*.png`.
- Erscheinungen, Fundstücke in der Hand, Zonen und Notizen `docs/screens/level0/wesen_r2..r5_*.jpg` (Einzelbilder
  `build/qa/creatures/level0/r2..r5/`).
- Fundstücke am Boden `docs/screens/props/items/l0_r2/`, `l0_r3/`, `l0_r5/` (Leuchtstab, Beil, Notiz). Beil-Schwung
  vorher und nachher: `docs/screens/props/items/l0_beil/swing_vorher.jpg`, `swing_nachher2.jpg`, `swing_griff.jpg`.
- Ausstattung `docs/screens/props/level0/` (vorher, r1–r5, final, Tapete, Bögen im Spiel),
  `docs/screens/level0/props_uebersicht_*.jpg`.
- Feste Ansichten von V12 `docs/screens/level0/01…15`, Gestalt `docs/screens/creatures/gestalt/`.

Gefundene und behobene Fehler:
- **Struktur** (27.10.):
  - Decken unter 2,7 m in der Dunkelzone.
  - Sichtachsen Median 22,0 m (→ breitere Durchbrüche, 25,5 m).
  - Hervorgehobene Raumnamen „Der lange Flur“ und „Vergilbtes Zimmer“.
- **Ausstattung** (PROPS_Level0):
  - Die Tapete wirkte auf 3–4 m glatt wie gestrichen (→ Bahnen); die erste Fassung war zu stark (Holzpaneele).
  - Der Griffschmutz war heller als die feuchte Wand.
  - Der Bogen stand im Studio mit einem Spalt über dem Sturz.
  - Teile von Notausgang und Aufzug fehlten (zu wenige feste Materialien, 160 → 224).
  - Die Schrift der Batterie war gespiegelt.
- **Abnahme**:
  - 7 grüne Sicherheitsleuchten mit Notausgang-Piktogramm an der Decke: das Notausgangsschild, das der Auftrag
    bemängelt. `signs.py` zählte sie nicht (→ entfallen; werden jetzt ausgewiesen).
  - Der Sicherungspunkt hieß noch „Der lange Flur“ (→ „Flur“).
  - In den Bogen-Durchgängen lag eine helle Türschwelle statt Teppich (→ Tür-Art `room_a`/`room_b`, `generator.cpp`).
  - Im Stromausfall blieben die Leuchtflächen weiß (→ erlöschen quadratisch, `gbuffer.hlsli`).
  - Helle Lichtschlieren auf dem Wasser der Dunkelzone: Die Ersatzspiegelung ging bis 45 m durch Wände (→ Strahl
    Zelle für Zelle, `ssr.hlsl`).
  - Beil: Beim Ausholen füllte eine riesige Hand die Bildecke; beim Hieb war nur der Nacken des Kopfs zu sehen; der
    Kopf war im Lampenlicht schwarz; der Stiel stand neben der offenen Hand (→ `body.cpp`, `world_renderer.cpp`).
- **Werkzeug**:
  - Der Rand der Gestalt war gesprenkelt, weil das Bild direkt nach dem Versetzen entstand (TAA, → 40 Bilder warten).
  - Die Gestalt der Halluzination steht regelgerecht hinter dem Spieler und wurde nicht aufgenommen (→ `face gestalt`).

## Level 5 – abgenommen (03.11.2026)

| Punkt | Stand |
|---|---|
| a) Strukturtyp, Raummetrik | Eintrag in `STRUKTURTYP.md` (Eigener Ort „Terror Hotel“): Main Hall mit langen Fluren und Zimmern, Beverly Room, Boiler Room (Kesselgang, Kesselräume), Aufzugshalle, Gang zu 5.3, Metro-Halt; begründete Ausnahmen für Kesselgänge (2 m, Wiki: beengt) und Aufzugskabinen (2,8 m). Raummetrik grün: 75 Räume, Decke min/Median 3,00/3,00 m, Gänge min/Median 3,0/3,5 m, Fläche Median 36 m², Sichtachsen Median 21,0 m, P90 98,0 m (die endlosen Hauptflure), 6 große Räume, 0 Stützen unter 1 m, 0 von 7 verstellt. Ähnlichkeit S 0,879, Ausreißer 13,1 % (61 Fenster). In der Abnahme: Deckenlicht des Büros auf ein Fünftel (`level5.py`, am Ende, ohne Zufall; sonst gleiche Karte, geprüft) |
| b) Ausstattung | `PROPS_Level5.md`: 21 Klassen neu in Blender (`models_l5.py`; Türen 6 Bauarten × 4 Fassungen mit 24 Nummern ohne Muster, Leuchter 3, Bildnisse 3, Bücherregal 4, Kessel mit Manometer, Wasserstandsglas, Dampfdom), 58 Modelle; Runden vorher, r1–r5, final (`docs/screens/props/level5/`, Übersichten `docs/screens/level5/props_uebersicht_*.jpg`). Im Laufweg geprüft (Runden 2 und 5): Damast, Täfelung, Kerzenleuchter alle 4 m, Stuck, Sofas, Sessel, Regale, Bildnisse, Kronleuchter, Mahjong-Tisch, Kessel und Öfen; kein Schmutz in den Hotelbereichen (die hellen Stellen der Sessel sind abgesessener Samt, `PROPS_Level5.md`) |
| c) Wesen | Alle Wesen der Level-5-Seite im Spiel (ENTITY_ABGLEICH, Abschnitt Level 5): Deathmoths (Weibchen im Stock, Männchen an den Birnen der Kesselgänge), Nest an Türen dunkler Zimmer, Hound, Skin-Stealer (wahre Gestalt), Death Rats, Growler (ausgewachsen), Nguithr'xurh, Wrangler (Kesselraum), Samantha (Rolle 19, Fleischhandel), Kolonien (Entity 89), Woodlins, Watchers, Gastgeber (Entity 18: Stimme, Bildnisse, Büro, Hand, Handel). Ausnahme nach Auftrag: Menschengruppen (Housekeeping, Homely Hotel, The Originals, Lost Hall Society). Je KREATUR-Datei Wow-Check und mindestens 5 Kritikrunden; in der Abnahme 4 weitere Runden für Samantha, Woodlins, Watchers und Gastgeber (Abschnitt „Abnahme Level 5“ in den Briefings) |
| d) Kritikrunden | 5 Ebenen-Runden (`KRITIK_Level5.md`): 1 Struktur (27.10.), 2–5 Abnahme mit Laufweg und allen 16 Wesen-Läufen in ihren Räumen (neu: Tabelle für Level 5 in `tools/qa/creatureshots.py` – Samantha im Gang, ohne und mit Fleisch; Kolonie, Woodlins, Watcher, Gastgeber mit Bildnis, Büro und Ablauf; Motten-Männchen, Nest mit Weibchen, Hound, Skin-Stealer, Death Rats, Growler, Nguithr'xurh, Wrangler; Licht der Ebene und Lampe, mit Anzeige für Untertitel). Bögen `docs/screens/level5/wesen_r2_*` … `wesen_r5_*`. Dazu die Runden der Ausstattung (r1–r5, final) und der Wesen (je 5 in den Briefings) |
| e) Laufweg | `walkway.py level5 --rooms 15 --seed 3`: 687 m durch 15 Räume, 172 Bilder alle 4 m (`docs/screens/level5/wegbilder_r2_*`, `_r5_*`, Karten `wegkarte_r2.png`, `wegkarte_r5.png`). Die Zimmer gehen einzeln von sehr langen Fluren ab – darum ist der Weg für 15 Räume dreimal so lang wie verlangt; E-036 greift nicht (80 Räume, alle erreichbar). Runde 1: 1 002 m durch 18 Räume, 251 Bilder |
| f) Prüfungen | Modellprüfung aller 125 Modelle der Ebene (Karte mit Varianten, Weltobjekte des Hotels, Fleisch, Büro, Wesen in allen Stufen: Samantha, Motte und Männchen, Nest, Hound, Skin-Stealer, Ratte, Growler, Nguithr'xurh mit Netzen, Wrangler): 0 Fehler (vorher 5: `ent_nest_l1/l2` und die neu gebauten `ent_samantha_l0..l2` mit gedrehtem Umlaufsinn, → `repair_glb.py`). Halt der Ausstattung (`gameplay_test`): level5 2256 Objekte mit Halt-Regel, 0 ohne Halt. Schilder: Notausgangsschilder 0 (Grenze 0), Notausgangstüren 0, falsch platziert 0. Raummetrik und Ähnlichkeit grün. Tests grün: gameplay 981/0, audio 121/0, coop --quick 209/0, net 72/0, worldgen 29658/0, supply OK. Smoke-Test 03.11. (`docs/metrics/smoke_level5.json`, 24 Stationen des Laufwegs): GPU 4,44 ms (max 5,26), Spiel 0,17 ms, bis 1,46 Mio. Dreiecke, VRAM 1023 MB, RAM 1124 MB (Spitze 1466 MB), 0 Fehler im Protokoll. Grafik-CPU 16,46 ms ist die Wartezeit auf den 60-Hz-Takt (wie Level 1 und 2) |
| g) Aufwand | Struktur 27.10. bis 17:51 (Kritikrunde 1 mit Level 0, Hauptstrang). Ausstattung 27.10. etwa 1,5 h und 01.11. etwa 8 h (laut `PROPS_Level5.md`; Commits 01.11. 19:40–20:26). Wesen (Agent, Commit-Zeiten): Samantha und die Wesen des Hotels ohne Rolle 02.11. bis 20:37, Kritikrunden 1–5, Briefings, Männchen der Deathmoths bis 20:55; Zusammenführen 03.11. 08:10. Growler, Nguithr'xurh, Wrangler, Skin-Stealer, Death Rats aus ihren eigenen Läufen (V13.2/V13.3, Briefings). Abnahme (Runden 2–5) 03.11. 09:16–10:10 (Uhr des Rechners; Runde 2 09:39, Runde 3 09:45, Runde 4 09:53): etwa ein Viertel Werkzeug (Level-5-Tabelle in `creatureshots.py`, Nest erzwingen, Prüfliste der Modelle), die Hälfte Neubau und Bilder (Ärmel, Woodlin-Kopf, Watcher-Kopf, Samanthas Maul in vier Anläufen, Bildnis und Augen), der Rest Prüfungen, Messungen, Tests und Dokumente |

**Neu gebaute Assets (Level 5):**
- **Ausstattung** (`tools/blender/models/models_l5.py`, Bögen `docs/screens/props/level5/`):
  - Sessel, Sofa (Chesterfield), Kleiderschrank, Bücherregal (4), Teetisch, Doppelbett;
  - Zimmertüren `hotel_door_a–f` (6 × 4 Fassungen mit Messingnummern), Silberschild „The Beverly Room“;
  - Täfelung, Stuckgesims, Kerzenwandleuchter (3), Bildnisse (3), helles Fenster nach Level 188;
  - Kronleuchter (2), grüne Hängelaterne (Sicherungspunkte), Aufzug mit Kupfergitter;
  - Mahjong-Tisch, Grammophon; Flammrohrkessel, Ziegelofen, Metro-Wagen.
- **Wesen und Erscheinungen** (Teil 4):
  - Samantha `ent_samantha_l0..l2` (Skelett `sa`, 37 Gelenke);
  - Kolonie `ant_ball` (4); Woodlin `wl_body`, `wl_jaw`, `wl_leg_a`, `wl_leg_b`;
  - Watcher `wa_head`, `wa_face`, `wa_eye`, `wa_hand` (2), `wa_body`, `wa_arm_a`, `wa_arm_b`, `wa_claw`;
  - Gastgeber `e18_arm_a/b/c`, `e18_hand`, `e18_eye`, Büro `e18_desk`, `e18_wingchair`, `e18_portrait`;
  - Fleisch `meat_steak`, `meat_tin`.
- **In der Abnahme neu oder neu gebaut**:
  - `e18_arm_a/b/c` als Stoffärmel (Falten, Naht, Nadelstreifen auf der Fläche, Hornknöpfe, Manschette; 0,7 m, im Zickzack);
  - `e18_eye` (12,5 mm, wunder Rand, nasses Weiß), `e18_portrait` (gemalter Kopf, nur das Gesicht abgekratzt, Nagelkratzer);
  - `wl_body` (gespaltener Kopf, Fleisch, Gewebefäden, Saft bis in den Bauch);
  - `wa_head` (Lidwülste, Unterbiss, 1,3-fach breiter, offener Mund), `wa_face` (Rachen, 18 Zähne, Haar über Scheitel, Haarvorhang);
  - `ent_samantha_l0..l2` (Zähne in der Mundhöhle, Schneidezähne vorn, kürzere Fänge); Material `FM_ENT_SM_FURW` und `FM_ENT_SM_GUM`;
  - `ent_nest_l1/l2` mit bereinigten Indexlisten (`repair_glb.py`).

**Bilder:**
- Laufweg `docs/screens/level5/wegbilder_r1_*`, `_r2_*`, `_r5_*`, `wegkarte_r2.png`, `wegkarte_r5.png`.
- Wesen in der Ebene `docs/screens/level5/wesen_r2_*` … `wesen_r5_*` (Einzelbilder `build/qa/creatures/level5/r2..r5/`).
- Ausstattung `docs/screens/level5/props_*.jpg`, `docs/screens/props/level5/`; Zonen `docs/screens/level5/01_*` … `19_*`.
- Wesen im Detail `docs/screens/creatures/samantha/`, `kolonie/`, `woodlins/`, `watchers/`, `gastgeber/`, `moth/` (Männchen).

Gefundene und behobene Fehler:
- **Samantha**:
  - Weiß im Kerzenlicht grau-braun wie eine Maus (die Fellebene dunkelt etwa um die Hälfte → 1,85).
  - Offen zeigte das Maul nach unten (→ Kopf hebt sich mit dem Kiefer).
  - Die Zahnreihen lagen 6 cm lang außen an den Wangen und lasen sich von vorn als zwei bleiche Blätter (→ in die Mundhöhle,
    Schneidezähne vorn).
- **Woodlins**: aus der Ferne ein heiles Holzpüppchen (→ gespaltener Kopf mit Fleisch, Spalt bis in den Bauch).
- **Watcher**:
  - Aus dem Gang ein schmaler Streifen an der Laibung (→ Kopf weiter vor, Gesicht dreht bis 72°).
  - Danach zu schmal, wie eine Flöte; kahl wie eine Erdnuss (→ breiter, Mund offen, nasses Haar).
- **Gastgeber**:
  - Der Arm ein gerader Stab, der auf den Spieler zeigt (→ Zickzack über der Lehne, Stoffärmel).
  - Der gefaltete Stoff glänzte grau wie ein Schlauch (Rauheit 0,45 → 1,0).
  - Bildnis-Augen aus mehr als 3 m unsichtbar (9 → 12,5 mm, nasses Weiß, wunder Rand).
  - Büro hell wie ein Gang (Deckenlicht → ein Fünftel).
  - Das abgekratzte Gesicht im Büro-Bildnis las sich als Vase (→ gemalter Kopf, nur das Gesicht abgekratzt).
- **Modellprüfung der Ebene**: `ent_nest_l1/l2` (116 und 182 Dreiecke) und `ent_samantha_l0..l2` mit gedrehtem Umlaufsinn nach
  dem Vereinfachen (→ `repair_glb.py`).
- **Werkzeug**:
  - Das Nest kam mit ruhiggestellter Regie nicht (`presence nest` ist in Level 5 die Männchen-Art → `hiveat` an einer Tür).
  - Der Hound erschien hinter dem Scherengitter der Aufzugskabine (→ Start im Hauptgang).
  - Mit Lampe griff der Hound sofort an (→ Bild aus 6 m vor dem Angriff).
  - Die Ratte lief vor der Nahaufnahme fort (→ Lampe zuerst).

**Restpunkte** (begründet, Einzelheiten in `KRITIK_Level5.md` Runde 5 und den Briefings):
- Samanthas Weiß ist im Kerzenlicht hellgrau, erst mit Lampe weiß.
- Die Hand des Gastgebers wirkt nah wie eine Gliederpuppe.
- Der Watcher ist erst ab etwa 4 m als Gesicht zu lesen (aus der Ferne ein heller Fleck an der Laibung).
- Der Wrangler zeigt in der Vorführung ohne Ausbruch nur den glatten, blassen Rücken (Ausbruch in `KREATUR_Wrangler.md`).

## Level 5.3 – abgenommen (03.11.2026)

| Punkt | Stand |
|---|---|
| a) Strukturtyp, Raummetrik | Eintrag in `STRUKTURTYP.md` (Eigener Ort „Promethei Bibliotheca“): Eingangsgang aus Level 5, große Halle mit drei Geschossen Regalen, Gänge und alte Gänge (+6 m), Obergeschoss, Treppe, Leere hinter dem Gitter, Ausgang zurück nach Level 5. Raummetrik grün: 207 Räume, Decke min/Median 3,00/7,40 m, Gänge min/Median 3,0/3,0 m, Fläche Median 310 m², Sichtachsen Median 28,2 m, P90 63,8 m, 3 große Räume, 0 Stützen unter 1 m, 0 von 173 verstellt. Ähnlichkeit S 0,812, Ausreißer 3,4 % (59 Fenster). In der Abnahme: unsichtbarer Block des Lesepults 1,25 → 1,15 m (`level5_3.py`, am Ende; Karte neu erzeugt, Vergleich: nur diese Zahl) |
| b) Ausstattung | `PROPS_Level5_3.md`: alle 16 Klassen neu in Blender (`models_l53.py`; Regale der Gänge 2 × 3 Varianten, Regalwand 3 Geschosse mit Galerien, Kandelaber, eiserner Kronleuchter, Tuffsäule, alte Tür, Inschrift, Renaissance-Gemälde 2 × 2, Wappen an der Decke, abgelöste Tapete, herabgefallene Bücher 3), Runden vorher, r1–r5, final (`docs/screens/props/level5_3/`, Übersichten `docs/screens/level5_3/props_uebersicht_*.jpg`). Im Laufweg geprüft (Runden 2 und 5): Damast und Täfelung im Eingangsgang, Regale in drei Geschossen, Tuffsäulen, Kandelaber, Holztreppe, Wappen, Gemälde mit Inschrift, Kandelaber alle 6 m in den Gängen, Marmor an den Kreuzungen; neu in der Abnahme die Bereiche abseits des Wegs (Leere mit Gitter, Ausgang, alte Gänge, Treppe, Obergeschoss, Kronleuchter) |
| c) Wesen | Alle Wesen der Level-5.3-Seite im Spiel (ENTITY_ABGLEICH, Abschnitt Level 5.3): der Hausherr (Entity 18, Rolle 20: Besuch durch den alten Gang, Szene mit Notizbuch, Ansprache, Stille, Abgang; Störung mit Wahrnehmen, Ankündigung, Griff, nie tödlich), Library Mind (fallende Bücher, Bücherhaufen in der Halle, Regale knarren, Stille, Zorn nach dem Besuch), Wissen der Bibliothek über die Ambassadors (Entity 141: Folio mit roter Tafel am Lesepult, Diktiergerät) und Macchina (Entity 189: Katalogschrank mit Karteikarten) – laut Seite nicht anwesend. Je Briefing Wow-Check und Kritikrunden; in der Abnahme 4 weitere Runden für den Hausherrn (Abschnitt „Abnahme Level 5.3“ in `KREATUR_Gentleman.md`) |
| d) Kritikrunden | 5 Ebenen-Runden (`KRITIK_Level5_3.md`): 1 Struktur (01.11.), 2–5 Abnahme mit Laufweg und den Wesen-Läufen in ihren Räumen (neu: Tabelle für Level 5.3 in `tools/qa/creatureshots.py` – Hausherr fern/mittel/nah mit Licht der Ebene und Lampe, Haltungen der Szene, Wahrnehmen von hinten, Ankündigung, Griff aus der Spielersicht, der Besuch und die Störung, wie sie im Spiel ablaufen, mit Untertiteln; Folio und Karteikarten mit und ohne Lampe, Lesen; ab Runde 4 Bereiche und Übergänge). Bögen `docs/screens/level5_3/wesen_r2_*` … `wesen_r5_*`. Dazu die Runden der Ausstattung (vorher, r1–r5, final) und des Hausherrn (Runden 0–5 im Briefing) |
| e) Laufweg | `walkway.py level5_3 --rooms 15 --seed 3`: 202 m durch 20 Räume, 51 Bilder alle 4 m (`docs/screens/level5_3/wegbilder_r2_*`, `_r5_*`, Karten `wegkarte_r2.png`, `wegkarte_r5.png`); 15 Räume sind schon nach etwa 120 m erreicht, der Weg geht bis 200 m weiter. E-036 greift nicht (173 Räume, alle erreichbar). Runde 1: 202 m durch 20 Räume, 51 Bilder |
| f) Prüfungen | Modellprüfung aller 42 Modelle der Ebene (28 Klassen der Karte mit Varianten, Weltobjekte Lesepult, Katalogschrank, Zettel, Karteikarten, Notizbuch, Bücherhaufen, Hausherr in allen Stufen): 0 Fehler (vorher 3: `gm_notebook` mit 28,8 % entarteten Dreiecken, `ent_gentleman_l1/l2` mit gedrehtem Umlaufsinn nach dem Vereinfachen → `repair_glb.py`). Halt der Ausstattung (`gameplay_test`): level5_3 1482 Objekte mit Halt-Regel, 0 ohne Halt. Schilder: Notausgangsschilder 0 (Grenze 0), Notausgangstüren 0, falsch platziert 0. Raummetrik und Ähnlichkeit grün. Tests grün: gameplay 983/0 (neu: Funde vom Platz des Lesers erreichbar), audio 121/0, coop --quick 209/0, net 72/0, worldgen 29658/0, supply OK. Smoke-Test 03.11. (`docs/metrics/smoke_level5_3.json`, 24 Stationen des Laufwegs): GPU 6,34 ms (max 10,73), Spiel 0,06 ms, bis 2,65 Mio. Dreiecke, VRAM 1119 MB, RAM 1207 MB (Spitze 1439 MB), 0 Fehler im Protokoll. Grafik-CPU 16,61 ms ist die Wartezeit auf den 60-Hz-Takt (wie Level 1, 2 und 5) |
| g) Aufwand | Struktur und Kritikrunde 1: 01.11. bis 01:21 (Hauptstrang, mit 188, 903, 37, 36). Ausstattung 27.10. etwa 2 h und 01.11. etwa 10 h für 5.3 und 188 zusammen (laut `PROPS_Level5_3.md`; Commits 01.11. 19:36 und 20:45). Wesen (Agent, Commit-Zeiten): Hausherr und Wissen der Bibliothek 02.11. bis 22:45 (Grundbau 22:03, Kritikrunden 0–5, Briefings); Zusammenführen 03.11. 08:10. Abnahme (Runden 2–5) 03.11. 10:10–11:05 (Uhr des Rechners; Commits Runde 2 10:31, Runde 3 10:39, Runde 4 10:46): etwa ein Fünftel Werkzeug (Level-5.3-Tabelle in `creatureshots.py` mit Ablauf und Störung, Bereiche, Prüfliste der Modelle), gut die Hälfte Neubau und Bilder (Nadelstreifen im Shader, Bemalung des Gesichts mit neuem Kanal in `build.py`, Revers in drei Anläufen, Weste in zwei, Nägel in drei, drei Neubauten des Hausherrn in Blender), der Rest der Spielfehler am Lesepult mit Test, Prüfungen, Messungen und Dokumente |

**Neu gebaute Assets (Level 5.3):**
- **Ausstattung** (`tools/blender/models/models_l53.py`, Bögen `docs/screens/props/level5_3/`):
  - Regale der Gänge `bookcase_aisle`, `bookcase_aisle_b` (je 3), Stirnseite `bookcase_end`;
  - Regalwand `library_wall` (3), `library_wall_lamp`, `library_wall_upper` mit Galerien, Geländer und Handlauf;
  - Kandelaber `candelabrum` (2), eiserner Kronleuchter `chandelier_iron`, Tuffsäule `column_tuff`;
  - alte Tür `door_old_open`, Inschrift `inscription_bibliotheca`, Gemälde `painting_renaissance_a/b` (je 2);
  - Wappen `ceiling_arms` (2), abgelöste Tapete `wallpaper_peel`, herabgefallene Bücher `books_fallen` (3).
- **Wesen und Wissen** (Teil 4, `tools/blender/creatures/gentleman.py`, `models_l53_wissen.py`):
  - der Hausherr `ent_gentleman_l0..l2` (Skelett `gm`, 35 Gelenke), Notizbuch `gm_notebook`;
  - Lesepult `lore_lectern` mit Folio, Katalogschrank `lore_catalog`, Zettel `lore_slip`, Karteikarten `lore_cards`.
- **In der Abnahme neu oder neu gebaut**:
  - `ent_gentleman_l0..l2`: bemaltes Gesicht (Rouge, Umbra, Bleiweiß; flacheres Rissnetz), Seidenrevers, flachere Weste,
    Hemdbrust, Nägel aus Elfenbein; Materialien `FM_ENT_GM_SILK` (neu), Frack mit Nadelstreifen (`kMatPinstripe`), Weste,
    Hand; Shader `pinstripes` und Bemalung (`kMatFlagEntPaint`) in `mesh_gbuffer.hlsl`; Kanal `paint` in `build.py`;
  - Zorn der Bibliothek als Kaskade aus vier Haufen (`session_library.cpp`);
  - `gm_notebook` mit bereinigter Indexliste (`repair_glb.py`).

**Bilder:**
- Laufweg `docs/screens/level5_3/wegbilder_r1_*`, `_r2_*`, `_r5_*`, `wegkarte_r1.png`, `wegkarte_r2.png`, `wegkarte_r5.png`.
- Wesen und Wissen in der Ebene `docs/screens/level5_3/wesen_r2_*` … `wesen_r5_*` (Einzelbilder `build/qa/creatures/level5_3/r2..r5/`).
- Ausstattung `docs/screens/level5_3/props_uebersicht_*.jpg`, `docs/screens/props/level5_3/`; Zonen `docs/screens/level5_3/01_*` … `18_*`.
- Hausherr im Detail `docs/screens/creatures/gentleman/` (Runden 0–5).

Gefundene und behobene Fehler:
- **Spielfehler Lesepult**: das Folio der roten Lichter ließ sich von keinem Platz aus lesen – der unsichtbare Block des
  Pults (1,25 m) lag über der Sichtlinie, die `findTarget` bis 1,2 m über dem Boden prüft (→ 1,15 m; neue Prüfung im
  `gameplay_test`, die mit der alten Karte fehlschlug).
- **Hausherr**:
  - Frack ohne Nadelstreifen (→ Shader, an der Haut haftend, gefiltert); dann zu hell wie ein Schlafanzug (→ feiner, dunkler).
  - Im Lampenlicht grau wie Kunststoff (Rauheit 1,0 → 1,6, Grund 0,05 → 0,04).
  - Gesicht ohne gemalte Schatten und Wangenrot (→ Bemalung je Punkt); grobes Rissnetz wie zerknittertes Papier (→ flacher).
  - Revers unsichtbar, der Frack ein schwarzer Sack mit rotem Ballon (→ Seidenrevers; Spitze als Knopf → ohne Spitze; runde
    Kuppe als Zunge → flaches Band).
  - Weste als leuchtender Ballon (→ flacher, dunkler); als Kasten ein Ranzen, der durch den Rücken schien (→ verworfen).
  - Hand im Griff rosig wie Knetmasse (→ wachsbleich); Nägel als weiße Kappen, schwarze Fingerhüte, Pflaster (→ klein, dünn,
    Elfenbein).
- **Zorn der Bibliothek**: zwei kleine Haufen lasen sich als liegengelassene Bücher (→ Kaskade aus vier Haufen).
- **Modellprüfung**: `gm_notebook` (28,8 % entartete Dreiecke), `ent_gentleman_l1/l2` (Umlaufsinn) → `repair_glb.py`.
- **Werkzeug**: der Spieler stand beim Erscheinen am Start im Gang und verbrauchte die Störung (→ erst fort); aus 12 m war
  die Szene zu klein (→ 6 m, beide Plätze über 3 m von seinem Weg); das Folio aus dem falschen Winkel; Taste „Escape“
  unbekannt.
- **Gefunden, nicht in dieser Abnahme behoben**: der glTF-Export schreibt v als 1 − v, das Spiel liest die zweite
  Texturkoordinate der Wesen ungespiegelt – die Kante (Wölbung) kommt bei allen Wesen invertiert an. Alle Wesen sind mit
  diesem Stand abgestimmt; die Bemalung rechnet es ein. Vorschlag für eine eigene Entscheidung.

**Restpunkte** (begründet, Einzelheiten in `KRITIK_Level5_3.md` Runde 5 und `KREATUR_Gentleman.md`):
- Der Firnis des gemalten Gesichts ist aus 1,5 m körnig wie Pappmaché; das Bemalte trägt das Bild.
- Die Seidenrevers sind nur unten an der Öffnung zu sehen.
- Im Griff wirken die Finger aus 38 cm wulstig.
- Das eine Lesepult ist eine begründete Ausnahme zu `LOOK_5.3.md` (Ort des Wissens, E-047).

## Level 188 – abgenommen (03.11.2026)

| Punkt | Stand |
|---|---|
| a) Strukturtyp, Raummetrik | Eintrag in `STRUKTURTYP.md` (Eigener Ort „The Courtyard of Windows“): Hof 46 × 46 m, 30 m hoch, weißer Gang, drei Zellen, Treppenhaus, Treppenabsatz; Schild über dem Eingangsfenster entfernt (Wiki nennt keine Schilder). Raummetrik grün: 7 Räume, Decke min/Median 2,80/2,80 m, Gänge min/Median 3,0/3,0 m, Fläche Median 20 m², Sichtachsen Median 42,8 m, P90 50,0 m, 1 großer Raum, 0 Stützen unter 1 m, 0 von 1 verstellt. Ähnlichkeit S 0,990, Ausreißer 0,0 % (15 Fenster). In der Abnahme: Leuchte und Sicherungspunkt „Weißer Gang“ 1,5 m nach Norden zwischen zwei Röhren (`level188.py`, am Ende, ohne Zufall; Karte neu erzeugt, Vergleich: nur diese beiden Zahlen) |
| b) Ausstattung | `PROPS_Level188.md`: alle 14 Klassen neu in Blender (`models_l188.py`; Fenster a/b/c je 2 Varianten, Fenster mit Umriss, erleuchtet, Eingangsfenster, Mahagonitür zu und offen, Pflaster 3, Birkenkreis mit Deckel, Stütze mit Brettschalung, Oberlicht mit 64 Feldern, Tisch mit Fotos, Büchern, Vase und Wecker, Glasgalerie), Runden vorher, r1–r5, final (`docs/screens/props/level188/`, Übersichten `docs/screens/level188/props_uebersicht_*.jpg`). Im Laufweg und in den Bereichen geprüft (Runden 3 und 5, `zones 1–12`): Fensterreihen, Moos (von weitem Rasen – so will es die Seite, nah Polster), Plattenweg, Birkenkreis mit Fuge, Tisch und Grammophon, Glasgalerie, Oberlicht, Mahagonitüren, Zellen, Treppenhaus, Absatz mit verschlossener Tür |
| c) Wesen | Alle Wesen der Level-188-Seite im Spiel (ENTITY_ABGLEICH, Abschnitt Level 188): Fensterwesen (drei dunkle Fenster im Erdgeschoss: flüstern, winken, greifen unter 1,9 m), Faceling im rosa Kleid (nur in den Gängen, tödlich), Skin-Stealer (wahre Gestalt, nur in den Gängen), schattenhafte Umrisse (Gestalt im Hof, Umrisse in erleuchteten Fenstern), „andere bösartige Menschenähnliche“ als Horcher (Rolle 21, gekennzeichnete Auslegung; darin das ungesehene „Es“ aus Log 6), Entity 39 als Wurzelpistole (Weltobjekt im Moos und im Riss des Gangs). Ausnahme nach Auftrag: Amor Incrementum (Menschengruppe) – nur Spuren (Probenstiche, leerer Kasten, Probenzettel). Je KREATUR-Datei Wow-Check und mindestens 5 Kritikrunden; in der Abnahme 5 weitere Runden für Horcher und Wurzelpistole (Abschnitt „Abnahme Level 188“ in den Briefings) |
| d) Kritikrunden | 7 Ebenen-Runden (`KRITIK_Level188.md`): 1 Struktur (01.11.), 2 gleicher Weg (Treppenhaus), 3–7 Abnahme mit Laufweg und allen 10 Wesen-Läufen in ihren Räumen (neu: Tabelle für Level 188 in `tools/qa/creatureshots.py` – Fensterwesen fern/Lampe/nah/Arm, Gestalt im Hof, Faceling im rosa Kleid als Vorführung und echt in ihrer Zelle bis zum Zugriff, Skin-Stealer ebenso, Horcher mit Posen und Auftritt aus der Zelle, Wurzelpistole mit Griff, Schuss, Keimlingen, Spuren und aus 4/7 m, Stromausfall, Beil, Bereiche; Licht der Ebene und Lampe). Bögen `docs/screens/level188/wesen_r3_*` … `wesen_r7_*`. Dazu die Runden der Ausstattung (vorher, r1–r5, final) und der Wesen (Briefings) |
| e) Laufweg | `walkway.py level188 --rooms 15 --seed 3`: 207 m durch alle 7 erreichbaren Räume (Hof, Gang, drei Zellen, Treppenhaus, Absatz – mehrfach und aus anderen Richtungen), 52 Bilder alle 4 m (`docs/screens/level188/wegbilder_r3_*`, `_r5_*`, Karten `wegkarte_r3.png`, `wegkarte_r5.png`). **E-036 greift:** die Karte hat nach Wiki-Strukturtyp (ein Atrium mit Gängen am Rand) nur 7 Räume; die 200 m bleiben Pflicht, mehr Räume nur für die Zahl widersprächen Teil 2 (Beleg: `room_metrics.py`, Spalte „Raeume“: 7). Runde 1: 214 m durch 6 Räume, 54 Bilder; Runde 2: 207 m durch 7 Räume, 43 Bilder |
| f) Prüfungen | Modellprüfung aller 50 Modelle der Ebene (Karte mit Varianten, Gaben `e39_*`, Wesen in allen Stufen: Horcher, Faceling im Kleid, Skin-Stealer, Fensterwesen, Gestalt; Funde): 0 Fehler (vorher 3: `ent_listener_l1/l2` und `ent_window_l2` mit gedrehtem Umlaufsinn nach dem Vereinfachen → `repair_glb.py`; der neue Kasten hatte zwischendurch 5,7 % entartete Dreiecke im Samt → Quelle korrigiert). Halt der Ausstattung (`gameplay_test`): level188 320 Objekte mit Halt-Regel, 0 ohne Halt. Schilder (`signs.py --check`): Notausgangsschilder 0 (Grenze 0), Notausgangstüren 0, falsch platziert 0, Sicherheitsleuchten mit Piktogramm 0. Raummetrik und Ähnlichkeit grün. Tests grün: gameplay 1020/0 (neu: Halbdunkel beim ersten Heraustreten), audio 121/0, coop --quick 209/0, net 72/0, worldgen 29658/0, supply OK. Smoke-Test 03.11. (`docs/metrics/smoke_level188.json`, 24 Stationen des Laufwegs): GPU 5,82 ms (max 8,22), Spiel 0,10 ms, bis 0,18 Mio. Dreiecke, VRAM 992 MB, RAM 1118 MB (Spitze 1421 MB), 0 Fehler im Protokoll. Grafik-CPU 16,53 ms ist die Wartezeit auf den 60-Hz-Takt (wie die anderen Ebenen) |
| g) Aufwand | Struktur und Kritikrunde 1: 01.11. bis 01:21 (Hauptstrang, mit 5.3, 903, 37, 36). Ausstattung 01.11. 19:36–20:45 gemeinsam mit 5.3 (laut `PROPS_Level5_3.md` etwa 10 h für beide). Wesen: Fensterwesen, Faceling im Kleid, Skin-Stealer in V13.2; Horcher und Wurzelpistole (Teil 4) 02.11. abends bis 03.11. 09:22 (WIP 07:59, Grundbau 08:54, Kritikrunden 1–6 bis 09:22), Zusammenführen 11:08. Abnahme (Runden 3–7) 03.11. 11:16–12:15 (Uhr des Rechners; Commits Runde 3 11:44, Runde 4 11:51, Runde 5 11:59, Runden 6–7 12:08): etwa ein Fünftel Werkzeug (Level-188-Tabelle in `creatureshots.py`, `near … weit`, `face` für die Wesen der Gänge, Bilder der Pistole aus 4/7 m), gut die Hälfte Neubau und Bilder (Kreidehaut als eigene Texturebene in zwei Anläufen, Halbdunkel mit Welle und ohne, Moosfleck in drei Anläufen, Kasten neu), der Rest Prüfungen, Messungen, Tests und Dokumente |

**Neu gebaute Assets (Level 188):**
- **Ausstattung** (`tools/blender/models/models_l188.py`, Bögen `docs/screens/props/level188/`):
  - Fenster `window_188_a/b/c` (je 2), `window_188_fig`, `window_188_lit`, Eingangsfenster `window_entry_188`;
  - Mahagonitür `door_mahogany`, offen `door_mahogany_open`; Pflaster `paving_strip` (3), Birkenkreis `birch_circle`;
  - Stütze `pillar_188`, Oberlicht `skylight_188`, Tisch `table_188` mit Gegenständen, Glasgalerie `walkway_glass`;
  - Sicherheitsleuchte ohne Piktogramm `safety_light@1` (E-050).
- **Wesen und Gaben** (Teil 4, `tools/blender/creatures/listener.py`, `models_l188_gifts.py`):
  - Horcher `ent_listener_l0..l2` (Skelett `hr`); Wurzelpistole `e39_gun`, `e39_root`, `e39_husk`, `e39_bed_moss`,
    `e39_bed_crack`, `e39_sprout`; Spuren `e39_samples`, `e39_case`, `e39_note`.
- **In der Abnahme neu oder neu gebaut**:
  - Texturebene „kreide“ (`EL_CHALK`, `texture_entity.cpp`): Kreidehaut des Horchers (Schollen 2,6 cm, enge dunkle Risse,
    rote Spalten nur wo die Haut aufklafft, abgeplatzte Schollen); Mikroebenen ab Platz 110;
  - `e39_case` (tiefer Kasten, stehender Deckel mit Strebe und Abdruck, Samtmulde, Messing), `e39_bed_moss` (ausgezehrter,
    rostbrauner Moosfleck 0,8 m aus 330 Polstern, Wurzelgeflecht);
  - `ent_listener_l1/l2`, `ent_window_l2` mit bereinigten Indexlisten (`repair_glb.py`).

**Bilder:**
- Laufweg `docs/screens/level188/wegbilder_r1_*` … `_r3_*`, `_r5_*`, Karten `wegkarte_r1.png` … `_r5.png`.
- Wesen in der Ebene `docs/screens/level188/wesen_r3_*` … `wesen_r7_*` (Einzelbilder `build/qa/creatures/level188/r3..r7/`);
  beste Ansichten: `wesen_r5_*` auftritt 3–4 (Horcher im Halbdunkel), `wesen_r6_*` horcher 10 (Kreidehaut), dress 3 und
  window 3, `wesen_r7_*` pistole 1–4 und 11 (Fleck), pistole 10 (Kasten).
- Ausstattung `docs/screens/level188/props_uebersicht_*.jpg`, `docs/screens/props/level188/`; Zonen `docs/screens/level188/01_*` … `13_*`.
- Wesen im Detail `docs/screens/creatures/horcher/`, `wurzelpistole/`, Fensterwesen und Faceling in ihren Briefings.

Gefundene und behobene Fehler:
- **Grünes Kreuz im Gang**: die Leuchte des Sicherungspunkts hing genau unter einer Röhre (Geometrie in Geometrie, von weitem
  ein Apothekenkreuz in einer schilderfreien Ebene) → zwischen zwei Röhren.
- **Horcher**:
  - Haut cremegelb und marmoriert (Zahnschmelz-Ebene) → eigene Kreidehaut; danach Netz wie Krakelee-Porzellan → kleinere
    Schollen, Risse meist eng und dunkel.
  - Erste Begegnung im grellen Licht ohne Halbdunkel → Röhren fallen beim ersten Heraustreten 7 s aus (Auslegung, Test).
  - Das Dunkel kam als Welle (der 24 m lange Gang galt als Halle), dauerte fast 15 s, Zellen und Treppenhaus blieben hell →
    `Haunt::powerCut` mit `wave = false` (negative Dauer, im Koop über das Vorzeichen; alle anderen Aufrufer unverändert).
- **Wurzelpistole**: aus 1,2 m ein Seestern, aus mehr als 3 m unsichtbar → ausgezehrter Moosfleck mit Wurzelgeflecht (als
  Ringfläche zuerst wie Papier → Polster; zuerst dunkel wie Laub → rostbraun, 0,8 m).
- **Kasten der Gabe**: von oben ein Bilderrahmen; der Deckel lag durch den Drehsinn im Boden → neu, Deckel steht.
- **Modellprüfung**: `ent_listener_l1/l2`, `ent_window_l2` (Umlaufsinn) → `repair_glb.py`; `e39_case` Samt mit entarteten
  Dreiecken → Quelle.
- **Werkzeug**: `near` stellte die Kamera im 3 m breiten Gang an die Wand (→ `near … weit`); `face` kannte die Wesen der
  Gänge nicht; Bild 8 der Pistole stand in der Wand; `zones_11` sah auf die Wand des Absatzes.
- **Gefunden, nicht in dieser Abnahme behoben**: während eines Stromausfalls, der nur einen Teil eines Stromkreises
  verdunkelt (Welle in großen Räumen), leuchten hell gebliebene Nachbarräume als Sägezahn aus Dreiecken durch die Oberkante
  der Wand (Schatten an der Wand-Decken-Kante). In Level 188 tritt es nicht mehr auf (ganzer Stromkreis); Vorschlag für die
  Buglist.

**Restpunkte** (begründet, Einzelheiten in `KRITIK_Level188.md` Runden 6–7 und den Briefings):
- Außerhalb des Auftritts sind die Gänge grell (Seite).
- Der Arm des Fensterwesens wirkt nah (unter 2 m, nur im Augenblick des Zugriffs) steif wie eine Gliederpuppe.
- Die Pistole bleibt aus mehr als 3 m ein dunkler Punkt; der Fleck verrät die Stelle. Nah wirken die toten Polster als
  gleichförmige Tupfen.
- Laufweg und Raumzahl nach E-036 (7 Räume).

## Level 903 – abgenommen (08.11.2026)

| Punkt | Stand |
|---|---|
| a) Strukturtyp, Raummetrik | Eintrag in `STRUKTURTYP.md` (Labyrinth, linear: endlos gleicher U-Bahnsteig, zwei Gleise, ruhender Zug, verbarrikadierte Treppen): ein Bahnsteig 196 × 22 m, Halle 5,3 m, Mittelsteig 8 m breit und 1,1 m über dem Gleisbett. Raummetrik grün: 1 Raum, Decke min/Median 5,30/5,30 m, keine Gänge, Fläche 4312 m², Sichtachsen Median 98,5 m, P90 170,2 m, 1 großer Raum, 0 Stützen unter 1 m, 0 von 0 verstellt. Ähnlichkeit S 1,000, Ausreißer 0,0 % (15 Fenster). In der Abnahme (`level903.py`, am Ende, ohne Zufall): Dunkelfläche `dark_end_903` an beiden Stirnwänden, ein letzter Zugabschnitt im Osten (dort klaffte die Hallenwand hinter dem unsichtbaren Zugblock); Karte neu erzeugt, erneutes Erzeugen am 08.11. ergibt dieselbe Datei |
| b) Ausstattung | `PROPS_Level903.md`: alle 13 Klassen neu in Blender (`models_l903.py`; fahrender Wagen 903 und derselbe Wagen für Level 37, Gleis 2, Bahnsteigkante 3, Röhre 3, Hängeschild a/b, Wandschild 2, Treppe, ruhender Zug a/b je 2, bewohnter Wagen, Kupplung), Runden vorher, r1–r5, final (`docs/screens/props/level903/`, Übersichten `docs/screens/level903/props_uebersicht_*.jpg`). In der Abnahme an Ort und Stelle geprüft (Läufe `metro`, `zug`, `zones` je Runde, Ebenenlicht und Lampe) und nachgebessert: Wagenkasten, Unterboden, Trittblech, Abdeckung des Wagenboden-Blocks, Treppenrückseite, Ablaufschmutz des ruhenden Zugs, Dunkelfläche (Nachträge in `PROPS_Level903.md`) |
| c) Wesen | Alle Wesen der Level-903-Seite im Spiel (ENTITY_ABGLEICH, Abschnitt Level 903): die Metro (Entity 756) als fahrender Wagen, der Ziele annimmt (vorn Level 37, hinten Level 36, Bildzeichen statt Schrift), neutral; friedliche Facelings (Entity 9, `KREATUR_Faceling.md`: Gruppen 2–4, drei Varianten, winken, wen sie eine Weile sehen; Briefing mit Wow-Check und Kritikrunden aus V13.2). Ausnahme nach Auftrag: Bewohner und feindselige Lokomotiv-Basen (Menschen) – der bewohnte Wagen ist als Kulisse da. Neu in der Abnahme belegt: Facelings erscheinen außer Sicht, auf dem offenen Steig fast nur im fahrenden Wagen – sie kommen mit dem Wagen an und gehen auf den Steig (`LEVELSPEC_903.md`); die Wagenseite hat im Lampenlicht keinen Eigenschatten (E-041 wirkt), war aber durch das Material schwarz → behoben |
| d) Kritikrunden | 6 Ebenen-Runden (`KRITIK_Level903.md`): 1 Struktur (01.11.), 2 Abnahme des Vorgängers (03.11., WIP 24e48f0, vom Nutzer angehalten; in dieser Abnahme geprüft und übernommen), 3–6 Abnahme mit Laufweg und allen 6 Läufen der neuen Level-903-Tabelle in `tools/qa/creatureshots.py` (Metro-Wagen: Front, Tür, innen mit beiden Zielanzeigen, vom Gleis, unter dem Wagen, Seite nur im Lampenlicht; Facelings: Ankunft im Wagen, Steig, Winken, Varianten; ruhender Zug mit bewohntem Wagen und Glas; Stromausfall; Beil; Bereiche nach LOOK_903 mit Treppen von vorn und hinten, Schildern und Enden). Bögen `docs/screens/level903/wesen_r2_*` … `wesen_r6_*`; dazu die Runden der Ausstattung (vorher, r1–r5, final) |
| e) Laufweg | `walkway.py level903 --rooms 15 --seed 3`: 294 m in einem Raum (der Bahnsteig hin und zurück bis in das Dunkel beider Enden), 74 Bilder alle 4 m (`docs/screens/level903/wegbilder_r3_*`, `_r6_*`, Karten `wegkarte_r3.png`, `wegkarte_r6.png`; vorher `_r1_*`, `_r2_*` mit denselben Zahlen). **E-036 greift:** die Seite beschreibt eine einzige lange Station; die Karte hat 1 Raum (`room_metrics.py`, Spalte „Raeume“: 1). Die 200 m bleiben Pflicht und sind erfüllt; `walkway.py` meldet wegen der Raumzahl Exitcode 1 |
| f) Prüfungen | Modellprüfung aller 32 Modelle der Ebene (Karte mit Varianten: Wagen, Dunkelfläche, ruhender Zug a/b je 2, bewohnter Wagen, Kupplung, Kante 3, Sicherheitsleuchte ohne Piktogramm, Hängeschilder, Wandschild 2, Treppe, Gleis 2, Röhre 3; Facelings in drei Varianten je Stufe l0–l2; dazu `metro_car_37`): 0 Fehler. Halt der Ausstattung (`gameplay_test`): level903 556 Objekte mit Halt-Regel, 0 ohne Halt. Schilder (`signs.py --check`): Notausgangsschilder 0 (Grenze 0), Notausgangstüren 0, falsch platziert 0, Sicherheitsleuchten mit Piktogramm 0 – die beleuchteten Schilder der Seite sind Stationsschilder in erfundener Schrift. Raummetrik und Ähnlichkeit grün. Tests grün: gameplay 1020/0, audio 121/0, coop --quick 209/0 (erster Lauf parallel zu den übrigen Tests 1 Fehler im Szenario „schlechtes Netz“, Abstand Host/Gast nach dem Anhalten – bekannt zeitabhängig; Wiederholung allein 209/0; geändert sind nur Modelle), net 72/0, worldgen 29658/0, supply OK. Smoke-Test 08.11. (`docs/metrics/smoke_level903.json`, 24 Stationen): GPU 2,17 ms (max 3,03), Spiel 0,03 ms, Grafik-CPU 8,28 ms, bis 0,35 Mio. Dreiecke, VRAM 1208 MB, RAM 1107 MB (Spitze 1403 MB), 0 Fehler im Protokoll |
| g) Aufwand | Struktur und Kritikrunde 1: 01.11. (Hauptstrang, mit 5.3, 188, 37, 36). Ausstattung etwa 8 h (27.10. und 01.11., `PROPS_Level903.md`). Wesen: Facelings in V13.2, der Wagen als Metro seit V12/V13.2. Abnahme: Runde 2 am 03.11. nach der Abnahme von Level 188 (12:15) bis zum Anhalten (WIP 12:43); Runden 3–6 am 08.11. 14:55–15:40 (Uhr des Rechners; Bau 14:57, Commits Runden 2–4 15:11, Runden 5–6 15:32, Abschluss mit diesem Eintrag): etwa ein Fünftel Werkzeug (Level-903-Tabelle fortgesetzt: Faceling-Lauf, Wagenseite im Dunkeln, Debug-Ansichten des Renderers für Schatten/Albedo/Normalen), gut ein Drittel Ursachensuche und Neubau des Wagenmaterials (drei Anläufe: Unterboden und Trittblech, Metallwerte, Silberlack), der Rest Bilder, Prüfungen, Messungen, Tests und Dokumente |

**Neu gebaute Assets (Level 903):**
- **Ausstattung** (`tools/blender/models/models_l903.py`, Bögen `docs/screens/props/level903/`):
  - fahrender Wagen `metro_car_903` (mit Gleis darunter), derselbe Wagen `metro_car_37`; Gleis `track_metro` (2);
  - Bahnsteigkante `platform_edge` (3), Röhre `tube_long` (3), Hängeschilder `sign_903_a/b`, Wandschild `sign_903_wall` (2);
  - Treppe `stairs_barricaded`; ruhender Zug `metro_dormant_a/b` (je 2), bewohnter Wagen `metro_dormant_home`, Kupplung
    `metro_dormant_coupling`; Sicherheitsleuchte ohne Piktogramm `safety_light@1` (E-050).
- **In der Abnahme neu oder neu gebaut**:
  - `dark_end_903` (neu): fast lichtlose Fläche 22 × 5,4 m vor beiden Stirnwänden (Halt-Regel in `mapkit.py`, `gameplay_test`);
  - `stairs_barricaded`: Rückseite mit Fliesen bis zur Decke, Kantenschutz, Wasserlauf, Revisionstür (Zarge, Bänder,
    Drücker, Schloss, Lüftungsgitter, Warnschild in erfundenen Zeichen);
  - `metro_dormant_a/b/home` (mit Varianten): Ablaufschmutz schmal mit Läufern;
  - `metro_car_903`: Abdeckung des Wagenboden-Blocks (Kopfquerträger, Rippen, Blech, Bremsleitung, Kabelschleife, Rost,
    Seitenbleche, dunkle Türschwellen); `metro_car_903` und `metro_car_37`: Wagenkasten Silberlack, Unterboden stumpf
    ohne Lacktextur, Trittblech gebürstet.

**Bilder:**
- Laufweg `docs/screens/level903/wegbilder_r1_*`, `_r2_*`, `_r3_*`, `_r6_*`, Karten `wegkarte_r1.png`, `_r2.png`, `_r3.png`,
  `_r6.png`.
- Wesen und Bereiche `docs/screens/level903/wesen_r2_*` … `wesen_r6_*` (Einzelbilder `build/qa/creatures/level903/r2..r6/`);
  beste Ansichten: `wesen_r6_2` faceling 1–2 (Ankunft im Wagen) und faceling 8 (Winken auf dem Steig), metro 11–12
  (Wagenseite im Lampenlicht); `wesen_r6_1` metro 1, 3, 4, 8 (Wagen am Steig, Tür, innen, vom Gleis); `wesen_r6_3` zug 2–3
  (bewohnter Wagen); `wesen_r6_4` zones 4 (Treppenrückseite).
- Beleg Wagenseite `docs/screens/level903/wagenseite_lampe.jpg` (vorher, Schattenansicht, Albedo, nachher); Schatten
  unsichtbarer Blöcke `schatten_vorher_nachher.jpg`, `schatten_gegenprobe.jpg` (E-041).
- Ausstattung `docs/screens/level903/props_uebersicht_*.jpg`, `docs/screens/props/level903/`; Bereiche `docs/screens/level903/01_*` … `12_*`.

Gefundene und behobene Fehler:
- **Ende der Station**: mit der Lampe eine helle Fliesenwand → Dunkelfläche an beiden Stirnwänden; im Osten klaffte hinter
  dem unsichtbaren Zugblock die Hallenwand → letzter Zugabschnitt.
- **Treppe von hinten**: offener Kasten (Stufenkörper, Rückseite der Ziegelwand) → Fliesenwand mit Revisionstür.
- **Ruhender Zug**: Ablaufschmutz als helles Band wie Schnee auf dem Dach → schmaler Rand mit Läufern.
- **Unter dem Wagen**: Stirnflächen und Türschwellen des begehbaren Blocks mit weißen Hallenfliesen → abgedeckt.
- **Front vom Gleis im Lampenlicht**: Trittblech als leeres weißes Rechteck, Rammschutz weiß-schwarz gefleckt → gebürstet,
  Unterboden ohne Lacktextur.
- **Wagenseite im reinen Lampenlicht schwarz**: kein Eigenschatten (Schattenansicht frei), sondern die Metalltextur (Metall
  aus der Textur 1, glatt; m und r allein halfen nicht) → Silberlack; gilt auch für den Wagen in Level 37.
- **Werkzeug**: `near … 6 0 1` stellte die Kamera hinter die Treppe; Facelings im Wagen waren hinter der Wagenwand nicht zu
  sehen → Ankunft durch die offene Tür, Steig, Winken.

**Restpunkte** (begründet, Einzelheiten in `KRITIK_Level903.md` Runden 3–6 und `PROPS_Level903.md`):
- Glas ist deckend (Engine); der ruhende Zug hat keinen Innenraum – seine dunklen Scheiben lesen sich wie bei einem
  abgestellten, unbeleuchteten Zug.
- Aus weniger als 6 m zeigt die Dunkelfläche im Lampenlicht eine schwache runde Aufhellung (liest sich als ferne Wand im Dunst).
- Facelings erscheinen auf dem offenen Steig fast nur im fahrenden Wagen (Regel „außer Sicht“); als Ankunft mit dem Wagen
  gedeutet.
- Im Wagen steht ein offenes Türblatt mit dunkler Scheibe vor der Innenwand (seit der Ausstattung).
- Laufweg und Raumzahl nach E-036 (1 Raum).

## Level 903, 37, 36

Strukturprüfungen grün (Level 5 mit begründeten Ausnahmen, Level 188 Schild entfernt, Level 36 vier Wiki-Bereiche ergänzt).
Ausstattung: Agenten in Arbeit. Einträge folgen mit der Abnahme.
