# LIMINAL V2

Ein First-Person-Erkundungsspiel, das komplett im Terminal läuft. Du gehst durch eine unendliche, prozedural erzeugte Welt aus Innenräumen: Büroflure, Betonhallen, unterirdische Tunnel, riesige leere Hallen und enge Wartungsgänge. Es gibt keine Gegner, kein Ziel und keine Punkte, nur Architektur, Licht und Nebel.

## Starten

Doppelklick auf **`LIMINAL.exe`**.

Du brauchst weder eine Python-Installation noch eine Entwicklungsumgebung. Die nötige Laufzeit liegt im Ordner `runtime`. Behalte den Ordner `LIMINAL V2` zusammen. Für den Desktop legst du am besten eine Verknüpfung auf `LIMINAL.exe` an, statt die EXE allein zu verschieben.

Optionen kannst du auch mitgeben, zum Beispiel über eine Verknüpfung oder die Konsole:

```bash
LIMINAL.exe --seed 4242
```

`LIMINAL.exe --demo` lässt die Welt selbstständig erkunden, bis du eine Taste drückst.

## Neu in V2

- **Maussteuerung:** Die Maus nach links oder rechts dreht dich, nach oben oder unten neigt sich der Blick. Eine einstellbare Glättung macht die Bewegung weich. Die Maus wird nur geführt, solange das Spielfenster aktiv ist; ESC gibt sie frei.
- **Menü (ESC):**
  - Mauseinstellungen: Empfindlichkeit, X- und Y-Faktor, Achsen invertieren, Glättung, Standardwerte
  - Grafik & Anzeige: Darstellung, weiche Beleuchtung, Leistungsmodus, Sichtfeld, Bildrate, Head-Bobbing, Laufgeschwindigkeit, Fenster maximieren
  - ausführliche Hilfe
  - Alle Einstellungen wirken sofort und werden in `%APPDATA%\LIMINAL\settings_v2.json` gespeichert.
- **Beleuchtung:**
  - Deutlich weniger Flackern: etwa 1 % statt 5 % defekte Lampen, dazu nur noch ein kaum sichtbares Pulsieren.
  - Defekte Röhren blenden weich ab und wieder auf, statt hart an und aus zu schalten.
  - Das Licht wird pro Pixel weich interpoliert, statt pro Kachel in Stufen zu springen.
  - Kontaktschatten in Ecken und am Wandfuß, weiche Schatten hinter Säulen und Regalen.
- **Breitere Gänge:** Die mittlere Gangbreite steigt von etwa 3,1 auf etwa 4,1 Kacheln, und es gibt keine 1 Kachel breiten Gänge mehr. Aufbau und Stimmung der Welt bleiben dieselben.
- **Höhere Auflösung:**
  - Die Halbblock-Grafik mit doppelter vertikaler Auflösung ist jetzt Standard.
  - Das Fenster wird beim Start maximiert.
  - Es werden nur Bildschirmzeilen neu gesendet, die sich verändert haben.
- **EXE mit eigenem Icon:** Das Icon zeigt den Blick in einen endlosen Flur mit leuchtender Tür, im Pixel-Look der Spielgrafik.

## Steuerung

| Eingabe | Funktion |
|---|---|
| W / S | vorwärts / rückwärts |
| A / D | seitlich gehen |
| Maus | umsehen (links/rechts drehen, hoch/runter schauen) |
| Pfeiltasten | umsehen ohne Maus |
| Shift | sprinten |
| + / − | Laufgeschwindigkeit |
| ESC | Menü öffnen oder schließen (pausiert das Spiel) |
| H | Hilfe im Menü |
| M oder Tab | Karte |
| F3 oder I | Debug-Anzeige |
| V | Darstellung: Halbblock-HiRes → ASCII → Mono |
| F | Leistungsmodus |
| B | Head-Bobbing |
| P | Screenshot als HTML-Datei (Ordner `screenshots`) |

Im Menü wählst du mit ↑ ↓ oder W S einen Eintrag, änderst Werte mit ← → oder A D, bestätigst mit Enter und gehst mit ESC eine Ebene zurück.

## Ordner

```
LIMINAL V2/
  LIMINAL.exe        Starter mit Icon
  runtime/           eigenständige Python-Laufzeit (3.14)
  liminal/           Spielcode (Python, nur Standardbibliothek)
  liminal3d.py       Einstiegspunkt
  assets/            liminal.ico, liminal_icon.png
  build/             Build-Skripte (Icon, Laufzeit, Starter)
  screenshots/       Beispielbilder (HTML)
```

Neu bauen (benötigt ein installiertes Windows-CPython; die EXE wird mit dem in Windows enthaltenen C#-Compiler erstellt):

```bash
python build\build_exe.py
```

Ohne EXE kannst du das Spiel auch mit jedem Python ab 3.8 starten: `python liminal3d.py`.

## Leistung

Gemessen mit `--benchmark` und Autopilot bei 200×56 Zeichen, ohne Terminalausgabe:

| Version | Darstellung | Zeit pro Frame |
|---|---|---|
| V1 | Halbblock | ca. 5,1 ms |
| V2 | Halbblock mit weicher Beleuchtung und Schatten | ca. 8,5 ms |

Beides liegt weit unter dem Frame-Budget bei 60 FPS. Meist begrenzt die Geschwindigkeit des Terminals die Bildrate. Wenn es ruckelt, hilft im Menü eine niedrigere Bildrate oder der Leistungsmodus.

## Hinweise

- Die Maussteuerung funktioniert in der Windows-Konsole und im Windows Terminal. In anderen Programmen, etwa einem in eine IDE eingebetteten Terminal, siehst du dich mit den Pfeiltasten um.
- Unter Linux und macOS liest das Spiel Mausbewegungen über die Mausmeldungen des Terminals. Am Fensterrand endet die Drehung dort.
