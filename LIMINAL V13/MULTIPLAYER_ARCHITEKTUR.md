# LIMINAL – Mehrspieler-Architektur (Koop)

Stand: 27.10.2026, V13.3. Das Dokument entstand in V13.2 vor der ersten Netzwerkzeile des Spiels.
- **V13.3** hat den Verbindungsaufbau ersetzt: Beitrittscode über eine Lobby, NAT-Durchdringung mit Relay-Rückfall, Verschlüsselung durch die Bibliothek. Betroffen sind die Abschnitte 1, 4–8, 12 und 13.
- Bindend sind die Regeln aus `MULTIPLAYER_BEDROHUNGSMODELL.md` (Abschnitt 6); dieses Dokument legt fest, wie sie umgesetzt werden.
- Wahl der Lösung: `MULTIPLAYER_RECHERCHE.md`, E-033. Betrieb: `MULTIPLAYER_SETUP.md`. Prüfungen: `MULTIPLAYER_TESTS.md`. Freigabe: `MULTIPLAYER_ENTSCHEIDUNG.md`.

Nichts hier behauptet, der Mehrspielermodus sei „vollkommen sicher“. Ziel ist ein kleines, bekanntes und geprüftes Restrisiko.

## 1. Rahmen

- Koop für 2–4 Spieler auf derselben Ebene, gemeinsame Reise durch die zehn Ebenen.
- Ein Spieler hostet, die anderen treten bei:
  - **mit einem Beitrittscode über eine Lobby**, ohne Router-Einstellungen;
  - oder direkt per Adresse und langem Code (LAN, Portfreigabe).
- Keine Konten, keine App-IDs, kein Dienst eines Dritten, der freigeschaltet werden müsste.
- Die Lobby mit Relay (`liminal_lobby.exe`) betreibt der Herausgeber oder eine Spielergruppe selbst. Was sie sieht: Abschnitt 8.

## 2. Gastgeber oder eigener Server?

**Entscheidung: hostautoritativ (Listen-Server).** Ein Spieler startet „Spiel hosten“. Sein Spiel ist der Server und rechnet die ganze Welt; die Gäste sind Clients.

| Kriterium | Listen-Server (gewählt) | Eigener Server |
|---|---|---|
| Aufwand für Spieler | ein Klick im Menü | eigenes Programm starten, betreiben |
| Infrastruktur | keine | Rechner oder Mietserver nötig (Kosten, Datenschutz) |
| Passt zur Spielgröße (2–4 Freunde) | ja | überdimensioniert |
| Vertrauen | der Host kann seinen eigenen Stand manipulieren (im Koop hinnehmbar); Gäste können nichts am Zustand ändern | wie Listen-Server, ohne bevorzugten Spieler |
| Spätere Erweiterung | Der Server-Teil läuft ohne Fenster (die Tests nutzen das), ein eigener Server ist damit möglich | – |

Die Sitzung (`GameSession`) kennt weder Renderer noch Audio; der Server-Teil läuft deshalb auch ohne Fenster. So laufen die Bot-Tests.

## 3. Autorität: der Client sendet nur Eingaben

Der Host entscheidet alles, was Spielzustand ist:
- Position und Bewegung aller Spieler
- Gesundheit, Verstand, Ausdauer
- Inventare, Aufnehmen und Ablegen von Gegenständen, Benutzen von Vorräten
- Lampenbatterie, Stromausfälle
- Wesen: KI, Angriffe, Schaden, Tod
- Übergänge zwischen Ebenen, Sicherungspunkte, Speichern

Ein Gast sendet nur:
- **Eingaben je Bild:** Folgenummer, Bilddauer, Bewegungsachsen (−1…1), Blickwinkel, Tastenbits (Rennen, Schleichen, Springen, Ducken).
- **Absichten:** „aufnehmen Gegenstand #id“, „Fach n benutzen“, „Fach n ablegen“, „Lampe schalten“, „am Ausgang bereit“.

Der Host prüft jede Absicht an seinem eigenen Zustand:
- Reichweite und Sicht zum Gegenstand
- Inhalt des Inventars
- Abklingzeiten

