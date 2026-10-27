# LIMINAL V13 – Bedrohungsmodell Mehrspieler

Stand 26.10.2026. Dieses Dokument entstand **vor** jeder Netzwerkzeile, wie im Auftrag verlangt. Es baut auf den früheren Analysen auf (`MULTIPLAYER_ASSESSMENT.md` aus V11, `MULTIPLAYER.md` aus V12) und macht daraus verbindliche Regeln. Es beschreibt, was ein Mehrspielermodus in LIMINAL gefährden würde und welche Regeln jede Umsetzung einhalten muss. Die Entscheidung, ob der Modus in V13 eingeschaltet wird, steht in `MULTIPLAYER_ENTSCHEIDUNG.md`, die Prüfungen in `MULTIPLAYER_TESTS.md`.

**V13.3 (27.10.2026):** neue Verbindungsschicht (Lobby mit Beitrittscode, WebRTC/DTLS, Relay); Umsetzung, neue Bedrohungen, Abhängigkeiten und Restrisiken in **Abschnitt 9**. Die Abschnitte 1–8 beschreiben den Stand bis V13.2.

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

## 8. Stand der Umsetzung (V13.2)

Seit V13.2 gibt es den Koop-Modus als Beta (`MULTIPLAYER_ARCHITEKTUR.md`, `MULTIPLAYER_ENTSCHEIDUNG.md`). Die Tabelle zeigt je Bedrohung, was umgesetzt ist und wo es geprüft wird (`MULTIPLAYER_TESTS.md` Abschnitt 5).

| # | Umsetzung | Prüfung |
|---|---|---|
| S1 | Beitrittsschlüssel 128 Bit (BCryptGenRandom); der Gast beweist ihn per HMAC über beide Zufallswerte, ohne ihn zu senden; der Host bestätigt verschlüsselt (gegenseitig) | Transport: falscher Schlüssel, Sperre nach 5 Fehlversuchen |
| S2 | Antwort des Hosts vor dem Beweis (CHALLENGE 25 Byte, REFUSE 2 Byte) kleiner als die Anfrage (HELLO 40 Byte); Keks im Beweis | Transport: Handshake von Hand |
| T1 | Der Gast sendet nur Eingaben (Achsen, Blick, Tasten, Zeit) und Absichten (aufnehmen, benutzen, ablegen, Lampe, Platz, zusehen); jede Absicht prüft der Host an seinem Zustand (Reichweite +0,6 m, Höhe, freie Sicht, Inventar, lebt) | Angriffe: erfundene und ferne Gegenstände, Wettlauf |
| T2 | Alle Felder mit Bereich; Zeit je Eingabe 1–100 ms und Zeitbudget je Gast (Uhr des Hosts + 0,5 s). **Abweichung:** Der Blick kommt als Winkel, nicht als begrenzte Änderung; er bestimmt nur Laufrichtung und Lampe, keine Treffer | Fuzzing, Speedhack-Test |
| T3 | Zähler je Kanal im Transport (Wiederholung und Alter verworfen); Folgenummern der Eingaben und Absichten im Spiel | Transport: Wiederholung, alter Zähler |
| T4 | AES-256-GCM je Richtung, Schlüssel aus HKDF; Kanal in den Zusatzdaten | Testvektoren, veränderte Nachricht, falscher Kanal |
| I1 | Längen nur innerhalb der geprüften Nachricht; Texte mit eigener Länge, höchstens 96 Byte, nur darstellbares UTF-8 | Fuzzing (alle angenommenen Nachrichten kanonisch) |
| I2 | Gäste erfahren keine Adressen anderer Gäste, nur Namen | Protokoll: kein Feld dafür |
| I3 | Keine Pfade, Rechnernamen oder Protokollzeilen im Netz | Durchsicht der Nachrichten |
| D1 | Rate je Gast im Transport (120/s, Vorrat 60) und je Nachrichtentyp im Spiel (Eingaben 75/s, Absichten 10/s, Ping 2/s) | Transport-Flut, Flut von Absichten |
| D2 | Höchstens 1200 Byte; höchstens 3 Gäste, 8 halbe Handshakes, 3 je Adresse | halboffene Flut |
| D3 | Gäste stoßen keine teure Arbeit an; Laden um ihre Figuren nur im üblichen Radius | Bots, Langlauf |
| D4 | Handshake 5 s, Stille 10 s | Transport, Langlauf |
| D5 | Sperre nach 5 falschen Beweisen (60 s); Zeitlimits im Handshake zählen als Fehlversuch; Kick mit Bann für die Sitzung | Transport, Bann-Test |
| E1 | Feste Felder, Texte mit Zeichensatzprüfung, Gegenstandsarten nur `[a-z0-9_]` und nur bekannte | Fuzzing unter ASan/UBSan (12 Mio. Fälle) |
| E2 | Keine Dateien, Skripte oder Befehle über das Netz | – |
| E3 | Erlaubnisliste je Rolle und Zustand | Fuzzing, Angriffe |

