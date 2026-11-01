# Biom-Audit der Ebenen (V13.3)

Stand 27.10.2026. Je Ebene: welche Zonen (Biome) die Karte hat, ob das Wiki (Wikidot) sie beschreibt, was entfernt,
behalten oder ergänzt wird, mit Karten vorher und nachher (`docs/screens/biom/<level>_vorher.png`, `_nachher.png`,
erzeugt mit `tools/qa/mapimage.py`; Farben je Zone, Legende mit Flächenanteil). Grundlage: `STRUKTURTYP.md`.

Regeln (Auftrag V13.3, Teil 2): Labyrinth-Ebenen haben eine Grundstruktur durchgehend und keine Wahrzeichen; Zusatzzonen
nur, wenn das Wiki sie beschreibt; selbst ergänzte Zonen werden entfernt; Variation nur im Kleinen.
Ähnlichkeit der Layouts: `tools/qa/similarity.py` (S = mittlere Ähnlichkeit der 24-m-Fenster zum Mittel der Ebene,
Ausreißer = Anteil der Fenster unter 0,75).

## Level 1 – Mischform (fertig)

| Zone | Fläche vorher | nachher | Wiki-Beleg | Entscheidung |
|---|---|---|---|---|
| aquila, aquila_dunkel | 22 %, 15 % | 18 %, 12 % | Sektor Aquila | behalten (dunkler Teil = eigener Stromkreis fürs Flackern) |
| gothic | 19 % | 16 % | Sektor Gothic | behalten |
| gild_ocker/blau/rosa/gruen | je 5 % | je 4–5 % | Sektor Gild (bunter, die meisten Kisten) | behalten |
| gaenge, zimmer, buero, kranken, bilder, ziegel, polster | 10 %, 3 %, 3 %, 3 %, 2 %, 1 %, 1 % | 9 %, 3 %, 3 %, 2 %, 1 %, 1 %, 1 % | The Corridors und ihre Räume | behalten |
| rohr | 1 % | 1 % | glatter Übergang nach Level 2 (Rohre und Anzeigen) | behalten |
| **ouroboros** | – | 2 % | Sektor Ouroboros (kleinster, ewige Baustelle) | **ergänzt** |
| **garden** | – | 8 % | Sektor Garden (überwuchert, Verwandlung) | **ergänzt** |
| **fabled** | – | 7 % | Sektor Fabled (Holz, Zinnoberrot, Neon, Kabel) | **ergänzt** |

Selbst ergänzte Zonen: keine. Karten: `docs/screens/biom/level1_vorher.png`, `level1_nachher.png`.
Ähnlichkeit: S 0,823 (Mischform, keine Grenze verletzt). Raummetrik nachher: Decken ≥ 2,8 m, Sichtachsen Median 36,8 m
(vorher 31,5 m), 15 große Räume (vorher 11).

## Level 0 – Labyrinth

| Zone | Fläche | Wiki-Beleg | Entscheidung |
|---|---|---|---|
| vorzimmer | 65 % | Grundstruktur: zufällig zerteilte Räume | behalten |
| saeulen | 13 % | Pillars | behalten |
| dunkel | 9 % | Blackout Zones | behalten |
| loecher | 5 % | Holes | behalten |
| rot | 4 % | Red Rooms | behalten |
| boegen | 2 % | Arches | behalten |
| manila | < 1 % | Manila Room | behalten (kaum zu finden, wie im Wiki) |
| naht0/naht1 | < 1 % | Ausgang: flackernde Wand nach Level 1 | behalten |
| Benennungen „Der lange Flur“, „Vergilbtes Zimmer“ | – | keine (Wahrzeichen) | **entfernen** (Räume bleiben Grundstruktur) |

Ähnlichkeit vorher: S 0,891, Ausreißer 13,3 %. Nachher: wird bei der Abnahme von Level 0 ergänzt.

## Level 2 – Labyrinth

