# Ausstattung Level 1 – Sektoren Ouroboros, Garden, Fabled (V13.3)

Neue Objektklassen für die drei Sektoren, die das Wiki (Backrooms-Wiki, Level 1, Abschnitt *Sectors*) beschreibt und die
`tools/levels/level1.py` seit V13.3 als eigene Hallen baut:

- **Ouroboros** – kleinster Sektor, ewige Baustelle; schattenhafte Arbeiter reparieren seit Jahrzehnten ohne Fortschritt,
  ihre Maschinen verletzen manchmal Menschen. → Gerüst, halb gemauerte Wand, Baumaterial, Mischer, Strahler, Absperrung.
- **Garden** – üppig überwuchert; nach etwa einer Stunde beginnt die Verwandlung in eine Pflanze. → Ranken, Efeu, Moos,
  Farne, Wurzeln und junge Bäume, die den Betonboden aufbrechen.
- **Fabled** – einst menschliche Hochburg mit Holz- und Zinnoberrot-Ausstattung, heute voller Neonleuchten und farbiger
  Kabel. → Möbel und Vertäfelung in gealtertem Zinnoberlack mit Abplatzern, Neonröhren und -formen, Kabelbündel.

Builder: `tools/blender/models/models_l1_sectors.py` (alle Klassen, eigene Hilfen: `Kit` sammelt Geometrie je Material,
`sweep`/`lathe`/`leaf`/`leaf_fan`/`blob`/`emit`). Bau: `blender -b --factory-startup --python tools/blender/build_models.py --
<name>` (ohne `@`: alle Varianten). Prüfung: `python tools/qa/validate_models.py <name …>` – alle 40 Modelle fehlerfrei.

**Konventionen:** Meter, z oben, Blick +x. Bodenmodelle Ursprung Mitte Fußpunkt. Wandmodelle Rückseite bei x = 0;
`formwork`, `ivy_wall`, `panel_vermilion` mit Ursprung am Boden, `neon_tube`/`neon_sign` mit Ursprung in der Mitte der
Leuchtform (Karte setzt z = 2,4 bzw. 2,0 m wie bei Schildern). Deckenmodelle Oberseite z = 0, hängen nach unten (max.
1,2 m Kabel in Fabled-Kammern mit 3,4 m Decke; Ranken bis 1,5 m bei 6 m Garden-Decke).
**Gerüst:** `level1.py` setzt das Feld mit yaw = Wandnormale + 90°; deshalb liegt die Feldlänge (2,5 m) entlang x,
die Wandseite bei +y, Geländer und Diagonale zur Raumseite (−y).
**Blätter** sind echte, zweiseitige Geometrie (Ausstattung wird einseitig gezeichnet, Alpha-Ausschnitte gibt es nur im
Schilderatlas). **Neon:** Leuchtglas über `look=glow glow=1,3…1,5`, Elektrodenenden dunkles Glas, Halter, Trafo, Kabel
nicht leuchtend. Licht in den Raum gibt die Karte (Kartenleuchten an Strahlern und Röhren).

## Werkzeug: Kontaktbögen für Klassen ohne Kartenvorkommen

`tools/qa/propsheet.py` hat neue Optionen (im Kopf der Datei beschrieben):

- `--extra name[@v]:floor|wall|ceiling[:z],…` nimmt Modelle aus `data/models` auf, die (noch) nicht in der Karte stehen;
  Grenzen aus der `.glb` (Knoten-Transformationen angewendet, glTF → Spielachsen), Befestigung wie angegeben, `z` = Höhe
  des Ursprungs bei Wandmodellen.
- `--extra-like <klasse>` wählt Zone, Raumtyp und Höhe einer Klasse der Karte als Vorlage (sonst die erste Klasse des
  Bogens).
- `--tag <t>` hängt `_<t>` an die Übersicht (`docs/screens/level1/props_uebersicht_<runde>_<t>.jpg`), damit parallele
  Arbeiten an Level 1 sich die Übersicht nicht überschreiben.

Die Sektoren stehen inzwischen in der Karte; 26 der 27 Klassen kommen daher über `--only` aus dem propdump (echte Zone und
Höhe: `ouroboros` 5,0 m, `garden` 6,0 m, `fabled` 4,4 m / Kammern 3,4 m). Nur `wall_unfinished` steht nicht in der Karte
(`level1.py` baut die halben Wände als Blöcke) und lief über `--extra wall_unfinished:floor,wall_unfinished@1:floor
--extra-like scaffold`. Aufruf je Runde:

    python tools/qa/propsheet.py level1 --round <r> --tag sektoren --only scaffold,cement_bags,…,panel_vermilion
        --extra wall_unfinished:floor,wall_unfinished@1:floor --extra-like scaffold
    python tools/qa/reviewsheet.py level1 <r> --only … --views 2m_0,50cm_0,50cm_lampe_1 --cols 6

