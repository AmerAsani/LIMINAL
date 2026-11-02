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

## Level 3 – Struktur fertig, Abnahme folgt

Struktur 27.10. 16:22–16:26: drei Ziegel-Gangnetze (Gang/Kammer 60 von 71 → 191 von 207 Räumen), Büro entfernt,
Paranoia-Gang 1 → 2 m breit, 2,3 → 2,6 m hoch; schwarze Rohre nur an einer Wand enger Gänge. Ausstattung: Agent in Arbeit.

## Level 0 – Struktur angepasst, Abnahme folgt

Decken ≥ 2,7 m, breitere Durchbrüche (Sichtachsen 22,0 → 25,5 m), keine hervorgehobenen Raumnamen; Ähnlichkeit S 0,89.
Laufweg vorher: 202 m durch 25 Räume (`docs/screens/level0/wegbilder_vorher_*.jpg`).

## Level 5, 5.3, 188, 903, 37, 36

Strukturprüfungen grün (Level 5 mit begründeten Ausnahmen, Level 188 Schild entfernt, Level 36 vier Wiki-Bereiche ergänzt).
Ausstattung: Agenten in Arbeit. Einträge folgen mit der Abnahme.
