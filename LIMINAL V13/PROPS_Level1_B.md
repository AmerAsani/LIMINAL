# Ausstattung Level 1 – Teil B: Blender-Modelle (V13.3)

Stand 27.10.2026. Teil A (Modelle, die bisher im Code entstanden: Regale, Leuchtbänder, Rohre …) steht in `PROPS_Level1.md`,
die neuen Sektoren (Ouroboros, Garden, Fabled) in `PROPS_Level1_Sektoren.md`. Builder: `tools/blender/models/models_l1.py`.
Kontaktbögen: `docs/screens/props/level1/<runde>/<klasse>.jpg` (Zeilen 50 cm / 2 m im Licht der Ebene, 50 cm / 2 m nur mit
streifender Lampe; Spalten 4 Winkel), Übersichten `docs/screens/level1/props_uebersicht_<runde>[_B].jpg`.

Wiki-Bezug (Wikidot Level 1): schlichte Holzkisten (oft auf Regalen), Haufen aus Holz, Verpackung und Metall, farbige
Stützen als Merkpunkte, Gothic mit Bögen und runden Säulen, Schwingtüren mit Notausgangsschild, Büros mit hängender
Glühbirne, Krankenzimmer (Bett in der Mitte, Beistelltische), Gummizellen, Räume mit Bildern an den Wänden und auf dem Boden.

## Klassen

Dreiecke je Stück vorher → nachher; Stückzahl in der Karte (V13.3). Größe des Modells (Breite, Höhe, Tiefe in m).

| Klasse | Befestigung | Stück | Dreiecke vorher → nachher | Größe | Was das Modell jetzt hat |
|---|---|---|---|---|---|
| arch_tall3, arch_tall4 | Durchbruch | 1, 7 | 352 → 2 424 / 2 584 | 1,09 × 3,4/4,4 × 1,9/2,1 | Bogen mit Laibung und Kämpfer, Abdruck der Brettschalung im Beton, Kantenabrieb |
| column_round | Boden | 48 | 1 120 → 4 720 | Ø 1,15 × 5,6 | Rundsäule mit Arbeitsfuge, Fuß mit Schmutzsaum, Abplatzer |
| crate_a/b/c | Boden | 5/7/7 | 1 628/1 628/2 068 → 1 394/1 887/2 554 | 0,64–1,17 | einzelne Bretter mit Fugen, Nägel, Eckbeschläge und Kantenschutz, Schablonenpfeile, Etikett, Abnutzung |
| debris_a/b | Boden | 7/7 | 500/360 → 2 278/2 018 | 1,9–2,4 | Haufen aus Brettern, Kanthölzern, Karton und Blechstreifen (per Starrkörper aufeinander gefallen), Spanngurt |
| exit_sign_green | Wand | 6 | 312 → 436 | 0,4 × 0,2 | Leuchtkasten mit Gehäuse, Abdeckscheibe, Piktogramm, Wandhalter |
| hanging_bulb | Decke | 83 | 604 → 1 282 | 0,75 lang | Kabel, Baldachin, Fassung, Birne mit Glühfaden-Leuchtteil |
| hospital_bed | Boden | 18 | 2 064 → 7 068 | 2,16 × 0,96 × 1,05 | Rohrgestell mit Rollen, Seitengitter, Matratze (quer unterteilt), Kissen, Decke weich gefaltet |
| padded_panel | Wand | 278 | 2 412 → 2 326 | 1,0 × 2,4 | 2 × 4 gesteppte Kunstleder-Kissen, Nähte, Knöpfe, seidiger Glanz, unterste Reihe abgegriffen |
| painting_a–f | Wand | 6/9/5/6/4/7 | 213–248 → 528–1 921 | 0,6–1,5 | Rahmen mit Profil, Leinwand mit Pinselstruktur und Verläufen, Aufhänger |
| painting_lean_a–c | Boden an Wand | 3/8/7 | 213–248 → 422–1 318 | 0,7–1,0 | angelehnte Bilder, Rückseite mit Keilrahmen |
| pillar_band*_ (5) | Stütze | 1–5 | 48 → 737–1 475 | 1 bzw. 2 m | Farbband um die Stütze mit Kantenabplatzern, Laufspuren, Schmutz am Fuß |
| swing_door1/2 | Durchbruch | 0/6 | – / 852 → 1 577 / 3 082 | 0,95 / 1,78 breit | Flügel mit Sichtfenster (Gummidichtung), Trittblech, Bändern, Zarge |
| work_table | Boden | 6 | 576 → 3 974 | 1,72 × 0,8 | Stahlgestell, Holzplatte mit Gebrauchsspuren, Schraubstock, Werkzeug, Ablage |

