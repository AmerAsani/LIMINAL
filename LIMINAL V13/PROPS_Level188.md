# Ausstattung Level 188 „Der Hof der Fenster“ (V13.3 Teil 1)

Alle 14 Objektklassen von Level 188 in Blender neu (`tools/blender/models/models_l188.py`). Der Geometrie-Kern kommt aus
`models_l53.py` (`Geo`, `sweep`, `lathe`, `tube`). Grundlage sind `LEVELSPEC_188.md` und `LOOK_188.md` (Wikidot
Level 188): ein zu großer, weißer, kalter Innenhof mit Fensterreihen wie ein Bürohaus, das nach innen schaut. Hinter
den meisten Jalousien ist nichts, hinter manchen schwaches Licht, in wenigen ein dunkler Umriss. Unten Moos, ein Weg
aus Platten, ein blasser Kreis aus Birkenholz, ein kleiner Mahagonitisch mit Gegenständen. Dazu Mahagonitüren,
Glasgalerien, weiße Stützen und das Oberlicht.

**Bauweise und Randbedingungen**
- **Glas ist im Spiel deckend.** Die Scheibe ist deshalb die Rückfläche des Fensters (x = 0,003, glatt und spiegelnd,
  mit schwachen Formen dahinter: Türrahmen, Deckenkante, Spiegelstreifen). Jalousien liegen davor. Bei erleuchteten
  Fenstern ist die Rückfläche eine Leuchtfläche in acht Stufen, heller zur Lampe hin.
- **Fensterwesen** (`window_188_c`, beide Varianten):
  - Rückfläche bis x = 0,005, frei zwischen 0,0055 und 0,026 (Ansicht und Umriss des Wesens);
  - Pfosten, Kämpfer und Flügelrahmen ab 0,026, Blendrahmen nur außerhalb der Öffnung (|y| ≥ 0,6, |z| ≥ 0,9);
  - geprüft mit `scratchpad/v13/wincheck.py`: 0 Punkte im Freiraum, Rückfläche bis x = 0,0036;
  - `presence.cpp` wählt die Fenster nach Modellnamen, die Variante stört nicht.
- Die Fensterbank (Oberseite z = −0,97, Tiefe 0,2 m) trägt weiter die Funde im Erdgeschoss.
- Rauheit/Metall je Material:
  - Fensterlack `r` 0,75, vorher spiegelte er unter der Lampe;
  - Aluminiumbank `r` 0,8, `m` 0,45; Edelstahl `r` 0,55, `m` 0,45;
  - Mahagoni `r` 1,3–1,7, Birke 1,8–2,0;
  - Beton 1,2–1,4.

## Klassen

Maße: Tiefe (x) × Breite (y) × Höhe (z) in m, Variante 0. „Stück“ = Anzahl in der Karte. Bögen:
`docs/screens/props/level188/vorher/<name>.jpg` und `.../final/<name>[_vN].jpg`, Nahansichten `.../final/<name>_nah.jpg`.
Übersichten: `docs/screens/level188/props_uebersicht_vorher.jpg` und `..._final.jpg`.

