# LIMINAL – Recherche: Verbindungsaufbau im Koop ohne Router-Einstellungen

Stand: 27.10.2026 (V13.3). Auftrag: Jeder Spieler auf der Welt soll einem Host beitreten können, ohne im Router etwas einzustellen. Keine eigene Kryptografie, kein eigenes Transportprotokoll, wenn es eine bewährte Lösung gibt. Ergebnis und Begründung: Abschnitt 5, Eintrag E-033 in `DECISIONS.md`.

Diese Datei trennt, was belegt ist (mit Quelle), von dem, was nur ein Hinweis ist. Alle Webseiten wurden am 27.10.2026 abgerufen. Versionen, Commits und Prüfsummen der gewählten Bibliotheken: `LICENSES.md`, Abschnitt 7.

## 1. Ausgangslage (V13.2)

- Transport: ENet 1.3.18 (UDP, MIT) in `liminal_net.dll`.
- Darüber ein **selbst geschriebener** Handshake (Beweis des Beitrittsschlüssels per HMAC) und eine **selbst zusammengesetzte** AES-256-GCM-Schicht über die Windows-Kryptografie (CNG).
- Verbindung nur per IP und Port. Über das Internet braucht der Host eine Portfreigabe. Hinter Carrier-Grade-NAT (Mobilfunk, viele Glasfaser- und Kabelanschlüsse) geht es gar nicht.
- Zwei Mängel gegenüber dem Auftrag:
  - Eigene Kryptografie und ein eigenes Handshake-Protokoll, obwohl es bewährte Lösungen gibt.
  - Keine NAT-Durchdringung und kein Relay.

## 2. Wie es erfolgreiche Koop-Spiele machen

| Spiel | Verbindung | Belege | Sicherheit des Belegs |
|---|---|---|---|
| **Lethal Company** (Unity, 4 Spieler, Host spielt mit) | Unity *Netcode for GameObjects* über **Facepunch Transport** auf Steam: Lobby, Einladung und Verbindung laufen über Steam (Steam Networking mit Relay). | Die Mod-Vorlage nennt die Spiel-DLLs „Facepunch Transport for Netcode for GameObjects.dll“ und „Facepunch.Steamworks.Win64.dll“ [1]. | mittel: aus den Dateien des Spiels, nicht vom Entwickler |
| **Phasmophobia** (Unity, 4 Spieler) | Räume über Serverregionen (NA, EU, AS …), Code zum Beitreten; Spieler auf verschiedenen Regionen finden einander nicht. Die Fehlermeldung „Game does not exist“ ist der Fehlercode *GameDoesNotExist* (32758) von **Photon** [3]. Sprachchat: von Photon Voice auf Dissonance umgestellt [2]. | Patchnotiz zur Sprache [2]; Regionen und Fehlermeldungen aus Spielerberichten [4]. | niedrig bis mittel: Photon für das Spiel selbst ist nicht offiziell bestätigt |
| **Valheim** (eigene Engine auf Unity, bis 10 Spieler) | Steam (Server-Browser, direkte IP). Für Crossplay (ab 2022) **PlayFab**: Der Server zeigt einen **Beitrittscode** (wird bei jedem Start neu erzeugt), der Verkehr läuft über ein Relay, **keine Portfreigabe nötig**. Nachteil laut Hostern: Relay kostet Latenz; fällt PlayFab aus, ist der Server unerreichbar. | Wiki zum Patch vom 27.09.2022, Hoster-Anleitungen [5][6]. | mittel |
| **Content Warning** (Landfall, Unity, 4 Spieler) | **Photon** als Netz; Freunde über Steam einladen. Landfall bat Modder, wenig Daten über Photon zu schicken, weil Landfall die Bandbreite bezahlt. | Beschreibung des Mod-Pakets MyceliumNetworking [7]; Mod „Self Sufficient“ tauscht die Photon-App-ID [8]. | mittel |
| **Deep Rock Galactic** (Unreal, 4 Spieler) | Ein Spieler hostet (keine dedizierten Server), Lobby über Steam. Spieler ohne Portfreigabe oder hinter CGNAT brauchen laut Gemeinschaft **Steam Datagram Relay (SDR)**. Crossplay nur zwischen Xbox und Microsoft Store. | Fan-Wiki „Multiplayer“ [9], Steam-Forum [10]. | niedrig bis mittel |

