# LIMINAL V13.3 – Bericht

Stand 08.11.2026, Schlussprüfung auf dem Stand nach der letzten Abnahme (`f45bc4d`, Branch des Schlussschritts
`worktree-wf_e3a3c0ee-980-4`). Auftrag: `PROMPT_V13_3.md`. Punkt für Punkt mit Belegen: `SELBSTKONTROLLE.md`.
Was hier „geprüft“ heißt, steht jeweils dabei (Test mit Zahl, Bild, Messung). Was nicht geprüft ist, steht unter
„Nicht geprüft“ und „Restrisiken“.

## 1. Kurzfassung

- **Alle zehn Ebenen abgenommen** (Schranken a–g, `AUFWAND.md`): Level 1 (01.11.), 2 und 3 (02.11.), 0, 5, 5.3 und 188
  (03.11.), 903, 37 und 36 (08.11.).
- **Tests der Schlussprüfung** grün (Abschnitt 4), Smoke-Test aller zehn Ebenen ohne Fehler (Abschnitt 5).
- **Koop-Langlauf 60 min: Lauf 1 fehlgeschlagen (10 Prüfungen, 2 Fehler: ein Gast nach 55 min getrennt), Lauf 2 OK (10/0)** (Abschnitt 6, B-036).
- **V10 und V12 unverändert** (Abschnitt 7).
- **In der Schlussprüfung gefunden und behoben:** 31 Modelle der Level-1-Wesen mit entarteten oder gedrehten Dreiecken
  (B-033), Port-Prüfung im Koop-Test (B-030). **Offen:** B-036 (Langlauf), B-012, B-024, B-031, B-032, B-035, Vermutung B-034 (Abschnitt 10).
- **README-Bilder V13:** 16 Bilder in `docs/readme_v13/` (Liste mit Begründung `docs/readme_v13/AUSWAHL.md`).
- **Upload von V13:** folgt im Hauptstrang (`GITHUB_UPLOAD.md`, Abschnitt „Offen“); in diesem Schritt nichts gepusht.

## 2. Änderungen in V13.3

| Teil | Inhalt | Beleg |
|---|---|---|
| 1 Ausstattung | Alle Objektklassen aller zehn Ebenen neu in Blender (Code-Modelle und GLB, Varianten `name@N`), Kontaktbögen 4 Winkel × 50 cm/2 m × Ebenenlicht/Lampe; Pfützen, klares Wasser, Deckenspiegelung im SSR; Glanz und Metall je Material | `PROPS_Level*.md`, `docs/screens/props/`, `tools/qa/propsheet.py` |
| 2 Struktur | Strukturtyp je Ebene nach dem Wiki, Raummetrik (Decke, Gänge, Flächen, Hierarchie, Sichtachsen, Stützen, Möbelabstände), Schilderzählung, Ähnlichkeit; Karten großzügiger und einheitlicher | `STRUKTURTYP.md`, `BIOM_AUDIT.md`, `docs/metrics/<level>.json`, E-035, E-036 |
| 3 Arm | Armkette, Handgelenk, gedrehte Faust, Ärmel; Belege aus der Spielerkamera | `HAND_ANALYSE.md`, `docs/screens/viewmodel/v13_3_*`, E-032 |
| 4 Wesen | Jedes Wesen der zehn Wiki-Seiten im Spiel (Ausnahmen nach Auftrag: Menschen, Erreger ohne Körper), je Wesen Briefing mit Wow-Check und mindestens 5 Kritikrunden | `ENTITY_ABGLEICH.md`, `KREATUR_*.md`, E-038 bis E-040, E-044, E-045, E-047, E-049 |
| 5 Mehrspieler | Lobby mit Beitrittscode, libdatachannel (ICE/STUN/TURN, DTLS, SCTP), Relay, Spielprotokoll v3; kein Beta-Label mehr | `MULTIPLAYER_SETUP.md`, `MULTIPLAYER_TESTS.md` Abschnitt 7, E-033 |
| 6 Audio | Ursache des Verstummens (B-017: Stimmen nie freigegeben), Unterläufe, Neuverbinden, Watchdog mit Stille-Erkennung, Stresstests | `AUDIO_BUG.md`, B-017 bis B-024, E-042 |
| 7 Tests | Tests erweitert (Wesen, Halt der Ausstattung, Raummetrik, Schilder, Ähnlichkeit, Smoke je Ebene, Modellprüfung) | Abschnitte 4 und 5 |
| Abnahme | Je Ebene 5–7 Kritikrunden, Laufweg ≥ 200 m (bei 188/903 alle erreichbaren Räume, E-036), Prüfungen, `AUFWAND.md`-Eintrag | `KRITIK_Level*.md`, `AUFWAND.md`, E-046, E-048, E-051 |
| Schluss | Modellprüfung über alle 670 Modelle (B-033), Port-Prüfung im Koop-Test (B-030), Diagnose der Trennungsgründe im Transport (`LIMINAL_NET_LOG=1`, B-036), `AUFWAND.md` Level 1 ergänzt, README-Bilder V13, dieser Bericht | E-052 |

