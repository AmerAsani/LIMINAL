# LIMINAL V11 – Weltaufbau: vollständig prozedural oder semi-prozedural?

Stand: 25.10.2026. Anlass: dein Vorschlag einer semi-prozeduralen Generierung, also feste Architektur je Level mit variabler Belegung aus festgelegten Spawnpunkten. Grundlage der Bewertung ist der Generator von V10 (`src/world/generator.cpp`, `world.cpp`, `room.cpp`) und die Ziele aus `LEVEL_SELECTION.md`.

**Ergebnis:** Für LIMINAL V11 ist der semi-prozedurale Aufbau die bessere Wahl. V11 wechselt dazu, mit drei Ergänzungen (Abschnitt 4). Der bisherige Generator bleibt im Code für alte Spielstände und die Regressionstests erhalten.

---

## 1. Was V10 heute macht

Die Welt ist unendlich und entsteht beim Laufen aus dem Seed: Sektoren von 96 × 96 m werden per BSP in Rechteckräume geteilt, Raumtypen und Zonen kommen aus Rauschfeldern, Türen aus einem Spannbaum. Jeder neue Lauf (neuer Seed) erzeugt eine völlig andere Architektur.

Was das im Spiel bewirkt (siehe Befund in `LEVEL_SELECTION.md`, Abschnitt 4.1):
- Räume entstehen zufällig; niemand hat Sichtachsen, Raumfolgen oder Orientierungspunkte entworfen.
- Übergänge zwischen den Ebenen sind breite Mischzonen, die man suchen muss.
- Begegnungen können überall entstehen, also nur mit allgemeinen Regeln abgesichert werden („außer Sicht, mindestens 15 m“).
- Gegenstände folgen Wahrscheinlichkeiten je Raum; Balance lässt sich nur statistisch prüfen.
- Ein Fehler in einem Seed ist schwer zu finden, weil es unendlich viele Seeds gibt.

## 2. Vergleich

| Kriterium | Vollständig prozedural (V10) | Semi-prozedural (Vorschlag) |
|---|---|---|
| Komponierte Räume, Sichtachsen, Raumfolgen | schwach – zufällige Rechtecke | **stark** – jeder Raum ist entworfen |
| Orientierung, Landmarken | schwach – nichts ist wiedererkennbar | **stark** – feste Landmarken, die man sich merken kann |
| Faire Begegnungen | nur über allgemeine Regeln | **gut prüfbar** – Spawnpunkte mit bekannten Sichtlinien und Fluchtwegen |
| Balance der Vorräte | statistisch | **planbar** – Budgets je Abschnitt und entlang des Weges |
| Übergänge zwischen Leveln | gesucht, zufällig | **gestaltet** – Treppenhaus, Aufzug, Kesselraum stehen an festen Orten |
| Endlosigkeit (Backrooms-Kern) | **echt unendlich** | endlich – muss gestaltet werden (Schleifen, Grenzen) |
| Wiederspielwert | hoch (neue Architektur) | mittel – Architektur bekannt, Belegung, Ereignisse und Gefahren wechseln |
| Desorientierung beim zweiten Lauf | bleibt | sinkt (Spieler lernen die Karte) |
| Testbarkeit, reproduzierbare Fehler | schwer (unendlich viele Seeds) | **einfach** – eine Karte je Level, vollständig prüfbar |
| Speicher und Leistung | Streaming nötig, unbegrenzt | begrenzt und vorhersagbar |
| Aufwand | gering je neuem Level (nur Daten) | **hoch** – jede Karte muss entworfen werden |
| Speichern/Laden | Seed + Zustand | Lauf-Seed + Zustand (genauso einfach) |

## 3. Abwägung

**Für den semi-prozeduralen Aufbau spricht:** Fast alle Anforderungen aus dem V11-Auftrag zielen auf Gestaltung, nicht auf Menge: komponierte Räume statt Zufallskorridore, Orientierungspunkte, Licht- und Schattenkontraste, faire, lesbare Begegnungen, nachvollziehbare Übergänge, ein neu entworfenes Level 1 mit Raumfolgen. Ein Zufallsgenerator kann das nur mühsam annähern. Eine feste Architektur liefert es direkt. Dazu kommen Testbarkeit und planbare Balance.

**Dagegen spricht vor allem zweierlei:**
1. **Die Endlosigkeit der Backrooms.** Die Wikis beschreiben die meisten Level als unendlich. Eine feste Karte ist endlich. → Lösung in Abschnitt 4.3.
2. **Der Aufwand.** Zehn entworfene Karten sind viel Arbeit, und ein visueller Level-Editor fehlt (`ENGINE_DECISION.md`: das ist ein Pluspunkt für Unreal und Unity). → Lösung: Karten werden als Text-Daten beschrieben und mit eigenen Werkzeugen geprüft und als Draufsicht dargestellt (Abschnitt 5). Die Engine-Entscheidung bleibt davon unberührt. Werden die Karten später von Hand in einem Editor gebaut, wäre sie neu zu prüfen.