## Objektklassen (27 Klassen, 40 Modelle)

Maße L×B×H in Spielachsen (x × y × z, Meter). Dreiecke „vorher“: die Klassen sind neu – es gab kein Modell und keinen
Bogen; als Vergleich steht die Erstfassung (Runde 1) daneben. Materialien = Zahl der Materialien im Modell.

| Sektor | Klasse | Var. | Befestigung | Maße (je Variante) | Dreiecke vorher / Erstfassung | Dreiecke final | Mat. | Bogen vorher | Bogen final |
|---|---|---|---|---|---|---|---|---|---|
| Ouroboros | `scaffold` | 3 | Boden | 2.74×0.89×3.63 / 2.74×0.98×3.62 / 2.74×1.02×3.61 | – (neu) / 5510 / 6467 / 4109 | 5510 / 6467 / 4109 | 12 / 14 / 7 | – (neu) | [final](docs/screens/props/level1/final/scaffold.jpg), [v1](docs/screens/props/level1/final/scaffold_v1.jpg), [v2](docs/screens/props/level1/final/scaffold_v2.jpg) |
| Ouroboros | `wall_unfinished` | 2 | Boden | 0.55×3.02×1.64 / 0.76×3.02×1.33 | – (neu) / 7889 / 3299 | 7889 / 3299 | 7 / 7 | – (neu) | [final](docs/screens/props/level1/final/wall_unfinished.jpg), [v1](docs/screens/props/level1/final/wall_unfinished_v1.jpg) |
| Ouroboros | `cement_bags` | 1 | Boden | 1.86×1.07×0.73 | – (neu) / 3157 | 3157 | 8 | – (neu) | [final](docs/screens/props/level1/final/cement_bags.jpg) |
| Ouroboros | `rebar_bundle` | 1 | Boden | 3.02×1.13×0.44 | – (neu) / 4308 | 4308 | 6 | – (neu) | [final](docs/screens/props/level1/final/rebar_bundle.jpg) |
| Ouroboros | `formwork` | 1 | Wand (Boden) | 0.78×3.17×2.58 | – (neu) / 2803 | 2803 | 12 | – (neu) | [final](docs/screens/props/level1/final/formwork.jpg) |
| Ouroboros | `work_light` | 1 | Boden | 1.20×0.99×2.10 | – (neu) / 2820 | 2820 | 11 | – (neu) | [final](docs/screens/props/level1/final/work_light.jpg) |
| Ouroboros | `cement_mixer` | 1 | Boden | 1.22×0.94×1.33 | – (neu) / 4709 | 4709 | 12 | – (neu) | [final](docs/screens/props/level1/final/cement_mixer.jpg) |
| Ouroboros | `wheelbarrow` | 1 | Boden | 1.86×0.67×0.64 | – (neu) / 2299 | 2299 | 6 | – (neu) | [final](docs/screens/props/level1/final/wheelbarrow.jpg) |
| Ouroboros | `sawhorse` | 1 | Boden | 1.66×1.31×0.97 | – (neu) / 663 | 871 | 10 | – (neu) | [final](docs/screens/props/level1/final/sawhorse.jpg) |
| Ouroboros | `site_barrier` | 1 | Boden | 0.62×2.16×1.32 | – (neu) / 1740 | 1740 | 7 | – (neu) | [final](docs/screens/props/level1/final/site_barrier.jpg) |
| Ouroboros | `tarp_pile` | 1 | Boden | 1.98×1.58×0.37 | – (neu) / 3620 | 3620 | 4 | – (neu) | [final](docs/screens/props/level1/final/tarp_pile.jpg) |
| Garden | `vines_ceiling` | 3 | Decke | 1.31×0.89×1.35 / 1.12×0.71×1.52 / 1.72×0.66×1.38 | – (neu) / 5314 / 3360 / 2922 | 3784 / 2616 / 2566 | 6 / 7 / 8 | – (neu) | [final](docs/screens/props/level1/final/vines_ceiling.jpg), [v1](docs/screens/props/level1/final/vines_ceiling_v1.jpg), [v2](docs/screens/props/level1/final/vines_ceiling_v2.jpg) |
| Garden | `ivy_wall` | 2 | Wand (Boden) | 0.33×1.34×2.54 / 0.41×1.94×1.68 | – (neu) / 3140 / 1858 | 2134 / 1788 | 7 / 7 | – (neu) | [final](docs/screens/props/level1/final/ivy_wall.jpg), [v1](docs/screens/props/level1/final/ivy_wall_v1.jpg) |
| Garden | `moss_patch` | 3 | Boden | 1.43×1.02×0.06 / 2.09×0.43×0.03 / 1.05×0.73×0.06 | – (neu) / 860 / 910 / 836 | 778 / 910 / 836 | 6 / 4 / 5 | – (neu) | [final](docs/screens/props/level1/final/moss_patch.jpg), [v1](docs/screens/props/level1/final/moss_patch_v1.jpg), [v2](docs/screens/props/level1/final/moss_patch_v2.jpg) |
| Garden | `fern_cluster` | 1 | Boden | 1.45×1.21×0.64 | – (neu) / 3393 | 2393 | 8 | – (neu) | [final](docs/screens/props/level1/final/fern_cluster.jpg) |
| Garden | `roots_floor` | 1 | Boden | 2.45×1.64×0.31 | – (neu) / 3576 | 2206 | 5 | – (neu) | [final](docs/screens/props/level1/final/roots_floor.jpg) |
| Garden | `shrub_concrete` | 1 | Boden | 2.22×2.26×1.36 | – (neu) / 6393 | 4915 | 10 | – (neu) | [final](docs/screens/props/level1/final/shrub_concrete.jpg) |
| Garden | `sapling` | 1 | Boden | 1.72×1.72×2.94 | – (neu) / 8112 | 6102 | 10 | – (neu) | [final](docs/screens/props/level1/final/sapling.jpg) |
| Garden | `overgrown_crate` | 1 | Boden | 1.59×1.01×0.76 | – (neu) / 4981 | 4981 | 13 | – (neu) | [final](docs/screens/props/level1/final/overgrown_crate.jpg) |
| Fabled | `neon_tube` | 4 | Wand (Mitte, z 2,4) | 0.07×1.31×0.86 / 0.07×1.31×0.86 / 0.07×1.31×0.86 / 0.07×1.31×0.86 | – (neu) / 1160 / 1160 / 1160 / 1160 | 1160 / 1160 / 1160 / 1160 | 7 / 7 / 7 / 7 | – (neu) | [final](docs/screens/props/level1/final/neon_tube.jpg), [v1](docs/screens/props/level1/final/neon_tube_v1.jpg), [v2](docs/screens/props/level1/final/neon_tube_v2.jpg), [v3](docs/screens/props/level1/final/neon_tube_v3.jpg) |
| Fabled | `neon_sign` | 2 | Wand (Mitte, z 2,0) | 0.08×1.11×0.76 / 0.09×0.90×0.71 | – (neu) / 2636 / 2284 | 2636 / 2284 | 8 / 7 | – (neu) | [final](docs/screens/props/level1/final/neon_sign.jpg), [v1](docs/screens/props/level1/final/neon_sign_v1.jpg) |
| Fabled | `cable_hang` | 2 | Decke | 0.16×2.04×1.01 / 0.22×0.62×1.20 | – (neu) / 1708 / 2166 | 1468 / 2166 | 12 / 14 | – (neu) | [final](docs/screens/props/level1/final/cable_hang.jpg), [v1](docs/screens/props/level1/final/cable_hang_v1.jpg) |
| Fabled | `cabinet_vermilion` | 1 | Boden | 0.58×1.06×1.86 | – (neu) / 1849 | 1899 | 11 | – (neu) | [final](docs/screens/props/level1/final/cabinet_vermilion.jpg) |
| Fabled | `table_vermilion` | 1 | Boden | 1.40×0.80×0.78 | – (neu) / 841 | 841 | 7 | – (neu) | [final](docs/screens/props/level1/final/table_vermilion.jpg) |
| Fabled | `bench_vermilion` | 1 | Boden | 0.38×1.60×0.47 | – (neu) / 379 | 491 | 7 | – (neu) | [final](docs/screens/props/level1/final/bench_vermilion.jpg) |
| Fabled | `screen_vermilion` | 1 | Boden | 0.28×1.78×1.80 | – (neu) / 2018 | 2049 | 8 | – (neu) | [final](docs/screens/props/level1/final/screen_vermilion.jpg) |
| Fabled | `panel_vermilion` | 1 | Wand (Boden) | 0.06×1.00×2.20 | – (neu) / 480 | 518 | 7 | – (neu) | [final](docs/screens/props/level1/final/panel_vermilion.jpg) |

