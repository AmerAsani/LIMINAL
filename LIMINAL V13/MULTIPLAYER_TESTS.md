# LIMINAL V13 – Tests Mehrspieler

Stand 26.10.2026. Alle Tests sind wiederholbar; Befehle aus dem Ordner `LIMINAL V13` (Werkzeuge aus `.toolchain`).

## 1. Prüfschicht des Protokolls (`net_protocol_test`)

```
cmake --build build/Release --target net_protocol_test
build/Release/tests/net_protocol_test.exe [fuzz-runden]
```

| Bereich | Prüfungen | Ergebnis |
|---|---|---|
| Hin und zurück: Hello, Welcome, Input, Snapshot (leer, voll), Bye, Ping – angenommen, dieselben Bytes zurück, feste Größen | 22 | bestanden |
| Gegentests: jede Kürzung, jedes Anhängsel (1–63 Byte), über 1200 Byte, leeres Paket, falsche Kennung, unbekannter Typ (0, 200) | 7 | bestanden |
| Rolle: Gast sendet Zustand oder Welcome, Host sendet Eingabe → abgelehnt | 3 | bestanden |
| Zustand: Eingabe vor dem Handshake, zweites Hello, nach dem Schließen → abgelehnt | 3 | bestanden |
| Bereiche: Achse −128, Blickänderung 251, Neigung −32768, unbekannte Taste, Snapshot mit falscher Anzahl, mehr als 4 Spielern, doppelter Kennung, Koordinate außerhalb (Achse −128 auch beim Schreiben) | 9 | bestanden |
| Name: leer, 17 Zeichen, Steuerzeichen, Nicht-ASCII, Daten hinter dem Namen; falsche Version | 6 | bestanden |
| Fuzzing: 1 000 000 Pakete (Zufallsbytes, Bitfehler, Kürzungen) × 6 Kombinationen aus Rolle und Zustand | 1 | bestanden: kein Absturz; 125 487 Annahmen, alle kanonisch (ergeben genau dieselben Bytes) |
| Rate (Marken-Eimer), Uhr rückwärts | 3 | bestanden |
| Folgenummern: doppelt, alt, Sprung über 2²⁰, Überlauf der Nummer | 3 | bestanden |
| Gegenstelle: falscher Sitzungscode → Trennung nach 8 Versuchen; getrennt bleibt getrennt; Handshake, Eingaben, vertauschte Pakete (zählen nicht als Fehler); Zeitlimits 5 s / 10 s; Flut (10 000 Pakete in 1 s → höchstens 80 angenommen, dann Trennung); Gast-Seite: Zustand vor Welcome, Welcome, Zustand, Host sendet Eingabe | 15 | bestanden |
| **Summe** | **72** | **OK: 72 Prüfungen, 0 Fehler** |

## 2. Dieselben Tests unter Sanitizern

```
clang++ -std=c++20 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc tests/net_protocol_test.cpp src/net/protocol.cpp
```

- Ergebnis mit 1 000 000 Fuzz-Runden: **OK: 72 Prüfungen, 0 Fehler**, keine Meldung von AddressSanitizer oder UndefinedBehaviorSanitizer.
- Gegenprobe, dass der Sanitizer wirklich arbeitet: ein absichtlicher Lesezugriff hinter einem Vektor meldet `heap-buffer-overflow`.

## 3. Kein Netzwerkcode im Spiel (`tools/check_no_network.py`)

```
python tools/check_no_network.py
```

| Programm | Ergebnis |
|---|---|
| `LiminalGame.exe` | 21 importierte DLLs, keine Netzwerk-DLL, keine Netzwerk-Zeichenkette (auch nicht als UTF-16 für `LoadLibrary`) |
| `LIMINAL.exe` (Starter) | 5 importierte DLLs, keine Netzwerk-DLL; bekannte Ausnahme `urlmon`/`URLDownloadToFile`: Download des Visual-C++-Pakets von Microsoft, nur auf alten Windows-Versionen und nur nach Rückfrage |
| Gesamt | **SAUBER** |

## 4. Signaturprüfung des Starters (B-003)

Die Prüfung aus `launcher/launcher.cpp` (gleicher Code, als Testprogramm gebaut) gegen drei Dateien:

| Datei | WinVerifyTrust | Herausgeber | Ergebnis |
|---|---|---|---|
| Microsoft Edge (`msedge.exe`) | 0 (gültig) | Microsoft Corporation | angenommen |
| Blender 4.5 (`blender.exe`) | 0 (gültig) | Blender Foundation | **abgelehnt** (vor V13: angenommen) |
| `LiminalGame.exe` (unsigniert) | 0x800B0100 | – | abgelehnt |