## 3. Getestete Funktionen

- **Spiellogik** (`gameplay_test`, 1020 Prüfungen): Wesen jeder Ebene (Auftreten, Kernverhalten, Licht/Lärm, Schaden,
  Rückzug), Halt der Ausstattung (10 690 Objekte), Ebenenwechsel, Vorräte, Speichern/Laden, Fundstücke.
- **Klang** (`audio_test` 121, `audio_device_test` mit echtem WASAPI-Gerät): Stimmen, Watchdog, Stille-Erkennung,
  Geräte- und Fehlerpfade, Start-Stresstest.
- **Mehrspieler:** Protokoll (`net_protocol_test` 72), Transport mit Lobby, Codes, Relay, TLS, Angriffen
  (`net_transport_test` 90), Koop mit Bots über den Netzstörer, Reise durch alle Ebenen, Angriffe, Tod, Wiederverbinden
  (`coop_test --quick`), Langlauf 60 min; zwei echte Spielprozesse (README-Koop-Bild, drei Läufe).
- **Welt:** V4-Referenz (`worldgen_test` 29 658), Versorgung (`supply_balance_test`), Raummetrik, Schilder, Ähnlichkeit.
- **Darstellung und Leistung:** Smoke-Test je Ebene (24 Stationen, Bildzeiten, Speicher), Modellprüfung aller Modelle,
  kein Netzwerk im Spiel ohne DLL (`check_no_network.py`).

## 4. Tests der Schlussprüfung (08.11.2026, `build/Release`, Basis `f45bc4d`)

| Test | Ergebnis |
|---|---|
| `gameplay_test data` | OK: 1020 Prüfungen, 0 Fehler |
| `audio_test` | OK: 121 Prüfungen, 0 Fehler |
| `audio_device_test` (echtes Gerät) | OK: 66 Prüfungen, 0 Fehler (319 s; Normalbetrieb 20 s: 0 Stille-Alarme, 0 Neustarts, 0 Unterläufe, Mischen max 0,78 ms) |
| `net_protocol_test` | OK: 72 Prüfungen, 0 Fehler |
| `net_transport_test` | OK: 90 Prüfungen, 0 Fehler (77 s) |
| `coop_test data --quick` | OK: 209 Prüfungen, 0 Fehler (allein, nach B-030). Der erste Lauf neben dem Langlauf: 200 Prüfungen, 1 Fehler („STUN/TURN-Server startet nicht“, Port-Kollision im Test, B-030). Dazu zwei Relay-Teile gleichzeitig: je 10/0 |
| `worldgen_test` (V4-Referenz) | OK: 29 658 Prüfungen, 0 Fehler |
| `supply_balance_test data` | OK (0 Fehler) |
| `room_metrics.py --check` | Raummetrik: OK (alle zehn Ebenen) |
| `signs.py --check` | Schilder: OK (schilderfreie Ebenen 0 Leuchten mit Piktogramm; Level 1 und 36 unter ihrer Grenze) |
| `similarity.py --check` | Ähnlichkeit: OK |
| `validate_models.py` (alle 670 Modelle) | zuerst 31 mit Fehlern (B-033), nach `repair_glb.py` **670 geprüft, 0 Fehler** |
| `check_no_network.py` | SAUBER |