Übersichten je Runde (2 m, erster Winkel): [r1](docs/screens/level1/props_uebersicht_r1_sektoren.jpg),
[r2](docs/screens/level1/props_uebersicht_r2_sektoren.jpg), [r3](docs/screens/level1/props_uebersicht_r3_sektoren.jpg),
[r4](docs/screens/level1/props_uebersicht_r4_sektoren.jpg), [r5](docs/screens/level1/props_uebersicht_r5_sektoren.jpg),
[r6](docs/screens/level1/props_uebersicht_r6_sektoren.jpg) (nur Garden), **[final](docs/screens/level1/props_uebersicht_final_sektoren.jpg)**.
Bögen der Erstfassung zum Vergleich: [cement_bags r1](docs/screens/props/level1/r1/cement_bags.jpg),
[ivy_wall r1](docs/screens/props/level1/r1/ivy_wall.jpg), [moss_patch r1](docs/screens/props/level1/r1/moss_patch.jpg),
[cabinet_vermilion r1](docs/screens/props/level1/r1/cabinet_vermilion.jpg).

## Aufbau und Materialien je Klasse

**Ouroboros**
- `scaffold`: Rahmengerüst 2,5 × 0,8 m, Beläge auf 1,35 und 2,60 m, Geländer bis 3,6 m; Rohr 48,3 mm (verzinkt, zwei Alterungen),
  Fußspindeln mit Gewinde, Flügelmutter und Fußplatte (Rost), Kupplungen mit Schraube/Mutter, Eckstreben, Diagonale.
  v0 vollständig: Stahlbeläge mit Sicken und Klauen, Durchstiegsboden (Sperrholz, Klappe offen) mit Alu-Leiter, Bordbretter,
  Mörtelkübel mit Kelle, Mörtelspritzer. v1: Holzbohlen mit Blechbeschlag, Gerüstplane mit Falten, Saum, Ösen, Riss und
  Schmutzfahnen, Ziegelstapel und Kantholz auf dem Belag. v2 halb abgebaut: ein Rahmen oben fehlt, Geländerrohr hängt an
  einer Kupplung, Rohre angelehnt, Belag am Boden, lose Kupplungen.
