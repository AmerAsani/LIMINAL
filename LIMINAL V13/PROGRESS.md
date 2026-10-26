# LIMINAL V13 – Fortschritt

Wiedereinstieg: diese Datei zuerst lesen, dann `DECISIONS.md` (V13 ab E-019).
Auftrag: „LIMINAL – V13: Finger-Fix, Kreaturen-Neubau, Texturen, Bugfixing, Qualitätsupdate, Multiplayer (nur wenn nachweislich sicher)“.
Vorgeschichte: `docs/archiv/PROGRESS_V12.md`, `docs/archiv/REPORT_V12.md`.

## Fertig
- [x] V13 aus V12 (Stand `e8481b6`) kopiert, Branch `liminal-v13`. V12 eingefroren (Manifest `Sicherungen/v12_frozen_2026-10-26.txt`, 25 550 Dateien mit SHA-256). Version 13 (eigene Einstellungen, Spielstände, Protokolle; Einstellungen aus V12 werden nur gelesen). Build frisch, Tests grün (gameplay 351/0, audio 106/0, worldgen 29 658/0, supply OK).

## Als Nächstes
1. Teil 1: Hand und Taschenlampe – `HAND_ANALYSE.md`, Ursache, Neubau, mindestens 4 Kritikrunden (`docs/screens/viewmodel/`).
2. Teil 2: Kreaturen – `KREATUREN_ANALYSE.md`, Briefings `KREATUR_<Name>.md`, Neubau in Blender mit Skelett, Vorher/Nachher (`docs/screens/creatures/`).
3. Teil 3: Texturen – `TEXTUR_AUDIT.md`, PBR, Kachelbrecher, Leistung und VRAM.
4. Teil 4: Qualitätsupdate aller Level, Validierung, Lore-Abgleich.
5. Teil 5: Smoke-Tests, `BUGLISTE.md`.
6. Teil 6: `MULTIPLAYER_BEDROHUNGSMODELL.md`, Entscheidung, Tests.
7. `REPORT_V13.md`, Schlussprüfung (V12-Manifest, V10 per Git).

## Bekannte Fehler
- (noch keine neuen; Liste folgt in `BUGLISTE.md`)
