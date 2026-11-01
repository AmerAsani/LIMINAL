# Ausstattung Level 0 „Die Vorzimmer“ und Fundstücke (V13.3 Teil 1)

Teil A: alle eigenen Objektklassen von Level 0 neu in Blender (`tools/blender/models/models_l0.py`), Grundlage `LOOK_0.md`,
`LEVELSPEC_0.md`, `LEVEL0_STANDARD.md` (Wikidot Level 0: mono-gelbe Tapete, feuchter Teppich, summende Leuchtstoffröhren,
keine Möbel). Dazu die Tapete (Wiki „mono-yellow wallpaper“). Teil B: die Fundstücke und Interaktionsobjekte, die bisher im
Code als einfache Netze entstanden (`src/render/meshes.cpp`), neu in Blender (`tools/blender/models/models_items.py`).

**Bauweise:** gemeinsamer Baukasten `tools/blender/models/kit_items.py` – umlaufende Profile mit Gehrung um Rechtecke mit
gerundeten Ecken (Rahmen, Flansche, Lamellen), Profile entlang offener Linienzüge (Kämpfer, Zarge, Adern), Drehkörper mit
Winkel-Abwandlung (Petaloid-Boden, Kerben, Dellen), Schrauben mit Kreuzschlitz, Flecken als eigene Flächen. Jede Fläche hat eine
bewusste Richtung (Rückseiten zeichnet das Spiel bei der Ausstattung nicht). Gebrauchsspuren sind eigene Flächen 0,2–0,6 mm
über dem Grund: Staubhof, Staubfahne, Griffschmutz, Farbnasen, Insekten, Wasserfleck, Sprünge, Haarrisse. Rauheit/Metall je
Material über `r=`/`m=`.

**Varianten (Level 0: Variation nur im Kleinen, Grundstruktur bleibt):** `tools/levels/level0.py` wählt die Variante aus der
Lage (`var_at`, nicht aus dem Zufallsgenerator der Karte) – die Karte ist sonst bitgleich (1057 Objekte, nur das Feld
`variant` neu; geprüft gegen die alte `data/maps/level0.json`).

## Teil A – Level 0

Maße Tiefe (x) × Breite (y) × Höhe (z) in m. Dreiecke je Variante. „Stück“ = Anzahl in der aktuellen Karte (je Variante).
Bögen vorher `docs/screens/props/level0/vorher/<name>.jpg`, final `docs/screens/props/level0/final/<name>[_vN].jpg`;
Übersicht `docs/screens/level0/props_uebersicht_vorher.jpg` → `props_uebersicht_final.jpg`.

| Klasse | Var. | Befestigung | Maße | Dreiecke vorher | Dreiecke nachher | Stück | Materialien |
|---|---|---|---|---|---|---|---|
| troffer | 3 | Decke (Raster 0,6 m) | 1.21 x 0.61 x 0.012 | 388 | 438/438/476 | 650 (227/218/205) | 9–12: Gehäuseflansch, Türrahmen, Schattenfuge, Kehle, Staubkante, Prismenscheibe in 5 Leuchtdichten (Röhrenband, Nachbar, Mitte, Ende, vergilbter Rand), v1 sterbende Röhre (dunkel, schwarze Enden), v2 Wasserfleck, Insekten |
| troffer_off | 2 | Decke | 1.21 x 0.61 x 0.012 | 388 | 438/460 | 89 (44/45) | 10: wie oben, Scheibe matt (Röhren als hellere Bänder sichtbar); v1 Scheibe gesprungen und an einem Ende aus dem Falz gerutscht |
| troffer_red | 2 | Decke | 1.21 x 0.61 x 0.012 | 388 | 438/438 | 49 (25/24) | wie troffer, rote Leuchtdichten; v1 sterbende Röhre |
| ceiling_diffuser | 3 | Decke | 0.60 x 0.60 x 0.05 (Staubhof bis 0.66) | 1000 | 946/866/946 | 122 (45/45/32) | 9: Blech, Lamellenkante, Staub auf den Lamellen, Schacht (verzinkt, dunkel), Schraube, Staubhof an der Decke in 3 Stufen (Deckentextur `ceiling_office`, dunkler getönt); v1 fast sauber, v2 starker Hof und verbogene Lamelle |
| outlet | 3 | Wand, 0,32 m (über dem Sockel) | 0.010 x 0.151 x 0.081 (v0/v2 mit Griffschmutz auf der Tapete 0.18 x 0.11) | 596 | 1242/1226/1198 | 118 (39/41/38) | 10: Kunststoff leicht vergilbt, Einsatz, Topf, Stiftlöcher, Schutzkontakte (Metall), Mittelschraube, Griffschmutz, überstrichener Rand (Wandtextur); v1 sauber abgeklebt, v2 Haarriss in der Ecke, fehlende Schraube |
| vent_grille | 2 | Wand, 0,45 m unter der Decke | 0.014 x 0.54 x 0.32 (Staubfahne bis +0.03) | 684 | 834/914 | 14 (6/8) | 9: Lack, Kante, Staub auf den Lamellenoberseiten, Kanal, Abplatzer, Schrauben, Staubfahne an der Wand (2 Stufen, Wandtextur); v1 eingedrückte Lamelle, stärkere Fahne |
| arch3 | 1 | Durchbruch (Sturz 2,35 m) | 1.08 x 3.11 x 0.575 | 320 | 980 | 8 | 4: Putz (`plaster_white`, Ton der Bögen-Zone), Kantenrundung (abgegriffen, dunkler), Kämpferprofil (Rundstab, Kehle, Plättchen, mit Gehrung um die Wandecke), Haarrisse; `arch2`/`arch4` aus derselben Funktion mitgebaut (982/980, in keiner Karte) |

