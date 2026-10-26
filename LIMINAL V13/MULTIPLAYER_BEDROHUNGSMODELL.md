# LIMINAL V13 – Bedrohungsmodell Mehrspieler

Stand 26.10.2026. Dieses Dokument entstand **vor** jeder Netzwerkzeile, wie im Auftrag verlangt. Es baut auf den früheren Analysen auf (`MULTIPLAYER_ASSESSMENT.md` aus V11, `MULTIPLAYER.md` aus V12) und macht daraus verbindliche Regeln. Es beschreibt, was ein Mehrspielermodus in LIMINAL gefährden würde und welche Regeln jede Umsetzung einhalten muss. Die Entscheidung, ob der Modus in V13 eingeschaltet wird, steht in `MULTIPLAYER_ENTSCHEIDUNG.md`, die Prüfungen in `MULTIPLAYER_TESTS.md`.

Kein Teil dieses Dokuments behauptet, ein Mehrspielermodus sei „vollkommen sicher“. Ein Netzwerkmodus vergrößert die Angriffsfläche immer; das Ziel ist, das Risiko klein, bekannt und überprüfbar zu halten.

## 1. Ausgangslage

- LIMINAL V12/V13 ist ein reines Einzelspielerspiel. Es gibt **keinen** Netzwerkcode: keine Sockets, keine Winsock-Bibliothek (`ws2_32.dll`), keinen HTTP-Client, keine offenen Ports.
- Die zehn Ebenen sind feste Karten (`data/maps/*.json`); Funde verteilt ein Spawnplan nach Seed. Wesen, Gegenstände, Türen, Aufzüge, Übergänge und Spielstände leben in einem einzigen Prozess und kennen genau einen Spieler.
- Angenommenes Modell für einen späteren Modus: **Koop für 2–4 Spieler** mit einem autoritativen Server – entweder ein eigenes Serverprogramm (Empfehlung aus V11) oder ein Spieler als Gastgeber (Host, Listen-Server). Die anderen verbinden sich direkt (LAN oder Internet mit Portfreigabe). Keine Konten. Die Regeln unten gelten für beide Varianten; „Host“ steht im Folgenden für den Server.

## 2. Schützenswerte Güter

| Gut | Warum |
|---|---|
| Rechner der Spieler | Ein Fehler beim Lesen von Netzwerkdaten (Pufferüberlauf, Ganzzahlüberlauf, Pfad aus dem Netz) kann zur Ausführung fremden Codes führen – der schwerste anzunehmende Schaden. |
| Dateien der Spieler | Spielstände, Einstellungen, Protokolle liegen unter `%LOCALAPPDATA%`. Kein Netzwerkpartner darf Dateien lesen, schreiben oder Pfade bestimmen. |
| Verfügbarkeit des Spiels | Absturz oder Hänger beim Host beendet die Sitzung aller; ein Absturz darf nie den Einzelspielerstand beschädigen. |
| Privatsphäre | IP-Adressen der Teilnehmer, Rechnername, Pfade in Protokollen. Nichts davon darf an andere Spieler gehen, außer der Adresse, die für die Verbindung nötig ist. |
| Spielzustand | Im Koop zählt Fairness weniger als im Wettkampf; ein Gast darf aber nicht die Welt des Hosts verändern (Gegenstände erzeugen, Wesen löschen, Türen öffnen, die er nicht erreicht). |
| Einzelspieler | Der Mehrspielermodus darf Stabilität, Leistung und Qualität des Einzelspielers nicht verschlechtern (Auftrag). |

## 3. Vertrauensgrenzen

1. **Netzwerk → Prozess des Hosts**: alle Pakete der Gäste sind feindlich, bis sie geprüft sind.
2. **Netzwerk → Prozess eines Gastes**: auch der Host kann bösartig sein (der Gast verbindet sich zu einem fremden Rechner).
3. **Spielstand und Daten auf der Platte**: bleiben lokal; nie aus dem Netz befüllt.
4. **Weg dazwischen** (Internet, WLAN): ein Angreifer auf dem Weg kann Pakete lesen, verändern, einschleusen und wiederholen.

## 4. Angreifer

| Angreifer | Fähigkeiten |
|---|---|
| Bösartiger Gast | Eigener, veränderter Client; sendet beliebige Bytes in beliebiger Menge und Reihenfolge; verbindet sich mehrfach. |
| Bösartiger Host | Wie oben, aber in Richtung Gast; kennt die IP-Adressen aller Gäste. |
| Angreifer auf dem Weg | Liest und verändert unverschlüsselten Verkehr (offenes WLAN, Router). |
| Scanner im Internet | Findet einen freigegebenen Port und sendet Zufallsdaten, Fluten oder bekannte Angriffe. |

## 5. Bedrohungen (STRIDE) und verlangte Gegenmaßnahmen