Summe Level 1 (Teil B, alle Stücke): 0,88 Mio. → 1,30 Mio. Dreiecke. `tools/qa/validate_models.py`: 29 Modelle, 0 Fehler
(vorher hatten swing_door1/2 36 % entartete Dreiecke).

## Runden

**Vorher** (`props_uebersicht_vorher_B.jpg`, Bögen `props/level1/vorher/`): Kisten waren Quader mit Brettertextur, die Haufen
einige Bretter ohne Verpackung und Metall, die Säulenbänder offene Farbkästen (von oben hohl), das Polsterfeld flache
Streifen, die Bilder flache Farbflächen, das Krankenbett ein Rahmen mit Brett, der Werktisch eine Platte auf Beinen.

**Runde 1 – Neubau** (Agent, Commit 2dc2240): alle Klassen neu aus Einzelteilen (siehe Tabelle). Befund danach (`r1`):
Holz zu gelb und gleichmäßig, Matratze ein Block, Polsternähte zu breit und zu viele Dreiecke, Säulenfuß sauber,
Glühbirne zu fein, Bilder flach gemalt, Werkbankplatte mit Rillen wie Wellblech.

**Runde 2** (Agent, Commit e8a57ee und unterbrochener Stand): Holztöne entsättigt, Matratze quer unterteilt, Decke weich,
Polsternähte schmal (886 Dreiecke), Säulenfuß mit Schmutzsaum, Glühbirne sparsamer, Bilder mit Verläufen und Pinselstruktur,
Werkbankplatte glatt mit Gebrauchsspuren. Die Runde wurde unterbrochen (Bögen ab `pillar_band_blau` fehlen).

**Runde 3** (übernommen, alle 28 Klassen, Bogen `props_uebersicht_r3.jpg`, Prüfbogen der Nahansichten): Kisten, Haufen,
Bett, Bilder, Schild, Birne und Werkbank überzeugen nah und mit streifender Lampe (Brettfugen, Nägel, Pinselstruktur).
Schlecht: das Polsterfeld wirkt flach – die Kissen lesen sich weder im Licht noch unter der Lampe als Wölbung. Werkzeugfehler:
Kabinen fern vom Spieler blieben schwarz (die Welt lädt nur um den Spieler) → `propsheet.py` stellt den ausgeblendeten Spieler
in jede Kabine.

**Runde 4** (Polsterfeld): feinere Teilung (11 statt 6 Stützstellen je Kissen), tiefere Nähte, pralle Kissen, Kunstleder
seidig (r = 0,7) statt stumpf. Neu schlecht: zufällig abgegriffene Einzelflächen sahen wie Pixelquadrate aus.

**Runde 5** (Polsterfeld): Einzelflecken verworfen, nur die unterste Reihe abgegriffen. Unter der streifenden Lampe stehen
die Kissen jetzt rund heraus, im Raumlicht zeigen Glanz und Nähte die Steppung. Übernommen.

## Restmängel (ehrlich)

- Die Lampenansichten der Durchbruch-Modelle (Bögen, Schwingtüren) sind im Studio zu dunkel: die Testlampe steht dort
  hinter der Wand des Vorraums. Die Modelle selbst sind im Raumlicht geprüft; das Werkzeug wird nachgebessert.
- Säulenbänder stehen im Studio ohne Stütze und wirken dort wie Quader; im Spiel umschließen sie die Stütze (geprüft im
  Laufweg der Ebene).