## 5. V13.2: Koop-Modus (Beta)

Seit V13.2 gibt es den Modus (`MULTIPLAYER_ENTSCHEIDUNG.md`, Stand V13.2). Die Tests laufen mit dem echten Netzweg: UDP über 127.0.0.1, ENet, Handshake, AES-256-GCM, Spielprotokoll v2 und echte Spielsitzungen ohne Fenster (Bots).

### 5.1 Transport (`net_transport_test`)

| Bereich | Ergebnis |
|---|---|
| Testvektoren: HMAC-SHA256 (RFC 4231, Fälle 1 und 2), HKDF-SHA256 (RFC 5869, Fall 1), AES-256-GCM (GCM-Spezifikation, Fälle 13 und 14), Manipulation von Geheimtext, Tag und Zusatzdaten | bestanden |
| Verbindung: Handshake, Name, beide Kanäle, 1200 Byte (1201 abgelehnt), 200 zuverlässige Nachrichten vollständig und geordnet, Trennung gemeldet | bestanden |
| Falscher Schlüssel: 5 Beweise abgelehnt, danach Sperre auch mit richtigem Schlüssel, nach Ablauf wieder Zugang | bestanden |
| Voll, falsche Version, Zufallsbytes vor dem Handshake (getrennt), 20 halboffene Verbindungen (höchstens 3 je Adresse warten, der Rest sofort abgewiesen, alle nach dem Zeitlimit getrennt, danach tritt ein echter Gast bei) | bestanden |
| Handshake von Hand: Wiederholung desselben Pakets, alter Zähler, veränderte Nachricht, Nachricht auf falschem Kanal – alle verworfen; Flut von 2000 Nachrichten: höchstens Rate plus Vorrat angenommen | bestanden |
| **Summe** | **OK: 52 Prüfungen, 0 Fehler** (dreimal hintereinander; auch unter ASan/UBSan, 5.4) |

### 5.2 Koop mit Bots (`coop_test`)

```
build/Release/tests/coop_test.exe data                 # alle Teile (etwa 12 min)
build/Release/tests/coop_test.exe data --only <teil>   # bots, pickup, save, full, attacks, death, journey, reconnect, fuzz, netfuzz
build/Release/tests/coop_test.exe data --long 3600     # Langlauf
```

Der Netzstörer ist ein UDP-Proxy im Test. Er stellt Laufzeit, Schwankung, Verlust, Doppelte, Umordnung und Bandbreite je Richtung ein.

