# Abgleich der Wesen mit dem Backrooms-Wiki (Wikidot)

Stand: 26.10.2026. Quelle ausschließlich **backrooms-wiki.wikidot.com**, alle Seiten an diesem Tag vollständig gelesen; lange Seiten (Level 1: 115 404 Zeichen, Level 3: 136 462 Zeichen) in mehreren Abschnitten bis zum Ende. Kein Fandom, nichts aus dem Gedächtnis. Alle Beschreibungen hier sind eigene Worte; die Wiki-Inhalte stehen unter CC BY-SA 3.0 (siehe `LICENSES.md`).

Status-Begriffe:

- **umgesetzt**: im Spiel, Verhalten nach Wiki
- **teilweise**: im Spiel, eine dokumentierte Eigenschaft fehlt (genannt)
- **geplant (V13.2)**: in dieser Runde zu bauen (Briefing `KREATUR_<Name>.md`); wird nach Abnahme auf „neu (V13.2)“ gesetzt
- **fehlt**: dokumentiert und sinnvoll umsetzbar, aber nicht im Spiel (Grund: Priorität, siehe unten)
- **bewusst weggelassen**: mit Grund
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
| Entity 99 (The Game Master) | https://backrooms-wiki.wikidot.com/entity-99 | 39 | 11.08.2026 08:28 |

## Je Ebene

### Level 0 – „The Lobby“

Die Seite nennt **keine bestätigten Wesen**.

| Wesen / Erscheinung | Wiki (eigene Worte) | Status im Spiel |
|---|---|---|
| Unbekannte dunkle Gestalten | Unbestätigte Berichte über dunkle Figuren, die einem folgen; das Gefühl, beobachtet zu werden. | **umgesetzt** als „Gestalt“: steht am Gangende, ändert sich nie, solange man hinsieht, und ist fort, sobald man wegsieht. Greift nie an (unbestätigt, daher harmlos). |
| Flüstern, vertraute Stimmen | Vielleicht Einbildung. | **umgesetzt** als Halluzination (`haunt.cpp`, Klang `hal_whisper_*`) |
| Kratzen hinter den Wänden | Folgt einem die Gänge entlang; man soll es ignorieren. | **umgesetzt** als Halluzination (`hal_scratch_*`) |
| Andere Wanderer (Menschen) | Menschen, die ebenfalls verirrt sind. | **bewusst weggelassen**: Das Spiel zeigt andere Wanderer nur über ihre Notizen (Einsamkeit als Grundton seit V7). Im Mehrspieler sind die Mitspieler die Wanderer. |

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
| Growler (Entity 130) | Wesen aus Kabeln; ein schwerer Gegenstand auf dem Kopf verwirrt sie. | **fehlt**: Priorität 8 |
| Nguithr'xurh (Entity 16) | Insektenartig an der Decke, bei kugelförmigen Netzen. | **fehlt**: Priorität 7 |
| Wrangler (Entity 75) | Riesig, schlangenartig, gräbt sich durch Wände, hypnotisiert; leise gehen, wenn die Wände beben. | **umgesetzt (V13.3)**: Beben, Durchzug durch Wand, Boden oder Decke, Sog des Anblicks, wer laut ist, wird geschnappt; selten Jungtiere – [KREATUR_Wrangler.md](KREATUR_Wrangler.md) |
| Staphylococcus liminalis (Entity 19), Plague Goblins (Entity 25) | Krankheitserreger. | **bewusst weggelassen**: keine Gestalt; das Spiel hat kein Krankheitssystem |
| Narrenfigur im Gemälde (Entity 99) | Ein Bild, das als Ausgang zu Level 389 dient. | **umgesetzt (V13.3)**: Gemälde am Boden vor den Hallentüren verschwinden unbeobachtet; ihr Bild mit Bildschirmgesicht (wechselt ungesehen), roten Kugeln und Fäden lädt ein und wirft „aus dem Spiel“ – anderswo in Level 1 (Interpretation, Level 389 fehlt) – [KREATUR_GameMaster.md](KREATUR_GameMaster.md) |
| Garden-Sektor (Verwandlung) | Wer etwa eine Stunde bleibt, wird zur Pflanze. | **umgesetzt (V13.3)**: Garden-Uhr, Zeichen, Ranken und Rinde auf der Hand, Blätter am Rand der Sicht, Tod durch Verwandlung, Erholung draußen; Gestalten aus Rinde – [KREATUR_L1_Erscheinungen.md](KREATUR_L1_Erscheinungen.md) |
| Dunkles, stilles Wesen an der Schlafraumtür (Erzählung) | Klatschende Schritte, Krallenspuren, zähe Flüssigkeit. | **umgesetzt (V13.3)**: Krallenrillen und schwarze Läufer neben einer Zimmertür, Abdrücke an der Schwelle, selten Schritte, Krallen und Atem hinter der Tür – [KREATUR_L1_Erscheinungen.md](KREATUR_L1_Erscheinungen.md) |
| Gruppen (M.E.G., B.N.T.G., Ariane Circle u. a.), schattenhafte Arbeiter im Overall | Menschliche Gruppen; Arbeiter aus einem Ouroboros-Sektor. | **bewusst weggelassen**: Menschen, keine Wesen; nur über Notizen angedeutet |