| Klasse | Var. | Befestigung | Maße | Dreiecke vorher | nachher | Stück | Mat. | Was das Modell jetzt hat |
|---|---|---|---|---|---|---|---|---|
| window_188_a | 2 | Wand | 0.20 x 1.38 x 1.99 | 960 | 620/620 | 47 | 12 | Blendrahmen mit Stufenprofil und Gehrung, Silikonfuge zur Wand, Dichtung, Pfosten und Kämpfer mit Fase, vier Flügelrahmen mit Glasleisten, Aluminiumbank mit Tropfnase und Endkappen, Staub auf der Bank; Jalousie fast zu (Kasten, gewölbte Lamellen, einzelne staubig, Leiterkordeln, Unterschiene, Zugschnur mit Eichel). v1 geknickte Lamelle, schiefe Unterschiene, Schmutzläufer am Rahmen |
| window_188_b | 2 | Wand | 0.20 x 1.38 x 1.99 | 792 | 556/584 | 33 | 14 | wie a, Jalousie halb, darunter dunkle Scheibe; v1 zwei Drittel, Lamellen fast waagrecht, Schmutz |
| window_188_c | 2 | Wand | 0.20 x 1.38 x 1.99 | 576 | 342/346 | 37 | 9 | ohne Jalousie, dunkler Raum mit schwachen Formen und Spiegelstreifen; Freiraum für das Fensterwesen; v1 andere Formen, Schmutz am Rahmen |
| window_188_fig | 1 | Wand | 0.20 x 1.38 x 1.99 | 589 | 473 | 11 | 16 | erleuchteter Raum (acht Stufen), dunkler Umriss ab der Hüfte mit Schultern, Hals und Kopf (Größe wie das Fensterwesen) |
| window_188_lit | 1 | Wand | 0.20 x 1.38 x 1.99 | 816 | 666 | 16 | 18 | Jalousie halb vor schwachem Licht |
| window_entry_188 | 1 | Wand | 0.20 x 1.38 x 1.99 | 660 | 610 | 1 | 17 | warmes Licht des Hotels, Jalousie oben |
| door_mahogany | 1 | Wand | 0.11 x 1.10 x 2.28 | 916 | 556 | 10 | 8 | weiße Zarge mit profilierter Bekleidung (Gehrung oben), Blatt mit vier erhabenen Kassetten (Fase, Profilleiste), Messingknauf mit Rosette, Schlüsselschild, drei Bänder, Griffspuren, Schattenfuge am Boden |
| door_mahogany_open | 1 | Durchbruch | 1.08 x 1.12 x 2.29 | 916 | 966 | 4 | 7 | Futter durch die ganze Wand, Bekleidungen auf beiden Seiten, Blatt mit Kassetten und Knauf auf beiden Seiten, um 75 Grad an der Zarge geöffnet (vorher drehte es sich um die Mitte des Durchgangs) |
| paving_strip | 3 | Boden | 1.00 x 1.60 x 0.06 | 12 | 450/377/324 | 102 | 8 | Betonplatten mit Fase, leicht verkantet, abgeplatzte Ecken, Randsteine, Fugen mit Erde und Moos, Moos am Rand, Flecken und Haarriss; v1 2 × 2, v2 3 × 1 Platten |
| birch_circle | 1 | Boden | 7.00 x 7.00 x 0.13 | 1432 | 2012 | 1 | 9 | Dielen aus Birke (drei Töne, einzelne vergraut) mit runden Enden und Fugen, Schrauben über den Lagern, gebogener Randstreifen mit Rundung, Moos unten am Rand, Deckel (Fuge) mit eingelassenem Ringzug |
| pillar_188 | 1 | Boden | 1.10 x 1.10 x 30.00 | 248 | 2602 | 3 | 5 | Sichtbeton mit Brettschalung (Bretter als Facetten in zwei Tönen), Arbeitsfugen alle 2,5 m mit Ankerlöchern, Sockel mit Fase, Schmutz und Moos am Fuß, Stahlplatte oben |
| skylight_188 | 1 | Decke | 8.10 x 8.10 x 0.08 | 228 | 1539 | 22 | 6 | 64 Mattglasfelder (einige verschmutzt), Laub auf dem Glas, T-Sprossen alle 1 m, Hauptprofile alle 4 m, Knotenbleche mit Schrauben, Rost an einzelnen Blechen |
| table_188 | 1 | Boden | 0.91 x 0.91 x 1.49 | 1778 | 2562 | 1 | 18 | Platte mit Profilkante, Zarge, gedrehte Säule, drei geschwungene Beine mit Messingschuhen; zwei Fotografien im Rahmen mit Stütze (Person, Sepia), drei Bücher mit Schnitt, Porzellanvase mit blauem Band und verzweigten trockenen Zweigen, Wecker mit zwei Glocken, Zifferblatt mit Strichen und Zeigern |
| walkway_glass | 1 | Wand | 1.60 x 1.00 x 1.46 | 160 | 182 | 12 | 7 | Betonplatte, Stahlkonsole (Doppel-T, zur Spitze schlanker) mit Wandplatte und Schrauben, Randprofil mit Tropfkante, Bodenschiene mit Deckblech und Schrauben, Glasscheibe mit Spiegelstreifen und grünen Kanten, U-Kappe und Handlauf aus Edelstahl |

Die Studiokabine zeigt die offene Tür im 2 m tiefen Durchbruch kaum; Bilder in der echten Ebene:
`docs/screens/props/level188/r3/door_mahogany_open_ebene.jpg`.

## Varianten und Karte

Varianten der häufigen Klassen: window_188_a, window_188_b und window_188_c je 2, paving_strip 3.

**Karte** (`tools/levels/level188.py`):
- Am Ende wird je Objekt `variant = vrng.randrange(n)` gezogen, mit eigenem Zufallsgenerator (Seed 18831).
- Die Anordnung bleibt unverändert. Vergleich vorher/nachher: 316 Objekte, 0 mit anderer Lage.
- Verteilung: Fenster a 23/24, b 14/19, c 19/18; Pflaster 25/37/40.

## Runden
Werkzeuge wie in `PROPS_Level5_3.md`. Die Hülle `l53_sheet.py` führt Fenster, Mahagonitür und Galerie als Wandteile
(ihre Namen stehen nicht in `mapkit.WALL_MOUNT`). Als Vorkommen nimmt sie das erste im Erdgeschoss mit Wand dahinter,
sonst hing das Fenster in der Kabine bei 20 m oder stand am Boden.

### vorher (V12-Fassung)
- **Fenster**: Rahmen aus Quadern ohne Profil, Lamellen als flache Quader, Pfosten vor den Lamellen, Bank als Quader,
  der Lack glänzte unter der Lampe; Umriss in `fig` 2,6 m groß.
