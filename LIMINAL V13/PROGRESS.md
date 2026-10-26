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

## Als Nächstes
2. Teil 2: Kreaturen – `KREATUREN_ANALYSE.md`, Briefings `KREATUR_<Name>.md`, Neubau in Blender mit Skelett, Vorher/Nachher (`docs/screens/creatures/`).
3. Teil 3: Texturen – `TEXTUR_AUDIT.md`, PBR, Kachelbrecher, Leistung und VRAM.
4. Teil 4: Qualitätsupdate aller Level, Validierung, Lore-Abgleich.
5. Teil 5: Smoke-Tests, `BUGLISTE.md`.
6. Teil 6: `MULTIPLAYER_BEDROHUNGSMODELL.md`, Entscheidung, Tests.
7. `REPORT_V13.md`, Schlussprüfung (V12-Manifest, V10 per Git).

## Bekannte Fehler
- Kapuzenjacke reißt am stark gebeugten Ellbogen auf (Skinning der Spielerfigur, Schulteransicht) → `BUGLISTE.md`.
- Rechte Hand (Gesten) ist weiter die prozedurale Hand.