- `wall_unfinished`: 3 × 0,24 m, Verband mit halben Steinen, rechts abgetreppt; Bitumen-Sperrbahn, Lager- und Stoßfugen mit
  herausgequollenem Mörtel, offene Steine mit echtem Lochbild (v0 Hochlochziegel 4 × 6 Löcher, v1 Betonhohlblock mit zwei
  Kammern), Abplatzer, Mörtelkleckse am Boden, Mörtelbatzen mit Kelle.
- `cement_bags`: Europalette nach Norm (3 Bodenbretter, 9 Klötze, 3 Querbretter, 5 Deckbretter, Nagelköpfe); 25-kg-Säcke als
  pralle Kissen (Kraftpapier, Aufdruckband rot, Etikett grau), Verband abwechselnd längs/quer, ein Sack aufgerissen mit
  Zementhaufen, ein Sack heruntergefallen, Zementstaub.
- `rebar_bundle`: 28 Rippenstähle (sternförmiges Profil mit Verdrehung, 12–16 mm, 2,8 m, versetzte Enden, Farbmarkierung),
  Bindedraht an drei Stellen mit verdrillten Enden, Lieferschild, drei Kanthölzer, vier lose Bügel.
- `formwork` (Wandmodell): fünf gelbe Dreischichtplatten (rote Kante, Betonreste), zwei Rahmentafeln (Rahmen rot, Schalhaut,
  Querrippen, Ankerhülsen, Kranöse, Betonkruste, Lackschäden), zwei Deckenstützen (lackiertes Außenrohr, verzinkter Auszug,
  Stellmutter, Steckbolzen, Kopf-/Fußplatte), Kantholz, Keil.
- `work_light`: Dreibeinstativ (Vierkantbeine, Streben, Gummifüße), gelber Mast mit verzinktem Auszug, Klemmen mit
  Sternknauf, T-Traverse, zwei Strahler (Druckguss mit Kühlrippen, Glas leuchtend `glow=1,7`, Schutzgitter, Bügel mit
  Knauf), Kabel mit Klebeband am Mast, Schlaufe am Boden, Schukostecker.
