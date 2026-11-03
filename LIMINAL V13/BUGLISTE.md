# LIMINAL V13 – Bugliste

Stand 26.10.2026. Reihenfolge der Bearbeitung: Abstürze und Blocker zuerst, dann Sicherheit, dann Darstellung. Jeder Eintrag: Fundort, Schwere, Ursache, Stand, Nachweis.

| Nr. | Bereich | Schwere | Befund | Stand |
|---|---|---|---|---|
| B-001 | Spielerfigur, Kapuzenjacke | mittel | Bei gebeugtem Arm (Lampe gehoben) riss der Ärmel am Ellbogen auf; von außen gesehen hing unter dem Arm ein Flügel aus Stoff mit gezacktem Rand, innen am hängenden Arm dunkle Zacken. Ursachen: (1) In der Ruhepose (Arm 24 Grad abgespreizt) verschmolzen Ärmel und Rumpf beim Vernetzen (Brust mit 7,5 cm Rundung) – Rumpffläche lag im Ärmel und riss beim Bewegen auf. (2) Lineares Mischen der Knochen ließ Punkte an Ellbogen und Schulter zusammenfallen und umklappen (Innenseite des Stoffs sichtbar). (3) Von außen hielt der Körper die Lampe wie im Ego-Bild (Hand unter dem Auge, Ellbogen über dem Kopf). | **behoben (V13.2)**: Ruhepose mit 54 Grad, Dual-Quaternion-Skinning für Menschen, eigene Lampenhaltung für die Ansicht von außen (Schulteransicht, Mitspieler), Arme im Stand 13 statt 7 Grad abgespreizt, Hose unter dem Saum schlanker. Nachweis im Abschnitt „B-001: Nachweis“. |
| B-002 | Speicher beim Start | gering | Beim Laden stieg der Arbeitsspeicher kurz auf rund 3,1 GB (bei Texturgröße 2048: 11,1 GB). Ursache: alle 91 Texturebenen lagen mit Mip-Stufen unkomprimiert im Speicher, dazu Höhen und Normalen aller Ebenen als float und je Thread Zwischenpuffer; hochgeladen wurde erst am Ende. | **behoben (V13.2)**: Ebene für Ebene erzeugen, sofort komprimieren, über eine wiederverwendete Zwischenablage auf die GPU kopieren und freigeben; höchstens drei bis vier Ebenen gleichzeitig. Spitze 672–682 MB je Ebene (vorher 3 088–3 116 MB), bei 2048 px 1,25–1,31 GB (vorher 11,07 GB). Messung im Abschnitt „B-002: Messung“. |
| B-003 | Starter (`LIMINAL.exe`) | Sicherheit, mittel | Die Signaturprüfung des heruntergeladenen Visual-C++-Pakets akzeptierte jede gültige Signatur, nicht nur die von Microsoft. Das Paket wird mit Administratorrechten ausgeführt. | **behoben** (E-023): Unterzeichner muss „Microsoft Corporation“ sein, Widerrufsprüfung der Kette. Nachweis `MULTIPLAYER_TESTS.md` Abschnitt 4. |
| B-004 | Protokoll | gering | Bei jedem ersten Start zwölf Warnungen „JSON-Datei nicht gefunden“ für Dateien, die fehlen dürfen (ältere Einstellungsversionen, die absichtlich entfernte `world.json` der alten Welt). Echte Warnungen gingen darin unter. | **behoben**: `loadJsonFile(path, optional)` schweigt bei fehlenden optionalen Dateien. |
| B-005 | Koop: Beitritt | hoch | Kein Gast konnte beitreten. Der Gast meldete „verbunden“ erst nach der ersten Nachricht des Hosts und sendete erst dann seinen Beitritt; der Host sendete erst nach dem Beitritt. Beide warteten aufeinander (erster Bot-Test, 57 Fehler). | **behoben (V13.2)**: Der Host sendet nach dem angenommenen Beweis eine leere, verschlüsselte Bestätigung. |
| B-006 | Koop: Host | hoch | Ein vom Host getrennter Gast (Müll, Kick, falsche Version) blieb mit seiner Transportnummer in der Liste. Der nächste Gast auf demselben Platz wurde ihm zugeordnet, sein Beitritt als „falscher Zustand“ verworfen. Ursache: `ln_drop` meldet keine Trennung. | **behoben (V13.2)**: Der Host trägt die Trennung selbst ein; ein neuer Gast auf einem belegten Platz löst den alten Eintrag. Angriffstest 11/11. |
| B-007 | Koop: Tod eines Gasts | mittel | Ein Gast mit 0 Leben starb nicht: Die Erholung der Werte lief vor der Prüfung und hob das Leben wieder an. | **behoben (V13.2)**: Tod vor der Erholung prüfen. |
| B-008 | Koop: toter Gast | mittel | Ein toter Gast konnte seine eigene Figur weiter laufen lassen (nur bei sich); beim Wiedereinstieg sprang sie. | **behoben (V13.2)**: Tote bewegen sich nicht (auch im Einzelspiel während des Schrecks). |
| B-009 | Koop: Beenden | hoch | Das Spiel stürzte beim Beenden ab (Exit 3), wenn ein Koop-Spiel lief: Die Koop-Schicht verwies noch auf die schon gelöschte Sitzung. | **behoben (V13.2)**: Koop vor der Sitzung beenden. Zwei-Prozess-Test endet mit 0. |
| B-010 | Koop: Gast | gering | Unter Paketvertauschung kamen Momentaufnahmen vor dem Willkommen an und wurden als ungültig gezählt (40 im schlechten Netz). | **behoben (V13.2)**: Momentaufnahmen erst nach der ersten Eingabe des Gasts. |
| B-011 | Koop: Vorhersage | gering | Ein Gast lief mit seiner eigenen eingestellten Laufgeschwindigkeit und ohne den Tempo-Bonus der Energy Bar; der Host rechnete mit dem Grundtempo. Ständige Korrekturen. | **behoben (V13.2)**: Gast mit Grundtempo, Bonus in der Momentaufnahme. |
| B-012 | Koop: Langlauf | gering | Im 60-min-Lauf vier Sprünge über 2 m, einer davon 89,9 m (Abgleich eines Bots). | **offen**: Ursache nicht untersucht (vermutlich ein Ortswechsel eines Bots über einen Übergang); im 30-min-Lauf mit Protokoll v3 kein Sprung. |
| B-013 | Wesen: Licht | mittel | Die Lampe „traf“ Wesen oft nicht: die Sichtprüfung beginnt an der Hand – steht man dicht an einer Wand, liegt die Hand in der Wandkachel und jeder Strahl gilt als verdeckt (betraf alle Lichtreaktionen, auch Hound und Motten). | **behoben (V13.2)**: verdeckt nur, wenn auch vom Auge aus keine Sicht ist; neue Wesen prüfen zusätzlich die Brust. Test: Skin-Stealer gereizt durch die Lampe. |
| B-014 | Skin-Stealer: Größe | mittel | Mit 2,3 m war er höher als die Türstürze der Ebenen (2,2 m): Kopf in der Decke, Sichtprüfungen zum Kopf scheiterten an Türen. | **behoben (V13.2)**: 2,05 m (E-031). |
| B-015 | Fensterwesen: Material | mittel | Der flache Umriss mit dem Rauchmaterial der Gestalt war fast unsichtbar (das Material dünnt Ränder aus – bei einer flachen Form ist alles Rand). | **behoben (V13.2)**: eigenes Material „Umbra“ (schwarz, matt, fest). |
| B-016 | Zweibeiner: Gang | mittel | Beim Losgehen blieb der Standfuß stehen, bis die Phase ihn rief; das Becken sank bis zum Boden (Spagat). | **behoben (V13.2)**: der hintere Fuß tritt beim Anlauf zuerst, Ausgleichsschritt, Ferse hebt sich, Becken sinkt höchstens 7 cm. |
| B-017 | Audio: Stimmen | hoch | **Audio verstummte nach einigen Minuten** (Hauptursache von „Audio geht manchmal, manchmal nicht“): Eine Dauerschleife, die beim Stoppen unhörbar war (außer Reichweite, Lautstärke 0), wurde nie freigegeben – das Ausblenden lief nur im hörbaren Zweig des Mischers. Leuchten, Rohre, Automaten und jeder Ebenenwechsel hinterließen solche Stimmen; nach 2–16 min waren alle 96 belegt, danach wurde fast jeder neue Klang abgewiesen (Schritte, Wesen, Türen). | **behoben (V13.3)**: gestoppte unhörbare Schleife sofort frei, Ausblendende zuerst verdrängen, Sicherheitsnetz im Watchdog. Reproduziert: Autopilot Level 2/3/5 voll nach 2,1/2,6/3,3 min, bis 208 407 Abweisungen; neu 4 × 28 min: 0 hängend, 0 abgewiesen. `AUDIO_BUG.md` 3.1. |
| B-018 | Audio: Aussetzer | mittel | Pufferunterläufe unter Last: Der Mischer lief mit normaler Priorität neben 12 Arbeitsthreads. Gleichzeitig gelaufene Tests: Ebenenwechsel alt 367 Unterläufe, längste Mischpause 3 621 ms, neu 0 und 10,8 ms; 100 Starts alt: Unterläufe in 45 Starts, Mischpause bis 1 850 ms, neu (1 000 Starts) 0. | **behoben (V13.3)**: MMCSS „Pro Audio“ (Rückfall `TIME_CRITICAL`), Puffer beim Öffnen vorgefüllt. `AUDIO_BUG.md` 3.2. |
| B-019 | Audio: Gerät | hoch | Kein Neuverbinden: kein `IMMNotificationClient` (neues Standardgerät, z. B. Kopfhörer, wurde ignoriert), keine `IAudioSessionEvents` (Formatwechsel, exklusiver Modus), und meldete der Treiber kein Ereignis mehr, wartete der Thread für immer (`WaitForSingleObject … continue`). Fehlercodes wurden nicht protokolliert. Im Code belegt; Auslöser nur simuliert (ein Gerät, keine Systemeinstellungen geändert). | **behoben (V13.3)**: Benachrichtigungen, Prüfung bei jeder Zeitüberschreitung, Neuöffnen mit Rückstaffelung, Rückfall auf das Mischformat des Geräts, Fehlercodes im Protokoll. Gerätetest: 200/200 simulierte Fälle erholt. `AUDIO_BUG.md` 3.4, 7. |
| B-020 | Audio: Start/Neuverbinden | gering | Klänge wurden verworfen, solange das Gerät (neu) geöffnet wurde (`play2d/play3d` brachen bei `!running_` ab). Im Code belegt; in den Läufen des alten Stands nicht aufgetreten (er verband nie neu). | **behoben (V13.3)**: Klänge werden angenommen, sobald sie erzeugt sind; ohne Gerät mischt ein Ersatztakt weiter. |
| B-021 | Audio: Hauptmenü | gering | Die Dämpfung des Schrecks (Tiefpass ~1,3 kHz) blieb im Hauptmenü hängen, ebenso eine Pause bis zur nächsten Sitzung (zurückgesetzt wurde nur in `updateAudio`, das im Menü nicht läuft). Im Code belegt, Gegentest offline. | **behoben (V13.3)**: `resetMix()` beim Hauptmenü und bei jeder neuen Sitzung. |

