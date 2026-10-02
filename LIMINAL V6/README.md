# LIMINAL V6

Ein First-Person-Erkundungsspiel in einer unendlichen, prozedural erzeugten Welt aus Innenräumen: Büroflure, Betonhallen, unterirdische Tunnel, riesige leere Hallen und enge Wartungsgänge. Deine Sanity schwindet mit der Zeit, und nur seltene Vorräte halten dich bei Verstand.

V6 ist die große Überarbeitung von V5 mit derselben Engine (**C++20**, eigener **Direct3D-11-Renderer**). Neu sind:

- eine **weitläufigere Weltstruktur** mit Passagen, Lichthöfen und langen Sichtachsen
- **räumlicher Klang**: Leuchtstoffröhren summen einzeln, Räume hallen, ferne Geräusche
- ein glaubwürdiger **Getränke- und Snackautomat**
- eine **neue Grafikgeneration** (GTAO, indirektes Licht, Lichtschächte, Parallax-Oberflächen, Gebrauchsspuren), die sich **automatisch an die Hardware anpasst**
- runde Erkundungs-Minimap, sichtbarer Strichmännchen-Körper
- Springen, Ausdauer, fallen gelassene Items, seltenere Energy Bars

Spielstände aus V5 und V4 lassen sich weiterspielen und behalten ihre bitgenau gleiche Welt. Neue Spiele nutzen den neuen Aufbau.

---

## Neu in V6

### Weltstruktur: weiter, offener, glaubwürdiger
Die V5-Welt bestand oft aus vielen sehr kleinen Räumen, dicht an dicht und durch unzählige Türen verbunden. Neue Spiele nutzen einen **weitläufigeren Aufbau**. Er ist in den Leveldateien als Abschnitt `layout_v6` beschrieben, reine Daten plus wenige Generatorregeln.
- **Größere Räume, breitere und höhere Flure**, deutlich weniger Kleinsträume
- **Passagen**: Die Hauptachsen eines Sektors werden zu breiten Zwischenräumen mit Räumen zu beiden Seiten, in drei Varianten:
  - Säulengang mit Nischen zwischen den Säulen
  - hohe Wandelhalle mit vereinzelten Säulen
  - breiter, schlichter Gang
- **Fluchten statt Türen**: Flure gehen offen ineinander über, große Hallen öffnen sich breit zum Flur.
- **Lichthöfe**: sehr hohe Hallen mit dicken Pfeilern
- **Längere Sichtachsen** und mehr Dunst in den großen Zwischenräumen: Oft ist nicht sofort zu erkennen, wie weit ein Bereich wirklich reicht.
- Gemessen mit `layout_stats` (5 Seeds × 25 Sektoren, Level 0):

  | Messwert | V5 | V6 |
  |---|---|---|
  | Räume je Hektar | 99 | 51 |
  | Median-Raumgröße | 36 m² | 80 m² |
  | Fläche in Kleinsträumen | 15 % | 2,5 % |
  | Weitläufige Bodenfläche (≥ 3 m zur nächsten Wand) | 33 % | 48 % |
  | Längste Sichtachse im Mittel | 20 m | 30 m |
  | Versorgungsräume je Fläche | 100 % | 87 % (ähnlich häufig) |

- **Alte Spielstände** (V3/V4/V5 und frühe V6-Stände) laden ihre Welt im alten Aufbau, bitgenau wie zuvor (`worldgen_test`). Neue Spielstände merken sich den Aufbau (`"layout": 6`).

### Klang und Atmosphäre
- **Leuchtstoffröhren summen einzeln und räumlich**:
  - Jede Leuchte in der Nähe ist eine eigene Klangquelle.
  - Der Klang folgt einem magnetischen Vorschaltgerät am 50-Hz-Netz: tiefes 100-Hz-Summen mit Obertönen, ein Rest 50 Hz, feines Zischen der Röhre im Takt der Halbwellen, langsame Pegelschwankungen und gelegentlich eine winzige Unruhe.
  - Es gibt vier Varianten. Industrieleuchten brummen tiefer, Notleuchten schnarren.
  - Die Lautstärke hängt von der Nähe ab, die Richtung ist hörbar. Hinter Wänden klingt das Summen dumpf und leise.
  - Flackernde, alte Röhren knacken beim Aussetzen, synchron zum sichtbaren Flackern.
  - Pegel: etwa 64 % des alten, raumfüllenden Brummens – dezent.
- **Raumklang**: Die Nachhallzeit wird nach Sabine aus Volumen, Fläche und Materialien des Raums berechnet. Teppich und Akustikdecke schlucken, Beton und Fliesen hallen. Schritte, Items und Leuchten klingen in großen Hallen lang nach, in Büros trocken.
- **Räumliches Hören**: Laufzeitunterschied zwischen den Ohren, Kopfschatten für Quellen hinter dir, Luftdämpfung über die Entfernung, Verdeckung durch Wände (über das Kachelraster berechnet).
- **Ferne Geräusche**, sehr selten und zur Zone passend: eine schwere Tür, Metall, ein tiefes Grollen, Klopfen, Dampf, Knarzen. Sie klingen weit weg, gedämpft und verhallt. Je weniger Verstand, desto öfter.
- Geprüft ohne Audiogerät mit `audio_test`: Richtung, Laufzeit (0,6 ms), Verdeckung (−14 dB, dumpfer), Nachhall, nahtlose Schleifen, Pegel.

