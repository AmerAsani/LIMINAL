# Hand und Taschenlampe – Analyse (V13, Teil 1)

Stand: 26.10.2026. Grundlage sind Aufnahmen aus V13 vor jeder Änderung, also identisch mit V12 (`docs/screens/viewmodel/v13_vorher/`).
Für die Prüfung von außen gibt es neu eine Debug-Kamera. Der Skriptbefehl `vmcam <az> <el> <abstand>` umkreist die Griffmitte im Rahmen der Lampe, `vmcam off` beendet ihn.

## 1. Wie die Hand bisher entsteht

| Stufe | Ort | Was passiert |
|---|---|---|
| Form | `tools/blender/models/models_vm.py` `_hand_parts()` | Rund 60 Ellipsoide und Röhren werden überlagert: Handteller aus drei flachen Ellipsoiden, Ballen, Knöchel, Sehnen, je Finger Röhre plus Gelenkwülste. |
| Pose | `_solve_finger()` | Ein Suchlauf über Grund- und Mittelgelenk (Endgelenk = 0,7 × Mittelgelenk) und **Spreizung ±12°** legt jeden Finger einzeln an den Schaft. |
| Vereinigung | Voxel-Remesh (0,6 mm) | Die überlappenden Teile verschmelzen zu einer Hülle. |
| Glättung | Smooth-Modifier (Faktor 0,6, 10 Durchgänge) | Glättet stark, aber ungleichmäßig. |
| Reduktion | Decimate **5 %** | Ergibt 19 262 Dreiecke mit unregelmäßigen Dreiecksfächern. |
| Nägel | eigene flache Ellipsoide | Liegen lose auf den Endgliedern, getrennt von der Hautfläche. |
| Material | `fm=skin_player` | Gleiche Hauttextur und Farbe wie der Körper. Rauheit 1,0, kein Durchscheinen, keine Färbung an Knöcheln, Fingerkuppen oder Falten. |
| Im Spiel | `gameplay/body.cpp` | Ein **starres, ungeriggtes** Netz im Rahmen der Lampe. Es erscheint nur, solange die Lampe gehalten wird (`flashHold_ > 0,5`). Sonst schrumpft die Hand des Skeletts ins Handgelenk (`FigureRig::hideHand`) bzw. kommt zurück. |

Prüfsumme des geladenen Modells: `data/models/vm_hand_l.glb` = `build/.../bin/data/models/vm_hand_l.glb`, SHA-256 `e8a2c3fa…3bda`.
Laut Protokoll lädt das Spiel genau dieses Netz (2 Teile, 19 262 Dreiecke).

## 2. Befund (Nahaufnahmen)

| Bild | Befund |
|---|---|
| `01_ego.jpg` | Aus dem Auge wirkt die Hand wie ein Klumpen. Die Finger sind nicht als Finger lesbar, der Daumen verschwindet. |
| `03_debug_links.jpg` | **Gespreizte Finger** mit ungleichen Lücken. Zeige- und Mittelfinger kreuzen den Schaft an verschiedenen Stellen. Der kleine Finger steht ab. |
| `04_debug_rechts.jpg`, `08_debug_nah.jpg` | Dellen und Löcher auf dem Handrücken (Glättung plus Reduktion). Knöchel als Beulen statt Gelenke. Die Nägel sind dunkle Flecken. |
| `06_debug_oben.jpg` | Von oben laufen die Finger wie getrennte Schläuche über den Schaft. Zwischen den Fingern bleibt Luft (schwebend), andere Stellen dringen ein. |
| `07_debug_unten.jpg` | Die Handfläche ist ohne Ballenform. Die Fingerkuppen enden in der Luft statt am Daumenballen. |
| `09_uebergang_aus_an.jpg` | Beim Ausschalten verschwindet die Lampe mitten im Absenken (bei 50 %). Beim Einschalten ist der Lichtkegel 6 Bilder lang an der Wand, bevor Hand und Lampe ins Bild kommen. |

## 3. Ursachen

1. **Falsche Methode (Hauptursache):**
   - Eine Hand aus vereinigten Ellipsoiden hat keine Gelenkflächen, keine Beugefalten, keine Fingerkuppen und keine Nagelbetten.
   - Die starke Glättung schrumpft dünne Teile (Finger) stärker als dicke (Handteller). Das Reduzieren auf 5 % legt dann zufällige Facetten darüber.
   - Bessere Winkelwerte ändern daran nichts. Das hat V12 in mehreren Runden gezeigt; der Auftrag verlangt dann einen Wechsel des Ansatzes.
2. **Kein Skelett, kein Skinning:**
   - Die Hand ist starr und kann deshalb keine Übergänge zeigen: kein Schließen der Finger beim Heben, kein Daumendruck auf den Schalter.
   - Zwischen Skeletthand und Blender-Hand wird hart umgeschaltet.
