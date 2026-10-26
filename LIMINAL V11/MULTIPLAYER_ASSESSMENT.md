# LIMINAL V11 – Mehrspieler: Machbarkeit und Bedrohungsanalyse

> **Ergebnis vorweg:** In V11 wird **kein** Mehrspielermodus gebaut. Die nötige Sicherheit lässt sich in dieser Umgebung nicht nachweisen, und die Tests, die dafür nötig wären, kann ich hier nicht durchführen (Abschnitt 6). Dieses Dokument beschreibt, was ein sicherer Mehrspielermodus bräuchte, welche Angriffe zu erwarten sind und in welcher Reihenfolge man ihn später bauen könnte. Es behauptet nirgends, ein Mehrspielermodus wäre sicher.

## 1. Ausgangslage im Code (Stand V11)

| Bereich | Zustand | Folge für Mehrspieler |
|---|---|---|
| Netzwerk | Es gibt keinen Netzwerkcode (keine Sockets, keine Bibliothek, kein Protokoll). | Alles wäre neu. |
| Spielzustand | `GameSession` hält **einen** Spieler (`player_`), sein Inventar, seine Werte, die Entities (`Presence`), Stromausfall, Sicherungen und Sicherungspunkte. | Die Sitzung müsste mehrere Spieler kennen; viele Regeln setzen genau einen Spieler voraus. |
| Takt | Das Spiel rechnet mit variabler Bildzeit, nur Tests und Aufnahmen nutzen `--fixed-dt`. | Ein Server braucht einen festen Simulationstakt. |
| Darstellung und Logik | Logik (`gameplay`, `world`) und Darstellung (`render`, `ui`, `app`) sind als Bibliotheken getrennt. Die Sitzung liefert aber auch Kamera, Körperpose und Klänge. | Die Trennung ist ein guter Anfang; für einen Server ohne Grafik wären weitere Schnitte nötig. |
| Entities | Lächeln und Hund bewegen sich nur, wenn **der** Spieler sie nicht ansieht; Halluzinationen und Sanity hängen an einem Spieler. | Diese Regeln müssen für mehrere Blicke neu entworfen werden (wer sieht was? wessen Sanity?). |
| Spielstände | lokale JSON-Dateien im Benutzerordner | Im Mehrspieler gehört der Stand auf den Server. |

## 2. Was „sicher“ hier heißen müsste

Vorgabe: **Der Client entscheidet nicht über kritische Spielzustände.** Daraus folgt eine serverautoritative Architektur:

- Der Client schickt nur **Eingaben** (Bewegungsabsicht, „E gedrückt“, „Slot 3 benutzen“), keine Ergebnisse.
- Der Server rechnet Bewegung mit Kollision, Aufnehmen (Abstand und Sichtlinie), Inventar, Health, Sanity, Tod, Wiedereinstieg, Sicherungen, Aufzüge und alle Entities.
- Der Client sagt nur Bewegung voraus (Prediction) und gleicht sie mit dem Server ab; alles andere zeigt er so an, wie der Server es meldet.
- Der Server schickt jedem Client nur, was dieser wissen darf (Interessenbereich), damit Positionen verborgener Wesen nicht ausgelesen werden können.

## 3. Mögliche Architekturen

| Variante | Beschreibung | Bewertung |
|---|---|---|
| **A: Gegenseitige Gleichschritt-Simulation (P2P-Lockstep)** | Alle rechnen alles, tauschen Eingaben aus | **Ausgeschlossen:** Jeder Client bestimmt mit, ein manipulierter Client kann alles verfälschen; IP-Adressen aller Teilnehmer sind sichtbar. |
| **B: Ein Spieler hostet (Listen-Server)** | Ein Client ist zugleich Server | Für Koop unter Freunden üblich. Der Host kann betrügen, seine IP-Adresse ist für die anderen sichtbar (oder ein Relay ist nötig). Sicherheit hängt am Host-Rechner. |
| **C: Eigener Server (dedicated)** | Ein Serverprogramm ohne Grafik, Clients verbinden sich | Am ehesten sicher zu machen. Braucht einen Server-Build, Betrieb, Updates, Datenschutz und Kosten. |

