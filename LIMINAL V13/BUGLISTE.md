# LIMINAL V13 – Bugliste

Stand 26.10.2026. Reihenfolge der Bearbeitung: Abstürze und Blocker zuerst, dann Sicherheit, dann Darstellung. Jeder Eintrag: Fundort, Schwere, Ursache, Stand, Nachweis.

| Nr. | Bereich | Schwere | Befund | Stand |
|---|---|---|---|---|
| B-001 | Spielerfigur, Kapuzenjacke | mittel | Bei gebeugtem Arm (Lampe gehoben) riss der Ärmel am Ellbogen auf; von außen gesehen hing unter dem Arm ein Flügel aus Stoff mit gezacktem Rand, innen am hängenden Arm dunkle Zacken. Ursachen: (1) In der Ruhepose (Arm 24 Grad abgespreizt) verschmolzen Ärmel und Rumpf beim Vernetzen (Brust mit 7,5 cm Rundung) – Rumpffläche lag im Ärmel und riss beim Bewegen auf. (2) Lineares Mischen der Knochen ließ Punkte an Ellbogen und Schulter zusammenfallen und umklappen (Innenseite des Stoffs sichtbar). (3) Von außen hielt der Körper die Lampe wie im Ego-Bild (Hand unter dem Auge, Ellbogen über dem Kopf). | **behoben (V13.2)**: Ruhepose mit 54 Grad, Dual-Quaternion-Skinning für Menschen, eigene Lampenhaltung für die Ansicht von außen (Schulteransicht, Mitspieler), Arme im Stand 13 statt 7 Grad abgespreizt, Hose unter dem Saum schlanker. Nachweis im Abschnitt „B-001: Nachweis“. |
| B-002 | Speicher beim Start | gering | Beim Laden stieg der Arbeitsspeicher kurz auf rund 3,1 GB (bei Texturgröße 2048: 11,1 GB). Ursache: alle 91 Texturebenen lagen mit Mip-Stufen unkomprimiert im Speicher, dazu Höhen und Normalen aller Ebenen als float und je Thread Zwischenpuffer; hochgeladen wurde erst am Ende. | **behoben (V13.2)**: Ebene für Ebene erzeugen, sofort komprimieren, über eine wiederverwendete Zwischenablage auf die GPU kopieren und freigeben; höchstens drei bis vier Ebenen gleichzeitig. Spitze 672–682 MB je Ebene (vorher 3 088–3 116 MB), bei 2048 px 1,25–1,31 GB (vorher 11,07 GB). Messung im Abschnitt „B-002: Messung“. |
| B-003 | Starter (`LIMINAL.exe`) | Sicherheit, mittel | Die Signaturprüfung des heruntergeladenen Visual-C++-Pakets akzeptierte jede gültige Signatur, nicht nur die von Microsoft. Das Paket wird mit Administratorrechten ausgeführt. | **behoben** (E-023): Unterzeichner muss „Microsoft Corporation“ sein, Widerrufsprüfung der Kette. Nachweis `MULTIPLAYER_TESTS.md` Abschnitt 4. |
| B-004 | Protokoll | gering | Bei jedem ersten Start zwölf Warnungen „JSON-Datei nicht gefunden“ für Dateien, die fehlen dürfen (ältere Einstellungsversionen, die absichtlich entfernte `world.json` der alten Welt). Echte Warnungen gingen darin unter. | **behoben**: `loadJsonFile(path, optional)` schweigt bei fehlenden optionalen Dateien. |

## Prüfungen (Teil 5)

