# LIMINAL V13.3 – „Audio geht manchmal, manchmal nicht“

Stand 02.11.2026 (zusammengeführt mit `liminal-v13` `0e586ef`, 534 Klänge). Auftrag: Teil 6 des Masterprompts (Ursache finden und an der Wurzel beheben, Watchdog, Stille-Erkennung, Stresstests). Alle Zahlen stammen aus Läufen auf diesem Rechner: Windows 11, 16 logische Kerne, RTX 5070 Laptop, genau ein Ausgabegerät „Lautsprecher (6- SoundWire Audio)“ (Mischformat 48 kHz, 2 Kanäle, 32 Bit float, Periode 10 ms). Parallel liefen Spiele und Werkzeuge anderer Arbeitsstände; die Last war hoch und wechselnd.

„Alter Stand“ = V13.2 plus reine Diagnose (Commit `16aaf16`, Mischer unverändert, als `LiminalGame_pre.exe` gebaut). „Neuer Stand“ = dieser Stand.

## 1. Kurzfassung

**Reproduziert: ja.** Hauptursache ist ein **Stimmenleck im Mischer (B-017)**: Eine Dauerschleife, die im Moment des Stoppens nicht zu hören ist (außer Reichweite oder Lautstärke 0), wurde nie freigegeben. Jede Leuchte, an der man vorbeigeht, jede Rohrstelle, jeder Automat und jeder Ebenenwechsel hinterließ solche Stimmen. Nach 2 bis 16 Minuten (je nach Ebene und Weg) waren alle 96 Stimmen belegt; weil laufende Schleifen nie verdrängt werden, nahm das System danach fast keinen neuen Klang mehr an – keine Schritte, keine Wesen, keine Türen –, ohne jede Meldung. Ein Neustart des Spiels half bis zum nächsten Mal. Das ist genau „manchmal geht es, manchmal nicht“.

Zweite bestätigte Ursache: **Pufferunterläufe unter Last (B-018)** – der Mischer lief mit normaler Priorität neben 12 Arbeitsthreads; im alten Stand bis zu 730 Unterläufe je Lauf und Mischpausen bis 3,6 s, im neuen Stand unter derselben Last 0.

Dazu Fehler, die im Code eindeutig sind, deren Auslöser hier aber nur simuliert werden konnte: kein Neuverbinden bei Gerätewechsel, Formatwechsel oder stummem Treiber (B-019), Klänge verworfen, solange das Gerät (neu) geöffnet wurde (B-020), Dämpfung aus dem Spiel blieb im Hauptmenü hängen (B-021).

Alles ist behoben. Ein Watchdog startet die Engine bei Geräteverlust, Meldungen von Windows, stehendem Strom, hängendem Thread, NaN oder Stille am Master-Ausgang ohne Zutun neu und schreibt den Grund ins Protokoll.

| Messung | alter Stand | neuer Stand |
|---|---|---|
| Autopilot je Ebene (Level 0, 2, 3, 5; 4 × 36 000 Bilder) | Stimmen voll nach 2,1 / 2,6 / 3,3 min (Level 0: 87 hängend nach 23 min); bis zu **208 407** abgewiesene Klänge; danach 7,9–64 statt 167–251 gestartete Klänge je Minute | **0** hängend, **0** abgewiesen, höchstens 35 gleichzeitige Stimmen, 73–86 Klänge je Minute über 28 min |
| Wesen-Stresstest mit allen 17 Wesen, 10 Ebenen (Endstand, 534 Klänge) | mit altem Freigabeverhalten: 6 Ebenen voll, bis 94 hängend, 88 754 abgewiesene Klänge | **0** hängend, **0** abgewiesen, 340/340 Pegelprüfungen |
| 220 Ebenenwechsel mit Pause, Fokusverlust, Minimieren, Gerätefehlern | bis 37 hängende Stimmen, **367** Unterläufe, Mischpause bis **3 621 ms**, 11 Messungen völlig stumm | 0 hängend, **0** Unterläufe, Mischen ≤ 2,3 ms, 220/220 Pegelprüfungen, 0 stumm |
| Start-Stresstest (jede Ebene 100 ×) | 100 Starts: Unterläufe in 45 Starts, Mischpause bis 1 850 ms | **1 000/1 000** fehlerfrei, 0 Unterläufe, Mischen ≤ 2,8 ms |
| Geräte-/Fehlerpfade (simuliert, Abschnitt 7), je 20 × | – (kein Pfad vorhanden) | 200/200 erholt, Klang wieder nach 44–2 283 ms (Median je Art) |
| Koop (zwei Prozesse, 10 Sitzungen) | – | 10/10 Beitritte, 20/20 Pegelprüfungen |

## 2. Was jetzt bei jedem Start protokolliert wird

Protokoll `logs/liminal_v13.log` (mit `LIMINAL_USER_DIR` im eigenen Ordner):