### Automaten und Objekte
- Der **Getränke- und Snackautomat** ist jetzt ein eigenes Modell statt eines leuchtenden Blocks:
  - dunkelrotes Lackgehäuse mit Fasen und Chromrahmen
  - Leuchtschild „ERFRISCHUNG“
  - Bedienfeld mit LED-Anzeige („WÄHLEN 0,00 €“), abgegriffenem Tastenfeld, Münzschlitz, Rückgabeknopf, Geldscheinprüfer und Rückgabeschale
  - Ausgabeklappe „HIER ENTNEHMEN“, Lüftungsgitter
  - Gebrauchsspuren: Kratzer, abgeplatzter Lack an Kanten, Schmutz zum Boden hin, Rost, Aufkleberrest, Fingerabdrücke
- **Glasfront mit echter Tiefe** (Interior Mapping): Hinter der Scheibe liegen fünf beleuchtete Fächer mit Flaschen, Dosen und Snacks hinter Spiralen, Preisleisten und einzelnen leeren Fächern. Die Fächer verschieben sich mit dem Blickwinkel. Die Scheibe spiegelt Leuchten und Raum.
- **Kisten** aus verwittertem Fichtenholz statt grellem Orange. **Theken** mit Furnier, Maserung, Ästen und Paneelfugen statt Sinuswellen.

### Grafik skaliert mit der Hardware
LIMINAL soll auf möglichst vielen PCs flüssig laufen und auf starken PCs möglichst gut aussehen.
- **Automatische Qualität** (Standard bei einer Neuinstallation):
  - Das Spiel schätzt aus der Grafikkarte (eigener Speicher, integriert oder dediziert, Software) eine Startstufe.
  - Im Hauptmenü misst es einige Sekunden die echte GPU-Zeit (ohne VSync) und wählt die höchste Stufe, die bei 60 FPS Luft lässt. Ultra nur mit großer Reserve.
  - Reicht selbst „Niedrig“ nicht, sinkt die interne Auflösung, und die dynamische Auflösung wird eingeschaltet.
  - Nach einem Wechsel der Grafikkarte oder der Auflösung misst es neu.
- **Leistungsstärkste Grafikkarte**: Auf Laptops mit zwei Grafikchips wählt LIMINAL jetzt die starke GPU (DXGI 1.6). Vorher lief es dort oft auf der integrierten Grafik. `--gpu sparsam` erzwingt die sparsame.
- **Dynamische Auflösung** (optional): Bei Last sinkt die interne Auflösung kurz, bei Reserve steigt sie wieder – mit Trägheit, damit nichts pumpt.
- **Stufen**: Niedrig für schwache und ältere Grafikchips, Mittel für normale PCs, Hoch für aktuelle Grafikkarten, Ultra für leistungsstarke PCs (darf teuer sein).
- Neue Einstellungen: Beleuchtungsqualität (gleichzeitig gerechnete Leuchten und Reichweite), Partikelmenge, Nachbearbeitung, Fenstergröße, Texturkompression.
- **Texturkompression** (BC1 für Farbe, BC5 für Normalen): etwa 45 % weniger Grafikspeicher. 2048er-Texturen sind auf Grafikkarten mit weniger als 3 GB eigenem Speicher gesperrt.
- Gemessen mit `--benchmark` (900 Bilder, 1920 × 1080; GPU-Zeit je Bild):

  | Grafikkarte | Niedrig | Mittel | Hoch | Ultra (2400 × 1350 intern) |
  |---|---|---|---|---|
  | NVIDIA GeForce RTX 5070 Laptop | – | – | 4,4 ms | 7,6 ms |
  | Intel Arc 140T (integriert) | 4,1 ms (1440 × 810 intern) | 9,5 ms | 11,4 ms | – |

  Die Automatik wählte auf der RTX 5070 „Ultra“ (3,5 ms bei Hoch gemessen) und auf der Arc 140T „Mittel“ (12,0 ms gemessen).

### Minimap
- **Rund**, oben rechts, kompakt (verdeckt wenig vom Spiel). Dreht sich mit dem Blick oder zeigt Norden oben (Einstellung).
- **Zeigt nur, was du erkundet hast.** Zu Beginn ist sie leer. Beim Gehen deckt das Spiel die Umgebung per Sichtlinien auf: Strahlen laufen durch das Kachelraster und enden an Wänden und hohen Einbauten. Ein Raum hinter einer Wand bleibt unbekannt, durch eine offene Tür sieht man ein Stück hinein. Unbekanntes wird nicht abgedunkelt, sondern ist schlicht nicht da.
- **Bleibt erhalten:** Aufgedecktes bleibt sichtbar, auch wenn der Bereich längst entladen ist, und wird im Spielstand gespeichert (nur Bitmasken je Chunk; das Aussehen entsteht beim Laden neu aus der Welt).
- Grundriss-Darstellung: Boden in gedämpften Zonenfarben, Wände als helle Kontur, Durchgänge orange, Treppen gestreift, Einbauten dunkler, Automaten pink, Items gelb (seltene pulsieren). Pfeil und Sichtkegel zeigen Position und Blickrichtung, »N« markiert Norden. Darunter steht die aktuelle Zone.
- **M** öffnet die große Karte (Norden oben, Legende, erkundete Fläche in m²).

### Eigener Körper (Ego-Perspektive)
- Du bist ein absichtlich simples, leicht albernes **Strichmännchen** – schwarz wie mit dem Filzstift gezogen: runder Kopf mit hellen Punktaugen und Grinsen, Strich-Körper, Arme und Beine, die einfach als Strich enden, kurze Strich-Füße.
- **Prozedural animiert** und direkt an die Bewegung gekoppelt: Beim Gehen setzen die Füße genau mit dem Schrittgeräusch auf, die Arme schwingen gegengleich, das Becken wippt. Sprinten: weite Schritte, angewinkelte Arme, Oberkörper nach vorn. Springen: Arme fliegen hoch („Juhu!“), Knie ziehen an. Fallen: Arme rudern, Beine strampeln. Landen: die Knie federn ein.
- **Hände greifen zu:** Beim Aufnehmen greift die Hand zum Item, beim Benutzen führt sie es vor den Mund.
- Die **Kamera sitzt im Kopf**: Auge, Nackengelenk und Wirbelsäule bestimmen die Kameraposition, Landungen und Gangbewegung übertragen sich auf die Sicht. Schaust du nach unten, siehst du Hände, Beine und Füße; Schnellleiste und Werte treten dabei dezent zurück.
- **F5** schaltet auf eine Schulterkamera (mit Wandkollision), um das Strichmännchen ganz zu sehen.
- Technisch: eigene Netze und Materialien, TAA rechnet die Spielerbewegung für Körperpixel heraus (kein Verschmieren beim Drehen), kameranahe Teile werden weich ausgeblendet.

