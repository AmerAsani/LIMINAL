# Selbstkontrolle V13.3 (PROMPT_V13_3.md, Abschnitt 11)

Stand 08.11.2026, Schlussprüfung auf `f45bc4d` plus den Commits des Schlussschritts (Branch `worktree-wf_e3a3c0ee-980-4`).
Den Auftrag habe ich für diese Liste noch einmal gelesen (Abschnitte „Abnahme je Level“, Teil 7 und 11). Jede Zeile:
Punkt, erfüllt ja/nein, Beleg. „Nein“ heißt: nicht erfüllt; dabei steht, warum und ob es eine echte Grenze ist.
Zusammenfassung am Ende.

## A. Punkte aus Abschnitt 11

| Nr. | Punkt | Erfüllt | Beleg |
|---|---|---|---|
| A1 | Alle zehn Level (Level 0 eingeschlossen) haben einen vollständigen `AUFWAND.md`-Eintrag auf gleichem Niveau | ja (nach Nachbesserung) | `AUFWAND.md`: „Level 1 – abgenommen“ (Zeile 12), Level 2 (83), 3 (167), 0 (239), 5 (302), 5.3 (371), 188 (434), 903 (498), 37 (558), 36 (619); je Tabelle a–g, neu gebaute Assets, Bilder, Fehler, Restpunkte. **Befund der Schlussprüfung:** der Eintrag von Level 1 war mit 3 498 Zeichen dünner als der von Level 0 (8 295) – Alarmzeichen nach Auftrag; ergänzt um Asset-Liste, Bilder, Restpunkte und den Nachtrag B-033 aus den vorhandenen Dokumenten (`6041901`), jetzt etwa 7 400 Zeichen (Level 37: 7 494; die anderen 8 300–10 000) |
| A2 | … und erreichen die Qualität bei Details, Raumproportionen, Schildern und Wiederholungsstruktur | ja | Kritikrunden je Ebene (`KRITIK_Level*.md`, 5–7 Runden), `room_metrics.py --check` OK, `signs.py --check` OK, `similarity.py --check` OK (Schlussprüfung 08.11., `REPORT_V13_3.md` Abschnitt 4). Das Urteil „Qualität“ ist ein Bildurteil aus den Runden |
| A3 | Keine Primitive mehr bei Props | ja | `PROPS_Level*.md` je Ebene: alle Klassen neu in Blender (Level 1 Teil A als Code-Modelle mit Varianten in Blender, `models_l1_code.py`); Kontaktbögen `docs/screens/props/<level>/`. Modellprüfung aller 670 Modelle: 0 Fehler (nach B-033) |
| A4 | Wasser und Reflexionen glaubwürdig (Kontaktblätter betrachtet) | ja | Pfützen/klares Wasser/Deckenspiegelung: `docs/screens/props/wasser/` (Vorher/Nachher); Level 37 Becken-Wasser `docs/screens/level37/wasser_vorher_nachher.jpg`, `KRITIK_Level37.md` Runden 2–6; README-Bild `docs/readme_v13/v13_level37_lichtnetz_wasser.jpg`. Grenzen genannt: keine Durchsicht unter Wasser, Lichtnetz als Näherung |
| A5 | Räume messbar großzügiger | ja | `STRUKTURTYP.md` je Ebene (Maße vorher → Ziel), `docs/metrics/<level>.json`, Grenzen `tools/qa/metric_limits.json`; `room_metrics.py --check`: „Raummetrik: OK“ (08.11.) |
| A6 | Struktur einheitlich und wiederkehrend | ja | `similarity.py --check`: „Aehnlichkeit: OK“ (08.11.; z. B. Level 37 S 0,948, Level 36 S 0,862, Ausreißer 10,6 % bei Grenze 20 %) |
| A7 | Schilder sinnvoll | ja | `signs.py --check`: „Schilder: OK“ (08.11.); schilderfreie Ebenen 0 Leuchten mit Piktogramm (E-050); Level 36: 1 Notausgangsschild (Grenze 2) |
| A8 | Metriktests grün | ja | `room_metrics.py --check`, `signs.py --check`, `similarity.py --check` je Exitcode 0 (08.11.) |
| A9 | Arm natürlich, belegt aus der Spielerkamera | ja | `HAND_ANALYSE.md` (Abschnitt „V13.3: Arm“, fünf Runden), `docs/screens/viewmodel/v13_3_*` (Spielerkamera in 12–13 Zuständen), E-032; aktuell aus der Spielerkamera: `docs/readme_v13/v13_level3_taschenlampe_ziegelgang.jpg`, `v13_level2_fenster_am_gangende.jpg` |
| A10 | Jede im Wiki belegte Entity ist gebaut | ja | `ENTITY_ABGLEICH.md` (je Ebene; „umgesetzt“ für alle Wesen; Ausnahmen nach Auftrag: Menschen/Gruppen, Staphylococcus liminalis) |
| A11 | … animiert, vertont | ja | je Wesen `KREATUR_*.md` (Animationen Idle/Wahrnehmen/Jagen/Angriff/Rückzug, Klang `src/audio/synth_*.cpp`); `gameplay_test` 1020/0 mit Prüfungen je Wesen |
| A12 | … im Multiplayer synchronisiert | ja | Rollen und Animationskanäle über `Presence::NetState` (Protokoll v3, Rollen bis 40); `coop_test --quick` 209/0 (Puppen, Wesen beim Host); Einschränkung: nicht jedes Wesen einzeln im Zwei-Prozess-Spiel gesehen (`REPORT_V13_3.md` Abschnitt 9) |
| A13 | … und besteht den Wow-Check | ja | Wow-Check-Satz und mindestens 5 Kritikrunden je `KREATUR_*.md`; Wesen-Läufe der Abnahmen (`docs/screens/<level>/wesen_r*`). Restschwächen offen benannt (`REPORT_V13_3.md` Abschnitt 9). Urteil aus Bildern, kein Test |
| A14 | Multiplayer über eine bewährte Bibliothek mit NAT-Durchdringung/Relay | ja | libdatachannel (ICE/STUN/TURN, DTLS, SCTP) und libjuice, E-033, `MULTIPLAYER_ARCHITEKTUR.md`; `net_transport_test` 90/0 (Wege direkt, NAT, nur Relay) |
| A15 | Multiplayer-Tests grün | nein | `net_protocol_test` 72/0, `net_transport_test` 90/0, `coop_test --quick` 209/0, Langlauf 60 min Lauf 1 fehlgeschlagen (10 Prüfungen, 2 Fehler: ein Gast nach 55 min getrennt), Lauf 2 OK (10/0) – B-036 offen, Einzelheiten `REPORT_V13_3.md` Abschnitt 6 |
| A16 | `MULTIPLAYER_SETUP.md` liegt vor | ja | `MULTIPLAYER_SETUP.md` |
| A17 | Audio-Bug reproduziert, behoben und per Stresstest belegt | ja | `AUDIO_BUG.md` (Ursache B-017, Fix, Watchdog; Stresstests 1 000 Starts, 220 Ebenenwechsel); Schlussprüfung: `audio_test` 121/0, `audio_device_test` 66/0 |
| A18 | Alle Tests grün | ja (ohne den Langlauf, siehe A15) | `REPORT_V13_3.md` Abschnitt 4 (gameplay 1020/0, audio 121/0, audio_device 66/0, net 72/0, Transport 90/0, coop --quick 209/0, worldgen 29 658/0, supply OK, Raummetrik/Schilder/Ähnlichkeit OK, Modellprüfung 670/0, kein Netzwerk) |
| A19 | V10 unverändert | ja | `git diff --quiet main HEAD -- "LIMINAL V10"` ohne Abweichung (08.11.) |
| A20 | V12 unverändert | ja | `freeze.py "LIMINAL V12" "Sicherungen\v12_frozen_2026-10-26.txt" --verify`: „Manifest 25550, veraendert 0, fehlend 0, neu 0“ (08.11.); `git diff --quiet liminal-v12 HEAD -- "LIMINAL V12"` |
| A21 | Upload verifiziert oder Befehle dokumentiert | nein | **Folgt im Hauptstrang** (dieser Schritt pusht nach Auftrag nicht). Geplante Schritte in `GITHUB_UPLOAD.md`, Abschnitt „Offen“ (Branch `liminal-v13`, Tag `v13.3`, Bau-Test aus frischem Klon, Release, README mit `docs/readme_v13/`). Keine technische Grenze, sondern Arbeitsteilung |
| A22 | `REPORT_V13_3.md` erstellt (Änderungen, getestete Funktionen, bekannte Probleme, Restrisiken, Stand pro Level und Entity) | ja | `REPORT_V13_3.md` Abschnitte 2, 3, 8–12 |