| Prüfung | Werkzeug | Ergebnis |
|---|---|---|
| Hauptmenü: Einstellungen (Maus, Grafik, Audio), Neues Spiel, zurück | Skript `smoke.ps1` (`expect`-Prüfungen der Oberfläche) | alle Seiten erreicht, keine Fehlermeldung |
| Je Ebene (10): Laden, Gehen, Rennen (Ausdauer), Lampe an/aus, Inventar, Karte, Pause, Notizbuch, Werte setzen, Speichern über das Pausemenü, Beenden | `smoke.ps1` | alle 10 Ebenen: alle Prüfungen bestanden, Ausgang 0, keine Warnung oder Fehlermeldung im Protokoll |
| Spielstand laden | `smoke.ps1` (`--save-file`, Werte direkt nach dem Laden) | alle 10: Gesundheit und Verstand wie gespeichert (37,9 → 38; 61). Der spätere Anstieg auf 48 ist die normale Erholung. |
| Übergänge zwischen allen Ebenen, Halt aller Objekte, Vorräte, Wesen | `gameplay_test` | siehe Abschlussbericht |
| Bildzeiten je Ebene (1600 × 900, Qualität 2) | Befehl `perf` | GPU 1,8–5,7 ms; das Bild ist auf 60 Hz begrenzt |
| Grafikspeicher, Arbeitsspeicher | Befehl `perf` (neu: DXGI 1.4, Prozessspeicher) | VRAM 935–1012 MB; RAM 470–620 MB im Spiel, Spitze beim Laden 3,1 GB (B-002) |
| Karten | `tools/levels/<ebene>.py` | alle 10 bestanden, Inhalt identisch |
| Auflösungen | `ui_shots.ps1`: Titel, Pause, HUD, Inventar, Karte bei 1280 × 720, 1920 × 1080, 2560 × 1440, 1024 × 768 (4:3), 3440 × 1440 (21:9) | alle 25 Bilder vollständig, nichts abgeschnitten oder überlappend (`docs/screens/ui/v13_aufloesungen.jpg`) |



## B-002: Messung (V13.2)

Befehl `perf` (Arbeitsspeicher jetzt und Spitze seit Prozessstart, `K32GetProcessMemoryInfo`). Start direkt auf der Ebene, 1600 × 900, Qualität 2 (Texturen 1024 px, BC1/BC5), 240 Bilder nach dem Laden. Vorher: Stand vor V13.2 (`bin_smoke`, gleicher Datenstand). Skript `mem_sweep.ps1`.

| Ebene | RAM-Spitze vorher | RAM-Spitze nachher | RAM im Spiel vorher | RAM im Spiel nachher | Start bis Texturen oben vorher | nachher |
|---|---|---|---|---|---|---|
| Level 0 | 3 116 MB | 672 MB | 559 MB | 493 MB | 6,43 s | 9,03 s (erster Lauf, kalter Dateicache) |
| Level 1 | 3 089 MB | 676 MB | 533 MB | 516 MB | 6,40 s | 6,63 s |
| Level 2 | 3 099 MB | 678 MB | 546 MB | 540 MB | 6,37 s | 6,59 s |
| Level 3 | 3 100 MB | 681 MB | 532 MB | 511 MB | 6,37 s | 6,54 s |
| Level 5 | 3 093 MB | 681 MB | 562 MB | 540 MB | 6,34 s | 6,55 s |
| Level 5.3 | 3 099 MB | 675 MB | 594 MB | 580 MB | 6,34 s | 6,57 s |
| Level 188 | 3 097 MB | 677 MB | 553 MB | 506 MB | 6,41 s | 6,59 s |
| Level 903 | 3 099 MB | 681 MB | 556 MB | 513 MB | 6,39 s | 6,53 s |
| Level 37 | 3 088 MB | 676 MB | 522 MB | 502 MB | 6,43 s | 6,55 s |
| Level 36 | 3 100 MB | 682 MB | 529 MB | 516 MB | 6,37 s | 6,63 s |

Texturgröße 2048 (Qualität 3), Level 0 und Level 5: Spitze vorher 11 074 / 11 091 MB, nachher 1 251 / 1 308 MB; bis die Texturen oben sind vorher 20,2 / 20,3 s, nachher 22,7 / 20,6 s. Grafikspeicher unverändert (953 MB bzw. 3 318 MB).

