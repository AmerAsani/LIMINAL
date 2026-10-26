# LIMINAL V11 – Herkunft und Namensnennung

LIMINAL V11 enthält **keine fremden Assets**. Modelle, Texturen, Animationen und Klänge entstehen beim Start im Programmcode. Die Texte im Spiel (Notizen, Hinweise, Hilfe) sind selbst geschrieben. Zur Lizenzstrategie siehe `LICENSE_REVIEW.md`.

## 1. Inspiration: Backrooms-Wikis (Recherche, keine übernommenen Inhalte)

Die Ebenen und Kreaturen von LIMINAL sind eigene Umsetzungen von Motiven, die auf diesen gemeinschaftlich geschriebenen Seiten beschrieben sind. Übernommen sind nur Ideen auf allgemeiner Ebene. Texte, Bilder, Audio, Ortsnamen, Gruppen und benannte Figuren der Wikis sind **nicht** übernommen. LIMINAL ist kein offizielles Projekt der Wikis.

Gelesen am 25.10.2026. Text jeweils unter **CC BY-SA 3.0** (Wikidot: Seitenfuß und Licensing Guide; Fandom: fandom.com/licensing). Bilder auf den Seiten haben eigene Lizenzen und wurden nicht verwendet.

### Backrooms Wiki (Wikidot) – backrooms-wiki.wikidot.com

| Seite | Autorinnen und Autoren laut Seite |
|---|---|
| [Level 0 – „Threshold“](https://backrooms-wiki.wikidot.com/level-0) | nach einem 4chan-Konzept, Reddit-Adaption; Neufassungen DivineAtlas, Robert Goerman, DrAkimoto |
| [Level 1 – „Habitable Zone“](https://backrooms-wiki.wikidot.com/level-1) | Praetor3005, DivineAtlas |
| [Level 2 – „Abandoned Utility Halls“](https://backrooms-wiki.wikidot.com/level-2) | Konzept u/M654z; Neufassung 1000dumplings; Seitenverantwortung Boring Talking |
| [Level 3 – „Electrical Station“](https://backrooms-wiki.wikidot.com/level-3) | u/M654z; Neufassungen Natedagreat563 |
| [Level 4 – „Abandoned Office“](https://backrooms-wiki.wikidot.com/level-4) | u/M654zy |
| [Level 5 – „Terror Hotel“](https://backrooms-wiki.wikidot.com/level-5) | Discord13; Neufassung Stretchsterz; Beiträge Ratscrapz |
| [Level 6 – „Lights Out“](https://backrooms-wiki.wikidot.com/level-6) | u/ThePizzaEater1000, u/Mileisbored; Neufassungen 1000dumplings, Abe Venrick, Bart0nius |
| [Level 7 – „Thalassophobia“](https://backrooms-wiki.wikidot.com/level-7) | u/Bart0nius, u/M654z; Neufassung Bart0nius |
| [Level 19 – „Attic Floorboards“](https://backrooms-wiki.wikidot.com/level-19) | u/BendyBoi3393; Neufassungen 1000Dumplings, Boring Talking |
| [Level 37 – „Sublimity“](https://backrooms-wiki.wikidot.com/level-37) | egglord |
| [Entity 3 – Smilers](https://backrooms-wiki.wikidot.com/entity-3), [Entity 4 – Deathmoths](https://backrooms-wiki.wikidot.com/entity-4), [Entity 8 – Hounds](https://backrooms-wiki.wikidot.com/entity-8) | siehe jeweilige Seite (Recherche aus V10, `docs/ENTITAETEN.md`) |
| [Entity 5 – Clump](https://backrooms-wiki.wikidot.com/entity-5) | Kekcroc; Neufassung TrailmixNCocoa |
| [Licensing Guide](https://backrooms-wiki.wikidot.com/licensing-guide) | Wiki-Team |

Zum Vergleich gelesen, aber nicht ausgewählt: Level 8, 9, 10, 11, 21, 26 („Level Fun“), 34, 54, 188 (Begründung in `LEVEL_SELECTION.md`).

### Backrooms Wiki (Fandom) – backrooms.fandom.com

Zum Vergleich gelesen: [Level 1](https://backrooms.fandom.com/wiki/Level_1), [Level 2](https://backrooms.fandom.com/wiki/Level_2), [Level 3](https://backrooms.fandom.com/wiki/Level_3), [Level 4](https://backrooms.fandom.com/wiki/Level_4), [Level 5](https://backrooms.fandom.com/wiki/Level_5), [Level 6](https://backrooms.fandom.com/wiki/Level_6), [Level 7](https://backrooms.fandom.com/wiki/Level_7), [Level 19](https://backrooms.fandom.com/wiki/Level_19), [Level 37](https://backrooms.fandom.com/wiki/Level_37), [Entity 5](https://backrooms.fandom.com/wiki/Entity_5), dazu aus V10 Level 0, Entity 3, 4 und 8. Die Autorinnen und Autoren stehen in der Versionsgeschichte der jeweiligen Seite.

## 2. Software im Paket

| Bestandteil | Herkunft | Lizenz |
|---|---|---|
| llvm-mingw-Laufzeit (libc++, libc++abi, libunwind, compiler-rt), statisch gebunden | https://github.com/mstorsjo/llvm-mingw | Apache License 2.0 mit LLVM Exceptions (Text im Paket unter `licenses/`) |
| mingw-w64-Laufzeit | https://www.mingw-w64.org | ZPL / MIT-ähnlich |

## 3. Externe Assets

Keine. Falls künftig Assets übernommen werden, steht hier je Datei: Datei im Projekt, Quelle (URL), Autor, Lizenz, Abrufdatum, Änderungen.