3. **Griff-Löser:**
   - Jeder Finger wird einzeln gesucht, mit erlaubter Spreizung von ±12° und ohne Rücksicht auf die Nachbarn. So entstehen ungleiche Lücken.
   - Die Grundgelenke liegen auf einer ebenen Handfläche ohne Querwölbung. Im Kraftgriff hebt sich der Mittelhandbogen; Ring- und Kleinfinger-Mittelhand beugen sich mit, das fehlt.
4. **Proportionen:**
   - Fingerglieder 44/25/20 mm (Zeige) und 49/30/22 mm (Mittel, ×1,03) sind 10–15 % zu lang.
   - Anthropometrische Mittel für Erwachsene: Grundglied Zeigefinger ≈ 40 mm, Mittelglied ≈ 22 mm, Endglied mit Kuppe ≈ 19 mm.
   - Zu lange Finger umschließen den 33-mm-Schaft mehr als ganz und müssen dafür spreizen oder eindringen.
5. **Material:**
   - Haut als triplanare Textur mit einer Farbe. Kein Licht dringt in die Haut ein (kein Subsurface), Knöchel und Kuppen sind nicht gerötet.
   - Die Nägel sind eigene Körper statt Teil der Haut.
6. **Übergänge:**
   - Das Netz wird bei 50 % Heben ein- und ausgeblendet.
   - Das Licht geht beim Tastendruck sofort an, auch wenn die Lampe noch unter dem Bildrand ist.

## 4. Neuer Ansatz (Entscheidung E-020)

- **Modell:**
  - Box-Modelling als Viereck-Käfig: Handteller als Gitter mit Rücken, Fläche, Seiten; vier Finger mit je drei Gliedern; Daumen aus der Daumenseite; Handgelenk und Unterarmansatz.
  - Danach Subdivision Surface (Catmull-Clark).
  - Ringe an den Gelenken bilden Beugefalten und Knöchel; Nagel und Nagelwall entstehen durch Einsetzen im Käfig (eigenes Material, Teil der Hülle).
- **Skelett:**
  - 21 Knochen: Unterarm, Hand, 4 Mittelhandknochen, 12 Fingerglieder, Daumen mit 3 Knochen (Mittelhand, Grund- und Endglied).
  - Die Gewichte werden beim Bau festgelegt (Ring für Ring, an den Gelenken gemischt) und durch die Subdivision weich interpoliert.
- **Posen:**
  - In Blender als Knochenrotationen: Ruhe (locker), Griff (Zylindergriff um den Schaft, Finger nach außen leicht stärker gebeugt), Daumendruck (auf den Schalter).
  - Ein Löser setzt die Beugung je Finger so, dass die Fingerballen am Schaft anliegen, ohne Spreizung und ohne Eindringen; Nachbarfinger dürfen sich nicht überschneiden.
- **Engine:**
  - Der glTF-Leser liest Skin (JOINTS_0, WEIGHTS_0, inverse Bindematrizen, Knotenhierarchie) und Posen.
  - Die Hand wird über den vorhandenen Skinning-Pfad der Figuren gezeichnet.
  - Posen werden je Knochen gemischt: Griff nach Hebeanteil, Daumendruck beim Schalten.
  - Die Hand bleibt während des ganzen Hebens und Senkens sichtbar, ohne harten Wechsel.
- **Haut:**
  - Eigenes Material für die Hand.
  - Im Licht-Shader neu: Durchscheinen der Haut (gewickeltes, rötliches Streulicht über den Schattenrand, Rückseitenlicht an dünnen Stellen).
  - Rötung an Knöcheln und Kuppen, dunklere Falten; das alles je Punkt vom Bau mitgegeben.
- **Prüfung:**
  - Mindestens vier Kritikrunden aus dem Auge und mit der Debug-Kamera von mehreren Seiten.
  - Zustände: Ruhe, Gehen, Laufen, Drehen, Lampe an/aus, Übergänge. Bilder in `docs/screens/viewmodel/v13_runde<N>/`.

## 5. Kritikrunden

Ansichten je Runde: aus dem Auge (Ruhe, Gehen, Laufen, Drehen, aus, Heben, an) und mit der Debug-Kamera `vmcam` (oben, rechts, links, unten, nah); dazu Blender-Ansichten des Modells.
Die Hilfsskripte liegen außerhalb des Projekts (Scratchpad); die Befehle stehen in `PROGRESS.md`.

### Runde 1 – neue Methode im Spiel (`v13_runde1/`)

**Gebaut:**
- Käfig mit Subdivision und Skelett mit 21 Knochen.
- Griff-Löser: Achse unter der Hohlhand, Finger im Raster, Daumen per Zufallssuche.
- Glb mit Skin und den Posen `grip` und `press`.
- Engine:
  - liest Skin und Posen;
  - zeichnet die Hand über den Skinning-Pfad;
  - Handgelenk, Unterarm und Handfläche kommen aus dem Skelett (Gelenk `elbow` als Markierung).

**Befund und Korrektur:**