Positionen, Werte oder Inventarinhalte eines Gastes nimmt der Host nie an. Es gibt dafür keine Nachricht.

## 4. Transport

**Entscheidung (V13.3, E-033): WebRTC-Datenkanäle mit libdatachannel 0.24.6** (MPL-2.0, Paul-Louis Ageneau). Darunter:
- libjuice 1.7.4: ICE, STUN, TURN;
- usrsctp: SCTP;
- Mbed TLS 3.6.7 LTS: DTLS 1.2.

Alle Quellen liegen unverändert in `third_party/`; Versionen, Commits und Prüfsummen in `LICENSES.md`, Abschnitt 7.

- **Je Gast eine Verbindung** (Peer-Verbindung) mit zwei Kanälen:
  - `r`: zuverlässig, geordnet – Beitritt, Ereignisse, Inventar, Gruppe;
  - `u`: ungeordnet, Lebensdauer 150 ms (PR-SCTP) – Eingaben, Momentaufnahmen, Ping. Was in 150 ms nicht durchkommt, verfällt; bei knapper Bandbreite stauen sich so keine veralteten Momentaufnahmen.
- **Verbindungsweg (ICE):** direkt (Adressen der Netzwerkkarten), über NAT (öffentliche Adresse per STUN) oder **über das TURN-Relay**, wenn nichts anderes geht. Die Wahl trifft ICE von selbst. Das Spiel zeigt den Weg je Spieler an (direkt / direkt durch NAT / über Relay).
- **Höchstgröße:** 1200 Byte Nutzdaten je Nachricht. Längeres verwirft die DLL; die Bibliothek nimmt höchstens 4 KiB je Nachricht an.
- **V13.2 verworfen:**
  - ENet 1.3.18 mit eigenem Handshake (HMAC-Beweis) und eigener AES-256-GCM-Schicht ist **entfernt**.
  - Begründung: eigene Kryptografie, keine NAT-Durchdringung (`MULTIPLAYER_RECHERCHE.md`).

| Bewertet (V13.3) | NAT ohne Portfreigabe | Verschlüsselung/Authentisierung | Urteil |
|---|---|---|---|
| libdatachannel + libjuice + Mbed TLS | ja (ICE + TURN) | DTLS, Fingerabdruck aus der Signalisierung | **gewählt** |
| Steam Networking Sockets / EOS P2P | ja (Relay des Anbieters) | ja | Konto und SDK nur mit Konto; später als zweiter Weg möglich |
| GameNetworkingSockets (Open Source) | ja (ICE, TURN seit 1.6) | ohne eigene Zertifizierungsstelle nicht authentisiert | Protobuf/Abseil, Linkprobleme mit LLVM |
| netcode/yojimbo, ENet | nein | netcode ja, ENet nein | verworfen |
| Photon, PlayFab Party | ja (Cloud) | ja | Konto, laufende Kosten, Verkehr bei Dritten |

## 5. Verschlüsselung und Beitritt

Keine eigene Kryptografie. Verschlüsselung und Echtheit der Gegenstelle macht DTLS. Dazu kommt eine Zusammensetzung bewährter Bausteine, kein neues Verfahren:

- **DTLS 1.2** je Verbindung (Mbed TLS):
  - ECDHE mit Vorwärtsgeheimnis;
  - AES-GCM bzw. ChaCha20-Poly1305;
  - Zertifikate ECDSA P-256.
  - Jede Seite prüft das Zertifikat der Gegenseite gegen den SHA-256-Fingerabdruck aus deren Verbindungsbeschreibung (RFC 8827).
