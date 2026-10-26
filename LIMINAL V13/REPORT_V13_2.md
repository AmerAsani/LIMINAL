# LIMINAL V13.2 – Abschlussbericht der Fortsetzung

Stand 27.10.2026, Branch `liminal-v13`. Auftrag: „LIMINAL – V13 FORTSETZUNG: OFFENE PUNKTE, MOTTENNEST, ENTITY-ABGLEICH, MULTIPLAYER, GITHUB-UPLOAD“.
Dieser Bericht trennt, was fertig und geprüft ist, von dem, was offen ist. Was nur teilweise erfüllt ist, steht so da.

---

## 1. Ergebnis in Kürze

- **Bewegung der Wesen:** Hound, Smiler, Clump, Gestalt, Deathmoth und das neue Nest haben eigene Zustände (Ruhe mit Atem und Gewichtsverlagerung, Gehen/Rennen, Wahrnehmen, Lichtreaktion, Angriff, Übergänge), je 3–5 Kritikrunden mit Bildfolgen (`ANIMATIONEN.md`). Dual-Quaternion-Skinning für Menschen (B-001).
- **Modelle:** das Mottennest von Grund auf neu (fünf Runden, E-026/E-029); ehrliche Prüfung der übrigen Wesen aus denselben Kameras (`KREATUREN_ANALYSE.md` Abschnitt 6) – der Smiler-Kopf wurde sichtbar verbessert, ein Versuch am Bündel nicht übernommen (nicht klar besser).
- **Fehler:** B-001 (Ärmel am gebeugten Ellbogen) und B-002 (Ladespitze 3,1 GB → 0,68 GB) behoben und belegt; dazu B-005 bis B-016 (Koop, Licht, Größe, Material, Gang).
- **Entity-Abgleich:** `ENTITY_ABGLEICH.md` gegen die Wikidot-Seiten aller zehn Ebenen und die Entity-Seiten. Fünf fehlende Wesen nach Priorität umgesetzt – **Faceling** (vier Verhaltensweisen je Ebene, fünf Varianten), **Fensterwesen** (Level 188), **Skin-Stealer** (wahre Gestalt und getarnt), **Wretch** (träge, schnell, flehend), **Death Rat** (mit Fell, haarlos, Schrei-Ereignis) – je mit Briefing, Blender-Modell, Skelett, eigener Bewegung, Verhalten nach dem Wiki, Klang, Gleichgewicht, Mehrspieler und Tests. Neun weitere Wesen (Rang 6–14) sind noch nicht gebaut.
- **Mehrspieler:** Koop für bis zu vier Spieler als **Beta** im Spiel: verschlüsselter Transport (ENet + Windows-Kryptografie), Host entscheidet alles, Gäste senden nur Eingaben, Prüfschicht für jede Nachricht. Getestet mit Bots über einen Netzstörer, Angriffen, Fuzzing (auch unter Sanitizern), Reise durch alle zehn Ebenen, zwei echten Spielprozessen, 60-min-Langlauf. **Nicht „vollkommen sicher“** – Restrisiken in Abschnitt 6.
- **GitHub:** nicht hochgeladen. Das vorhandene `origin` ist öffentlich, ein privates Repository ließ sich ohne Anmeldung nicht anlegen; alles ist lokal vorbereitet, die Befehle stehen in `../GITHUB_UPLOAD.md` (E-025).

## 2. Abschlusskriterien

