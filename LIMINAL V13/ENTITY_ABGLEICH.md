# Abgleich der Wesen mit dem Backrooms-Wiki (Wikidot)

Stand: 26.10.2026. Quelle ausschließlich **backrooms-wiki.wikidot.com**, alle Seiten an diesem Tag vollständig gelesen; lange Seiten (Level 1: 115 404 Zeichen, Level 3: 136 462 Zeichen) in mehreren Abschnitten bis zum Ende. Kein Fandom, nichts aus dem Gedächtnis. Alle Beschreibungen hier sind eigene Worte; die Wiki-Inhalte stehen unter CC BY-SA 3.0 (siehe `LICENSES.md`).

Status-Begriffe:

- **umgesetzt**: im Spiel, Verhalten nach Wiki
- **teilweise**: im Spiel, eine dokumentierte Eigenschaft fehlt (genannt)
- **geplant (V13.2)**: in dieser Runde zu bauen (Briefing `KREATUR_<Name>.md`); wird nach Abnahme auf „neu (V13.2)“ gesetzt
- **fehlt**: dokumentiert und sinnvoll umsetzbar, aber nicht im Spiel (Grund: Priorität, siehe unten)
- **Ausnahme (Menschen / Erreger ohne Körper)**: einzige erlaubte Ausnahmen nach Auftrag V13.3 Teil 4
- **bewusst weggelassen**: mit Grund (Altstand, wird in V13.3 abgebaut)
- **eigene Ergänzung**: im Spiel ohne Wiki-Beleg

## Quellen

| Seite | URL | Revision | Zuletzt bearbeitet |
|---|---|---|---|
| Entity-Übersicht | https://backrooms-wiki.wikidot.com/entities | 911 | 26.10.2026 13:43 |
| Level 0 | https://backrooms-wiki.wikidot.com/level-0 | 50 | 06.10.2026 13:32 |
| Level 1 | https://backrooms-wiki.wikidot.com/level-1 | 83 | 07.10.2026 14:09 |
| Level 2 | https://backrooms-wiki.wikidot.com/level-2 | 93 | 18.10.2026 05:03 |
| Level 3 | https://backrooms-wiki.wikidot.com/level-3 | 18 | 26.09.2026 16:16 |
| Level 5 | https://backrooms-wiki.wikidot.com/level-5 | 184 | 07.08.2026 06:34 |
| Level 5.3 | https://backrooms-wiki.wikidot.com/level-5-3 | 40 | 24.08.2026 13:03 |
| Level 188 | https://backrooms-wiki.wikidot.com/level-188 | 69 | 24.10.2026 20:46 |
| Level 903 | https://backrooms-wiki.wikidot.com/level-903 | 60 | 24.10.2026 17:11 |
| Level 37 | https://backrooms-wiki.wikidot.com/level-37 | 52 | 18.08.2026 02:36 |
| Level 36 | https://backrooms-wiki.wikidot.com/level-36 | 33 | 15.08.2026 14:34 |
| Entity 2 (Windows) | https://backrooms-wiki.wikidot.com/entity-2 | 36 | 24.10.2026 17:07 |
| Entity 3 (Smiler) | https://backrooms-wiki.wikidot.com/entity-3 | 83 | 07.08.2026 06:33 |
| Entity 4 (Deathmoths) | https://backrooms-wiki.wikidot.com/entity-4 | 6 | 31.01.2026 19:33 |
| Entity 5 (Clump) | https://backrooms-wiki.wikidot.com/entity-5 | 38 | 23.09.2025 09:55 |
| Entity 6 (Dullers) | https://backrooms-wiki.wikidot.com/entity-6 | 52 | 07.08.2026 06:53 |
| Entity 8 (Hounds) | https://backrooms-wiki.wikidot.com/entity-8 | 56 | 30.09.2025 17:26 |
| Entity 9 (Facelings) | https://backrooms-wiki.wikidot.com/entity-9 | 39 | 02.07.2025 01:55 |
| Entity 10 (Skin-Stealers) | https://backrooms-wiki.wikidot.com/entity-10 | 61 | 09.03.2025 00:58 |
| Entity 15 (Wretches) | https://backrooms-wiki.wikidot.com/entity-15 | 54 | 07.08.2026 06:38 |
| Entity 17 (Crawlers) | https://backrooms-wiki.wikidot.com/entity-17 | 40 | 01.06.2025 11:47 (als „veraltet“ markiert) |
| Entity 24 (Death Rats) | https://backrooms-wiki.wikidot.com/entity-24 | 25 | 01.06.2025 11:46 (als „in Überarbeitung“ markiert) |
| Entity 71 (The Red Knight) | https://backrooms-wiki.wikidot.com/entity-71 | 44 | 11.07.2026 23:05 (gelesen 02.11.2026) |
| Entity 99 (The Game Master) | https://backrooms-wiki.wikidot.com/entity-99 | 39 | 11.08.2026 08:28 |

## Je Ebene

### Level 0 – „The Lobby“

Die Seite nennt **keine bestätigten Wesen**.

