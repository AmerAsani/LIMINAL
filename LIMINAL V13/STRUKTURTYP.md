# Strukturtyp der Ebenen (V13.3)

Stand 27.10.2026. Quelle ausschließlich **backrooms-wiki.wikidot.com** (Seiten am 27.10.2026 erneut gelesen: Level 0 Rev. 50,
Level 1 Rev. 83, Level 2 Rev. 93, Level 3 Rev. 18, Level 5 Rev. 184, Level 5.3 Rev. 40, Level 188 Rev. 69, Level 903 Rev. 60,
Level 37 Rev. 52, Level 36 Rev. 33). Alle Wiki-Inhalte in eigenen Worten (CC BY-SA 3.0, `LICENSES.md`).

Drei Typen:

- **Labyrinth**: eine Grundstruktur durchgehend, keine Wahrzeichen; Variation nur im Kleinen; Zusatzzonen nur, wenn das Wiki
  sie beschreibt. Prüfung: Ähnlichkeit der Layouts hoch, wenige Ausreißer (`tools/qa/similarity.py`).
- **Mischform**: eine wiederkehrende Grundstruktur mit Teilbereichen, die das Wiki ausdrücklich benennt (Sektoren, Raumtypen).
- **Eigener Ort**: ein unverwechselbarer Ort mit Wahrzeichen und Funktionszonen.

Mess- und Prüfwerkzeuge: `tools/qa/room_metrics.py --check` (Mindestwerte `tools/qa/metric_limits.json`),
`tools/qa/signs.py --check`, `tools/qa/similarity.py --check`. Vorher-/Nachher-Karten: `BIOM_AUDIT.md`.

| Ebene | Typ | Kernbeleg (Abschnitt der Wikidot-Seite) |
|---|---|---|
| 0 | Labyrinth | Description/Aberrations: Meile um Meile zufällig gereihter Räume, Grundriss verschiebt sich; keine Wahrzeichen |
| 1 | Mischform | The Halls: endlose Tiefgarage/Lagerhaus; Sectors: Aquila, Gild, Gothic, Ouroboros, Garden, Fabled; The Corridors mit Zweckräumen |
| 2 | Labyrinth | Description: unendliches Netz euklidischer Wartungstunnel, 45-Grad-Knicke, Flure in Vielfachen von 5 m; Räume hinter Türen |
| 3 | Mischform | Description: Ziegelgänge, großenteils durch Gitter zum Labyrinth versperrt; Rooms: Generator, Assembly Lines, Boiler, Sanctums |
| 5 | Eigener Ort | Description: unendliches Hotel mit Main Hall, Beverly Room (Wahrzeichen), Boiler Room |
| 5.3 | Eigener Ort | Generalities: benannte Bibliothek „Promethei Bibliotheca“, sieben Etagen; Gänge endlos |
| 188 | Eigener Ort | Description/Layout: ein Atrium (45 m, 30 m hoch) mit Mittelpfad und Birkenkreis, Gänge am Rand |
| 903 | Labyrinth (linear) | Description: endlos gleicher U-Bahnsteig, zwei Gleise, ruhender Wagen, verbarrikadierte Treppen |
| 37 | Labyrinth | Description: endloses Netz gleicher, gefluteter Fliesenräume; keine Wahrzeichen |
| 36 | Mischform | Sections 1–3: Gepäckausgabe, Check-in, Concourses (nicht-euklidisches Labyrinth), Läden, Gates; endlos wiederholte Teile |

## Level 0 – „The Lobby“ (Labyrinth)

**Wiki:** Hinterbüros/Lagerräume des Einzelhandels, Meile um Meile zufällig zerteilter Räume; der Grundriss verzieht sich,
wenn man wegsieht („Peripheral Shift“). Wiederkehrende Raumtypen, keine Orte: **Bögen** (stabilste Räume), **Säulenräume**
(Gitter-/Rasteranordnung, meilenweit), **Löcher** (schwarze Gruben im Raster), **Blackout Zones** (unbeleuchtet, raue Wände,
abgesenkte Böden mit Flüssigkeit), **Red Rooms** (geschlossene Schleifen), **Manila Room** (einziger benannter Raum, kaum zu
finden). Keine Schilder, keine Türen auf der Seite.

**Im Spiel:** Zonen vorzimmer (Grundstruktur, 65 %), saeulen, dunkel, loecher, rot, boegen, manila, naht (Ausgang „flackernde
Wand“ nach Level 1). Alle Zonen sind Wiki-Raumtypen. **Entfernen:** die Benennung „Der lange Flur“ und „Vergilbtes Zimmer“
als hervorgehobene Orte – sie wirken als Wahrzeichen; die Räume bleiben Teil der Grundstruktur (Zickzack-Flur nach LOOK_0).
Schilder: **0** (Wiki nennt keine).

