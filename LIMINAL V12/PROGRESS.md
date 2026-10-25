# LIMINAL V12 – Fortschritt

Wiedereinstieg: diese Datei zuerst lesen, dann `DECISIONS.md`.

## Fertig
- [x] V11 eingefroren (Manifest), V10 geprüft (0 Unterschiede), V12 aus V11 kopiert, Branch `liminal-v12`, Version 12, Build frisch, Tests grün (gameplay 349, audio 106, worldgen 29 658, supply ok). Commit `7b5d36a`.
- [x] Blender 4.5.14 LTS portabel in `.toolchain/blender/` (Prüfsumme geprüft), läuft im Hintergrundmodus.
- [x] Schritt 0: Audit und Entscheidungen je System (`DECISIONS.md` E-002, E-003).

- [x] Schritt 1: Levelauswahl (`LEVELAUSWAHL.md`). Gewählt: 0, 1, 2, 3, 5, 5.3, 188, 903, 37, 36. Commit `3b58e34`.
- [x] Pipeline Texturen: 37 CC0-Sets (ambientCG), Lock mit Prüfsummen, 48 Ebenen im Spiel, `wall_tex`/`floor_tex`/`ceil_tex`. Commit `a595e4e`.
- [x] Pipeline Modelle: Blender → `.glb` → eigener Leser, Spiegelung y, Test (357/0).

## Als Nächstes
1. Skinning für Modelle aus Blender (Kreaturen, Hand) – erst, wenn das erste Wesen gebaut wird.
3. Querschnitt: Viewmodel (gestellte Hand mit Lampe), Oberfläche bei mehreren Auflösungen.
4. Level der Reihe nach: Spec → Look → Bau → Platzierung → Items → Entities → Audio → Qualitätsschleife → Commit.

## Bekannte Fehler / offen
- Einmalig hing in V11 ein Testlauf beim Start (Protokoll endete nach dem Laden der Level). In vier Wiederholungen nicht wieder aufgetreten. Ursache unbekannt; beobachten.