| Wesen / Erscheinung | Wiki (eigene Worte) | Status im Spiel |
|---|---|---|
| Unbekannte dunkle Gestalten | Unbestätigte Berichte über dunkle Figuren, die einem folgen; das Gefühl, beobachtet zu werden. | **umgesetzt** als „Gestalt“: steht am Gangende, ändert sich nie, solange man hinsieht, und ist fort, sobald man wegsieht. Greift nie an (unbestätigt, daher harmlos). |
| Flüstern, vertraute Stimmen | Vielleicht Einbildung. | **umgesetzt** als Halluzination (`haunt.cpp`, Klang `hal_whisper_*`) |
| Kratzen hinter den Wänden | Folgt einem die Gänge entlang; man soll es ignorieren. | **umgesetzt** als Halluzination (`hal_scratch_*`) |
| Andere Wanderer (Menschen) | Menschen, die ebenfalls verirrt sind. | **Ausnahme (Menschen, keine Wesen)**: Das Spiel zeigt andere Wanderer nur über ihre Notizen (Einsamkeit als Grundton seit V7). Im Mehrspieler sind die Mitspieler die Wanderer. |

### Level 1 – „Habitable Zone“

Die Liste der häufigen Wesen auf der Ebenenseite:

| Wesen | Wiki (eigene Worte) | Status |
|---|---|---|
| Smiler (Entity 3) | Gesichter im Dunkeln, angezogen von Licht und Lärm; Licht ausmachen oder als Köder werfen. | **umgesetzt (V13.3)**: Licht hält ihn, Licht aus mit Abstand verliert er das Interesse, Rennen und Lärm lösen den Angriff aus (V13.2). Neu: Leuchtstab als werfbarer Köder (Fundstück, F knicken und werfen) – der Smiler geht zum stärkeren Licht und lässt vom Spieler ab, solange er leuchtet; Lärm oder Nähe holen ihn zurück (`KREATUR_Smiler.md`, Abschnitt V13.3). Eine geworfene Taschenlampe: nicht umgesetzt (Begründung dort). |
| Hound (Entity 8) | Menschenähnlich auf allen vieren; durch Blick und Lärm einschüchtern. | **umgesetzt** |
| Clump (Entity 5) | Verschmolzene Glieder, blind, jagt nach Gehör; still bleiben. | **umgesetzt** |
| Skin-Stealer (Entity 10) | Weiße Menschengestalten, die Haut tragen; ansprechen entlarvt sie; ruhig zurückweichen. | **umgesetzt (V13.3)**: getarnt in Menschenhaut, satt oder hungrig (V13.2). Neu: Ansprechen (E auf eine menschenähnliche Gestalt) – er antwortet verzögert, nachgeahmt und falsch betont (synthetisch); ruhiges Zurückweichen danach überlebt, Rennen, erneutes Ansprechen oder zu lange Nähe lösen den Angriff aus (`KREATUR_SkinStealer.md`, Abschnitt V13.3). |
| Wretch (Entity 15) | Aggressive, untote Menschengestalten. | **neu (V13.2)**: träge und schnelle Wretches (`KREATUR_Wretch.md`) |
| Duller (Entity 6) | Menschengestalten, die durch Wände greifen; bei Schaben in der Wand Abstand von Wänden halten. | **umgesetzt (V13.3)**: Hinterhalt durch die Wand mit Schaben und Wölbung, flieht gesehen lautlos, meidet Mandelwasser – [KREATUR_Duller.md](KREATUR_Duller.md) |
| Arms (französischer Zweig, entite-15-fr) | Eine Gliedmaße durch Lüftungen und Risse bei Stromausfall; Licht darauf richten. | **umgesetzt (V13.3)**: aus Deckenlüftungen und Rissen nur im Dunkeln, von Lärm angelockt, Schaben im Schacht, Licht verbrennt es – [KREATUR_Arms.md](KREATUR_Arms.md) |
| Crawler (Entity 17) | Ein krankmachender Pilz. | **umgesetzt (V13.3)**: weißer Befall an feuchten Stellen (Wand, Pfütze, Kadaver, Lager eines Infizierten mit Schleifspur), Ansteckung durch Waten im Befall oder offenes Mandelwasser, Stadien nach Wiki-Zeitleiste bis zum Tod, Flecken und Belag auf der eigenen Hand, Mandelwasser heilt früh (Interpretation) – [KREATUR_Crawler.md](KREATUR_Crawler.md) |
| Growler (Entity 130) | Wesen aus Kabeln; ein schwerer Gegenstand auf dem Kopf verwirrt sie. | **umgesetzt (V13.3)**: ausgewachsen, blind (Echoortung), telepathisches Rauschen, tödlicher Stoß nach Ankündigung; ein geworfener Gegenstand verwirrt ihn – [KREATUR_Growler.md](KREATUR_Growler.md) |
| Nguithr'xurh (Entity 16) | Insektenartig an der Decke, bei kugelförmigen Netzen. | **umgesetzt (V13.3)**: Kolonien an Gangdecken, Netzkugeln mit Betäubungsstoff, groß und hungrig – [KREATUR_Nguithrxurh.md](KREATUR_Nguithrxurh.md) |
| Wrangler (Entity 75) | Riesig, schlangenartig, gräbt sich durch Wände, hypnotisiert; leise gehen, wenn die Wände beben. | **umgesetzt (V13.3)**: Beben, Durchzug durch Wand, Boden oder Decke, Sog des Anblicks, wer laut ist, wird geschnappt; selten Jungtiere – [KREATUR_Wrangler.md](KREATUR_Wrangler.md) |
| Staphylococcus liminalis (Entity 19) | Bakterieninfektion; Hygiene schützt (Fußnote). | **Ausnahme (Erreger ohne Körper)**: das Spiel hat kein Krankheitssystem |
| Plague Goblins (Entity 25) | Kleine Wesen, mit Messer oder Axt zu verscheuchen (Fußnote; die Wesensseite: katzengroße Vogel-Katzen in Pestarztmaske und Umhang, Banden von 9–27, bestehlen Alleinreisende). | **umgesetzt (V13.3)**: Bande mit Portal der Torwächterin, Späher, Henchmen; Diebstahl von hinten mit Ankündigung, Geschwächte werden bedrängt (nie bis zum Tod), Handel gegen Mandelwasser, Flucht vor Hounds; das Beil als Fund in Level 1 verscheucht und trifft – [KREATUR_PlagueGoblins.md](KREATUR_PlagueGoblins.md) |
| Narrenfigur im Gemälde (Entity 99) | Ein Bild, das als Ausgang zu Level 389 dient. | **umgesetzt (V13.3)**: Gemälde am Boden vor den Hallentüren verschwinden unbeobachtet; ihr Bild mit Bildschirmgesicht (wechselt ungesehen), roten Kugeln und Fäden lädt ein und wirft „aus dem Spiel“ – anderswo in Level 1 (Interpretation, Level 389 fehlt) – [KREATUR_GameMaster.md](KREATUR_GameMaster.md) |
| Garden-Sektor (Verwandlung) | Wer etwa eine Stunde bleibt, wird zur Pflanze. | **umgesetzt (V13.3)**: Garden-Uhr, Zeichen, Ranken und Rinde auf der Hand, Blätter am Rand der Sicht, Tod durch Verwandlung, Erholung draußen; Gestalten aus Rinde – [KREATUR_L1_Erscheinungen.md](KREATUR_L1_Erscheinungen.md) |
| Dunkles, stilles Wesen an der Schlafraumtür (Erzählung) | Klatschende Schritte, Krallenspuren, zähe Flüssigkeit. | **umgesetzt (V13.3)**: Krallenrillen und schwarze Läufer neben einer Zimmertür, Abdrücke an der Schwelle, selten Schritte, Krallen und Atem hinter der Tür – [KREATUR_L1_Erscheinungen.md](KREATUR_L1_Erscheinungen.md) |
| Gruppen (M.E.G., B.N.T.G., Ariane Circle u. a.) | Menschliche Gruppen. | **Ausnahme (Menschen, keine Wesen)**: Menschen, keine Wesen; nur über Notizen angedeutet |
| Schattenhafte Arbeiter im Overall (Sektor Ouroboros; backrooms-remodeling-co) | Reparieren seit Jahrzehnten ohne Fortschritt, beachten Lebende nicht, verletzen mit ihren Maschinen; meiden. | **umgesetzt (V13.3)**: Baustelle nur im Sektor Ouroboros – eine Mauer, gebaut und wieder eingerissen, ein Gerüst, das umkippt; sie sehen nie zum Spieler und schieben ihn beiseite; Gefahr nur durch Maschinen im Absperrband, jede angekündigt – [KREATUR_OuroborosArbeiter.md](KREATUR_OuroborosArbeiter.md) |