Teil 7 (Testen), soweit nicht oben enthalten:

| Nr. | Punkt | Erfüllt | Beleg |
|---|---|---|---|
| T1 | Tests erweitert um neue Props, Entities, Raummetriken, Schilderzählung, Ähnlichkeitsmessung | ja | `gameplay_test` (Wesen je Ebene, Halt der Ausstattung 10 690 Objekte), `tools/qa/room_metrics.py`, `signs.py`, `similarity.py`, `validate_models.py`, `smoke.py` |
| T2 | Smoke-Test aller zehn Level mit Frametimes und Speichermessung | ja | `docs/metrics/smoke_<level>.json` (08.11.), Tabelle `REPORT_V13_3.md` Abschnitt 5: alle zehn OK, GPU 1,97–4,88 ms im Mittel, RAM-Spitze 1 413–1 560 MB, 0 Fehler |
| T3 | Performance-Probleme durch neue Details behoben, ohne sichtbaren Qualitätsverlust | ja | alle Ebenen unter 16,7 ms GPU im Mittel; höchste Spitze Level 3 14,87 ms (Fertigungsstraße, E-046) |
| T4 | Fehler in `BUGLISTE.md`, bestätigt getrennt von Vermutungen | ja | `BUGLISTE.md`, Abschnitt „Schlussprüfung (V13.3, 08.11.2026)“: B-030, B-033 behoben; B-031, B-032, B-035, B-036 offen (bestätigt); B-034 Vermutung |