**Muster:**
- Alle fünf verlassen sich für Lobby, NAT und Relay auf eine **Plattform** (Steam, Photon, PlayFab). Keines verlangt eine Portfreigabe.
- Eingeladen wird über die Plattform (Steam-Freunde) oder mit einem **kurzen Code** (Valheim, Phasmophobia).
- Der Host ist Server (Listen-Server); über Relay kostet das Latenz, dafür bleibt die IP verborgen.
- Der Anbieter von Lobby und Relay ist immer **vertrauenswürdig gesetzt**. Ein Spiel bezahlt ihn (Photon, PlayFab) oder ist an seine Plattform gebunden (Steam).

## 3. Vergleich der Lösungen

Bewertet für LIMINAL: Listen-Server bis 4 Spieler, eigene C++20-Engine, Windows, **llvm-mingw** (Clang, MinGW-Laufzeit), Netz nur in `liminal_net.dll`. Sternchen (*) = nicht selbst geprüft, nur aus der Dokumentation.

### 3.1 Übersicht

| Lösung | Hinter NAT ohne Portfreigabe | Verschlüsselung / Authentisierung | Reife, Wartung, Lücken | Lizenz, Weitergabe | Kosten, Konto | Datenschutz | Passung (llvm-mingw, DLL, Architektur) |
|---|---|---|---|---|---|---|---|
| **Steam Networking Sockets** (Steamworks SDK) | ja: Steam Datagram Relay (SDR) über Valves Netz [11][12] | ja: „All traffic you receive is authenticated, encrypted, and rate-limited“ [12]; Identität = Steam-Konto | sehr hoch (CS2, Dota 2) | Steamworks SDK unter Valves Vertrag; nur zusammen mit dem Spiel auf Steam | 100 $ je App (Steam Direct, verrechenbar ab 1000 $ Umsatz) [13]; jedes Spiel und jeder Spieler braucht Steam | Valve sieht Verbindungsdaten; IPs bleiben zwischen Spielern verborgen | C-API (flat) wäre ladbar; **SDK-Download nur mit Steamworks-Konto** – nicht ohne Konto integrierbar |
| **Epic Online Services (EOS) P2P** | ja: NAT-Durchdringung, sonst Relays von Epic (laut Forenberichten, Doku nicht abrufbar*) [14][15] | Verbindung je Konto-ID (Connect-Schnittstelle); Verschlüsselung laut Doku* | hoch; 2020 fand ein Forscher Entwurfsschwächen: Client-Zugangsdaten im Programm, Kapern von Sitzungen, IP-Adressen über die Sitzungssuche [16] | EOS-SDK-Vertrag; SDK nur mit Epic-Entwicklerkonto | kostenlos [15]; Entwicklerkonto, Produkt-, Sandbox-, Deployment-IDs, Client-Zugangsdaten | Epic verarbeitet IPs und Konto-IDs; Datenschutzerklärung nötig | C-API als DLL; **ohne Konto nicht zu bekommen** |
| **Valve GameNetworkingSockets** (Open Source, v1.6.1 vom 03.10.2026) | teilweise: ICE (eigener Client, seit 1.6.0 „out of beta“), **TURN seit 1.6.0** [17][18]; Signalisierung und TURN-Server muss man selbst stellen; das SDR-Relay gibt es nur mit Steam [17] | ja: „AES-GCM-256 per packet, Curve25519 for key exchange“ [17]; **Authentisierung ohne Steam nur mit eigener Zertifizierungsstelle** (sonst verschlüsselt, aber ohne Identität) [19] | hoch (dieselbe Transportschicht wie Steam); aktiv | BSD-3-Clause | keine; eigener Signalisierungs- und TURN-Server | wie eigener Server | Abhängigkeit **Protobuf** (ab 22 mit Abseil), Krypto über OpenSSL, libsodium oder bcrypt; bekannter Hinweis „Link errors may occur with LLVM 10+“ [20]; MinGW-Fix erst in 1.6.1 [18] |
| **yojimbo / netcode** (Glenn Fiedler; netcode 1.4.8, yojimbo 1.14.0) | **nein**: für dedizierte Server; keine NAT-Durchdringung, kein Relay [21] | ja: libsodium; Beitritt nur mit **Connect-Token** aus einem eigenen Web-Backend, dessen privater Schlüssel nie im Client liegen darf [21] | gut gepflegt | BSD-3 | eigenes Backend nötig | wie eigener Server | passt nicht: Listen-Server hinter NAT ist nicht vorgesehen |
| **ENet + Absicherung** (Stand V13.2) | **nein** (Portfreigabe) | nur mit eigener Schicht (V13.2: eigener Handshake + AES-GCM) | ENet reif, aber ohne Sicherheit | MIT | keine | Host-IP muss weitergegeben werden | passt technisch, verstößt gegen „keine eigene Kryptografie“ und löst NAT nicht |
| **WebRTC-Datenkanäle: libdatachannel** (v0.24.6 vom 26.09.2026) mit **libjuice** (ICE, STUN, TURN; v1.7.4), **usrsctp**, **Mbed TLS 3.6 LTS** | **ja**: ICE mit STUN, **TURN-Relay als automatischer Rückfall** [22][23] | ja: **DTLS 1.2** (ECDHE, Vorwärtsgeheimnis) mit Zertifikats-Fingerabdruck aus der Signalisierung (RFC 8827) [22]; SCTP-Kanäle zuverlässig/unzuverlässig | aktiv (Releases 2026-09); in den Advisory-Datenbanken (OSV, GitHub) **keine bekannte Lücke** für die verwendeten Commits (Abschnitt 4); bekannt: ein Absturz durch ungeprüfte Eingaben in einer Fremdanwendung (CVE-2026-103936, Fehler des Aufrufers) [24] | libdatachannel und libjuice **MPL-2.0** (Dateiebene: unveränderte Dateien weitergeben, Quellort nennen), usrsctp BSD-3, Mbed TLS Apache-2.0, plog MIT | keine Lizenzkosten, kein Konto; ein Lobby- und TURN-Server muss irgendwo laufen | Lobby und TURN sieht IPs; Peers sehen einander (oder nur das Relay, Einstellung „nur Relay“) | C++17, reine Quellen; **libjuice bringt einen STUN/TURN-Server mit** [23], libdatachannel einen **WebSocket-Server und -Client** [22] → eigener Lobby-Server ohne weitere Bibliothek. MinGW: libjuice ausdrücklich, libdatachannel nicht in der CI (selbst geprüft, Abschnitt 6) |
| **WebRTC mit coturn** (TURN-Server, Linux) | wie oben, Relay über coturn | TURN-Anmeldung (REST-API mit Zeitlimit) | sehr verbreitet; **2026 zwei Lücken** (Speicherleck über HTTP-Umleitung CVE-2026-62959, CVSS 8.2; Absturz einer TCP-Verbindung CVE-2026-68552), behoben in 4.15.0; aktuell 4.18.0 [25][26] | BSD-3 | eigener Server | wie oben | für einen öffentlichen Linux-Server die erste Wahl als TURN; nicht als Teil des Spiels |
| **Photon Realtime / Fusion** (Exit Games) | ja: alles läuft über Photon-Server (Cloud-Relay) | Verschlüsselung optional*, eigene Authentisierung über einen Web-Dienst* | sehr verbreitet (Content Warning, vermutlich Phasmophobia) | proprietäres SDK | **20 CCU nur zur Entwicklung**, dann z. B. 95 $ einmalig für 100 CCU (12 Monate) oder 95 $/Monat für 500 CCU; Verkehr über Kontingent 0,05–0,10 $/GB [27]; Konto und App-ID | Photon (Exit Games, Hamburg) sieht allen Verkehr | C++-SDK für MSVC*; mit llvm-mingw fraglich; Konto nötig |
| **PlayFab Party** (Microsoft) | ja: P2P, sonst Relay [28] | ja: verschlüsselt, Relay mit DTLS [28] | hoch (Valheim-Crossplay [5]) | proprietär | nutzungsabhängig (Spielerminuten, Relay-Verkehr) [29]; Azure/PlayFab-Konto | Microsoft sieht Verbindungsdaten | SDK für MSVC/GDK*; Konto nötig |
| Spieler-VPN (ZeroTier, Tailscale) | ja | ja | reif | proprietär/BSL | Konto je Spieler | Dritter sieht Verkehrsdaten | **verworfen**: jeder Spieler müsste ein Programm installieren und ein Konto anlegen |