| Teil | Was | Ergebnis |
|---|---|---|
| Schlüssel | Base32 hin und zurück, Kleinbuchstaben, zu kurz, zu lang, fremdes Zeichen | bestanden |
| Fuzzing Protokoll | 240 000 Fälle (Zufall, veränderte Vorlagen; beide Rollen, beide Zustände): kein Absturz, jede angenommene Nachricht kanonisch; größte Momentaufnahme 1 155 Byte (Protokoll v3: 24 Wesen mit je 18 Animationswerten, 3 Mitspieler; v2: 1 059 Byte); Steuerzeichen, Richtungsumkehr (U+202E), überlange UTF-8-Formen und Pfadzeichen werden nicht kodiert; falsche Rolle, falscher Zustand, 1201 Byte | bestanden |
| Bots direkt | Host + 1, Host + 3 (vier Spieler), je 25–30 s mit Zufallsbewegung, Lampe, Aufnehmen, Benutzen, Ablegen; ein Hound beim Host | bestanden: Vorhersage im Mittel 1,8–2,1 cm daneben, über 10 cm in 1–2 % der Momentaufnahmen; nach dem Anhalten Host und Gast höchstens 15 cm auseinander; geteilter Zustand gleich |
| LAN | 20 ms, ±5 ms, 1 % Verlust | bestanden: 2,5 cm im Mittel; 2,8 kB/s zum Gast, 5,0 kB/s zum Host je Gast |
| Internet | 80 ms ±30 ms, 3 % Verlust, 1 % Doppelte, 2 % Umordnung, 256 kbit/s | bestanden: 2,7 cm im Mittel; 3,0 / 5,1 kB/s |
| Schlechtes Netz | 250 ms ±80 ms, 10 % Verlust, 5 % Umordnung, 128 kbit/s | bestanden: 4,1 cm im Mittel; einzelne Sprünge (über 2 m) |
| Ablegen, Aufnehmen, Wettlauf | Figur legt ab → bei Host und Gast; Gast nimmt auf (Ziel aus der eigenen Sicht), ein zweiter greift im selben Bild nach derselben Kennzahl und bekommt sie nicht; Benutzen wirkt beim Host; die Hound-Puppe folgt dem Original (100 ms zurück) | 12/12 |
| Koop-Stand | Host speichert Werte und Inventar eines Gasts nach Namen; neuer Host mit dem Stand gibt sie einem Gast mit demselben Namen zurück, einem anderen nicht | 5/5 |
| Voll | Host + 3, ein fünfter wird abgewiesen, die anderen bleiben | bestanden |
| Angriffe (Gast mit Schlüssel) | falsche Rolle, zweiter Beitritt, falsche Länge, unbekannter Typ, Rest hinter der Nachricht → nach 8 getrennt; zu schnelle Uhr (6-fache Zeit) → Eingaben verworfen, höchstens Laufweite; erfundene und ferne Gegenstände abgelehnt; Flut von Absichten gedrosselt; andere Spieldaten abgewiesen; der ehrliche Gast spielt ungestört | 11/11 |
| Tod | Gast stirbt (Host und Gast), sieht dem Host zu, nach 20 s zurück; alle tot → Neubeginn am Sicherungspunkt | 5/5 |
| Reise | Host + 2 Gäste wechseln als Gruppe durch alle zehn Ebenen (12 Wechsel, Sackgassen zuletzt): gleiche Ebene, gleicher Seed, gleiche Vorräte, beisammen am Anker, verbunden | 50/50 |
| Wiederverbinden, Kick, Bann | Verbindung verloren → mit Token zurück: gleiche Spielernummer, Inventar zurück; entfernen; sperren → erneuter Versuch abgewiesen | bestanden |
| Fuzzing über den Netzweg (`--only netfuzz`, 300 s) | Ein Angreifer mit gültigem Schlüssel schickt über 768 Verbindungen 8 134 veränderte und zufällige Nachrichten an den echten Host (Transport, Handshake, Spielprotokoll); daneben spielt ein ehrlicher Gast | **OK: 2/2** – kein Absturz, der ehrliche Gast bleibt verbunden; Host: 54 Nachrichten abgelehnt, 307 gedrosselt, von 20 969 Eingaben 1 226 verworfen (Zeitbudget), von 505 Absichten 326 abgelehnt, 6 Trennungen |
| **Summe (alle Teile)** | | **OK: 169 Prüfungen, 0 Fehler** (`--quick` und vollständig, Protokoll v3; dazu Koop-Stand 5/5 und Netz-Fuzzing 2/2) |

#### Langlauf (`--long`)

Vier Spieler (Host und drei Bots) über den Netzstörer (Internet-Profil), Zufallsbewegung, Lampe, Aufnehmen, ein Hound beim Host.

| Lauf | Ergebnis |
|---|---|
| 60 min (Protokoll v2) | **OK: 9/9**. Vorhersage im Mittel 3,1 cm daneben, über 10 cm in 12 570 von 191 540 Momentaufnahmen (6,6 %); 105 714 Korrekturen, 4 Sprünge (Abgleich über 2 m); größte Abweichung 89,9 m – **Ursache nicht untersucht**: vermutlich ein Ortswechsel eines Bots (Übergang), den der Gast erst mit dem Abgleich übernimmt; im 30-min-Lauf trat kein Sprung auf (offener Punkt, BUGLISTE B-012); Host lehnte 740 von 1 653 Absichten ab (Bots greifen nach Fernem), 12 766 Eingaben verworfen (Zeitbudget); 2,9 kB/s zum Gast, 5,0 kB/s zum Host je Gast; Speicher 96 → 191 MB |
| 30 min mit Speicherproben (Protokoll v3, mit den neuen Wesen) | **OK: 9/9**. Speicher nach 0/5/10/15/20/25/30 min: 108 / 163 / 198 / 213 / 219 / 224 / 214 MB, Sektoren beim Host durchgehend 12. Der Speicher wächst in der ersten Viertelstunde (vier Sitzungen in einem Prozess laden Welt, Wege und Klänge) und bleibt dann bei 215–225 MB stehen – **kein fortlaufendes Leck festgestellt**. Vorhersage 2,9 cm im Mittel, 0 Sprünge, größte Abweichung 1,44 m |

### 5.3 Zwei echte Spielprozesse

`coop_shots.ps1` (Arbeitsordner) startet zwei `LiminalGame.exe`: Der Host öffnet per Skriptbefehl `coophost`, der Gast tritt per `coopjoin` mit dem Schlüssel aus einer Datei bei. Beide nehmen Bilder aus der Schulteransicht auf (`docs/screens/koop/`).