| Befund | Korrektur |
|---|---|
| Der Unterarmansatz stand groß neben dem Ärmel. Ursache: Das glb war vor der Markierung gebaut, die Engine fiel auf die Werte von V12 zurück. | Neu gebaut; der Test vergleicht jetzt die Geometrie aus dem Skelett mit den Werten des Baus (Abweichung < 2 mm). |
| Erster Lampenwinkel (58°) verlangte einen Ellbogen über dem Handgelenk. | Lampenwinkel und Handgelenk neu (siehe E-020). |
| Ein Unterarm „von links unten“ erzwang eine Handfläche nach oben. | Drehung um die Lampe nach der Handfläche (zur Körpermitte) statt nach dem Unterarm. |
| Blasse Hautfetzen am Bündchen: Teile der Skeletthand ragten heraus. Nachweis mit `vmdebug 1` (Hand ausgeblendet), siehe `05_ohne_hand_kein_rest.jpg`. | Netzvariante der Spielerfigur ohne die linke Hand (`_nh1`), dazu eine kleine Schnittkugel am Handgelenk. |

**Schaltfolge neu:**
- Die Lampe bleibt immer in der Hand.
- Einschalten: Der Arm hebt sie, der Daumen drückt, erst dann Licht und Klick.
- Ausschalten: Der Daumen drückt, das Licht geht aus, danach sinkt der Arm.

### Runde 2 – Form (`v13_runde2/`)

| Befund | Ursache | Korrektur |
|---|---|---|
| Finger wirkten angesteckt und dünn | Schwimmhäute saßen am Knöchel | Beugeseite und Seiten der Fingeransätze 2 cm weiter vorn, Ringe schräg (Rücken näher am Knöchel) |
| Keine Knöchel in der Faust | Alle Punkte eines Rings folgten demselben Knochen | Gewichte je Gelenkseite: Rücken bleibt beim Knochen davor (Knöchel = dessen Kopf), Beugeseite folgt dem Glied danach |
| Kralle mit Graten zwischen den Fingern | Grundgelenke von 27° bis 97°: Die Achse lief direkt unter dem Zeigefinger-Knöchel | Lampe steiler (52°); Ring- und Kleinfinger-Mittelhand beugen stärker (8°/18°); Nachbarfinger gekoppelt (Sehnenverbindungen); Corrective Smooth nach dem Skinning; Finger, die sich berühren, legen sich flach aneinander |
| Nägel zur Hälfte auf der Seite | Falsche Flächenstreifen gewählt | Die beiden oberen Streifen |
| Daumen lag nur am Gelenk auf, Druck ging ins Leere | Kuppe nicht als Ziel; Beugen hebt sie hier vom Schaft | Ziel ist die Beugeseite der vorderen Hälfte des Endglieds; Druck über Anziehen/Drehen der Daumen-Mittelhand (1,7 mm) |
| Schalter nicht unter dem Daumen | – | Lage aus dem Bau der Hand (`vm_grip.json`), flacher (1,2 mm) |
| Treppenkante am Bündchen (erst ohne die alte Hand sichtbar) | Scharfer Ebenenschnitt im Distanzfeld | Schnitt mit weichem Maximum gerundet (8 mm) |

**Ergebnis:**
- Aus dem Auge liegen Daumen oben und Finger unten am Schaft, beim Gehen, Laufen und Drehen.
- Gesenkt ist die Hand außer Sicht.
- Beim Heben bleibt das Licht aus, bis der Daumen gedrückt hat.

### Runde 3 – Material, Daumendruck, Übergänge (`v13_runde3/`)

| Befund | Korrektur |
|---|---|
| Nahaufnahmen der Debug-Kamera waren gerastert durchsichtig. Ursache: Das Ausblenden mitbewegter Teile nahe der Kamera (gegen riesige Schultern im Bild) – kein Modellfehler. | Bei aktiver Debug-Kamera ausgeschaltet (`gWave.w`). |
| Haut gleichförmig rosa, Rötung kaum sichtbar (Albedo-Ansicht `08/09_albedo_vorher`) | Knöchel und Kuppen deutlich röter, Handrücken leicht dunkler, Handfläche röter, Beugefalten dunkler (Tönung je Punkt) |
| Daumendruck kaum zu sehen (1,7 mm) | 2,5 mm (Daumen-Mittelhand dreht 5°, Endgelenk 3°) |

**Geprüft:**
- Schulteransicht: Ausgeschaltet hängt die Lampe in der Hand an der Hüfte; beim Einschalten hebt der Arm sie.
- Dunkler Raum: Nur der Kegel leuchtet, die Hand ist Silhouette.
- Blick steil nach unten und oben: Lampe und Kegel folgen.

Hautporen und feine Linien sind in der Hauttextur (0,7 mm Poren, 10 cm je Kachel). Aus 40 cm sind sie kaum zu sehen, wie in echt. Knöchelfalten wurden bewusst nicht modelliert: Über der Faust ist die Haut gespannt.

### Runde 4 – Ärmel und Abschluss (`v13_runde4/`)

