# LIMINAL V13 – Qualitätsupdate der Ebenen und Lore-Abgleich

Stand 26.10.2026. Teil 4 des Auftrags: alle Ebenen aus festen Kameras, mit und ohne Lampe, mit ihren Wesen; Validierung; Abgleich mit der jeweiligen Wikidot-Seite.

## 1. Was geprüft wurde

| Prüfung | Werkzeug | Ergebnis |
|---|---|---|
| Feste Kameras je Ebene, Licht der Ebene und Lampe (Kameras mit `lamp`) | `qa_level.ps1` mit `<ebene>_cams.txt`; Bögen `docs/screens/texturen/<ebene>_{vorher,nachher}.jpg` | 10 Ebenen, 142 Ansichten (davon 8 mit Lampe), vor und nach V13 |
| Wesen in ihrer Ebene, im Licht und mit Lampe | `level_entities.ps1`; Bogen `docs/screens/level_wesen.jpg` | Abschnitt 3 |
| Karten neu erzeugt und geprüft (Wände, Türen, Erreichbarkeit, Spawn- und Sicherungspunkte, Halt der Objekte) | `python tools/levels/<ebene>.py` | alle 10 bestanden; Inhalt identisch mit dem Stand im Repository (nur die Versionszeile im Kopf weicht ab, zurückgesetzt) |
| Spiellogik | `gameplay_test` (Reise durch alle Übergänge, Halt aller Objekte, Vorräte je Ebene, Wesen), `worldgen_test`, `supply_balance_test`, `audio_test` | Abschnitt 4 |
| Wiki-Stand | Wikidot-Seite je Ebene, Revision am 26.10.2026 abgerufen | alle Revisionen gleich wie in den Levelspezifikationen von V12 (Tabelle unten) |

## 2. Lore-Abgleich je Ebene

Grundlage sind die Levelspezifikationen `LEVELSPEC_<ebene>.md` (V12, eigene Worte, CC BY-SA 3.0 beachtet). Am 26.10.2026 wurde jede Seite erneut abgerufen; keine hat sich seitdem geändert.

| Ebene | Wikidot-Revision (Spezifikation → heute) | Kern laut Seite (eigene Worte) | Im Spiel | Abweichung (Grund) |
|---|---|---|---|---|
| 0 – Threshold | 50 → 50 | Labyrinth gelber Räume, feuchter beiger Teppich, summende Leuchten, Säulenfelder, Bögen, Lochfelder, Dunkelzonen, rote Räume; keine bestätigten Wesen, nur dunkle Gestalten; Ausgang über eine flackernde Wand nach Level 1 | alles vorhanden (Bögen, Säulen, Löcher, Dunkelzone, roter Raum gesehen); Gestalt (neu, Abschnitt 3) | Raum verschiebt sich nicht, wenn man wegsieht (feste Karte, `WORLD_GENERATION.md`) |
| 1 – Habitable Zone | 83 → 83 | Betonhallen mit Pfeilern und Pfützen, enge Gänge mit Büros, Krankenzimmern, Gummizellen; Stromausfälle, bei denen gejagt wird (Clump genannt); Kisten mit Vorräten; Smiler, Hounds | Hallen, Gänge, Krankenzimmer, Gummizelle, Bilderraum, Lager mit Kisten; Stromausfall; Smiler, Hound, Clump | Duller und Skin-Stealer fehlen (nicht gebaut; nur Wesen mit eigener Seite und Briefing) |
| 2 – Pipe Dreams | 93 → 93 | enge Wartungstunnel mit 45°-Knicken, kreidiger Beton oder Ziegel, dichte Rohre, in Reihe geschaltete Leuchten, verschlossene Stahltüren; Smiler, Clumps, Hounds | Tunnel aus Beton und Ziegel, Rohre, Maschinen, Stahltüren, Gleis; Smiler, Hound, Clump | Wrangler und die Anhänger fehlen (s. o.) |
| 3 – Electrical Station | 18 → 18 | Ziegelwände, staubiger Beton, schwache Röhren, Stromtechnik, Gitter, Rohre mit schwarzem Schlamm; Heiligtümer mit Engelsstatuen; Generatorräume, Fertigungsstraßen | Ziegelgänge, Generatorraum, Fertigung, Sanktuarium mit Engel, Gitter, schwarze Lachen; Hound, Smiler, Clump | Deathmoths, Facelings, Wretches, Todesratten fehlen; Deathmoths gibt es nur im Hotel (Level 5 nennt sie „überall“) |
| 5 – Terror Hotel | 184 → 184 | Hotel im Stil der 1930er, mahagonirote und goldene Tapete, Hauptsaal, Ballsaal, Kesselraum; Aufzüge; Deathmoths überall mit Nestern | Damasttapete, Kronleuchter, Ballsaal, Kesselraum, Aufzug, Zimmer; Hound, Motten, Nest (neu) | die übrigen Wesen der Seite fehlen |
| 5.3 – Bibliothek | 40 → 40 | Wände aus Marmor mit Granit, Säulen aus Tuff, Eisengitter wie in Level 3, Kronleuchter, sieben Stockwerke mit Holztreppen, Gemälde über den Regalen; ein Geist der Bibliothek, der Erinnerungen nimmt | Marmor, Tuffsäulen, Gitter, Kronleuchter, Treppen, Wappen; Verstand sinkt (Erinnerungsverlust) | keine Wesen (die Seite nennt keine, die man treffen würde) |
| 188 – Hof der Fenster | 69 → 69 | Atrium aus weißem Beton mit Fensterreihen, fünfeckiger Weg zu einem Kreis aus Birke (kein Ausgang), weiße Gänge mit Mahagonitüren; Fensterwesen, Schatten | Atrium, Fensterreihen mit Gestalten in Fenstern, Weg, Birkenkreis, Tisch, Grammophon; Gestalt (neu) | Facelings und Skin-Stealer fehlen |
| 903 – Endstation | 60 → 60 | U-Bahnhof mit grauem Klinker, zwei Gleisen, langen Leuchten, Schildern in fremder Schrift; ein Wagen, der durch Wände fährt; friedliche Bewohner | Bahnsteig, Gleise, Wagen, Schilder in fremder Schrift, bewohnte Ecke | keine Facelings (friedlich, ohne Briefing) |
| 37 – Poolrooms | 52 → 52 | Räume und Gänge unter lauwarmem, blaugrünem Wasser, weiße Fliesen, Säulen, Gruben, Licht aus schrägen Winkeln; keine Wesen | alles vorhanden | – |
| 36 – Flughafen | 33 → 33 | Gepäckbänder, Check-in, Anzeigetafeln mit Countdown, Gänge, die in sich zurücklaufen, Läden, Laufbänder, Terminals A–Z mit Gates; Treffpunkt, Unterebene 36.1 | Gepäckausgabe, Check-in, Abflugtafeln, Rundgang, Ladenzeile, Laufbänder, Gates C30–C34, Ende am Gate | nur Terminal C (Umfang) |