- `cement_mixer`: Fahrgestell mit Holmen, Stützbeinen, Luftreifen; Joch und Lager; Trommel als Drehkörper (außen Orange,
  innen und Lippe Betonkruste, Mischschaufeln, Betonabläufe unter der Lippe), Zahnkranz mit 40 Zähnen, Motor mit Rippen,
  Schalter, Kipp-Handrad, Kabel, Beton am Boden.
- `wheelbarrow`: Mulde als gerundete, nach vorn geneigte Schale (verzinkt, innen Betonkruste), Rollrand, Holme mit
  Gummigriffen, Beine mit Fußbügel, Luftrad mit Profilstollen, rote Felge.
- `sawhorse`: Bock mit gespreizten Beinen, Sperrholz-Stirnbrettern, Querlatten, Nagelköpfen, verzinkten Lochblechen mit
  Schrauben; aufgelegtes Brett mit Sägeschnitt, Fuchsschwanz, Sägemehl, Abschnitte.
- `site_barrier`: Absperrgitter 2 × 1 m, Rohrrahmen rot-weiß in 25-cm-Abschnitten, Füllstäbe, Recycling-Gummifüße mit
  Steckhülse, Lackschäden (Rost), gelbe Warnleuchte (leuchtend).
- `tarp_pile`: Plane über verdeckten Formen (weich gezogen, Falten), Unterseite, eingerollter Umschlag mit Saum und Ösen,
  zwei Ziegel zum Beschweren.

**Garden** (gedämpftes Grün in fünf Tönen, Blätter zweiseitig; Moos mit der CC0-Textur `moss`)
- `vines_ceiling`: flach an der Decke kriechende Triebe (Zufallsweg) mit Blättern nach unten; hängende Ranken mit dünner
  werdendem Stengel, eingerollter Spitze, kurzen Stielen und herzförmigen Blättern. v0 dichter Vorhang, v1 wenige lange
  Ranken teils vertrocknet, v2 verholzte Liane als Bogen mit umwundenem Strang.
- `ivy_wall` (Wandmodell): kriechende Triebe mit Haftwurzeln, Verzweigungen, fünflappige Blätter (gewölbt, weich
  schattiert, an Stielen, seitlich versetzt), Erde am Fuß; v1 kleiner mit Ausläufern auf dem Boden.
- `moss_patch`: Polster als Polarnetz mit welligem Rand, weich auslaufender Höhe und Buckeln, trockener Randbereich, feuchter
  dunkler Hof (glänzender Beton), Sporenkapseln; v1 entlang eines Risses, v2 drei Polster.
- `fern_cluster`: Riss mit Erde, angehobene Bruchstücke (Estrich oben, Bruchkante rau), Krümel; acht gebogene Wedel mit
  Fiederpaaren, ein vertrockneter Wedel, zwei eingerollte junge Wedel.
- `roots_floor`: drei Hauptwurzeln (unrundes Profil, tauchen ab und auf), Seitenwurzeln, Erde an den Buckeln, Plattenstücke,
  die mit einer Kante auf der Wurzel liegen (Achse parallel zur Wurzel), Moos in Fugen, Bruchstücke.
- `shrub_concrete`: Riss mit aufgeworfenen Stücken, sieben Triebe mit Verzweigung, Blattbüschel an den Zweigenden, Laub am Boden.
- `sapling`: junge Birke 3,1 m aus einem aufgebrochenen Loch (Plattenkranz), Stamm unten dunkle Borke, oben weiß mit
  Lentizellen, 13 spiralig stehende Äste mit Zweigen und Blattbüscheln.
- `overgrown_crate`: Kiste aus Altholz (Algen unten, gebrochenes Brett mit Splittern, verschobenes Deckelbrett,
  Eckleisten), Moospolster auf dem Deckel, Moos an Kanten, Efeu an zwei Seiten bis über den Deckel, Grasbüschel, kleiner Farn.

**Fabled** (Zinnoberlack gealtert in drei Stufen, Holz an Abplatzern, Kreidegrund, Schmutz, Messing blank/angelaufen)
- `neon_tube`: Glasröhre 1,2 m (`look=glow`, `glow=1,5`) 5,5 cm vor der Wand, Enden in Bögen zur Wand mit dunklen
  Elektrodenstücken und Gummi-Durchführungen, drei Halter mit Klammer, Trafo mit Typenschild, Hochspannungskabel zu beiden
  Enden, Netzkabel mit Schellen nach oben. Varianten 0–3: Rot, Grün, Blau, Violett (wie die Kartenleuchten).