- **Zusätzliche Prüfung in der DLL:** Nach dem Öffnen der Kanäle muss der von DTLS geprüfte Fingerabdruck vorliegen und gleich dem aus der Beschreibung sein. Hintergrund: libdatachannel setzt mit Mbed TLS `VERIFY_OPTIONAL`; ein Gegenüber ohne Zertifikat würde sonst nicht abgewiesen (`MULTIPLAYER_RECHERCHE.md`, Abschnitt 4).
- **Beitrittscode** (Crockford-Base32, Vierergruppen; `src/netdll/joincode.*`):

  | Teil | Zeichen | Bit | Wer prüft | Zweck |
  |---|---|---|---|---|
  | Raumnummer | 6 | 30 | Lobby | Raum finden |
  | Geheimteil | 6 | 30 | Host, **im DTLS-Kanal** (Vergleich in fester Zeit) | Schutz vor Fremdbeitritt; die Lobby sieht ihn nie |
  | Prüfteil (nur langer Code) | 12 | 60 | Gast, **bevor er etwas sendet** | Pinning: erste 60 Bit des SHA-256-Fingerabdrucks des Host-Zertifikats |

  - **Kurzer Code** (12 Zeichen, z. B. `K7QF-M2XA-9FJT`) für die Lobby mit `wss://`.
  - **Langer Code** (24 Zeichen) für „Direkt“: dort Pflicht, weil die Signalisierung zur eingebauten Lobby unverschlüsselt ist. Über die Lobby geht er auch.
  - Das Host-Zertifikat erzeugt die DLL einmal je Sitzung (`hostcert.cpp`, Mbed TLS) und nutzt es für alle Gäste. Nur so kann der Prüfteil im Code stehen.
- **Ablauf:**
  1. Gast → Lobby: Raumnummer, Name.
  2. Host ↔ Gast über die Lobby: Beschreibungen (SDP mit Fingerabdruck) und ICE-Kandidaten.
  3. Der Gast prüft den Prüfteil, wenn vorhanden.
  4. ICE findet einen Weg, DTLS prüft die Fingerabdrücke.
  5. Gast → Host: **HALLO** (Version der DLL, Geheimteil, Name) im zuverlässigen Kanal.
  6. Host → Gast: **WILLKOMMEN** oder **ABSAGE** mit Grund. Erst jetzt melden beide „verbunden“; der Gast verlässt die Lobby.
- **Rahmung in der DLL** (das Einzige, was über den Daten der Bibliothek liegt):
  - je Nachricht 1 Byte Art: Hallo, Willkommen, Absage, Nutzdaten, Ping, Pong;
  - feste Längen; alles andere zählt als ungültig.
- **Fehlversuche:**
  - Falsches Geheimteil, falscher Name, Zeitlimit oder Müll zählen je Adresse.
  - Nach 5 Fehlversuchen in 10 min weist der Host die Adresse 10 min lang ab, schon wenn die Lobby den Beitritt meldet. Die Lobby nennt dem Host dafür eine Kennung der Adresse je Raum, nicht die IP.
  - Ein Bann durch den Host (Pausenmenü) gilt auch in der Lobby: Sie weist die Adresse für diesen Raum ab.
  - Die Lobby selbst sperrt eine Adresse nach 10 unbekannten Raumnummern in 10 min für 10 min.
- **Sitzungstoken** (128 Bit, Spielprotokoll) wie bisher: Wer innerhalb von 120 s mit demselben Code und Token wiederkommt, bekommt seinen Platz, seine Werte und sein Inventar zurück.
- **Versionsprüfung auf drei Ebenen:**
  - Lobby-Protokoll v1 (Lobby antwortet „version“);
  - DLL-Rahmung v3 (Absage VERSION);
  - Spielprotokoll v3 mit Prüfsumme der Spieldaten (Absage „andere Version“ bzw. „andere Spieldaten“).

## 6. Lobby und Relay

`src/netdll/lobby_server.*`, als Programm `tools/lobby/lobby_main.cpp` → `liminal_lobby.exe`. Eigener Code, weil es für Beitrittscodes keinen fertigen, kontofreien Dienst gibt. Bewusst klein gehalten.