Weitere Klassen in Level 0 gehören anderen Teilaufträgen: `puddle` (6 Varianten) und `water_surface` (Wasser und Pfützen),
`safety_light` (`models_l1_code.py`, `PROPS_Level1.md`).

**Dreiecksbudget Level 0:** Ausstattung gesamt 502 808 → 634 683 Dreiecke (propdump, 1167 Stück; Grenze etwa 4 Mio.). Die
Rasterleuchte bleibt mit 438–476 Dreiecken sparsam (650 Stück = 293 000 Dreiecke).

**Bögen im Spiel:** im Studio stand der Bogen in einem Durchbruch bis zur Decke (Spalt über dem Sturz, Lampenzeilen dunkel), daher
zusätzlich feste Kameras in den Bögen von Level 0: `docs/screens/props/level0/boegen_im_spiel_final.jpg` (Putz geht in die
Putzwand über, Kämpfer mit Rückläufen, Kante von unten, streifende Lampe).

### Tapete

Die Wände wirkten auf 3–4 m glatt wie gestrichen (`docs/screens/props/level0/tapete_vorher.jpg`): die Raufaser (ambientCG
Wallpaper001A) hat nur feines Relief, das mit dem Abstand verschwindet. Raufaser wird in Bahnen von 53 cm geklebt –
`tools/textures/wallpaper_l0.py` macht daraus eine Kachel von 2,12 m mit vier Bahnen: feine Stoßfuge (Schattenlinie, eine leicht
aufstehende Kante, Normalen/Höhe/Verdeckung), jede Bahn ein wenig anderer Ton (andere Rolle, anders vergilbt), großflächige
Vergilbung und leichte senkrechte Schlieren. Einfarbig, ohne Muster (LOOK_0: kein Rautenmuster) – „mono-yellow wallpaper“.
Runde 1 war zu stark (wirkte wie Holzpaneele, `tapete_r1_zu_stark.jpg`), Runde 2 übernommen (`tapete_final.jpg`): auf
Distanz als geklebte Tapete lesbar, aus der Nähe weiter Raufaser. `textures.json`: `meters` 2,0 → 2,12; Lizenz CC0 bleibt
(LICENSES.md ergänzt).

## Teil B – Fundstücke und Interaktionsobjekte

Modelle `data/models/item_<name>.glb`; `src/render/item_models.cpp` ersetzt damit beim Start die Netze aus Code unter demselben
Namen (Aufruf am Ende von `MeshLibrary::init`); fehlt eine Datei, bleibt das alte Netz (wie bei der Taschenlampe). Lage,
Größe und Ursprung wie bisher. Bögen `docs/screens/props/items/vorher/<name>.jpg` (Netze aus Code) und `.../final/<name>.jpg`,
Übersicht `docs/screens/props/items/uebersicht_vorher.jpg` → `uebersicht_final.jpg`. Dreiecke vorher aus dem Code gezählt.

