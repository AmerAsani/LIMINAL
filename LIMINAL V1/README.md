# LIMINAL

Ein First-Person-Erkundungsspiel, das komplett im Terminal läuft. Du gehst durch eine unendliche, prozedural erzeugte Welt aus Innenräumen: Büroflure, Betonhallen, unterirdische Tunnel, riesige leere Hallen und enge Wartungsgänge.

Es gibt keine Gegner, keine Items, kein Ziel und keine Punkte. Das Spiel besteht nur aus Architektur, Licht und Nebel.

Das Spiel ist in reinem Python geschrieben und nutzt nur die Standardbibliothek (≥ 3.8). Externe Pakete wie `windows-curses` braucht es nicht.

## Starten

```bash
python liminal3d.py
```

Weitere Beispiele:

```bash
python liminal3d.py --seed 4242          # bestimmte Welt, reproduzierbar
python liminal3d.py --mode hires         # Halbblock-Grafik mit doppelter vertikaler Auflösung
python liminal3d.py --demo               # Autopilot wandert selbstständig, Taste übernimmt
python liminal3d.py --benchmark 900      # Messung ohne Terminal, gibt FPS aus
python -m liminal --help                 # alle Optionen
```

Empfohlen ist ein modernes Terminal mit Truecolor, zum Beispiel Windows Terminal, kitty, WezTerm oder iTerm2, und ein Fenster mit etwa 160×45 Zeichen oder mehr. Eine kleinere Schrift ergibt eine höhere Auflösung.

Auf diesem Rechner ist Python nicht im PATH. Das mit Inkscape gelieferte Python 3.11 funktioniert aber:

```bash
& "C:\Program Files\Inkscape\bin\python.exe" liminal3d.py
```

## Steuerung

| Taste | Funktion |
|---|---|
| W / S | vorwärts / rückwärts |
| A / D | seitlich gehen |
| ← / → (oder Q / E) | umsehen (mit weicher Beschleunigung) |
| ↑ / ↓ | nach oben / unten sehen |
| Shift | sprinten |
| + / − | Laufgeschwindigkeit einstellen |
| V | Darstellung wechseln: ASCII-Schattierung → Halbblock-HiRes → Monochrom |
| F3 oder I | Debug-Anzeige |
| M oder Tab | Karte (Draufsicht der geladenen Welt) |
| F | Leistungsmodus (halbe horizontale Auflösung) |
| B | Head-Bobbing an/aus |
| P | Screenshot als HTML-Datei |
| H | Hilfe |
| ESC | beenden |

Die Tasten reagieren direkt, ohne Enter. Unter Windows liest das Spiel echte Drück- und Loslass-Ereignisse der Konsole. Unter Linux und macOS nutzt es das Kitty-Tastaturprotokoll, wenn das Terminal es kann. Sonst schätzt es das Halten einer Taste aus der Tastenwiederholung.

## Aufbau

```
liminal/
  rng.py        deterministisches Hashing, RNG (SplitMix64), Value-Noise
  zones.py      5 Zonen (Palette, Licht, Nebel, Raumgrößen) + Zonenfeld mit weichen Übergängen
  roomtypes.py  Katalog der ~30 Raumtypen und Seltenheitsverteilung
  rooms.py      Room / Door: Metadaten und das Rastern einzelner Kacheln (Säulen, Regale, Treppen, Lampen)
  generator.py  Generator / Sector: BSP-Teilung, Türen, Portale, Bodenhöhen, Materialien
  world.py      World / Chunk: Nachladen, Entladen, Flackern
  player.py     Player: Trägheit, Kollision mit Wand-Sliding, Treppen, Schwerkraft, Head-Bobbing
  raycaster.py  Raycaster / Camera: 2.5D-Raycasting mit variablen Boden- und Deckenhöhen
  renderer.py   Shader (Farbtabellen, Nebel, Tonkurve) / Renderer (Terminalausgabe)
  terminal.py   Terminal / Input (Windows-Konsole, Unix-Terminals)
  hud.py        Debug-Anzeige, Karte, Hilfe, Meldungen
  game.py       Game-Loop, Kommandozeile, Benchmark, Demo-Autopilot
```

Weltgenerierung und Darstellung sind strikt getrennt. Die Generierung liefert nur Kachelzellen mit Material-IDs. Wie diese aussehen, entscheidet allein der Renderer.

## Weltgenerierung