- **WebSocket-Server** (libdatachannel, TLS über Mbed TLS) mit JSON-Nachrichten (nlohmann/json):
  - Raum öffnen;
  - beitreten;
  - Beschreibungen und Kandidaten weiterreichen;
  - abweisen, sperren, verlassen.

  Format und Prüfregeln: `src/netdll/lobby_proto.hpp`. Jede Seite prüft jedes Feld (Typ, Länge, Zeichen, Bereich). Weitergereicht wird nie das empfangene JSON, sondern eine neu gebaute Nachricht aus geprüften Feldern.
- **STUN/TURN-Server** von libjuice im selben Programm:
  - Für jeden Raum legt die Lobby **eigene, zeitlich begrenzte TURN-Zugangsdaten** an (zufällig, Gültigkeit = Lebensdauer des Raums, höchstens 12 h) mit einer Obergrenze an Zuteilungen.
  - Feste Zugangsdaten gibt es nicht.
- **Grenzen:**

  | Grenze | Wert |
  |---|---|
  | Nachricht | höchstens 8 KiB, Schachtelungstiefe 4 |
  | Nachrichten je Verbindung | 30/s, Vorrat 60; anhaltend darüber: getrennt |
  | Verbindungen | 512 insgesamt, 12 je Adresse |
  | Räume | 256 insgesamt, 4 je Adresse |
  | Beitrittsversuche je Adresse | 20/min |
  | gleichzeitig wartende Gäste je Raum | 4 |
  | Zeitlimits | ohne Anmeldung 10 s; wartender Gast 60 s; Raum höchstens 12 h |

  Verstöße trennen die Verbindung; die Lobby läuft weiter.
- **Eingebaute Lobby (Direkt):**
  - Der Host startet denselben Server in seiner DLL: ein Raum, ohne Relay; „Host“ nur über die eigene Loopback-Adresse.
  - Alle Gäste-Verbindungen nutzen einen festen UDP-Port (ICE-Mux), Standard 27500; damit ist eine Portfreigabe (TCP und UDP 27500) möglich.
  - Der Gast nimmt die eingegebene Adresse zusätzlich als ICE-Kandidaten.
- **Verbindung des Spiels zur Lobby** über **WinHTTP** (`src/netdll/lobby_client.*`):
  - Windows prüft das TLS-Zertifikat von `wss://` (Schannel, Stammzertifikate, Sperrlisten), oder es zählt ein festgelegtes Zertifikat (Pinning über `koop.json`).
  - Der WebSocket-Client von libdatachannel wurde dafür nicht genommen: Unter Windows schaltet er die Zertifikatsprüfung ab (`websocket.cpp`: „not supported on Windows“).
- **Lobby verloren (Host):** Laufende Spiele laufen weiter (direkt oder über das Relay). Die DLL versucht alle 5 s neu und meldet einen **neuen** Code, sobald die Lobby wieder da ist.

## 7. Prozess- und Bibliotheksgrenze (Regel 6)

- Netzwerk, WebRTC, Lobby-Verbindung und Kryptografie liegen in **`liminal_net.dll`**. Das Spiel lädt sie mit `LoadLibrary` erst, wenn man hostet oder beitritt.
- `LiminalGame.exe` importiert keine Netzwerkbibliothek; `tools/check_no_network.py` prüft das weiter.
- Die DLL darf `ws2_32`, `iphlpapi` (Netzwerkschnittstellen für ICE) und `winhttp` (Lobby) importieren, keine Download-Bibliothek. Sie exportiert genau die elf `ln_`-Funktionen.
- Ohne Mehrspieler öffnet das Spiel keinen Port.
- **Schnittstelle der DLL** (`src/netdll/ln_api.h`, Version 3):

  | Funktion | Aufgabe |
  |---|---|
  | `ln_host`, `ln_join` | Host/Gast öffnen (Lobby oder direkt) |
  | `ln_poll` | Ereignisse: verbunden, getrennt mit Grund, Nachricht, Code bereit, Fortschritt |
  | `ln_send`, `ln_drop` | senden; Kick, Bann |
  | `ln_stats` | Laufzeit, Verlust, Weg, Bytes |
  | `ln_code`, `ln_port`, `ln_close`, `ln_random`, `ln_version` | Hilfen |

  Sie liefert nur entschlüsselte, geprüfte Nutzdaten fester Höchstgröße.
