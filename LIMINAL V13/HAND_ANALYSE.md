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