| Prüfung | Ergebnis |
|---|---|
| Beitritt, Ebene, beide Figuren in beiden Sichten | bestanden (`koop_hell_host_gast.jpg`) |
| Lampen beider Spieler im dunklen Zimmer (eigenes Spotlicht je Mitspieler) | bestanden (`koop_dunkel_lampen_host_gast.jpg`) |
| Namen über den Mitspielern, Statuszeile und Spielerliste (Beta) | bestanden |
| Beenden beider Prozesse | Exit 0 (vorher Absturz, B-009) |
| Bildzeiten (V13.2, `coop_perf.ps1`): Level 1, 1280 × 720, Qualität 2, Echtzeit, Lampe an, je 10 Messungen (Befehl `perf`); Host und Gast als zwei Prozesse auf **einem** Rechner (teilen sich CPU und Grafikkarte) | Spiellogik je Bild: Einzelspieler 0,03–0,50 ms, Koop-Host 0,04–0,07 ms (mit Gast-Figur, Prüfschicht, Momentaufnahmen), Gast 0,02–0,08 ms. Bild: alle drei auf 60 Hz (16,0–16,6 ms je Bild einschließlich Warten auf die Bildwiederholung); der verdeckte Host zeichnete währenddessen kaum (0,3 ms). Die reine Grafikzeit ohne das Warten ist hier nicht getrennt gemessen. |

### 5.4 Sanitizer

```
clang++ -std=c++20 -O1 -g -fsanitize=address,undefined -Isrc tests/coop_proto_fuzz.cpp src/net/coop_proto.cpp src/net/protocol.cpp
clang++ ... tests/net_transport_test.cpp src/netdll/transport.cpp src/netdll/secure.cpp third_party/enet/*.c (ebenfalls mit Sanitizern) -lws2_32 -lwinmm -lbcrypt
```

| Lauf | Ergebnis |
|---|---|
| `coop_proto_fuzz` mit 2 000 000 Runden (12 000 000 Fälle) | **OK**, 82 300 angenommen, alle kanonisch, keine Meldung |
| `net_transport_test` mit ENet, Transport und Kryptografie unter Sanitizern | **OK: 52 Prüfungen, 0 Fehler**, keine Meldung |

### 5.5 Einzelspieler unverändert

| Test | Ergebnis |
|---|---|
| `gameplay_test` | OK: 446 Prüfungen, 0 Fehler (V13.2: mit den neuen Wesen) |
| `audio_test` | OK: 106 Prüfungen, 0 Fehler |
| `worldgen_test` (V4-Referenz) | OK: 29 658 Prüfungen, 0 Fehler |
| `supply_balance_test` | OK (0 Fehler); die Vorräte für mehrere Spieler ändern bei Faktor 1,0 nichts |
| `net_protocol_test` (v1) | OK: 72 Prüfungen, 0 Fehler |
| `tools/check_no_network.py` | SAUBER: `LiminalGame.exe` ohne Netzwerk-DLL und ohne statischen Import von `liminal_net.dll`; die DLL importiert nur ws2_32 (UDP), keine HTTP-Bibliothek, exportiert genau zehn Funktionen |

## 6. Nicht geprüft

- Spiel über das echte Internet mit NAT und Portfreigabe: nur über den Netzstörer nachgestellt.
- Mehr als vier Spieler: nicht vorgesehen (abgewiesen).
- Eine unabhängige Durchsicht durch Dritte: nicht möglich. Durchgesehen habe nur ich, entlang des Bedrohungsmodells (`MULTIPLAYER_BEDROHUNGSMODELL.md`, Abschnitt 8).

## 7. V13.3: neue Verbindungsschicht (Lobby, WebRTC, Relay)

Stand 27.10.2026. Alle Tests von Abschnitt 5 laufen jetzt gegen die **neue** Verbindungsschicht:
- Lobby mit Beitrittscode;
- libdatachannel (ICE/STUN/TURN, DTLS, SCTP);
- Rahmung der DLL;
- Spielprotokoll v3.

ENet und die eigene Kryptografie gibt es nicht mehr; die Testvektoren für HMAC/HKDF/AES aus 5.1 entfallen mit ihnen.

**Aufbau im Test** (ein Rechner, Windows 11):
- Getrennte Loopback-Adressen wie getrennte Rechner: Host 127.0.0.3, Gäste 127.0.0.2, Lobby und TURN 127.0.0.1. So greifen die Berechtigungen des TURN-Servers wie zwischen echten Rechnern.
- Der **Netzstörer** (UDP-Proxy im Test) stellt je Richtung Laufzeit, Schwankung, Verlust, Doppelte, Umordnung und Bandbreite ein.

