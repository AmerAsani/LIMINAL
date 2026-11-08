# Ausstattung Level 37 „Sublimity“ (V13.3 Teil 1)

Grundlage `LEVELSPEC_37.md`/`LOOK_37.md` (Wikidot Level 37: geflutete weiße Fliesenräume, keine Gegenstände, keine
Möbel, „nur Fliesen, Wasser, Licht“). Die einzige Ausstattung aus Dateien ist der Metro-Wagen aus Level 903, der im
gefluteten Tunnel hält und mit dem vorderen Ende in der Fliesenwand steckt (eigene Ergänzung, `LEVELAUSWAHL.md`).

**Klassen der Ebene (propdump):** `metro_car_37` (1 Stück), `safety_light` (1, Builder `models_l1_code.py`, gehört zu
Level 1), `water_surface@1/@2` (Wasserflächen, Code-Modell der Engine). Wandleuchten, Deckenlichter, Licht von unten
und die Treppen in die Gruben erzeugt die Engine aus der Karte (keine Modellklassen). Fliesen-Elemente, Leitern oder
Abläufe gibt es in der Karte nicht. Leitern widersprächen dem Wiki („viele Wasserflächen haben keinen Rand zum
Herausklettern“) und dem LOOK („keine Gegenstände“); neue Klassen hätten außerdem eine Kartenänderung über die
Variantenwahl hinaus verlangt – nicht gemacht. `metro_car_37` ist damit die einzige Klasse dieser Ebene in meinem
Auftrag. Builder: `tools/blender/models/models_l903.py` (gemeinsam mit dem Wagen von Level 903, `_build_car`).

## Klasse

| Klasse | Var. | Befestigung | Maße (T × B × H) | Dreiecke vorher | Dreiecke nachher | Stück | Materialien |
|---|---|---|---|---|---|---|---|
| metro_car_37 | 1 | Boden (Wagenboden auf PLAT + 0,01) | 19,44 × 3,60 × 3,52 | 3330 | 39 895 | 1 | 41: Aluminium blank/matt, Rot, Dichtung, Glas, Staubglas, Innen creme/grau, Boden, Sitz, Edelstahl, Lichtband, Anzeige, Unterboden, Drehgestell, Rad, Lauffläche, Feder, Bremsstaub … |

Bögen: `docs/screens/props/level37/vorher/metro_car_37.jpg`, `docs/screens/props/level37/final/metro_car_37.jpg`;
Übersicht `docs/screens/level37/props_uebersicht_vorher.jpg` und `props_uebersicht_final.jpg` (Runden r1–r5 ebenda);
an Ort und Stelle (Tür vom Wasser, Tunnel schräg, Wagenende, innen, Fahrwerk, Lampe innen)
`docs/screens/level37/props_insitu_vorher.jpg` und `props_insitu_final.jpg`.

Derselbe Wagen wie `metro_car_903` (Aufbau siehe `PROPS_Level903.md`, Abschnitt „Was gebaut ist“), mit drei
Unterschieden:
- Zielanzeigen: vorn (in der Wand) dunkel mit einem Leuchtpunktrest, hinten das Bildzeichen „Wagen von vorn“ (zurück zur
  Metro); die Außenanzeige hinten zeigt dasselbe Zeichen.
- Kein Gleis unter dem Wagen: Der Wagen fährt durch feste Materie (Wiki 903); im Tunnel liegt nur Wasser.
- Drehgestelle 12 cm tiefer: Die Räder stehen knapp im Wasser (Wasser 0,30 m, Radunterkante 0,26 m) statt 10 cm darüber
  zu schweben; die Luftfedern sind entsprechend länger.

## Runden
Die Runden liefen gemeinsam mit Level 903 (gleicher Builder); je Runde Kontaktbogen und sechs Bilder an Ort und Stelle.
- **Vorher:** Kasten aus Quadern, Fenster schwarze Flächen, Fahrwerk ein Quader über dem Wasser, Wagenende leer, an
  den Türen nur die gefliesten Stufen der Karte.
- **r1:** Wagen neu (wie 903). Befund: Bremsstaub als breite braune Welle über dem Wasser – wirkte wie Schlamm; Sitze
  fast schwarz; Lichtbänder überstrahlt.