### Bewegung und Kamera
- **Springen** (Leertaste / Gamepad B), etwa 0,6 m hoch, mit kurzer Nachsicht an Kanten und gemerkter Taste kurz vor der Landung. Unter niedrigen Decken stößt der Kopf an, statt festzuhängen.
- **Treppen hinab** werden jetzt sauber abgegangen. In V5 fiel die Figur jede Stufe einzeln hinunter (mit Landegeräusch je Stufe).
- Die Kamera folgt Stufen weich, schwankt beim Seitwärtsgehen minimal mit und weitet das Sichtfeld beim Sprinten leicht. Head-Bobbing hat seinen tiefsten Punkt jetzt genau beim Aufsetzen des Fußes. Taste B schaltet alle Kamerabewegungen ab.
- Schritte zählen nur noch am Boden (nicht mehr in der Luft).

### Energy Bars sind selten
- Die Item-Auswahl der Level bleibt V4-kompatibel. Danach greift eine neue, datengesteuerte Seltenheitsregel (`spawn` in `data/items.json`): Ein Energy Bar bleibt nur zu 30 % erhalten. Sonst wird er zu 45 % zu Mandelwasser, ansonsten bleibt der Platz leer. Die Regel ist deterministisch je Seed. Weltaufbau, Item-IDs und alte Spielstände bleiben gültig.
- Dafür ist ein Energy Bar ein **seltener Fund**: +30 % Sanity und +30 % Tempo für 75 s (vorher +20 % / 60 s). Er schimmert leicht auf der Theke, klingt beim Aufnehmen besonders und wird mit „Seltener Fund!“ gemeldet.
- **Balance geprüft** (`supply_balance_test`, 8 Seeds × 49 Sektoren, beide Level, alle Schwierigkeitsgrade): Energy Bars kommen nur noch zu etwa 31–35 % der V5-Menge vor. Die Sanity, die ein Versorgungsraum im Mittel liefert, bleibt im Rahmen von ±2 % gleich.

### Ausdauer und Items fallen lassen
- **Sprinten kostet Ausdauer** (schmale Leiste zwischen Health und Sanity, nur sichtbar, wenn sie nicht voll ist). Je nach Schwierigkeit reicht sie für 9 / 7 / 5,5 s Sprint, ein Sprung kostet etwas. Ist sie leer, bist du **erschöpft**: kein Sprint, etwas langsamer, schweres Atmen, das Strichmännchen keucht und lässt die Schultern hängen. Sprinten geht erst wieder ab 30 %. Ein Energy Bar füllt die Ausdauer ganz auf.
- **Q** lässt das gewählte Item fallen (**Strg + Q** den ganzen Stapel), auch im Inventar. Das Item wird kurz nach vorn geworfen, springt auf und bleibt liegen – mit Aufprallgeräusch. Fallen gelassene Items werden im Spielstand gespeichert und liegen beim nächsten Besuch noch dort.

### Grafik: neue Generation
Alle Verfahren sind echt berechnet, nichts davon ist aufgemalt. Der Look bleibt LIMINAL: gelbliches Neonlicht, gedämpfte Farben, Dunst, Stille.
- **GTAO** (Ground-Truth Ambient Occlusion) ersetzt SSAO: Die Horizonte zu beiden Seiten werden im Tiefenpuffer gesucht und die kosinusgewichtete Sichtbarkeit wird analytisch integriert. Dazu kommen Mehrfachreflexion (helle Flächen werden in Ecken weniger dunkel) und Glanzlicht-Verdeckung (Ecken spiegeln nicht mehr).
- **Indirektes Licht:** ein Licht-Bounce im Bildraum. Jede verdeckende Fläche gibt ihr Licht aus dem letzten Bild weiter: gelbe Wände tönen Böden, Lichtkegel hellen Ecken auf.
- **Volumetrisches Licht** ist jetzt ein eigener Durchgang mit echten Schatten. Das Streulicht jeder Leuchte wird analytisch entlang des Blickstrahls integriert und an mehreren Punkten gegen das Höhenfeld der Welt verschattet. So entstehen Lichtschächte durch Türen, zwischen Säulen und Regalen, Deckenleuchten strahlen nach unten, und der Dunst treibt langsam. Es gibt drei Stufen.
- **Kontaktschatten:** ein kurzer Strahl durch den Tiefenpuffer für das, was das Höhenfeld nicht kennt – Items, den eigenen Körper, feine Kanten.
- **Parallax-Occlusion-Mapping:** Fugen, Mörtel, Gitter, Deckenraster und Holzdielen haben echte Tiefe, die sich mit dem Blickwinkel verschiebt und gegenseitig verdeckt. Grundlage sind die echten Höhendaten der prozeduralen Texturen. Es wirkt in der Nähe und blendet mit der Entfernung weich aus.
- **Gebrauchsspuren**, weltfest und je Seed gleich, aus der Raumgeometrie abgeleitet:
  - Wasserflecken mit braunen Wasserrändern an den Decken
  - Schmutzkante am Wandfuß, Staub in Bodenecken und an Einbauten
  - Schrammen auf Hüfthöhe, Ruß unter der Decke
  - seltene Wasserläufe an Wänden
  - feuchte, leicht glänzende Stellen im Teppich, abgelaufene Laufwege
