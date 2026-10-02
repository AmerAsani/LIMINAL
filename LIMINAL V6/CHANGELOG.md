# LIMINAL V6 – Änderungen gegenüber V5

V6 ist die bisher größte Überarbeitung von LIMINAL. Die Engine ist dieselbe (C++20, eigener Direct3D-11-Renderer, alles prozedural, keine Bild- oder Tondateien). Fast jedes System ist erweitert oder neu.

## Kurzfassung

- **Weitläufigere Welt:** halb so viele Räume je Fläche, doppelt so große Räume, breite Passagen, Lichthöfe, offene Übergänge, 50 % längere Sichtachsen
- **Räumlicher Klang:** Jede Leuchtstoffröhre summt einzeln und hörbar aus ihrer Richtung. Räume hallen passend zu Größe und Material. Wände dämpfen. Ferne Geräusche im Gebäude.
- **Neue Grafikgeneration:** GTAO, indirektes Licht, Lichtschächte mit Schatten, Kontaktschatten, Parallax-Oberflächen, Gebrauchsspuren, saubere Spiegelungen, neue Farbabstimmung
- **Grafik passt sich der Hardware an:** automatische Qualitätswahl per Messung, dynamische Auflösung, Texturkompression, Wahl der stärkeren GPU auf Laptops
- **Glaubwürdiger Getränkeautomat** mit Glasfront und echter Tiefe dahinter
- **Eigener Körper:** ein animiertes Strichmännchen in der Ego-Perspektive, dazu Schulterkamera (F5)
- **Runde Erkundungs-Minimap**, die nur zeigt, was du wirklich gesehen hast
- **Springen, Ausdauer, Items fallen lassen**, deutlich seltenere Energy Bars
- **Alte Spielstände bleiben gültig:** V3-, V4- und V5-Stände laden ihre bitgenau gleiche Welt.

---

## Welt

| Messwert (Level 0, 5 Seeds × 25 Sektoren) | V5 | V6 |
|---|---|---|
| Räume je Hektar | 99 | 51 |
| Median-Raumgröße | 36 m² | 80 m² |
| Fläche in Kleinsträumen (< 40 m²) | 15 % | 2,5 % |
| Weitläufige Bodenfläche (≥ 3 m zur Wand) | 33 % | 48 % |
| Längste Sichtachse (Mittel) | 20 m | 30 m |
| Versorgungsräume je Fläche | 100 % | 87 % |

- Größere Räume, breitere und höhere Flure, viel weniger Kleinsträume
- **Passagen** als Zwischenräume auf den Hauptachsen: Säulengang mit Nischen, hohe Wandelhalle oder breiter Gang
- **Fluchten:** Flure gehen offen ineinander über, große Hallen öffnen sich breit zum Flur.
- **Lichthöfe** (10–18 m hoch) und mehr Dunst in großen Bereichen: Wie weit ein Raum reicht, ist oft nicht sofort zu erkennen.
- Der neue Aufbau ist eine Daten-Überlagerung (`layout_v6` in den Leveldateien). Der alte Aufbau bleibt bitgenau erhalten und wird für alte Spielstände verwendet.

## Klang und Atmosphäre

- **Leuchten als einzelne 3D-Quellen:**
  - 100-Hz-Summen eines magnetischen Vorschaltgeräts (50-Hz-Netz) mit Obertönen, feinem Zischen der Röhre, langsamen Schwankungen und gelegentlicher Unruhe
  - vier Varianten; Industrieleuchten brummen tiefer, Notleuchten schnarren
  - Lautstärke nach Nähe, hörbare Richtung
  - flackernde Röhren knacken synchron zum Bild
  - dezent: etwa 64 % des alten, raumfüllenden Brummens
- **Raumakustik:**
  - Nachhallzeit nach Sabine aus Volumen, Fläche und Materialien (Teppich, Akustikdecke, Beton, Fliesen)
  - Nachhall-Bus nach Freeverb-Prinzip
- **Räumliches Hören:**
  - Laufzeitunterschied zwischen den Ohren
  - Kopfschatten für Quellen hinter dir
  - Luftdämpfung über die Entfernung
  - Verdeckung durch Wände: leiser und dumpfer, über das Kachelraster berechnet
- **Ferne Geräusche,** selten und zur Zone passend: Tür, Metall, Grollen, Klopfen, Dampf, Knarzen. Mit sinkendem Verstand häufiger.

## Grafik

- **GTAO** (Ground-Truth Ambient Occlusion) mit Mehrfachreflexion und Glanzlicht-Verdeckung, statt SSAO
- **Indirektes Licht** im Bildraum (ein Bounce)
- **Volumetrisches Licht** als eigener Durchgang: analytisches Streulicht je Leuchte, Schatten aus dem Höhenfeld (Lichtschächte durch Türen und zwischen Säulen), Abstrahlrichtung, treibender Dunst
- **Kontaktschatten** für Items, den eigenen Körper und feine Kanten
- **Parallax-Occlusion-Mapping:** echte Tiefe in Fugen, Mörtel, Gittern und Deckenrastern
- **Gebrauchsspuren**, weltfest:
  - Wasserflecken mit Rändern an Decken
  - Schmutz am Wandfuß und Staub in Ecken
  - Schrammen und Ruß
  - Wasserläufe an Wänden
  - feuchte Stellen im Teppich
