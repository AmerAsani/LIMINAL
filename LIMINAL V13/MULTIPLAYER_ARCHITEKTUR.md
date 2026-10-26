# LIMINAL – Mehrspieler-Architektur (Koop, Beta)

Stand: 26.10.2026, V13.2. Dieses Dokument entstand vor der ersten Netzwerkzeile des Spiels. Bindend sind die Regeln aus `MULTIPLAYER_BEDROHUNGSMODELL.md` (Abschnitt 6); dieses Dokument legt fest, wie sie umgesetzt werden. Ergebnisse der Prüfungen: `MULTIPLAYER_TESTS.md`, Entscheidung über die Freigabe: `MULTIPLAYER_ENTSCHEIDUNG.md`.

Nichts hier behauptet, der Mehrspielermodus sei „vollkommen sicher“. Ziel ist ein kleines, bekanntes und geprüftes Restrisiko.

## 1. Rahmen

- Koop für 2–4 Spieler auf derselben Ebene, gemeinsame Reise durch die zehn Ebenen.
- Ein Spieler hostet, die anderen treten bei (LAN oder Internet mit Portfreigabe).
- Keine Konten, keine fremden Server, keine Vermittlung, kein Datenabfluss an Dritte.

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

**Entscheidung: ENet 1.3.18** (MIT, Lee Salzman; Quelle: github.com/lsalzman/enet, Commit `2662c0de09e36f2a2030ccc2c528a3e4c9e8138a`, unverändert in `third_party/enet`).
- Bewährte UDP-Bibliothek seit 2002, in vielen Spielen im Einsatz.
- Etwa 4000 Zeilen C ohne weitere Abhängigkeiten.
- Bringt zuverlässige und unzuverlässige, geordnete Kanäle, Verbindungsverwaltung, Fragmentierung und Bandbreitenbegrenzung mit.

| Bewertet | Lizenz | Abhängigkeiten | Verschlüsselung | Urteil |
|---|---|---|---|---|
| ENet | MIT | keine | keine (siehe 5) | gewählt: klein, bewährt, gepflegt |
| GameNetworkingSockets (Valve) | BSD-3 | Protobuf, OpenSSL oder libsodium | ja | zu schwer für den Umfang |
| yojimbo / netcode | BSD-3 | libsodium, mbedTLS | ja | gut, aber zwei Kryptobibliotheken mehr |
| eigene Zuverlässigkeit auf Winsock | – | – | – | verworfen (fehleranfällig) |

## 5. Verschlüsselung und Beitritt

Das Bedrohungsmodell verlangt authentisierte Verschlüsselung nach dem Handshake (T4) und verbietet eigene Kryptografie (Regel 7).

**Entscheidung:** Windows-Kryptografie (CNG, `bcrypt.dll`), Teil von Windows, keine zusätzliche Bibliothek.
- **Beitrittsschlüssel:** 128 zufällige Bit aus `BCryptGenRandom`. Der Host zeigt ihn als 26 Zeichen an (Base32, in Fünfergruppen); kopierbar, zusammen mit Adresse und Port als eine Zeile.
- **Sitzungsschlüssel je Verbindung:** HKDF-SHA256 (HMAC aus CNG) über den Beitrittsschlüssel und je 16 zufällige Byte von Host und Gast. Getrennte Schlüssel je Richtung.
- **Jede Nachricht nach dem Handshake:** AES-256-GCM (CNG). Die Nonce besteht aus Richtung und 64-Bit-Zähler; der Zähler dient zugleich als Schutz vor Wiederholung (Fenster `SeqWindow` aus `src/net`).
- **Handshake:** Der Gast beweist den Schlüssel, ohne ihn zu senden (HMAC über beide Zufallswerte). Falsche Beweise zählen; nach 5 Fehlversuchen wird die Adresse 60 s gesperrt.
- **Gewollte Grenze:** ENet selbst (Verbindungsaufbau, Bestätigungen) bleibt unverschlüsselt. Wer auf dem Weg sitzt, kann Verbindungen stören, aber Spielnachrichten weder lesen noch fälschen. Kein Vorwärtsgeheimnis: Wer den Beitrittsschlüssel später erfährt und den Verkehr aufgezeichnet hat, kann ihn lesen. Der Schlüssel gilt nur für eine Sitzung.

## 6. Prozess- und Bibliotheksgrenze (Regel 6)

- Netzwerk, ENet und Verschlüsselung liegen in **`liminal_net.dll`**. Das Spiel lädt sie mit `LoadLibrary` erst, wenn man „Spiel hosten“ oder „Beitreten“ wählt.
- `LiminalGame.exe` importiert keine Netzwerkbibliothek (`ws2_32`); `tools/check_no_network.py` prüft das weiter.
- Ohne Mehrspieler öffnet das Spiel keinen Port.
- Schnittstelle der DLL: wenige C-Funktionen (Host/Client öffnen und schließen, Paket senden, Ereignisse abholen, Messwerte). Sie liefert nur entschlüsselte, geprüfte Nutzdaten fester Höchstgröße.
- Das Spielprotokoll prüft `src/net` (Prüfschicht aus V13, erweitert): Typ, Länge, Bereich, Rate, Folge, Rolle.

