# Kritikrunden Level 36 (Abnahmeschranke d)

Laufweg `python tools/qa/walkway.py level36 --round rN --rooms 15 --seed 3`, Bögen `docs/screens/level36/wegbilder_rN_*.jpg`.
Grundlage: Wikidot Level 36 (keine Wesen).

## Runde 1 (Struktur mit den vier ergänzten Bereichen, Ausstattung V13.2)

Laufweg 481 m durch 15 Räume, 121 Bilder.

Befunde:
1. **Wie im Wiki:** Halle mit Terrazzo, Check-in-Schalter mit „36“, Abflugtafeln, Gänge zu den Gates. Meeting Place mit roter Damasttapete und blauen Sitzreihen.
2. **Sicherheitskontrolle leer** (018–020): Die Modelle `xray_scanner`, `security_gate` und `tray_stack` fehlen noch; man sieht nur die verdeckten Kollisionsblöcke als dunkle Flächen am Boden.
   - In der Lounge fehlen ebenso `lounge_chair` und `lounge_table`.
   - Das gehört zum Ausstattungs-Agenten für Level 36 und steht in seinem Auftrag.
3. **Check-in-Schalter:** schwarze Kästen ohne Detail. Auch das baut der Ausstattungs-Agent neu.

Änderung: keine an der Struktur. Runde 2 folgt nach der Ausstattung.

## Abnahme (V13.3, Schranken d–g)

Ab Runde 2 je Runde zusätzlich gezielte Aufnahmen der Bereiche (`python tools/qa/creatureshots.py level36 --round rN`,
Bögen `docs/screens/level36/wesen_rN_*.jpg` – der Name kommt vom Werkzeug; die Seite nennt keine Wesen, nur Reisende und
die Durchsagestimme, beides keine Figur): Gepäckausgabe (Ankunft am Band, Lamellen nah mit Lampe, Fundsachen, Fundbüro),
Check-in (Halle, Tafeln, Schalter nah mit Lampe, BTA-Stand), Boden (Terrazzo nah, streifend mit Lampe, im Rundgang;
Teppich der Gates), Sicherheitskontrolle (2 + 1), Rundgang (Wände, Laufband mit Glas und Lampe, stehendes Band, Blick in
die Sackgasse hinter EXIT, Sackgasse mit Lampe, Gang zu den Läden), Ladenzeile (Fronten, Schaufenster, Mode mit Lampe,
Lounge, Meeting Place), Gates (Gatehalle, Uhr und Brunnen, C32 mit Lampe, C33 von weitem und nah).
Schwerpunkte nach Auftrag: die Glas- und Metallflächen (in V12 verschmierte Spiegelungen) und die Schärfe des Terrazzos.

## Runde 2 (Bestand auf 55c073a)

Bögen `wegbilder_r2_*.jpg` (481 m, 15 Räume, 121 Bilder), `wesen_r2_1..4.jpg`.

Gut: Gepäckband mit Koffern und Anzeige „METRO 903“, Fundsachen-Regale, Schalterreihe mit Waagen und „36“, Abflugtafeln,
BTA-Stand violett, Röntgenbänder und Torsonden außer Betrieb, Laufbänder mit Kammplatten und grünen Anzeigen, Wegweiser
gelb auf Anthrazit, Lounge, Meeting Place mit Damast, Gates mit Teppich, Sitzreihen, C32-Lager, C33 mit Licht hinter der Tür.

Schlecht:
- **Wände der Gänge, des Rundgangs, der Gatehalle und der Sicherheitskontrolle sind dunkle, verschmierte Spiegel**
  (`wegbilder_r2_1` 018–029, `_2` 036–058, `_3` durchgehend; `wesen_r2_2` sicherheit, rundgang). Ursache: die Textur
  `metal_aluminum` ist Vollmetall (Metallkanal 0,98–1,00) mit Rauheit 0,04–0,09. Metall hat keinen Streuanteil; die Wand
  zeigt nur Spiegelbilder – im Bildraum (SSR) verwischte Bilder von Boden und Deckenleuchten, außerhalb davon die schwache
  Umgebung (Spiegelanteil 0,35), also dunkelgrau. Das ist der V12-Befund „verschmierte Spiegelungen“. Statt
  „Aluminiumpaneele“ (LOOK_36) sieht man graue Spiegelwände ohne Fugen; das Muster `panels` wird von der Textur verdeckt.