- Die Schnittstelle ist so geschnitten, dass sich ein zweiter Transport (z. B. Steam Networking Sockets) dahinter setzen lässt, ohne das Spiel zu ändern.
- Das Spielprotokoll prüft `src/net` (Prüfschicht aus V13, erweitert): Typ, Länge, Bereich, Rate, Folge, Rolle.
- Alle Aufrufe der Bibliothek stehen in `try/catch` (sie meldet Fehler als Ausnahme). Ihre Rückrufe legen nur Beschreibungen, Kandidaten und neue Kanäle in einen Eingang. Nachrichten holt die DLL im Spiel-Thread ab (höchstens 256 je Kanal und Bild; der Rest bleibt in der Bibliothek, Gegendruck).

## 8. Verbindung, NAT, Firewall, Datenschutz

- **Über die Lobby:**
  - Host und Gäste verbinden sich nur **ausgehend** (TCP zur Lobby, UDP für ICE).
  - Ohne Router-Einstellungen: direkter Weg, wenn NAT es zulässt, sonst das Relay der Lobby.
  - Netze, die UDP ganz sperren, gehen nicht (libjuice spricht TURN nur über UDP; Abschnitt 12).
- **Direkt:** Im LAN genügt die Adresse des Hosts; übers Internet braucht der Host eine Portfreigabe (TCP und UDP 27500).
- **Firewall:** Beim ersten Hosten im Modus „Direkt“ fragt Windows, ob das Spiel Verbindungen annehmen darf. Ob die Abfrage auch im Lobby-Modus erscheint (UDP-Sockets für ICE), ist **nicht geprüft**.
- **Daten im Netz:**
  - Spielername (höchstens 16 Zeichen) und Spielzustand.
  - Gäste erfahren keine Adressen anderer Gäste.
  - Host und Gast sehen einander, wenn ein direkter Weg gefunden wird. Mit **„Eigene IP verbergen“** (nur Relay) meldet die DLL keine direkten Kandidaten, und ICE nutzt nur das Relay.
  - Grenze: libjuice schickt Prüfpakete auch von der eigenen Adresse an die Relay-Adresse der Gegenseite. Der TURN-Server lässt sie nur durch, wenn die Absender-IP in den Berechtigungen der Gegenseite steht. Zwischen getrennten Rechnern ist das nicht so; im Test auf getrennten Loopback-Adressen nachgestellt.
- **Was die Lobby sieht:**
  - IP-Adressen (nur im Arbeitsspeicher, für Raten und Sperren);
  - Raumnummer, Namen, Verbindungsbeschreibungen.
  - **Nicht:** das Geheimteil des Codes, den Spielverkehr.
  - Das Relay sieht Adressen und verschlüsselte Pakete.
  - Folgen für den Betrieb: `MULTIPLAYER_SETUP.md`, Abschnitt 5.
- **Protokolle:**
  - Die Lobby schreibt keine IP-Adressen ins Protokoll.
  - Das Spiel protokolliert lokal, nie über das Netz.
  - Die DLL schreibt nur mit `LIMINAL_NET_LOG=1` Meldungen der Bibliothek auf stderr.

## 9. Welt und Synchronisierung

- **Welt aus dem Seed:** Die Ebenen sind feste Karten, Funde verteilt der Spawnplan nach Seed. Host und Gäste erzeugen dieselbe Welt (der Willkommensgruß überträgt Ebene, Seed des Laufs und Schwierigkeit). Prüfsumme der Welt zum Vergleich.
- **Zustandsänderungen** (zuverlässiger Kanal, Ereignisse mit fortlaufender Nummer):
  - Gegenstand aufgenommen (id)
  - Gegenstand abgelegt (id, Art, Ort)
  - Stromausfall (Raum, Dauer)
  - Ebenenwechsel
  - Sicherungspunkt
  - Tod und Wiedereinstieg eines Spielers
  - Meldungen