| Netz (Modell) | Befestigung | Maße | Dreiecke vorher | nachher | Materialien |
|---|---|---|---|---|---|
| energy | Boden, liegend | 0.22 x 0.068 x 0.024 | 1152 | 2504 | Folie (Atlas FM_ITEM_ENERGY): Schlauchbeutel mit Falten zur Naht, gezackte, längs geriffelte Quernähte, Einreißkerbe |
| almond | Boden, stehend (Griff 11 cm) | Ø 0.109 x 0.262 | 768 | 3136 | Flasche trüb (`look=glass`, wie das offene Mandelwasser), Etikett mit Klebenaht und Deckel mit Garantieband (Atlas FM_ITEM_ALMOND); Petaloid-Boden mit 5 Füßen, Griffrillen, Tragring |
| battery | Boden, stehend | Ø 0.033 x 0.0645 | 336 | 1944 | Atlas FM_ITEM_BATTERY: Mantel über die Kanten gelegt, Pluspol mit Isolierring, Minuspol mit Pressring, Delle; Schrift liest richtig herum (vorher gespiegelt) |
| note | Boden, liegend | 0.21 x 0.15 x 0.012 | 160 | 600 | Atlas FM_ITEM_NOTE: einmal gefaltet und aufgeklappt, flacher zweiter Knick, gewellte Ränder, Eselsohr mit leerer Rückseite |
| fuse | Boden, liegend | 0.176 x Ø 0.038 | 324 | 1152 | Atlas FM_ITEM_FUSE: Porzellan mit Aufdruck, Messingkappen mit Wulst, drei Kerben, Kontaktzapfen |
| fusebox / fusebox_empty | Wand (x = -0,04), Kasten z 1,08–1,70 | 0.61 x 0.58 x 3.05 (mit Rohr) | 522 / 198 | 4773 / 3621 | 17: Blechwanne mit eingekanteter Lippe, Tür mit Gummidichtung, Schloss, Bändern, Warnschild und Schaltplan in der Klarsichttasche; Montageplatte (Atlas), Hutschiene mit 2 LS-Schaltern, Bakelit-Halter, Messing-Federklemmen, Sicherung, Kontrolllampe (FM_FUSE_GLOW; leer: aus), drei Adern, Verschraubungen, PVC-Rohr mit Muffen und Schellen, Warnschild an der Wand |
| vending | Boden, Rücken an der Wand | 0.82 x 0.92 x 1.86 | ≈140 | 2096 | Atlas FM_VEND_BODY/GLOW/GLASS: Tür mit gerundeter Kante und Fuge, Chromrahmen ums Fenster, Glas (Fächer), Ausgabeklappe schräg in der Öffnung, Leuchtschild mit Rand, Bedienfeld mit 12 echten Tasten, Münzschlitz, Rückgabeknopf, Geldscheinprüfer, Rückgabeschale, Sockel mit Stellfüßen |
| exit_door | Wand (Ursprung in der Wandebene) | 0.13 x 1.07 x 2.40 | ≈172 | 1594 | 13: Stahlzarge (Profil mit Gehrung), Türblatt (Atlas: Lack, Aufkleber, Abrieb), Dichtung, Trittblech, Panikstange mit Gehäusen, drei Bänder, Obentürschließer mit Gleitarm, Schwelle mit Rillen, Rettungszeichenleuchte mit Schild (FM_EXIT_SIGN) und Betriebs-LED |
| elev_frame | Wand (Kern), Kabine x -2..0 | 2.06 x 2.52 x 2.64 | ≈168 | 2738 | 12: Rahmenprofil, Anzeigegehäuse mit dunkler Scheibe (die Etagenanzeige aus Code sitzt davor), Ruftableau mit Leuchtring, Schwelle mit Führungsnuten, Kabine mit Paneelen und Fugen, Sockelleiste, Gummiboden, Lichtfeld (FM_FLASH_LENS), Handlauf, Tableau innen, Schachtwand außen |
| elev_door | Türflügel (zwei je Aufzug) | 0.035 x 1.0 x 2.14 | 12 | 260 | lackiertes Blech, Gummikanten an beiden Seiten (beide Flügel sind dasselbe Netz) |

Die Taschenlampe gehört dem Viewmodel und bleibt. **Passung zu den neuen Fundstücken anderer Zweige:** das offene Mandelwasser
(`models_l1_phenomena.py`) hat denselben Flaschenkörper (Material `flasche_trueb`, Radius 5,4 cm, Höhe 26,2 cm) – geschlossene und
offene Flasche sehen gleich aus, nur Deckel und Inhalt unterscheiden sich. Leuchtstab (14 cm, `mesh_glowstick.cpp`) und Beil
(42 cm, `makeHatchet`) bleiben Code-Netze anderer Zweige; Maßstab und Materialweg (feste Materialien) passen zu den
Fundstücken hier.