## Audio (V13.3): Vermutungen und Härtungen (nicht beobachtet)

Getrennt von den bestätigten Fehlern oben. Ausführlich in `AUDIO_BUG.md`, Abschnitt 4.

| Nr. | Bereich | Vermutung | Stand |
|---|---|---|---|
| B-022 | Audio: NaN | Ungeprüfte Eingaben: eine NaN-Position hätte Hall, Filter und Begrenzer dauerhaft vergiftet (= Stille). In keinem Lauf beobachtet (0 ungültige Eingaben, 0 NaN). | gehärtet: Eingaben geprüft, NaN am Ausgang erkannt und zurückgesetzt (Test). |
| B-023 | Audio: 3D-Culling | Im ersten Bild nach Ebenenwechsel/Wiedereinstieg stand der Hörer für `play3d` noch an der alten Stelle (Quellen könnten fälschlich verworfen werden). Nicht gemessen. | gehärtet: Hörer vor den Klängen des Bildes gesetzt. |
| B-024 | Fenster/Audio | Einmal im alten Stand nach Minimieren/Wiederherstellen ~22 s keine neuen Klänge (das Spiel rechnete vermutlich keine Bilder). In 158 weiteren Zyklen (alt und neu) nicht wieder aufgetreten. | offen, Ursache nicht geklärt; kein Audiofehler im engeren Sinn. |