**Durchsicht:** Eine unabhängige Durchsicht durch Dritte war nicht möglich. Ich habe den Code selbst entlang dieser Tabelle durchgesehen. Dabei fand ich B-006 (getrennter Gast behielt seinen Platz in der Liste) und die fehlende Bestätigung im Handshake (B-005); beide sind behoben.

**Restrisiken der Beta (zusätzlich zu Abschnitt 7):**
- Der Spielername steht im HELLO unverschlüsselt (vor dem Schlüsseltausch). Wer mitliest, sieht ihn, sonst nichts.
- ENet selbst (Verbindungsaufbau, Bestätigungen) ist unverschlüsselt. Ein Angreifer mit gefälschten Absendern kann die freien ENet-Plätze des Hosts kurzzeitig belegen und so Beitritte verzögern (Gegenmittel: Zeitlimit, 3 je Adresse, Port nicht veröffentlichen).
- Kein Vorwärtsgeheimnis: Wer den Beitrittsschlüssel später erfährt und den Verkehr aufgezeichnet hat, kann ihn lesen.
- Der Host ist vertrauenswürdig gesetzt. Er kann Werte und Inventare der Gäste nach Belieben setzen und Meldungen schicken (geprüft auf Länge und Zeichen, nicht auf Inhalt).
- Fuzzing über den echten Netzweg lief Minuten, nicht Stunden; die Prüfschicht selbst lief 12 Millionen Fälle unter Sanitizern.

## 9. V13.3: neue Verbindungsschicht (Lobby, WebRTC, Relay)

Stand 27.10.2026. Der Verbindungsaufbau ist ersetzt (`MULTIPLAYER_ARCHITEKTUR.md`, Abschnitte 4–8; Wahl: `MULTIPLAYER_RECHERCHE.md`, E-033). Die Regeln aus Abschnitt 6 gelten weiter; Regel 7 („keine eigene Kryptografie“) ist jetzt auch für den Handshake erfüllt. Die eigene Schicht aus V13.2 (HMAC-Beweis, AES-GCM über CNG) und ENet sind entfernt.

### 9.1 Neue Vertrauensgrenzen und Angreifer

| # | Grenze | Neu, weil |
|---|---|---|
| 5 | **Lobby** (`liminal_lobby.exe`, im Internet) ↔ Spiel | Die Lobby reicht Verbindungsbeschreibungen weiter; sie ist ein Dienst im Netz und kann angegriffen werden oder selbst bösartig sein |
| 6 | **TURN-Relay** ↔ Spiel | Das Relay leitet Pakete weiter und sieht Adressen |
| 7 | **Fremde Bibliotheken** (libdatachannel, libjuice, usrsctp, Mbed TLS) | Parser für STUN, DTLS, SCTP und WebSocket laufen auf Daten aus dem Netz |

| Angreifer (zusätzlich zu Abschnitt 4) | Fähigkeiten |
|---|---|
| Böswilliger Betreiber der Lobby (oder wer sie übernommen hat) | sieht IPs, Namen, Raumnummern, Beschreibungen; kann Beschreibungen tauschen (Mann in der Mitte), Räume verweigern |
| Angreifer zwischen Spiel und Lobby | liest und ändert `ws://`; bei `wss://` nur, wenn das Zertifikat nicht geprüft würde |
| Scanner gegen die Lobby | rät Raumnummern, flutet Verbindungen und Nachrichten |
| Missbrauch des Relays | nutzt TURN als offenes Relay für fremden Verkehr |