- `Audio: Thread-Prioritaet MMCSS 'Pro Audio' (angelaufen nach 12 ms, COM 0 ms, MMCSS 0 ms)` – Priorität (ohne MMCSS: `TIME_CRITICAL`) und Anlaufzeit des Audiothreads.
- `Audio: WASAPI '<Gerät>' geoeffnet (Geraet 48000 Hz, 2 Kanaele, 32 Bit; Periode 10.0 ms) - Strom 48000 Hz, 2 Kanaele float 32 Bit, Puffer 1920 Bilder (40.0 ms); Oeffnen 136 ms [- davon Initialize … ms]` – Geräteinitialisierung, Mischformat des Geräts, Abtastrate und Format des Stroms, Puffergröße, Dauer (Schritte über 20 ms einzeln).
- Fehlschläge mit Schritt und Fehlercode: `Audio: Geraet nicht verfuegbar (GetDefaultAudioEndpoint: 0x80070490 E_NOTFOUND (kein Geraet)) - Versuch 1, naechster in 100 ms`; Wiederkehr `Audio: wieder verfuegbar nach 4 Fehlversuchen (2.17 s ohne Ausgabe)`.
- Windows-Seite: `Audio: Geraet '…' ist STUMM geschaltet (Lautstaerke 82 %)` bzw. `Windows-Lautstaerke dieses Spiels … STUMM`. Das ist kein Fehler des Spiels, aber eine häufige Erklärung für Stille. **Auf dem Testrechner war der Lautsprecher in Windows die ganze Zeit stumm geschaltet**; die Messungen am Master-Ausgang des Spiels sind davon unabhängig.
- `Audio-Status: …` (Skriptbefehl `audiostat`; vor jedem Ebenenwechsel als `Audio vor dem Wechsel`, beim Beenden als `Audio beim Beenden`): Gerät offen/zu, Mischformat, Strom, Mischerrate, Puffer, Periode; Öffnungen, Fehlschläge, Verluste, Neustarts, letzter Fehlercode und Grund; Stimmen (aktiv, Schleifen, im Ausblenden, hängend, Spitze); gestartete, abgewiesene, verworfene, zurückgeholte Klänge, ungültige Eingaben; Pegel am Master-Ausgang (Spitze, RMS der letzten Sekunde, erwarteter Pegel); Unterläufe, Zeitüberschreitungen, NaN, Stille-Alarme; längster Mischvorgang und längste Wartezeit auf die Sperre; Master, Pause, Dämpfung.
- Watchdog: `Audio-Watchdog: <Grund> - Engine startet neu` (Geräteverlust mit Fehlercode, Standardgerät gewechselt, Sitzung getrennt mit Grund, Strom hängt, Stille am Master-Ausgang, NaN, Thread reagiert nicht).
- Beenden: `Beenden ueber das Menue` bzw. `Fenster geschlossen (WM_CLOSE/WM_QUIT)` (für die Auswertung der Läufe).

## 3. Reproduktion und Stresstests (mit Zahlen)

Werkzeuge (Arbeitsordner, nicht im Repository): Skripte für Start-, Ebenenwechsel-, Fenster-, Lade- und Koop-Läufe, die die Protokollzeilen `Audio-Status` auswerten. Jede Ebene/jeder Lauf mit eigenem `LIMINAL_USER_DIR`. Die Skriptbefehle sind im Spiel (Abschnitt 11). „Endstand“ heißt: zusammengeführter Stand mit allen neuen Wesen (534 Klänge) und allen Behebungen. Läufe, die nur „neu“ heißen, benutzten einen Zwischenstand mit allen Behebungen von B-017 bis B-021; spätere Änderungen betrafen nur Anlauf und Öffnen des Geräts (Abschnitt 3.3).

### 3.1 Autopilot je Ebene (Stimmenleck B-017)

`--demo` (der Autopilot erkundet die Ebene), `--fixed-dt`, Seed 11, 36 000 Bilder, Audio-Status alle 15 s; vier Ebenen gleichzeitig.

| Ebene | alt: Stimmen voll (96/96) | alt: hängend am Ende | alt: abgewiesen | alt: gestartete Klänge/min vorher → nachher | neu: hängend / abgewiesen | neu: Spitze Stimmen | neu: Klänge/min |
|---|---|---|---|---|---|---|---|
| Level 0 | nicht ganz (95/96, 87 hängend nach 23 min; erste Abweisung nach 16,3 min) | 87 | 619 | 99 (Mittel) | 0 / 0 | 22 | 83 |
| Level 2 | nach 2,1 min | 86–93 | 208 407 | 197 → 7,9 | 0 / 0 | 35 | 86 |
| Level 3 | nach 2,6 min | 93 | 188 588 | 167 → 16,5 | 0 / 0 | 27 | 85 |
| Level 5 | nach 3,3 min | 93 | 100 010 | 251 → 63,9 | 0 / 0 | 25 | 73 |

Laufzeit alt 11,5–25,8 min, neu 28,3–28,5 min je Ebene (unter wechselnder Last unterschiedlich viele Bilder je Sekunde). Ein erster kurzer Lauf (Level 0, Seed 5, 3 min) zeigte das Wachstum schon: 17 → 30 → 45 → 52 → 58 → 62 hängende Stimmen. Neu: 0 hängende Stimmen, 0 Abweisungen, 0 Unterläufe, längster Mischvorgang 6,6–10,6 ms.

### 3.2 Ebenenwechsel (≥ 200), Pause, Fokusverlust, Minimieren, Gerätefehler

Ein Prozess, 220 Wechsel durch alle zehn Ebenen (Skriptbefehl `level`; jeder Wechsel speichert), dazwischen jede 5. Runde Pause/Fortsetzen, jede 7. Fokusverlust (Alt+Tab, über `WM_ACTIVATE` nachgestellt: öffnet die Pause wie im echten Spiel), jede 11. Minimieren/Wiederherstellen, im neuen Stand jede 6. ein Gerätefehler (`audiofault`, acht Arten reihum, je 4–8 ×) und nach jedem Wechsel eine Pegelprüfung am Master-Ausgang (`audioexpect -75`).

| | alt | neu (1. Lauf, gleichzeitig mit „alt“) | neu (Endstand) |
|---|---|---|---|
| Wechsel | 220 | 220 | 220 |
| hängende Stimmen (Spitze) | 37 | 0 | 0 |
| gleichzeitige Stimmen (Spitze) | 55 | 26 | 22 |
| Pegelprüfungen bestanden | (Befehl fehlt) | 220/220 | 220/220 |
| Messungen ganz stumm (−180 dBFS) | 11 (B-024) | 0 | 0 |
| leisester Ebenenpegel | – | −66,9 dBFS | −64,1 dBFS |
| Unterläufe | 367 | 0 | 0 |
| längster Mischvorgang | 3 621 ms | 10,8 ms (Sperre 1,1 ms) | 2,3 ms (Sperre 0,03 ms) |
| Gerätefehler ausgelöst / Neustarts / Stille-Alarme / NaN erkannt | – | 36 / 33 / 4 / 4 | 36 / 32 / 4 / 4 |
| Fehlermeldungen | 44 (alle: Tastenname im Testskript) | 44 (dto.) und 1 überflüssiger Watchdog-Neustart (behoben, Abschnitt 5) | 0 |