Der geringere Wiederspielwert ist vertretbar. LIMINAL ist eine Reise durch zehn Ebenen mit Ende, kein endloses Rogue-like. Was sich zwischen Läufen ändert (Belegung, Gefahren, Stromausfälle, Ereignisse), reicht für erneutes Spielen.

## 4. Entscheidung für V11

### 4.1 Fest (in jedem Lauf gleich)
- Wände, Raumaufteilung, Raumhöhen, Böden und Treppen
- Türen, Durchgänge und ihre Verbindungen
- Korridore, Hallen, Rampen, Treppenhäuser, Schächte
- Feste Objekte für Orientierung und Fortschritt: Landmarken (Wagen, Kronleuchter, Statuen), Übergänge (Aufzug, Treppenhaus, Bodenloch, Kesselraum), Leuchten und ihre Stromkreise
- Levelgrenzen

### 4.2 Variabel (je Lauf aus festgelegten Spawnpunkten)
| Element | Auswahl |
|---|---|
| Gegenstände und Vorräte (Mandelwasser, Riegel, Batterien) | aus Spawnpunkten mit Etikett (Regal, Theke, Kiste, Boden, Tisch) |
| Fortschrittsgegenstände (Sicherung) | aus mehreren Kandidaten, **garantiert genau einer** |
| Notizen | aus Spawnpunkten, Reihenfolge der Inhalte wie bisher |
| Gegner | aus Spawnpunkten und Zonen je Kreatur (Lüftungsgitter für das Bündel, dunkle Lagerboxen für den Hund, Türen dunkler Zimmer für Nester). Die Regie wählt zur Laufzeit einen Punkt, der die Fairnessregeln erfüllt. V11: Jeder Spawnpunkt `entity` mit der Rolle `clump` wird ein Deckenschacht (Modell `ceiling_vent`, das Gitter hängt offen); das Bündel zieht in einen Schacht auf 14–60 m Weg, der nicht im Blick liegt |
| Interaktive Objekte ohne Fortschrittsfunktion (Automaten, Wasserspender, Kisten) | aus Spawnpunkten |
| Dekoration (Paletten, Pfützen, Müll, Bilder, abgestellte Wagen in Nebenbuchten) | aus Spawnpunkten mit Varianten |
| Ereignisse | welche Lichtfelder defekt sind, wann ein Stromausfall kommt, welche fernen Geräusche |

**Regeln der Auswahl** (deterministisch aus dem Lauf-Seed, damit ein Spielstand nur den Seed braucht):
1. **Gültigkeit**, beim Übersetzen der Karte geprüft: begehbare Kachel, genug Kopfhöhe, Abstand zu Türen und Übergängen, Grundfläche des Objekts frei.
2. **Erreichbarkeit:** nur Punkte, die vom Levelanfang aus erreichbar sind (vorberechnetes Wegefeld).
3. **Abstände:** Mindestabstand je Art (z. B. 12 m zwischen Mandelwasser), sich ausschließende Gruppen (zwei Punkte am selben Regal).
4. **Balance:**
   - Budget je Abschnitt und entlang des Hauptweges, z. B. erstes Mandelwasser spätestens nach 60 m Weg;
   - Batterien vor dunklen Abschnitten;
   - Summe des Verstands je Level innerhalb eines Bandes.
5. **Fairness der Gegner:** Spawnpunkt außer Sicht, Wegabstand ≥ 15 m, ein freier Fluchtweg (`ENTITY_BEHAVIOR.md`, F1–F7).

### 4.3 Endlosigkeit, gestaltet statt zufällig
- **Schleifen:** Wo die Quellen Endlosigkeit betonen (Level 0, Level 6), schließt die Karte an ihren Rändern an sich selbst an. Wer immer weiter geradeaus geht, kommt wieder durch bekannte Räume („Hier warst du schon“). Technisch: Ein Gangstück liegt an beiden Rändern identisch; beim Durchqueren wird der Spieler unsichtbar versetzt.
- **Erzählte Grenzen** in den anderen Leveln: verschlossene Stahltüren, eingestürzte Abschnitte, Gitter (Level 3), schwarze Fenster (Level 4), das Meer (Level 7).
- **Größe nach Rolle:**
  - Level 1 als größte Ebene (≈ 300 × 300 m);
  - kleinere Ebenen, wo die Dichte zählt: Level 6 ≈ 130 × 130 m, Level 19 ≈ 130 × 130 m.

## 5. Umsetzung

