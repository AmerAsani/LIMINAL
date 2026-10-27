# LIMINAL V13 – Entscheidung Mehrspieler

## V13.3: Koop ohne Router-Einstellungen, Beta-Etikett entfernt (Stand 27.10.2026, E-033)

**Der Koop-Modus heißt jetzt „Koop“ (ohne „Beta“).**
- Beitritt mit Code über eine Lobby, ohne Portfreigabe. Kein direkter Weg → automatisch über das Relay.
- Direkt (LAN/IP) mit langem Code für Fortgeschrittene.
- Verschlüsselung und Echtheit der Gegenstelle durch die Bibliothek (DTLS). Der eigene Handshake und die eigene AES-Schicht aus V13.2 sind entfernt.

Er ist **nicht „vollkommen sicher“**; die Restrisiken stehen in `MULTIPLAYER_BEDROHUNGSMODELL.md`, Abschnitte 7, 8 und 9.5.

Bedingungen des Auftrags für das Entfernen des Beta-Etiketts:

| Bedingung | Stand | Beleg |
|---|---|---|
| Recherche abgeschlossen | erfüllt | `MULTIPLAYER_RECHERCHE.md` (fünf Spiele, neun Lösungen, Quellen) |
| bewährte Bibliothek integriert | erfüllt | libdatachannel 0.24.6, libjuice 1.7.4, Mbed TLS 3.6.7 LTS, usrsctp; unverändert in `third_party/` (`LICENSES.md`, Abschnitt 7.1) |
| alle Tests grün | erfüllt | `MULTIPLAYER_TESTS.md`, Abschnitt 7: Transport, Koop mit Bots über alle Wege, Angriffe, Fuzzing, Reise, Langlauf 60 min; Einzelspieler-Tests unverändert grün |
| Bedrohungsmodell aktualisiert | erfüllt | `MULTIPLAYER_BEDROHUNGSMODELL.md`, Abschnitt 9 |
| Restrisiken dokumentiert | erfüllt | ebenda, Abschnitt 9.5; `MULTIPLAYER_SETUP.md`, Abschnitt 5 (Datenschutz) |

**Offen, ehrlich benannt** (nicht Teil der Bedingungen, aber Bedingungen aus V13):
- **Unabhängige Durchsicht durch Dritte:** weiterhin nicht möglich.
- **Fuzzing über Stunden:**
  - Die eigenen Parser liefen Hunderttausende Fälle, der Netzweg 5 min, der Langlauf 60 min.
  - Die Parser der Bibliotheken (STUN, DTLS, SCTP) habe ich nicht selbst gefuzzt; sie sind Teil von OSS-Fuzz bzw. gepflegter Projekte.
- **Kein Test über das echte Internet:** Es fehlen zwei Rechner in verschiedenen Netzen, echte NAT-Router und Mobilfunk. Alles lief auf einem Rechner mit Netzstörer. Die Anleitung für den Echttest steht in `MULTIPLAYER_TESTS.md`, Abschnitt 7.6.
- **Lobby öffentlich betreiben:** Das macht der Herausgeber (`MULTIPLAYER_SETUP.md`). Ohne eingetragene Lobby geht nur „Direkt“.

---

## V13.2: Koop als Beta freigegeben (Stand 26.10.2026, DECISIONS E-030)

**Der Koop-Modus ist als Beta im Spiel:** Hauptmenü „Koop (Beta)“ (Hosten oder Beitreten), im Pausenmenü „Mitspieler“. Bis zu vier Spieler, ein Spieler hostet (`MULTIPLAYER_ARCHITEKTUR.md`). Er ist nie „vollkommen sicher“; das Restrisiko steht in `MULTIPLAYER_BEDROHUNGSMODELL.md`, Abschnitte 7 und 8.

Regel des Auftrags: Beta, wenn die Tests bestehen. Sie bestehen (`MULTIPLAYER_TESTS.md`, Abschnitt 5). Die Bedingungen aus V13 (unten) im Einzelnen:

| Bedingung (V13) | Stand V13.2 |
|---|---|
| 1. Alle Regeln aus Abschnitt 6 des Bedrohungsmodells; nur `src/net` führt Netzbytes ins Spiel | erfüllt. Abweichung: Der Blick kommt als Winkel (begründet im Bedrohungsmodell, T2) |
| 2. Verschlüsselt und authentisiert mit geprüfter Bibliothek | erfüllt: Windows-Kryptografie (CNG), Testvektoren aus RFC 4231, RFC 5869 und der GCM-Spezifikation |
| 3. Tests mit echten Prozessen: Beitritt, Abbruch, Wiederverbinden, Host verlässt, Wettläufe beim Ebenenwechsel, Verlust und Vertauschung, Flut | erfüllt: Bots über echtes UDP und Netzstörer, zwei echte Spielprozesse, Reise durch zehn Ebenen |
| 4. Fuzzing des Netzwegs über Stunden; Sanitizer | **teilweise**: Prüfschicht 12 Mio. Fälle unter ASan/UBSan, Transporttest unter Sanitizern, Netzweg-Fuzzing über Minuten, Langlauf 60 min. Nicht über Stunden |
| 5. Einzelspieler-Tests und Bildzeiten unverändert | erfüllt (Tests). Ohne Koop lädt das Spiel die DLL nicht und öffnet keinen Port |
| 6. Unabhängige Durchsicht | **nicht erfüllt**: niemand Drittes verfügbar; Selbstdurchsicht entlang des Bedrohungsmodells dokumentiert |