„Endstand“ ist der zusammengeführte Stand mit allen neuen Wesen (534 Klänge). Der erste Lauf benutzte im Testskript den falschen Tastennamen `Escape` (richtig: `Esc`); die Pause wurde dort nur über den Fokusverlust geöffnet. Der Endstand-Lauf hat alle Pausen. Neustarts < ausgelöste Fehler, weil `nan` den Strom nicht neu öffnet (nur Zurücksetzen des Mischers).

### 3.3 Start-Stresstest (jede Ebene 100 ×)

Je Start: Ebene laden, 150 Bilder, `audiostat`, `audioexpect -75`, beenden; 4 Starts gleichzeitig, Seeds verschieden.

| Lauf | Starts | fehlerfrei | Unterläufe | längster Mischvorgang | Befunde |
|---|---|---|---|---|---|
| alt (gleichzeitig mit „Zwischenstand 1“) | 100 (10 je Ebene) | 100 | **114**, in 45 Starts | **1 850 ms** | Leck zeigt sich in 150 Bildern noch nicht |
| Zwischenstand 1 | 1 000 | 995 | 0 | 6,8 ms | 3 × Audiothread lief erst 4–23 s nach dem Erzeugen an, der Watchdog verlangte deshalb einen überflüssigen Neustart; 1 × Öffnen 4,6 s (davon 4,4 s Sitzungsmeldungen/Lautstärke-Abfrage) – die Pegelprüfung kam vor dem ersten Klang; 1 × vor dem Laden der Ebene von außen beendet |
| Zwischenstand 2 (Thread sofort `TIME_CRITICAL`, Anlaufzeiten im Protokoll, Abfragen im Hilfsthread nach dem Start) | 1 000 | 1 000 | 149 × je 1, alle beim Anlauf | 5,4 ms | die Abfragen beim laufenden Strom kosteten je einen Unterlauf 14–154 ms nach dem Start (eigens gemessen) |
| **Endstand** (Abfragen vor dem Start, höchstens 250 ms gewartet; zusammengeführt, 534 Klänge) | **1 000** | **1 000** | **0** | **2,8 ms** | – |

Endstand im Einzelnen: jede Ebene 100/100; Gerät 48 000 Hz/2 Kanäle/32 Bit float, Strom 48 000 Hz, Puffer 1 920 Bilder (40 ms) in allen Starts; Öffnen Median 238 ms (95 %: 706 ms, max 2 689 ms); Audiothread angelaufen nach Median 12 ms (99 %: 143 ms, max 1 456 ms); Pegel am Master-Ausgang −55,7 bis −32,4 dBFS (Median −44,3); 8–14 Stimmen; 0 verworfene, 0 abgewiesene, 0 zurückgeholte Klänge, 0 ungültige Eingaben, 0 „Klang fehlt“; längste Wartezeit auf die Sperre 0,28 ms; 0 Warnungen außer dem Windows-Stumm-Hinweis.

### 3.4 Fenster, Fokus, Pause (eigener Lauf)

50 Zyklen in Level 0: Minimieren/Wiederherstellen, Fokusverlust/-rückkehr mit Pause, Pause über `Esc`/Fortsetzen; nach jedem Schritt Pegelprüfung. Neu: **150/150** Prüfungen bestanden, 0 Unterläufe, längster Mischvorgang 4,0 ms, 0 Warnungen. Alt (gleiche Abfolge ohne Pegelprüfung, 34 Zyklen, dann von außen beendet – siehe unten): kein stummer Messpunkt (leisester −46,8 dBFS), aber 62 Unterläufe und eine Mischpause von 322 ms.

### 3.5 Speichern und Laden

Je Ebene ein neues Spiel (speichert), dann 5 × mit `--save-file` geladen, Pegelprüfung, Pause, beenden: **49/50** fehlerfrei, Pegel −51,8 bis −33,1 dBFS, 0 Unterläufe. Der eine fehlende Lauf wurde während des Ladens von außen beendet (Abschnitt 3.9). Gespeichert wird außerdem bei jedem der 440 Ebenenwechsel aus 3.2.

### 3.6 Mehrspieler (Koop, zwei Prozesse)

10 Sitzungen, je eine andere Ebene: Host `coophost`, Gast `coopjoin` mit dem Schlüssel aus der Datei; beide prüfen den Pegel, der Host öffnet zusätzlich das Menü (im Koop läuft die Welt und ihr Klang weiter). **10/10 Beitritte, 20/20 Pegelprüfungen bestanden** (Endstand: −47,3 bis −23,2 dBFS), 0 hängende Stimmen, 0 Abweisungen, 0 Unterläufe; derselbe Lauf vor dem Zusammenführen ebenfalls 10/10 und 20/20. Frühere Durchgänge mit zu kurzer Wartezeit des Hosts (Host beendete sich, bevor der langsam startende Gast prüfte; der Gast fiel ins Hauptmenü, das keinen Dauerklang hat) sind Fehler des Testskripts, nicht des Spiels.

### 3.7 Gerätetest mit echtem WASAPI (`audio_device_test`)