- **Putz glänzt fleckig** (`wesen_r2_2` rundgang_4: helle Wischer an beiden Wänden der Sackgasse; `wesen_r2_4` gates_5):
  `plaster_white` hat Rauheit 0,16–0,90; die glatten Stellen liegen unter der SSR-Grenze 0,42 und spiegeln die Leuchten als
  Wolken. Gestrichener Putz ist matt.
- **Terrazzo wirkt unscharf** (`wesen_r2_1` boden_1, `wesen_r2_2` boden_3, Ausschnitte in voller Größe): die Körner sind
  scharf gezeichnet, aber auf großen Flächen liegen zwei Versätze der Textur halb durchscheinend übereinander (Kachelbruch
  mit breitem Übergang, `smoothstep(0,2 … 0,8)` – rund 60 % der Fläche sind Mischzonen). Die Körner verdoppeln sich, der
  Kontrast sinkt, das Auge liest Unschärfe.
- **Rückwand der Läden** (`wesen_r2_3` laeden_2, laeden_3): weiß mit grauen Wolken – der Look `paint` (Abplatzer-Struktur)
  auf `white_dull`; nicht „sauber“.
- **Sackgasse nicht dunkel** (`wesen_r2_2` rundgang_4–5, `wesen_r2_3` rundgang_5_lampe): LOOK_36 und LEVELSPEC_36 verlangen eine
  dunkle Sackgasse; im Bild ist sie mittelgrau ausgeleuchtet. Zu prüfen: Licht des Rundgangs durch die 4 m breite Öffnung
  oder Belichtungsanpassung.
- Violette Schlieren an Gangwänden (`wegbilder_r2_2` 066, 067, 071) – vermutlich Spiegelung; nach dem Wandwechsel prüfen.
- Schaufenster: die diagonalen Spiegelstreifen lesen sich aus 3 m wie zwei schräge Stangen vor der Auslage (Ausstattung,
  bewusst wegen des deckenden Glases) – beobachten.

Änderungen für Runde 3:
- **Neue Wandverkleidung `alu_panel_36`** (eigene, prozedurale Textur, `tools/textures/generate.py`): Aluminium-
  Verbundplatten 1,5 × 3 m mit offener 12-mm-Fuge und dunkler Hinterlage, gekantete Plattenränder, seidenmatter
  Metallic-Lack (Rauheit um 0,5, Metall 0,45: Streulicht und weicher Glanz, keine Bildraum-Spiegelung über 0,42), je Platte
  leicht anderer Ton und Glanz, feine waagrechte Schleifspur, flache Welligkeit, wenige Fingerabdrücke und Schrammen auf
  Griffhöhe. Für die Zonen `gang` und `sicherheit`.
- **Matter Putz `plaster_matt_36`**: dieselben Bilder wie `plaster_white`, Rauheit + 0,3 (min 0,46), weniger Flecken
  (Variation 0,15) – Wände und Decken aller Level-36-Zonen. Andere Ebenen behalten `plaster_white`.
- **Terrazzo `terrazzo_36`**: dieselben Bilder wie `terrazzo_antique`, Kachelbruch mit schmalem Übergang (`antitile` 2:
  ± 0,08 statt ± 0,3, stärker vom Helligkeitsunterschied geführt, die Grenze läuft an Kornkanten entlang;
  `gbuffer.hlsli`, `imported_textures.cpp`). Texturen mit `antitile` 1 rechnen wie bisher.
- `process.py` kennt Einträge `same_as` (dieselben Bilder mit eigenen Werten), damit die Varianten reproduzierbar bleiben.
- Läden: Rückwand innen eigenes Material `wall_in` (matt, Look `plastic`); `storefront_a/b/c` neu gebaut (Dreiecke gleich).

## Runde 3

Bögen `wegbilder_r3_*.jpg` (481 m, 15 Räume, 121 Bilder), `wesen_r3_1..4.jpg`.

Besser:
- **Gänge lesen sich als verkleidete Flughafengänge**: Plattenraster mit feinen dunklen Fugen, keine Spiegelbilder mehr
  (`wegbilder_r3_1` 018–029, `_2` 036–058, `_3` durchgehend; `wesen_r3_2` rundgang_1–3, sicherheit_1–2). Die
  Laufband-Balustraden bleiben dunkles Glas (deckend, PROPS_Level36), die Spiegelung des polierten Terrazzos (Leuchten als
  weiche Flecken) ist gewollt.