**Befund:** Das Ärmelende des Körpers war im Bild gezackt. Der Ärmel ist ein abgeschnittenes Distanzfeld, auf 1 cm vernetzt und auf 27 % reduziert.

**Erster Versuch – verworfen:** ein zweiteiliger Ärmel mit feinem Unterarmstück. In Ruhe stimmten die Maße (Radius 5,9 cm, 45–96 % des Unterarms), in der Pose wichen die beiden Stücke voneinander ab.

**Lösung:**
- Der weite Ärmel läuft 5 cm vor dem Handgelenk in einen engen Strickbund aus; es gibt keine Schnittkante mehr. Die Hand kommt aus dem gerundeten Ende des Bundes.
- Der Ansatz der Hand aus Blender ist auf 2,5 cm gekürzt.

**Stand am Ende:**

| Zustand | Ergebnis | Bild |
|---|---|---|
| Ruhe, Gehen, Laufen, Drehen | Daumen oben auf dem Schaft, Finger unten geschlossen, Lampe im Blick | 01–04 |
| Aus | gesenkt außer Sicht | 05 |
| Heben, an | Licht erst nach dem Daumendruck | 06–07 |
| Debug-Kamera | Hand von allen Seiten geschlossen; kein Eindringen in die Lampe sichtbar; Bündchen sauber | 08–14 |
| Material | Haut mit Rötung und Durchscheinen | 15–16 |
| Schulteransicht | aus/an | 17–18 |
| Dunkel | nur der Kegel leuchtet | 19 |
| Wand | Lampe zieht zurück und kippt | 20–21 |
| Blick | unten/oben | 22–23 |

Tests: gameplay 357/0, darunter neue Prüfungen:
- Skelett mit mindestens 21 Gelenken;
- Posen `grip` und `press`;
- Geometrie aus dem Skelett = Werte des Baus (< 2 mm);
- `grip` = Ruhepose;
- `press` bewegt nur den Daumen;
- Unterarm passt zum Modell (< 12°).

### Offene Punkte (Hand)

- **Ellbogen des Ärmels** (`24_ellbogen_fehler_aermel.jpg`):
  - Bei stark gebeugtem Arm reißt die Kapuzenjacke am Ellbogen auf (lineares Skinning der Körperfigur). Das ist kein Fehler der Hand.
  - In der Ego-Ansicht nicht im Bild, in der Schulteransicht von der Seite sichtbar.
  - Eingetragen in `BUGLISTE.md` (Teil 5).
- **Rechte Hand:**
  - Die rechte Hand des Körpers (Gesten: Greifen, Essen, Trinken) ist weiter die prozedurale Hand mit einem Fingerknochen.
  - Der Auftrag betrifft die Hand an der Lampe; die rechte Hand mit demselben Modell (gespiegelt, Posen Greifen/Halten) ist ein möglicher nächster Schritt.
- **Unterarm:** Beim Blick steil nach unten weicht der Unterarm des Körpers bis 11,5° vom Unterarm des Handmodells ab (Arm-IK, Test < 12°). Das Bündchen deckt das.

## V13.3: Arm

Stand: 27.10.2026. Auftrag (Teil 3): Die Lampe in der Hand sieht gut aus, der Arm wirkt verkrüppelt – Ärmel und Oberarm aufgeblasen, Ellbogen und Unterarm unplausibel. Armkette Schulter – Oberarm (ca. 30 cm) – Ellbogen – Unterarm (ca. 27 cm) – Handgelenk neu, Ärmel wie eine echte Jacke, Finger unverändert.

Bilder: `docs/screens/viewmodel/v13_3_<runde>_*.jpg`. Je Runde dieselben Kameras:
- `ego_zustaende`: Spielerkamera – Ruhe, Gehen, Rennen, Drehen, Ducken (Schleichen, Taste C), Interagieren (Mandelwasser trinken, rechte Hand), Ausweichen (seitlich mit Sprung), Blick unten, Blick oben, Lampe aus, Heben, an;
- `ego_ruhe`: Ruhe in voller Größe;
- `debug_arm`: Debug-Kamera `vmcam` in 0,55 m um die Griffmitte, zwölf Richtungen;
- `debug_hand`: die Nahansichten der Hand aus Teil 1 (Finger-Vergleich);
- `aussen_armkette`: Schulteransicht mit der Ego-Haltung (`egopose 1`), Kamera um den Spieler (`figcam`) – die ganze Kette Schulter bis Hand.

Hinweis: `vmcam` kreist im Rahmen der Lampe. Ab Runde 2 ist dieser Rahmen im Ego-Bild um die Lampenachse gedreht (siehe dort); relativ zur Hand stehen die Kameras gleich, nur die Umgebung erscheint gedreht.

### Befund vorher (Runde 0, `v13_3_r0_*`)

