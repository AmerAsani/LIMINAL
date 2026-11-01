# LIMINAL – Koop einrichten: Lobby, Relay, Veröffentlichung

Stand: 27.10.2026 (V13.3). Technik: `MULTIPLAYER_ARCHITEKTUR.md`, Wahl der Lösung: `MULTIPLAYER_RECHERCHE.md`, Risiken: `MULTIPLAYER_BEDROHUNGSMODELL.md`.

**Kurz:** Damit Spieler auf der ganzen Welt ohne Router-Einstellungen zusammenfinden, braucht es **einen** öffentlich erreichbaren Lobby-Server. Er ist im Spiel enthalten: `liminal_lobby.exe`. Es gibt kein Konto, keinen App-Schlüssel und keinen fremden Dienst, der freigeschaltet werden müsste. Im Spiel fehlt dann nur noch **ein Eintrag** in `koop.json`: die Adresse dieser Lobby.

Diese Arbeit hat **keine** Konten angelegt, keine Zugangsdaten eingegeben und keinen Server gemietet. Getestet wurde lokal auf einem Rechner (`MULTIPLAYER_TESTS.md`, Abschnitt 7).

---

## 1. Was wofür

| Teil | Aufgabe | Wer betreibt es |
|---|---|---|
| `liminal_net.dll` | Verbindung im Spiel (WebRTC: ICE/STUN/TURN, DTLS) | jedes Spiel (lädt die DLL erst beim Hosten/Beitreten) |
| `liminal_lobby.exe` | **Lobby:** Beitrittscodes, Austausch der Verbindungsdaten (WebSocket, TCP). **STUN/TURN:** eigene öffentliche Adresse erkennen; Relay, wenn kein direkter Weg geht (UDP) | ein Rechner mit öffentlicher Adresse (Herausgeber oder Spielergruppe) |
| `koop.json` | Adresse der Lobby (und optional ihr Zertifikat) | liegt neben dem Spiel (vom Herausgeber) oder im Nutzerordner (eigene Lobby) |

Ohne Lobby geht nur **„Direkt (LAN/IP)“**: Der Host startet eine kleine Lobby in seinem eigenen Spiel. Im selben Netz genügt das. Übers Internet braucht der Host dann eine Portfreigabe (TCP und UDP 27500).

---

## 2. Sofort ausprobieren (ein Rechner oder ein LAN, ohne Konto)

1. **Lobby starten** (Eingabeaufforderung im Spielordner):
   ```
   liminal_lobby.exe --port 27515 --turn-port 3478
   ```
   Ausgabe: `LIMINAL Lobby laeuft: ws://<alle>:27515 | STUN/TURN UDP 3478`. Windows fragt beim ersten Start, ob das Programm Verbindungen annehmen darf. Für Tests im LAN: „Private Netzwerke“ erlauben.
2. **`koop.json`** in den Spielordner legen (oder nach `%LOCALAPPDATA%\LIMINAL\koop.json`):
   ```json
   { "lobby": "ws://192.168.0.10:27515" }
   ```
   `192.168.0.10` ist die Adresse des Rechners mit der Lobby (`ipconfig`). Auf demselben Rechner: `ws://127.0.0.1:27515`.
3. **Spiel → Koop → Hosten**, Verbindung „Über die Lobby“. Im Pausenmenü unter **Mitspieler** steht der **Beitrittscode** (z. B. `K7QF-M2XA-9FJT`); **Strg+C** kopiert ihn.
4. **Zweites Spiel → Koop → Beitreten:** Code eingeben (Strg+V geht), „Über die Lobby“, **Beitreten**.

`ws://` ist **unverschlüsselt** zur Lobby. Der Spielverkehr selbst ist trotzdem verschlüsselt (DTLS). Ein Angreifer im selben Netz könnte aber die Verbindungsdaten austauschen und sich dazwischenschalten. Mit dem **langen Code** (24 Zeichen, Pausenmenü) prüft der Gast den Host trotzdem. Für das Internet gilt: `wss://` (Abschnitt 3.3).

---

## 3. Öffentliche Lobby (weltweit, ohne Router-Einstellungen bei den Spielern)

### 3.1 Rechner

