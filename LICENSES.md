# LIMINAL – Lizenzen und Quellen (Übersicht)

Stand: 08.11.2026. Das ist keine Rechtsberatung. Die vollständigen Angaben je Version stehen im Versionsordner:
[`LIMINAL V13/LICENSES.md`](https://github.com/AmerAsani/LIMINAL/blob/liminal-v13/LIMINAL%20V13/LICENSES.md) (Branch `liminal-v13`),
[`LIMINAL V12/LICENSES.md`](https://github.com/AmerAsani/LIMINAL/blob/liminal-v12/LIMINAL%20V12/LICENSES.md) (Branch `liminal-v12`),
[`LIMINAL V11/ATTRIBUTION.md`](https://github.com/AmerAsani/LIMINAL/blob/liminal-v11/LIMINAL%20V11/ATTRIBUTION.md) und
[`LIMINAL V11/LICENSE_REVIEW.md`](https://github.com/AmerAsani/LIMINAL/blob/liminal-v11/LIMINAL%20V11/LICENSE_REVIEW.md) (Branch `liminal-v11`).
Namensnennung des Wikis: [`ATTRIBUTION.md`](ATTRIBUTION.md).

## Lizenz des Spiels

- **Für LIMINAL selbst (Code, Modelle, Klänge, Texte) ist noch keine Lizenz gewählt.** Ohne Lizenzdatei gilt: alle Rechte
  vorbehalten; ansehen und auf GitHub forken ist nach den GitHub-Nutzungsbedingungen erlaubt.
- Der Licensing Guide des Backrooms-Wikis (Wikidot) verlangt für Spiele, die auf Wiki-Inhalten aufbauen, CC BY-SA 3.0 oder
  GPLv3 mit vollständigem Quellcode. Ob und wie weit LIMINAL eine Bearbeitung im Sinne der Lizenz ist, ist **nicht geklärt**
  (`LICENSE_REVIEW.md`, Abschnitt 3–6). Die Entscheidung liegt beim Eigentümer des Projekts.

## Fremdes Material

| Material | Wo | Lizenz | Nachweis |
|---|---|---|---|
| Backrooms Wiki (Wikidot): Beschreibungen der Ebenen und Wesen | Grundlage der Ebenen und Wesen ab V11; nur in eigenen Worten, keine Texte, Bilder oder Töne übernommen | [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/) | [`ATTRIBUTION.md`](ATTRIBUTION.md), je Version `LICENSES.md` / `ATTRIBUTION.md` |
| Texturen von [ambientCG](https://ambientcg.com) | ab V12 (`data/textures/`) | [CC0 1.0](https://docs.ambientcg.com/license/) | Liste mit Set, URL und Prüfsumme in `LIMINAL V12/LICENSES.md`, Abschnitt 2 |
| Schrift Inter (nur als Geometrie in Schildern) | ab V12 | SIL Open Font License 1.1 | [`licenses/Inter-OFL-1.1.txt`](licenses/Inter-OFL-1.1.txt) |
| llvm-mingw-Laufzeit (libc++, libc++abi, libunwind, compiler-rt), statisch gebunden | Spielpakete | Apache License 2.0 mit LLVM Exceptions | [`licenses/LLVM-Apache-2.0-with-LLVM-exception.txt`](licenses/LLVM-Apache-2.0-with-LLVM-exception.txt) |
| mingw-w64-Laufzeit, statisch gebunden | Spielpakete | ZPL 2.1 und weitere freie Lizenzen | [`licenses/mingw-w64-runtime.txt`](licenses/mingw-w64-runtime.txt) |
| Netzbibliotheken des Koop-Modus: libdatachannel 0.24.6, libjuice 1.7.4, usrsctp, plog, Mbed TLS 3.6.7, nlohmann/json 3.12.0 | ab V13.3 in `liminal_net.dll` und `liminal_lobby.exe`; Quelltext unverändert in `LIMINAL V13/third_party` | MPL-2.0 (libdatachannel, libjuice), BSD-3-Clause (usrsctp), MIT (plog, nlohmann/json), Apache-2.0 (Mbed TLS) | `LIMINAL V13/LICENSES.md`, Abschnitt 7.1; Lizenztexte in `third_party/` und im Spielpaket unter `licenses/` |
| Eingebettetes Python (nur V2–V4) | `LIMINAL V2–V4/runtime/` | Python Software Foundation License | `runtime/LICENSE.txt` im Versionsordner |

Eigene Arbeit (keine fremden Vorlagen): alle Modelle (Blender-Skripte ab V12, davor im Programmcode), alle Klänge
(Synthese im Programmcode, keine Samples), alle Texte im Spiel, die Karten (Python-Skripte) und alle Bilder in diesem
Repository (Aufnahmen aus dem Spiel). Außer den CC0-Materialtexturen (Fotoscans von Oberflächen wie Ziegel, Teppich, Beton)
gibt es keine fremden Fotos und keine Bilder realer Personen.

Werkzeuge, die nicht im Spielpaket liegen: Blender (GPL-2.0-or-later), CMake (BSD-3-Clause), Ninja (Apache 2.0).