## B. Abnahmeschranken a–g je Ebene

a) `STRUKTURTYP.md`-Eintrag, Raummetrik grün · b) `PROPS_<Level>.md` vollständig, Klassen neu gebaut, Kontaktbögen ·
c) alle Wesen der Ebene im Spiel und spielbar · d) mindestens 5 Kritikrunden mit Bildern · e) Laufweg ≥ 200 m und ≥ 15
Räume/Bereiche, alle paar Meter ein Bild · f) Objektvalidierung, Schilder, Smoke grün; Bildzeiten und Speicher gemessen ·
g) `AUFWAND.md`-Eintrag.

Gemeinsame Belege der Schlussprüfung (08.11.): Raummetrik OK, Schilder OK, Ähnlichkeit OK, Modellprüfung 670/0,
Smoke-Test je Ebene OK (`docs/metrics/smoke_<level>.json`), `gameplay_test` 1020/0.

| Ebene | a | b | c | d | e | f | g |
|---|---|---|---|---|---|---|---|
| Level 0 | ja – `STRUKTURTYP.md` „Level 0“, Raummetrik OK | ja – `PROPS_Level0.md` (7 Klassen, `models_l0.py`), `docs/screens/props/level0/` | ja – `ENTITY_ABGLEICH.md` Level 0 (Gestalt, Halluzinationen) | ja – 5 Runden, `KRITIK_Level0.md` | ja – 205 m / 24 Räume, 52 Bilder (`docs/screens/level0/wegbilder_r2_*`…`_r5_*`) | ja – Modelle 0 Fehler, Schilder 0, Smoke GPU 1,97 ms, RAM 1 121 MB | ja – `AUFWAND.md` Z. 239 |
| Level 1 | ja – `STRUKTURTYP.md` „Level 1“, Raummetrik OK | ja – `PROPS_Level1.md`, `_B`, `_Sektoren` | ja – Level 1 (16 Wesen und Erscheinungen) | ja – 7 Runden, `KRITIK_Level1.md` | ja – 836 m / 18 Räume, 209 Bilder | ja – **nach Nachbesserung**: Wesen-Modelle der Ebene waren in der Abnahme nicht geprüft (B-033), jetzt 0 Fehler (`8dcd0d4`); Schilder 6 (Grenze 12); Smoke GPU 4,55 ms, RAM 1 338 MB | ja (nach Nachbesserung) – `AUFWAND.md` Z. 12, ergänzt 08.11. (A1) |
| Level 2 | ja – „Level 2“, Raummetrik OK | ja – `PROPS_Level2.md` (28 Klassen, 41 Modelle) | ja – Level 2 (17 Wesen) | ja – 7 Runden | ja – 208 m / 28 Räume, 52 Bilder | ja – Modelle 0, Schilder 0, Smoke GPU 2,14 ms, RAM 1 221 MB | ja – Z. 83 |
| Level 3 | ja – „Level 3“, Raummetrik OK | ja – `PROPS_Level3.md` (24 Klassen, 38 Modelle) | ja – Level 3 (14 Einträge, Red Knight) | ja – 7 Runden | ja – 201 m / 27 Räume, 51 Bilder | ja – Modelle 0, Schilder 0, Smoke GPU 4,88 ms (max 14,87), RAM 1 277 MB | ja – Z. 167 |
| Level 5 | ja – „Level 5“, Raummetrik OK (begründete Ausnahmen) | ja – `PROPS_Level5.md` (21 Klassen) | ja – Level 5 (12 Einträge, Samantha) | ja – 5 Runden | ja – 687 m / 15 Räume, 172 Bilder | ja – Modelle 0, Schilder 0, Smoke GPU 2,78 ms, RAM 1 175 MB | ja – Z. 302 |
| Level 5.3 | ja – „Level 5.3“, Raummetrik OK | ja – `PROPS_Level5_3.md` (16 Klassen) | ja – Level 5.3 (Hausherr, Wissen) | ja – 5 Runden | ja – 202 m / 20 Räume, 51 Bilder | ja – Modelle 0, Schilder 0, Smoke GPU 3,78 ms, RAM 1 301 MB | ja – Z. 371 |
| Level 188 | ja – „Level 188“, Raummetrik OK | ja – `PROPS_Level188.md` (14 Klassen) | ja – Level 188 (Horcher, Wurzelpistole, …) | ja – 7 Runden | **nein** – 207 m ja, aber 7 statt 15 Räume: die Ebene hat nur 7 erreichbare Räume (Wiki: ein Hof mit Gängen); alle besucht, mehrfach und aus anderen Richtungen. Echte Grenze, E-036, Raumzahl aus `room_metrics.py` | ja – Modelle 0, Schilder 0, Smoke GPU 3,34 ms, RAM 1 109 MB | ja – Z. 434 |
| Level 903 | ja – „Level 903“, Raummetrik OK | ja – `PROPS_Level903.md` (13 Klassen) | ja – Level 903 (Metro, Facelings) | ja – 6 Runden | **nein** – 294 m ja, aber 1 statt 15 Räume: die Ebene ist ein einziger Bahnsteig (Wiki). Echte Grenze, E-036 | ja – Modelle 0, Schilder 0, Smoke GPU 2,41 ms, RAM 1 102 MB | ja – Z. 498 |
| Level 37 | ja – „Level 37“, Raummetrik OK | ja – `PROPS_Level37.md` (einzige Klasse Metro-Wagen; Seite kennt keine Gegenstände) | ja – Seite nennt keine Wesen | ja – 6 Runden | ja – 203 m / 21 Räume, 51 Bilder | ja – Modelle 0, Schilder 0, Smoke GPU 2,72 ms, RAM 1 079 MB | ja – Z. 558 |
| Level 36 | ja – „Level 36“, Raummetrik OK | ja – `PROPS_Level36.md` (34 Klassen) | ja – Seite nennt keine Wesen (Durchsage als Klang) | ja – 6 Runden | ja – 481 m / 15 Räume, 121 Bilder | ja – Modelle 0, Schilder 1 (Grenze 2), Smoke GPU 3,27 ms, RAM 1 141 MB | ja – Z. 619 |