- `neon_sign`: v0 zwei Wellen (Pink, Cyan) und eine Raute (Gelb), v1 Ring (Orange) mit Zickzack (Violett); schwarzer
  Kabelkanal mit Trafo, Halter, Durchführungen, Kabel.
- `cable_hang`: v0 acht farbige Kabel in Bögen zwischen Abzweigdose und Deckenhaken, zwei hängen weiter mit Kupferlitze,
  Kabelbinder; v1 abgerissene Gitterrinne, Bündel fällt heraus, Schlaufen, abisolierte Enden, Isolierband, Klemme.
- `cabinet_vermilion`: Sockel mit Füßen, Korpus mit Kranz, Rahmenfront, zwei Schubladen und zwei Kassettentüren (eine
  einen Spalt offen, dunkles Inneres), Messingbänder, runde Beschläge mit Bügelgriff, Schlossrosette, abgegriffene Stellen,
  Abplatzer an Kanten, großflächig matte Stellen, Staub auf dem Deckel.
- `table_vermilion`: Platte mit Fase, abgewetzte Stellen, Glasränder, Kratzer, Zarge, verjüngte Beine mit Pferdehuf-Füßen,
  Eckbänder, schwarze Fußstege, Abplatzer.
- `bench_vermilion`: Sitzbrett mit abgewetzten Sitzstellen, gespreizte Beine, Zargen, Stege, Eckbänder, schwarze Fußstege.
- `screen_vermilion`: vier Flügel im Zickzack, Gitterwerk oben (ein Flügel mit fehlenden und einem abgebrochenen Stab),
  Kassettenfüllung unten, Füße, Messingscharniere, Abnutzung.
- `panel_vermilion` (1 m, Wandmodell): Rahmen mit halben Außenstielen (stoßen an das Nachbarmodul) und Mittelstiel, sechs
  erhabene Füllungen (Sockelfeld, hohes Feld, Fries), Sockelleiste dunkel, profilierte Abdeckleiste, Abplatzer, matte Stellen,
  Kratzer.

## Dreiecke in der Ebene (propdump nach dem Schlusslauf)

| | Dreiecke |
|---|---|
| Level 1 gesamt (3 917 Objekte) | 3 677 962 |
| davon Sektorenklassen | 1 749 345 |
| – Ouroboros | 176 780 |
| – Garden | 1 096 325 |
| – Fabled | 476 240 |

Nach dem ersten Schlusslauf waren es 3,84 Mio. (Garden 1,26 Mio.); Runde 6 hat die Pflanzen um 160 000 Dreiecke entlastet.
Häufigste Klassen: `panel_vermilion` 217 ×, `fern_cluster` 82 ×, `cable_hang` 78 ×, `ivy_wall` 92 ×, `moss_patch` 114 ×.

## Runden

Runde **vorher** entfällt: die 27 Klassen gab es nicht (kein Modell, keine Code-Fassung) – die Karte verwies auf leere
Namen. Vor den Spielrunden lief jede Klasse durch schnelle Workbench-Vorschauen aus Blender (Geometrie, Form, Lage);
dabei fielen u. a. auf und wurden vor Runde 1 behoben: Mauerschnur ohne Pfosten (entfernt), Zementsäcke linsenförmig,
Mischer-Abläufe als eckige Streifen (jetzt Klecksketten), Sägebrett durch den Balken, Farn-Fiedern mit Größe 0 (entartete
Dreiecke), Efeu ragte in die Wand (x < 0), Tischbeine schwebten 3 cm, Vertäfelung wirkte wie eine Doppeltür (jetzt drei
Felderreihen), Neonform v1 mit störender schwarzer Verbindungsröhre (entfernt), Gerüst in Kartenlage gedreht.
Jede Runde: alle 40 Modelle bauen, `validate_models.py`, Kontaktbögen im Studio (Zone/Licht des Sektors), kompakte
Prüfbögen (2 m, 50 cm, 50 cm mit streifender Lampe) ansehen; Vergleich einzelner Aufnahmen zwischen Runden.

