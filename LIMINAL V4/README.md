# LIMINAL V4

Ein First-Person-Erkundungsspiel, das komplett im Terminal läuft. Du gehst durch eine unendliche, prozedural erzeugte Welt aus Innenräumen: Büroflure, Betonhallen, unterirdische Tunnel, riesige leere Hallen und enge Wartungsgänge. Deine Sanity schwindet mit der Zeit. Seltene Vorräte halten dich bei Verstand.

## Starten

Doppelklick auf **`LIMINAL.exe`**.

Du brauchst weder eine Python-Installation noch eine Entwicklungsumgebung; die Laufzeit liegt im Ordner `runtime`. Behalte den Ordner `LIMINAL V3` zusammen und leg für den Desktop am besten eine Verknüpfung an.

Beim Start setzt LIMINAL automatisch deinen letzten Spielstand fort.

```bash
LIMINAL.exe --new
```

startet stattdessen eine neue Welt, und `LIMINAL.exe --seed 4242` eine bestimmte Welt.

## Neu in V4

- **Hauptmenü beim Start:** Neues Spiel, Spiel fortsetzen, Einstellungen, Beenden. Hinter dem Menü dreht sich langsam die Spielwelt.
- **Neues Spiel:** Du tippst einen Namen ein und wählst die Schwierigkeit:
  - *Easy*: Sanity −10 %/min, 25 s bis zum Tod, deutlich mehr Items
  - *Medium*: Sanity −15 %/min, 15 s bis zum Tod, normale Item-Menge
  - *Hard*: Sanity −22 %/min, 10 s bis zum Tod, wenige Items

  Auch die Erholung der Health hängt von der Schwierigkeit ab.
- **Spiel fortsetzen:** Zeigt alle Spielstände mit Vorschaubild, Name, Schwierigkeit, Datum, Spielzeit und Werten. Enter lädt einen Stand, zweimal Entf löscht ihn. Ein vorhandener V3-Spielstand erscheint automatisch in der Liste.
- **Pausenmenü (ESC):** Neu sind *Spiel speichern* (legt den Spielstand an oder aktualisiert ihn) und *Zum Hauptmenü*. Der Autosave schreibt in denselben Spielstand. Spielstände liegen in `%APPDATA%\LIMINAL\saves_v4`.
- **Vollbild:** Beim Start wird das Konsolenfenster maximiert und in den Vollbildmodus geschaltet (wie Alt+Enter). Beides lässt sich unter Grafik & Anzeige abschalten.
- Mit `LIMINAL.exe --seed N --name X --difficulty hard` startest du direkt ohne Hauptmenü.

## Neu in V3

- **Echte Kameraperspektive:** Die Welt wird zuerst als Panorama gerendert; aus jeder Richtung ist das ein gleichmäßiges Raster aus Dreh- und Höhenwinkel. Danach wird sie für jeden Bildpunkt in eine korrekt geneigte Kamera umprojiziert.
  - Beim Blick nach oben oder unten werden Wände und Säulen weder zu hoch noch zu breit. Senkrechte Kanten laufen zusammen wie auf einem Foto.
  - Die Abweichung zu einer idealen Kamera beträgt höchstens etwa 1 Pixel. Du kannst bis etwa 52° nach oben und unten schauen.
  - Bei weitem Sichtfeld (über 90°) mischt die Einstellung *Projektion: Auto* die Panini-Projektion bei; sie verhindert, dass Dinge am Bildrand stark in die Breite gezogen werden. Du kannst auch *Lochkamera* oder *Panini* fest wählen.
  - *Bildproportion* gleicht die Zeichenhöhe deines Terminals aus. Beides findest du unter ESC → Grafik & Anzeige.
