# LIMINAL V12 – Mehrspieler

> **Nicht implementiert.** Der Auftrag für V12 schließt Mehrspieler ausdrücklich aus. Dieses Dokument hält nur fest, was sich durch V12 an der Ausgangslage geändert hat und was ein späterer Mehrspielermodus bräuchte. Die gründliche Machbarkeits- und Bedrohungsanalyse steht in `MULTIPLAYER_ASSESSMENT.md` (aus V11) und gilt weiter.

## 1. Was V12 an der Ausgangslage ändert

| Bereich | V11 | V12 | Folge für Mehrspieler |
|---|---|---|---|
| Welt | Level 0–37 teils prozedural, eine zusammenhängende Welt (V8) | zehn feste Karten (`data/maps/*.json`) aus Python-Skripten, feste Übergänge mit Ankern | Leichter: Alle Teilnehmer laden dieselbe Karte, ohne Seed-Abgleich der Welt. Die Funde wählt weiter ein Spawnplan nach Seed. |
| Übergänge | Notausgänge, Aufzüge mit Sicherung | Nähte, Türen, Aufzug, Fenster, Metro-Wagen; das Ende an einem Gate | Ein Ebenenwechsel betrifft heute nur **einen** Spieler. Im Mehrspieler: Gruppe wechselt gemeinsam (Abstimmung) oder Spieler sind auf verschiedenen Ebenen (getrennte Server-Instanzen je Ebene). |
| Wesen | Regie für einen Spieler | gleich, je Level auf die Wesen des Wikis beschränkt | Unverändert: Smiler, Gestalt und Hound reagieren auf **den** Blick und das Licht **eines** Spielers und müssten neu entworfen werden. |
| Verstand | je Spieler, Raumtypen ziehen ab (Bibliothek: Erinnerungsverlust, Hof: Kälte, Flughafen) | dito | pro Spieler serverseitig rechnen |
| Oberfläche | V11 | neue Tafeln, Streckenplan, Ankunftstafel | Der Streckenplan könnte zeigen, wo die anderen sind. |

## 2. Was ein späterer Mehrspielermodus mindestens bräuchte

1. **Serverautoritativ:**
   - Der Client schickt nur Eingaben.
   - Der Server rechnet Bewegung, Kollision mit Blöcken und Treppen, Funde, Inventar, Leben, Verstand, Tod, Sicherungspunkte, Übergänge und Wesen.
2. **Fester Takt:** Die Sitzung läuft heute mit variabler Bildzeit (nur Tests nutzen `--fixed-dt`).
3. **Ebenen als Instanzen:** Ein Server je belegter Ebene, Übergabe des Spielers samt Inventar beim Übergang (heute `carryOver`).
4. **Interessenbereich:** nur senden, was ein Client sehen darf (z. B. keine Positionen verborgener Hounds und Clumps).
5. **Sicherheit:** Siehe `MULTIPLAYER_ASSESSMENT.md`, Abschnitte 2–6:
   - Bedrohungen: Manipulation, Überflutung, Auslesen;
   - verschlüsselter Transport;
   - Begrenzung von Rate und Größe;
   - keine IP-Adressen anderer Spieler offenlegen.
   Ohne Tests in einer Netzumgebung lässt sich keine Sicherheit behaupten.

## 3. Was bewusst nicht getan wurde

- kein Netzwerkcode, keine Bibliothek, kein Protokoll, keine offenen Ports
- keine Vorbereitung im Code, die später falsche Annahmen festschreiben würde