### Runde 1 (Erstfassung im Spiel)
| Klasse | Befund | Änderung |
|---|---|---|
| Holz allgemein (Palette, Bohlen, Bock, Kanthölzer, Kiste) | Birkentextur färbt alles leuchtend orange | ohne Textur (Holz-Look), Fichte/Altholz gedämpft |
| `cement_bags` | Säcke fast weiß, Aufdruck wie Zuckerstange | Kraftpapier, Aufdruck gedämpft |
| `cement_mixer` | Orange grell, glänzend | 0,72/0,30/0,06, r 0,95 |
| `formwork` | gut lesbar; Rahmenrot zu satt | gedämpft |
| `scaffold` v1 | Plane flach und weiß | schmutzig-weiß, Falten tiefer |
| `tarp_pile` | Blau zu satt | gedämpft |
| `wall_unfinished` v0 | Ziegel grell | dunkler Terrakotta |
| `wheelbarrow` | Mulde wirkt weiß | älteres Verzinkt |
| `ivy_wall` | einzelne Stränge statt Teppich, weiße Glanzkanten, Gelb grell | Blätter seitlich versetzt, Triebe breiter, weniger Glanz/Gelb |
| `moss_patch` | Rasen-Grün, harte dunkle Hofkontur | Tönung gedämpft, Hof schmaler und heller, 6 statt 7 Ringe |
| `fern_cluster` | gut; 3 393 Dreiecke × 82 in der Karte | 7 Wedel, 9 Fiederpaare, Krümel ohne Fase |
| `roots_floor` | Wurzeln wie Stöcke | 10 statt 7,5 cm |
| `shrub_concrete` | kahl | mehr und größere Blätter je Zweigende, Äste sparsamer |
| `vines_ceiling` | Blätter klein | 11 cm, ein Strang weniger |
| Fabled-Möbel, Vertäfelung | Zinnober lachsrosa-plastikhaft, Abplatzer aus 2 m unsichtbar | Lack tiefer, großflächige matte Stellen und Schmutz |
| `neon_tube`, `neon_sign`, `cable_hang`, `work_light`, `site_barrier`, `sapling` | gut | Trafo dunkler |

### Runde 2
| Klasse | Befund | Änderung |
|---|---|---|
| Holz | jetzt Fichte/Altholz – klar besser | übernommen |
| `cement_bags` | nur wenig dunkler (Belichtung gleicht aus, Mittel 146 → 126) | noch weiter zu Kraftpapier |
| `ivy_wall` | breiter, dichter; Blätter facettig mit Glanzkanten | gewölbte Spreite weich schattiert (verschweißt, 55°) |
| `moss_patch` | immer noch grell (Textur selbst gesättigt) | Tönung nach Rot/Braun verschoben (entsättigt) |
| `fern_cluster` | 7 Wedel wirken dünn | 8 Wedel, Fiedern breiter |
| `shrub_concrete` | dicht belaubt; 6 971 Dreiecke | etwas weniger Blätter je Zweig |
| Fabled | dunkler, aber unter warmem Licht noch leuchtend | Zinnober 0,40/0,10/0,06 |

### Runde 3
| Klasse | Befund | Änderung |
|---|---|---|
| `cement_bags` | Kraftpapier – gut | – |
| `moss_patch` | olivgrün, wirkt feucht – klar besser | – |
| `ivy_wall` | weich, gewölbt – besser | – |
| Fabled | Lack wirkt lachsrosa: Glanzschleier im warmen Licht | stumpfer alter Lack (r 1,05), etwas brauner |
| `wall_unfinished` v0 | Ziegel wie roter Granit (Textur `concrete_rough`) | ruhigere Textur `concrete_cast` |
| `rebar_bundle` | Stahl unter der Lampe schwarz | heller Flugrost |

### Runde 4
| Klasse | Befund | Änderung |
|---|---|---|
| Fabled | tief rot statt lachsrosa, Abnutzung sichtbar – deutlich besser | übernommen |
| `wall_unfinished` | Terrakotta ruhiger, als Ziegel lesbar | übernommen |
| `rebar_bundle` | unter streifender Lampe weiter dunkel (dünne Stäbe im Schatten) | so gelassen |
| `scaffold` v1, `sawhorse`, `bench_vermilion`, `cable_hang` | Plane zu sauber; Bock ohne Beschläge; Bank schlicht; Kabel 78 × in der Karte | Schmutzfahnen; verzinkte Lochbleche mit Schrauben; Eckbänder und Fußstege; 12 statt 15 Ringe |