**Maße:** Decke 2,7–3,1 m (Bürobau mit Rasterdecke 0,6 m; heute min 2,61 m → mindestens 2,7 m), Flure 2,0–2,6 m
(Bürofluren 1,5–2,4 m; großzügig), Raumflächen gemischt (Median ≥ 28 m²), Sichtachsen Median ≥ 24 m (heute 22 m) durch
versetzte Durchbrüche, mindestens zwei Hallen ≥ 150 m² (Säulenfeld, Bogenhalle).

## Level 1 – „Habitable Zone“ (Mischform)

**Wiki:** Hallen – weit, offen, Wände über Meilen fade und verfärbt, gelegentliche Betonsäulen; Tiefgarage und Lagerhaus
zugleich; nicht-euklidisch, Orientierung über selbst gewählte Merkpunkte (Tisch, farbige Säule, Schaltkasten, Kiste).
Sektoren: **Aquila** (grau, riesige Betonsäulen, Parkhaus, lecke Rohre, Pfützen), **Gild** (bunter, mehr Leuchten, die meisten
Kisten), **Gothic** (geschwungen, Bögen, runde Säulen), **Ouroboros** (kleinster Sektor, ewige Baustelle mit schattenhaften
Arbeitern), **Garden** (üppig überwuchert), **Fabled** (alte Holz-/Zinnoberrot-Ausstattung, heute Neon und farbige Kabel).
Korridore: eng, schwach beleuchtet, über Kilometer; Drucktüren aus den Hallen, Exit-Schilder darüber; Räume: kleine Büros mit
altem Rechner und Glühbirne, Ziegelräume, Krankenzimmer, Gummizellen, Räume mit Bildern. Flackern: Hallenlicht fällt aus.

**Im Spiel:** aquila, aquila_dunkel (Teil von Aquila, eigener Stromkreis fürs Flackern), gothic, gild (vier Farben), gaenge,
zimmer/buero/kranken/bilder/ziegel/polster (Wiki-Räume), schleuse (Drucktür-Vorraum, Wiki: Drucktüren), rohr/rohrgang
(Wiki: glatter Übergang nach Level 2, Rohre und Anzeigen tauchen allmählich auf). **Ergänzen (Wiki belegt, fehlt):**
Sektoren **Ouroboros**, **Garden** und **Fabled** als eigene Hallenabschnitte. Schilder: Exit-Schilder nur über den Drucktüren
Halle → Korridor (Grenze 12) und das grüne Exit-Licht.

**Maße:** Hallen 4–6 m hoch (Tiefgarage 2,5–3 m, Lagerhalle 6–10 m; Mittelwert), Raster der Säulen 8–13 m, Korridore 2,0 m
breit (Wiki: eng), Räume Schlafzimmergröße (Wiki) mit Decke ≥ 2,7 m (heute min 2,5 m im Korridor), Sichtachsen Median ≥ 32 m,
90-%-Wert ≥ 85 m (Hallen), mindestens 8 große Räume.

## Level 2 – „Pipe Dreams“ (Labyrinth)

**Wiki:** unendliches, verzweigtes Netz euklidischer Wartungstunnel unterschiedlicher Größe; Knicke streng 45 Grad (auch 45-
und 90-Grad-Kurven), Flure in Vielfachen von 5 m; eng, manchmal kaum für zwei Menschen nebeneinander, durch Rohre und
Maschinen verengt; schmutzig, ungleichmäßig beleuchtet. Hinter Türen: Industriekomplexe/Packhallen (atriumgroß), bürohafte
Flure, Einzelräume, schwarze Leeren. Feste Stellen: alte Straßenbahngleise, heiße Bereiche (Übergang Level 127), Bereiche im
Dauer-„Brownout“, verlassene Macchina-Bereiche mit Werkzeug und Teilen. Schilder: Warnschilder an Türen, aufgesprühte
Symbole, Wegweiser zu EL3A (Menschen-Basis, nicht dargestellt); keine Exit-Schilder.

**Im Spiel:** tunnel (beton, ziegel), kammer, maschine, anlage, lager, buero, buero_flur, leere, gleis, heiss. Alles
Wiki-Typen, aber **falsche Gewichtung**: Hallen hinter Türen (anlage 23 %, lager 23 %) überwiegen die Tunnel (16 %) –
das Labyrinth der Tunnel muss die Grundstruktur sein. **Ändern:** Tunnelnetz auf mindestens 50 % der Fläche, Hallen hinter
Türen kleiner und seltener; **ergänzen:** Brownout-Bereich und ein Macchina-Bereich (Wiki). Ähnlichkeit heute S = 0,78,
Ausreißer 36 % → Ziel S ≥ 0,85, Ausreißer ≤ 20 %. Schilder: **0** Exit-Schilder (nur Warnschilder an Türen).