| # | Bedrohung | Beispiel | Gegenmaßnahme (Pflicht für jede Umsetzung) |
|---|---|---|---|
| S1 | Fremder gibt sich als Gast aus | Verbindung ohne Einladung, Übernahme einer Sitzung | Beitritt nur mit zufälligem Sitzungscode (128 Bit, vom Host angezeigt); Handshake mit Protokollversion; Sitzungs-ID je Verbindung. |
| S2 | Gefälschte Absenderadresse (UDP) | Pakete mit fremder IP | Antwort vor abgeschlossenem Handshake nie größer als die Anfrage (keine Verstärkung); Cookie im Handshake. |
| T1 | Gast sendet Zustände statt Eingaben | „Ich stehe bei der Sicherung“, „Gegenstand X gehört mir“ | **Der Client sendet nur Eingaben, nie Zustände** (Auftrag): Bewegungsachsen, Blickänderung, Tastenbits. Der Server rechnet alles selbst. |
| T2 | Werte außerhalb des Bereichs | Achse = 10⁹, NaN, Blickdrehung 1000 rad je Paket | Jeder Wert mit festem Bereich; NaN/Unendlich verwerfen; Blickänderung je Paket begrenzt. |
| T3 | Wiederholte oder vertauschte Pakete | Alte Eingaben erneut senden | Fortlaufende Nummern je Verbindung; ältere als die letzte angenommene verwerfen; Fenster begrenzt. |
| T4 | Veränderung auf dem Weg | Eingaben eines anderen Gastes fälschen | Authentisierte Verschlüsselung des Verkehrs nach dem Handshake (oder: Modus nur im LAN, klar angezeigt). |
| I1 | Speicherinhalt nach außen | Längenfeld größer als der Inhalt → Server sendet fremden Speicher zurück | Längen nie aus dem Paket übernehmen, um Antworten zu bilden; feste Nachrichtengrößen. |
| I2 | IP-Adressen anderer Gäste | Host verteilt Adressliste | Gäste kennen nur den Host; Spielernamen ja, Adressen nein. |
| I3 | Pfade und Rechnernamen | Protokollzeilen, Fehlermeldungen ans Netz | Nichts aus dem Dateisystem oder Protokoll ins Netz. |
| D1 | Paketflut | Tausende Pakete je Sekunde | Ratenbegrenzung je Verbindung (Eimer mit Marken); Überschreitung → Pakete verwerfen, dauerhaft → trennen. |
| D2 | Speicher erschöpfen | Länge 4 GB angekündigt, viele halbe Verbindungen | Feste Höchstgröße je Paket (1200 Byte), keine Zuteilung nach Längenfeld, höchstens 3 Gäste und 8 halbe Handshakes. |
| D3 | Rechenzeit erschöpfen | Teure Anfragen (Pfadsuche, Weltgenerierung auf Wunsch) | Gäste können keine Arbeit anstoßen, nur Eingaben senden; Arbeit pro Bild begrenzt. |
| D4 | Verbindung hängt | Gast antwortet nie, halbe Pakete | Zeitlimits (Handshake 5 s, Stille 10 s) → trennen; Host läuft ohne den Gast weiter. |
| D5 | Wiederverbinden in Schleife | Gast verbindet sich 100-mal je Sekunde | Sperre je Adresse nach wiederholtem Abbruch; begrenzte Rate für neue Handshakes. |
| E1 | Codeausführung durch Parserfehler | Überlauf beim Lesen eines Strings | Nur feste, einfache Felder (Ganzzahlen, Gleitkommazahlen mit Bereich, Bits); Namen höchstens 16 Zeichen aus erlaubtem Zeichensatz; keine Zeiger, Größen oder Pfade aus dem Netz; Fuzz-Tests des Parsers. |
| E2 | Daten als Code | Skripte, Dateien, Mods über das Netz | Nichts davon: keine Dateiübertragung, keine Skriptbefehle (die Testskript-Schnittstelle `--script` bleibt rein lokal). |
| E3 | Rollen vertauscht | Gast sendet Host-Befehle (Level wechseln, Speichern) | Rollen fest beim Handshake; Nachrichtentypen je Rolle als Erlaubnisliste. |

## 6. Regeln für die Architektur (bindend)

1. **Autoritativer Server.** Nur der Host führt Spielregeln, Physik, Wesen, Gegenstände und Speichern aus. Gäste zeigen an und senden Eingaben.
2. **Eingaben, keine Zustände.** Ein Eingabepaket enthält: Folgenummer, Bewegungsachsen (−1…1), Blickänderung (begrenzt), Tastenbits (fester Satz). Alles andere wird verworfen.
3. **Prüfung jeder Nachricht**: Typ (Erlaubnisliste je Rolle und Verbindungszustand), Länge (exakt passend zum Typ), Bereich jedes Felds, Rate (je Verbindung), Folge (Nummer).
4. **Keine neuen Schreibwege auf die Platte.** Gäste speichern nichts; der Host speichert nur seinen eigenen Stand, getrennt vom Einzelspieler.
5. **Fehler beenden die Verbindung, nie das Spiel.** Ein ungültiges Paket wird gezählt und verworfen; zu viele → Trennung. Kein `assert`, kein Absturz durch Netzdaten.
6. **Einzelspieler unberührt.** Ohne ausdrückliches Starten des Mehrspielermodus öffnet das Spiel keinen Port und lädt keine Netzwerkbibliothek.
7. **Abhängigkeiten.** Für Verschlüsselung nur geprüfte Bibliotheken (z. B. libsodium) oder die Windows-Kryptografie (CNG); keine eigene Kryptografie.

## 7. Restrisiken (auch bei sauberer Umsetzung)

- Parserfehler lassen sich durch Tests und Fuzzing verringern, nicht ausschließen.
- Ohne Verschlüsselung ist Internetspiel auf offenen Netzen angreifbar; mit Verschlüsselung bleibt der Host selbst ein möglicher Angreifer.
- Portfreigaben machen den Host im Internet sichtbar.
- Die bisherigen Systeme (Wesen-KI, Gegenstände, Türen, Aufzüge, Spielstände) sind für einen Spieler gebaut; jede Anpassung für mehrere Spieler kann Fehler in den Einzelspieler tragen.