- **Spiegelungen (SSR) überarbeitet:** Die Tiefe wird punktgenau gelesen, und jeder Durchgang hinter eine Oberfläche wird verfeinert, bevor entschieden wird. Die Streifen in Bodenspiegelungen bei flachem Blick (seit V5) sind behoben.
- **Staub in der Luft:** feine Partikel um die Kamera, beleuchtet von den nächsten Leuchten. Sichtbar vor allem im Lichtkegel.
- **Nachschärfen** (kontrastadaptiv, nach AMD CAS) gegen die Weichheit von TAA.
- **Farbabstimmung:**
  - Split-Toning: Schatten leicht kühl-grünlich wie das Streulicht von Neonröhren, Lichter warm, Mitteltöne neutral
  - leichte Halation um Leuchten
  - Filmkorn, das wie bei Film in den Mitteltönen am stärksten ist
- **Leistung:** Der Durchgang mit AO und indirektem Licht und die Volumetrie rechnen in reduzierter Auflösung und werden tiefenbewusst hochskaliert. Messwerte stehen oben unter *Grafik skaliert mit der Hardware*. Die Debug-Anzeige (F3) und der Benchmark zeigen die GPU-Zeit jedes Durchgangs. Der G-Buffer braucht 0,4 ms (RTX 5070) bis 1,3 ms (Arc 140T). Die Geometrie ist also nicht der Engpass, zusätzliche LOD-Stufen oder Occlusion Culling brächten keinen messbaren Gewinn. Der wirksame Hebel in großen offenen Bereichen ist die Zahl der Leuchten, siehe Einstellung *Beleuchtung*.

### Oberfläche und Qualität
- Meldungen erscheinen gestapelt (bis zu drei), werden weich ein- und ausgeblendet und gehen nicht mehr verloren.
- Der Aufnehmen-Hinweis zeigt die echte Belegung (z. B. `[E]` oder am Gamepad `[A]`) und blendet weich. Das Fadenkreuz wächst leicht, wenn etwas aufgenommen werden kann.
- Weiche Überblendung aus Schwarz beim Spielstart, Laden und Hauptmenü. Die große Karte öffnet animiert.
- »Gespeichert« erscheint jetzt unten rechts, oben rechts sitzt die Minimap.
- Neue Einstellungen unter *Grafik & Anzeige*: Perspektive, eigener Körper, Minimap, Minimap-Ausrichtung (dazu die Leistungs- und Grafikoptionen oben).
- Behobene Fehler:
  - Zonennamen mit Umlauten wurden falsch großgeschrieben („BüROFLURE“).
  - Die Kartenaktualisierung hing von der Bildrate ab.
  - Beim Fortsetzen alter Spielstände wurde „(V4)“ Teil des Namens.