**Maße:** Tunnel 2,0–3,0 m breit (Wiki: eng; Spielbarkeit mit Rohren an der Wand: mindestens 2,0 m), Decke ≥ 2,6 m,
Flurlängen in 5-m-Schritten (Netz im 15-m-Raster). Sichtachsen: Median ≥ 15 m, 90-%-Wert ≥ 38 m – das Wiki beschreibt die
Tunnel als eng und verwinkelt mit 45-Grad-Knicken, lange Fluchten gibt es nur entlang gerader Abschnitte; vorher kamen die
langen Blicke aus den übergroßen Hallen (16,8 m), nachher aus den Tunneln (16,0 m). Ähnlichkeit: S ≥ 0,80, Ausreißer ≤ 20 %
(die Räume hinter Türen sind Wiki-Raumtypen). Verstellte Räume (< 0,9 m frei): vorher 68 von 220, nachher 0 von 181.

## Level 3 – „Electrical Station“ (Mischform)

**Wiki:** Gänge aus verschieden gefärbten Ziegeln, staubiger Beton, schwache Röhren, Kabel, Elektrokästen, Rohre; Maße
schwanken stark (sehr eng bis food-court-groß); der Großteil durch unzerstörbare Gitter zum Labyrinth versperrt. Dazwischen
verstreut: Generatorräume, Fließbänder (häufig), Kesselräume, Sanktuarien (griechisch-römisch, Engelsstatue, innen dunkel).
Feste Orte sind Menschen-Basen (nicht dargestellt). Schilder: M.E.G.-Warnschilder an Angst-Stellen; keine Exit-Schilder.

**Im Spiel:** gang/hinter_gitter (Ziegel in sechs Farben), generatorraum, fertigung, kesselraum, sanktuarium, paranoia
(Wiki: Stimmen in Paranoia-Gängen), abgang/aufzugsvorraum (Ausgänge: Aufzüge nach Level 4/5), buero. **Falsche Gewichtung:**
Sonderräume 74 % der Fläche (Fertigung 31 %, Sanktum 21 %, Generator 19 %), Gänge nur 20 %. **Ändern:** Ziegelgänge mit
Gittern als Grundstruktur (≥ 50 % der begehbaren Fläche), Sonderräume kleiner und verstreut; das **Büro entfernen** (keine
Wiki-Grundlage außer Menschen-Basen). Schilder: **0** Exit-Schilder; Warnschilder an dunklen Engstellen erlaubt.

**Maße:** enge Gänge 1,5 m (Wiki: bis 14 Zoll – im Spiel nicht begehbar, daher 1,5 m als enge Stelle), Gänge sonst 2–3 m,
Decke ≥ 2,6 m (Paranoia-Gang vorher 1 m breit und 2,3 m hoch, nachher 2 m und 2,6 m), große Hallen ≥ 5.
**Umgesetzt (V13.3):** drei weitere Ziegel-Gangnetze (Ost, unter dem Sanktuarium, Süd) mit kleinen Kammern; Fertigungsstraße
60 × 32 m (vorher 60 × 40), Sanktuarium 28 × 40 m (vorher 32 × 52), Büro → leere Ziegelkammer. Gang- und Kammerräume 60 von 71 → 191 von 207
Räumen; nach Fläche bleiben die Hallen groß (Wiki: so groß wie ein Food-Court), Ziegelzonen 25 % → 40 %. Dadurch sinken die
Kennwerte, die vorher von den Hallen kamen: Sichtachsen Median 32,5 → 23,5 m (Grenze 22 m, Gänge mit Knicken und Gittern),
Median der Raumfläche 82 → 25 m² (Grenze 20 m², Kammern im Labyrinth).

## Level 5 – „Terror Hotel“ (Eigener Ort)

**Wiki:** unendliches Hotel der 1930er, Möbel der 1920er; Main Hall (Flure mit Zimmern, Möbeln, Aufzügen, goldene
Zimmerschilder ohne Muster), Beverly Room (zentraler Knoten mit vielen Türen, Tisch unter großem Kronleuchter, Mahjong,
Silberschilder an den Eingängen), Boiler Room (heiß, vollgestellt, Maschinen, Rohre, Kessel, Öfen). Ausgänge u. a. Treppe
nach Level 4, Aufzug nach Level 3.

**Im Spiel:** flur, zimmer, ballsaal (Beverly Room), kesselraum/kesselgang, aufzugshalle, metro (Übergang zur Metro, Level 903),
flur_alt (Zugang zu Level 5.3). Alles belegt. Schilder: Zimmerschilder und das Silberschild; **0** Exit-Schilder.