Endstand (534 Klänge), echtes Standardgerät, Master 0,25. Teil 1: 200 × System anlegen, Gerät öffnen, Klang am Master-Ausgang abwarten (RMS über −50 dBFS), abbauen. Teil 2: ein laufendes System mit Dauerklang, jede Fehlerart 20 ×; „erholt“ heißt: das passende Ereignis ist eingetreten (Neustart, Stille-Alarm bzw. NaN erkannt), das Gerät ist offen, es wird wieder gemischt, und der Pegel liegt über −50 dBFS. Bei Fehlern mit Zeitfenster enthält die Dauer das Fenster (1,5 bzw. 2 s). Teil 3: 20 s Normalbetrieb mit Schritten, Master 0 und Pause.

| Fall | erholt | bis Klang (Median / max) |
|---|---|---|
| 200 Starts | 200/200 Gerät offen und Klang | offen 253 / 628 ms, Klang 279 / 628 ms (95 %: 476 ms) |
| Gerät ungültig (`AUDCLNT_E_DEVICE_INVALIDATED`) | 20/20 | 282 / 382 ms |
| Standardgerät gewechselt | 20/20 | 260 / 379 ms |
| Abtastrate → 44 100 Hz (Strom und Mischer in 44,1 kHz geprüft) | 20/20 | 189 / 336 ms |
| Abtastrate → 48 000 Hz | 20/20 | 258 / 371 ms |
| exklusiver Modus eines anderen Programms (1,5 s) | 20/20 | 2 224 / 4 479 ms |
| Gerät abgezogen (2 s kein Gerät) | 20/20 | 2 283 / 4 240 ms |
| Treiber meldet sich nicht (1,5 s) | 20/20 | 1 540 / 3 995 ms |
| Master-Ausgang stumm (Stille-Erkennung) | 20/20, 20 Alarme | 1 299 / 1 467 ms |
| NaN im Filterzustand | 20/20, 20 erkannt | 44 / 56 ms |
| Format abgelehnt → Mischformat des Geräts | 20/20 | 252 / 385 ms |
| Normalbetrieb 20 s (inkl. Master 0, Pause) | 0 Stille-Alarme, 0 Neustarts | – |

Insgesamt 196 Neustarts, 0 verlorene Dauerklänge, 0 Unterläufe, längster Mischvorgang 1,4 ms; **66/66 Prüfungen**.

### 3.8 Neue Wesen und Dauerklänge (nach dem Zusammenführen mit `liminal-v13`, 534 Klänge)

Mit `liminal-v13` (Stand `0e586ef`) kamen Duller, Arms, Wrangler, Growler, Nguithr'xurh, Ouroboros, Plague Goblins, Erscheinungen und der Leuchtstab dazu – darunter neue Dauerklänge, die mit Lautstärke 0 beginnen und bei kleinem Pegel gestoppt werden (`growler_static`, `ngui_daze`, Wrangler-Schleifen), also genau das Muster von B-017. Wesen-Stresstest: je Ebene läuft der Autopilot, nacheinander werden alle 17 Wesen erzwungen (`presence <wesen> 10`, zwei Runden, je 5 s), nach jedem Wesen Audio-Status und Pegelprüfung; zehn Ebenen.

Zum Vergleich wurde derselbe Stand einmal mit dem alten Freigabeverhalten des Mischers gebaut (die drei Stellen der Behebung von B-017 abgeschaltet, nur für diesen Lauf, nicht eingecheckt):

| | altes Freigabeverhalten | neuer Stand |
|---|---|---|
| Ebenen mit vollen Stimmen (96/96) | 6 von 10 (Level 0, 1, 2, 3, 5, 903; dazu Level 36 mit 95) | 0 |
| hängende Stimmen (Spitze) | bis 94 | 0 |
| abgewiesene Klänge | bis 38 131 je Ebene (Summe 88 754) | 0 |
| gleichzeitige Stimmen (Spitze) | 96 | 38 |
| Pegelprüfungen | 340/340 | 340/340 |
| Unterläufe / vom Watchdog zurückgeholt | 0 / 0 | 0 / 0 |

Bemerkenswert: Auch mit vollen Stimmen bestand der alte Stand alle Pegelprüfungen – die Klangbetten, die vor dem Vollaufen gestartet waren, liefen weiter. Genau so äußert sich der Fehler: Die Atmosphäre ist da, aber Schritte, Wesen, Türen und alle neuen Dauerklänge fehlen. Deshalb misst die Stille-Erkennung allein diesen Fehler nicht; die Zahl der abgewiesenen Klänge und der hängenden Stimmen im Audio-Status (und der Gegentest in `audio_test`) tun es.

### 3.9 Störungen von außen

Zweimal wurden laufende Spiele dieses Auftrags von außen beendet: um 05:23:07 (UTC) drei Prozesse gleichzeitig (Fenstertest alt und neu, ein Ladelauf), einmal ein Start vor dem Laden der Ebene. Auf dem Rechner liefen gleichzeitig Arbeitsstände anderer Aufträge. Die Audio-Protokolle dieser Läufe sind bis zum Ende unauffällig. Seitdem schreibt das Spiel den Grund des Beendens ins Protokoll (`Beenden ueber das Menue` / `Fenster geschlossen`).

## 4. Ursachen

### Bestätigt (reproduziert, mit Zahlen)

**B-017 – Stimmenleck: gestoppte, unhörbare Schleifen blieben für immer belegt (Hauptursache).**
- Code (`AudioSystem::mix`, Stand V11 bis V13.2): Unhörbare Schleifen (`gain < 1e-5`) laufen in einem Sparzweig nur weiter (`continue`). Das 50-ms-Ausblenden, an dessen Ende `active = false` steht, gibt es nur im hörbaren Zweig. Wird eine Schleife gestoppt, während sie unhörbar ist, kommt sie dort nie an.
- Wann das passiert: Die Klanglandschaft stoppt Leuchten, Rohrstellen und Automaten genau dann, wenn sie außer Reichweite sind (`Soundscape::updateLamps`: Stopp, wenn eine nicht mehr gewollte Leuchte leise genug ist – sie ist dann jenseits ihres Radius, der Abstandsfaktor ist exakt 0). Dazu Klangbetten und Herzschlag, die mit Lautstärke 0 beginnen, und `stopAll()` beim Ebenenwechsel.
- `allocVoice` verdrängt nur einmalige Klänge, nie Schleifen. Sind alle 96 Stimmen mit hängenden Schleifen belegt, schlägt fast jedes `play` und jedes `startLoop` fehl – stumm, ohne Meldung.
- Beleg: Abschnitt 3.1 und 3.2; Gegentest `audio_test` („500 gestoppte unhörbare Schleifen“: neu 0 aktiv, 0 hängend).