### 9.2 Bedrohungen und Umsetzung in V13.3

| # | Umsetzung V13.3 | Prüfung (`MULTIPLAYER_TESTS.md`, Abschnitt 7) |
|---|---|---|
| S1 Fremder als Gast | Code: Raumnummer (Lobby) + Geheimteil (30 Bit, prüft nur der Host im DTLS-Kanal, Vergleich in fester Zeit); 5 Fehlversuche je Adresse → 10 min abgewiesen; die Lobby sperrt nach 10 unbekannten Räumen 10 min und begrenzt Versuche auf 20/min je Adresse | falsches Geheimteil, Sperre nach 5, Code raten |
| S1' Fremder als **Host** (Mann in der Mitte) | DTLS prüft den Fingerabdruck aus der Beschreibung. Langer Code: der Gast prüft 60 Bit dieses Fingerabdrucks, **bevor** er das Geheimteil sendet. Lobby mit `wss://`: Zertifikat von Windows geprüft oder festgelegt | falscher Prüfteil → Absage ohne Senden; fremdes Lobby-Zertifikat, unbekannte Zertifizierungsstelle → abgewiesen |
| S2 Gefälschte Absender | ICE prüft jede Adresse mit Kurzzeit-Zugangsdaten (STUN-Integrität); DTLS-Cookies gegen Verstärkung (libdatachannel); die Lobby läuft über TCP | Bibliothek (nicht eigens geprüft) |
| T1 Zustände statt Eingaben | unverändert (Spielprotokoll) | Angriffe, Netz-Fuzzing |
| T2 Werte außerhalb | unverändert | Fuzzing |
| T3 Wiederholung | DTLS-Fenster gegen Wiederholung (Bibliothek); Folgenummern im Spiel | Bibliothek; Spiel unverändert |
| T4 Veränderung auf dem Weg | DTLS 1.2 (AEAD), Vorwärtsgeheimnis durch ECDHE (V13.2 hatte keins). **Zusatz:** die DLL verlangt, dass der von DTLS geprüfte Fingerabdruck vorliegt und zur Beschreibung passt (Abschnitt 9.4) | Verbindungstests, Fingerabdruck geprüft |
| I1 Speicher nach außen | Längen nur innerhalb geprüfter Nachrichten; Lobby baut weitergereichte Nachrichten neu | Fuzzing der Lobby-Nachrichten (300 000 Fälle) |
| I2 Adressen anderer Gäste | Gäste verbinden sich nur mit dem Host; Option „Eigene IP verbergen“ (nur Relay). Grenze: Abschnitt 9.5 | Relay-Weg auf getrennten Adressen |
| I3 Pfade, Rechnernamen | keine im Netz; die Lobby protokolliert keine IP | Durchsicht |
| D1 Flut | DLL: 120/s und Vorrat 60 je Gast, anhaltend → getrennt; höchstens 256 Nachrichten je Kanal und Bild (Rest bleibt in der Bibliothek); Lobby: 30/s je Verbindung | Flut gegen Host und Lobby |
| D2 Speicher | Nachrichten bis 1200+1 Byte (DLL), 4 KiB (Bibliothek), 8 KiB und Tiefe 4 (Lobby); Eingang der Rückrufe höchstens 2048 Einträge; unzuverlässiger Puffer 16 KiB | Übergröße, tiefe Schachtelung |
| D3 Rechenzeit | unverändert; Lobby: Nachrichten je Durchgang begrenzt | Lobby-Fuzzing |
| D4 Hängende Verbindungen | Beitritt 20 s, Stille 10 s (Ping 2/s), Lobby: ohne Anmeldung 10 s, wartende Gäste 60 s, Räume 12 h | Zeitlimit ohne Hallo, Netzausfall, Host fällt aus |
| D5 Wiederverbinden in Schleife | Lobby: 12 Verbindungen und 20 Beitritte/min je Adresse; Host: 4 halbe Beitritte, 3 je Adresse | Verbindungsflut gegen die Lobby |
| E1 Codeausführung durch Parser | eigene Parser: feste Felder (DLL), nlohmann/json mit Größen- und Tiefengrenze (Lobby). Fremde Parser: STUN, DTLS, SCTP, WebSocket, HTTP (Bibliotheken, Abschnitt 9.3) | Fuzzing eigener Parser; Netz-Fuzzing des Hosts |
| E2 Daten als Code | unverändert | – |
| E3 Rollen vertauscht | Spielprotokoll unverändert; Lobby: Host-Befehle nur vom Host, „to“ nur vom Host | Gast sendet Host-Befehl → getrennt |