- Debug-Anzeige (F3) mit Bildzeit (Spiel/Grafik), erkundeter Fläche, Kamera- und Sprungzustand. Der Benchmark protokolliert langsame Bilder samt Ursache.
- V6 hat **eigene Dateien** (`settings_v6.json`, `saves_v6\`, `liminal_v6.log`) und übernimmt V5-Einstellungen beim ersten Start. V5 bleibt unangetastet und kann parallel gespielt werden.

---

## Starten

Doppelklick auf **`LIMINAL.exe`**.

Der Starter prüft zuerst, ob der PC alles Nötige hat:

- Windows-Version
- Direct3D 11
- Microsoft C-Laufzeit
- Vollständigkeit des Spielordners

Danach startet er `LiminalGame.exe`. Fehlt etwas, erklärt er verständlich, was zu tun ist. Wo es geht, bietet er die **offizielle** Installation von Microsoft an:

- Er lädt nur von `aka.ms` / microsoft.com.
- Er prüft die digitale Signatur der heruntergeladenen Datei.
- Er installiert erst, nachdem du zugestimmt hast.

Einen versteckten Download oder eine versteckte Installation gibt es nicht.

**Voraussetzungen**

| | |
|---|---|
| System | Windows 10 oder 11, 64 Bit |
| Grafik | Direct3D 11 (Feature-Level 11.0), aktueller Treiber. Ohne passende Grafikkarte gibt es einen (langsamen) Software-Modus. |
| Laufzeit | Universal C Runtime (in Windows 10/11 enthalten) |

Das Spiel ist statisch gebaut und braucht keine zusätzlichen DLLs. Es nutzt nur Windows-Systembibliotheken.

---

## Neu in V5 (Grundlage von V6)

### Technik
- **C++20** statt Python.
- **Modularer Aufbau:** Die Engine besteht aus Modulen mit klaren Abhängigkeiten, siehe [Architektur](#architektur).
- **Streaming auf mehreren Kernen:** Sektoren, Räume und Chunk-Netze entstehen in einem Job-System auf Worker-Threads. Der Hauptthread übernimmt nur die fertigen Ergebnisse.
- **Datengesteuert:** Level, Zonen, Raumtypen, Lichtstile, Items, Schwierigkeiten, Tastenbelegung und Prefabs (Entities) liegen als JSON unter `data/`.

### Grafik
- **Deferred Rendering mit PBR:** GGX/Lambert. Alle Leuchten sind Flächenlichter.
- **Beleuchtung pro Kachel:** Ein Compute-Shader teilt das Bild in Kacheln (16 × 16 Pixel) und berechnet für jede nur die Lichter, die sie betreffen (bis zu 128 je Kachel).
- **Weiche Schatten aller Lampen.** Grundlage ist die Kachelwelt als Höhenfeld: Kein Licht dringt durch Wände, Treppen werfen korrekte Schatten.
- **Weitere Effekte:**
  - SSAO (Kontaktschatten)
  - Spiegelungen auf glatten Böden (SSR)
  - Lichtschein im Dunst (volumetrisch)
  - Bloom
  - automatische Belichtung
  - AgX-Tonemapping
  - Filmkorn, Vignette, chromatische Aberration
- **TAA (temporales Anti-Aliasing):** glättet Kanten und die weichen Halbschatten.
- **Prozedurale PBR-Texturen** bis 2048 × 2048: Tapeten, Teppich, Fliesen, Beton, Ziegel, Paneele, Gitter. Mit Mipmaps, Normal- und Rauheitskarten und großflächiger Variation gegen sichtbare Wiederholung.
- **Retro-Darstellungen aus V4** als Filter (Taste **V**): Terminal, ASCII, Monochrom.
- **Leistung:** Auf einer RTX 3070 bei 1920 × 1080 mit allen Effekten im Mittel **2,1 ms pro Bild (≈ 470 FPS)**. Das langsamste Bild braucht 3,4 ms (Benchmark über 3000 Bilder mit rund 180 Lichtern).
  - Nur ein Draw-Call pro Chunk.
  - Frustum-Culling, Lichtauswahl nach Entfernung, Tiefenvorsortierung.
  - Puffer werden von Bild zu Bild wiederverwendet statt neu angelegt.

### Spiel
- **Neues Level:** *Level 1 – Das Tiefgeschoss* (Kellergänge, Betonkeller, Technikgeschoss). Es besteht nur aus Daten, siehe `data/levels/level1.json`. Das Level wählst du unter *Neues Spiel*.
- **Klang:** Alle Klänge werden prozedural erzeugt.
  - Brummen der Leuchtstoffröhren, Automaten, Schritte je Untergrund, Herzschlag
  - Ambience je Zone
  - positionsabhängig (Stereo), latenzarm über WASAPI
- **Gamepad-Unterstützung** (XInput), frei belegbare Tasten in `data/input.json`.
- **Spielstände:** Spielstände mit Vorschaubild, abgesichert gegen Abstürze (temporäre Datei + `.bak`).
- **Fehlerbehandlung:** Bei einem Absturz schreibt das Spiel einen Fehlerbericht (Minidump).

---

## Steuerung

| Eingabe | Funktion |
|---|---|
| W / A / S / D | gehen |
| Maus (Pfeiltasten als Alternative) | umsehen |
| Shift | sprinten |
| **Leertaste** | springen |
| + / − | Laufgeschwindigkeit |
| **E** | Item aufnehmen |
| **F** | gewähltes Item der Schnellleiste benutzen |
| **Q** / Strg + Q | gewähltes Item fallen lassen / ganzen Stapel |
| **1–9 / Mausrad** | Platz der Schnellleiste wählen |
| **Tab** | Inventar |
| M | große Karte (die Minimap ist immer eingeblendet) |
| F5 | Ego-Perspektive / Schulterkamera |
| ESC | Menü (Einstellungen, Hilfe, Speichern, Hauptmenü) |
| H oder F1 | Hilfe |
| F3 oder I | Debug-Anzeige |
| V | Darstellung: Modern / Terminal / ASCII / Monochrom |
| L | Leistungsmodus (halbe interne Auflösung) |
| B | Head-Bobbing |
| P oder F12 | Screenshot (PNG in *Bilder\LIMINAL*) |
| Alt + Enter | Vollbild / Fenster |

**Gamepad**

| Eingabe | Funktion |
|---|---|
| linker Stick | gehen |
| rechter Stick | umsehen |
| L3 | sprinten |
| B | springen |
| R3 | Ego-Perspektive / Schulterkamera |
| A | aufnehmen |
| X | benutzen |
| Steuerkreuz unten | Item fallen lassen |
| Y | Inventar |
| LB / RB | Schnellleiste |
| Start | Menü |
| Back | Karte |

Alle Tasten lassen sich in `data/input.json` ändern.

---

## Einstellungen

Du findest sie im Menü (ESC → Einstellungen). Die Einstellungen werden in `%APPDATA%\LIMINAL\settings_v6.json` gespeichert. Beim ersten Start übernimmt V6 die Werte aus V5 (`settings_v5.json`, nur lesend), sonst aus V3/V4 (`settings_v3.json`), soweit sie passen.

- **Grafik & Anzeige:**
  - Grafikqualität: **Automatisch** (misst die Leistung und wählt die Stufe) / Niedrig / Mittel / Hoch / Ultra
  - V6 Leistung: dynamische Auflösung, interne Auflösung (50–150 %), Fenstergröße, Bildrate (max.), VSync
  - Beleuchtung (Niedrig / Mittel / Hoch / Ultra: 160 / 384 / 1024 / 2048 gleichzeitig gerechnete Leuchten)
  - Schatten, Umgebungsverdeckung (Aus / GTAO / GTAO hoch), indirektes Licht, Kontaktschatten, Spiegelungen (SSR), volumetrisches Licht (Aus / Niedrig / Mittel / Hoch)
  - Oberflächendetails (Parallax + Gebrauchsspuren), Texturqualität, Texturkompression, anisotrope Filterung, Sichtweite
  - Partikel (Aus / Wenig / Normal / Viel), Kantenglättung (TAA)
  - Nachbearbeitung (Aus / Dezent / Voll) sowie einzeln: Bloom, Bildschärfe, Filmkorn, Vignette, chromatische Aberration
  - Helligkeit, Sichtfeld, Darstellung, Vollbild, Head-Bobbing, Laufgeschwindigkeit
  - Perspektive (Ego / Schulterkamera), eigener Körper, Minimap, Minimap-Ausrichtung (dreht mit / Norden oben)
- **Maus & Steuerung:** Empfindlichkeit, Achsen getrennt, Invertieren, Glättung, Gamepad-Empfindlichkeit.
- **Audio:** Gesamt, Effekte, Atmosphäre (Zonenklang, Leuchten, ferne Geräusche).

Eine Neuinstallation startet mit **Automatisch**. Wer V6 schon vor dieser Automatik gespielt hat, behält seine gewählte Stufe und kann *Automatisch* im Menü wählen.

**Nur in der Datei:** Folgende Feinwerte lassen sich nur in der Einstellungsdatei ändern:

- `ambient_strength`, `light_intensity`
- `fog_brightness`, `fog_density`
- `ao_direct`
- `exposure_auto`, `exposure_base`, `bloom_strength`

Ungültige Werte werden beim Laden auf sinnvolle Bereiche begrenzt.

---

## Spielstände und Kompatibilität

| Was | Wo |
|---|---|
| Spielstände V6 | `%APPDATA%\LIMINAL\saves_v6\` (JSON mit Vorschaubild und erkundeter Karte, `.bak` als Sicherung) |
| Spielstände V5 | `%APPDATA%\LIMINAL\saves_v5\`. Erscheinen in der Liste mit „(V5)“, werden nur gelesen und beim Speichern als V6-Stand fortgeführt. Die Minimap beginnt dann leer. |
| Spielstände V4 | `%APPDATA%\LIMINAL\saves_v4\`. Erscheinen mit „(V4)“, ebenfalls nur lesend. |
| Spielstand V3 | `%APPDATA%\LIMINAL\save_v3.json`. Wird ebenfalls gefunden. |
| Einstellungen | `%APPDATA%\LIMINAL\settings_v6.json` |
| Screenshots | `Bilder\LIMINAL\` |
| Protokoll | `%LOCALAPPDATA%\LIMINAL\logs\liminal_v6.log` |
| Benchmark | `%LOCALAPPDATA%\LIMINAL\benchmark_v6.txt` |
| Fehlerberichte | `%LOCALAPPDATA%\LIMINAL\crash\liminal_v6_*.dmp` |

**Weltstruktur:** Spielstände merken sich ihren Aufbau. Neue V6-Spiele speichern `"layout": 6`. Stände ohne diesen Eintrag (V3, V4, V5 und V6-Stände von vor dem neuen Aufbau) laden die Welt im alten Aufbau, genau wie gespeichert.

V6 verändert keine Dateien von V3, V4 oder V5. Mit der Umgebungsvariable `LIMINAL_USER_DIR` lässt sich der Benutzerordner umlegen, z. B. für eine portable Installation auf einem USB-Stick.

---

## Kommandozeile

Alle Optionen gelten für `LIMINAL.exe` und `LiminalGame.exe`.

| Option | Wirkung |
|---|---|
| `--seed N` `--name X` `--difficulty easy\|medium\|hard` `--level level1` | direkt ein neues Spiel starten |
| `--windowed` `--size 1600x900` | Fenstermodus und Größe |
| `--warp` | Software-Rendering (ohne Grafikkarte) |
| `--gpu leistung\|sparsam` | V6: bei zwei Grafikchips die leistungsstärkste (Standard) oder die sparsamste verwenden |
| `--quality 0..3` | feste Qualitätsstufe für diesen Start (schaltet die Automatik ab) |
| `--mode modern\|terminal\|ascii\|mono` | Darstellung |
| `--debug` | Debug-Anzeige einschalten |
| `--set schluessel=wert` | Einstellung nur für diesen Start ändern, z. B. `--set ssr=false` |
| `--benchmark N` | N Bilder automatische Kamerafahrt, Ergebnis im Protokoll |
| `--demo` | automatische Kamerafahrt (Vorführung) |
| `--shot datei.png` `--shot-frames N` `--room typ[@zone]` `--shot-ui seite` `--pos x,y` `--yaw` `--pitch` | Test- und Bildwerkzeuge (`--shot-ui` u. a. `hud`, `map`, `use`, `inventory`, `pause`) |
| `--fixed-dt` | feste Simulationszeit (1/60 s je Bild) für reproduzierbare Aufnahmen und Skripte |
| `--script "wait 40; key Esc; expect Pause/main; …"` | automatischer Bedientest: spielt Tastendrücke ab und prüft Menüzustände (`wait N`, `key Taste`, `text …`, `expect Modus[/Seite]`, `shot datei.png`, `quit`). Exitcode 7 bei Fehler. |

---

## Architektur

```
src/
  core/      Grundlagen: Mathematik, Zufall (V4-kompatibel), JSON, Pfade, Protokoll, Job-System
  platform/  Fenster, Raw-Input, Bilddateien (WIC), Absturzbericht
  input/     Aktionen, Tastenbelegung, XInput
  world/     Level-Definition (Daten), Zonen, Raumgenerator (bitgenau V4), Sektoren,
             Streaming, Chunk-Aufbau (Kacheln, Lichter, Netze)
  physics/   Kollision gegen die Kachelwelt (Kapsel, Stufen, Kanten)
  ecs/       Entity-Component-System (Sparse Sets)
  render/    Direct3D 11: GPU-Hilfen, Welt-/Mesh-/UI-Renderer, prozedurale Texturen, Schrift, Post-Processing
  audio/     WASAPI-Mixer, prozedurale Klänge, räumliche Wiedergabe (V6: Laufzeit, Verdeckung, Nachhall)
  gameplay/  Spieler, Werte, Inventar, Items, Prefabs, Komponenten, Spielsitzung, Spielstände,
             V6: body (Strichmännchen-Animation, Kopfkamera), explore (Erkundungskarte)
  ui/        Menüs, HUD, Inventar, Minimap/Karte (ui_map), sprechen über eine Host-Schnittstelle mit der Anwendung
  app/       Anwendung: verbindet alle Module, Hauptschleife, Einstellungen,
             V6: soundscape (Leuchten, Raumakustik, ferne Klänge), autotune (Grafik nach Hardware)
shaders/     HLSL; werden beim Bauen zu .cso kompiliert
launcher/    LIMINAL.exe (Starter ohne C-Laufzeit)
data/        Level, Items, Schwierigkeiten, Tastenbelegung, Prefabs
tests/       Weltgenerierung gegen V4-Referenz, V6: Item-Balance, Gameplay (Sprung, Treppen, Karte, Körper),
             Klang (audio_test), Weltstruktur (layout_stats)
tools/       Build-, Paket- und Toolchain-Skripte
```

**Grundregeln**

- **Welt, Spiellogik, Darstellung und Menüs sind getrennt.**
  - `world` kennt weder Renderer noch Spiel.
  - `gameplay` kennt den Renderer nicht. Es liefert nur Listen sichtbarer Modelle und Klangquellen.
  - Der Renderer bekommt fertige Chunk-Daten.
  - Die UI spricht nur über `ui::Host` mit der Anwendung.
- **Level sind reine Daten.** Der Generator liest Zonen, Raumtypen und Lichtstile aus der Level-Datei.

---

## Erweitern

### Neues Level

1. `data/levels/level1.json` kopieren, z. B. nach `data/levels/level2.json`.
2. `id`, `name` und `title` ändern.
3. Zonen, Raumtypen und Lichtstile anpassen.

Das Level erscheint automatisch unter *Neues Spiel*. Du musst nichts kompilieren.

**Zonen** (`zones`): Bereiche der Welt, die weich ineinander übergehen.

- `key`, `name`
- Farben: `wall`, `floor`, `ceil`, `trim`, `lamp`, `fog`
- `fog_density`, `ambient`, `light_style`
- Muster: `wall_pattern` (`plain`, `wallpaper`, `blocks`, `bricks`, `panels`, `concrete`), `floor_pattern` / `ceil_pattern` (`none`, `carpet`, `slabs`, `bigtiles`, `grate`, `tiles`, `checker`, `lines`)
- Raumaufteilung: `min_leaf`, `max_leaf`, `big_chance`, `corridor_chance`, `corridor_width`, `wall_thickness`, `loop_chance`
- Höhen: `h_room`, `h_corridor`, `h_big`, `door_height`
- optional: `sunken`, `sunken_depth`, `origin_bias`, `door_clearance`, `wide_doors`

Die Ambience der Zone heißt `amb_<key>`. Für eigene Zonen legst du dazu eine WAV-Datei unter `data/sounds/` ab.

**Raumtypen** (`room_types`):

- `key`, `label`, `category` (`normal`, `corridor`, `large`, `unusual`, `rare`)
- Größen: `short`, `long`
- `affinity`: Gewicht je Zone
- `features`: `counter`, `pillars`, `pillars_sparse`, `thick_pillars`, `giant_pillars`, `colonnade`, `cubicles`, `shelves`, `crates`, `machines`, `podium`, `monolith`, `beams`, `vault`, `pipes`, `checker`
- optional: `height`, `sunken`, `stair_step`, `light`, `ambient_mul`, `fog_mul`, `supply`, `biggish`, `extra_doors`, `multi_doors`, `style`

**Lichtstile** (`light_styles`):

- Anordnung: `panels`, `panels_wide`, `panels_long`, `pools`, `wall`, `emergency`, `void`, `bright`, `dark`
- Werte je Stil: `extra`, `intensity`, `ambient`, `dark_fog`
- `center_fallback`: Räume, in die das Lampenraster keine Lampe setzt, bekommen eine Lampe in der Mitte.

**Items in Versorgungsräumen:** `supply_items` mit Gewichten.

**Entities** (`entities`): Setzt Prefabs in die Mitte passender Räume. Level 1 nutzt das für tropfendes Wasser in Kellerräumen:

```json
"entities": [
  { "prefab": "water_drip", "room_types": ["gewoelbe", "tunnel"], "chance": 0.35, "height": 2.2 }
]
```

**V6-Aufbau** (`layout_v6`, optional): Überlagert für neue Spiele einzelne Werte, ohne den alten Aufbau zu verändern.

- `zones` / `room_types`: Schlüssel des Eintrags → Felder, die ersetzt werden
- `add_room_types`: zusätzliche Raumtypen
- `light_styles`: Lichtstile ergänzen oder ersetzen
- `structure`:
  - `passage_chance`, `passage_width`, `passage_types`: breite Passagen auf den Hauptachsen
  - `open_join`: Flur trifft Flur → offener Übergang
  - `open_large`: Flur trifft Halle → breite Öffnung
  - `endless_mul`: Faktor für die Chance auf endlose Gänge

`data/levels/level0.json` ohne `layout_v6` ist die unveränderte V4-Welt. Der Test `worldgen_test` prüft, dass sie bitgenau der V4-Welt entspricht. `layout_stats` vergleicht die räumliche Struktur beider Aufbauten.

### Neues Item

1. In `data/items.json` einen Eintrag anlegen:
   - `key`, `name`, `description`
   - Wirkung: `sanity`, `health`, `speed` mit `speed_time`
   - `model`, `icon`, `scale`, `use_sound`, `max_stack`
   - V6, optional:
     - `spawn`: Seltenheit `{ "keep": 0.3, "replace": "almond", "replace_chance": 0.45 }`
     - `rare`: seltener Fund, schimmert auf der Theke
     - `hold_offset`: Griffhöhe am Modell, für die Hand beim Benutzen
2. Den Eintrag in `supply_items` eines Levels aufnehmen.

Ein neues 3D-Modell registrierst du in `src/render/meshes.cpp` (`MeshLibrary::init`). Danach verwendest du es in Items und Prefabs über seinen Namen. Ein neues Symbol legst du in `src/render/texture_gen.cpp` an.

### Neue Entities, NPCs, interaktive Objekte

**Prefabs** in `data/prefabs/*.json` setzen sich aus Komponenten zusammen:

| Komponente | Bedeutung |
|---|---|
| `transform` | Position, Drehung, Größe |
| `mesh` | sichtbares Modell, Sichtweite |
| `pickup` | aufnehmbares Item |
| `interactable` | Hinweistext, Reichweite |
| `audio_emitter` | Dauerklang an einer Position |
| `wanderer` | läuft selbstständig zwischen zufälligen Zielen umher und bleibt auf begehbaren Flächen |

Beispiel für ein umherwanderndes Objekt, das nur aus Daten besteht:

```json
{
  "name": "mein_npc",
  "components": {
    "transform": { "scale": 1.0 },
    "mesh": { "model": "almond", "draw_distance": 60 },
    "wanderer": { "speed": 1.2 }
  }
}
```

Über `entities` in der Level-Datei erscheint es in der Welt.

**Neue Komponenten** brauchen Code. Die Schritte:

1. Struktur in `src/gameplay/components.hpp` anlegen.
2. JSON-Leser in `src/gameplay/prefabs.cpp` registrieren.
3. System als Funktion in `GameSession` schreiben und in `GameSession::update` aufrufen. Vorlage: `updateWanderers`.

### Klänge

Die Klänge werden in `src/audio/synth.cpp` erzeugt. Eine Datei `data/sounds/<name>.wav` ersetzt den gleichnamigen Klang, z. B. `step_carpet_0.wav` oder `amb_office.wav`.

---

## Bauen

Du brauchst keine Visual-Studio-Installation. Das Skript lädt eine **portable Toolchain** in `.toolchain\` (nur dieser Ordner, keine Systemänderung):

- llvm-mingw (Clang, C++20)
- CMake
- Ninja

```bash
powershell -ExecutionPolicy Bypass -File tools\build.ps1
```

Das erzeugt ein startfertiges Build in `build\Release\bin\`. Mit `-Debug` entsteht ein Debug-Build, mit `-Clean` ein Neuaufbau.

```bash
powershell -ExecutionPolicy Bypass -File tools\package.ps1
```

Das erzeugt das Release-Paket:

- `Release\LIMINAL V6\`: Spielordner mit Programmen, Daten, Shadern, Icon, Lizenzhinweisen, `LIESMICH.txt`
- `Release\LIMINAL_V6.zip`

Test der Weltgenerierung gegen V4:

```bash
build\Release\tests\worldgen_test.exe data\levels\level0.json tests\data\v4_reference.json
```

Die Referenzdaten erzeugt `tools\v4_reference\dump_ref.py` aus dem unveränderten V4-Ordner (nur lesend).

V6-Tests: Item-Balance V5 gegen V6 (Energy Bars seltener, Sanity je Versorgungsraum gleich, Item-IDs stabil):

```bash
build\Release\tests\supply_balance_test.exe data
```

Sprung, Treppen, Erkundungskarte und Körperanimation (ohne Grafik):

```bash
build\Release\tests\gameplay_test.exe data
```

Klang ohne Audiogerät: Richtung, Laufzeitunterschied, Verdeckung, Nachhall, nahtlose Schleifen, Pegel:

```bash
build\Release\tests\audio_test.exe
```

Räumliche Struktur, alter gegen neuen Aufbau (Raumdichte, Raumgrößen, Sichtachsen, Versorgung). `--map ordner` schreibt Draufsichten als BMP:

```bash
build\Release\tests\layout_stats.exe data --check
```

---

## Hinweis: Signatur, SmartScreen und Smart App Control

Die Programme sind **nicht digital signiert**. Deshalb gilt auf anderen PCs:

- **SmartScreen** fragt beim ersten Start nach („Weitere Informationen“ → „Trotzdem ausführen“).
- **Smart App Control** (Windows 11) blockiert unsignierte Programme, wenn es aktiv ist.

Für eine Weitergabe an andere empfiehlt sich eine Code-Signatur, z. B. über **Azure Trusted Signing** oder ein Code-Signing-Zertifikat. Beide EXE-Dateien werden dann mit `signtool` signiert. Der Starter erkennt eine Blockade und erklärt sie.

---

## Fehlerbehebung

| Problem | Lösung |
|---|---|
| Starter meldet fehlende Dateien | Den kompletten Ordner behalten bzw. neu entpacken. Für den Desktop eine Verknüpfung auf `LIMINAL.exe` anlegen, nicht die EXE herauskopieren. |
| „Direct3D 11 nicht verfügbar“ / sehr langsam | Neuesten Grafiktreiber von NVIDIA, AMD oder Intel installieren. Ohne Grafikkarte ist nur der Software-Modus möglich (`--warp`). |
| Ruckeln | Grafikqualität *Automatisch* wählen (misst neu), *Dynamische Auflösung* einschalten, *Beleuchtung* senken oder Taste **L** (halbe interne Auflösung). |
| Läuft auf der integrierten Grafik (Laptop) | V6 wählt die stärkere GPU selbst. Sonst in den Windows-Grafikeinstellungen `LiminalGame.exe` auf „Hohe Leistung“ stellen. |
| Zu dunkel / zu hell | Grafik & Anzeige → Helligkeit. |
| Maus reagiert nicht | Das Spielfenster muss aktiv sein. Mit geöffnetem Menü ist die Maus frei. |
| Einstellungen kaputt | `%APPDATA%\LIMINAL\settings_v6.json` löschen. Beim nächsten Start werden Standardwerte angelegt und die Grafik neu gemessen. |
| Absturz | Fehlerbericht in `%LOCALAPPDATA%\LIMINAL\crash\` und Protokoll `liminal_v6.log`. Der letzte Autosave bleibt erhalten. |