**B-018 – Pufferunterläufe unter Last.**
- Der Mischer lief mit normaler Thread-Priorität neben 12 Arbeitsthreads der Welt- und Texturerzeugung (und hier anderen Prozessen). Er muss alle 10 ms liefern (Puffer 40 ms); verdrängt läuft der Puffer leer: Knacken, Aussetzer.
- Beleg: Abschnitt 3.2 und 3.3 – die Stände liefen gleichzeitig unter derselben Last: Ebenenwechsel alt 367 Unterläufe und eine Mischpause von 3 621 ms, neu 0 Unterläufe und höchstens 10,8 ms; Starts alt Unterläufe in 45 von 100, neu 0 in 1 000. Die längste Wartezeit des neuen Mischers auf die Sperre war 2,6 ms; die Pausen kamen also von der Priorität, nicht von der Sperre.
- Beim Start-Stresstest gefunden und behoben: Der Audiothread lief in 3 von 1 000 Starts erst 4–23 s nach dem Erzeugen an (er hatte bis zum Aufruf von MMCSS normale Priorität), und die Abfragen beim Audiodienst (Sitzungsmeldungen, Lautstärke) dauerten unter Last bis 4,4 s – in dieser Zeit blieb es still. Jetzt setzt der Thread zuerst `TIME_CRITICAL`, die Abfragen laufen in einem Hilfsthread (vor dem Start, höchstens 250 ms gewartet; bei laufendem Strom gestellt, kosteten sie je einen Unterlauf).

### Im Code belegt, Auslöser nur simuliert

Ein echtes Umstecken war nicht möglich: Der Rechner hat genau ein Ausgabegerät; Standardgerät oder Abtastrate in Windows umzustellen wäre eine Änderung der Systemeinstellungen, die ich nicht vornehmen darf. Die folgenden Fehler sind im alten Code eindeutig (fehlender Pfad); ihr Auslöser wurde über die Benachrichtigungen bzw. Fehlerpfade der API nachgestellt (Abschnitt 7). Ob sie beim Nutzer aufgetreten sind, ist offen.

**B-019 – Kein Neuverbinden bei Gerätewechsel, Formatwechsel oder stummem Treiber.**
- Kein `IMMNotificationClient`: Wurde ein anderes Standardgerät gewählt (Kopfhörer eingesteckt, USB, Bluetooth), spielte das Spiel weiter auf dem alten Gerät.
- Keine `IAudioSessionEvents`: Formatwechsel, exklusiver Modus eines anderen Programms, entferntes Gerät wurden nicht bemerkt.
- `if (WaitForSingleObject(ev, 200) != WAIT_OBJECT_0) continue;` – meldet der Treiber kein Ereignis mehr (das ist nach `AUDCLNT_E_DEVICE_INVALIDATED` der Fall), wartete der Thread für immer, ohne das Gerät je wieder zu prüfen. Stille bis zum Neustart des Spiels.
- Fehlercodes wurden nirgends protokolliert, Fehlschläge beim Öffnen auch nicht.

**B-020 – Klänge verworfen, solange das Gerät (neu) geöffnet wurde.**
- `play2d/play3d` brachen ab, solange `running_` falsch war: beim Start bis zum Öffnen (gemessen 100–1 000 ms nach der Klangerzeugung) und bei jedem Neuverbinden (Öffnen bis 2,8 s unter Last). In den Läufen dieses Auftrags trat das im alten Stand nicht auf (0 verworfene Klänge, weil das alte Programm nie neu verband und die Tests ohne Hauptmenü starten); es betrifft vor allem die Neuverbindungen, die es erst jetzt gibt.

**B-021 – Dämpfung (Schreck/Tod) blieb im Hauptmenü hängen.**
- Der Schreck setzt `setMuffle(0.75)`. Zurückgesetzt wurde nur in `updateAudio`, das im Hauptmenü nicht läuft. Wer nach dem Tod ins Hauptmenü ging, hörte die Menüklänge dumpf (Tiefpass rund 1,3 kHz). Ebenso blieb `setPaused` aus dem Pausenmenü stehen, bis eine neue Sitzung lief. Beleg: Codepfad; Gegentest `audio_test` („Pause + Dämpfung, dann resetMix“).

### Vermutungen und Härtungen (nicht beobachtet)

- **B-022 NaN im Mischer:** Keine Eingabe wurde geprüft. Eine NaN-Position hätte über Hall, Gleichspannungsfilter und Begrenzer den Ausgang dauerhaft ungültig gemacht (= Stille). In keinem Lauf beobachtet (0 ungültige Eingaben, 0 NaN in allen Protokollen). Jetzt abgewiesen und, falls doch, erkannt und zurückgesetzt.
- **B-023 Culling mit veraltetem Hörer:** `play3d` verwirft Quellen außer Reichweite; im ersten Bild nach Ebenenwechsel oder Wiedereinstieg stand der Hörer noch an der alten Stelle. Jetzt wird der Hörer vor den Klängen des Bildes gesetzt. Nicht gemessen.
- **B-024 Kurz keine neuen Klänge nach Minimieren (einmal, alter Stand):** Im Ebenenwechsel-Lauf des alten Stands wurden nach einem Minimieren/Wiederherstellen rund 22 s (11 Wechsel) keine Klänge gestartet; der Mischer lief weiter, die alten Schleifen blieben. Wahrscheinlich rechnete das Spiel in dieser Zeit keine Bilder (minimiertes Fenster), so dass auch die Klanglandschaft ruhte. In 20 weiteren Zyklen desselben Laufs, 20 Zyklen im neuen Lauf und je 34 Zyklen eines eigenen Fenstertests (alt und neu) nicht wieder aufgetreten. Ursache nicht geklärt; kein Audiofehler im engeren Sinn.