### 3.2 Was die Bewertung entschieden hat

1. **Ohne Konto integrierbar und testbar?** Steamworks SDK, EOS SDK, Photon und PlayFab bekommt man nur mit Konto. Konten darf diese Arbeit nicht anlegen. Bleiben: GameNetworkingSockets, libdatachannel/libjuice, netcode/yojimbo, ENet.
2. **Hinter NAT ohne Portfreigabe?** netcode/yojimbo und ENet nicht. Bleiben: GameNetworkingSockets und libdatachannel.
3. **Authentisierung ohne Steam?**
   - GameNetworkingSockets: ohne eigene Zertifizierungsstelle nur verschlüsselt, nicht authentisiert. Ein Angreifer auf dem Weg (oder der Betreiber des Relays) könnte sich dazwischen schalten. Eine eigene Stelle mit Zertifikaten je Sitzung hieße: ein eigenes Protokoll um die Bibliothek herum.
   - WebRTC: DTLS prüft den Zertifikats-Fingerabdruck aus der Signalisierung (Standard, RFC 8827). Der Fingerabdruck lässt sich zusätzlich im Beitrittscode festhalten („Pinning“, Abschnitt 5.2).
4. **Bau mit llvm-mingw:**
   - GameNetworkingSockets braucht Protobuf (mit Abseil). Die Dokumentation warnt vor Linkfehlern mit LLVM ab Version 10; der MinGW-Fix kam erst am 03.10.2026.
   - libdatachannel braucht nur reine C/C++-Quellen; Mbed TLS baut ohne Perl/Python aus dem Release-Archiv.