## Speicherfehler nach dem Einmischen der Wesen-Zweige (V13.3, 02.11.2026)

Nach dem Merge der Level-2- und Level-3-Wesen stürzten `gameplay_test` und `coop_test` ab (Heap beschädigt). Gefunden mit
einem eigenen Build unter AddressSanitizer (`build/asan`, RelWithDebInfo, `-fsanitize=address`); danach beide Tests unter
ASan ohne Befund (901/0, 209/0).

| Nr. | Bereich | Fehler | Stand |
|---|---|---|---|
| B-025 | Aufzüge (seit V8) | `updateElevators` hielt einen Zeiger auf die Aufzug-Komponente über `findElevatorTarget` hinweg; die Suche kann Chunks laden und Aufzüge anlegen, der Pool wächst – heap-use-after-free. Erst durch das neue Speicherbild sichtbar. | **behoben**: Zeiger nach der Suche neu geholt (`elevator.cpp`). |
| B-026 | Wesen-Schleife | `Presence::update` lief mit Iterator über `agents_`; `buildTrap` (Fallen der Death Rats) legte darin ein Wesen an und entfernte die älteste Falle – Iterator ungültig. | **behoben**: Schleife über den Index bis zur Anzahl davor (neue Wesen ab dem nächsten Bild); abgeräumte Fallen erst nach der Schleife entfernt (`cull_`). |
| B-027 | Level-3-Motten | Platz im Gesicht: negativer Rest bei negativer `phase` las vor `taken[8]`; Gruppenvergleiche ohne Rolle konnten fremde Wesen (z. B. Fallen, Rolle Rat, Variante 3) als Motten behandeln und ihr `ext` falsch umdeuten. | **behoben**: Index normalisiert, alle Gruppenvergleiche nur für Motten der Varianten 3–5. |
| B-028 | Koop-Test | `testReconnectKick` gab die Sitzung des gesperrten Gasts frei, bevor `stop()` darin aufräumte (die App räumt in der richtigen Reihenfolge auf: `leaveCoop` → `stop()` → Sitzung). | **behoben** im Test (erst `stop()`, dann die Sitzung). |
| B-029 | Materialien | Nach dem Merge der Wesen von Level 3, 5 und 5.3 reichten die festen Wesen-Materialien bis Platz 195; die 33 Materialien der Fundstücke (ab `FM_FIG_END`) hätten bis 228 gebraucht, Grenze 224 – die letzten 5 Fundstücke fielen still auf ein falsches Material (nur Warnung im Protokoll, kein Test schlug an). | **behoben**: `kFixedMaterials` 288, dazu `static_assert(FM_FIG_END + kItemMaterialSlots(48) <= kFixedMaterials)`; Smoke Level 0: Plätze 195..227, keine Warnung. |

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

**V13.3 (Arm, E-032):** Ärmel, Gewichte am Ellbogen und Ego-Haltung sind neu; der Test bleibt grün. Ego geradeaus jetzt Dehnung 2,82 und 5 von 27 416 Dreiecken umgeklappt, an der Wand 266 (Grenze 274; Stichprobe: 62 von 67 auf der Beugeseite, wo der Unterarm auf dem Oberarm liegt), von außen 8 bzw. 21 (Grenze 68). Die Jacke ist in Stufe 0 feiner vernetzt (3,6-mal so viele Dreiecke); die Grenzen sind Anteile. Werte je Haltung in `HAND_ANALYSE.md`, Abschnitt „V13.3: Arm“.