```
cmake --build build/Release
cd build/Release/bin
../tests/net_transport_test.exe [--only codes,proto,join,wrong,paths,direct,tls,host,lobby]
../tests/coop_test.exe data [--quick] [--only fuzz,bots,netem,relay,direct,pickup,save,full,attacks,death,journey,outage,hostabort,nopath,reconnect,netfuzz]
../tests/coop_test.exe data --long 3600
../tests/lobby_proto_fuzz.exe [runden]
python ../../../tools/check_no_network.py
```

### 7.1 Transport (`net_transport_test`): **OK, 90 Prüfungen, 0 Fehler** (84–99 s; mehrfach)

| Bereich | Was | Ergebnis |
|---|---|---|
| Codes, Zertifikat | kurzer/langer Code, Kleinbuchstaben, O→0, I/L→1, falsche Längen und Zeichen; Host-Zertifikat: Fingerabdruck aus PEM = beim Erzeugen; Prüfteil aus bekannten Bits; Zufall | 17/17 |
| Lobby-Nachrichten | hin und zurück je Art; 22 Gegentests (falsche Typen, Längen, Zeichen, Steuerzeichen, „to“ vom Gast, Host-Befehle vom Gast, Schachtelung, über 8 KiB, unbekannte Fehlercodes, TURN-Zugang mit Leerzeichen); **Fuzzing 300 000 Fälle**: 7 814 angenommen, jede neu gebaut und wieder gelesen | 10/10 |
| Beitritt über die Lobby | Code nach dem Start; Gast mit kurzem Code verbunden nach **108–187 ms**; Name beim Host; 1200 Byte ja, 1201 nein; 200 zuverlässige Nachrichten vollständig und geordnet; unzuverlässiger Kanal; Weg „direkt“; zweiter Gast mit langem Code; Trennung gemeldet; nach dem Ende des Hosts „Code unbekannt“ | 14/14 |
| Falsche Codes, Sperre, voll, Bann | Format falsch → BADCODE; unbekannter Raum → NOROOM; falscher Prüfteil → CERT (nichts gesendet); falsches Geheimteil → BADKEY (4×); danach richtig → verbunden; voll → FULL; 5. Fehlversuch → Adresse gesperrt, auch mit richtigem Code; Bann durch den Host → Gast erfährt es, erneuter Versuch schon in der Lobby abgewiesen | 12/12 |
| Wege | **nur Relay** (beide Seiten verbergen die IP) → beide Seiten Weg „Relay“, verbunden nach 312–531 ms; **NAT**: die Lobby verwirft alle direkten Kandidaten → über das Relay verbunden (356–380 ms); **kein Weg** (kein Relay, alle Kandidaten verworfen) → Grund „kein Verbindungsweg“ nach 20 s, kein halber Gast beim Host | 6/6 |
| Direkt ohne Lobby | eingebaute Lobby auf einem Port; kurzer Code direkt abgelehnt (Prüfteil Pflicht); langer Code verbunden, Weg direkt, Nachricht kommt an; zweiter Raum an der eingebauten Lobby abgewiesen | 7/7 |
| Lobby mit TLS (wss) | festgelegtes Zertifikat als PEM und als Fingerabdruck: Raum offen; **fremdes Zertifikat mit gleichem Namen: abgewiesen (CERT)**; selbst signiert gegen die Windows-Stammzertifikate: abgewiesen (CERT); Gast über wss verbunden | 5/5 |
| Angriffe auf den Host | Gast mit gültiger Raumnummer, eigener WebRTC-Stack: Nutzdaten vor dem Hallo, Text statt Binär, 3000 Byte → je getrennt; falsche Version → Absage VERSION; kein Hallo → Zeitlimit; Flut nach gültigem Hallo (3000 in 0,2 s) → höchstens Vorrat + Rate angenommen, anhaltende Flut → getrennt; der ehrliche Gast bleibt verbunden | 10/10 |
| Angriffe auf die Lobby | Unsinn, Binär, 20 KB, 3000-fach geschachtelt, Host-Befehl vom Gast → getrennt; stumm → nach 10 s getrennt; 12 Verbindungen bei Grenze 6 → höchstens 6 offen; 14 Raumnummern geraten → 10× unbekannt, dann gesperrt; danach 3 000 veränderte Nachrichten über 60 Verbindungen gegen eine Lobby ohne Bremse → echter Beitritt geht | 9/9 |