- **Items:** Es gibt zwei Items; beide sind selten.
  - *Energy Bar*: +20 % Geschwindigkeit für 60 Sekunden und +20 % Sanity.
  - *Mandelwasser*: +35 % Sanity.

  Sie liegen nie zufällig auf dem Boden, sondern auf den Theken kleiner Versorgungsräume. Jeder dieser Räume hat einen rot leuchtenden Automaten, dessen Licht auch in den Flur fällt. Etwa jeder 20. Raum ist ein Versorgungsraum, im Schnitt gibt es rund 4 Items pro 96 × 96 m.
  - Auf der Karte (M) sind Automaten magenta und Items gelb markiert.
  - Kommst du einem Versorgungsraum mit Items nahe, erscheint der Hinweis „Ganz in der Nähe summt ein Automat …".
  - Aufgenommene Items bleiben dauerhaft verschwunden, auch nach dem Laden.
- **Inventar im Minecraft-Stil:** Eine Schnellleiste mit 9 Plätzen unten im Bild und ein Inventarfenster mit 27 weiteren Plätzen (Tab). Gleiche Items stapeln sich.
- **Health und Sanity:**
  - Die Sanity sinkt um 15 % pro Minute.
  - Bei 0 % färbt sich der Bildschirmrand zunehmend rot, mit leichtem Herzschlag, und ein Countdown erscheint.
  - Nach 15 Sekunden stirbt der Spieler.
  - Nach dem Tod lädst du den letzten Spielstand (volle Health, mindestens 30 % Sanity) oder beginnst eine neue Welt.
- **Autosave:** Alle 3 Minuten Spielzeit sowie beim Beenden. Gespeichert werden Seed, Position, Blickrichtung, Health, Sanity, aktive Effekte, Inventar, aufgenommene Items, Strecke und Spielzeit. Das Spiel schreibt zuerst in eine temporäre Datei und benennt sie dann um; der vorige Stand bleibt als `.bak` erhalten und wird bei einer beschädigten Datei automatisch geladen. Ablage: `%APPDATA%\LIMINAL\save_v3.json`.

## Steuerung

| Eingabe | Funktion |
|---|---|
| W / A / S / D | gehen |
| Maus | umsehen (Pfeiltasten als Alternative) |
| Shift | sprinten |
| **E** | Item aufnehmen (wenn `[E]` eingeblendet wird) |
| **F** | gewähltes Item der Schnellleiste benutzen |
| **1–9 / Mausrad** | Platz der Schnellleiste wählen |
| **Tab** | Inventar öffnen oder schließen |
| ESC | Menü (Einstellungen, Hilfe, Neue Welt, Beenden) |
| M | Karte |
| F3 oder I | Debug-Anzeige |
| V | Darstellung wechseln |
| **L** | Leistungsmodus (in V2 lag er auf F, das jetzt zum Benutzen dient) |
| B | Head-Bobbing |
| P | Screenshot |
| + / − | Laufgeschwindigkeit |

Im Inventar bewegst du dich mit den Pfeiltasten oder WASD. Enter nimmt einen Stapel auf oder legt ihn ab bzw. tauscht ihn, F benutzt ein Item, und 1–9 legt es auf diesen Platz der Schnellleiste.

Menü und Inventar pausieren das Spiel, auch den Sanity-Verlust. Alle übrigen Funktionen, Einstellungen und das Design entsprechen V2.

## Ordner

```
LIMINAL V3/
  LIMINAL.exe        Starter mit Icon (Version 3.0)
  runtime/           eigenständige Python-Laufzeit (3.14)
  liminal/           Spielcode (Python, nur Standardbibliothek)
    items.py         Item-Definitionen (Wirkung, 3D-Sprite, Icons)
    inventory.py     Inventar (36 Plätze, Stapel)
    stats.py         Health, Sanity, Effekte, Tod
    savegame.py      Autosave, abgesicherter Spielstand
    ui.py            Schnellleiste, Werte, Inventarfenster, Todesbildschirm
  liminal3d.py       Einstiegspunkt
  assets/            Icon
  build/             Build-Skripte (python build\build_exe.py)
  screenshots/       Beispielbild
```

Ohne EXE kannst du das Spiel auch mit Python ab 3.8 starten: `python liminal3d.py`.