## 7. Verbindung, NAT, Firewall, Datenschutz

- Direktverbindung: IP und Port (Standard 27500/UDP) plus Beitrittsschlüssel. Im LAN genügt die angezeigte Adresse.
- Übers Internet braucht der Host eine Portfreigabe im Router. Es gibt kein NAT-Traversal über fremde Server; das ist bewusst so, denn jeder Vermittlungsserver wäre ein Dritter mit Einblick.
- Beim ersten Hosten fragt die Windows-Firewall, ob das Spiel Verbindungen annehmen darf. Der Dialog wird vorher im Menü angekündigt („Windows fragt gleich …“); ohne Zustimmung geht nur ein Spiel auf demselben Rechner.
- **Daten im Netz:** Spielername (höchstens 16 Zeichen), Spielzustand. Gäste erfahren keine IP-Adressen anderer Gäste, nur den Spielernamen.
- Keine Protokollzeile mit Pfaden geht ins Netz. Protokolle bleiben lokal und enthalten nur die Adresse des Hosts bzw. der Gäste.

## 8. Welt und Synchronisierung

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
  - die Wesen in der Nähe (Art, Ort, Richtung, Eingaben ihrer Bewegung)
  - letzte verarbeitete Eingabe je Gast

  Höchstens 1200 Byte je Paket.
- **Bewegung:**
  - **Vorhersage:** Der Gast bewegt sich sofort mit derselben Physik wie der Host.
  - **Korrektur:** Mit jeder Momentaufnahme setzt der Gast den Stand des Hosts ein und rechnet die noch unbestätigten Eingaben nach. Kleine Fehler gleichen sich weich aus, große springen.
- **Andere Spieler und Wesen:** 100 ms in der Vergangenheit zwischen zwei Momentaufnahmen interpoliert.
- **Körper der Mitspieler:** das vorhandene Spielermodell mit der Lampenhaltung von außen (B-001). Ihr Lichtkegel ist ein eigenes Licht im Renderer.
- **Wesen:** KI nur auf dem Host. Gäste zeigen „Puppen“: Ort, Richtung und Eingaben der Bewegung kommen vom Host, die Bewegung (Gang, Atem, Licht) rechnet der Gast mit derselben Animation. Die Wesen wählen als Ziel den nächsten Spieler, den sie wahrnehmen. Keine Erscheinung im Blickfeld irgendeines Spielers.

## 9. Spielregeln im Koop

- **Ebenenwechsel:** Wer einen Ausgang betritt, ist „bereit“. Alle sehen „Ausgang gefunden – 1/3 bereit“. Sind alle Lebenden bereit, wechselt die Gruppe. Nach 60 s ohne alle zieht der Host die Übrigen mit (Ankündigung ab 15 s). Spieler auf verschiedenen Ebenen gibt es nicht.
- **Tod:** Der Gestorbene sieht einem lebenden Mitspieler zu (Wechsel mit der Maus). Er kommt nach 20 s am letzten Sicherungspunkt zurück, solange jemand lebt. Sterben alle, beginnt die Gruppe am Sicherungspunkt neu (wie im Einzelspiel).
- **Vorräte:** Die Zahl der Funde wächst mit der Spielerzahl (Faktor 1 + 0,6 × (n − 1)). Jeder hat ein eigenes Inventar; man kann Dinge ablegen, damit andere sie aufnehmen.
- **Verstand und Halluzinationen:** Den Verstand rechnet der Host. Halluzinationen sieht nur der Betroffene.
- **Speichern:** nur der Host, in einem eigenen Koop-Spielstand (Werte je Spielername), nie über den Einzelspielerstand. Gäste speichern nichts.

## 10. Rollen und Rechte

| Rolle | Darf |
|---|---|
| Host | Sitzung öffnen und schließen, Spieler entfernen (Kick) oder sperren (Bann für die Sitzung), speichern |
| Gast | Eingaben und Absichten senden, Sitzung verlassen |

- Nachrichtentypen haben je Rolle und Verbindungszustand eine Erlaubnisliste.
- Ein Gast, der eine Host-Nachricht sendet, wird getrennt.
- Höchstens 4 Spieler (Host + 3).
- Halbe Handshakes: höchstens 8 gleichzeitig, Zeitlimit 5 s.
- Stille: 10 s, dann wird getrennt.
- Wiederverbinden: innerhalb von 120 s mit demselben Sitzungstoken kehrt der Gast mit Inventar und Werten zurück.

## 11. Grenzen der Beta