### Level 2 – „Pipe Dreams“

| Wesen | Wiki (eigene Worte) | Status |
|---|---|---|
| Windows (Entity 2) | Am Gangende; nicht nähern. | **umgesetzt (V13.3)**: bildet sich ungesehen ein eigenes altes Stahlfenster in der Stirnwand einer Sackgasse (9 Gangenden), kündigt sich mit Glas, Rahmen und Außenluft an, dann Flüstern, Winken, Greifen wie in Level 188 – [KREATUR_Fensterwesen.md](KREATUR_Fensterwesen.md), Abschnitt Level 2 |
| Smiler (Entity 3) | Häufig, lieben Dunkelheit, schwärmten beim großen Stromausfall. | **umgesetzt** (an Stromausfälle gebunden) |
| Deathmoths (Entity 4) | Vom Licht angezogen, meist friedlich, seit dem Stromausfall kleiner. | **umgesetzt (V13.3)**: kleine Gruppen an brennenden Röhren ohne Nest, meist Männchen mit Saugrüssel; die Lampe lockt sie, aus der Nähe gereizt schwirren sie vor dem Gesicht, nur ein Weibchen beißt (leicht); treiben in die Netze der Nguithr'xurh – [KREATUR_Deathmoth.md](KREATUR_Deathmoth.md), Abschnitt Level 2 |
| Clump (Entity 5) | Häufig und tödlich; lauern an Ecken und springen. | **umgesetzt** (Lager an Kreuzungen und in Schächten) |
| Jerry (Entity 7) | In einem Zimmer eingeschlossen; wen er oder sein Kult sieht, der entkommt nicht. | **umgesetzt (V13.3)**: ein kranker, halb gerupfter Hyazinth-Ara auf einem rostigen Rohr in einer blau gestrichenen Kammer mit nur einer Tür (Bild der Followers an der Wand, Opfertisch mit Kerzen); man hört ihn vorher mit ruhiger Menschenstimme murmeln. Gefahr ist der Blickkontakt: der Bann wächst, zieht den Blick zu ihm und lähmt, kündigt sich mit Klingeln und zuletzt mit ausgebreiteten Flügeln und Kreischen an – wegsehen und gehen rettet; wer ihm zu nahe kommt, dem springt er ins Gesicht – [KREATUR_Jerry.md](KREATUR_Jerry.md) |
| Hound (Entity 8) | In Rudeln, werden gestört zu schnellen Jägern. | **umgesetzt (V13.3)**: nur im Rudel (3–4, eigenes Modell: Glieder und Haar von der schwarzen Flüssigkeit der Rohre verklebt, offene Stellen); es zieht langsam umher (man hört Kläffen und Pfoten), gestört (Lampe, Rennen, Sicht) ruft einer, alle werden schlagartig schnell und kreisen ein; immer nur einer springt, nach scharfem Kläffen – wer ihn ansieht, bricht den Sprung ab; Bisse schwer, nicht sofort tödlich; Rennen reizt, ruhiges Zurückweichen mit Blickkontakt lässt das Interesse verfliegen – siehe `KREATUR_Hound.md`, Abschnitt „Rudel“ |
| Faceling (Entity 9) | Intelligent, in Gruppen, lassen einen in Ruhe, wenn man sie nicht reizt. | **neu (V13.2)**: Gruppen, reizbar (Lampe im Gesicht, Bedrängen, Anrempeln), dann leichte Schläge; die Gruppe macht mit (`KREATUR_Faceling.md`) |
| Skin-Stealer (Entity 10) | Haben die Ebene nach dem Stromausfall verlassen. | **umgesetzt (V13.3): Spuren, laut Seite nicht mehr da** – abgestreifte, vertrocknete Menschenhäute (eine geschmolzen in den heißen Tunneln, eine an einem Haken), Kratzspuren, eine verlassene Lagerstelle – [KREATUR_L2_Spuren.md](KREATUR_L2_Spuren.md) |
| Wretch (Entity 15) | Auf fast jeder Ebene; griffen 2014 die Survivors an. | **neu (V13.2)**: gemischt träge und schnell, 30 % im zweiten Stadium (flehen, greifen nicht an) |
| Nguithr'xurh (Entity 16) | Jagen Insekten und Motten, selten Menschen. | **umgesetzt (V13.3)**: klein, über den Maschinen, eingesponnene Deathmoths im Netz; nur selten hungrig – [KREATUR_Nguithrxurh.md](KREATUR_Nguithrxurh.md) |
| Death Rat (Entity 24) | Fast harmlos, fressen Flüssigkeiten aus Fässern und Essen. | **umgesetzt (V13.3)**: haarlose Ratten klettern auf die Fässer und trinken am Spund, fressen Essensreste, fliehen vor dem Spieler in die Ecken – [KREATUR_DeathRat.md](KREATUR_DeathRat.md), Abschnitt Level 2 |
| Cannibal Cuisine (Entity 36) | Unbewegliche, leuchtende Wesen an Sackgassen; Wegmarken im Stromausfall, nicht gefährlich. | **umgesetzt (V13.3)**: 6 leuchtende Automaten an Sackgassen und kahlen Wänden, ihr Licht bleibt im Stromausfall an; drinnen flüstern Leichen, Knochenhände schieben gratis Ware vor, die süchtig macht (Mandelwasser wirkt nicht mehr, Entzug fern der Automaten); nur wer ihn angreift, den schlagen Skelettarme aus den Klappen – [KREATUR_CannibalCuisine.md](KREATUR_CannibalCuisine.md) |
| Wrangler (Entity 75) | Graben sich durch den Boden und überfallen aus den Gängen. | **umgesetzt (V13.3)**: vor allem Männchen, quer durch die Gänge, Kopf bricht aus Wand oder Boden; keine Jungtiere – [KREATUR_Wrangler.md](KREATUR_Wrangler.md) |
| Photoshop (Entity 104) | Keine Art; selten, vor allem in PC-Räumen. | **umgesetzt (V13.3)**: höchstens einer je Lauf (60 %), hinter dem Röhrenmonitor eines Büros, der sein Bild zeigt; Klotz aus Fleischfotos mit Augen und Pitbull-Maul (Abnahme Level 2: echte Aufnahmen gebauter Szenen aus Blender als blutige Abzüge, Collage um den Klotz), glitcht; unter fünf Fuß schrumpft eine Box mit schwarzen Kanten (5 s) – hindurchgehen oder warten heißt ausgeschnitten, ein hingeworfener Gegenstand rettet (Arm aus dem Bildschirm, dann ist er fort); keine Kamera im Spiel (Interpretation) – [KREATUR_Photoshop.md](KREATUR_Photoshop.md) |
| Growler (Entity 130) | Klein; verschmelzen in engen Gängen, bis sie stecken bleiben. | **umgesetzt (V13.3)**: junge Growler (leuchtend bunt), verschmelzen, bleiben stecken und verhungern; Leichen in den Gängen – [KREATUR_Growler.md](KREATUR_Growler.md) |
| Swindlebird (Entity 192) | Eine einzelne, unbestätigte Sichtung. | **umgesetzt (V13.3)**: höchstens eine flüchtige Sichtung je Lauf (50 %) – ein großer blauer, papageienhafter Vogel am Ende eines Tunnels, aufgetaucht, wo niemand hinsah; angesehen, angeleuchtet oder bei Annäherung fort, beim Wegsehen einfach weg; keine Spur, kein Beweis – [KREATUR_Swindlebird.md](KREATUR_Swindlebird.md) |
| Macchina (Entity 189) | Seit etwa 1850 nicht mehr gesehen. | **umgesetzt (V13.3): Reste, laut Seite seit 1850 nicht gesehen** – in der Werkstatt ein leerer Anzug mit geschweißtem Gesicht (klingt im Luftzug), ein unfertiges Kindergestell, ein Wandtuch der großen Antwort, eine Pike mit Brandröhre und eine Blechtafel mit Zeichen (Notizbuch) – [KREATUR_Macchina.md](KREATUR_Macchina.md) |
| Gruppen (Followers of Jerry, Survivors, B.N.T.G. …) | Menschen. | **Ausnahme (Menschen, keine Wesen)** (Menschen). V13.3: von den Followers of Jerry nur ihre Spuren – Wandbild, Opfertisch, Fotos und Pfeile zu seiner Tür, Flugblätter als Notizen ([KREATUR_Jerry.md](KREATUR_Jerry.md)) |