| Ansicht | Befund |
|---|---|
| Ego, Ruhe | Ein grüner Sack füllt das linke Drittel des Bildes (bis zur halben Bildhöhe), die Hand kommt unten aus einem gerundeten Ende. Die Hand wirkt daneben winzig. |
| Debug, vorn rechts | Der Oberarm steigt von der Schulter nach oben, der Ellbogen ist ein runder Klumpen über der Hand, der Unterarm hängt kurz und dick herab. |
| Debug, rechts | Unterarm-Ärmel so dick wie die Hand lang, bis kurz vor das Handgelenk, dann ein Sprung auf das Bündchen. |
| Außen | Mit der Ego-Haltung steht der Ellbogen über Kopfhöhe vor dem Gesicht. |

### Ursachen der Fehlproportionen

1. **Ärmel = Körperform plus 2,2 cm rundum.** Der Ärmel bestand aus abgerundeten Kegeln mit demselben Aufmaß wie der Rumpf: Unterarm 6,1 cm Radius am Ellbogen und noch 5,7 cm fünf Zentimeter vor dem Handgelenk (11–12 cm dick), dann auf 3,25 cm Bündchen. Eine echte Kapuzenjacke (Größe M) ist am Ellbogen etwa 31 cm, über dem Bündchen 24 cm weit (Radius 4,9 bzw. 3,8 cm). Dazu 4 mm Stoffrauschen auch am Arm (Beulen) und eine geschlossene Halbkugel als Ende.
2. **Haltung (Hauptursache im Ego-Bild).** Das Handmodell hält die Lampe 52° schräg in der Faust, das Handgelenk ist 33° zur Elle geknickt und 14° gestreckt. Mit dem Daumen genau oben und der Lampe in Blickrichtung fällt der Unterarm des Modells um 18° zur Hand ab. Die IK legte den Ellbogen deshalb **16 cm über die Schulter** (Oberarm 33° nach oben). Der Unterarm lief quer, nur 29 cm vor dem Auge, durchs Bild und war perspektivisch riesig. Nachgerechnet mit einem Modell der Kette (Scratchpad `armgeo.py`), es stimmt mit dem Spiel überein (Beugung 58,9°, Test 59°).
3. **Unterarm 25,4 cm** (0,146 × Körpergröße) statt rund 27 cm.
4. **Gewichte am Ellbogen nur aus dem Abstand zu den Knochen**, geteilt durch deren Radius: Der dünnere Unterarm bekam erst hinter dem Gelenk Gewicht, die Mischzone war breit und schief – der Ellbogen wurde zum Wulst.

### Runde 1 – Ärmel und Längen (`v13_3_r1_*`)

**Geändert:**
- `src/render/figure_models.cpp`: Der Ärmel ist ein eigener Schlauch entlang der Armkette (Distanzfeld `sleeveDist`) mit dem Querschnitt einer Jacke. Radien über monotone kubische Interpolation (keine Wülste zwischen den Stützstellen):

  | Stelle | Radius | Umfang |
  |---|---|---|
  | Schulter | Oberarm + 1,85 cm = 6,35 cm | 40 cm |
  | Mitte Oberarm | Oberarm + 1,25 cm = 5,75 cm | 36 cm |
  | Ellbogen | Unterarm + 1,25 cm = 4,95 cm | 31 cm |
  | 35 % des Unterarms | Unterarm + 0,75 cm = 4,45 cm | 28 cm |
  | vor der Raffung | Handgelenk + 1,25 cm = 3,85 cm | 24 cm |
  | Bündchen (5,8 cm) | Handgelenk + 0,58 → 0,46 cm = 3,2 → 3,1 cm | 19–20 cm |

- Zwischen Ärmel und Bündchen eine Raffung auf 1,1 cm (Naht), das Bündchen endet 0,6 cm hinter dem Handgelenk mit einer Öffnung.
- Stoffrauschen am Arm um 65 % schwächer, im Bündchen keins.
- Ober- und Unterarm teilen sich den Ärmel an der Ebene des Ellbogens (Winkelhalbierende, Mischzone ±4,5 cm). Mit ±3 cm klappten bei 81° Beugung von außen 48 Dreiecke in der Armbeuge um (Grenze 46) – verworfen.
- Kapuzenjacke in Stufe 0 auf 6,5 mm statt 10,5 mm vernetzt (das Ego-Bild nutzt Stufe 0).
- `src/anim/figures.cpp`: Unterarm 27 cm.

**Befund:**
- Besser: Der Ärmel ist schlank und verjüngt sich zum Handgelenk; die Raffung zum Bündchen ist zu sehen.
- Schlecht: Die Öffnung des Bündchens (2 cm tief, Wand 3–4 mm) wird im 6,5-mm-Netz zerfranst (`v13_3_r1_ego_buendchen_nah.jpg`).
- Schlecht: Die Haltung ist unverändert – der Arm kommt waagerecht vom linken Bildrand, außen steht der Ellbogen über der Schulter.

### Runde 2 – Armkette und Handgelenk (`v13_3_r2_*`)