## 3. Wesen in ihrer Ebene

Bild: `docs/screens/level_wesen.jpg` – jedes Wesen am Startpunkt seiner Ebene, einmal im Licht der Ebene, einmal mit der Lampe (Skript `level_entities.ps1`, Vorführung mit `figure`).

| Ebene | Wesen laut Konfiguration (`data/levels/<ebene>.json`, `presence`) | Im Bild |
|---|---|---|
| 0 | Gestalt (Wahrnehmung) | dunkler Umriss hinter einer Ecke, im Lampenlicht kaum heller |
| 1 | Smiler, Hound, Clump, Stromausfall | Smiler zwischen den Pfeilern (Augen und Zähne), Hound auf allen vieren, Clump aus Gliedern; mit Lampe feucht glänzende Haut |
| 2 | Smiler, Hound, Clump, Stromausfall | im Gleistunnel; der Clump füllt den Gang |
| 3 | Hound, Smiler, Clump, Stromausfall | im Ziegelgang; im schwachen Licht vor allem Umrisse |
| 5 | Hound, Deathmoths mit Nestern | Hound und Motte im Saal; das Nest frei vor den Aufzügen (im Spiel an Zimmertüren) |
| 188 | Gestalt | Umriss auf dem Rasen vor den Fensterreihen |
| 5.3, 903, 37, 36 | keine Wesen | – |

Alle Wesen kommen aus den neuen Blender-Modellen (Protokoll je Start: `Quelle Blender`); Einzelheiten und Kritikrunden in `KREATUREN_ANALYSE.md` und `KREATUR_<Name>.md`.

## 4. Tests

| Test | Ergebnis (26.10.2026, Stand nach den neuen Wesen) |
|---|---|
| `gameplay_test` (u. a. Reise durch alle Übergänge mit Gegenstücken, Halt aller Objekte auf allen 10 Karten, Vorräte je Ebene, Verhalten der Wesen, Hand und Lampe) | **OK: 357 Prüfungen, 0 Fehler** |
| `worldgen_test` (Weltgenerierung gegen die V4-Referenz) | **OK: 29 658 Prüfungen, 0 Fehler** |
| `supply_balance_test` | **OK (0 Fehler)** |
| `audio_test` | **OK: 106 Prüfungen, 0 Fehler** |
| Smoke-Test im Spiel, alle 10 Ebenen (`BUGLISTE.md`, Prüfungen) | alle Prüfungen bestanden, keine Warnung im Protokoll |
| Karten neu erzeugt | alle 10 bestanden, Inhalt identisch |