### Runde 5
| Klasse | Befund | Änderung |
|---|---|---|
| `scaffold` v1 | Schmutzfahnen als harte dunkle Rechtecke (Rasterzellen 12 cm) | heller, nur 4 Bahnen |
| `sawhorse` | Beschläge gut (871 Dreiecke) | – |
| `bench_vermilion` | deutlich weniger schlicht (491 Dreiecke) | – |
| `cable_hang` | gleiches Bild, 1 468 statt 1 708 Dreiecke | – |

### Runde 6 (Dreiecke) und Schluss
Nach dem ersten Schlusslauf zeigte der propdump 3,84 Mio. Dreiecke für ganz Level 1 (Grenze etwa 4 Mio.), davon Garden
1,26 Mio. Runde 6 nur für die Pflanzen: Rankenblätter mit einer Station (8 statt 16 Dreiecke), Stengel vierseitig, Efeu
weiter auseinander mit größeren Blättern, Wurzelprofil 7 Punkte, Strauch und Birke weniger, dafür größere Blätter.
Bildvergleich r5/r6 gleichwertig (Anordnung durch neue Zufallsfolge etwas anders). Danach Birke auf knapp 3 m gekürzt
(Vorgabe 2,5–3 m) und Schlusslauf `final` für alle 40 Modelle.

## Tests
- `python tools/qa/validate_models.py <alle 27 Klassen>`: 40 Modelle geprüft, 0 mit Fehlern.
- `build/Release/tests/gameplay_test.exe data` (in `build/Release/bin`): **OK: 446 Prüfungen, 0 Fehler** (Halt der
  Ausstattung: level1 1 477 Objekte mit Halt-Regel, 0 ohne Halt – die neuen Klassen stehen noch nicht in der Halt-Liste
  des Tests, siehe Restmängel).

## Restmängel (ehrlich)
- **Dreiecke:** Garden trägt 1,10 Mio., die Ebene liegt bei 3,68 Mio. – Spielraum für andere Klassen von Level 1 ist klein.
  Werden andere Klassen schwerer, sollte `level1.py` die Dichte senken (Efeu 35 % der Wandkacheln, Farne je 30 m²) statt
  die Pflanzen weiter zu vereinfachen.
- **Blätter** sind einfarbige Flächen ohne Adern/Textur; Efeublätter haben alle denselben Umriss, Rankenblätter nur eine
  Querstation. Aus 50 cm erkennbar als einfache Polygone.
- **Moos** nutzt die CC0-Textur `moss` (in `textures.json` für Level 188 eingetragen, wird aber global geladen); die Textur
  ist sehr gesättigt, die Tönung dämpft sie nur teilweise.
- **Abplatzer** im Zinnoberlack sind flache Inseln 0,5 mm über der Fläche (keine echte Vertiefung, kein Craquelé); der Lack
  wirkt im warmen Licht der Fabled-Halle noch recht gesättigt.
- **Gerüstplane** (v1): Schmutzfahnen folgen dem 12-cm-Raster des Tuchs (kantig). **Bewehrungsstahl** wirkt unter der
  streifenden Lampe sehr dunkel.
- **Neon** leuchtet nur als Fläche; Licht in den Raum kommt aus den Kartenleuchten (level1.py setzt sie an Röhren und
  Strahlern, nicht an `neon_sign`).
- `wall_unfinished` steht nicht in der Karte (die halben Wände sind dort Blöcke); das Modell ist fertig und kann die Blöcke
  ersetzen oder ergänzen.
- Die neuen Klassen fehlen in der Halt-Liste von `gameplay_test` und in `mapkit.WALL_MOUNT`/`CEILING_MOUNT`/`FLOOR_MOUNT`
  (C++/Karten-Werkzeug nicht geändert, wie vorgegeben) – der Halt-Test prüft sie deshalb noch nicht.
- Einige Lampen-Aufnahmen der Bögen (Gerüst v1/v2, Deckenmodelle) sind fast schwarz, weil die streifende Lampe des Werkzeugs
  das Objekt kaum trifft; beurteilt wurde dort mit den übrigen Ansichten.

## Aufwand
Etwa 3,5 Stunden aktive Arbeit (ca. 09:45–11:25 und 15:50–17:20, dazwischen Unterbrechung durch die Nutzungsgrenze).
**7 Spielrunden** (r1–r6, final; mindestens 5 Kritikrunden erfüllt) plus Workbench-Vorschauen vor Runde 1; 27 Klassen,
40 Modelle neu gebaut.
