# Aufwand je Ebene (V13.3, Abnahmeschranke g)

Je Ebene: Runden, neu gebaute Assets, Bildpfade, gefundene/behobene Fehler, Zeitaufwand. Eine Ebene ohne vollständigen
Eintrag gilt nicht als abgenommen; ein Eintrag dünner als der von Level 0 ist ein Warnzeichen. Zeiten aus den Commit-Zeitpunkten
(Hauptstrang und Agenten-Zweige), gerundet. Stand wird bei jeder Abnahme fortgeschrieben.

Gemeinsame Vorarbeit für alle Ebenen (nicht einer Ebene zugerechnet): Werkzeuge `tools/qa/` (Kontaktbögen, Raummetrik,
Schilder, Ähnlichkeit, Laufweg, Smoke-Test, Kartenbild, Modellprüfung, Spot-Bilder, Prüfbogen), Engine-Erweiterungen
(freie Kamera, Testlampe, Weltlicht, Modellvarianten, Glanz je Material, Pfützen und Wasser, Deckenspiegelung), Wiki-Recherche
aller zehn Ebenen und ihrer Wesen (Wikidot, Notizen je Ebene), `LEVEL0_STANDARD.md`, `STRUKTURTYP.md`, `BIOM_AUDIT.md`.

## Level 1 – Abnahme läuft

| Punkt | Stand |
|---|---|
| a) Strukturtyp, Raummetrik | Eintrag in `STRUKTURTYP.md`; Sektoren Ouroboros, Garden, Fabled ergänzt; Decken ≥ 2,8 m, Sichtachsen Median 31,5 → 36,8 m; Raummetrik grün |
| b) Ausstattung | Teil B (29 Klassen) neu gebaut, 5 Runden (`PROPS_Level1_B.md`); Teil A (Code-Modelle, 22 Varianten) und Sektoren-Ausstattung (27 Klassen) in Arbeit |
| c) Wesen | Smiler, Hound, Clump, Skin-Stealer, Wretch im Spiel; Duller, Arms, Growler, Nguithr'xurh, Wrangler, Ouroboros-Arbeiter, Crawler, Jerry, Leuchtstab/Ruf in Arbeit |
| d) Kritikrunden | Ausstattung 5 Runden (Teil B); Ebenen-Runden folgen mit den fertigen Assets |
| e) Laufweg | vorher: 899 m durch 15 Räume, 225 Bilder (`docs/screens/level1/wegbilder_vorher_*.jpg`, `wegkarte_vorher.png`) |
| f) Prüfungen | Objektvalidierung Teil B 0 Fehler; Schilder 6 (Grenze 12, alle über Drucktüren); Smoke-Test: GPU 3,6 ms (max 4,9), RAM-Spitze 730 MB, VRAM 1 067 MB, 0 Fehler (`docs/metrics/smoke_level1.json`) |
| g) Aufwand | Struktur und Messung 27.10. 06:00–06:22 und 10:50–11:20 (Hauptstrang); Ausstattung Teil B 00:45–11:18 (Agent und Hauptstrang); Wasser/Pfützen 06:22–06:44 (gilt für alle Ebenen) |

Gefundene und behobene Fehler: Zonenlimit 16 der Engine (→ 24); Tests mit festen Koordinaten in Aquila (→ freie Stelle
gesucht); Kontaktbögen schwarz bei fernen Kabinen (Streaming → Spieler in die Kabine); Kisten über Pfützen-Spawns;
Wesen-Spawns zu nah an Sicherungspunkten; Säulen-Rohre in Öffnungen.

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