### Level 3 – „Electrical Station“

| Wesen | Wiki (eigene Worte) | Status |
|---|---|---|
| Hound (Entity 8) | Lauern allein, bellen, um gemeinsam zu jagen, quälen ihre Opfer. | **umgesetzt (V13.3)**: drei lauern einzeln flach in Nischen (Brandnarben, Ruß; Augen ohne Schein), springen von hinten an oder rufen – die anderen antworten aus ihren Gängen und kommen; der Rufer bellt im Takt, schneller, wenn man auf eine Sackgasse oder einen anderen Hound zuläuft; sie quälen in Stufen (Nacken: gelähmt, Bein: hinkt, Kehle: kein Ruf an die Mitspieler), angekündigt, Blickkontakt hält sie kurz zurück – siehe `KREATUR_Hound.md`, Abschnitt „Rudel“ |
| Faceling (Entity 9) | Alle feindselig und bewaffnet (Steinwerkzeuge), „entwickelt“ mit Augen-, Nasen- und Mundansätzen. | **neu (V13.2)**: Variante „entwickelt“ mit Steinwerkzeug; kurzsichtig und schwerhörig (Entity 9) – bemerken einen erst nah (Abnahme Level 3: Bart aus Strähnen statt Klotz, eigene stumpfe Haut und eigenes Haar – [KREATUR_Faceling.md](KREATUR_Faceling.md), Runde 4) |
| Wretch (Entity 15) | Selten, behalten den Körper, rennen und tragen Waffen, sprechen in Bruchstücken. | **umgesetzt (V13.3)**: selten (~480 s), bekleidet (zerrissene Arbeitsjacke, Wundbrand), rennen in Stößen, selbst gebaute Waffen (Rohrstück, Nagelbrett, Blechklinge) mit Ausholen und Schrei als Ankündigung; Sprachfetzen ohne Lippen (synthetisch, mit Untertitel); lockt von weitem mit verständlichen Fetzen – wer näher kommt oder ihn anspricht, wird angegriffen (Wiki: nicht interagieren) – [KREATUR_Wretch.md](KREATUR_Wretch.md), Abschnitt V13.3 |
| Deathmoths (Entity 4) | Gefährliche Schwärme, die Mund, Nase, Ohren und Augen angreifen; Praetorianer überwiegen. | **umgesetzt (V13.3)**: Schwarm an einer brennenden Röhre (gewöhnliche Motten in voller Größe, Männchen und Weibchen greifen an) mit gut 40 % Praetorianern (knapp 1 m, acht Giftbeine, verkümmerte Flügel; eigenes Modell); Licht lockt; Angriff auf Mund, Nase, Augen, Ohren mit Ankündigung (Brummen vor dem Gesicht; Abnahme Level 3: Fülllicht und Flügelstaub, damit Flügel, Leib und Staub im Gesicht lesbar sind), Lampe aus und Rennen helfen; Praetorianer bäumen sich auf, stechen (Lähmung), spucken Säure, einer nimmt den Umweg und kommt von hinten (Noclip) – [KREATUR_Deathmoth.md](KREATUR_Deathmoth.md), Abschnitt V13.3 Level 3 |
| Skin-Stealer (Entity 10) | Tarnen sich als Menschen, sprechen einfaches Englisch, halten die Tarnung tagelang. | **umgesetzt (V13.3)**: getarnt im Mantel (Kapuze, Schal, Sonnenbrille, faulende Haut); schließt sich an und bleibt lange in der Nähe, spricht einfache, leicht falsche englische Sätze mit Akzent (synthetisch, mit Untertitel); verwandelt sich erst nach langer Zeit (hungrig) oder bei Entlarvung – Ansprechen (beim zweiten Mal derselbe Satz, beim dritten fällt die Tarnung) oder Lampe im Gesicht; satt verabschiedet er sich – [KREATUR_SkinStealer.md](KREATUR_SkinStealer.md), Abschnitt V13.3 |
| Smiler (Entity 3) | Verbergen Augen und Zähne hinter Lidern und Lippen; greifen ohne Anlass an. | **umgesetzt (V13.3)**: mit Stromausfall; das Verbergen als langsames Öffnen der Augen. **Ohne Anlass:** etwa jeder dritte greift nach 9–22 s an, auch wenn man still steht und hinsieht – fair angekündigt: Lider und Lippen pressen sich zu und reißen auf (2,2 s, eigener Klang, Hinweis); Wegrennen, Licht aus und zur Seite oder Raumlicht retten (`smiler_unprovoked: 0.35`). Ein geworfener Leuchtstab lässt ihn hier nur kurz stutzen (`smiler_bait: hesitate`) – [KREATUR_Smiler.md](KREATUR_Smiler.md), Abschnitt „V13.3: Level 3“ |
| Clump (Entity 5) | Viel größer, geschickt, mit Waffen aus eigenen Knochen. | **umgesetzt (V13.3)**: neben den kleinen Bündeln das große (eigenes Modell, ×1,35: Muskelknoten, geordnete Arme, Knochenspieß, Knochensichel, rostiges Rohr; Abnahme Level 3: eigene Haut mit Totenflecken, Narbennähten, Blut, Schmier und Nässe statt Puppenhaut); es hängt an der Decke, jagt dort nach Gehör, lässt sich fallen, läuft koordiniert, holt angekündigt aus (Knochen schabt) und schlägt zur Stelle des Lärms; bricht das Rohr, reißt es sich den Knochen frei (zweites Modell); vor mehreren Rennenden weicht es zurück – siehe `KREATUR_Clump.md`, Abschnitt „Level 3“ |
| Death Rat (Entity 24) | Wie Wasserschweine mit geschärften Zähnen, bauen Fallen, menschlich klug. | **umgesetzt (V13.3)**: groß wie ein Wasserschwein (eigenes Modell: Fassleib, frei liegende geschliffene Nagezähne, graue Finger, Sprachgerät am Hals); wetzt die Zähne am Gitter (Geräusch vor Sicht), spricht über das Gerät (Untertitel), wartet außerhalb der Lampe, baut Stromfallen auf dem Weg des Spielers (blanker Draht in 18 cm – Abnahme Level 3: zum Seil verdrillt, mit hellen Fetzen abgenagter Isolierung, damit man ihn sieht –, Knistern, rote Leuchte; Schlag mit Lähmung, jede vierte versagt), treibt zur Falle, droht vor dem Biss, merkt sich Weg, Lampe und Sprünge – [KREATUR_DeathRat.md](KREATUR_DeathRat.md), Abschnitt V13.3 Level 3 |
| „Purpurne“ tote Wesen in den Wänden (Purpification) | Selten (17 Fälle): tote Wesen, halb in die Wand genoclippt; sie und die Wand um sie immer ganz violett; Gammastrahlung und eine unbekannte, violett leuchtende Energie; neben einem Hound eine gemalte Botschaft. | **umgesetzt (V13.3)**: Erscheinung ohne Rolle – ein Skin-Stealer im dunklen Paranoia-Gang, ein Hound mit Botschaft im Ziegelgang (eigene Modelle, Wand in vier Stufen violett); violettes Licht, Brummen und Knistern, Strahlung zehrt nah am Verstand, die Lampe stockt; siehe `KREATUR_L3_Purpur.md` |
| Schattenfigur hinter den Gittern, Stimmen in Paranoia-Gängen | Unbestätigt. | **umgesetzt** über den Paranoia-Gang der Karte und die Halluzinationen |
| Engelsstatue im Sanktuarium | Schreckt alle Wesen ab; einziger sicherer Ort. | **umgesetzt** (Sanktuarium mit Engel, bleibt hell). Neu in V13.2: Wesen meiden den Raum (siehe Briefings) |
| Jerry (Entity 7) | Nur in einem Vorfallbericht (Followers in den Wartungsgängen unter einer Basis). | **umgesetzt (V13.3)** als Wesen von Level 2 (sein Raum, Blick und Bann) – [KREATUR_Jerry.md](KREATUR_Jerry.md); auf Level 3 kein eigener Auftritt (die Seite zeigt ihn dort nicht) |
| Red Knight (Entity 71) | Vorfall GB-03: rannte in Panik durch das Atrium von Base Gamma, die Rüstung schwer verbeult, mit Purpurspuren; niemand erinnert sich, wie er kam oder ging; vermutlich wegen einer antimemetischen Kraft. Entity-71-Seite: Ritter in Plattenrüstung, Zweihänder auf der Schulter, roter Umhang mit Goldsaum; rettet Menschen, schneidet einen weißen Riss in die Luft. | **umgesetzt (V13.3)**: selten (~15 min, höchstens zweimal); man hört ihn vor Sicht (Rüstung im Laufschritt, Keuchen im Helm), er rennt in Panik am Spieler vorbei (eigenes Modell: verbeulte Platten mit violett glimmenden Dellen, zerfetzter roter Umhang, Zweihänder), haut in die leere Luft hinter sich, sieht den Spieler kurz an, schneidet außer Sicht einen Riss (weißes Licht) und ist fort; danach zieht unsichtbar der Verfolger vorbei (Leuchten flackern, Druck in den Ohren, einmal −3 Verstand). Greift nie an. Variante der Rolle Wretch (keine eigene Rolle) – [KREATUR_RedKnight.md](KREATUR_RedKnight.md) |
| Basen und Gruppen (M.E.G., Base Gamma) | Menschen und Siedlungen. | **Ausnahme (Menschen, keine Wesen)** (Menschen, keine Wesen; nur über Notizen angedeutet) |

