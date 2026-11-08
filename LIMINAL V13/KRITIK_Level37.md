# Kritikrunden Level 37 (Abnahmeschranke d)

Laufweg `python tools/qa/walkway.py level37 --round rN --rooms 15 --seed 3`, Bögen `docs/screens/level37/wegbilder_rN_*.jpg`.
Grundlage: Wikidot Level 37 (keine Wesen).

## Runde 1 (Struktur, Ausstattung V13.2)

Laufweg 203 m durch 21 Räume, 51 Bilder.

Befunde:
- **Wie im Wiki:** weiße Fliesen, flaches türkises Wasser mit Wellen und Spiegelung, tiefe Becken (Schwimmen mit sichtbaren Händen), Durchgänge, Deckenleuchten, Übergang aus dem Metrowagen.
- **Keine Änderung.**

## Abnahme (V13.3, Schranken d–g)

Ab Runde 2 je Runde zusätzlich gezielte Aufnahmen der Bereiche (`python tools/qa/creatureshots.py level37 --round rN`,
Bögen `docs/screens/level37/wesen_rN_*.jpg` – der Name kommt vom Werkzeug; die Ebene hat laut Seite keine Wesen, die
Läufe zeigen die Bereiche nach `LOOK_37.md` und das Wasser): Tunnel mit Wagen (5 Ansichten, 2 mit Lampe), Räume mit Licht
tief von der Wand, aus der Ecke, von unten und dunkel (4 + 2), Säulenhalle (3 + 1), Gruben hell, dunkel, unter Wasser,
an der Oberfläche (5 + 1), Wasser nah (Wasserlinie, Spiegelung, Fresnel, Tropfen; ab Runde 5 sechs Ansichten + 1).
Schwerpunkt nach Auftrag Teil 1: Wasser, Pfützen, Reflexionen.

## Runde 2 (Bestand auf 11352ea)

Bögen `wegbilder_r2_*.jpg` (203 m, 21 Räume, 51 Bilder), `wesen_r2_1..3.jpg`.

Gut: Wellen wandern, die Fliesen darunter sind leicht verzogen (Brechung), Spiegelung mit Fresnel (Bildraum-Spiegelung,
außerhalb des Bildes die Decke), Lampen glitzern im Wasser, Gruben dunkel blaugrün, Unterwasser-Nebel.

Schlecht:
- **Kein Lichtnetz (Kaustik).** Der Boden unter dem Wasser ist gleichmäßig beleuchtet; das Wasser wirkt wie ein türkis
  gestrichener Boden. Das Kennzeichen solcher Becken – das wandernde helle Netz auf dem Grund und an Wänden und Decke –
  fehlt ganz.
- **Licht von unten unsichtbar.** Die Lampen „von unten“ (Säulenhallen: zwei je Halle, LOOK) liegen unter der
  Wasserfläche; man sieht sie nicht, nur helle Streifen an den Säulenfüßen; die Fläche um die Lampe bleibt dunkel
  (`halle_1`, `halle_2`).
- **Keine Tiefenwirkung.** Die Bodentextur liegt auf der Wasserfläche selbst (keine Parallaxe); die 4,5 m tiefe Grube
  zeigt das Fliesenraster an der Oberfläche in derselben Größe und Schärfe wie 0,3 m flaches Wasser (`grube_5`).
- **Keine Ringe von Tropfen**, obwohl der Klang der Ebene alle 3–9 s tropft (Teil 1: „Ripples bei Tropfen“).
- **Wasserlinie hart:** die Wand steht ohne Übergang im Wasser.
- Wandleuchten tief, Licht aus der Ecke und dunkle Räume wie im LOOK; Wagen, Stufen und Notiz in Ordnung.