- **Momentaufnahmen** (unzuverlässig, 20 je Sekunde):
  - alle Spieler (Ort, Geschwindigkeit, Blick, Lampe, Haltung)
  - die eigenen Werte (Gesundheit, Verstand, Ausdauer, Batterie)
  - die Wesen in der Nähe (Art als Zahl bis `kRoles` = 40, Ort, Richtung, Eingaben ihrer Bewegung)
  - letzte verarbeitete Eingabe je Gast

  Höchstens 1200 Byte je Paket.
- **Bewegung:**
  - **Vorhersage:** Der Gast bewegt sich sofort mit derselben Physik wie der Host.
  - **Korrektur:** Mit jeder Momentaufnahme setzt der Gast den Stand des Hosts ein und rechnet die noch unbestätigten Eingaben nach. Kleine Fehler gleichen sich weich aus, große springen.
- **Andere Spieler und Wesen:** 100 ms in der Vergangenheit zwischen zwei Momentaufnahmen interpoliert.
- **Körper der Mitspieler:** das vorhandene Spielermodell mit der Lampenhaltung von außen (B-001). Ihr Lichtkegel ist ein eigenes Licht im Renderer.
- **Wesen:** KI nur auf dem Host. Gäste zeigen „Puppen“: Ort, Richtung und Eingaben der Bewegung kommen vom Host, die Bewegung (Gang, Atem, Licht) rechnet der Gast mit derselben Animation. Die Wesen wählen als Ziel den nächsten Spieler, den sie wahrnehmen. Keine Erscheinung im Blickfeld irgendeines Spielers.

## 10. Spielregeln im Koop

- **Ebenenwechsel:** Wer einen Ausgang betritt, ist „bereit“. Alle sehen „Ausgang gefunden – 1/3 bereit“. Sind alle Lebenden bereit, wechselt die Gruppe. Nach 60 s ohne alle zieht der Host die Übrigen mit (Ankündigung ab 15 s). Spieler auf verschiedenen Ebenen gibt es nicht.
- **Tod:** Der Gestorbene sieht einem lebenden Mitspieler zu (Wechsel mit der Maus). Er kommt nach 20 s am letzten Sicherungspunkt zurück, solange jemand lebt. Sterben alle, beginnt die Gruppe am Sicherungspunkt neu (wie im Einzelspiel).
- **Vorräte:** Die Zahl der Funde wächst mit der Spielerzahl (Faktor 1 + 0,6 × (n − 1)). Jeder hat ein eigenes Inventar; man kann Dinge ablegen, damit andere sie aufnehmen.
- **Verstand und Halluzinationen:** Den Verstand rechnet der Host. Halluzinationen sieht nur der Betroffene.
- **Speichern:** nur der Host, in einem eigenen Koop-Spielstand (Werte je Spielername), nie über den Einzelspielerstand. Gäste speichern nichts.

## 11. Rollen und Rechte

| Rolle | Darf |
|---|---|
| Host | Sitzung öffnen und schließen, Spieler entfernen (Kick) oder sperren (Bann für die Sitzung, auch in der Lobby), speichern |
| Gast | Eingaben und Absichten senden, Sitzung verlassen |

- Nachrichtentypen haben je Rolle und Verbindungszustand eine Erlaubnisliste.
- Ein Gast, der eine Host-Nachricht sendet, wird getrennt.
- Höchstens 4 Spieler (Host + 3).
- Halbe Beitritte (Signalisierung, ICE, DTLS, Hallo): höchstens 4 gleichzeitig, 3 je Adresse, Zeitlimit 20 s (über das Relay dauert der Aufbau länger als die 5 s der ENet-Zeit).
- Stille: 10 s, dann wird getrennt.
- Wiederverbinden: innerhalb von 120 s mit demselben Sitzungstoken kehrt der Gast mit Inventar und Werten zurück.

## 12. Grenzen (V13.3)