Empfehlung für einen späteren Versuch: **C**, als ersten Schritt nur im **lokalen Netz** und hinter einem Schalter.

## 4. Bausteine, die nötig wären

1. **Simulation ohne Grafik:** `GameSession` mit festem Takt (z. B. 30 Hz) ohne Abhängigkeit von Renderer, Audio und Fenster; mehrere Spieler je Sitzung.
2. **Transport:** UDP mit Verbindungsaufbau, Zuverlässigkeit für wichtige Nachrichten und Verschlüsselung. Mögliche Bibliotheken: ENet (MIT), Valve GameNetworkingSockets (BSD-3-Clause), yojimbo (BSD-3-Clause). Die Lizenzen stehen so in den Angaben der Projekte; vor einer Übernahme sind sie erneut zu prüfen und in `ATTRIBUTION.md` einzutragen. **Übernommen wurde nichts.**
3. **Protokoll:** versionierte Nachrichten mit festen Höchstlängen, ein Decoder mit Grenzprüfung für jedes Feld, keine Zeiger und keine Längen aus dem Paket ohne Prüfung.
4. **Replikation:** Spielerpositionen (interpoliert), Entities, Funde (aufgenommen oder liegend), Türen und Aufzüge, Stromausfall, Sicherungen; Ereignisse für Klänge und Effekte.
5. **Neue Regeln für mehrere Spieler:**
   - Wann darf sich das Lächeln bewegen? (Vorschlag: nur, wenn **niemand** hinsieht.)
   - Wen jagt der Hund?
   - Sanity und Halluzinationen bleiben je Spieler und werden vom Server gerechnet; die Darstellung ist lokal.
   - Wiedereinstieg und Sicherungspunkte gelten je Spieler, die Sicherungen für alle.
6. **Sitzung und Identität:** Beitrittscodes oder Einladungen, Sitzungsschlüssel je Verbindung, keine Konten (also keine Passwörter auf unserer Seite).
7. **Betrieb:** Serverprogramm, Aktualisierung bei Sicherheitslücken, Protokolle, Datenschutzhinweis.

## 5. Bedrohungsanalyse (nach STRIDE)

| Bedrohung | Beispiel in LIMINAL | Gegenmaßnahme | Restrisiko |
|---|---|---|---|
| **Spoofing** (Identität vortäuschen) | fremdes Paket mit der Adresse eines Mitspielers; Übernahme einer Sitzung | Handshake mit Zufallscookie vor jeder Antwort, Sitzungsschlüssel, verschlüsselte und authentisierte Pakete | gestohlene Beitrittscodes |
| **Tampering** (Manipulation) | Geschwindigkeits- oder Teleport-Hack, durch Wände gehen, Items doppeln, „unsterblich“ | Server rechnet Bewegung samt Kollision, prüft Aufnahme-Abstand und Sichtlinie, Inventar nur auf dem Server, Raten- und Plausibilitätsgrenzen | Host-Betrug bei Variante B |
| **Repudiation** (Abstreiten) | „Ich habe nichts manipuliert“ | Serverprotokoll der Eingaben und Entscheidungen (ohne unnötige personenbezogene Daten) | gering |
| **Information Disclosure** (Datenabfluss) | Wallhack: Positionen verborgener Wesen und Funde aus dem Speicher lesen; IP-Adressen der Mitspieler | Interessenbereich: nur Sichtbares und Hörbares senden; Relay oder Server statt direkter Verbindungen | Klänge verraten Richtung (gewollt) |
| **Denial of Service** (Überlastung) | Paketflut, Verstärkung über UDP (Reflexion), riesige oder kaputte Pakete, viele halboffene Verbindungen | Antwort erst nach Handshake, Grenzen für Größe, Rate und Verbindungen, Zeitlimits | volumetrische Angriffe auf den Server-Host |
| **Elevation of Privilege** (Codeausführung) | Speicherfehler beim Lesen eines Pakets in C++ führt zu fremdem Code auf dem Rechner eines Spielers | Decoder mit Grenzprüfung, keine rohen Kopien aus dem Paket, Fuzzing aller Decoder, Sanitizer-Builds, Compiler-Härtung | **das höchste Risiko eines selbstgeschriebenen C++-Netzcodes** |
| Datenschutz | IP-Adressen sind personenbezogene Daten (DSGVO) | Datensparsamkeit, Hinweis, Speicherfristen | rechtliche Bewertung nötig, hier nicht geleistet |