Wegen 4 und 6 bleibt es eine **Beta**, deutlich so beschriftet. Sprachübertragung gibt es nicht: Mikrofonzugriff, Codec und Moderation wären ein eigenes Datenschutz- und Sicherheitsthema.

---

## V13 (ursprüngliche Entscheidung, überholt durch V13.2)

Stand 26.10.2026. Grundlage: `MULTIPLAYER_BEDROHUNGSMODELL.md` (dazu die früheren Analysen `MULTIPLAYER_ASSESSMENT.md`, V11, und `MULTIPLAYER.md`, V12), Prüfungen: `MULTIPLAYER_TESTS.md`. Entscheidung E-024 in `DECISIONS.md`.

## Entscheidung

**Der Mehrspielermodus bleibt in V13 ausgeschaltet und ist nicht erreichbar.**

- Kein Menüpunkt, keine Befehlszeilenoption, keine Einstellung.
- Kein Netzwerkcode im Spiel: `LiminalGame.exe` importiert keine Netzwerkbibliothek und enthält keinen Winsock-, WinHTTP- oder WinINet-Aufruf. Geprüft mit `python tools/check_no_network.py` (Ergebnis in `MULTIPLAYER_TESTS.md`).
- Kein offener Port, kein Hintergrunddienst.

Damit ändert sich für den Einzelspieler nichts: weder Stabilität noch Leistung noch Angriffsfläche.

## Begründung

Der Auftrag erlaubt das Einschalten nur, wenn der Modus **nachweislich sauber** ist und alle Tests bestehen. Das ist in V13 nicht erreichbar, ohne Stabilität und Qualität des Einzelspielers zu gefährden:

1. **Es gibt keine Grundlage im Spiel.** Wesen-KI (Smiler, Hound, Clump, Gestalt, Motten), Gegenstände, Türen, Aufzüge, Notizen, Spielstände und die zehn Ebenen mit Übergängen sind für einen Spieler gebaut. Ein autoritativer Server verlangt, dass jedes dieser Systeme mehrere Spieler kennt (wen jagt ein Wesen, wem gehört ein Gegenstand, wer wechselt die Ebene). Das berührt fast jede Datei unter `src/gameplay` – genau dort entstünden Fehler im Einzelspieler.
2. **„Nachweislich sicher“ verlangt mehr als Tests einer Teilschicht.** Nachweise wären: vollständige Umsetzung aller Regeln aus Abschnitt 6 des Bedrohungsmodells, Fuzzing des echten Netzwegs, Tests für Verbindungsabbruch, Wettläufe und Wiederverbinden mit echten Prozessen, eine verschlüsselte und authentisierte Verbindung (geprüfte Bibliothek, neue Abhängigkeit mit Lizenz- und Lieferkettenprüfung) und eine unabhängige Durchsicht. Eine halbe Umsetzung mit Beta-Etikett würde die Angriffsfläche öffnen, ohne diese Nachweise zu liefern.
3. **Regel des Auftrags bei Unklarheit:** höhere Sicherheit, höhere Stabilität.

## Was V13 trotzdem liefert

- **Bedrohungsmodell** mit verbindlichen Architekturregeln (`MULTIPLAYER_BEDROHUNGSMODELL.md`).
- **Geprüfte Prüfschicht des Protokolls** (`src/net/protocol.*`, Bibliothek `lim_net`, vom Spiel nicht gebunden):
  - Nachrichten fester Größe; der Gast kann nur Eingaben senden (Bewegungsachsen, begrenzte Blickänderung, Tastenbits), nie Zustände.
  - Prüfung jeder Nachricht nach Kennung, Typ, Rolle, Verbindungszustand, exakter Länge, Version, Wertebereich, Namen, Tasten, Sitzungscode (Vergleich in fester Zeit), Folgenummer und Rate.
  - Gegenstelle mit Zeitlimits (Handshake 5 s, Stille 10 s), Trennung nach 8 ungültigen Paketen oder anhaltender Flut.
- **Tests:** `net_protocol_test` mit 72 Prüfungen, 1 000 000 Fuzz-Runden, zusätzlich unter AddressSanitizer und UndefinedBehaviorSanitizer (`MULTIPLAYER_TESTS.md`).
- **Härtung beim Starter:** Der Starter lädt nur auf alten Windows-Versionen ohne Universal C Runtime und nur nach Rückfrage das Visual-C++-Paket von Microsoft. Bisher genügte irgendeine gültige Signatur. Jetzt muss der Unterzeichner „Microsoft Corporation“ sein, und die Zertifikatskette wird auf Widerruf geprüft (`BUGLISTE.md` B-003).

## Bedingungen für ein späteres Einschalten (als Beta)

1. Alle Regeln aus Abschnitt 6 des Bedrohungsmodells umgesetzt; `lim_net` ist der einzige Weg von Netzbytes ins Spiel.
2. Verbindung verschlüsselt und authentisiert mit einer geprüften Bibliothek (Lizenz in `LICENSES.md`), oder der Modus ist auf das lokale Netz beschränkt und sagt das deutlich.
3. Tests mit echten Prozessen: Beitritt, Abbruch, Wiederverbinden, Host verlässt das Spiel, Wettläufe beim Ebenenwechsel, Paketverlust und -vertauschung, Flut.
4. Fuzzing des Netzwegs (nicht nur der Prüfschicht) über Stunden ohne Fund; Sanitizer-Läufe.
5. Einzelspieler-Tests (gameplay, audio, worldgen, supply) und Bildzeiten unverändert.
6. Unabhängige Durchsicht des Netzcodes.

Erst dann: Menüpunkt mit Beta-Hinweis, und auch dann nie die Behauptung, der Modus sei „vollkommen sicher“.
