# LEVELSPEC 5 – „Terror Hotel“ (im Spiel: „Das Hotel“)

- **Quelle:** https://backrooms-wiki.wikidot.com/level-5
- **Revision:** 184, zuletzt geändert am 07.08.2026, 06:34. Abgerufen am 25./26.10.2026.
- **Autoren:** Ursprünglich von Discord13 (Fandom); neu geschrieben von Stretchsterz; Ideen, Code und der Absatz zu Entity 18 von ratscrapz.
- **Lizenz:** CC BY-SA 3.0. Nur Fakten in eigenen Worten, keine Texte und keine Bilder (`LICENSES.md`).

## 1. Was das Wiki belegt (eigene Worte)

**Allgemein**
- Ein scheinbar endloser Hotelkomplex. Bauweise und Einrichtung wie in den 1930er Jahren, manche Möbel um 1920.
- Drei frei zugängliche Bereiche: Main Hall, Beverly Room, Boiler Room.
- Auffällig sauber: Flecken und Schmutz verschwinden binnen Minuten, die Böden wirken gepflegt.
- Leiser Jazz aus alten Plattenspielern und Lautsprechern; Musik und Wechsel scheinbar zufällig, die Quelle unbekannt.
- Unheimliche Geräusche: fernes Partygemurmel hinter Wänden, unverständliches Flüstern, Tippen auf die Schulter, das Gefühl, von alten Gemälden beobachtet zu werden.

**Main Hall**
- Wände: mahagonirote Tapete mit goldener Verzierung.
- Böden: dunkles Nussbaumholz, weißer Marmor, rot-goldene Teppiche.
- Einrichtung: Bücherregale, Schränke, Sofas, viele antike Dekorationen.
- Licht: antike Kerzenhalter, manche davon elektrisch, mit einem gleichmäßigen Summen wie in Level 0.
- Türen mit goldenen Plaketten, deren Zimmernummern keinem Muster folgen. Viele verschlossen, manche öffnen und schließen sich selbst.
- Die meisten Zimmer leer, manche voll eingerichtete Wohnräume.
- Aufzüge: viele Schächte, von modernen Aufzügen bis zu alten mit Kupfergitter.
- Der belebteste Bereich; hier findet man die meisten Dinge des Levels.

**Beverly Room („The Eternal Ballroom“)**
- Der zentrale Knoten mit Türen auf allen Seiten, sehr geräumig.
- In der Mitte ein kleiner, verzierter Tisch unter einem großen Kronleuchter, darauf viele Getränke und eine unfertige Mahjong-Partie.
- An den Eingängen ein kleines Silberschild mit dem Namen des Raums.

**Boiler Room**
- Eng und beklemmend; meist Kesselräume, manche Ofenräume.
- Große alte Maschinen, verschlungene Industrierohre an den Wänden, Ablassventile. In den Rohren fließt unter Druck Mandelwasser, es leckt.
- Heiß, Dampf in der Luft, die Flächen aber meist trocken. Ständiges Maschinengeräusch, Tropfen; manche hören leises Flüstern aus großen, rauchenden Öfen.
- Wartungsaufzüge (die Seite rät ab).

**Wesen:** Deathmoths sind die häufigsten Wesen; Level 5 gilt als zentraler Stock, mit Nestern im ganzen Level; sie schwärmen aus dunklen Zimmern in die Gänge. Andere Wesen (Hounds u. a.) eher selten.

**Gruppen:** M.E.G.-Außenposten „Housekeeping“, „Homely Hotel“, „The Originals“, „The Lost Hall Society“ (Reste in Level 5.3) – nicht dargestellt.

**Ein- und Ausgänge**
- Hinein u. a. aus Level 3 per Aufzug.
- Hinaus u. a. zurück nach Level 3 per Aufzug; tief in den Boiler Room nach Level 6; weitere Türen.

## 2. Was LIMINAL bewusst ergänzt

- **Maße:** Die Seite nennt keine. Hauptgänge 4 m breit und 3,6 m hoch, Zimmer 6 × 6 m, Beverly Room 42 × 34 m und 7 m hoch.
- **Tapete:** eigene, prozedural erzeugte Damasttapete in Mahagonirot und Gold (`tools/textures/generate.py`). Es gab keine passende CC0-Vorlage.
- **Zimmernummern** ohne Muster (117, 2041, 31, 608, 9, 343); Schrift Inter (SIL OFL 1.1) als Geometrie.
- **Vertäfelung** aus Nussbaum unter der Tapete; Bildnisse an den Wänden (die Seite nennt alte Gemälde).
- **Funde** liegen auf Beistelltischen in Gängen und Zimmern.
- **Übergänge** (siehe `LEVELAUSWAHL.md`):
  - Ein langer Gang führt nach Level 5.3; die Seite von 5.3 nennt einen Gang nahe dem Außenposten „Housekeeping“.
  - Ein helles Fenster führt nach Level 188, dessen Seite Fenster in Innen-Leveln als Eingang nennt.
  - Ein Metro-Wagen hält am Ende des Kesselgangs (**eigene Ergänzung:** der Ort des Halts) und fährt nach Level 903.

## 3. Konflikte und Grenzen

- **Bewohner und Gruppen** (Homely Hotel, Originals, Housekeeping): nicht dargestellt; der Hauptgang ist leer statt „belebt“.
- **Tippen auf die Schulter, sich selbst öffnende Türen:** nicht umgesetzt.
- **Aufzüge zu Stockwerk 1–382:** Die Aufzüge sind Kulisse; nur der Ankunftsaufzug hat eine Funktion (Ankunft aus Level 3).
- **Lobby Room:** unbestätigt, nicht dargestellt.
- **Ausgang nach Level 6:** gehört nicht zur Auswahl; der Kesselraum führt zur Metro.