**Rechnung vorab** (`armgeo.py`, Blick waagerecht, Schulter 20 cm unter und 10 cm hinter dem Auge, 17 cm seitlich): Mit Daumen oben braucht die Lampe in Blickrichtung einen Unterarm, der zur Hand abfällt. Eine natürliche Haltung – Ellbogen unter der Schulter, Unterarm steigt zur Hand – gibt es nur, wenn die Faust um die Lampenachse gedreht ist (Daumen nach außen, Handfläche schräg nach oben, wie beim Halten von unten).

| Daumen nach außen | Ellbogen zur Schulter | Unterarm | Ellbogen–Auge |
|---|---|---|---|
| 0° (vorher) | +13 cm darüber | fällt 16° | 31 cm |
| 20° | +7 cm | fällt 5° | 37 cm |
| 40° | 0 cm | steigt 6° | 41 cm |
| 50° | –4 cm darunter | steigt 12° | 42 cm |

(Handgelenk 30° gestreckt, Griff 19 cm unter und 10 cm links vom Auge.)

**Geändert:**
- `tools/blender/models/hand_grip.py`: Handgelenk 30° statt 14° gestreckt. 30–35° ist die übliche Stellung im Kraftgriff. Der Unterarm kommt damit mehr von außen statt von hinten, die Beugung im Ellbogen steigt von 58° auf 66°. Neu gebaut (`vm_hand_l`). **Finger und Nägel sind punktgleich** (Vergleich der glb: größte Abweichung 0,000 mm). Handballen höchstens 2 mm, Daumenballen 0,9 mm (Glättung nahe dem Handgelenk), nur der Unterarmansatz bewegt sich (bis 7,6 mm). Die Lampenmodelle sind unverändert.
- `src/gameplay/body.cpp`: Im Ego-Bild ist die Faust um die Lampenachse gedreht (`kLampRoll`, 40°, Daumen nach außen). Griff 2 cm tiefer (19 cm unter dem Auge) und 2 cm näher zur Mitte. Die Ansicht von außen (B-001) bleibt wie sie war.
- `figure_models.cpp`: Statt der tiefen Öffnung eine gerundete Kante (6 mm) mit flacher Mulde; die Öffnung ist dunkel gefärbt (Färbung je Punkt, `hoodieTint`). Naht der Raffung leicht dunkler, Bündchen 12 % dunkler (Schatten der Rippen).

**Befund:**
- Besser (Ego, alle Zustände): Der Unterarm kommt von links unten aus der Bildecke und steigt zur Hand. Der Ärmel ist am Bildrand nicht mehr breiter als die halbe Bildhöhe.
- Besser (außen): Ellbogen auf Schulterhöhe statt darüber, der Arm hält die Lampe gestreckt nach vorn. Die Kante des Bündchens ist glatt.
- Test B-001: Ego geradeaus Dehnung 2,0 statt 3,4, umgeklappt 0 statt 1.
- Schlecht: Der Ärmel ist ein glatter Gummischlauch – keine Falten in der Armbeuge, kein Stau über dem Bündchen.
- Schlecht: Die Schulter mit dem gehobenen Arm ist von hinten eine dicke Kugel (`aussen_armkette`, „hinten“, „nah_hinten“).
- Schlecht: Die Faust liegt mit 30,4–30,9° vom Blick knapp am Bildrand (Test: < 31°).
- Grenzwertig: Die Kante des Bündchens zeigt aus 26 cm noch Facetten.

### Runde 3 – Falten, Netzdichte, Schulter (`v13_3_r3_*`)

**Geändert** (`figure_models.cpp`, `body.cpp`):
- Falten als Verschiebung des Distanzfelds (`sleeveFolds`), in der Ruhepose angelegt, das Skinning nimmt sie mit:
  - Armbeuge: drei bis vier Querfalten auf der Innenseite des Ellbogens (Gauß ±4,5 cm um das Gelenk, nur zur Beuge hin), leicht schräg zu den Seiten.
  - Stau über dem Bündchen: der Ärmel schiebt sich auf 4–11 cm vor der Raffung in Ringen zusammen (Abstand 2 cm, Tiefe je nach Rauschen 60–100 %) und ist dort 1,2 mm weiter.
  - Rinnen der Falten bis 30 % dunkler (Färbung je Punkt).
- Netz der Jacke in Stufe 0: 40 % der Dreiecke bleiben (vorher 27 %), für Kante und Falten.
- Schulter: Deltamuskel unter der Jacke nur mit halbem Aufmass; Ärmel an der Schulter 6,1 statt 6,35 cm; der Schlauch beginnt 2 cm im Deltamuskel.
- Faust 3° weiter gedreht (43°) und 1 cm höher: Abstand zum Bildrand 29,6–29,8° statt 30,4–30,9° (Test < 31°).

**Zwischenschritt verworfen:** Querfalten der Beuge mit 3,4 mm Tiefe und 2,4 cm Abstand. Bei der stärksten Beugung (Ego dicht an der Wand, 160°) klappten 320 von 27 650 Dreiecken um (Grenze 1 % = 276). Mit 2,6 mm und 2,8 cm: 267.