## 5. Behebung

| Fehler | Behebung | Datei |
|---|---|---|
| B-017 | Im Sparzweig: gestoppte unhörbare Schleife sofort frei (`if (v.stopping) { v.active = false; continue; }`). `allocVoice` nimmt zuerst eine ausblendende Stimme. Sicherheitsnetz: hängt eine Stimme länger als 0,5 s im Ausblenden, wird sie freigegeben und protokolliert (griff in keinem Lauf). | `src/audio/audio.cpp` |
| B-018 | Mischer-Thread sofort `THREAD_PRIORITY_TIME_CRITICAL`, dann MMCSS „Pro Audio“ (`AvSetMmThreadCharacteristicsW`); Puffer beim Öffnen mit Stille vorgefüllt; Abfragen beim Audiodienst im Hilfsthread. | `src/audio/audio.cpp` |
| B-019 | Geräte-Benachrichtigungen (`IMMNotificationClient`: Standardgerät, Zustand, Geräteformat), Sitzungs-Benachrichtigungen (`IAudioSessionEvents::OnSessionDisconnected`), jeder Fehlercode von `GetCurrentPadding/GetBuffer/ReleaseBuffer` sowie 500 ms ohne Treiberereignis führen zum Neuöffnen. Neue Versuche mit Rückstaffelung 100/250/500/1 000/2 000 ms, sofort bei einer Gerätemeldung. Lehnt Windows Stereo-float ab, wird das Mischformat des Geräts genommen (Mischer in dessen Rate, beliebige Kanalzahl, float/16/24/32 Bit). | `src/audio/audio.cpp` |
| B-020 | Klänge werden angenommen, sobald sie erzeugt sind. Ohne Gerät mischt ein Ersatztakt die vergangene Zeit und verwirft sie (Einzelklänge laufen ab, nichts staut sich). | `src/audio/audio.cpp` |
| B-021 | `resetMix()` (Pause aus, Dämpfung 0) beim Hauptmenü und bei jeder neuen Sitzung. | `src/audio/audio.cpp`, `src/app/app.cpp` |
| B-022 | Positionen, Lautstärken, Tonhöhen werden geprüft (ungültig: abgewiesen, gezählt). | `src/audio/audio.cpp` |
| B-023 | `setListener` vor den Klängen des Bildes (`App::handleEvents`). | `src/app/app.cpp` |

Beim Testen selbst gefunden und vor dem Abschluss behoben: Der erste Watchdog hielt ein langsames `IAudioClient::Initialize` (unter Last bis 2,8 s) für einen hängenden Thread und forderte einen überflüssigen zweiten Neustart an. Jetzt gilt der Thread erst nach 3 s ohne Herzschlag als hängend, und steckt er in einem Aufruf von Windows (Öffnen/Schließen), wird nur gewarnt.

## 6. Watchdog und Stille-Erkennung

Drei Ebenen, alle ohne Zutun des Spielers, jede mit Protokolleintrag:

1. **Im Audiothread:** Geräteverlust (Fehlercode), Meldungen von Windows (Standardgerät gewechselt, Gerät entfernt/deaktiviert, Geräteformat geändert, Sitzung getrennt mit Grund), 500 ms ohne Treiberereignis („Strom hängt“) → Strom schließen, neu öffnen. Stimmen und Schleifen bleiben erhalten; nach dem Neuöffnen klingt es weiter, wo es war (Gerätetest: 0 verlorene Dauerklänge in 200 Neustarts).
2. **Stille-Erkennung am Master-Ausgang** (im Mischer, bei offenem Gerät): Je Block wird neben dem Ausgang der *erwartete* Pegel berechnet (Energie jeder Stimme × ihre Verstärkung × Bus × Master). Liegt der Ausgang 1 s lang unter −100 dBFS, obwohl der über diese Zeit gemittelte erwartete Pegel über −60 dBFS liegt, schlägt sie Alarm: Hall, Filter und Begrenzer werden zurückgesetzt, der Strom neu geöffnet. Gewollte Stille (Master 0, nichts spielt, Pause) löst nichts aus. NaN/Inf am Ausgang: Block stumm, Zustand sofort zurückgesetzt. (Ein erster Entwurf verlangte den erwarteten Pegel in *jedem* Block; leise Stellen eines Klangbetts setzten den Zähler zurück und der Alarm kam bei Master 0,25 nie – deshalb jetzt gemittelt.)
3. **Im Hauptthread** (`AudioSystem::watchdog()`, je Bild aus `App::mainLoop`, auch minimiert): Herzschlag des Audiothreads; 3 s ohne Herzschlag → Neustart angefordert (noch nicht angelaufen, im Anlauf, Öffnen oder Schließen nur Warnung – dort hilft ein Neustart nicht). Strom offen, aber 1,5 s nichts gemischt → Neustart.

## 7. Was simuliert ist (ehrlich)

Echt ist in allen Fällen: der WASAPI-Strom auf dem echten Gerät, das Schließen und Neuöffnen, das Mischen in Echtzeit, die Pegelmessung am Master-Ausgang und – beim Abtastratenwechsel – ein echter Strom mit 44 100 Hz (Windows rechnet ins Geräteformat um, der Mischer läuft in 44 100 Hz) und zurück auf 48 000 Hz.