- **Keine Sprachübertragung.** Begründung in `MULTIPLAYER_ENTSCHEIDUNG.md`: Mikrofonzugriff, Audio-Codec und Moderation sind ein eigenes Datenschutz- und Sicherheitsthema.
- **Kein Serverbrowser:** Beitritt nur mit Code (bewusst: keine öffentliche Liste von Räumen).
- **Nur UDP:**
  - libjuice spricht TURN nur über UDP.
  - Netze ohne UDP (manche Firmen- und Schulnetze) bekommen keine Verbindung, auch nicht über das Relay. Das Spiel meldet dann klar „kein Verbindungsweg“.
  - Abhilfe wäre TURN über TCP/TLS (z. B. coturn mit libnice); nicht umgesetzt.
- **Lobby-Betrieb:** Ohne öffentlich erreichbare Lobby geht nur „Direkt“. Die Adresse der Lobby ist der eine fehlende Eintrag (`koop.json`).
- **Lobby und Spiel nur für Windows gebaut und geprüft.** Der Code der Lobby ist bis auf nichts Windows-Spezifisches portabel, ein Bau unter Linux ist aber nicht versucht.
- Die letzte Aufzugsfahrt (Flucht mit drei Sicherungen, V9) gibt es nur in der alten, zusammenhängenden Welt. Die Reise durch die zehn Ebenen nutzt sie nicht; im Koop ist sie aus.

## 13. Umsetzung (Stand V13.3)

| Teil | Wo | Anmerkung |
|---|---|---|
| Transport, Code, Rahmung | `src/netdll/transport.cpp`, `joincode.*`, `hostcert.*` → `liminal_net.dll` | libdatachannel (statisch), Exporte: genau die elf `ln_`-Funktionen (`tools/check_no_network.py`) |
| Lobby-Nachrichten | `src/netdll/lobby_proto.*` | Prüfschicht, gemeinsam für DLL und Lobby |
| Lobby- und Relay-Server | `src/netdll/lobby_server.*`, `tools/lobby/lobby_main.cpp` → `liminal_lobby.exe` | auch eingebaut im Host (Direkt) und in den Tests |
| Verbindung zur Lobby | `src/netdll/lobby_client.*` | WinHTTP (TLS-Prüfung durch Windows, Pinning) |
| Einstellungen von Mbed TLS | `src/netdll/config/liminal_mbedtls_config.h` | DTLS-SRTP (zum Bauen nötig), Threads, ohne Sockets |
| Spielprotokoll v3 | `src/net/coop_proto.*` | unverändert bis auf die Obergrenze `kRoles` = 40 (Format gleich, Abschnitt 9) |
| Host und Gast | `src/coop/coop.*`, `netapi*.cpp` | `CoopHost::start(NetSettings)`, `CoopClient::connect(NetSettings, Adresse, Code)`; Gründe als Text (`reasonText`) |
| Einstellungen | `koop.json` (Programmordner, dann Nutzerordner), `App::netSettings` | `MULTIPLAYER_SETUP.md`, Abschnitt 3.4 |
| Oberfläche | `src/ui/ui_coop.cpp` | Hosten/Beitreten, Code, Lobby oder Direkt, „Eigene IP verbergen“; Pausenmenü „Mitspieler“ mit Code (Strg+C), Ping, Verlust, Weg; klare Fehlermeldungen; Fortschritt beim Beitritt |
| Prüfungen | `tests/net_transport_test.cpp`, `tests/coop_test.cpp` | `MULTIPLAYER_TESTS.md`, Abschnitt 7 |

Seit V13.3 zusätzlich:

- **Rollen der Wesen:**
  - Die Obergrenze `kRoles` im Spielprotokoll ist 40 (Format unverändert: ein Byte).
  - Neue Wesen in `Presence::Role` brauchen bis 40 keine Protokolländerung; `static_assert` in `coop.cpp` wacht darüber.
  - Rollen, die ein Gast nicht kennt, zeigt er nicht an (statt sie auf ein anderes Wesen zu klemmen). Klänge zu Rollen über 5 erreichen die Gäste jetzt auch (vorher auf 0–5 geklemmt).
- **Regelbrecher:** Wer wegen ungültiger Nachrichten getrennt wird, behält keinen Platz zum Wiederverbinden.