**Befund:**
- Besser: Der Stau über dem Bündchen liest sich als Stoff (`aussen_unterarm_falten`, `debug_arm` „vl“, „unten“).
- Besser: Die Kante des Bündchens ist glatter (`buendchen_kante_r2_r3`), aus 26 cm aber noch mit einzelnen Facetten.
- Schlecht: Die Ringe sind zu gleichmäßig (Ziehharmonika). An einer Stelle entsteht eine scharfe dunkle Kerbe.
- Schlecht: Die Falten der Armbeuge sind kaum zu sehen. Im Ego-Bild liegt die Beuge ohnehin außerhalb, von außen ist sie bei gebeugtem Arm verdeckt.
- Unklar: Die Schulter von hinten ist nur wenig schlanker (`schulter_hinten_r0_r2_r3`: Runde 0, 2, 3).

### Runde 4 – ungleiche Falten, Kante, Oberarm (`v13_3_r4_*`)

**Geändert** (`figure_models.cpp`):
- Stau über dem Bündchen: Phase der Ringe großräumig verbogen (schräg, wellig), Tiefe stark wechselnd (0–100 %), einzelne Falten laufen aus; Tiefe 2,3 statt 2,6 mm.
- Kante des Bündchens mit 9 statt 6 mm gerundet.
- Oberarm schlanker: an der Schulter 5,8 cm (vorher 6,1), Mitte 5,5 cm (Umfang 34,5 cm, vorher 5,75 cm).
- Neue Spielerkamera-Ansicht „Wand“ (dicht vor einer Wand, stärkste Beugung).

**Befund:**
- Besser: Die Kante des Bündchens ist auch aus 26 cm glatt und rund (`buendchen_kante_r2_r3_r4`: Runde 2, 3, 4).
- Besser: Die Ringe sind ungleicher, die scharfe Kerbe ist zu einer flachen Delle geworden (`falten_r3_r4`).
- Ego (`ego_unterarm_nah`): Der Ärmel liest sich als Stoff – schlank, Stau vor der Raffung, Bündchen, Handgelenk kommt heraus.
- Wand: Die Lampe zieht wie bisher zurück und kippt; der Arm stört nicht.
- Schlecht: Die Ringe laufen von außen gesehen (`aussen_armkette` „vr“) noch fast parallel um den Arm.
- Offen: Schulter von hinten (siehe Restmängel).

### Runde 5 – Zickzack, Kerbe, Schulteransatz, Haltung von außen (`v13_3_r5_*`)

**Geändert** (`figure_models.cpp`):
- Stau über dem Bündchen: Die Phase hängt jetzt auch vom Winkel um den Arm ab – die Falten liegen schräg im Zickzack statt als Ringe. Großräumige Verbiegung halbiert (sie erzeugte Stellen, an denen zwei Falten zusammenlaufen), Beginn der Zone weich und je Seite verschieden. Rinnen 22 statt 30 % dunkler.
- Der Ärmelschlauch beginnt 5 statt 2 cm im Deltamuskel, an der Schulter 5,6 cm Radius.

**Gegenprobe zur Kerbe:** Mit abgeschalteten Beugefalten blieb sie, mit abgeschaltetem Stau verschwand sie. Ohne Stau liegt an derselben Stelle ein Zickzack-Muster der Strickstruktur: Dort wechselt die dreiplanare Textur die Projektionsrichtung, und die Rinne einer Falte wirkt an diesem Übergang wie eine Kerbe. Weichere Falten haben sie gemildert, nicht entfernt.

**Befund:**
- Besser: Die Falten über dem Bündchen laufen schräg und ungleich (`falten_r4_r5`: oben außen Runde 4 → 5, darunter Debug „vl“ Runde 4 → 5).
- Gleich: Ego-Bild in allen Zuständen wie Runde 4 (die Änderungen liegen im Stoff, nicht in der Haltung).
- Neu geprüft, die Haltung von außen (`aussen_eigene_haltung`, Schulteransicht ohne `egopose`): Der Ellbogen hängt neben dem Rumpf, der Unterarm hält die Lampe vor dem Bauch, beide Ärmel schlank; vorher hingen dort zwei Ballonärmel (`v13_3_vergleich_aussen.jpg`, untere drei Zeilen).
- Schulter von hinten: kaum verändert (Restmangel).

### Ergebnis (Vorher/Nachher aus denselben Kameras)

| Bild | Inhalt |
|---|---|
| `v13_3_vergleich_ego.jpg` | Spielerkamera: Ruhe, Rennen, Blick unten, Interagieren |
| `v13_3_vergleich_aussen.jpg` | Außen mit Ego-Haltung („vl“, „vr“) und mit eigener Haltung (vorn, „vl“, nah vorn) |
| `v13_3_vergleich_debug_arm.jpg` | Debug-Kamera um die Hand, 0,55 m |
| `v13_3_vergleich_finger.jpg` | Nahansichten der Hand aus Teil 1 |

