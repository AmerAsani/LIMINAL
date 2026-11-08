# README-Bilder V13 – Auswahl (08.11.2026)

16 Bilder, je 1920 × 1080, JPG q85, 139–290 KB, zusammen 3,25 MB. Aufgenommen mit dem Spiel auf dem Stand `f45bc4d`
(alle zehn Ebenen abgenommen), ohne Anzeige (`--no-hud`), feste Zeitschritte, eigener Benutzerordner je Lauf.

**Herkunft der Kandidaten** (rund 230 Bilder):
- je Ebene 12 Blickpunkte mit langer, gerahmter Sicht aus der aktuellen Karte (je Raum die Stelle mit dem Rücken nahe einer
  Wand, langem Mittelstrahl und freien Seitenstrahlen), Qualität Ultra;
- die gespeicherten Kamerastellen der Abnahmen (11–18 je Ebene), Qualität Ultra;
- Wesen-Läufe von `tools/qa/creatureshots.py` (Level 2, 3, 5; Qualität Hoch, Wesen in ihren Räumen, Ebenenlicht und Lampe);
- Koop mit zwei echten Spielprozessen (Host und Gast, direkt über 127.0.0.1, Schulteransicht `figcam`), Qualität Hoch.

Jedes Bild wurde einzeln in halber Größe angesehen. Geprüft: scharf, belichtet, keine Anzeige, keine leere Wand, kein Clipping.

| Datei | Inhalt | Warum gewählt |
|---|---|---|
| `v13_level0_lobby_saeulenraum.jpg` | Level 0: gelbe Räume mit Säule, Rasterleuchten, Durchblicke in die Tiefe | das Grundmotiv; Säule als Mitte, drei Sichtachsen, Teppich nah scharf |
| `v13_level1_parkhalle.jpg` | Level 1: Betonhalle mit Pfeilern, Farbbändern und Leuchtstoffröhren | weite, gleichförmige Halle – typisch für Level 1; kühles Licht |
| `v13_level2_fenster_am_gangende.jpg` | Level 2: enger Gang, Lampe in der Hand, am Ende das Fensterwesen als Umriss | Atmosphäre und Wesen in einem Bild; Hand und Finger natürlich um die Lampe |
| `v13_level2_clump_im_lampenlicht.jpg` | Level 2: Clump am Boden vor einem Schaltpult, nur im Lampenlicht | Schock: blasse Gliedmaßen im Dunkel, Licht nur aus der Hand |
| `v13_level3_hauptgang.jpg` | Level 3: Ziegelgang mit Rohren, Kabeltrasse, Pfützen, Nebel am Ende | starke Fluchtlinie, Materialien (Ziegel, Rohr, nasser Boden) |
| `v13_level3_taschenlampe_ziegelgang.jpg` | Level 3: dunkler Ziegelgang, einzige Lichtquelle die Lampe in der Hand | die Hand mit Taschenlampe (Daumen oben, Finger geschlossen), Lichtkegel im Dunst |
| `v13_level3_praetorianer_und_motten.jpg` | Level 3: Deathmoths und Praetorianer am Boden und in der Luft, im Lampenlicht vor dem verhängten Tisch | Schock: mannsgroße Insekten mit Rüssel und Facettenaugen in Armlänge, Hand mit Lampe im Bild |
| `v13_level5_hotelgang.jpg` | Level 5: endloser Hotelgang, Damasttapete, Wandleuchten, Sessel | warmes Licht und Tiefe – das Hotel auf einen Blick |
| `v13_level5_samantha.jpg` | Level 5: Samantha im Gang, Maul bis zur Brust aufgerissen | Schock: die harmlose Katze zeigt ihr Gebiss; scharf, mitten im Bild |
| `v13_level5_3_bibliothek.jpg` | Level 5.3: Saal mit Säulen, Regalen bis unter die Decke, Kerzenständern | Größe des Saals, Säulenreihe in die Tiefe, Kerzenlicht |
| `v13_level188_hof_der_fenster.jpg` | Level 188: Hof mit Fensterreihen auf drei Seiten, Glasdach, Rasen | die Fenster (einige mit Gestalten) und die Höhe des Hofs; keine leere Wand im Bild |
| `v13_level903_metro_steig.jpg` | Level 903: Bahnsteig, einfahrender Wagen, ruhender Zug, Leuchtbänder | Licht und Fluchtlinie, Zug von vorn |
| `v13_level37_wagen_im_wasser.jpg` | Level 37: Metro-Wagen im gefliesten, gefluteten Tunnel | das Unwirkliche der Ebene; Wasser mit Lichtnetz und Spiegelung |
| `v13_level37_lichtnetz_wasser.jpg` | Level 37: Lampe unter Wasser, Lichtnetz auf Boden, Wänden und Decke | das Wasser- und Spiegelungsbild (Kaustik, Brechung, Fresnel) |
| `v13_level36_rundgang_laufband.jpg` | Level 36: Rundgang mit Laufband, Verbundplatten, Terrazzo, Wegweiser | die in der Abnahme neu gebauten Flächen; klare Linien |
| `v13_koop_zwei_spieler_garten.jpg` | Level 1 (Garden): zwei Spieler im Koop, beide mit Lampe, Schatten am Boden | Koop-Bild aus der Sicht des Gasts (zwei echte Prozesse); dunkle, bewachsene Halle |

Abgedeckt: alle zehn Ebenen (Level 2, 3, 5 und 37 mit je zwei Bildern, Level 1 dazu im Koop-Bild), Hand mit Taschenlampe
(Level 3, auch im Level-2-Gang), drei Wesen mit Schockwirkung (Clump, Praetorianer mit Motten, Samantha; dazu das
Fensterwesen),
Wasser und Spiegelung (Level 37), Koop mit zwei Spielern.

**Verworfen** (Beispiele):
- Level 0: Blicke auf nahe Wände; ein Blick mit verwischt wirkendem Teppich im Vordergrund (streifender Winkel).
- Level 2: Tunnel mit Gleis (schwarzes Rechteck am Tunnelende wirkt wie ein Bildfehler, angeschnittene Maschine am Rand).
- Level 188: Hof mit halbseitig fensterloser Wand; zwei Eckblicke, bei denen die Kamera über dem Glasdach stand;
  Eckblick mit Wandkante im Bild.
- Großes Bündel nah (Level 3): erst gewählt, in voller Größe verworfen – harte Texturkante am langen Arm und die
  Eisenstange der Waffe läuft durch das Handgelenk (Clipping).
- Wesen mit rotem Schadensrand (Wretch, Faceling, Jerry, Photoshop); Smiler aus der Ferne zu klein; Bilder mit Kamera im
  Dunkeln ohne erkennbares Wesen.
- Koop: Bilder des Hosts nach dem Ende des Gasts (nur eine Figur), Blicke gegen Wände.