## 5. Smoke-Test aller zehn Ebenen

`tools/qa/smoke.py <level>`: Qualität Hoch, 1600 × 900, 24 Stationen entlang des Laufwegs, je Station `perf`. Gemessen
nacheinander auf einem Rechner, während im Hintergrund der Koop-Langlauf lief (CPU-Last; die GPU war frei). Grenze:
GPU im Mittel ≤ 16,7 ms, RAM ≤ 4 GB, 0 Fehler im Protokoll. Die Grafik-CPU-Zeit von etwa 8 ms ist das Warten auf den
Bildtakt.

| Ebene | GPU Mittel (max) ms | Spiel ms | Dreiecke max | VRAM MB | RAM MB (Spitze) | Fehler |
|---|---|---|---|---|---|---|
| Level 0 | 1,97 (2,33) | 0,03 | 0,53 Mio. | 1217 | 1121 (1413) | 0 |
| Level 1 | 4,55 (6,59) | 0,54 | 1,91 Mio. | 1419 | 1338 (1422) | 0 |
| Level 2 | 2,14 (2,61) | 0,04 | 2,31 Mio. | 1330 | 1221 (1419) | 0 |
| Level 3 | 4,88 (14,87) | 0,06 | 2,15 Mio. | 1318 | 1277 (1526) | 0 |
| Level 5 | 2,78 (3,17) | 0,12 | 1,46 Mio. | 1060 | 1175 (1560) | 0 |
| Level 5.3 | 3,78 (6,13) | 0,03 | 2,65 Mio. | 1386 | 1301 (1518) | 0 |
| Level 188 | 3,34 (4,55) | 0,04 | 0,18 Mio. | 1196 | 1109 (1425) | 0 |
| Level 903 | 2,41 (3,41) | 0,03 | 0,35 Mio. | 1208 | 1102 (1421) | 0 |
| Level 37 | 2,72 (3,18) | 0,03 | 0,09 Mio. | 1188 | 1079 (1428) | 0 |
| Level 36 | 3,27 (5,94) | 0,03 | 0,39 Mio. | 1216 | 1141 (1419) | 0 |

Alle zehn: OK. Die Spitze von Level 3 (14,87 ms) ist die Fertigungsstraße (bekannt seit der Abnahme, E-046; unter 16,7 ms).
Ergebnisse: `docs/metrics/smoke_<level>.json`.

## 6. Koop-Langlauf 60 min

`coop_test data --long 3600` (Host und drei Bots über den Netzstörer, Internet-Profil, über das Relay):

| Lauf | Ergebnis |
|---|---|
| 1 (08.11., 17:56–18:56) | **FEHLGESCHLAGEN: 10 Prüfungen, 2 Fehler** – „nach 3602 s noch alle verbunden“ und „Weg aller Gäste 3“. Im Protokoll (`coop_test.log`) trennte der Host Bot2 nach 55 min mit Grund 2 (Zeitlimit), 7 s nach dessen Wiedereinstieg am Sicherungspunkt (Bot2 war 20 s zuvor gestorben). Bis 18:36 liefen daneben die übrige Schlussprüfung, Bildaufnahmen und zwei Spielprozesse; zur Zeit der Trennung lief von mir nichts weiter. Speicher nach 5 … 60 min 629 / 669 / 641 / 646 / 784 / 685 / 650 / 730 / 638 / 676 / 612 / 655 MB, Sektoren beim Host 12 → 19; Ping 144 ms (±42), Verlust 8 %; Vorhersage im Mittel 3,3 cm daneben, über 10 cm in 10 662 von 175 080 Momentaufnahmen (6,1 %); 94 331 Korrekturen, 14 Sprünge über 2 m (größter 159,4 m); 25,0 / 25,4 kB/s je Gast; Host lehnte 741 von 1 600 Absichten ab, 11 808 Eingaben verworfen (Zeitbudget) |
| 2 (08.11., 19:06–20:06, Rechner ruhig, mit `LIMINAL_NET_LOG=1` und neuer Diagnose der Trennungsgründe) | **OK: 10 Prüfungen, 0 Fehler** – alle drei Gäste nach 3 600 s verbunden, Weg Relay. Speicher 611 / 679 / 630 / 699 / 639 / 654 / 623 / 825 / 692 / 656 / 611 / 623 MB (kein stetiger Anstieg), Sektoren 12 → 16; Ping 148 ms (±19), Verlust 6 %, SCTP-Laufzeit 148–151 ms; Vorhersage im Mittel 4,5 cm daneben, über 10 cm in 16 603 von 180 956 (9,2 %); 97 857 Korrekturen, **160 Sprünge** über 2 m (größter 214,0 m, B-012); 29,3 / 29,8 kB/s je Gast; 822 von 1 708 Absichten abgelehnt, 12 170 Eingaben verworfen. Keine Trennung im Diagnoseprotokoll |

