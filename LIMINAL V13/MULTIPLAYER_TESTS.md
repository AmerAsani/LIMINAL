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

## 5. Nicht geprüft (weil nicht vorhanden)

Echter Netzweg, Verschlüsselung, Beitritt, Abbruch, Wiederverbinden, Wettläufe zwischen Prozessen – der Modus existiert in V13 nicht (`MULTIPLAYER_ENTSCHEIDUNG.md`). Diese Tests sind Bedingung für ein späteres Einschalten.