Simuliert ist nur der **Auslöser** (Skriptbefehl `audiofault <art> [wert]`, im Test `AudioSystem::injectFault`):

| Art | Was passiert | Echter Fall, der so nachgestellt wird |
|---|---|---|
| `invalidate` | der nächste `GetCurrentPadding` liefert `AUDCLNT_E_DEVICE_INVALIDATED` | Gerät abgezogen, Treiber neu geladen, Formatwechsel |
| `default` | `IMMNotificationClient::OnDefaultDeviceChanged` wird mit einer erfundenen Geräte-ID aufgerufen (derselbe Rückruf, den Windows aufruft) | Kopfhörer/USB/Bluetooth eingesteckt oder abgezogen, Standardgerät gewechselt |
| `format <Hz>` | `IAudioSessionEvents::OnSessionDisconnected(DisconnectReasonFormatChanged)`; der Strom öffnet danach in der angegebenen Rate | Abtastrate/Bittiefe in den Windows-Soundeinstellungen geändert |
| `exclusive <ms>` | `OnSessionDisconnected(DisconnectReasonExclusiveModeOverride)`, danach liefert `Initialize` so lange `AUDCLNT_E_DEVICE_IN_USE` | anderes Programm übernimmt das Gerät exklusiv |
| `remove <ms>` | `OnDeviceStateChanged(UNPLUGGED)`, so lange liefert `GetDefaultAudioEndpoint` `E_NOTFOUND`, dann `OnDeviceStateChanged(ACTIVE)` | einziges Gerät entfernt und wieder eingesteckt |
| `stall <ms>` | Ereignisse des Treibers werden so lange ignoriert | Treiber meldet sich nicht mehr (kommt nach Geräteverlust vor) |
| `mute` | Master-Ausgang wird bis zum Zurücksetzen des Mischers genullt | hängender stummer Mischerzustand |
| `nan` | NaN im Zustand des Gleichspannungsfilters | vergifteter DSP-Zustand |
| `initfail <n>` | die nächsten n `Initialize` liefern `AUDCLNT_E_UNSUPPORTED_FORMAT` | Treiber ohne automatische Formatumrechnung |

Nicht simuliert und nicht geprüft: ein echtes Umstecken (nur ein Ausgabegerät vorhanden), ein echter Wechsel der Geräte-Abtastrate in Windows und ein echtes Programm im exklusiven Modus (beides Änderungen an Systemeinstellungen, die ich nicht vornehmen darf), Bluetooth-Geräte, mehrkanalige Geräte (der Pfad für das Mischformat mit beliebiger Kanalzahl ist im Gerätetest nur mit dem 2-Kanal-Gerät gelaufen), ein Neustart des Windows-Audiodienstes.

## 8. Testergebnisse (Endstand, nach dem Zusammenführen)

Alle Programme aus `build/Release/bin`, Endstand dieses Branches:

| Test | Ergebnis |
|---|---|
| `audio_test data` | 121/0 (vorher 106 Prüfungen; Klangbank geteilt: 42 s → 8–19 s) |
| `audio_device_test` (echtes Gerät) | 66/66 (Abschnitt 3.7) |
| `gameplay_test data` | 647/0 |
| `worldgen_test data/legacy/levels/level0.json ../../../tests/data/v4_reference.json` | 29 658/0 |
| `supply_balance_test data` | OK (0 Fehler) |
| `net_protocol_test` | 72/0 |
| `net_transport_test` | 90/0 |
| `coop_test data --quick` | 209/0 (ein früherer Lauf unter Volllast durch vier parallele Spielstarts hatte 2 wechselnde Zeitfehler in Koop-Spielabläufen, die das Audio nicht berühren; ohne diese Last grün) |

Neuer Test für den Fehler: `audio_test` „500 gestoppte unhörbare Schleifen“ (vorher liefen die Stimmen nach 96 Schleifen voll, jetzt 0 aktiv, 0 hängend, 0 abgewiesen) und „Ebenenwechsel 200 ×“ (nach jedem Wechsel alle Stimmen frei). Dazu Stille-Erkennung (Alarm nach 1 s, kein Fehlalarm bei Master 0, Pause, Stille), NaN, ungültige Eingaben, Pause/Dämpfung, erwarteter gegen gemessenen Pegel.

Stresstests im Spiel (Abschnitt 3): 1 000/1 000 Starts, 220/220 Ebenenwechsel-Prüfungen, 340/340 Wesen-Prüfungen, 150/150 Fenster/Fokus/Pause, 49/50 Ladevorgänge (1 von außen beendet), 10/10 Koop-Sitzungen mit 20/20 Prüfungen.

## 9. Geprüfte und ausgeschlossene Ursachen