- Putz matt, keine Wischer mehr in Sackgasse und Gates (`wesen_r3_2` rundgang_4, `wesen_r3_4` gates_5).
- Rückwand der Läden sauber weiß (`wesen_r3_3` laeden_2, laeden_3).

Schlecht:
- **Platten zu dunkel und schieferblau** (`wegbilder_r3_2` 042–058): Metall 0,45 lässt nur 55 % Streulicht; mit dem
  bläulichen Nebel wirken die Wände anthrazit statt hell aluminiumfarben (LOOK_36: „hell, ordentlich“).
- **Terrazzo** (Ausschnitte r2/r3 in voller Größe): weniger Doppelkörner, aber der Übergang schneidet Körner jetzt sichtbar
  ab (halbe Körner mit gerader Kante). Grundproblem bleibt die Texeldichte: 1024 Bildpunkte auf 2 m (2 mm je Punkt) – aus
  1 m Abstand wird die Textur vergrößert; die Körner sind 6–12 cm groß (für Terrazzo grob) und weich gezeichnet.
- **Violette Flecken an den Wandfüßen** der Gänge (`wesen_r3_3` rundgang_6, `wegbilder_r3_2` 066, `_3` 072). Mit den
  Debug-Ansichten des Renderers eingegrenzt (Einstellung `debug_view`, Beleg `docs/screens/level36/violett_belege.jpg`):
  - nicht im Direktlicht, nicht in der Albedo, nicht im SSAO;
  - im Umgebungslicht (Ansicht 2), auch ohne indirektes Licht, SSR, TAA und Volumenlicht;
  - unabhängig vom violetten Licht des BTA-Stands, von der Lampenfarbe der Läden, vom Ladenlicht vor den Schaufenstern und
    von den Ladenfronten selbst (je Gegenprobe in einer Kopie der Karte unverändert);
  - verschwindet, wenn die Zone `gang` neutral getönt ist (Lampe 1/1/1, Wand neutral).
  Ursache im Einzelnen nicht geklärt (das Umgebungslicht ist je Kachel lampe × Wandton der Zone); Vorschlag für die Buglist.
- **Sackgasse** weiter mittelgrau: Licht des Rundgangs fällt durch die 4 m breite, raumhohe Öffnung; dazu rechnet
  `Room::lightAt` bis 5,5 m hinter einer Öffnung 60 % des Nachbarraums ein (gilt für alle Ebenen).

Änderungen für Runde 4:
- `alu_panel_36`: Grundton 0,76, Metall 0,35 (mehr Streulicht, Metallic-Glanz bleibt).
- `terrazzo_36`: Kachel 1,2 m (Körner 4–7 cm, Texeldichte 1,2 mm); im schmalen Übergang gewinnen die dunkleren Körner
  beider Versätze (Führung umgekehrt), damit keine Körner abgeschnitten werden.
- Zone `gang`: Lampe 1/1/1, Wand 0,88 neutral (das kalte Licht der Leuchten steht als Farbe in der Karte und bleibt).

## Runde 4

Bögen `wegbilder_r4_*.jpg` (481 m, 15 Räume, 121 Bilder), `wesen_r4_1..4.jpg`.

Besser:
- **Wände** hell aluminiumfarben mit Metallic-Glanz, Fugenraster klar; im kalten Licht der Leuchten leicht kühl
  (`wegbilder_r4_2` 042–058, `_3` 072–107; `wesen_r4_2` rundgang_1–3).
- **Keine violetten Flecken** mehr (`wegbilder_r4_2` 066–071, `_3` 072; `wesen_r4_3` rundgang_6).
- **Terrazzo scharf** (Vergleich r2/r4 in voller Größe): kleinere Körner (4–7 cm), klar gezeichnet, keine halb
  durchscheinenden Doppelkörner und keine abgeschnittenen Körner; in den schmalen Übergängen liegen die Körner etwas
  dichter (liest sich wie natürliche Streuung). Aus der Ferne feines Korn, keine Helligkeitswolken
  (`wesen_r4_1` boden_1, boden_2; `wesen_r4_2` boden_3).
- Übrige Bereiche unverändert gut: Gepäckband, Fundsachen, Schalter, BTA, Röntgenbänder, Läden, Lounge, Meeting Place,
  C32, C33.

Schlecht:
- **Sackgasse** weiter mittelgrau ausgeleuchtet (`wesen_r4_2` rundgang_4–5): die Zone hat das dunkle Ebenenlicht
  (`dark`, Umgebung 0,01), aber der Nebel ist mit 0,06 zu dünn, um das Streulicht aus dem Rundgang zu schlucken.