### Level 2 – „Pipe Dreams“

| Wesen | Wiki (eigene Worte) | Status |
|---|---|---|
| Windows (Entity 2) | Am Gangende; nicht nähern. | **fehlt (bewusst offen, E-031)**: die Rohrgänge haben keine Fenster; ein eigenes Fenster an Gangenden wäre ein neues Ausstattungsstück. Fensterwesen gibt es in Level 188. |
| Smiler (Entity 3) | Häufig, lieben Dunkelheit, schwärmten beim großen Stromausfall. | **umgesetzt** (an Stromausfälle gebunden) |
| Deathmoths (Entity 4) | Vom Licht angezogen, meist friedlich, seit dem Stromausfall kleiner. | **bewusst weggelassen**: Die Motten mit Nest sind auf Level 5 verdichtet (dort „zentraler Stock“); auf Level 2 beschreibt die Seite sie als selten und friedlich. |
| Clump (Entity 5) | Häufig und tödlich; lauern an Ecken und springen. | **umgesetzt** (Lager an Kreuzungen und in Schächten) |
| Jerry (Entity 7) | In einem Zimmer eingeschlossen; wen er oder sein Kult sieht, der entkommt nicht. | **bewusst weggelassen**: Die Gefahr geht von einem menschlichen Kult aus; das Spiel hat keine Kämpfe gegen Menschen. Jerry selbst beschreibt die Seite nicht. |
| Hound (Entity 8) | In Rudeln, werden gestört zu schnellen Jägern. | **teilweise**: Hound umgesetzt, aber einzeln; Rudel fehlen |
| Faceling (Entity 9) | Intelligent, in Gruppen, lassen einen in Ruhe, wenn man sie nicht reizt. | **neu (V13.2)**: Gruppen, reizbar (Lampe im Gesicht, Bedrängen, Anrempeln), dann leichte Schläge; die Gruppe macht mit (`KREATUR_Faceling.md`) |
| Skin-Stealer (Entity 10) | Haben die Ebene nach dem Stromausfall verlassen. | **bewusst weggelassen** (laut Seite nicht mehr da) |
| Wretch (Entity 15) | Auf fast jeder Ebene; griffen 2014 die Survivors an. | **neu (V13.2)**: gemischt träge und schnell, 30 % im zweiten Stadium (flehen, greifen nicht an) |
| Nguithr'xurh (Entity 16) | Jagen Insekten und Motten, selten Menschen. | **fehlt**: Priorität 7 |
| Death Rat (Entity 24) | Fast harmlos, fressen Flüssigkeiten aus Fässern und Essen. | **neu (V13.2), teilweise**: haarlose Ratten (Entity 24), stieben davon, beißen bedrängt (`KREATUR_DeathRat.md`); das Fressen aus Fässern fehlt |
| Cannibal Cuisine (Entity 36) | Unbewegliche, leuchtende Wesen an Sackgassen; Wegmarken im Stromausfall, nicht gefährlich. | **fehlt**: Priorität 11 |
| Wrangler (Entity 75) | Graben sich durch den Boden und überfallen aus den Gängen. | **umgesetzt (V13.3)**: vor allem Männchen, quer durch die Gänge, Kopf bricht aus Wand oder Boden; keine Jungtiere – [KREATUR_Wrangler.md](KREATUR_Wrangler.md) |
| Photoshop (Entity 104) | Keine Art; selten, vor allem in PC-Räumen. | **bewusst weggelassen**: Die Seite beschreibt weder Gestalt noch Verhalten auf dieser Ebene. |
| Growler (Entity 130) | Klein; verschmelzen in engen Gängen, bis sie stecken bleiben. | **fehlt**: Priorität 8 |
| Swindlebird (Entity 192) | Eine einzelne, unbestätigte Sichtung. | **bewusst weggelassen** (unbestätigt) |
| Macchina (Entity 189) | Seit etwa 1850 nicht mehr gesehen. | **bewusst weggelassen** (ausgestorben) |
| Gruppen (Followers of Jerry, Survivors, B.N.T.G. …) | Menschen. | **bewusst weggelassen** (Menschen) |