5. **Relay ohne fremde Dienste:**
   - libjuice enthält einen TURN-Server. Damit gibt es einen startbaren Relay- und Lobby-Server ohne zweite Bibliothek.
   - Für GameNetworkingSockets bräuchte man zusätzlich coturn (Linux) oder einen TURN-Anbieter.

## 4. Sicherheitslage der gewählten Bausteine (geprüft am 27.10.2026)

| Baustein | Version / Commit | Abfrage | Ergebnis |
|---|---|---|---|
| libdatachannel | v0.24.6, `6b1e2e62` (26.09.2026) | OSV (Commit-Abfrage), GitHub Security Advisories, Websuche | keine Einträge. CVE-2026-103936 betrifft `@libp2p/webrtc`: Es reichte ungeprüfte Werte aus STUN-Paketen an libdatachannel weiter; eine Ausnahme beendete den Prozess. **Folge für LIMINAL:** Jeder Aufruf der Bibliothek steht in `try/catch`, Werte aus dem Netz werden vorher auf Länge und Zeichen geprüft. |
| libjuice | v1.7.4, `b89c792e` (26.09.2026) | OSV, GitHub Advisories, Websuche | keine Einträge |
| usrsctp | `fec583d5` (26.06.2025, von libdatachannel 0.24.6 festgelegt) | OSV, Websuche | keine Einträge für diesen Stand. Ältere Lücken: CVE-2019-20503 (Lesen außerhalb des Puffers, behoben 2019) [30]. Der Code ist groß und alt; er läuft nur **nach** DTLS, also nur mit Gegenstellen, die die Signalisierung durchlaufen haben. |
| Mbed TLS | 3.6.7 LTS (07.07.2026), Archiv-SHA-256 laut Release `a7e8bcbe…` | Release-Prüfsumme verglichen; OSS-Fuzz-Projekt | aktuelle LTS-Linie (3.6). libdatachannel verlangt Mbed TLS 3 (`find_package(MbedTLS 3)`), deshalb nicht 4.x |
| plog | `94899e0b` (Protokoll-Hilfe) | OSV | keine Einträge |
| nlohmann/json | v3.12.0 (`55f93686`) | OSV | keine Einträge; parst die Nachrichten der Lobby |

**Eigene Beobachtung beim Lesen des Codes:**
- libdatachannel setzt mit Mbed TLS für DTLS `MBEDTLS_SSL_VERIFY_OPTIONAL`. Bei OpenSSL ist das Gegenstück `SSL_VERIFY_FAIL_IF_NO_PEER_CERT`.
- Folge: Ein Gegenüber ohne Zertifikat würde nicht abgewiesen. Ein falscher Fingerabdruck bricht ab, weil Mbed TLS Fehler des Prüf-Rückrufs als fatal behandelt.
- **Gegenmaßnahme in LIMINAL:** Nach dem Öffnen der Kanäle verlangt `liminal_net.dll`, dass `remoteFingerprint()` gesetzt ist und zum Fingerabdruck aus der Beschreibung passt; sonst trennt sie.
- Außerdem prüft die TLS-Verbindung der WebSocket-Lobby mit Mbed TLS **kein** Zertifikat (`MBEDTLS_SSL_VERIFY_NONE`), solange man nicht ausdrücklich prüfen lässt. LIMINAL schaltet die Prüfung ein und übergibt die Stammzertifikate von Windows oder ein festgelegtes Zertifikat (Abschnitt 5.2).
- **Status:** Beides ist nicht als Fehler an die Entwickler gemeldet (kein Konto). Es steht als offener Punkt in `MULTIPLAYER_BEDROHUNGSMODELL.md`.