- Ein kleiner Server mit **öffentlicher IPv4-Adresse**:
  - Mietserver (VPS) mit Windows; das Programm ist bisher nur für Windows gebaut und geprüft.
  - oder ein eigener Rechner mit Portfreigabe – dann muss nur **dieser eine** Rechner freigeben, nicht jeder Spieler.
- Last:
  - Die Lobby selbst braucht kaum etwas (wenige kB je Beitritt).
  - Das **Relay** trägt den ganzen Spielverkehr der Spieler, die keinen direkten Weg finden.
  - Gemessen: etwa 5–8 kB/s je Gast und Richtung im Spiel; über das Relay doppelt (hin zum Relay, weiter zum Host). Grob **50–60 MB je Gast und Spielstunde**, wenn er über das Relay läuft.
  - Wie viele Verbindungen in der Praxis das Relay brauchen, ist **nicht gemessen** (dafür bräuchte es echte Spieler in echten Netzen). Für die Planung: ein Teil der Verbindungen, nicht alle.

### 3.2 Starten

```
liminal_lobby.exe --port 27515 --turn-port 3478 --public-host lobby.example.org ^
                  --relay-ports 50000-50199 --tls-cert lobby.crt --tls-key lobby.key
```

| Einstellung | Bedeutung |
|---|---|
| `--public-host` | Name oder IP, unter der Spieler STUN/TURN erreichen (sonst derselbe Name wie die Lobby) |
| `--external-ip` | öffentliche IP, wenn der Server selbst hinter NAT steht (Relay-Adressen) |
| `--public-turn-port` | wenn der TURN-Port von außen anders heißt (Weiterleitung) |
| `--relay-ports` | Bereich der UDP-Ports für Relay-Zuteilungen; genau diesen Bereich in der Firewall öffnen |
| `--max-rooms` | Obergrenze gleichzeitiger Räume (Standard 256) |
| `--verbose` | Ereignisse ausgeben (ohne IP-Adressen) |

**Firewall des Servers:**
- TCP 27515 (Lobby);
- UDP 3478 (STUN/TURN);
- UDP 50000–50199 (Relay).

Mehr nicht.

**Dauerbetrieb:**
- Als Dienst einrichten, z. B. mit der Aufgabenplanung „Beim Start“ oder einem Dienst-Wrapper.
- Die Lobby speichert nichts auf der Platte; ein Neustart schließt nur die offenen Räume. Laufende Spiele (direkter Weg) laufen weiter, Spiele über das Relay brechen ab.

### 3.3 TLS (wss://) – empfohlen

Das Spiel verbindet sich mit `wss://` über **WinHTTP** von Windows. Das Zertifikat prüft Windows (Schannel), samt Zertifizierungsstelle, Name und Sperrlisten. Zwei Wege:

1. **Zertifikat einer öffentlichen Zertifizierungsstelle** (z. B. Let's Encrypt für `lobby.example.org`): als PEM-Dateien an `--tls-cert`/`--tls-key`. In `koop.json` genügt `"lobby": "wss://lobby.example.org:27515"`.
2. **Eigenes Zertifikat (selbst signiert) mit Pinning:**
   - Das Zertifikat liegt dem Spiel bei, oder sein SHA-256-Fingerabdruck steht in `koop.json`:
     ```json
     { "lobby": "wss://203.0.113.7:27515", "lobby_cert": "lobby.crt" }
     ```
     `lobby_cert` ist entweder eine PEM-Datei neben `koop.json` oder der Fingerabdruck `"AB:CD:…"` (95 Zeichen).
   - Dann zählt **nur** dieser Fingerabdruck; Name, Aussteller und Datum prüft das Spiel nicht.
   - Geprüft wird, bevor die erste Lobby-Nachricht gesendet wird.

Ohne gültiges Zertifikat meldet das Spiel „Zertifikat passt nicht“ und sendet nichts an die Lobby.

### 3.4 Eintrag im Spiel

`koop.json` (Muster: `koop.example.json` im Spielordner):

| Feld | Pflicht | Bedeutung |
|---|---|---|
| `lobby` | **ja** (der fehlende Eintrag) | `wss://name:port` der Lobby |
| `lobby_cert` | nein | PEM-Datei oder Fingerabdruck der Lobby (Abschnitt 3.3) |
| `ice_servers` | nein | weitere Server, z. B. `["turn:benutzer:passwort@turn.example.org:3478"]` (TURN eines Anbieters, Abschnitt 4) |
| `relay_only` | nein | `true`: immer nur über das Relay (die eigene IP bleibt Mitspielern verborgen; mehr Verzögerung). Im Menü auch je Spiel wählbar |
| `direct_port` | nein | Port für „Direkt“ (Standard 27500, TCP und UDP) |

**Reihenfolge beim Lesen:**
1. `koop.json` neben `LIMINAL.exe` (vom Herausgeber);
2. dann `%LOCALAPPDATA%\LIMINAL\koop.json` (vom Spieler). Die zweite überschreibt einzelne Felder.

---

## 4. Kosten

| Posten | Kosten | Anmerkung |
|---|---|---|
| Bibliotheken | keine | MPL-2.0, BSD, MIT, Apache-2.0 (`LICENSES.md`, Abschnitt 7) |
| Konten, App-IDs | keine | nicht nötig |
| Server für Lobby und Relay | Miete eines kleinen Servers (Preise je Anbieter; hier nicht ermittelt) oder ein eigener Rechner mit Portfreigabe | Verkehr des Relays beachten (Abschnitt 3.1) |
| Alternativ TURN eines Anbieters | z. B. Cloudflare Realtime TURN: 0,05 $/GB, die ersten 1 000 GB im Monat frei (Stand 27.10.2026) | Anbieter-Konto nötig. Dessen Zugangsdaten sind in der Regel kurzlebig und kommen über eine Programmierschnittstelle des Anbieters. Die Lobby kann solche Zugangsdaten heute **nicht** abholen; feste Zugangsdaten über `ice_servers` gingen, sind aber für eine Veröffentlichung nicht zu empfehlen (jeder Spieler sähe sie) |
| Domain und Zertifikat | Domain jährlich; Let's Encrypt kostenlos | für `wss://` mit öffentlicher Zertifizierungsstelle |

---

## 5. Datenschutz (Folgen des Betriebs)

**Was wohin geht:**

| Daten | Wer sieht sie | Wie lange |
|---|---|---|
| IP-Adresse jedes Spielers | Betreiber der Lobby (für Raten und Sperren) | nur im Arbeitsspeicher. Für Raten 1–10 min, Sperren eines Raums bis zum Ende des Raums (höchstens 12 h). Nicht in Protokollen, nicht auf der Platte |
| IP-Adresse beim Relay | Betreiber des Relays (der TURN-Server sieht Absender und Ziel) | im Arbeitsspeicher, solange die Zuteilung lebt |
| IP-Adresse unter Spielern | Host und Gast sehen einander, wenn ein direkter Weg gefunden wird. Gäste sehen einander **nicht** | während der Sitzung. Mit „Eigene IP verbergen“ (nur Relay) sieht das Gegenüber nur die Adresse des Relays |
| Spielername (bis 16 Zeichen) | Lobby (beim Beitritt), Host und Mitspieler | Lobby: nur zum Weiterreichen, nicht gespeichert |
| Raumnummer (6 Zeichen) | Lobby | solange der Raum offen ist |
| Geheimteil des Codes | nur Host und Gast (im verschlüsselten Kanal) | – |
| Spielverkehr | nur die Spieler (DTLS); das Relay leitet nur verschlüsselte Pakete weiter | – |

**Für eine Veröffentlichung:**
- **IP-Adressen sind personenbezogene Daten** (DSGVO). Der Betreiber von Lobby und Relay ist dafür verantwortlich.
- Eine **Datenschutzerklärung** ist nötig. Sie nennt Zweck (Vermittlung, Missbrauchsschutz), Rechtsgrundlage (in der Regel berechtigtes Interesse bzw. Vertrag), Speicherdauer (oben), Empfänger (Mietserver-Anbieter) und Ort des Servers.
- Mit einem **Mietserver** ist ein **Vertrag zur Auftragsverarbeitung** mit dem Anbieter nötig.
- Mit einem **TURN-Anbieter** kommt ein weiterer Empfänger hinzu (ggf. außerhalb der EU).
- Den Hinweis „Die Lobby sieht deine IP-Adresse“ zeigt das Spiel im Koop-Menü.
- Das ist **keine Rechtsberatung**; vor der Veröffentlichung prüfen lassen.

---

## 6. Hinweise für die Veröffentlichung

1. **Paket** (`tools/package.ps1`):
   - `liminal_net.dll`, `liminal_lobby.exe`, `koop.example.json`;
   - Lizenztexte der Netzbibliotheken in `licenses/`.
2. **`koop.json` mit der Adresse der eigenen Lobby** neben das Spiel legen. Ohne den Eintrag zeigt das Menü „Keine Lobby eingetragen“; „Direkt (LAN/IP)“ geht trotzdem.
3. **MPL-2.0** (libdatachannel, libjuice):
   - Die Quellen sind unverändert im Quellcode des Spiels (`third_party/`).
   - Wer nur das Spiel weitergibt, nennt, wo es den Quellcode der MPL-Teile gibt. Das steht in `licenses/THIRD_PARTY_NOTICES.txt`.
   - Wer Dateien dieser Bibliotheken ändert, muss die geänderten Dateien offenlegen.
4. **Lobby aktuell halten:**
   - Sie ist ein Dienst im Internet.
   - Neue Versionen von libdatachannel, libjuice und Mbed TLS verfolgen (GitHub-Releases, Sicherheitshinweise, OSV) und die Lobby mit dem Spiel neu bauen. Das Vorgehen der Prüfung beschreibt `MULTIPLAYER_TESTS.md`, Abschnitt 7.5.
5. **Versionen:**
   - Lobby-Protokoll v1, DLL-Protokoll v3, Spielprotokoll v3; dazu die Prüfsumme der Spieldaten.
   - Ein Spiel mit anderer Version bekommt eine klare Absage („andere Version“). Lobby und Spiel bei Änderungen gemeinsam ausliefern.
6. **Steam später:**
   - Erscheint LIMINAL auf Steam, wäre Steam Networking Sockets (Relay von Valve, keine eigene Lobby nötig) ein zweiter Weg hinter derselben DLL-Schnittstelle (`MULTIPLAYER_RECHERCHE.md`, Abschnitt 5.2).
   - Dafür braucht es das Steamworks-Konto des Herausgebers und die App-ID; nichts davon liegt im Repository.
7. **Keine Geheimnisse im Repository:**
   - Zertifikate und Schlüssel der Lobby (`--tls-key`) gehören auf den Server, nicht in Git.
   - Zugangsdaten von TURN-Anbietern ebenso.

---

## 7. Fehlerbilder

| Meldung im Spiel | Ursache | Abhilfe |
|---|---|---|
| Keine Lobby eingetragen | `koop.json` fehlt oder ohne `lobby` | Abschnitt 3.4, oder „Direkt (LAN/IP)“ |
| Lobby nicht erreichbar | Adresse falsch, Server aus, TCP-Port gesperrt | Server, Firewall (TCP 27515), Adresse prüfen |
| Zertifikat passt nicht | `wss://` mit unbekanntem/abgelaufenem Zertifikat oder falscher Fingerabdruck; beim Gast: Prüfteil des langen Codes falsch | Zertifikat (Abschnitt 3.3), Code neu kopieren |
| Code unbekannt oder Sitzung beendet | Tippfehler, Host hat beendet, Lobby neu gestartet | Code neu vom Host |
| falscher Code | Raumnummer stimmt, Geheimteil nicht | Code neu kopieren. Nach 5 Fehlversuchen ist die Adresse 10 min gesperrt |
| kein Verbindungsweg (Firewall/NAT) | weder direkt noch über das Relay; z. B. UDP ganz gesperrt (manche Firmen- und Schulnetze) oder Relay-Ports zu | Relay-Ports öffnen; in Netzen ohne UDP geht es nicht (`MULTIPLAYER_RECHERCHE.md`, Abschnitt 5.4) |
| Host nimmt gerade andere Gäste auf | mehrere Beitritte gleichzeitig | kurz warten, erneut |