### Level 3 – „Electrical Station“

| Wesen | Wiki (eigene Worte) | Status |
|---|---|---|
| Hound (Entity 8) | Lauern allein, bellen, um gemeinsam zu jagen, quälen ihre Opfer. | **teilweise**: Lauern und Bellen umgesetzt, gemeinsames Jagen fehlt |
| Faceling (Entity 9) | Alle feindselig und bewaffnet (Steinwerkzeuge), „entwickelt“ mit Augen-, Nasen- und Mundansätzen. | **neu (V13.2)**: Variante „entwickelt“ mit Steinwerkzeug; kurzsichtig und schwerhörig (Entity 9) – bemerken einen erst nah |
| Wretch (Entity 15) | Selten, behalten den Körper, rennen und tragen Waffen, sprechen in Bruchstücken. | **neu (V13.2), teilweise**: schnelle Wretches (rennen in Stößen); Waffen und Sprache fehlen (kein Modell, keine Stimme) |
| Deathmoths (Entity 4) | Gefährliche Schwärme, die Mund, Nase, Ohren und Augen angreifen; Praetorianer überwiegen. | **bewusst weggelassen**: Die Motten sind auf Level 5 (Stock); eine zweite Mottenebene direkt davor würde die Begegnung entwerten. Der Praetorian (fast 1 m groß) ist nicht umgesetzt. |
| Skin-Stealer (Entity 10) | Tarnen sich als Menschen, sprechen einfaches Englisch, halten die Tarnung tagelang. | **neu (V13.2), teilweise**: getarnt; sprechen Silben nach (keine verständlichen Worte) |
| Smiler (Entity 3) | Verbergen Augen und Zähne hinter Lidern und Lippen; greifen ohne Anlass an. | **teilweise**: Umgesetzt mit Stromausfall; das Verbergen ist als langsames Öffnen der Augen angedeutet; „ohne Anlass“ ist nicht umgesetzt (Spielbarkeit: der Angriff kündigt sich an). V13.3: ein geworfener Leuchtstab lässt ihn hier nur kurz stutzen (Seite: der Trick wirkt dort kaum; `smiler_bait: hesitate`). |
| Clump (Entity 5) | Viel größer, geschickt, mit Waffen aus eigenen Knochen. | **teilweise**: Bündel umgesetzt, die große, bewaffnete Form nicht |
| Death Rat (Entity 24) | Wie Wasserschweine mit geschärften Zähnen, bauen Fallen, menschlich klug. | **neu (V13.2), teilweise**: Ratten in Hausrattengröße; Fallen, menschliche Klugheit und die Größe „wie Wasserschweine“ bewusst weggelassen (Begründung im Briefing) |
| „Purpurne“ tote Wesen in den Wänden | Unklare Erscheinung mit violetter Energie. | **fehlt**: Priorität 12 (Dekoration) |
| Schattenfigur hinter den Gittern, Stimmen in Paranoia-Gängen | Unbestätigt. | **umgesetzt** über den Paranoia-Gang der Karte und die Halluzinationen |
| Engelsstatue im Sanktuarium | Schreckt alle Wesen ab; einziger sicherer Ort. | **umgesetzt** (Sanktuarium mit Engel, bleibt hell). Neu in V13.2: Wesen meiden den Raum (siehe Briefings) |
| Red Knight (Entity 71), Jerry (Entity 7), Basen und Gruppen | Nur in Vorfallberichten oder Gruppen. | **bewusst weggelassen** (Erzählung, Menschen) |

### Level 5 – „Terror Hotel“