- **Keine Sprachübertragung.** Begründung in `MULTIPLAYER_ENTSCHEIDUNG.md`: Mikrofonzugriff, Audio-Codec und Moderation sind ein eigenes Datenschutz- und Sicherheitsthema.
- **Kein NAT-Traversal, kein Serverbrowser** (keine Dritten).
- Die letzte Aufzugsfahrt (Flucht mit drei Sicherungen, V9) gibt es nur in der alten, zusammenhängenden Welt. Die Reise durch die zehn Ebenen nutzt sie nicht; im Koop ist sie aus.

## 12. Umsetzung (Stand V13.2)

| Teil | Wo | Anmerkung |
|---|---|---|
| Transport, Handshake, Verschlüsselung | `src/netdll/` → `liminal_net.dll` (ENet 1.3.18, CNG) | Exporte: genau die zehn `ln_`-Funktionen (`tools/check_no_network.py` prüft das) |
| Spielprotokoll v3 (V13.2: v2 + Gesten der neuen Wesen) | `src/net/coop_proto.*` | Join, Input (bis 4 Eingaben je Paket), Action, Ping, Bye; Welcome, Snapshot, Event, Inventory, Roster, Pong, Check |
| Host und Gast | `src/coop/coop.*` | `CoopHost`, `CoopClient`; DLL-Lader `netapi_dll.cpp` (nur aus dem Ordner des Spiels) |
| Figuren der Mitspieler | `src/gameplay/pawn.*` | Spielermodell, Lampenhaltung von außen, eigener Lichtkegel |
| Sitzung im Koop | `src/gameplay/session_coop.cpp` | Absichten des Gasts am Zustand des Hosts geprüft, Welt nach dem Host beim Gast |
| Wesen für mehrere Spieler | `src/gameplay/presence.cpp` | Ziel je Wesen (nächster lebender Spieler), Regie reihum, Erscheinen nie im Blick irgendeines Spielers, Puppen beim Gast |
| Laden um mehrere Spieler | `src/world/world.cpp` | Kacheln um den Host und um jede Figur eines Gasts |
| Oberfläche | `src/ui/ui_coop.cpp` | Hauptmenü „Koop (Beta)“, Pausenmenü „Mitspieler“, Statuszeile, Namen über den Mitspielern |
| Prüfungen | `tests/net_transport_test.cpp`, `tests/coop_test.cpp` | `MULTIPLAYER_TESTS.md` |

Abweichungen und Ergänzungen gegenüber den Abschnitten 1–11:

- **Bestätigung im Handshake:** Nach dem angenommenen Beweis sendet der Host eine leere, verschlüsselte Nachricht. Erst damit meldet der Gast „verbunden“ und darf senden. Ohne sie warteten beide aufeinander (gefunden im ersten Bot-Test). Die Authentisierung ist dadurch gegenseitig, bevor ein Byte Spielinhalt fließt.
- **Blick als Winkel, nicht als Änderung:** Eine Eingabe trägt Gier- und Nickwinkel absolut (Nicken auf ±83° begrenzt). Der Blick bestimmt im Koop nur die Laufrichtung und die Lampe, nie Treffer oder Werte; eine begrenzte Blickänderung (Bedrohungsmodell T2) schützt hier nichts. Die Bewegung selbst bleibt Eingabe (Achsen, Tasten), ihr Zeitanteil ist begrenzt (1–100 ms je Eingabe, Zeitbudget je Gast: nicht mehr Spielzeit als die Uhr des Hosts plus 0,5 s).
- **Momentaufnahmen erst nach der ersten Eingabe:** Der Host sendet sie, sobald der Gast spielt. Davor kämen sie auf dem unzuverlässigen Kanal vor dem Willkommen an, und der Gast müsste sie verwerfen.
- **Vorräte:** Der Faktor 1 + 0,6 × (n − 1) gilt ab der nächsten Ebene (n = verbundene Spieler beim Wechsel). Der Host überträgt ihn mit dem Wechsel und im Willkommen. Der Generator würfelt die Zusatzfunde über einen eigenen Hash, deshalb bleibt die Grundbelegung (und das Einzelspiel mit 1,0) unverändert.
- **Ebenenwechsel:** Der Ausgang ist ein Bereich, keine Tür. Wer darin steht, ist bereit; verlässt er ihn, ist er es nicht mehr. Braucht der Übergang eine Sache (Sicherung im Aufzug von Level 3), genügt sie bei einem der Bereiten.
- **Speichern:** Der Host speichert einen eigenen Koop-Stand (Markierung `coop` im Spielstand) mit den Werten, Inventaren und Lampen der Gäste nach Namen. Hostet er „Koop-Stand fortsetzen“, bekommt ein Gast mit demselben Namen seine Sachen zurück. Gäste speichern nichts.
- **Namen über den Mitspielern:** Alle Körper sind dasselbe Spielermodell; der Name schwebt über dem Kopf (mit denselben Matrizen wie das Bild projiziert).
- **Gezeichnete Wesen beim Gast:** Puppen mit derselben Animation wie beim Host. Ihre Schritte erzeugt der Gast selbst; Knurren, Bellen, Flattern, Klopfen und die Schrecken des eigenen Todes kommen als Ereignisse.