| Kriterium des Auftrags | Stand | Beleg |
|---|---|---|
| Kreaturen haben eigene, passende Animationen … belegt durch Screenshots und Frame-Reihen | **erfüllt** | `ANIMATIONEN.md`; Bildfolgen je Zustand (`docs/screens/anim/`, `docs/screens/<wesen>/`); Hound, Smiler, Clump, Deathmoth je 3, Gestalt 4 Runden; neue Wesen eigene Runden |
| … und verbesserte Modelle | **teilweise** | Nest neu (5 Runden); Smiler-Kopf verbessert (Vorher/Nachher aus 1 m); Bündel: Versuch verworfen (nicht klar besser); Hound, Gestalt, Deathmoth: geprüft, keine Änderung (`KREATUREN_ANALYSE.md` 6). „Sculpt, Topologie, UVs“: die Wesen entstehen weiter als Distanzfelder mit Neuvernetzung (E-021); Detail kommt aus Verschiebungen und Texturen, nicht aus Pinselarbeit |
| Mottennest neu gestaltet (oder begründet entfernt) | **erfüllt** | Wiki-Beleg geprüft (E-026: belegt), Neuentwurf in 5 Runden (E-029, `docs/screens/nest/`) |
| `ENTITY_ABGLEICH.md` liegt vor | **erfüllt** | zehn Ebenen, Entity-Übersicht, Entity-Seiten mit Revisionen |
| … jede belegte, sinnvoll umsetzbare Entity ist im Spiel oder begründet ausgenommen | **teilweise** | Rang 1–5 umgesetzt (E-027, E-031); bewusst weggelassen mit Grund: Menschen und Gruppen, Krankheitserreger, Unbestätigtes, Ausgestorbenes, nur Erzähltes. **Noch offen:** Duller, Nguithr'xurh, Growler, Arms, Wrangler, Cannibal Cuisine, purpurne Wesen, Woodlins/Watchers/Ants, Samantha (Rang 6–14) und das Fensterwesen in Level 2 – Grund: Aufwand je Wesen (volle Behandlung); Reihenfolge festgelegt |
| B-001 und B-002 behoben und gemessen/belegt | **erfüllt** | `BUGLISTE.md` (Nachweise), Test in `gameplay_test`; Ladespitze je Ebene gemessen |
| Multiplayer gebaut, getestet, aktiviert (Beta) oder begründet deaktiviert; Validator eingebunden; Tests dokumentiert | **erfüllt** (Beta aktiv) | `MULTIPLAYER_ARCHITEKTUR.md`, `MULTIPLAYER_ENTSCHEIDUNG.md`, `MULTIPLAYER_BEDROHUNGSMODELL.md` (Stand 8), `MULTIPLAYER_TESTS.md`; Prüfschicht `src/net/coop_proto.*` in Host und Gast |
| Alle bisherigen Tests grün, V12 und V10 unverändert, Build startet und läuft | **erfüllt** | Abschnitt 4; V12-Manifest 25 550 Dateien, 0 verändert/fehlend/neu; V10 ohne Änderung im Arbeitsbaum und in den Commits dieses Branches |
| V11, V12, V13 verifiziert auf GitHub (privat) oder mit dokumentierten Befehlen vorbereitet | **vorbereitet** | `../GITHUB_UPLOAD.md`: Branches `liminal-v11`/`-v12`/`-v13`, Tags `v11`/`v12`/`v13`, Prüfungen (Geheimnisse, Pfade, Größe, Lizenzen), Befehle |
| `REPORT_V13_2.md` | erfüllt | dieses Dokument |
| Arbeitsweise: keine Rückfragen, Entscheidungen in `DECISIONS.md`, Commit je Teilschritt, `PROGRESS.md` | erfüllt | E-025 bis E-031; Commits in Abschnitt 3 |
| Lizenzen, Wiki nur in eigenen Worten, keine Kopien anderer Backrooms-Spiele, keine Platzhalter | erfüllt | `LICENSES.md` (ENet als einzige neue Fremdquelle); Briefings mit Quelle, Autoren und CC BY-SA 3.0; keine Wiki-Bilder, keine Texturen aus anderen Spielen |

## 3. Änderungen (Commits dieser Fortsetzung)