### Level 5 – „Terror Hotel“

| Wesen | Wiki (eigene Worte) | Status |
|---|---|---|
| Deathmoth (Entity 4) | Sehr häufig, die größte Gefahr; fliegen aus dunklen Zimmern, die Weibchen bleiben in den Nestern; die Ebene gilt als einer der zentralen Stöcke. | **umgesetzt** |
| Nest der Motten | Ebenenseite: Nester überall auf der Ebene, ohne Beschreibung. Entity-4-Seite: Schichten aus verdautem Material, Pflanzenfasern und Harz, 60–100 cm dick, Säulen aus Resten, fleischige Wucherungen über Türen, Eier an der Auskleidung. | **umgesetzt**, in V13.2 neu entworfen (Entscheidung E-026) |
| Hound (Entity 8) | Kommt vor, ohne Verhalten. | **umgesetzt** (selten) |
| Skin-Stealer (Entity 10) | Kommt vor, ohne Verhalten. | **neu (V13.2)**: wahre Gestalt (Verhalten nach der Entity-Seite) |
| Death Rat (Entity 24) | Kommt vor, ohne Verhalten. | **neu (V13.2)**: Ratten mit dem Schrei-Ereignis (Entity 24: Kreischen, das Licht flackert); dass sie männliche Motten fressen, ist nur dieses Ereignis außer Sicht (das Spiel hat nur Weibchen) |
| Woodlins (Entity 45), Watchers (Entity 38), Ants (Entity 89) | Nur genannt, ohne Verhalten auf der Ebenenseite. | **fehlt**: Priorität 13. Ohne Verhalten auf der Ebene wäre jedes Verhalten erfunden; die Entity-Seiten sind noch nicht ausgewertet. |
| Growlers (Entity 130), Nguithr'xurh (Entity 16) | Nur genannt, ohne Verhalten auf der Ebenenseite. | **umgesetzt (V13.3)** nach den Entity-Seiten: selten ein ausgewachsener Growler, mittelgroße Nguithr'xurh – [KREATUR_Growler.md](KREATUR_Growler.md), [KREATUR_Nguithrxurh.md](KREATUR_Nguithrxurh.md) |
| Wrangler (Entity 75) | Eine einzelne Meldung aus dem Kesselraum. | **umgesetzt (V13.3)**: selten (~900 s), nur im Kesselgang/Kesselraum – [KREATUR_Wrangler.md](KREATUR_Wrangler.md) |
| Samantha (Entity 26) | Wandert manchmal durch die Haupthalle; wer sich nähert, soll gutes Fleisch mitbringen. | **fehlt**: Priorität 14. Ein einzelnes, benanntes Wesen mit Handel (Fleisch) – das Spiel hat keinen Tauschhandel. |
| Entity 18 (Stimme, die in ihr Büro bittet) | Erzählstimme der Seite. | **bewusst weggelassen** (Erzählung) |
| Unsichtbare Anwesenheit, Flüstern aus den Kesseln, Partygeräusche | Unbestätigt. | **umgesetzt** als Halluzinationen und Klang der Ebene |
| Gruppen (Housekeeping, Homely Hotel, The Originals, Lost Hall Society) | Menschen. | **Ausnahme (Menschen, keine Wesen)** (Menschen) |