**Neue Bedrohungen:**

| # | Bedrohung | Gegenmaßnahme | Prüfung |
|---|---|---|---|
| L1 | Lobby überlasten (Verbindungen, Räume, Nachrichten) | Grenzen je Adresse und insgesamt (Architektur, Abschnitt 6); Verstöße trennen | Angriffe auf die Lobby, 3 000 veränderte Nachrichten über das Netz |
| L2 | Raumnummern raten | 30 Bit, Ratenbremse, Sperre; selbst ein Treffer nützt ohne Geheimteil nichts | Code raten |
| L3 | Lobby bösartig/übernommen | Lobby sieht das Geheimteil nicht; mit langem Code kann sie den Host nicht vortäuschen. **Mit kurzem Code** könnte eine bösartige Lobby Beschreibungen tauschen und sich dazwischenschalten (Restrisiko) | Prüfteil-Test |
| L4 | `wss://` ohne Prüfung | WinHTTP/Schannel prüft (Kette, Name, Sperrlisten) oder festgelegtes Zertifikat; geprüft, bevor die erste Lobby-Nachricht geht | TLS-Tests |
| L5 | TURN als offenes Relay | nur mit Zugangsdaten je Raum (zufällig, höchstens 12 h), Obergrenze an Zuteilungen je Raum (4 × Plätze) und insgesamt (1000) | Bibliothek; Zugangsdaten nicht eigens angegriffen |
| L6 | Eingebaute Lobby (Direkt) offen im LAN | ein Raum; „Host“ nur über die eigene Loopback-Adresse; dieselben Grenzen; der lange Code ist Pflicht | zweiter Raum abgewiesen |

### 9.3 Abhängigkeiten: Versionen und bekannte Lücken (27.10.2026)

Abfrage: OSV (Commit-Abfrage), GitHub Security Advisories der Projekte, Websuche (`MULTIPLAYER_RECHERCHE.md`, Abschnitt 4).

| Baustein | Stand | Bekannte Lücken für diesen Stand | Aktuell? |
|---|---|---|---|
| libdatachannel | v0.24.6 (`6b1e2e62`, 26.09.2026) | keine | neuester Release |
| libjuice | v1.7.4 (`b89c792e`, 26.09.2026) | keine | neuester Release |
| usrsctp | `fec583d5` (26.06.2025; von libdatachannel 0.24.6 festgelegt) | keine; ältere (CVE-2019-20503) behoben | Hauptzweig hat neuere Commits (10/2026), es gibt keine Releases |
| Mbed TLS | 3.6.7 LTS (07.07.2026, SHA-256 des Archivs geprüft) | keine bekannte offene für 3.6.7 | neueste 3.6 (4.x wird von libdatachannel nicht unterstützt) |
| plog | `94899e0b` | keine | nicht neuester Release (1.1.11); nur Protokollausgabe, abgeschaltet |
| nlohmann/json | 3.12.0 | keine | neuester Release |
| coturn (nicht verwendet, Vergleich) | – | CVE-2026-62959, CVE-2026-68552 (behoben in 4.15.0) | – |

Lehre aus CVE-2026-103936 (`@libp2p/webrtc`, Absturz durch ungeprüfte Werte an libdatachannel): Alle Aufrufe der Bibliothek in `try/catch`, Werte aus dem Netz vorher auf Länge und Zeichen geprüft.

### 9.4 Funde beim Lesen der Bibliotheken (nicht gemeldet)