| Wesen | Wiki (eigene Worte) | Status |
|---|---|---|
| Deathmoth (Entity 4) | Sehr häufig, die größte Gefahr; fliegen aus dunklen Zimmern, die Weibchen bleiben in den Nestern; die Ebene gilt als einer der zentralen Stöcke. | **umgesetzt** |
| Nest der Motten | Ebenenseite: Nester überall auf der Ebene, ohne Beschreibung. Entity-4-Seite: Schichten aus verdautem Material, Pflanzenfasern und Harz, 60–100 cm dick, Säulen aus Resten, fleischige Wucherungen über Türen, Eier an der Auskleidung. | **umgesetzt**, in V13.2 neu entworfen (Entscheidung E-026) |
| Hound (Entity 8) | Kommt vor, ohne Verhalten. | **umgesetzt** (selten) |
| Skin-Stealer (Entity 10) | Kommt vor, ohne Verhalten. | **neu (V13.2)**: wahre Gestalt (Verhalten nach der Entity-Seite) |
| Death Rat (Entity 24) | Kommt vor, ohne Verhalten. | **neu (V13.2)**: Ratten mit dem Schrei-Ereignis (Entity 24: Kreischen, das Licht flackert); dass sie männliche Motten fressen, ist nur dieses Ereignis außer Sicht (das Spiel hat nur Weibchen) |
| Woodlins (Entity 45), Watchers (Entity 38), Ants (Entity 89), Growlers (Entity 130), Nguithr'xurh (Entity 16) | Nur genannt, ohne Verhalten auf der Ebenenseite. | **fehlt**: Priorität 13. Ohne Verhalten auf der Ebene wäre jedes Verhalten erfunden; die Entity-Seiten sind noch nicht ausgewertet. |
| Wrangler (Entity 75) | Eine einzelne Meldung aus dem Kesselraum. | **umgesetzt (V13.3)**: selten (~900 s), nur im Kesselgang/Kesselraum – [KREATUR_Wrangler.md](KREATUR_Wrangler.md) |
| Samantha (Entity 26) | Wandert manchmal durch die Haupthalle; wer sich nähert, soll gutes Fleisch mitbringen. | **fehlt**: Priorität 14. Ein einzelnes, benanntes Wesen mit Handel (Fleisch) – das Spiel hat keinen Tauschhandel. |
| Entity 18 (Stimme, die in ihr Büro bittet) | Erzählstimme der Seite. | **bewusst weggelassen** (Erzählung) |
| Unsichtbare Anwesenheit, Flüstern aus den Kesseln, Partygeräusche | Unbestätigt. | **umgesetzt** als Halluzinationen und Klang der Ebene |
| Gruppen (Housekeeping, Homely Hotel, The Originals, Lost Hall Society) | Menschen. | **bewusst weggelassen** (Menschen) |

### Level 5.3 – „Promethei Bibliotheca“

| Wesen | Wiki (eigene Worte) | Status |
|---|---|---|
| Library Mind | Bewusstsein der Bibliothek; nimmt Erinnerungen gegen Wissen, zeigt Wut durch fallende Bücher und einstürzende Regale. | **umgesetzt** als Erscheinung: Bücher fallen, Verstand sinkt langsam (keine Gestalt) |
| Entity 18 („Gentleman“, Beast of Level 5) | Feind der Bibliothek, erscheint in der Schlussgeschichte. | **bewusst weggelassen** (Erzählung, keine Begegnung auf der Ebene) |
| Ambassadors (Entity 141), Macchina (Entity 189) | Nur als Wissen in der Bibliothek genannt. | **bewusst weggelassen** (nicht anwesend) |
| Menschen und Gruppen | Erzählung. | **bewusst weggelassen** |

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
| Bewohner, feindselige Lokomotiv-Basen | Menschen. | **bewusst weggelassen** (Menschen; der bewohnte Wagen ist als Kulisse da) |

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
| 7 | Nguithr'xurh (Entity 16) | 1, 2, (5) | offen |
| 8 | Growler (Entity 130) | 1, 2, (5) | offen |
| 9 | Arms (entite-15-fr) | 1 | **umgesetzt (V13.3)**: [KREATUR_Arms.md](KREATUR_Arms.md) |
| 10 | Wrangler (Entity 75) | 1, 2, 5 (Kesselraum) | umgesetzt (V13.3) – [KREATUR_Wrangler.md](KREATUR_Wrangler.md) |
| 11–14 | Cannibal Cuisine, purpurne Wesen, Woodlins/Watchers/Ants, Samantha | je eine | offen |

Ab Rang 6 sind die Wesen in dieser Runde nicht gebaut. Der Grund ist der Aufwand je Wesen: Briefing, Blender-Modell, Rig, Animationen, Verhalten, Klang, Gleichgewicht, mindestens drei Kritikrunden sowie Server-KI und Tests im Mehrspieler. Die Reihenfolge steht hier, damit die nächste Runde dort weitermacht. Der jeweilige Stand ist in `REPORT_V13_2.md` vermerkt.