### Level 5.3 – „Promethei Bibliotheca“

| Wesen | Wiki (eigene Worte) | Status |
|---|---|---|
| Library Mind | Bewusstsein der Bibliothek; nimmt Erinnerungen gegen Wissen, zeigt Wut durch fallende Bücher und einstürzende Regale. | **umgesetzt** als Erscheinung: Bücher fallen, Verstand sinkt langsam (keine Gestalt) |
| Entity 18 („Gentleman“, Beast of Level 5) | Feind der Bibliothek, erscheint in der Schlussgeschichte. | **bewusst weggelassen** (Erzählung, keine Begegnung auf der Ebene) |
| Ambassadors (Entity 141), Macchina (Entity 189) | Nur als Wissen in der Bibliothek genannt. | **bewusst weggelassen** (nicht anwesend) |
| Menschen und Gruppen | Erzählung. | **Ausnahme (Menschen, keine Wesen)** |

### Level 188 – „The Courtyard of Windows“

| Wesen | Wiki (eigene Worte) | Status |
|---|---|---|
| Fensterwesen (Entity 2) | Ersetzen gewöhnliche Fenster, große Teile der Atriumwände; dahinter schattige Gestalten; nicht nähern, sie sind keine Ausgänge. | **neu (V13.2)**: drei Fensterwesen in dunklen Fenstern des Hofs – flüstern, winken, greifen unter 1,9 m (`KREATUR_Fensterwesen.md`) |
| Faceling im rosa Kleid (Entity 9) | In den Gängen; tödlich, tötete ein Expeditionsmitglied mit Hieben und Stichen. | **neu (V13.2)**: Variante „rosa Kleid“, nur in den Gängen, tödlich; lässt im Hof ab |
| Skin-Stealer (Entity 10) | Bösartige Bewohner der Gänge. | **neu (V13.2)**: wahre Gestalt, nur in den Gängen |
| Schattenhafte Umrisse | Gleichen den Fensterwesen, an Ecken und am Rand des Blicks; nicht direkt ansehen. | **umgesetzt** als Gestalt |
| Andere bösartige menschenähnliche Wesen | Nicht beschrieben. | **bewusst weggelassen** (keine Beschreibung) |
| Entity 39, Amor Incrementum | Nur verlinkt; ein Kult ohne Mitglieder auf der Ebene. | **bewusst weggelassen** |