| Verdacht | Ergebnis |
|---|---|
| Race Condition bei der Initialisierung / asynchrones Laden | Die Klänge werden vor dem Start des Audiothreads vollständig erzeugt (`synthesizeAll` wartet auf alle Jobs); der Thread liest danach nur. Kein Wettlauf. Was es gab: Klänge vor dem Öffnen des Geräts wurden verworfen (B-020). |
| Fehler beim Laden einzelner Klänge | 0 Meldungen „Klang fehlt“ in allen Protokollen (2 100 Starts, 440 Ebenenwechsel, Autopilot, Koop). Eigene WAV-Dateien, die nicht lesbar sind, werden jetzt gemeldet statt still übergangen. |
| Stimmenlimit/erschöpfte Kanäle | **Ursache** (B-017). Nach der Behebung höchstens 35 gleichzeitige Stimmen (Abschnitt 3.1), 0 Abweisungen in allen Läufen (3 458 Statuszeilen). |
| Falsches Culling bei 3D-Quellen | Das Culling selbst ist richtig (jenseits der Reichweite unhörbar). Der Fehler war, dass gestoppte unhörbare Quellen nie frei wurden (B-017). Veralteter Hörer im ersten Bild nach einem Wechsel: gehärtet (B-023). |
| Lautstärke-/Mute-Zustand bleibt nach Levelwechsel oder Pause hängen | Im Spiel setzt `updateAudio` Pause und Dämpfung jedes Bild neu; nach 220 Ebenenwechseln mit Pause, Fokusverlust und Minimieren keine hängende Pause/Dämpfung (Zeile „Master … Pause aus Dämpfung 0.00“ im Protokoll). Im Hauptmenü hing die Dämpfung (B-021). |
| Verlorenes Gerät ohne Neuinitialisierung | **Ursache** im Code (B-019): ohne Treiberereignis keine Prüfung, keine Geräte-Benachrichtigung. |
| Exklusiver Gerätemodus | Das Spiel öffnet im geteilten Modus (`AUDCLNT_SHAREMODE_SHARED`), blockiert also niemanden. Übernimmt ein anderes Programm das Gerät exklusiv, wird das jetzt gemeldet und nach dessen Ende neu geöffnet (simuliert). |
| Threading Spiel-/Audiothread | Alle gemeinsamen Daten liegen unter einer Sperre. Längste gemessene Wartezeit des Mischers auf die Sperre in allen Läufen: 2,6 ms (Autopilot), sonst unter 1,1 ms – keine Ursache. Die Aussetzer kamen von der Priorität (B-018). |
| Pufferunterläufe | **Ursache** unter Last (B-018). |
| Stummschaltung in Windows | Kein Fehler des Spiels; auf dem Testrechner war der Lautsprecher in Windows stumm geschaltet. Wird jetzt beim Öffnen als Warnung protokolliert. |

## 10. Was nicht geprüft werden konnte

- Echtes Umstecken eines Geräts, ein zweites Ausgabegerät, Bluetooth, ein echter Wechsel der Geräte-Abtastrate in Windows, ein Programm im exklusiven Modus, ein Neustart des Windows-Audiodienstes (Gründe in Abschnitt 7).
- Der Rückfall auf das Mischformat des Geräts lief nur mit diesem Gerät (2 Kanäle, float); andere Kanalzahlen und PCM-Formate sind im Code behandelt, aber nicht an echter Hardware geprüft.
- Hörprobe: Der Lautsprecher war in Windows stumm geschaltet. Alle Aussagen über „Klang“ beruhen auf der Pegelmessung am Master-Ausgang des Spiels, nicht auf Hören.
- Ein dauerhaft hängender Treiberaufruf kann vom Watchdog nur gemeldet, nicht abgebrochen werden (ein Thread lässt sich nicht sicher aus einem Windows-Aufruf holen); in keinem Lauf aufgetreten.

## 11. Geänderte Dateien, Befehle, Tests

| Datei | Änderung |
|---|---|
| `src/audio/audio.hpp`, `src/audio/audio.cpp` | Stimmenleck behoben, Gerätethread neu (Benachrichtigungen, Fehlerpfade, Neuöffnen mit Rückstaffelung, Ersatztakt ohne Gerät, Rückfall auf das Mischformat, Hilfsthread für Sitzungsmeldungen), MMCSS/TIME_CRITICAL, Watchdog, Stille-Erkennung, NaN-Schutz, Eingabeprüfung, `AudioStats`/`statsLine`, `resetMix`, `injectFault`, geteilte Klangbank (`initShared`) |
| `src/app/app.cpp`, `src/app/app.hpp` | Watchdog je Bild, `resetMix` bei Hauptmenü/neuer Sitzung, Hörer vor den Klängen des Bildes, Audio-Status vor jedem Wechsel und beim Beenden, Grund des Beendens, Skriptbefehle |
| `tests/audio_test.cpp` | +15 Prüfungen (Stimmenleck, 200 Ebenenwechsel ohne Gerät, Stille-Erkennung mit und ohne Fehlalarm, NaN, ungültige Eingaben, Pause/Dämpfung, erwarteter Pegel); Klangbank geteilt (42 s → 19 s) |
| `tests/audio_device_test.cpp` (neu), `CMakeLists.txt` | Gerätetest: Start-Stresstest, Geräte-/Fehlerpfade, Normalbetrieb ohne Fehlalarm |
| `AUDIO_BUG.md` (neu), `BUGLISTE.md`, `AUDIO_DESIGN.md` | Dokumentation |

Neue Skriptbefehle (`--script`):

| Befehl | Wirkung |
|---|---|
| `audiostat` | Audio-Status ins Protokoll (auch `audioinfo` schreibt ihn jetzt mit) |
| `audiolog <s>` | Audio-Status alle s Sekunden (0 aus) |
| `audioexpect <dBFS>` | Prüfung: Gerät offen und RMS am Master-Ausgang (letzte Sekunde) mindestens so hoch, sonst schlägt das Skript fehl |
| `audiofault <art> [wert]` | Fehlerpfad auslösen: `invalidate`, `default`, `format <Hz>`, `exclusive <ms>`, `remove <ms>`, `stall <ms>`, `mute`, `nan`, `initfail <n>` |
| `audiorestart` | Strom schließen und neu öffnen |
| `level <id> [anker]` | Ebenenwechsel wie durch einen Ausgang |
| `focus off\|on` | Fokusverlust/-rückkehr (Alt+Tab) über `WM_ACTIVATE`; öffnet wie im Spiel die Pause |
| `minimize`, `restore` | Fenster minimieren/wiederherstellen |
| `resume` | Pausenmenü schließen |

Tests:
- `../tests/audio_test.exe data` – ohne Gerät (Mischer, Stimmen, Stille-Erkennung, NaN).
- `../tests/audio_device_test.exe [starts] [wiederholungen] [protokoll]` – mit dem echten Standardgerät, spielt leise (Master 0,25) Klänge ab; Standard 200 Starts, 20 Wiederholungen je Fehlerart, Dauer etwa 8 min.