**Bewertung:** Einer von zwei Läufen ist fehlgeschlagen, die Ursache ist nicht bestimmt (B-036, offen). Der Lauf ohne Last bestand. Die Trennung fiel auf einen Wiedereinstieg; ob Last in der ersten Hälfte, ein Stillstand von SCTP über das verlustreiche Relay (wie vor E-033) oder der Wiedereinstieg die Ursache war, zeigt das Protokoll nicht. Für weitere Läufe gibt die DLL mit `LIMINAL_NET_LOG=1` jetzt den genauen Grund aus (Stille, voller Puffer, Verbindung gescheitert/geschlossen). Gegenüber dem Lauf vom 01.11. (8 Sprünge, 19 kB/s) gibt es mit den neuen Wesen deutlich mehr Tode, Sprünge und Verkehr.

## 7. Versionen V10 und V12

- **V12:** `freeze.py "LIMINAL V12" "Sicherungen\v12_frozen_2026-10-26.txt" --verify` im Hauptordner:
  `Manifest 25550, veraendert 0, fehlend 0, neu 0`. Im Git: `LIMINAL V12` gleich dem Branch `liminal-v12`
  (`git diff --quiet liminal-v12 HEAD -- "LIMINAL V12"`), letzter Commit am Ordner `e8481b6` (26.10.2026).
- **V10:** `git diff --quiet main HEAD -- "LIMINAL V10"`: keine Abweichung (213 Dateien).

## 8. Ehrlicher Stand pro Ebene

„Abgenommen“ heißt: alle Schranken a–g belegt (`AUFWAND.md`, `SELBSTKONTROLLE.md`). Restpunkte sind begründet und in
`AUFWAND.md` je Ebene aufgeführt; hier die wichtigsten.

| Ebene | Stand | Runden, Laufweg | Wichtigste Restpunkte |
|---|---|---|---|
| Level 0 | abgenommen 03.11. | 5 Runden (`KRITIK_Level0.md`) | Leuchtstab färbt die Decke grün (gewollt); Hand schließt sich beim Beilhieb nicht ganz |
| Level 1 | abgenommen 01.11. | 7 Runden, 836 m / 18 Räume | Fabled-Halle bewusst dunkel; Modellprüfung der Wesen erst in der Schlussprüfung (B-033, behoben) |
| Level 2 | abgenommen 02.11. | 7 Runden | Jerry im Ebenenlicht schwer zu sehen (Raum bewusst dunkel) |
| Level 3 | abgenommen 02.11. | 7 Runden | Kopf des entwickelten Facelings glatt; GPU-Spitze 14,9 ms an der Fertigungsstraße; großes Bündel nah mit Clipping der Waffe (B-035) |
| Level 5 | abgenommen 03.11. | 5 Runden, 687 m | Samanthas Weiß im Kerzenlicht grau; Hand des Gastgebers wie eine Gliederpuppe; Watcher erst ab 4 m lesbar |
| Level 5.3 | abgenommen 03.11. | 5 Runden | Firnis des gemalten Gesichts aus 1,5 m körnig; Finger im Griff wulstig; ein Lesepult als begründete Ausnahme |
| Level 188 | abgenommen 03.11. | 7 Runden, 207 m / alle 7 Räume (E-036) | Gänge außerhalb des Auftritts grell (Seite); Arm des Fensterwesens nah steif; Wurzelpistole aus > 3 m ein dunkler Punkt |
| Level 903 | abgenommen 08.11. | 6 Runden, 294 m / 1 Raum (E-036) | Glas deckend (Engine); ruhender Zug ohne Innenraum; Facelings fast nur im fahrenden Wagen |
| Level 37 | abgenommen 08.11. | 6 Runden, 203 m / 21 Räume | Lichtnetz als Näherung; keine Durchsicht unter Wasser; Spiegellicht verstärkt (Kunstgriff); Ausgänge zu 130/43/233 nicht gebaut |
| Level 36 | abgenommen 08.11. | 6 Runden, 481 m / 15 Räume | Ursache der violetten Flecken offen (B-031); Sackgasse halbdunkel; Glas deckend |