### 7.2 Koop mit Bots (`coop_test`, vollständig): **OK, 201 Prüfungen, 0 Fehler** (714 s; Lauf vom 27.10.2026 vor der SCTP-Änderung aus 7.8 – die Abschlussläufe danach: 7.10)

Bots, Momentaufnahmen, Vorhersage und Abgleich wie in 5.2, jetzt über die neuen Wege. „Ping“ ist die von SCTP gemessene Laufzeit, „Verlust“ der Anteil unbeantworteter Pings der letzten 20 s.

| Weg und Netz | Ergebnis |
|---|---|
| Lobby, direkt, Host + 1 | Weg direkt; Vorhersage 1,8 cm im Mittel, über 10 cm in 4 von 529; Ping 11 ms |
| Lobby, direkt, Host + 3 (vier Spieler) | Weg direkt; 1,7 cm, 14 von 1 403 über 10 cm; 0 Sprünge |
| Netzstörer **LAN** (20 ms, ±5, 1 % Verlust), direkter Weg durch den Proxy | Weg „durch NAT“ (Proxy = fremde Adresse); 2,1 cm; Ping 30 ms; **4,7 kB/s abwärts, 5,3 kB/s aufwärts je Gast** (mit DTLS/SCTP) |
| Netzstörer **Internet** (80 ms ±30, 3 % Verlust, 1 % doppelt, 2 % umgeordnet, 256 kbit/s) | 3,5 cm; Ping 96 ms; 3 Sprünge; 5,9/6,9 kB/s je Gast |
| Netzstörer **schlecht** (250 ms ±80, 10 % Verlust, 5 % umgeordnet, 128 kbit/s), 2 Gäste | 6,8 cm; Ping 345 ms; größte Abweichung 8,2 m; Ort nach dem Anhalten im Rahmen. **Vorher (erster Lauf, ohne Lebensdauer):** Laufzeit 6,6 s, 19 m Abweichung → Fehler; behoben (E-033) |
| **Nur Relay** („NAT auf beiden Seiten“: die Lobby verwirft alle direkten Kandidaten; der Verkehr zum TURN-Server läuft durch den Netzstörer, je Strecke 80 ms ±30, 3 % Verlust, 1 % doppelt, 2 % umgeordnet) | **Weg „Relay“ für alle Gäste**; 5,7 cm; Ping 165 ms; Verlust 19 % (zwei gestörte Strecken je Richtung); 3 Sprünge; Verkehr am Störer 33 kB/s je Gast (beide Strecken, TURN-Kopf) |
| **Direkt ohne Lobby** (eingebaute Lobby, langer Code), 2 Gäste | Weg direkt; 1,7 cm |
| Ablegen, Aufnehmen, Wettlauf, Puppen; Koop-Stand; voll | wie 5.2, bestanden |
| Angriffe (Gast mit gültigem Code) | 8 ungültige → getrennt; Speedhack (6-fache Zeit): 20 von 31 Eingaben verworfen, höchstens Laufweite; erfundene/ferne Gegenstände abgelehnt; Flut von Absichten gedrosselt; andere Spieldaten abgewiesen; ehrlicher Gast ungestört |
| Tod, Zuschauen, Wiedereinstieg | bestanden (Prüfung jetzt in Spielzeit) |
| Reise | Host + 2 Gäste durch alle zehn Ebenen (12 Wechsel), gleicher Seed, beisammen, verbunden |
| **Netzausfall** | Netzstörer verwirft 13 s lang alles → Gast merkt es nach **10,0 s**; Host führt ihn als getrennt; mit demselben Code zurück: gleiche Spielernummer, Inventar zurück |
| **Host beendet** | Gast getrennt nach **0,07 s** („Host hat das Spiel beendet“) |
| **Host fällt aus** (alles verworfen, wie ein abgestürzter Prozess) | Gast getrennt nach **10,0 s** („Zeitlimit“) |
| **Kein Weg** (UDP gesperrt, kein Relay) | klare Meldung nach 20 s: „kein Verbindungsweg (Firewall/NAT) – auch das Relay war nicht erreichbar“ |
| Wiederverbinden, Entfernen, Sperren | gleicher Code → gleiche Spielernummer, Inventar zurück; entfernt; gesperrt → erneuter Versuch abgewiesen („gesperrt“) |
| **Fuzzing über den Netzweg** (300 s) | 15 130 veränderte und zufällige Nachrichten über 35 Verbindungen gegen den echten Host. Host: 311 abgelehnt, 2 900 gedrosselt, 11 769 von 11 993 Eingaben verworfen (Zeitbudget), 1 012 von 1 443 Absichten abgelehnt, 34 Trennungen; der ehrliche Gast spielt ungestört |

