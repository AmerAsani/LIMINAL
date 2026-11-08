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
- **Sackgasse nicht dunkel** (`wesen_r2_2` rundgang_4, `wesen_r2_3` rundgang_5): LOOK_36 und LEVELSPEC_36 verlangen eine
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