- **r2:** Bremsstaub unregelmäßig und tiefer, Sitze blau ohne Teppichtextur, Lichtbänder gedämpft. Befund: Staub noch
  zu braun, Lampenansicht schwarz.
- **r3:** Staub dunkelgrau. Befund: Lampenansicht von außen bleibt schwarz – die unsichtbaren Wagenwände der Karte
  (y 7–8) werfen Schatten auf die Wagenseite (Restmangel); Ansicht in den Wagen verlegt.
- **r4:** Außenanzeige hinten größer (Bildzeichen lesbar).
- **r5:** Halteschlaufen und Werbekarten innen; Ringe hingen zu tief – gekürzt.
- **final:** keine neuen Befunde.

## Prüfungen
- `validate_models.py metro_car_37`: 0 Fehler (gemeinsamer Lauf mit Level 903: 21 Modelle, 0 Fehler).
- `gameplay_test`: 647 Prüfungen, 0 Fehler; „Halt der Ausstattung“ level37: 2 Objekte, 0 ohne Halt.
- `room_metrics.py --check` (alle Ebenen): OK (Level 37: 338 Räume, verstellt 0/0).
- Abschluss nach `git merge liminal-v13`, neu gebaut: alle Testprogramme grün (gameplay 647/0, audio 106/0, worldgen
  29 658/0, Koop 209/0, supply, net 72/0, Transport 90/0).
- Budget: 43 284 Dreiecke Ausstattung (Wagen 39 895).

## Restmängel (ehrlich)
- Die Wagenseite zum Wasser bekommt kein direktes Licht (Schatten der unsichtbaren Kollisionsblöcke der Karte); im
  Lampenlicht bleibt sie dunkel.
- Glas ist deckend; die Hauptscheiben sind heruntergelassen, damit man die Fliesen durch den Wagen sieht.
- Die Räder stehen 4 cm im Wasser; unter Wasser liegt kein Gleis (bewusst, Wiki: fährt durch feste Materie).
- Außer dem Wagen keine eigenen Modellklassen in dieser Ebene (siehe oben).

## Aufwand
In den rund 8 Stunden von Level 903 enthalten (gemeinsamer Builder); eigener Anteil etwa 0,5 Stunden (Varianten-
Parameter, Bilder an Ort und Stelle, Dokument). Runden: vorher, r1–r5, final.

## Nachträge der Abnahme (08.11.2026): Wasser nach Teil 1

Die Wasserflächen (`water_surface@1` flach, `@2` tief; Code-Modell, je Raum und Durchgang eine Fläche) sind hier die
größte „Objektklasse“. Geprüft nach den Mindestanforderungen für Wasser (Auftrag Teil 1), Runden 2–6 in
`KRITIK_Level37.md`, Vergleich `docs/screens/level37/wasser_vorher_nachher.jpg`:

| Anforderung | Stand |
|---|---|
| Rand ohne harte Polygonkante | Becken reichen von Wand zu Wand; an der Wand eine schmale Wasserlinie (2 cm dunkler, darüber 9 cm leicht) |
| dunklere nasse Umgebung | glasierte Fliesen werden nass nicht dunkler; Wasserlinie an der Wand (s. o.) |
| Reflexion mit Fresnel | Bildraum-Spiegelung und Decke als Ersatz (seit V13.3), dazu neu: das Wasser spiegelt das Licht der Lampen als wanderndes Netz an Wände und Decke |
| leichte Tiefenwirkung | neu: Boden um die Tiefe versetzt (Brechung 1,33, Parallaxe), in der Tiefe weicher; Lichtnetz mit der Tiefe schwächer; Lampen unter Wasser als Schein |
| Ripples bei Tropfen | neu: Tropfenringe alle 7–13 s in 60 % der 4-m-Zellen, sichtbar im Glanz |
| keine flache, harte Glasfläche | Wellen, Brechung, Lichtnetz auf dem Grund |

Bilder: `docs/screens/level37/wesen_r2_*` (vorher) … `wesen_r6_*`; Einzelbilder `build/qa/creatures/level37/r2..r6/`.