- **Spiegelungen:** Streifenartefakte bei flachem Blick behoben (seit V5)
- **Staub in der Luft**, kontrastadaptives **Nachschärfen**
- **Farbabstimmung:** Split-Toning (kühle Schatten, warme Lichter), Halation, filmähnliches Korn
- **Getränke- und Snackautomat** als eigenes Modell:
  - Lackgehäuse mit Chromrahmen und Leuchtschild
  - Bedienfeld mit LED-Anzeige, Tastenfeld, Münzschlitz und Geldscheinprüfer, Ausgabeklappe
  - Kratzer, Rost, Aufkleberrest
  - **Glasfront mit Interior Mapping:** Fächer mit Flaschen, Dosen und Snacks, die sich mit dem Blickwinkel verschieben; die Scheibe spiegelt den Raum.
- Kisten aus verwittertem Fichtenholz, Theken mit echtem Furnier

## Leistung und Skalierung

- **Automatische Qualität** bei Neuinstallation:
  - Schätzung aus der Grafikkarte, dann Messung der echten GPU-Zeit im Hauptmenü
  - Gewählt wird die höchste Stufe mit Reserve für 60 FPS.
  - Gemessen wird neu, wenn sich Grafikkarte oder Auflösung ändern.
- **Stärkere GPU auf Laptops:** Bei zwei Grafikchips nimmt LIMINAL jetzt die leistungsstarke GPU (DXGI 1.6). Vorher lief es oft auf der integrierten Grafik. `--gpu sparsam` wählt die sparsame.
- **Dynamische Auflösung** hält die Ziel-Bildrate.
- **Stufen** Niedrig / Mittel / Hoch / Ultra nach Aufwand, nicht nach einem bestimmten PC. Ultra darf teuer sein.
- **Neue Optionen:** Beleuchtungsqualität (Zahl gleichzeitig gerechneter Leuchten), Partikel, Nachbearbeitung, Fenstergröße, Texturkompression
- **Texturkompression** BC1/BC5: etwa 45 % weniger Grafikspeicher. Große Texturen sind auf Karten mit wenig Speicher gesperrt.
- **GPU-Zeitmessung** je Durchgang in der Debug-Anzeige (F3) und im Benchmark
- Messwerte (1920 × 1080, GPU-Zeit je Bild):

  | Grafikkarte | Niedrig | Mittel | Hoch | Ultra |
  |---|---|---|---|---|
  | NVIDIA GeForce RTX 5070 Laptop | – | – | 4,4 ms | 7,6 ms |
  | Intel Arc 140T (integriert) | 4,1 ms | 9,5 ms | 11,4 ms | – |

## Spiel

- **Strichmännchen-Körper** in der Ego-Perspektive:
  - prozedurale Animation für Gehen, Sprinten, Springen, Fallen und Landen
  - Hände greifen zu und führen Items zum Mund.
  - Die Kamera sitzt im Kopf.
- **Schulterkamera** mit Wandkollision (F5)
- **Runde Minimap**, die nur Erkundetes zeigt (Sichtlinien durch das Kachelraster), und große Karte (M). Das Erkundete wird gespeichert.
- **Springen** mit Nachsicht an Kanten. Treppen hinab ohne Mini-Stürze.
- **Ausdauer:** begrenzter Sprint, Erschöpfung, Keuchen
- **Items fallen lassen** (Q, Strg+Q für den ganzen Stapel). Sie bleiben in der Welt liegen und werden gespeichert.
- **Energy Bars** nur noch etwa ein Drittel so häufig, dafür stärker und als seltener Fund markiert. Die Sanity je Versorgungsraum bleibt gleich (geprüft).

## Oberfläche und Komfort

- gestapelte, weich eingeblendete Meldungen
- Aufnehmen-Hinweis mit echter Tastenbelegung
- Überblendungen beim Start, Laden und Wechsel ins Hauptmenü
- neues Grafikmenü mit Hilfetexten zu jeder Option

## Behobene Fehler

- Streifen in Bodenspiegelungen bei flachem Blick
- Treppen hinab: Die Figur fiel jede Stufe einzeln (mit Landegeräusch).
- Zonennamen mit Umlauten wurden falsch großgeschrieben.
- Die Kartenaktualisierung hing von der Bildrate ab.
- Beim Fortsetzen alter Spielstände wurde „(V4)“ Teil des Namens.
- Auf Laptops mit zwei Grafikchips lief das Spiel auf der schwächeren GPU.

## Kompatibilität

- Spielstände aus V3, V4 und V5 werden gefunden und als V6-Stand fortgeführt. Ihre Welt bleibt bitgenau gleich.
- Neue Spiele nutzen den V6-Aufbau. Der Spielstand merkt sich das (`"layout": 6`).
- Eigene Dateien: `settings_v6.json`, `saves_v6\`, `liminal_v6.log`. Beim ersten Start werden die V5-Einstellungen übernommen (nur lesend). V5 und V6 lassen sich parallel spielen.

## Für Entwickler

- **Neue Module:**
  - `app/soundscape`: Leuchten, Raumakustik, ferne Klänge
  - `app/autotune`: Grafik nach Hardware
  - `gameplay/body`: Strichmännchen
  - `gameplay/explore`: Erkundungskarte
  - `render/gpu_timer`, `render/texture_compress`, `render/texture_vending`
- **Neue Tests:**
  - `audio_test`: Klang ohne Audiogerät
  - `layout_stats`: Weltstruktur alt gegen neu
  - `supply_balance_test`: Item-Balance, beide Weltstrukturen
  - `gameplay_test`: Sprung, Treppen, Karte, Körper, Ausdauer, Ablegen
- `worldgen_test` prüft weiterhin, dass der alte Aufbau bitgenau der V4-Welt entspricht.
- **Leveldaten:** `layout_v6` überlagert Zonen und Raumtypen, ergänzt Raumtypen und steuert Passagen und offene Übergänge (`structure`). Details in der README unter *Erweitern → Neues Level*.