## 5. Ergebnis

### 5.1 Wahl

**WebRTC-Datenkanäle mit libdatachannel 0.24.6 (ICE/STUN/TURN über libjuice 1.7.4, DTLS über Mbed TLS 3.6.7 LTS, SCTP über usrsctp) und ein kleiner eigener Lobby-Server (Beitrittscodes, Signalisierung) mit eingebautem STUN/TURN-Relay aus libjuice.**

- Beitritt per **Code** statt IP. NAT-Durchdringung über ICE; geht kein direkter Weg, **automatisch über das TURN-Relay**.
- Verschlüsselung und Authentisierung macht die Bibliothek (DTLS). Der eigene Handshake und die eigene AES-Schicht aus V13.2 sind **entfernt**, ENet ebenso.
- Eigener Code bleibt: Spiellogik, Autorität, Prüfung jeder Nachricht, die Lobby (Vermittlung von Codes und Signalisierung, siehe 5.3) und eine kleine Rahmung in der DLL (Hallo mit Version und Geheimteil des Codes, Ping für die Verbindungsqualität).
- Alles Netz bleibt in `liminal_net.dll`. Das Spiel ohne Koop enthält weiterhin keinen Netzwerkcode.

### 5.2 Warum nicht Steam/EOS, und was das für die Veröffentlichung heißt

- Steam (SDR) ist für ein Spiel **auf Steam** die bequemste Lösung: kein eigener Server, Relay weltweit, IPs verborgen. Dasselbe gilt für EOS mit Epic-Konto.
- Beide verlangen Konten und SDKs, die nur mit Konto zu bekommen sind. LIMINAL ist (noch) nicht auf Steam.
- Die DLL-Schnittstelle (`ln_*`) ist so geschnitten, dass sich später ein zweiter Transport (Steam Networking Sockets) dahinter setzen lässt: Host öffnen, Code, beitreten, senden, Ereignisse. Am Spiel ändert sich dafür nichts.

### 5.3 Warum eine eigene Lobby

- Für die Vermittlung von Codes gibt es keinen fertigen, kontofreien Dienst, den man weitergeben dürfte.
- Öffentliche „Signalisierungsserver“ aus Beispielen sind zum Testen gedacht, nicht zum Betrieb.
- **Umfang der Lobby, bewusst klein:**
  - Raum anlegen, beitreten, Beschreibungen und Kandidaten weiterreichen, sperren.
  - Feste Höchstgrößen, Raten und Zeitlimits.
  - **Keine** Kryptografie: TLS macht libdatachannel/Mbed TLS, die Echtheit der Gegenstelle prüft DTLS.
- **Der Lobby-Server sieht nur:**
  - die IP-Adressen,
  - die Raumnummer (nicht das Geheimteil des Codes),
  - Spielernamen,
  - die Verbindungsbeschreibungen (SDP).
  Den Spielverkehr sieht er nicht. Das TURN-Relay leitet nur verschlüsselte Pakete weiter.

### 5.4 Was nicht gelöst ist

- Netze, die **jeden UDP-Verkehr sperren** (manche Firmen- und Schulnetze): libjuice spricht TURN nur über UDP. Ein Relay über TCP oder TLS (Port 443) ginge nur mit einem anderen TURN-Server (coturn) und libnice; nicht umgesetzt.
- Für weltweites Spiel muss **jemand den Lobby-Server öffentlich betreiben** (Herausgeber des Spiels oder eine Spielergruppe). Das ist der eine Eintrag in der Konfigurationsdatei (`MULTIPLAYER_SETUP.md`).

## Quellen (abgerufen am 27.10.2026)