Änderungen für Runde 3:
- Lichtnetz in der Beleuchtung (`shaders/lighting.hlsl`, `causticNet`): Ränder einer langsam wandernden Zellstruktur,
  verzerrt, zwei Lagen. Wo es wirkt, sagt ein neuer Kanal: der Umgebungs-Ring trägt im Alpha den Wasserspiegel klarer
  Becken (`world_renderer.cpp`, `Room::waterLook`), sonst −60000 – das dunkle Meer in Level 0 und alle anderen Ebenen
  bleiben unberührt.
  - Wasserfläche (zeigt den Boden): Licht der Lampen über dem Wasser als Netz, mit der Tiefe schwächer.
  - Unter Wasser (Gruben): Netz von oben, mit der Tiefe schwächer.
  - Über Wasser: Licht von Lampen unter Wasser kommt als Netz an (Decke, Säulen, Wände).
  - Licht der Lampen über dem Wasser wird gespiegelt (Spiegellampe unter dem Wasserspiegel, Fresnel, Spiegelpunkt auf
    demselben Becken, Schatten auf beiden Wegen) und wirft ein Netz an Wände und Decke.
  - Schmale Wasserlinie an der Wand.
- Lampen unter Wasser als Schein in der Wasserfläche (Kern und Hof).
- Becken tragen ihre Tiefe im Material (`generator.cpp`, `clearWater`, Metall-Zuschlag = Tiefe): der Boden erscheint
  um die Tiefe verschoben (Brechung, Parallaxe) und in der Tiefe weicher; Tropfenringe auf der Fläche
  (`shaders/gbuffer.hlsli`).

## Runde 3

Bögen `wesen_r3_1..3.jpg`.

Gut: Netz auf dem Grund (`becken_1`, `becken_4`, `tunnel_3`) – die Becken lesen sich sofort als Wasser; Decke über der
Lampe von unten mit wanderndem Netz (`becken_3`, `halle_1`), Schein der Lampen im Wasser sichtbar.

Schlecht:
- **Netz an Wänden und Säulen körnig** wie Schmutz (`becken_3`, `halle_1`): die Wandkoordinate kam aus der Normalen im
  G-Buffer, die das Fugenrelief trägt – über 20–40 m Weltkoordinate verrauschte das zu Körnung.
- **Schein zu stark:** in `halle_3` ein weißer Fleck von 3 m.
- **Wasserfläche an der Oberfläche der Grube weiß** (`grube_5`): die Fläche von unten bekam das Spiegellicht.
- **Gespiegeltes Netz an den Wänden unsichtbar** (Verstärkung 1,6).

Änderungen für Runde 4: Wandkoordinate aus der Achse der Fläche; Wasserfläche von unten ausgenommen; Schein Kern 0,9,
Hof 0,12; Spiegellicht: Probe mit 16 (überstrahlt, nicht übernommen), dann 6.

## Runde 4

Bögen `wesen_r4_1..3.jpg`.

Gut: Säulen und Wände mit klarem Netz (`halle_1`, `halle_2`, `becken_3`); gespiegeltes Netz an der Nordwand des Tunnels
(`tunnel_1`) und die Taschenlampe, vom Wasser auf die Rückwand gespiegelt (`becken_1_lampe`, `becken_4_lampe`,
`grube_2_lampe`); der Grund der Grube im Lampenlicht mit Netz.

Schlecht:
- **Grube an der Oberfläche wieder hell** (`grube_5`): der Schein der Lampe am Grund wurde flach über das Wasser genauso
  stark gezeigt wie von oben; dazu lag er immer genau über der Lampe.
- **Tropfen nicht zu sehen** (Prüfbilder `wasser_4..6` neu).
- Schein in `halle_3` noch groß.

Änderungen für Runde 5: Schein dort, wo der gebrochene Blickstrahl die Tiefe der Lampe erreicht, mal (1 − Fresnel) und
Weg im Wasser; Kern 0,6. Tropfen: Probe mit 1 s Takt und Rot-Probe der Tiefe (Tiefe kommt im Shader an; Ringe zeigen
sich wie in echt in Glanz und Spiegelung als Bogen aus Lichtpunkten), Wellenhöhe 0,45, Takt 7–13 s in 60 % der 4-m-Zellen.