1. **DTLS mit Mbed TLS:**
   - Befund: `MBEDTLS_SSL_VERIFY_OPTIONAL`; ein Gegenüber ohne Zertifikat würde nicht abgewiesen. Ein falscher Fingerabdruck bricht dagegen ab.
   - **Gegenmaßnahme in der DLL:** `remoteFingerprint()` muss gesetzt und gleich dem aus der Beschreibung sein, sonst Trennung.
2. **WebSocket-Client unter Windows:**
   - Befund: libdatachannel schaltet die TLS-Prüfung ab („not supported on Windows“); mit Mbed TLS ohne Prüfung sogar `VERIFY_NONE`.
   - **Gegenmaßnahme:** Die Verbindung zur Lobby läuft über WinHTTP. Der Server-Teil der Lobby nutzt libdatachannel weiter; er prüft keine Client-Zertifikate.
3. **Relay-Politik:**
   - Befund: `TransportPolicy::Relay` filtert nur die gemeldeten eigenen Kandidaten. libjuice prüft Wege auch von der eigenen Adresse aus.
   - Abschnitt 9.5.

Status: Nicht an die Entwickler gemeldet (dafür wäre ein Konto nötig). Empfehlung: melden, sobald ein Konto des Projekts besteht.

### 9.5 Restrisiken V13.3 (zusätzlich zu Abschnitt 7)

- **Kurzer Code vertraut der Lobby:**
  - Eine bösartige oder übernommene Lobby kann sich bei kurzem Code zwischen Host und Gast schalten (Beschreibungen tauschen) und so auch das Geheimteil erfahren.
  - Der lange Code verhindert das. Im Modus „Direkt“ ist er Pflicht.
- **Lobby und Relay sind ein Ziel im Internet:**
  - Die Grenzen dämpfen Überlast, ein verteilter Angriff (viele Adressen) kann die Lobby trotzdem auslasten. Laufende Spiele auf direktem Weg sind davon nicht betroffen, neue Beitritte schon.
- **Fremde Parser:**
  - STUN/TURN (libjuice), DTLS (Mbed TLS), SCTP (usrsctp, groß und alt), WebSocket/HTTP (libdatachannel, WinHTTP) verarbeiten Daten aus dem Netz, bevor eigener Code sie sieht.
  - SCTP läuft erst nach DTLS, also nur mit Gegenstellen, die die Signalisierung durchlaufen haben.
  - Diese Teile wurden **nicht** selbst gefuzzt (nur über die eigenen Netz-Tests mitbenutzt).
- **„Eigene IP verbergen“ hängt am TURN-Server:**
  - libjuice sendet Prüfpakete auch von der eigenen Adresse an die Relay-Adresse der Gegenseite. Der TURN-Server stellt sie nur zu, wenn die Absender-IP in den Berechtigungen der Gegenseite steht. Bei getrennten Rechnern ist das nicht so; nachgestellt auf getrennten Loopback-Adressen.
  - Ein anderer TURN-Server mit lockeren Berechtigungen könnte das ändern.
- **Host sieht IPs der Gäste** bei direktem Weg (wie vorher); Gäste sehen einander nicht.
- **Lobby-Betreiber sieht IPs** (nur im Speicher). Datenschutzpflichten beim Betreiber (`MULTIPLAYER_SETUP.md`, Abschnitt 5).
- **Nur über UDP:** In Netzen ohne UDP gibt es keinen Weg. Das ist kein Sicherheits-, aber ein Verfügbarkeitsrisiko.
- **Nicht über das echte Internet geprüft:** alle Tests liefen auf einem Rechner (Loopback, Netzstörer). Eine Anleitung für den Test mit zwei Rechnern in verschiedenen Netzen steht in `MULTIPLAYER_TESTS.md`, Abschnitt 7.6.
- **Durchsicht:** wieder nur durch mich, entlang dieser Tabelle. Dabei gefunden und behoben:
  - Absagen kamen beim Gast nicht an, weil die Verbindung sofort geschlossen wurde → jetzt 1 s Nachlauf;
  - Regelbrecher belegten 120 s einen Platz;
  - der Gast sah Wesen-Rollen über 5 nicht in Ereignissen.
  - Eine unabhängige Durchsicht durch Dritte gibt es weiterhin nicht.