| Commit | Inhalt |
|---|---|
| `9596202` | GitHub-Sicherung vorbereitet: README, `.gitignore`, `GITHUB_UPLOAD.md`, E-025 (dazu Branches `liminal-v11`, `liminal-v12`, Tags `v11`, `v12`) |
| `196142c` | Entity-Abgleich (zehn Ebenen, Entity-Seiten), Nest belegt (E-026), Reihenfolge (E-027), Smiler reagiert auf Lärm |
| `422c281` | B-002: Texturen Ebene für Ebene, Ladespitze 3,1 → 0,68 GB |
| `2af425e` | B-001: Ruhepose 54°, Dual Quaternions, Lampenhaltung von außen |
| `c0e0efe` … `0cc7464` | Bewegung: Hound, Smiler, Clump, Gestalt, Deathmoth |
| `3fd0e3b` | Mehrspieler Stufe 0: Transport (`liminal_net.dll`), Transporttest 52/52 |
| `bf8e662` | Mottennest neu (fünf Runden) |
| `933fba9` | Koop (Beta): Protokoll, Host und Gast, Wesen für mehrere Spieler, Tod und Zusehen, Wiederverbinden, Koop-Stand, Menüs; Tests |
| `4734414` | Neue Wesen (Grundlage): Modelle, Skelette, Bewegung, Verhalten, Klang, Ebenen; Fensterwesen; Protokoll v3 |
| `118dd4a` | Briefings mit Kritikrunden der neuen Wesen, Abgleich, E-031, B-012 bis B-016; Koop-Klang im Menü, Langlauf |
| `e72ac05` | Modellier-Analyse der übrigen Wesen |
| `46e1d3e` | Smiler-Kopf verbessert; Bündel-Versuch verworfen; Koop-Bildzeiten |
| (dieser) | Bericht, README, `GITHUB_UPLOAD.md`, Tag `v13` |

## 4. Geprüfte Funktionen

| Prüfung | Ergebnis |
|---|---|
| `gameplay_test` | **OK: 446 Prüfungen, 0 Fehler** (V13: 357) – darunter 65 neue für die neuen Wesen: friedliche Facelings winken, reizbare reagieren auf die Lampe und schlagen leicht, bewaffnete bemerken einen erst nah, das rosa Kleid ist tödlich und lässt im Hof ab; Skin-Stealer satt harmlos, gereizt tödlich, hungrig folgt er, spricht nach, hält inne, ruhiges Zurückweichen hilft; Wretch jagt und schlägt schwer, aber nicht sofort tödlich, flehende greifen nicht an, der schnelle faucht hoch; Ratten fliehen und quieken, Schrei-Ereignis mit Flackern; drei Fensterwesen flüstern, greifen zu nah Stehende, wer zurücktritt, entkommt |
| `audio_test` | OK: 106/0 (alle Klänge gültig, nicht übersteuert, Lautheit; 34 neue Klänge) |
| `worldgen_test` (V4-Referenz) | OK: 29 658/0 |
| `supply_balance_test` | OK (0 Fehler) |
| `net_protocol_test` | OK: 72/0 |
| `net_transport_test` | OK: 52/0 |
| `coop_test` (alle Teile) | **OK: 169/0**; Netz-Fuzzing 300 s 2/0; Koop-Stand 5/5 |
| Koop-Langlauf | 60 min 9/0; 30 min mit Speicherproben 9/0 (108 → 224 → 214 MB, flacht ab) |
| Koop, zwei echte Prozesse | Beitritt, Figuren, Lampen, Namen, Beenden ohne Absturz; Bildzeiten: Spiellogik 0,02–0,08 ms je Bild, 60 Hz bei Host und Gast |
| Sanitizer (ASan/UBSan) | Prüfschicht 12 000 000 Fuzz-Fälle, Transport 52/0, keine Meldung |
| `tools/check_no_network.py` | SAUBER (Spiel ohne Netzwerkcode und ohne statischen Import der Netz-DLL; die DLL ohne HTTP-/Download-Bibliothek, genau 10 Exporte) |
| V12 eingefroren | Manifest 25 550 Dateien, 0 verändert, 0 fehlend, 0 neu |
| V10 | keine Änderung (Arbeitsbaum, Commits von `main` bis `liminal-v13`) |
| Bilder im Spiel | je neues Wesen 3–5 Kritikrunden in seinen Ebenen (`docs/screens/<wesen>/`), Bewegungsreihen je Zustand |

## 5. Bekannte Probleme