### Level 903 – „The Metro“

| Wesen | Wiki (eigene Worte) | Status |
|---|---|---|
| Metro (Entity 756) | Ein empfindungsfähiger, selbstfahrender Zug, der Ziele annimmt; neutral. | **umgesetzt** als fahrender Wagen, der Ziele annimmt (Level 37, Level 36) |
| Faceling (Entity 9) | Friedlich, leben neben Menschen. | **neu (V13.2)**: drei gewöhnliche Varianten, winken, wen sie wiedererkennen |
| Bewohner, feindselige Lokomotiv-Basen | Menschen. | **Ausnahme (Menschen, keine Wesen)** (Menschen; der bewohnte Wagen ist als Kulisse da) |

### Level 37 – „Sublimity“

Die Seite sagt ausdrücklich: **keine Wesen** („Devoid of Entities“). Im Spiel: keine. **übereinstimmend**

### Level 36 – „Timeless Airport“

Die Seite nennt **keine Wesen** (Wertung 0/5); nur Reisende, Gruppen und eine Durchsagestimme. Im Spiel: keine Wesen, die Durchsage gehört zum Klang der Ebene. **übereinstimmend**

## Bestehende Wesen gegen ihre Entity-Seiten

| Wesen | Entity-Seite (eigene Worte) | Spiel | Abweichung und Maßnahme |
|---|---|---|---|
| Smiler | Leuchtende Augen und breites Lächeln, Körper unsichtbar; folgt Lichtträgern; greift an, wenn man in Panik rennt **oder laut ist**. Blick halten, langsam zurück, Licht aus, Lichtquelle werfen. | Blick hält ihn; Lampe an hält sein Interesse; Rennen löst nach 1,5 s Ankündigung den Zugriff aus. | Lärm fehlte als Auslöser → **V13.2: harte Landung und Aufprall geworfener Dinge in seiner Nähe wirken wie Rennen.** Werfen einer Lichtquelle: **V13.3: Leuchtstab als Köder** (`KREATUR_Smiler.md`). |
| Hound | Auf allen vieren, langes schwarzes Haar über einem großen Maul, dürr; wird feindselig, sobald er einen Menschen sieht; Blickkontakt schüchtert kurz ein; Knurren heißt: er ist nah. | Genau so umgesetzt (Knurren, geduckt pirschen, Blick schüchtert ein, Rücken oder Rennen → Angriff). | keine |
| Clump | Bündel menschlicher Glieder, klebrige Haut, Augen und Ohren im Klumpen, Maul in der Mitte; greift mit dem längsten Glied ab etwa 2,4 m; ignoriert manche Vorbeigehende; still bleiben. | Hört nur; holt unter 2,5 m mit dem langen Arm aus. | keine. Das Lager im Deckenschacht ist **eigene Ergänzung** (die Seite: klettert Wände, nutzt Lüftungen). |
| Deathmoth | Männchen friedlich, Weibchen in Schwärmen gefährlich; Licht lockt; je näher am Stock, desto feindseliger. | Schwarm fliegt bei Licht oder Nähe auf, umkreist die Lampe, einzelne stechen. | keine; die Kasten (Praetorian, Empress) fehlen bewusst (Level 5 zeigt gewöhnliche Motten). |
| Gestalt | – (Level 0 und Level 188: unbestätigte dunkle Gestalten) | ändert sich nie, solange man hinsieht | keine; der Name „Gestalt“ ist eigene Benennung |