- **Armkette:** Oberarm 29,9 cm, Unterarm 27,0 cm (vorher 25,4). Im Ego-Bild hängt der Ellbogen auf Schulterhöhe (vorher 16 cm darüber), Beugung 67° (vorher 59°), Abstand Ellbogen–Auge 42 statt 29 cm. Der Unterarm kommt von links unten und steigt zur Hand. Gerechnet (`armgeo.py`): Blick 37° nach unten – Ellbogen 16 cm unter der Schulter; 32° nach oben – 12 cm darüber (der Arm hebt die Lampe mit).
- **Handgelenk:** 30° gestreckt (vorher 14°), 33° zur Elle wie bisher. Die Faust ist im Ego-Bild 43° um die Lampenachse gedreht (Daumen nach außen); Unterarm des Körpers und des Handmodells weichen höchstens 0,7° voneinander ab (vorher 0,5°, Grenze 12°).
- **Ärmel:** Querschnitt einer Jacke von 5,6 cm (Schulter) über 4,95 cm (Ellbogen) auf 3,85 cm vor der Raffung, Bündchen 3,1–3,2 cm mit gerundeter Kante und dunkler Öffnung; Falten in der Armbeuge und Stau über dem Bündchen.
- **Finger:** punktgleich (glb-Vergleich: Finger und Nägel 0,000 mm Abweichung). Die Nahansichten zeigen dieselbe Hand; die Bilder sind gegeneinander gedreht, weil die Debug-Kamera im gedrehten Lampenrahmen kreist (gleiche Lage zur Hand, andere Bildlage).
- **Tests:** `gameplay_test` 446/0, darunter „Kapuzenjacke am gebeugten Ellbogen“ (B-001):

  | Haltung | vorher: Dehnung / umgeklappt | nachher |
  |---|---|---|
  | Ego geradeaus | 3,43 / 1 von 7 684 | 2,82 / 5 von 27 416 |
  | Ego nach unten | 2,69 / 0 | 1,95 / 1 |
  | Ego nach oben | 3,62 / 10 | 3,23 / 7 |
  | Ego an der Wand (160°) | 3,42 / 48 | 3,59 / 266 (Grenze 274) |
  | außen geradeaus | 1,96 / 1 | 2,37 / 8 (Grenze 68) |
  | außen nach unten | 1,71 / 2 | 2,09 / 21 (Grenze 68) |

  Das Netz der Jacke hat in Stufe 0 jetzt 3,6-mal so viele Dreiecke; die Grenzen sind Anteile (1 % bzw. 0,25 %). Unterarm passt zum Handmodell (höchstens 0,7°), Faust im Bild (höchstens 29,8° vom Blick). `audio_test` 106/0.
- **Kosten:** Spielerfigur Stufe 0 60 398 statt 40 666 Dreiecke; Modellieren beim Start 1,80 statt 1,43 s (im Hintergrund).

### Restmängel (V13.3)

- **Schulter von hinten:** Der Schulterbereich der Jacke bleibt von hinten breit und rund (`v13_3_r3_schulter_hinten_r0_r2_r3.jpg`). Ursache ist die Ruhepose (Arme 54° abgespreizt, B-001): Beim hängenden oder gehobenen Arm dreht der Ärmelansatz um das Schultergelenk und legt sich über Rumpf und Deltamuskel. Eine eigene Gewichtung oder Korrekturform für die Schulter würde B-001 erneut berühren und ist nicht umgesetzt. Im Ego-Bild ist die Schulter nicht zu sehen.
- **Beugefalten:** In der Armbeuge kaum sichtbar – im Ego-Bild liegt die Beuge außerhalb, von außen bei gebeugtem Arm verdeckt. Tiefer gingen sie nicht: Bei 160° Beugung klappten dann zu viele Dreiecke um (Test B-001).
- **„Ego an der Wand“** liegt mit 266 umgeklappten Dreiecken knapp unter der Grenze (274); fast alle in der Armbeuge, wo der Unterarm auf dem Oberarm liegt (Stichprobe: 62 von 67 auf der Beugeseite, verdeckt).
- **Dreiplanare Textur:** An Übergängen der Projektionsrichtung zeigt die Strickstruktur ein Zickzack; kreuzt eine Falte dort, wirkt sie wie eine Kerbe (nur in Nahansichten der Debug-Kamera).
- **Verdrehung im Unterarm:** Das Skelett hat einen starren Unterarmknochen ohne eigene Drehung (Pronation). Die lange Achse des Handgelenks steht dadurch 45–50° zur Lage des Ellbogens; der runde Bund verdeckt das, ein ovaler Bund ließe sich deshalb nicht passend ausrichten.
- **Debug-Kamera:** `vmcam` kreist im gedrehten Lampenrahmen; Bilder aus Teil 1 und V13.3 sind relativ zur Hand gleich, in der Bildebene aber gedreht.
- **Rechte Hand:** weiter die prozedurale Hand (wie in Teil 1 offen); ihr Ärmel und Bündchen sind neu.