- **Bild unverändert:** Bildschirmfoto Level 1 vorher gegen nachher (gleicher Seed, feste Bildzeit): 231 von 1 440 000 Pixeln weichen um mehr als 4 Stufen ab, verstreut (schwebender Staub, zufällig je Lauf); mittlere Abweichung 0,04 Stufen.
- **Zwischenschritt verworfen:** Ebenen einzeln mit `UpdateSubresource` hochladen senkte die Spitze, ließ aber im Spiel rund 350 MB mehr Arbeitsspeicher stehen (Hochladepuffer des Treibers). Deshalb eine eigene Zwischenablage (`D3D11_USAGE_STAGING`, zwei im Wechsel) und `CopySubresourceRegion`.
- **Zeitverlauf** (Abtastung alle 50 ms, `mem_trace.ps1`): vorher Sprung auf rund 1 GB mit dem Anlegen der Ergebnisfelder, danach bis 3,1 GB; nachher gleichmäßig 500–700 MB während des Erzeugens.


## B-001: Nachweis (V13.2)

Test `gameplay_test` („Kapuzenjacke am gebeugten Ellbogen“). Er häutet die Jacke auf der CPU wie der Shader (Dual Quaternions) in sechs Haltungen und misst die Dehnung jeder Kante gegen die Ruhepose sowie die umgeklappten Dreiecke (Fläche zeigt gegen ihre Normalen; so wurde die Innenseite des Stoffs sichtbar). 7 662–7 684 Dreiecke.

| Haltung | Ellbogen gebeugt | vorher: Dehnung / umgeklappt | Zwischenstand (37 Grad, lineares Mischen + Gewichtsregel) | nachher: Dehnung / umgeklappt |
|---|---|---|---|---|
| Ego geradeaus | 59 Grad | 15,9 / – (Flügel) | 6,4 / 45 | 3,4 / 1 |
| Ego nach unten | 60 Grad | 13,7 / – | 5,7 / 39 | 2,7 / 0 |
| Ego nach oben | 56 Grad | 15,9 / – | 7,0 / 45 | 3,6 / 10 |
| Ego dicht an der Wand (stärkste Beugung) | 161 Grad | 15,2 / – | 6,6 / 92 | 3,4 / 48 |
| außen geradeaus | 67 Grad | (Ellbogen über dem Kopf) | 4,1 / 41 | 2,0 / 1 |
| außen nach unten | 78 Grad | – | 2,4 / 25 | 1,7 / 2 |

„Vorher“: Stand V13 (Ruhepose 24 Grad, lineares Mischen, Ego-Haltung auch von außen); die umgeklappten Dreiecke wurden erst ab dem Zwischenstand gezählt. Die 48 umgeklappten Dreiecke bei 161 Grad liegen in der Ellbogenbeuge, wo der Unterarm auf dem Oberarm liegt (verdeckt); die übrigen in der Achsel. Grenzen im Test: von außen höchstens 0,25 % umgeklappt und Dehnung unter 3; Ego-Haltung höchstens 1 % und unter 6,5.

Verworfener Weg: eine Gewichtsregel (Ärmelpunkte ab dem Deltamuskel ohne Rumpfanteil) beseitigte zuerst den Flügel, verschob aber die Faltung an die Schulternaht (mehr umgeklappte Dreiecke). Nach der weiteren Ruhepose war sie überflüssig und ist entfernt.

Bilder (je 12 Richtungen, Kamera um den Spieler bzw. um die Hand), `docs/screens/b001/`:
- `vorher_schulter_12_seiten.jpg` / `nachher_schulter_12_seiten.jpg` – Schulteransicht, Lampe gehoben
- `nachher_egohaltung_von_aussen_12_seiten.jpg` – die Ego-Haltung (stärkere Beugung) von außen (Skriptbefehl `egopose 1`)
- `vorher_ego_kamera_um_hand.jpg` / `nachher_ego_kamera_um_hand.jpg` – Kamera um die Hand (`vmcam`)