- **Türen**: Quaderblatt mit Quaderkassetten, Zylinderknauf; die offene Tür drehte sich um die Mitte des Durchgangs.
- **Pflaster**: ein Quader (12 Dreiecke) mit Pflastertextur.
- **Birkenkreis**: Zylinder und Torus, Fugen als Linien.
- **Stütze**: zwei Zylinder. **Oberlicht**: Leuchtquader mit Quadersprossen.
- **Tisch**: Grundkörper. **Galerie**: Quadersteg mit Glasquader.

### Runde r1 (alle Klassen neu, 19 Modelle)
- **erleuchtete Fenster** (`lit`, `entry`, `fig`): Die Lampe dahinter war eine hart begrenzte Ellipse.
  - Änderung: weiche Ringe.
- **Umriss in `fig`**: 2,6 m groß und dünn.
  - Änderung: ab der Hüfte, Schultern, Kopf wie das Fensterwesen.
- **Birkenkreis**: Dielen enden treppig.
  - Änderung: runde Enden auf dem Kreis.
- **Galerie**: zu schlicht, das Glas liest sich nicht.
  - Änderung: Konsole, Randprofil, Spiegelstreifen.
- **Mahagoni**: zu glänzend.
  - Änderung: `r` 1,3–1,7.
- **Tür, Tisch, Oberlicht, Pflaster, Fenster a/b/c**: gut.

### Runde r2
- **Birkenkreis**: Der Randstreifen fehlt (Normalen nach innen, Rückseiten werden ausgeblendet).
  - Änderung: Normalen gedreht.
- **Leuchtflächen**: vier Stufen als Ringe sichtbar.
  - Änderung: acht feinere Stufen.
- **Fenster**: Rahmen flach, nur Blendrahmen.
  - Änderung: Flügelrahmen mit Glasleisten je Scheibe (ab x = 0,026).
- **offene Tür**: Kameras im Durchbruch angepasst.

### Runde r3 (mit Bildern in der Ebene)
- Randstreifen des Kreises rund und sichtbar, Flügelrahmen gut, Stufen kaum noch sichtbar.
- Offene Tür in der Ebene: liegt geöffnet an der Laibung, Kassetten und Knäufe auf beiden Seiten.
- Keine Änderung.

### Runde r4
- **Pflaster**: Platten zu sauber und gleich.
  - Änderung: Flecken und Haarriss auf einzelnen Platten.
- **Stütze**: Brettschalung kaum zu sehen.
  - Änderung: mehr Kontrast zwischen den Brettern.

### Runde r5
- **Pflaster**: Flecken als dunkle Kleckse.
  - Änderung: weicher (näher am Plattenton).
- Stütze abgenommen.

### Schlussrunde final (nach Merge von liminal-v13 a8afc3f, neu gebaut)
Alle 19 Bögen, Übersicht `docs/screens/level188/props_uebersicht_final.jpg`, Nahansichten `final/*_nah.jpg`. Keine neuen
Befunde.

## Prüfungen
- `validate_models.py` für alle eigenen Klassen beider Ebenen: 47 Modelle, 0 Fehler.
- `gameplay_test` 647/0. „Halt der Ausstattung“: level188 316 Objekte, 0 ohne Halt.
- `room_metrics.py --check level188`: Raummetrik OK (Decke 2,80 m, Gang 3,0 m, verstellt 0/1).
- **Dreiecke der Ausstattung von Level 188** (316 Objekte): 0,19 Mio. (vorher 0,14 Mio.). Davon die eigenen Klassen
  0,17 Mio. (vorher 0,14 Mio.).
- Übrige Tests siehe `PROPS_Level5_3.md`.

## Restmängel (ehrlich)
- **Glas ist deckend**:
  - Jalousien liegen vor der Scheibe statt dahinter.
  - Die Glasbrüstung der Galerie ist eine getönte, spiegelnde Platte, man sieht nicht hindurch.
- **Leuchtflächen** sind Stufen, kein echter Verlauf. Aus der Nähe sind die Ringe schwach zu erkennen.
- **Offene Tür**: Im Studio nicht sinnvoll abzubilden (2 m tiefer Durchbruch); beurteilt mit Bildern in der Ebene.
- **Galerie**: Die Konsole reicht 0,14 m tiefer als die alte Platte (Hülle −0,385 statt −0,25 m). Das ist eine
  Abweichung von rund 10 % der Höhe.
- **Fensterwesen**: Ansicht und Umriss des Wesens wurden nicht geändert. Sie sitzen weiter flach auf der dunklen
  Rückfläche.
- **Pflaster**: An den Ecken des Fünfecks überlappen die Streifen wie vorher (Anordnung der Karte unverändert).
- **Stütze**: Die Brettschalung wiederholt sich gleichmäßig.

## Aufwand
Gemeinsam mit Level 5.3 erfasst (`PROPS_Level5_3.md`, Abschnitt Aufwand): Runden vorher, r1–r5, final; Nahansichten ab
r1, Bilder in der Ebene in r3.