## 9. Ehrlicher Stand pro Wesen

Alle Wesen der zehn Wiki-Seiten sind laut `ENTITY_ABGLEICH.md` im Spiel. Jedes hat ein Briefing (`KREATUR_*.md`) mit
Wow-Check und mindestens fünf Kritikrunden; die Wesen wurden in den Abnahmen ihrer Ebenen mit `creatureshots.py` in ihren
Räumen aufgenommen (Ebenenlicht und Lampe) und beurteilt. Der Wow-Check ist ein Urteil aus Bildern, kein Test.
Im Mehrspieler laufen alle Wesen beim Host; die Puppen der Gäste bekommen Rolle, Lage, Zustand und Animationskanäle über
Protokoll v3 (geprüft in `coop_test`: Puppen und Wesen-Gesten; nicht jedes Wesen einzeln im Zwei-Prozess-Spiel).

| Ebene | Wesen (Stand) | Bekannte Schwächen |
|---|---|---|
| 0 | Gestalt, Halluzinationen (Flüstern, Kratzen, Gestalt im Raum, Stromausfall) – umgesetzt | – |
| 1 | Smiler, Hound, Clump, Skin-Stealer, Wretch, Duller, Arms, Crawler, Growler, Nguithr'xurh, Wrangler, Plague Goblins, Spielleiterin (Entity 99), Garden-Verwandlung, Wesen an der Schlafraumtür, Ouroboros-Arbeiter – umgesetzt | Modelle von Arms, Duller, Goblins, Ouroboros, Skin-Stealer-Haut erst jetzt geprüft und bereinigt (B-033) |
| 2 | Fensterwesen, Smiler, Deathmoths, Clump, Jerry, Hound-Rudel, Faceling, Skin-Stealer-Spuren, Wretch, Nguithr'xurh, Death Rats, Cannibal Cuisine, Wrangler, Photoshop, Growler, Swindlebird, Macchina-Reste – umgesetzt | Jerry im Ebenenlicht schwer zu sehen |
| 3 | Hound-Rudel, Faceling (entwickelt), Wretch, Deathmoths mit Praetorianer, Skin-Stealer im Mantel, Smiler, großes und kleines Bündel, große Death Rats mit Fallen, purpurne Wesen, Schattenfigur, Red Knight – umgesetzt | Kopf des entwickelten Facelings glatt; großes Bündel nah: Waffe durch das Handgelenk (B-035) |
| 5 | Deathmoths mit Nest, Hound, Skin-Stealer, Death Rats, Ants, Woodlins, Watchers, Growler, Nguithr'xurh, Wrangler, Samantha, Gastgeber-Stimme (Entity 18) – umgesetzt | Samantha im Kerzenlicht grau; Gastgeber-Hand; Watcher erst ab 4 m lesbar; Wrangler in der Vorführung nur Rücken |
| 5.3 | Library Mind, Hausherr (Entity 18), Ambassadors und Macchina als Wissen – umgesetzt | Gesicht des Hausherrn nah körnig; Finger im Griff wulstig |
| 188 | Fensterwesen, Faceling im rosa Kleid, Skin-Stealer, Gestalt, Horcher (Auslegung), Wurzelpistole (Entity 39) – umgesetzt | Arm des Fensterwesens nah steif; Pistole aus der Ferne ein Punkt |
| 903 | Metro (Entity 756) als Wagen, Facelings – umgesetzt | Facelings fast nur im fahrenden Wagen |
| 37 | Seite nennt keine Wesen | – |
| 36 | Seite nennt keine Wesen (nur Reisende, Durchsage als Klang) | – |

