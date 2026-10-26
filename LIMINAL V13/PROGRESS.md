# LIMINAL V13 – Fortschritt

Wiedereinstieg: diese Datei zuerst lesen, dann `DECISIONS.md` (V13 ab E-019).
Auftrag: „LIMINAL – V13: Finger-Fix, Kreaturen-Neubau, Texturen, Bugfixing, Qualitätsupdate, Multiplayer (nur wenn nachweislich sicher)“.
Vorgeschichte: `docs/archiv/PROGRESS_V12.md`, `docs/archiv/REPORT_V12.md`.

## Fertig
- [x] V13 aus V12 (Stand `e8481b6`) kopiert, Branch `liminal-v13`. V12 eingefroren (Manifest `Sicherungen/v12_frozen_2026-10-26.txt`, 25 550 Dateien mit SHA-256). Version 13 (eigene Einstellungen, Spielstände, Protokolle; Einstellungen aus V12 werden nur gelesen). Build frisch, Tests grün (gameplay 351/0, audio 106/0, worldgen 29 658/0, supply OK).

- [x] Teil 1: Hand und Taschenlampe. Analyse und vier Kritikrunden in `HAND_ANALYSE.md`, Bilder in `docs/screens/viewmodel/v13_*`, Entscheidung E-020.
  - Neue Hand: Käfig mit Subdivision, Skelett mit 21 Knochen, Griff gelöst, Posen `grip` und `press`.
  - Engine: Skin und Posen aus glTF; Schaltfolge mit Daumendruck; Ärmel mit Bündchen; Debug-Kamera `vmcam`, Ausblenden `vmdebug`.
  - Tests: gameplay 357/0.
  - Hilfsbefehle:
    - Hand bauen: `blender -b --factory-startup --python tools/blender/build_models.py -- vm_hand_l vm_flashlight vm_flashlight_on`;
    - Vorschau: `tools/blender/preview_hand.py -- <ordner> rest|grip|final|press`.

- [x] Teil 2: Kreaturen (E-021). Alle sechs Wesen neu in Blender (`tools/blender/creatures/`): Hound 6, Smiler 5, Clump 7, Gestalt 5, Deathmoth 4, Nest 4 Kritikrunden; Vorher/Nachher aus denselben Kameras (`docs/screens/creatures/`). Commits `137ca9e`, `fffc771`.
  - Bauen: `blender -b --factory-startup --python tools/blender/creatures/build.py -- <wesen> --preview <ordner>`, danach `cmake --build build/Release` (kopiert data/); im Protokoll muss `Quelle Blender` stehen.
- [x] Teil 3: Texturen – `TEXTUR_AUDIT.md`, Kachelbruch (E-022), Leistung (G-Buffer +0,06–0,18 ms) und Grafikspeicher (939–1003 MB). Commit `fcfaede`.
- [x] Teil 6: Mehrspieler bleibt aus (E-024): Bedrohungsmodell, Entscheidung, Prüfschicht `src/net` mit 72 Prüfungen und Fuzzing, `tools/check_no_network.py`; Starter-Signatur (E-023). Commits `e72ba4d`, `3285333`.

- [x] Teil 4: Qualitätsupdate – `QUALITAET_LEVEL.md` (feste Kameras, Lampe, Wesen in ihren Ebenen, Karten geprüft, Lore-Abgleich mit den Wikidot-Revisionen vom 26.10.).
- [x] Teil 5: Bugsuche – `BUGLISTE.md` (Smoke-Test aller Ebenen und Menüs, Speichern/Laden, fünf Auflösungen, Bildzeiten, Speicher); B-003 und B-004 behoben.
- [x] Abschluss: `REPORT_V13.md`; V12-Manifest geprüft (25 550 Dateien, 0 verändert), V10 ohne Änderung; Tests gameplay 357/0, audio 106/0, worldgen 29 658/0, supply OK, net 72/0.

## V13 Fortsetzung (V13.2) – Auftrag „Offene Punkte, Mottennest, Entity-Abgleich, Multiplayer, GitHub-Upload“
Wiedereinstieg: diesen Abschnitt lesen, dann `DECISIONS.md` ab E-025.

- [x] Schritt 0: GitHub-Sicherung lokal vorbereitet (E-025, `../GITHUB_UPLOAD.md`): `origin` ist öffentlich, daher kein Push; Branches `liminal-v11`/`liminal-v12`, Tags `v11`/`v12`, README, `.gitignore`, Prüfungen. Befehle für ein privates Repository stehen bereit.
- [x] Entity-Abgleich: `ENTITY_ABGLEICH.md` (zehn Ebenenseiten, Entity-Übersicht, Entity-Seiten; Revisionen vom 26.10.2026). Nest belegt (E-026), Reihenfolge der fehlenden Wesen (E-027). Smiler reagiert auf Lärm.
- [x] B-002: Texturen Ebene für Ebene, Ladespitze 3,1 GB → 0,68 GB (2048 px: 11,1 → 1,3 GB), Messung je Ebene in `BUGLISTE.md`.
- [x] B-001: Ärmel am gebeugten Ellbogen (E-028): Ruhepose 54 Grad, Dual Quaternions, Lampenhaltung von außen; Test und Bilder (`docs/screens/b001/`).
- [ ] Animationen und Modelle der Wesen, Nest-Neuentwurf
- [ ] Fehlende Wiki-Wesen (Faceling, Fensterwesen, Skin-Stealer, Wretch, Death Rat)
- [ ] Mehrspieler (Architektur, Stufen 1–7, Sicherheit) und Tests
- [ ] Abschluss: Tests, V12/V10 prüfen, Tag `v13`, `REPORT_V13_2.md`

Hilfsskripte (Scratchpad `v13`): `b.sh [gameplay|all]` (bauen + Tests), `mem_sweep.ps1` / `mem_trace.ps1` (Speicher), `elbow_tp.ps1` (Kamera um den Spieler, Skriptbefehle `figcam` ohne Vorführung in der Schulteransicht, `egopose 1`), `elbow_shots.ps1` (`vmcam`), `crop.py`.

## Als Nächstes (Vorschläge für V14)
- Bewegung der Wesen auf die neue Anatomie abstimmen.
- ORM-Texturen komprimieren (≈ 360 MB VRAM), Ladespitze senken (B-002).
- Rechte Hand mit dem neuen Handmodell; Ellbogen der Kapuzenjacke (B-001).

## Bekannte Fehler
- Rechte Hand (Gesten) ist weiter die prozedurale Hand.