- **Kartenformat** `data/maps/<level>.json`: Räume (Rechteck, Zone, Höhen, Raumtyp, Einbauten), Türen und Öffnungen, Leuchtengruppen (Stromkreise), Spawnpunkte (Art, Etiketten, Gruppe), feste Objekte, Übergänge mit Ankerpunkt.
- **Entwurf:**
  - Je Level ein Entwurfsskript `tools/levels/<level>.py`. Es beschreibt die Gestalt bewusst: Hauptachsen, Raumfolgen, Landmarken, Hauptweg, Übergänge.
  - Kleinteilige Wiederholungen (z. B. Stellplätze, Bürozellen) entstehen darin regelbasiert, aber **einmalig**. Das Ergebnis liegt als feste Datei im Projekt und ändert sich zwischen Läufen nicht.
  - Die Karte lässt sich direkt in der JSON-Datei nachbearbeiten.
- **Prüfung:** Das Testprogramm `map_check` meldet überlappende Räume, Türen ohne zwei Seiten, unerreichbare Räume, ungültige Spawnpunkte, Übergänge ohne Gegenstück und zu kleine Budgets. Eine Draufsicht je Level (BMP) zeigt Räume, Weg, Spawnpunkte und Landmarken.
- **Laufzeit:**
  - Ein Level ist eine feste Karte, die in Sektoren der bestehenden Kachelwelt übersetzt wird. Licht, Treppen, Einbauten, Chunks, Streaming und Renderer bleiben unverändert nutzbar.
  - Ein Übergang lädt die nächste Karte (Mechanik aus V7) und setzt den Spieler am Gegenstück des Übergangs ab.
- **Bleibt erhalten:** der unendliche Generator (`worldgen_test` bitgenau wie bisher) für Tests und als Grundlage der Entwurfsskripte.

### 5.1 Sicherungspunkte (Wiedereinstieg nach dem Tod)

Die Kreaturen töten sofort (`ENTITY_BEHAVIOR.md`, F4). Nach dem Tod geht es am **letzten Sicherungspunkt** weiter. Diese Punkte gehören zur festen Karte (`"checkpoints"`) und werden im Entwurfsskript mit `safe_point()` gesetzt. Für ihre Lage gelten fünf Regeln, `mapkit.validate()` prüft sie beim Speichern:

1. **Am Eingang jedes Bereichs** (Gang vor dem Deck, Rampen, Vorräume der Hallen, Wartung, Zugang und Kreuzungen im Labyrinth, Rohrgang). Ein Tod wirft höchstens einen Bereich zurück.
2. **Sicher im Stromausfall:** in Räumen mit eigenem Stromkreis, nie im offenen Deck und nie an dessen Stromkreis. Die Rampen haben dafür eigene Kreise bekommen. Sie sind zugleich der Fluchtweg ins Licht.
3. **Beleuchtet und erkennbar:** Jeder Punkt hat eine grüne Sicherheitsleuchte mit eigener Batterie, die immer brennt. Wo der Gang keine eigene Leuchte hat, kommt ein schwaches Licht dazu.
4. **Abstand zu Kreaturen:** mindestens 15 m von den Spawnpunkten umherstreifender Kreaturen (Hund, Bündel), nicht in einem Übergang, nicht auf einem Block. Das Lächeln erscheint nur im Stromausfall und mit mindestens 15 m Weg zum Spieler.
5. **Blick nach vorn:** Man steht nach dem Wiedereinstieg in der Richtung, in der es weitergeht.

**Im Spiel:**
- **Auslösen:** Wer näher als 2,2 m vorbeigeht, erreicht den Punkt. Es erscheint „Sicherer Ort · <Name>“, der Starter der Leuchte klickt, und das Spiel speichert. Mitten in einer Begegnung wird nicht gesichert.
- **Start:** Der Punkt am Start gilt still als erreicht.
- **Wiedereinstieg:**
  - volle Gesundheit, Verstand mindestens 60 %, Ausdauer voll;
  - Inventar und Funde bleiben;
  - der Stromausfall ist vorbei, die Entities sind fort, danach 90 s Schonzeit.
- **Spielstand:** Der letzte Punkt steht im Spielstand.

Level 1 hat 12 Punkte. Der Bereich um Deck B hat keinen eigenen Labyrinth-Zugang, weil alle Kandidaten zu nah an der Ecke des Hundes lagen; die Vorräume der Hallen und die Labyrinth-Kreuzungen decken ihn ab.

## 6. Was das an den anderen Dokumenten ändert

- `LEVEL_SELECTION.md` Abschnitt 2: Die Ebenen sind keine Schichten einer endlosen Welt mehr, sondern zehn feste Karten. Die Übergänge stehen an festen Orten. Die Reihenfolge und die Grundlagen der Übergänge bleiben gleich.
- `ENTITY_BEHAVIOR.md`: Gegner erscheinen an Spawnpunkten statt an beliebigen Kacheln. Die Fairnessregeln gelten unverändert.
- `ENGINE_DECISION.md`: Ein visueller Level-Editor gewinnt an Gewicht. Solange die Karten als Text entworfen und geprüft werden, bleibt die Entscheidung bestehen.