- **Sektoren (96×96 Kacheln):** Jeder Sektor entsteht nur aus `(Seed, sx, sy)` und ist unabhängig von der Reihenfolge, in der du die Welt erkundest. Wenn du zurückläufst, siehst du dieselben Räume.
- **Portale:** Auf jeder Sektorkante liegen 2 bis 4 Durchgänge. Ihre Position hängt nur von der Kante selbst ab, also berechnen beide Nachbarsektoren dieselben Türen.
- **BSP-Teilung:** Rechtecke werden rekursiv geteilt, gelegentlich mit einem eingefügten Korridorstreifen (ein Flur mit Räumen zu beiden Seiten). Selten zieht sich ein endloser Gang durch den ganzen Sektor. Teilungswände dürfen nie ein Portal treffen. Räume können sich daher nicht überschneiden, und Wände laufen nicht durch Nachbarräume.
- **Verbindungen:** Jede Teilungswand bekommt mindestens eine Tür zwischen angrenzenden Räumen. Das ergibt einen Spannbaum, und alles bleibt erreichbar. Zusätzliche Türen erzeugen Schleifen. Große Räume verbinden sich über breite Tore oder offene Übergänge.
- **Höhen:** Tunnel, abgesenkte Hallen und Treppenhäuser liegen tiefer. Eine Fixpunkt-Iteration hebt jeden Raum an, in dem eine Treppe keinen Platz hätte. Treppen enden deshalb immer auf der Höhe der Türschwelle.
- **Zonen:** Fünf niederfrequente Rauschfelder bestimmen die Zone. An Übergängen mischen sich Farben, Böden und Raumtypen allmählich.
- **Seltenheit:** Die Grundverteilung ist grob 60 % normal, 20 % Korridore, 10 % Hallen, 7 % ungewöhnlich und 3 % extrem selten. Seltenes wird häufiger, je weiter du dich vom Start entfernst.
- **Streaming:** 16×16-Chunks werden rund um dich nachgeladen (höchstens zwei pro Frame, die nächsten zuerst) und in der Ferne entladen. Sektor-Metadaten werden ebenfalls verworfen.
- **Raum-Metadaten:** Jeder Raum speichert Position, Größe, Höhe, Boden, Türen, Verbindungen, Typ, Zone und Seed. Die bereits generierten Nachbarräume zeigt die Debug-Anzeige.

## Darstellung

- **Raycaster:** Pro Spalte läuft ein DDA-Strahl durch das Raster. Jede Kachel hat eine eigene Boden- und Deckenhöhe. So entstehen Türstürze, Treppenstufen, halbhohe Trennwände, über die du hinwegsehen kannst, Kisten, Podeste und Hallen mit 40 m hohen Decken.
- **Shading:** Die Helligkeit ergibt sich aus Raumlicht, Lampen mit Lichtabfall, Licht aus Nachbarräumen, der Ausrichtung der Wandfläche, Entfernung und exponentiellem Nebel in Zonenfarbe. Das Ergebnis wird in Zeichen von `█ ▓ ▒ ░ : . ·` mit 24-Bit-Vorder- und Hintergrundfarbe übersetzt.
- **Muster:** Fugen, Tapete, Deckenplatten, Bodenraster und Schachbrett. Die Linien werden abhängig von der Entfernung verbreitert (einfaches Anti-Aliasing) und flimmern in der Ferne nicht.
- **Leistung:** Farben stehen in vorberechneten Tabellen je Material, Licht und Entfernung. Eine Bodenspalte zu füllen ist ein einziges Slice-Assignment. Die Terminalausgabe sendet Farbcodes nur, wenn sich die Farbe ändert.

Gemessen mit `--benchmark` und dem Autopiloten auf dem Entwicklungsrechner (Python 3.11, ohne Terminalausgabe):

| Größe | Modus | Zeit pro Frame |
|---|---|---|
| 160×48 | ASCII | ca. 2 ms |
| 240×65 | Halbblock (240×130 Pixel) | ca. 5 ms |

Der Engpass ist meist das Terminal selbst. Das Spiel ist deshalb standardmäßig auf 60 FPS begrenzt (`--max-fps`).

## Grenzen

- Maussteuerung ist nicht eingebaut. Du siehst dich mit den Pfeiltasten und Q/E um.
- In Terminals ohne Loslass-Ereignisse (nicht Windows und ohne Kitty-Protokoll) läuft die Bewegung nach dem Loslassen einer Taste bis zu einer halben Sekunde nach, weil das Halten nur aus der Tastenwiederholung geschätzt wird.