| Nr. | Problem | Stand |
|---|---|---|
| B-012 | Koop-Langlauf 60 min: vier Sprünge über 2 m beim Abgleich, einer 89,9 m | offen, Ursache nicht untersucht (im 30-min-Lauf keiner) |
| – | Nahe an der Lampe wirkt dunkler Stoff fast weiß (Abstandsquadrat, Belichtung) | geprüft (Albedo und Normalen richtig), nicht geändert |
| – | Bündel: Glieder wirken aus der Mitte wie Puppenteile; Hound: Hände und Zähne aus der Nähe | Analyse, offen für V14 |
| – | Skin-Stealer: Nachsprechen sind Silben ohne Worte; „Ansprechen entlarvt sie“ (Level 1) fehlt – der Spieler hat keine Stimme | bewusst so (E-031) |
| – | Wretch: keine Heilung mit Mandelwasser, keine Ansteckung durch den Schlamm, keine Waffen; Ratten: keine Fallen, keine Wasserschwein-Größe in Level 3 | nicht umgesetzt (Briefings) |
| – | Fensterwesen in Level 2 (Rohrgänge ohne Fenster) | nicht umgesetzt |
| – | Wesen Rang 6–14 (Duller, Nguithr'xurh, Growler, Arms, Wrangler u. a.) | nicht gebaut |
| B-001 | Ego-Haltung bei 161° Beugung: 48 umgeklappte Dreiecke in der (verdeckten) Ellbogenbeuge | innerhalb der Testgrenze, verdeckt |

## 6. Offene Fragen und Restrisiken

- **Lizenz des Spiels:** weiter ungeklärt (Wiki-Inhalte CC BY-SA 3.0; der Licensing Guide knüpft Spiele an CC BY-SA 3.0 oder GPLv3). Deshalb kein öffentlicher Upload.
- **GitHub:** Soll `origin` (öffentlich, V1–V10) bleiben, wie es ist, und ein zweites, privates Repository (`liminal-privat`; `liminal` ist wegen `LIMINAL` belegt) dazukommen? Befehle stehen bereit.
- **Mehrspieler (Beta) – Restrisiken:** Parserfehler sind verringert, nicht ausgeschlossen; Spiel über das echte Internet mit NAT nur nachgestellt; der Host ist vertrauenswürdig gesetzt (er kann Werte der Gäste setzen); ENet selbst ist unverschlüsselt (Beitritte lassen sich kurz verzögern); kein Vorwärtsgeheimnis; der Spielername steht vor dem Schlüsseltausch im Klartext; Fuzzing über den echten Netzweg lief Minuten, nicht Stunden; keine unabhängige Durchsicht. Einzelheiten: `MULTIPLAYER_BEDROHUNGSMODELL.md` Abschnitte 7 und 8.
- **Neue Wesen:** Wo das Wiki schweigt, sind Auslöser und Zahlen eigene Auslegung (E-031, je Briefing markiert); zwei Entity-Seiten (Wretch, Death Rat) sind im Wiki als „wird überarbeitet“ markiert und können sich ändern.

## 7. Befehle

```
# Bauen (aus dem Ordner LIMINAL V13; kopiert auch data/ nach build/Release/bin)
export PATH="$PWD/.toolchain/llvm-mingw/bin:$PWD/.toolchain/cmake/bin:$PWD/.toolchain/ninja:$PATH"
cmake --build build/Release

# Tests (aus build/Release/bin)
../tests/gameplay_test.exe data
../tests/audio_test.exe data
../tests/supply_balance_test.exe data
../tests/worldgen_test.exe data/legacy/levels/level0.json ../../../tests/data/v4_reference.json
../tests/net_protocol_test.exe data
../tests/net_transport_test.exe data
../tests/coop_test.exe data                       # alle Teile (etwa 15 min); --quick, --only <teil>, --long <s>

# Kein Netzwerkcode im Spiel
python tools/check_no_network.py

# Ein Wesen neu bauen (danach cmake --build, sonst lädt das Spiel die alte Fassung)
.toolchain/blender/blender-4.5.14-windows-x64/blender.exe -b --factory-startup --python tools/blender/creatures/build.py -- <wesen> --preview <ordner>
#   <wesen>: hound smiler clump gestalt moth nest faceling faceling_evo faceling_dress faceling_b faceling_c
#            skinstealer skinstealer_skin wretch rat rat_bald window
# Skelette nach Änderungen in src/anim/creature.cpp neu schreiben (Zeilenenden CRLF wiederherstellen)
build/Release/bin/LiminalGame.exe --dump-skeletons tools/blender/creatures/skeletons.json

# Wesen im Spiel ansehen (Skript): presence <wesen>; near <wesen> <abstand>; figure <wesen> <abstand>; figstate <feld> <wert>
```