**Maße:** Hotelflure der 1930er 2,4–3,0 m breit, Decken ≥ 3,0 m (Grand Hotel 3,2–3,6 m), Ballsaal 7 m hoch, Sichtachsen
Median ≥ 20 m (lange Hauptflure). Gemessen: alle Flure ≥ 3 m, Zimmer ≥ 3,0 m. **Ausnahmen** (`exempt_types`): Kesselgänge
(2 m – Wiki: der Boiler Room ist beengend und vollgestellt) und Aufzugskabinen (2,8 m – übliche Kabinenhöhe).

## Level 5.3 – „Promethei Bibliotheca“ (Eigener Ort)

**Wiki:** Bibliothek aus Marmor und Tuff, Renaissance-Gemälde, sieben Etagen über Holztreppen, Hunderte Gänge in alle
Richtungen; Eisenstäbe an einem Gang neben einer Leere; Sackgassen mit Holztür zurück zum Eingang; Zugangsflur von Level 5
mit abgeblätterter Tapete.

**Im Spiel:** halle, gang, gang_alt, leere, podest/obergeschoss, treppe, eingang. Belegt. Schilder: nur die Inschrift.
**Maße:** Gänge ≥ 3 m, Decken ≥ 3 m (Hallen 7–8 m), Sichtachsen Median ≥ 28 m.

## Level 188 – „The Courtyard of Windows“ (Eigener Ort)

**Wiki:** Atrium ca. 45 m Durchmesser, 30 m hoch, weiße Betonwände mit 100–200 Fenstern, Mittelpfad (Fünfeck, vier Zuwege)
zum Birkenkreis, Moos; Gänge am Rand über mehrere Etagen, weiß, grell; leere weiße Räume. Keine Schilder.

**Im Spiel:** Hof 46 × 46 m, 30 m hoch; weißer Gang, Treppenhaus, Zellen. Belegt. **Entfernt (V13.3):** das Exit-Schild über
dem Eingangsfenster (Wiki nennt keine Schilder) → Grenze 0, Prüfung grün.

## Level 903 – „The Metro“ (Labyrinth, linear)

**Wiki:** endlos gleicher U-Bahnsteig, grauer Ziegelboden, links ein Gleis (fahrende Wagen halten kurz), rechts ein ruhender
Wagen durch die ganze Ebene, verbarrikadierte Treppen, lange Röhren, beleuchtete Schilder in unbekannter Sprache; zu Basen
umgebaute Wagen.

**Im Spiel:** ein Bahnsteig 196 × 22 m. Belegt. Schilder: die beleuchteten Schilder in fremder Schrift (keine Exit-Schilder).
**Maße:** Bahnsteighalle ≥ 4 m hoch (heute 5,3 m), Sichtachsen entlang des Bahnsteigs (Median ≥ 80 m).

## Level 37 – „Sublimity“ (Labyrinth)

**Wiki:** riesiges Netz verbundener, gefluteter Räume aus identischen weißen Fliesen; Becken und Flure, ungewöhnlich
geformte offene Bereiche, Säulenhallen, Treppen in tiefe Gruben, dunkle Bereiche, dunkle Tunnel; keine Wahrzeichen, keine Wesen.

**Im Spiel:** fliesen (eine Zone), Typen fliesenraum, saeulenhalle, grube, dunkelraum, tunnel, fliesengang – alle belegt.
Ähnlichkeit S = 0,95. Schilder: 0.

## Level 36 – „Timeless Airport“ (Mischform)

**Wiki:** Gepäckausgabe (endlose Bänder an einer Wand, Lost-and-Found-Büro), Check-in (endlose Schalterreihe, Anzeigen,
BTA-Stand mit lila Schildern), außer Betrieb gesetzte Sicherheitskontrollen, Concourses als nicht-euklidisches Labyrinth,
Duty-free-Läden, Laufbänder, Lounges, der „Meeting Place“, Gates mit Countdown (Gate C32 mit Gemeinschaft); Schilder
überall, „Exit“-Schildern nicht trauen.

**Im Spiel:** gepaeck, checkin, gang (Rundgang), laeden, gatehalle, gate C30–C34, sackgasse. **Ergänzen (Wiki belegt, fehlt):**
Sicherheitskontrolle (außer Betrieb), Lost-and-Found-Büro, eine Lounge, der Meeting Place. Schilder: Wegweiser überall;
Exit-Schilder höchstens 2 (Wiki: ihnen nicht trauen).
**Maße:** Hallen 4,5–8 m hoch, Gänge ≥ 4 m breit (Flughafen-Concourse 6–12 m). **Umgesetzt (V13.3):** Sicherheitskontrolle
(drei Spuren, außer Betrieb), Fundbüro an der Gepäckausgabe, Lounge, Meeting Place (Sitzreihen, rote Damasttapete). Die neuen,
kleineren Räume senken die Sichtachsen: Median 49,9 → 43,2 m, 90-%-Wert 95,2 → 84,7 m (Grenzen 42 / 84 m).