Ausnahmen nach Auftrag (Teil 4): Menschen und Gruppen (nur Spuren und Notizen), Staphylococcus liminalis (kein Körper).

## 10. Bekannte Probleme

Bestätigt und offen (`BUGLISTE.md`):
- **B-036** Koop-Langlauf 60 min über das gestörte Relay: ein Gast nach 55 min getrennt (Zeitlimit); Einzelheiten und
  Stand in Abschnitt 6.
- **B-012** Koop-Langlauf: einzelne Sprünge über 2 m beim Abgleich (Ursache nicht untersucht); Stand dieses Laufs in
  Abschnitt 6.
- **B-024** Einmal ~22 s keine neuen Klänge nach Minimieren/Wiederherstellen (alter Stand, in 158 Zyklen nicht wieder).
- **B-031** Violette Flecken im Umgebungslicht bei kühl getönter Zone (Level 36 vermieden, Ursache offen).
- **B-032** Teil-Stromausfall eines Stromkreises: helle Nachbarräume leuchten als Sägezahn durch die Wandoberkante
  (in Level 188 durch ganzen Stromkreis vermieden).
- **B-035** Großes Bündel (Level 3) aus der Nähe: Eisenstange der Waffe durch das Handgelenk, harte Kante zwischen
  blasser und roter Haut am langen Arm (beim Auswählen der README-Bilder gefunden; Bild verworfen).

Vermutung, nicht bestätigt:
- **B-034** Koop mit zwei Spielprozessen auf einem stark belasteten Rechner: einmal Trennung des Gasts nach 2 min
  („Zeitlimit“, 10 s Stille); in zwei weiteren Läufen nicht.

Grenzen der Engine (bewusst, in den Abnahmen genannt): Glas ist deckend; keine Durchsicht unter Wasser; Lichtnetz als
Näherung; rechte Hand (Gesten) weiter prozedural.

## 11. Nicht geprüft

- Spiel über das echte Internet mit echtem NAT und fremdem TURN-Server (nur nachgestellt: Netzstörer, getrennte
  Loopback-Adressen, eigenes Relay).
- Echtes Umstecken von Kopfhörern, echter Formatwechsel, Bluetooth (nachgestellt über Fehlerpfade, `AUDIO_BUG.md`).
- Hören: Lautsprecher stumm; Klang nur über Pegelmessung am Master-Ausgang belegt.
- Leistung auf schwächerer Hardware (gemessen nur auf diesem Rechner, Qualität Hoch).
- Eine Durchsicht durch Dritte (Sicherheit, Gestaltung).

## 12. Restrisiken

- **Lizenz** des Spiels ist offen (Entscheidung des Eigentümers, `GITHUB_UPLOAD.md`); Wiki-Inhalte nach CC BY-SA 3.0
  in eigenen Worten, benannte Einzelfiguren als eigene Gestaltung (E-039, `LICENSE_REVIEW.md`).
- **Mehrspieler:** nicht „vollkommen sicher“ (`MULTIPLAYER_BEDROHUNGSMODELL.md`); Netzweg-Fuzzing über Minuten, nicht
  über Stunden.
- **Kunstgriffe** in der Darstellung (Spiegellicht ×6 in Level 37, Lichtnetz) können auf anderen Grafikkarten anders
  wirken.
- **Wow-Check** ist ein Urteil aus Bildern eines einzelnen Prüfers.
- **Upload:** V13 ist noch nicht hochgeladen; der Hauptstrang macht das nach `GITHUB_UPLOAD.md`.

## 13. Upload

Erledigt am 08.11.2026 im Hauptstrang: Branch `liminal-v13`, Tag `v13.3`, Release v13.3 (30 Teile, Prüfsumme als
Fremder bestätigt), README auf `main` mit den 16 Bildern und der Koop-Anleitung, Bau-Test aus frischem Klon – Einzelheiten
in `GITHUB_UPLOAD.md`, Abschnitt „V13-Upload“.