## C. Zusammenfassung

Geprüft: 96 Punkte (22 aus Abschnitt 11, 4 aus Teil 7, 70 Schranken a–g). **92 ja, 4 nein.**

Die vier „nein“:
1. **A15 Multiplayer-Tests grün:** der 60-min-Langlauf schlug in Lauf 1 fehl (ein Gast nach 55 min getrennt, B-036); Lauf 2 ohne Last bestand. Alle anderen Mehrspieler-Tests grün (Protokoll 72/0, Transport 90/0, Koop 209/0). Nicht behoben, weil die Ursache nicht bestimmt ist und sich im zweiten Lauf nicht zeigte; ein Eingriff in SCTP-Wartezeiten oder Stille-Grenze ohne Nachweis wäre geraten. Diagnose ist eingebaut (`LIMINAL_NET_LOG=1`); weitere Langläufe nötig.
2. **A21 Upload verifiziert oder Befehle dokumentiert:** folgt im Hauptstrang (in diesem Schritt nach Auftrag kein Push). Schritte in `GITHUB_UPLOAD.md`, Abschnitt „Offen“.
3. **Level 188, Schranke e:** 207 m erfüllt, 15 Räume nicht – die Ebene hat nur 7 erreichbare Räume (Wiki: ein Hof mit Gängen). Echte Grenze, E-036.
4. **Level 903, Schranke e:** 294 m erfüllt, 15 Räume nicht – die Ebene ist ein einziger Bahnsteig (Wiki). Echte Grenze, E-036.

In der Schlussprüfung nachgebessert (jetzt „ja“): A1 (`AUFWAND.md` Level 1 war dünner als Level 0), Level 1 f (31 Modelle der Wesen ungeprüft, B-033). Weitere Funde ohne Einfluss auf eine Schranke: B-030 (behoben), B-031, B-032, B-035 (offen), B-034 (Vermutung).