## 6. Was für einen Nachweis nötig wäre – und warum er hier nicht möglich ist

| Nachweis | Nötig, weil … | Hier möglich? |
|---|---|---|
| Fuzzing jedes Decoders (lange, mit Abdeckungsmessung, null Abstürze) | Speicherfehler sind der gefährlichste Angriffsweg | **Technisch teilweise:** Die Toolchain (llvm-mingw) enthält libFuzzer- und AddressSanitizer-Laufzeiten (geprüft: `libclang_rt.fuzzer-x86_64.a`, `libclang_rt.asan_dynamic-x86_64.dll.a`). Sinnvoll ist Fuzzing aber erst, wenn es ein Protokoll gibt, und es ersetzt die übrigen Nachweise dieser Tabelle nicht. Ausprobiert wurde es hier nicht. |
| Sanitizer-Builds (AddressSanitizer, UndefinedBehaviorSanitizer) | finden Speicher- und Ganzzahlfehler | **Ungeprüft:** Die ASan-Laufzeit liegt vor; ob ein Sanitizer-Build des ganzen Spiels unter Windows läuft, wurde nicht getestet. |
| Tests mit mehreren echten Clients über ein echtes Netz (Paketverlust, Latenz, NAT) | Prediction, Abgleich und Zeitlimits müssen unter echten Bedingungen halten | **Nein:** nur ein Rechner, kein zweites Netz |
| Last- und Flutungstests gegen einen Server | DoS-Schutz | **Nein:** keine Server-Infrastruktur |
| Prüfung durch Dritte (Penetrationstest) | Eigenprüfung übersieht eigene Denkfehler | **Nein** |
| Rechtliche Prüfung Datenschutz | IP-Adressen, Protokolle | **Nein** |

Weil diese Nachweise fehlen, gilt die Vorgabe: **kein Mehrspieler in V11.** Ein Prototyp ohne diese Nachweise wäre ein Platzhalter mit unbekanntem Risiko für die Rechner der Mitspieler.

## 7. Empfohlener Weg für später

1. **Festen Simulationstakt und Trennung der Sitzung von Darstellung und Audio** einführen. Das lohnt sich auch ohne Mehrspieler: Wiederholbarkeit, Tests, Aufnahmen.
2. **Spielregeln für mehrere Spieler** entwerfen und im Einzelspieler mit simulierten zweiten Spielern testen (Lächeln, Hund, Sanity, Wiedereinstieg).
3. **Netzwerkbibliothek** wählen (Lizenz prüfen, `ATTRIBUTION.md`). Darauf einen **Koop-Prototyp nur im lokalen Netz**: serverautoritativ, hinter einem Schalter, standardmäßig aus.
4. **Härtung:**
   - Decoder mit Grenzprüfung, dazu Fuzzing (libFuzzer) und Sanitizer-Builds; die Laufzeiten liegen der Toolchain bei, die Einrichtung ist noch zu erproben;
   - Plausibilitätsprüfungen auf dem Server samt Tests (Teleport, Tempo, Items doppeln, Aufnehmen durch Wände);
   - Ratenbegrenzung und Handshake.
5. **Externe Prüfung** und Datenschutzhinweis, **bevor** irgendetwas über das Internet spielbar ist.

Grobe Größenordnung (Schätzung, nicht gemessen): einige Monate Arbeit für eine Person, davon ein erheblicher Teil für Härtung und Tests, nicht für den Netzcode selbst.

## 8. Was in V11 tatsächlich gemacht wurde

- Bestandsaufnahme des Codes (Abschnitt 1) und diese Analyse.
- **Kein Netzwerkcode, keine Bibliothek, keine offenen Ports.** Das Spiel baut weiterhin keine Netzverbindung auf.