1. EvaisaDev, LethalCompanyUnityTemplate – https://github.com/EvaisaDev/LethalCompanyUnityTemplate
2. Phasmophobia-Patch, Umstellung auf Dissonance – https://www.gfinityesports.com/phasmophobia/phasmophobia-gets-a-new-update-changing-voice-chat/
3. Photon-Forum, Fehlercode 32758 „Game does not exist“ – https://forum.photonengine.com/discussion/19492/error-joining-room-code-32758-game-does-not-exist-2-unity-projects-with-same-photon-appid
4. Phasmophobia-Fehler „game does not exist / server version mismatch“ – https://www.gameskinny.com/1t4eu/phasmophobia-game-does-not-exist-error-fix-server-version-mismatch
5. Valheim-Wiki, Patch vom 27.09.2022 – https://valheim-wiki.mave.me/September_27,_2022_Patch
6. Valheim Crossplay (Hoster) – https://www.gameserverkings.com/knowledge-base/valheim/is-valheim-crossplay.md und https://xgamingserver.com/docs/valheim/crossplay-setup
7. MyceliumNetworking für Content Warning – https://www.nuget.org/packages/RugbugRedfern.MyceliumNetworking.CW
8. Mod „Self Sufficient“ (Photon-App-ID) – https://thunderstore.io/c/content-warning/p/Computery/Self_Sufficient/
9. Deep Rock Galactic Wiki, Multiplayer – https://deeprockgalactic.wiki.gg/wiki/Multiplayer
10. Steam-Forum Deep Rock Galactic (SDR, Portfreigabe) – https://steamcommunity.com/app/548430/discussions/1/3203748171375296001
11. Steamworks, Networking – https://partner.steamgames.com/doc/features/multiplayer/networking
12. Steamworks, Steam Datagram Relay – https://partner.steamgames.com/doc/features/multiplayer/steamdatagramrelay
13. Steam Direct – https://partner.steamgames.com/steamdirect
14. Edgegap zu EOS-Relays – https://edgegap.com/blog/can-epic-online-services-eos-relays-allow-for-dedicated-server-or-authoritative-server
15. Epic Online Services, Multiplayer – https://onlineservices.epicgames.com/multiplayer
16. B. Demirkapi, „Insecure by Design, Epic Games Peer-to-Peer Multiplayer Service“ (17.12.2020) – https://billdemirkapi.me/insecure-by-design-epic-games-p2p-multiplayer-services/
17. GameNetworkingSockets, README – https://github.com/ValveSoftware/GameNetworkingSockets
18. GameNetworkingSockets, Releases (1.5.0–1.6.1) – https://github.com/ValveSoftware/GameNetworkingSockets/releases
19. GameNetworkingSockets, README_P2P – https://github.com/ValveSoftware/GameNetworkingSockets/blob/master/README_P2P.md
20. GameNetworkingSockets, BUILDING – https://github.com/ValveSoftware/GameNetworkingSockets/blob/master/BUILDING.md
21. netcode (mas-bandwidth) – https://github.com/mas-bandwidth/netcode
22. libdatachannel – https://github.com/paullouisageneau/libdatachannel (Quellen v0.24.6 gelesen: `tlstransport.cpp`, `verifiedtlstransport.cpp`, `dtlstransport.cpp`, `peerconnection.cpp`)
23. libjuice – https://github.com/paullouisageneau/libjuice
24. CVE-2026-103936 – https://www.mend.io/vulnerability-database/CVE-2026-103936/
25. CVE-2026-62959 (coturn) – https://mondoo.com/vulnerability-intelligence/vulnerability/CVE-2026-62959
26. CVE-2026-68552 (coturn) – https://app.opencve.io/cve/CVE-2026-68552
27. Photon, Preise – https://www.photonengine.com/pricing
28. PlayFab Party, Funktionen – https://learn.microsoft.com/en-za/xbox/playfab/multiplayer/networking/party-features
29. PlayFab Party, Abrechnung – https://learn.microsoft.com/hu-hu/gaming/playfab/features/multiplayer/networking/pricing
30. CVE-2019-20503 (usrsctp) – https://cve.circl.lu/cve/CVE-2019-20503
31. Cloudflare Realtime TURN, FAQ und Preise – https://developers.cloudflare.com/realtime/turn/faq/ und https://developers.cloudflare.com/realtime/pricing/
32. coturn – https://github.com/coturn/coturn
33. Mbed TLS 3.6.7 – https://github.com/Mbed-TLS/mbedtls/releases/tag/mbedtls-3.6.7