**Prüfstand (`tools/qa/itemsheet.py`):** die Fundstücke stehen so im Spiel, wie das Spiel sie setzt – mit ihren festen
Materialien: Gegenstände über Spawnpunkte der Karte (je Lauf eine Art), Notiz und Sicherungskasten über Spawnpunkte (der zweite
Kasten wird mit Taste E geleert), Automat und Aufzug als Entities der Leveldatei in Nischen (Raum-Seed so gesucht, dass sie
gerade stehen; `core/rng.cpp` bitgenau nachgebaut), Notausgang über die Fundregeln weit vom Ursprung (Lage per Vorlauf
`propdump`). 4 Winkel, 50 cm / 2 m, Licht von Level 0 und streifende Lampe; kleine Fundstücke mit engem Ausschnitt (Bildwinkel 26°/40°).

**Engine (nur Laden):** `src/render/item_models.{hpp,cpp}` (neu), je eine Zeile in `meshes.cpp` und `world_renderer.cpp`.
Teile mit `fm=` bekommen das feste Atlas-Material; alle anderen werden wie Ausstattung beschrieben (`look`, `tex`, `r`, `m`) und
auf freie feste Materialplätze ab `FM_FIG_END` gelegt (31 Materialien); ihre UV sind Meter (Kastenprojektion aus Blender) und
werden auf die Kachelgröße der Textur umgerechnet, Tangenten aus den UV. `kFixedMaterials` 160 → 224 – nach den neuen Wesen
waren ab `FM_FIG_END` = 143 nur 17 Plätze frei (Runde 1: Teile von Notausgang und Aufzug fehlten).

## Runden

**vorher** (Code- bzw. alte Blender-Modelle):
- troffer: gleichmäßig leuchtendes Rechteck mit zwei Querstegen (gibt es an Prismenscheiben nicht), flacher Rahmen; keine
  Röhren, kein Schmutz, keine Vergilbung. troffer_off: graue Platte; troffer_red: rotes Rechteck.
- ceiling_diffuser: beste Klasse – aber Lamellen als Quaderringe ohne Schräge, Kern schwarz, keine Schraube, kein Staubhof.
- outlet: Kasten mit flachen Scheiben und Punkten; kein Topf, keine Schutzkontakte, keine Schraube, reinweiß.
- vent_grille: flacher Rahmen, Lamellen als gedrehte Quader, schwarz dahinter, zu sauber.
- arch3: glatter Segmentbogen mit scharfer Kante, Kämpfer als Leiste.
- Fundstücke: Riegel als glatter Stab mit Farbband, Flasche als weißer Drehkörper, Batterie und Sicherung mit gespiegelter
  Schrift, Notiz als gewölbte Fläche, Sicherungskasten aus Quadern, Automat aus Flächen, Notausgang und Aufzug aus Quadern
  (Aufzug dunkler Edelstahl).

**r1** (erste neue Fassung):
- troffer: Röhrenbänder, staubiger Rand, Schattenfuge, Insekten – gut. Banding stufig, aber übernommen.
- ceiling_diffuser: Staubhof kaum sichtbar → dunkler (3 Stufen dunkler getönt).
- outlet: Topf und Ausschnittkanten zeigten von vorn die Rückseite – die Wand schien durch (`docs/screens/props/level0/r1/outlet.jpg`)
  → Innenseiten zum Betrachter ausgerichtet (`fix_toward`).
- vent_grille: Lamellen standen 3,6 mm vor dem Rahmen → zurückgesetzt; Staubfahne heller als die Wand → dunkler.
- arch3: Studio ungeeignet (Durchbruch bis zur Decke, Lampenzeilen schwarz) → Bögen in Level 0 fotografiert.
- Fundstücke: feste Materialplätze erschöpft, Notausgang und Aufzug unvollständig (`docs/screens/props/items/uebersicht_r1.jpg`)
  → `kFixedMaterials` 224. Aufzug von außen schwarz (Kabinenwände nur innen) → Schachtwand außen. Kameras in den Nischen steckten
  in der Wand → Kamerabereich auf die Nische begrenzt.

**r2:**
- Varianten gebaut und in der Karte verteilt. troffer_red v1: schwarzer Block am Röhrenende zu hart (`r2/troffer_red_v1.jpg`)
  → Ende 0,45 statt 0,30; troffer v2: Wasserfleck schwarz → leuchtender, bräunlicher Hof.
- Tapete Runde 1 zu stark (Paneele) → Fugen, Tonsprünge und Normalen auf etwa ein Drittel; übernommen.
- Fundstücke: kleine Gegenstände zu klein im Bild → engerer Ausschnitt bei gleichem Abstand. Warnschild an der Sicherungskasten-Tür
  falsch (UV nach dem Aufschwenken berechnet) → UV vor dem Drehen. Aufzug: Metall mit Metalltextur im schwachen Licht schwarz →
  lackiertes Blech.