| Zone | Fläche | Wiki-Beleg | Entscheidung |
|---|---|---|---|
| beton, ziegel (Tunnel) | 8 %, 8 % | Wartungstunnel (Beton oder Ziegel) | behalten, **Anteil auf ≥ 50 %** |
| anlage, lager | 23 %, 23 % | Industriekomplexe/Packhallen hinter Türen | behalten, **kleiner und seltener** |
| buero, buero_flur | 12 %, 3 % | Räume mit PCs, bürohafte Flure | behalten, kleiner |
| kammer | 7 % | Einzelräume | behalten |
| leere | 8 % | schwarze Leeren hinter Türen | behalten |
| maschine | 3 % | Maschinen | behalten |
| gleis | 3 % | alte Straßenbahngleise | behalten |
| heiss | 2 % | heiße Bereiche (Übergang Level 127) | behalten |
| **brownout**, **macchina** | 4 %, 2 % | Dauer-Brownout; verlassene Macchina-Bereiche | **ergänzt** |

Ähnlichkeit vorher: S 0,780, Ausreißer 35,7 % (zu viele Hallen). **Nachher** (`level2_nachher.png`): Tunnelnetz im
15-m-Raster nach Osten und Süden erweitert, Hallen von 46 % auf 21 % der Fläche verkleinert (Anlage 40 × 28 m, Packlager
40 × 26 m, Bürogang 38 × 33 m), Tunnel 16 % → 31 %, Brownout 4 %, Macchina-Werkstatt 2 %; Tür nach Level 3 aus dem
Ostnetz. S und Ausreißer siehe `docs/metrics/` (Prüfung grün).

## Level 3 – Mischform

| Zone | Fläche | Wiki-Beleg | Entscheidung |
|---|---|---|---|
| ziegel_* (Gänge, hinter Gittern) | 25 % → 40 % | Ziegelgänge, Gitter-Labyrinth | drei Gangnetze ergänzt (`level3_nachher.png`); 191 von 207 Räumen sind Gänge/Kammern (vorher 60 von 71) |
| fertigung | 31 % | Assembly Lines | behalten, kleiner |
| sanktum | 21 % | Sanctums | behalten, kleiner |
| generator | 19 % | Generator Rooms | behalten, kleiner |
| kessel | 3 % | Boiler Rooms | behalten |
| aufzug (Abgang, Kabine) | < 1 % | Aufzüge nach Level 4/5 | behalten |
| buero | 1 % | nur Menschen-Basen | **entfernt** (leere Ziegelkammer) |

## Level 5 – Eigener Ort

Zonen zimmer 36 %, ballsaal 25 %, flur 23 %, kessel 9 %, halle 5 %, metro 1 %, flur_alt 1 % – alle belegt (Main Hall,
Beverly Room, Boiler Room, Aufzüge, Übergänge). Entscheidung: behalten; Maße anheben (Flure ≥ 2,4 m, Decken ≥ 3,0 m).

## Level 5.3 – Eigener Ort

Zonen halle 44 %, gang 24 %, gang_alt 11 %, leere 8 %, alt 7 %, obergeschoss 4 %, flur_alt 2 % – belegt (Bibliothek,
Gänge, Leere mit Eisenstäben, Etagen, Zugangsflur). Entscheidung: behalten.

## Level 188 – Eigener Ort

hof 93 %, weiss 5 %, zelle 3 % – belegt. Entscheidung: behalten; Exit-Schild entfernen.

## Level 903 – Labyrinth (linear)

station 100 % – belegt. Entscheidung: behalten.

## Level 37 – Labyrinth

fliesen 100 % (Typen fliesenraum 52 %, saeulenhalle 19 %, grube 16 %, dunkelraum 6 %, tunnel 3 %, fliesengang 3 %) – alle
Typen belegt. Ähnlichkeit S 0,948. Entscheidung: behalten.

## Level 36 – Mischform

gang 33 %, halle 33 %, gate 18 %, laeden 16 % – belegt. **Ergänzt (V13.3, `level36_nachher.png`):** Sicherheitskontrolle
(außer Betrieb) 3 %, Fundbüro 1 %, Lounge 8 %, Meeting Place 6 % (Wiki).
