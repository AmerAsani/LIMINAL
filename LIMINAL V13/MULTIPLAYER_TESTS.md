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
| Fuzzing Protokoll | 240 000 Fälle (Zufall, veränderte Vorlagen; beide Rollen, beide Zustände): kein Absturz, jede angenommene Nachricht kanonisch; größte Momentaufnahme 1 059 Byte (24 Wesen, 3 Mitspieler); Steuerzeichen, Richtungsumkehr (U+202E), überlange UTF-8-Formen und Pfadzeichen werden nicht kodiert; falsche Rolle, falscher Zustand, 1201 Byte | bestanden |
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
| **Summe (alle Teile)** | | **OK: 164 Prüfungen, 0 Fehler** (dazu Koop-Stand 5/5) |

### 5.3 Zwei echte Spielprozesse

`coop_shots.ps1` (Arbeitsordner) startet zwei `LiminalGame.exe`: Der Host öffnet per Skriptbefehl `coophost`, der Gast tritt per `coopjoin` mit dem Schlüssel aus einer Datei bei. Beide nehmen Bilder aus der Schulteransicht auf (`docs/screens/koop/`).

| Prüfung | Ergebnis |
|---|---|
| Beitritt, Ebene, beide Figuren in beiden Sichten | bestanden (`koop_hell_host_gast.jpg`) |
| Lampen beider Spieler im dunklen Zimmer (eigenes Spotlicht je Mitspieler) | bestanden (`koop_dunkel_lampen_host_gast.jpg`) |
| Namen über den Mitspielern, Statuszeile und Spielerliste (Beta) | bestanden |
| Beenden beider Prozesse | Exit 0 (vorher Absturz, B-009) |

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
| `gameplay_test` | OK: 376 Prüfungen, 0 Fehler |
| `audio_test` | OK: 106 Prüfungen, 0 Fehler |
| `worldgen_test` (V4-Referenz) | OK: 29 658 Prüfungen, 0 Fehler |
| `supply_balance_test` | OK (0 Fehler); die Vorräte für mehrere Spieler ändern bei Faktor 1,0 nichts |
| `net_protocol_test` (v1) | OK: 72 Prüfungen, 0 Fehler |
| `tools/check_no_network.py` | SAUBER: `LiminalGame.exe` ohne Netzwerk-DLL und ohne statischen Import von `liminal_net.dll`; die DLL importiert nur ws2_32 (UDP), keine HTTP-Bibliothek, exportiert genau zehn Funktionen |

## 6. Nicht geprüft

- Spiel über das echte Internet mit NAT und Portfreigabe: nur über den Netzstörer nachgestellt.
- Mehr als vier Spieler: nicht vorgesehen (abgewiesen).
- Eine unabhängige Durchsicht durch Dritte: nicht möglich. Durchgesehen habe nur ich, entlang des Bedrohungsmodells (`MULTIPLAYER_BEDROHUNGSMODELL.md`, Abschnitt 8).