## Priorität der fehlenden Wesen

Bewertet nach Zahl der Ebenen, Dichte der Beschreibung auf den Ebenenseiten und Eignung fürs Spiel:

| Rang | Wesen | Ebenen im Spiel | Entscheidung |
|---|---|---|---|
| 1 | Faceling (Entity 9) | 2, 3, 188, 903 | **neu (V13.2)**: vier Verhaltensweisen je Ebene, fünf Varianten |
| 2 | Fensterwesen (Entity 2) | 2, 188 | **neu (V13.2)** in Level 188; Level 2 offen |
| 3 | Skin-Stealer (Entity 10) | 1, 3, 5, 188 | **neu (V13.2)**: wahre Gestalt und getarnt |
| 4 | Wretch (Entity 15) | 1, 2, 3 | **neu (V13.2)**: träge, schnell, flehend |
| 5 | Death Rat (Entity 24) | 2, 3, 5 | **neu (V13.2)**: mit Fell, haarlos, Schrei-Ereignis |
| 6 | Duller (Entity 6) | 1 | **umgesetzt (V13.3)**: [KREATUR_Duller.md](KREATUR_Duller.md) |
| 7 | Nguithr'xurh (Entity 16) | 1, 2, 5 | **umgesetzt (V13.3)**: [KREATUR_Nguithrxurh.md](KREATUR_Nguithrxurh.md) |
| 8 | Growler (Entity 130) | 1, 2, 5 | **umgesetzt (V13.3)**: [KREATUR_Growler.md](KREATUR_Growler.md) |
| 9 | Arms (entite-15-fr) | 1 | **umgesetzt (V13.3)**: [KREATUR_Arms.md](KREATUR_Arms.md) |
| 10 | Wrangler (Entity 75) | 1, 2, 5 (Kesselraum) | umgesetzt (V13.3) – [KREATUR_Wrangler.md](KREATUR_Wrangler.md) |
| 11–14 | Cannibal Cuisine, purpurne Wesen, Woodlins/Watchers/Ants, Samantha | je eine | Cannibal Cuisine und purpurne Wesen **umgesetzt (V13.3)**; Woodlins/Watchers/Ants und Samantha offen (Level 5) |

Ab Rang 6 sind die Wesen in dieser Runde nicht gebaut. Der Grund ist der Aufwand je Wesen: Briefing, Blender-Modell, Rig, Animationen, Verhalten, Klang, Gleichgewicht, mindestens drei Kritikrunden sowie Server-KI und Tests im Mehrspieler. Die Reihenfolge steht hier, damit die nächste Runde dort weitermacht. Der jeweilige Stand ist in `REPORT_V13_2.md` vermerkt.