- Sicherheitskontrolle kühler und dunkler als die Gänge (eigene Zone, Boden 0,62) – als eigener Bereich in Ordnung.
- Schaufenster: die schrägen Spiegelstreifen bleiben als Zeichen für Glas (Glas ist im Spiel deckend; PROPS_Level36).

Änderung für Runde 5:
- Zone `sackgasse`: dichter dunkler Nebel 0,18 statt 0,06 (Farbe `dark_fog`), Karte unverändert.

## Runde 5

Bögen `wegbilder_r5_*.jpg` (481 m, 15 Räume, 121 Bilder), `wesen_r5_1..4.jpg`.

Bestätigt: Wände, Terrazzo, Putz, Läden, keine violetten Flecken – wie Runde 4, über den ganzen Weg.

Schlecht:
- **Sackgasse** im Bild fast unverändert (mittlere Helligkeit des Ausschnitts r4 75,0 → r5 72,2): der Nebel folgt dem Raum
  des Spielers mit einer Zeitkonstante von gut 0,8 s (`session.cpp`), das Bild entstand 1 s nach dem Versetzen; dazu
  gleicht die Belichtung aus. Die Aufnahme zeigt also den Übergang, nicht den Zustand.
- **C33: eine helle Platte schwebt im Türausschnitt** (`wesen_r5_4` gates_5, Ausschnitt in voller Größe oben rechts in der
  Öffnung). Ursache: das Licht aus der offenen Tür steht in der Karte als Wandleuchte (Art 1); dazu zeichnet die Engine
  einen Leuchtkörper 0,5 × 0,26 m auf 1,9 m Höhe an die Wand – vor der hellen Fläche der Fluggastbrücke.

Änderungen für Runde 6:
- `tools/levels/level36.py` (am Ende, ohne Zufall): das Licht an C33 wird ein Licht ohne Leuchtkörper (Art 0 ohne Panel,
  2,2 m hoch, Ort, Farbe, Stärke wie bisher); Karte neu erzeugt, Vergleich mit der alten Karte: nur dieser Eintrag.
- `creatureshots.py`: in der Sackgasse 5 s warten (Nebel und Belichtung angeglichen), dann Bild und Lampe.

## Runde 6 (Schlussrunde)

Bögen `wegbilder_r6_*.jpg` (481 m, 15 Räume, 121 Bilder), `wesen_r6_1..4.jpg`; Vergleich Runde 2 / Runde 6:
`docs/screens/level36/abnahme_vorher_nachher.jpg` (Rundgang, Gang 6, Terrazzo nah, Tür an C33).

Befunde:
- **C33**: der Türausschnitt ist frei, die Fluggastbrücke eine helle Fläche mit Bodenstreifen; vor der Tür ein heller
  Lichtfleck auf dem Teppich (`wesen_r6_4` gates_4, gates_5). Abgenommen.
- **Sackgasse** nach 5 s im Raum dunkler (mittlere Helligkeit des Ausschnitts 72 → 64), die Rückwand liegt im Nebel;
  mit Lampe ein Lichtkegel auf der Rückwand (`wesen_r6_2` rundgang_5, `wesen_r6_3` rundgang_5_lampe). Ganz dunkel wird sie nicht: die Belichtung passt
  sich an, und durch die 4 m breite Öffnung fällt das Licht des Rundgangs (Karte und Engine bleiben, siehe Restpunkte).
- Wände, Terrazzo, Putz, Läden, Lounge, Meeting Place, Gatehalle, C32 unverändert gut; keine violetten Flecken; kein
  Spiegelbild auf Wandflächen; die Spiegelung im polierten Terrazzo bleibt weich und ruhig (gewollt).
- Keine weiteren Änderungen.

Restpunkte (begründet):
- Ursache der violetten Flecken im Umgebungslicht bei kühl getönter Zone nicht im Einzelnen geklärt (Beleg
  `violett_belege.jpg`); in Level 36 durch neutrale Zonenfarbe behoben. Vorschlag für die Buglist.
- Sackgasse nur halbdunkel (Belichtung, breite Öffnung, `Room::lightAt` rechnet 60 % des Nachbarraums bis 5,5 m ein).
- Glas deckend (Engine): Schaufenster mit Spiegelstreifen, Laufband-Balustraden dunkles Glas.
- Sicherheitskontrolle kühler als die Gänge (eigene Zone).