**r3:**
- troffer v2: Wasserfleck jetzt orange und zu satt → blasser (0,97/0,84/0,62, Leuchtdichte 0,68).
- Bögen im Spiel: Putz geht in die Wand über, Kämpfer mit Rückläufen gut – keine Änderung.
- Aufzug: `painted_metal_grey` ist fleckig rot verrostet (`docs/screens/props/items/r3/elevator.jpg`) → glatter Lack ohne Textur.

**r4:**
- Aufzug hellgrau lackiert – übernommen. troffer v2 jetzt dezent – übernommen.
- outlet: Griffschmutz auf der Tapete neu – wirkte heller als die Wand (die Wand ist dort im Feuchtigkeitsrand 18 % dunkler).
- Fundstücke: Einreißkerbe am Riegel, Klebenaht am Etikett, zweiter Knick der Notiz – aus 50 cm kaum sichtbar, bleiben.

**r5:**
- outlet: Griffschmutz dunkler – jetzt ein leichter Hof um den Rahmen.
- Sicherungskasten: Schaltplan in der Klarsichttasche innen an der Tür (Papier aus dem Notizatlas) – übernommen.

**final:** alle Klassen und Varianten aus dem Stand nach r5 (`final/`, Übersichten oben).

## Restmängel (ehrlich)

- Die Prismenscheibe hat keine echte Prismenstruktur, nur Leuchtdichtefelder; 650 Leuchten teilen sich drei Varianten.
- Flecken auf Wand und Decke (Griffschmutz, Staubfahne, Staubhof) nehmen die Textur der Vorzimmer-Zone und sind nur dort
  richtig (alle heutigen Stellen liegen dort); Feuchtigkeitsrand, Deckenruß und Parallaxe der Welt fehlen auf ihnen – aus sehr
  flachem Winkel kann der Rand sichtbar werden. Durch die Flecken wachsen die Grenzen von outlet v0/v2 um etwa 20 % (flach an der
  Wand; Ursprung und Halt unverändert, Halt-Test grün).
- Kleine Details (Kerbe, Klebenaht, zweiter Knick, Haarrisse im Bogen) sind aus 50 cm kaum zu sehen.
- Sicherungskasten, Automat, Notausgang und Aufzug kommen heute nur in alten Spielständen der zusammenhängenden Welt vor; geprüft
  im Prüfstand. Dabei gefunden (nicht behoben, Engine): Sicherungskästen aus Spawnpunkten fester Karten hängen 1,2 m zu hoch
  (`Sector::fuseboxes` setzt z = Boden + 1,2 m, das Modell trägt seine Höhe selbst); keine heutige Karte setzt solche Punkte.
- Aufzug im Prüfstand: Ursprung in der Kachelmitte statt an der Kernwand, der Rahmen steht dort frei.
- Metallteile mit Metalltextur wirken im schwachen Licht der Ebenen fast schwarz (darum der Aufzug lackiert).
- Symbole im Inventar unverändert (Oberflächenatlas).

## Prüfungen

- `validate_models.py`: alle 29 eigenen Modelle (11 Fundstücke, 18 Level-0-Modelle mit Varianten) 0 Fehler. (Gesamtlauf: 94 Modelle
  anderer Teilaufträge mit Fehlern, keines von hier.)
- `gameplay_test`: 647 Prüfungen, 1 Fehler (Ouroboros-Träger, bekannt, anderer Auftrag); Halt der Ausstattung: level0 1057 Objekte,
  0 ohne Halt; alle Ebenen 10 506, 0 ohne Halt.
- `room_metrics.py --check`: alle Ebenen OK (Level 0: Decke min 2,70 m, Sicht Median 25,5 m).
- `audio_test` 106/0; `coop_test` 209 Prüfungen, 3 Fehler: 2 in testLateJoin (bekannt, anderer Auftrag) und eine Ortsabweichung
  von 7,7 m im Lauf mit simuliertem schlechtem Netz (250 ms, 10 % Verlust) – Netzcode, unabhängig von dieser Arbeit (nur
  Darstellung der Fundstücke geändert).

## Aufwand

- 27.10. 10:40–11:30: Bestand, Kontaktbögen vorher, erste Builder Level 0 (danach Sitzungsgrenze).
- 01.11. 13:00–20:00: Zusammenführung, Lade-Code, Fundstücke in Blender, Prüfstand `itemsheet.py`, Varianten und Karte, Tapete,
  Runden r1–r5 und final für Level 0 und Fundstücke, Prüfungen, Dokument.
- Runden: 5 Kritikrunden + Schlussrunde (Level 0 und Fundstücke), Tapete 2 Runden; 7 Klassen Level 0 mit 16 Varianten
  (+ arch2/arch4), 11 Fundstück-Modelle.
