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

## Level 2 – Struktur fertig, Abnahme folgt

Struktur 27.10. 16:00–16:22: Tunnelnetz im 15-m-Raster als Grundstruktur (Hallen 46 → 21 %, Tunnel 16 → 31 %), Brownout und
Macchina-Werkstatt ergänzt, verstellte Räume 68 → 0, Raummetrik/Ähnlichkeit/Schilder grün. Gefundene Fehler: Messung las alte
Kartenkopie (→ `game.py` gleicht Karten ab), Rohrbündel über Kopf als Hindernis gezählt (→ Metrik ab 1 m Höhe ausgenommen),
Engstelle 0,92 m am Trichterwagen (→ Nordwand frei). Ausstattung: Agent (29 Klassen) in Arbeit.

## Level 3 – Struktur fertig, Abnahme folgt

Struktur 27.10. 16:22–16:26: drei Ziegel-Gangnetze (Gang/Kammer 60 von 71 → 191 von 207 Räumen), Büro entfernt,
Paranoia-Gang 1 → 2 m breit, 2,3 → 2,6 m hoch; schwarze Rohre nur an einer Wand enger Gänge. Ausstattung: Agent in Arbeit.

## Level 0 – Struktur angepasst, Abnahme folgt

Decken ≥ 2,7 m, breitere Durchbrüche (Sichtachsen 22,0 → 25,5 m), keine hervorgehobenen Raumnamen; Ähnlichkeit S 0,89.
Laufweg vorher: 202 m durch 25 Räume (`docs/screens/level0/wegbilder_vorher_*.jpg`).

## Level 5, 5.3, 188, 903, 37, 36

Strukturprüfungen grün (Level 5 mit begründeten Ausnahmen, Level 188 Schild entfernt, Level 36 vier Wiki-Bereiche ergänzt).
Ausstattung: Agenten in Arbeit. Einträge folgen mit der Abnahme.