Gefunden und behoben während dieser Tests:
- Absagen kamen beim Gast nicht an (Verbindung zu früh geschlossen) → 1 s Nachlauf.
- Drei Gäste hinter einer Adresse wurden als „voll“ abgewiesen (Grenze 2 halbe Beitritte je Adresse) → 3, eigener Grund „Host nimmt gerade andere Gäste auf“.
- Wer wegen ungültiger Nachrichten getrennt wurde, belegte 120 s einen Platz → frei.
- Stau veralteter Momentaufnahmen bei knapper Bandbreite → Lebensdauer 150 ms.

### 7.3 Zwei echte Spielprozesse über `liminal_lobby.exe`

`coop_real.ps1`, `coop_menu.ps1`, `coop_title.ps1` (Arbeitsordner):
- `liminal_lobby.exe` läuft als eigenes Programm (127.0.0.1, TCP 27615, STUN/TURN UDP 34780).
- Zwei `LiminalGame.exe` mit eigenen Nutzerordnern und `koop.json` (`"lobby": "ws://127.0.0.1:27615"`).
- Der Host öffnet per Skript `coophost … lobby`, der Gast tritt per `coopjoin … lobby` mit dem Code aus einer Datei bei.

| Prüfung | Ergebnis |
|---|---|
| Code von der Lobby, Beitritt per Code | bestanden: Lobby „Raum geöffnet“, „Gast wartet“; Gast „willkommen als Spieler 1“ |
| Beide Figuren, Namen, Zeile „KOOP“ (ohne Beta) | bestanden (`docs/screens/koop/koop_lobby_gast.jpg`, `koop_lobby_host.jpg`) |
| Pausenmenü „Mitspieler“: Code, lange Form, Ping, Weg | bestanden (`koop_mitspieler_host.jpg`: „Gast – unterwegs, 9 ms, direkt“; `koop_mitspieler_gast.jpg`) |
| Koop-Menü: Beitreten mit Code, Lobby-Hinweis; Hosten direkt | bestanden (`koop_menue_beitreten.jpg`, `koop_menue_hosten_direkt.jpg`); dabei zu lange Hinweiszeilen gefunden und umbrochen |
| Host beendet → Gast | „verlassen (Host hat das Spiel beendet)“; beide Prozesse Exit 0 |

### 7.4 Sanitizer und Fuzzing der eigenen Parser

```
clang++ -std=c++20 -O1 -g -fsanitize=address,undefined ... tests/lobby_proto_fuzz.cpp src/netdll/lobby_proto.cpp src/netdll/joincode.cpp <Mbed TLS mit denselben Schaltern>
clang++ ... tests/coop_proto_fuzz.cpp src/net/coop_proto.cpp src/net/protocol.cpp
```

| Lauf | Ergebnis |
|---|---|
| `lobby_proto_fuzz` 1 000 000 Runden (Lobby-Nachrichten beider Richtungen, Codes, Prüfteil, Zeichenprüfungen) unter ASan/UBSan | **OK**, 9 832 angenommen, alle neu gebaut und wieder gelesen, keine Meldung |
| `coop_proto_fuzz` 500 000 Runden (3 000 000 Fälle, Spielprotokoll mit `kRoles` = 40) unter ASan/UBSan | **OK**, 20 641 angenommen, alle kanonisch, keine Meldung |

Nicht unter Sanitizern: libdatachannel, libjuice, usrsctp, Mbed TLS als Ganzes (nur über die Netz-Tests mitbenutzt, ohne Sanitizer).

### 7.5 Abhängigkeiten: Versionen und Lücken prüfen

Vorgehen (wiederholbar, vor jeder Veröffentlichung):
1. **Neueste Releases** über die GitHub-API (`/repos/<projekt>/releases/latest`) mit den Ständen in `LICENSES.md`, Abschnitt 7.1, vergleichen.
2. **OSV:** `POST https://api.osv.dev/v1/query` mit `{"commit": "<sha>"}` je Commit.
3. **GitHub Security Advisories** der Projekte (`/repos/<projekt>/security-advisories`) und eine Websuche nach „<projekt> CVE“.
4. **Mbed TLS:** Prüfsumme des Release-Archivs gegen `mbedtls-<version>-sha256sum.txt` aus dem Release.

Ergebnis am 27.10.2026:
- Keine bekannten Lücken für die verwendeten Stände.
- libdatachannel, libjuice, Mbed TLS 3.6 und nlohmann/json sind auf dem neuesten Stand.
- usrsctp und plog stehen auf den von libdatachannel 0.24.6 festgelegten Commits.
- Einzelheiten: `MULTIPLAYER_BEDROHUNGSMODELL.md`, Abschnitt 9.3.

### 7.6 Echtwelt-Test (zwei Rechner in verschiedenen Netzen) – Anleitung

**Nicht durchgeführt:** Für diese Arbeit gab es nur einen Rechner und keinen öffentlichen Server. Was im Internet anders sein kann: echte NAT-Router (auch symmetrische und CGNAT), Mobilfunk, Firmen-Firewalls, Paketverlust und Laufzeiten über Kontinente, Windows-Firewall-Abfragen.

**Aufbau:**
- **Server S:** VPS oder Rechner mit öffentlicher IP. `liminal_lobby.exe --port 27515 --turn-port 3478 --relay-ports 50000-50199 --public-host <IP oder Name>`; Firewall: TCP 27515, UDP 3478, UDP 50000–50199.
- **Rechner A** (Host) im Heimnetz hinter einem gewöhnlichen Router, **ohne** Portfreigabe.
- **Rechner B** (Gast) in einem **anderen** Netz, z. B. über einen Mobilfunk-Hotspot (oft CGNAT).
- Auf A und B: `koop.json` mit `"lobby": "ws://<S>:27515"` (oder `wss://` mit Zertifikat, `MULTIPLAYER_SETUP.md`, Abschnitt 3.3).

**Schritte und Erfolgskriterien:**

| # | Schritt | Erfolg, wenn |
|---|---|---|
| 1 | A: Koop → Hosten → Über die Lobby | im Pausenmenü steht nach höchstens 3 s ein Code |
| 2 | B: Koop → Beitreten, Code eingeben | B spielt nach höchstens 10 s; Toast „beigetreten … (direkt / direkt durch NAT / über das Relay)“ |
| 3 | A: Pausenmenü → Mitspieler | B steht mit Ping und Weg da. Den Weg notieren: Hinter CGNAT ist „über Relay“ zu erwarten, sonst oft „direkt durch NAT“ |
| 4 | 10 min gemeinsam spielen, Ebene wechseln | keine Trennung; keine Abweichung zwischen den Sichten (Gegenstände, Wesen); Ping-Anzeige plausibel |
| 5 | B: „Eigene IP verbergen“ an, neu beitreten | Weg „über Relay“ |
| 6 | B: WLAN/Hotspot 15 s trennen, dann wieder an; mit demselben Code beitreten | B kommt mit derselben Spielernummer und seinem Inventar zurück |
| 7 | A: Spiel beenden | B meldet sofort „Host hat die Sitzung beendet“ |
| 8 | B: falschen Code eingeben (ein Zeichen geändert) | klare Meldung („Code unbekannt“ oder „falscher Code“), kein Absturz |
| 9 | S: `liminal_lobby.exe` während eines Spiels beenden | laufendes Spiel über direkten Weg läuft weiter; A zeigt „Lobby-Verbindung verloren“; nach Neustart von S einen neuen Code |
| 10 | Wiederholen mit B im Firmen- oder Schulnetz | entweder verbunden (Weg notieren) oder klare Meldung „kein Verbindungsweg“ |

**Zu protokollieren:** Weg, Ping und Verlust aus dem Mitspieler-Menü, die Zeit bis zum Beitritt, die Ausgabe von `liminal_lobby.exe --verbose` und `%LOCALAPPDATA%\LIMINAL\logs` beider Rechner.

### 7.7 Einzelspieler unverändert (V13.3)

| Test | Ergebnis |
|---|---|
| `gameplay_test` | OK: 446 Prüfungen, 0 Fehler |
| `audio_test` | OK: 106 Prüfungen, 0 Fehler |
| `worldgen_test` (V4-Referenz) | OK: 29 658 Prüfungen, 0 Fehler |
| `supply_balance_test` | OK (0 Fehler) |
| `net_protocol_test` (v1) | OK: 72 Prüfungen, 0 Fehler |
| `tools/check_no_network.py` | **SAUBER**: `LiminalGame.exe` ohne Netzwerk-DLL und ohne Netzwerk-Zeichenkette, nennt weder `liminal_net` statisch noch `liminal_lobby`. `liminal_net.dll` importiert ws2_32, iphlpapi, winhttp, bcrypt, crypt32, keine Download-Bibliothek, exportiert genau elf `ln_`-Funktionen |
