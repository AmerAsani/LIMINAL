# LIMINAL V11 – Prüfliste je Ebene (Wiki-Abgleich)

Stand: 25.10.2026. Für jede der zehn Ebenen steht hier:
- die **Quellversion**: Seite und Revision im Backrooms-Wiki auf Wikidot, abgerufen am 25.10.2026;
- was davon **umgesetzt** ist, was **fehlt** und was **eigene Ergänzung** ist;
- wo LIMINAL bewusst **abweicht**, mit Grund.

Die Wiki-Inhalte sind in eigenen Worten zusammengefasst; im Spiel stehen keine Wiki-Texte oder -Namen (`LICENSE_REVIEW.md`, Weg B). Wo Fandom herangezogen wurde, steht das dabei. Die Fandom-Revisionen sind **nicht** festgehalten.

**Zeichen:** ✓ umgesetzt · ◐ teilweise · ✗ fehlt · ＋ eigene Ergänzung · ⚠ Abweichung (mit Grund)

**Wie geprüft wurde:**
- „Karte“: Entwurf in `tools/levels/levelN.py`, automatisch geprüft (Wände, Erreichbarkeit, Halt der Ausstattung), Draufsicht in `tools/levels/preview/`.
- „Test“: `gameplay_test` (Halt aller 2930 Objekte, Vorräte je Ebene, Reise durch alle Übergänge, Kreaturen).
- „Bild“: Aufnahme im Spiel (Skript), angesehen.

| Ebene | Wikidot-Seite | Revision | zuletzt geändert |
|---|---|---|---|
| 0 | Level 0 – „Threshold“ | 50 | 06.10.2026 |
| 1 | Level 1 – „Habitable Zone“ | 83 | 07.10.2026 |
| 2 | Level 2 – „Abandoned Utility Halls“ | 93 | 18.10.2026 |
| 3 | Level 3 – „Electrical Station“ | 18 | 26.09.2026 |
| 4 | Level 4 – „Abandoned Office“ | 110 | 29.05.2025 |
| 5 | Level 5 – „Terror Hotel“ | 184 | 07.08.2026 |
| 6 | Level 6 – „Lights Out“ | 65 | 09.08.2026 |
| 7 | Level 7 – „Thalassophobia“ | 48 | 10.03.2026 |
| 19 | Level 19 – „Attic Floorboards“ | 54 | 25.05.2026 |
| 37 | Level 37 – „Sublimity“ | 52 | 18.08.2026 |
| – | Licensing Guide | 15 | 29.05.2026 |

---

## Level 0 – „Die Vorzimmer“

| Merkmal laut Quelle | Stand | Umsetzung / Grund | Geprüft |
|---|---|---|---|
| Labyrinth aus Räumen, Fluren, Durchbrüchen wie Hinterzimmer von Büro oder Laden | ✓ | Gegliederte Räume (2,85–3,15 m hoch), Durchbrüche statt Türen, ein langer Flur (28–31, 6–128). 347 Räume, 548 Durchgänge | Karte, Bild |
| Gelbe Tapete, feuchter Teppich, brummende Leuchtstoffröhren | ✓ | Rasterleuchten mit Streuscheibe; jede Leuchte brummt einzeln | Bild |
| Keine Türen, keine Möbel | ✓ | keine Türblätter; Steckdosen und Lüftungsgitter nur an echten Wandflächen | Test (Halt) |
| Säulenfelder | ✓ | eigener Bereich | Bild |
| Bogenräume (blass, Bogendurchgänge, ruhigste Orte) | ✓ | blasse Zone, Bögen als eigenes Modell im Durchgang | Bild |
| Lochfelder (Gruben ohne Boden) | ✓ | Gruben alle 4 m, 14 m tief, schwarz. Wer fällt, kommt am letzten sicheren Ort wieder zu sich | Bild, Karte |
| Dunkelzonen (raue Wände, Stille, knöcheltiefe Flüssigkeit) | ✓ | Zone mit 12 cm Wasser, kaum Licht | Karte |
| Rote Räume (meiden) | ◐ | kosten Verstand. ⚠ Man kann sie verlassen (sonst gäbe es keinen fairen Ausweg) | Karte |
| Kleiner Raum in Aktendeckel-Tönen (Wiki: „Manila Room“) | ✓ | im Spiel „Vergilbtes Zimmer“ (ohne Wiki-Namen), erholt den Verstand ein wenig | Karte |
| Umgebung verändert sich, wenn man nicht hinsieht | ✗ | feste Karte (`WORLD_GENERATION.md`); nur die Belegung wechselt je Lauf | – |
| Flüstern, Kratzen in den Wänden | ✓ | seltene Klangereignisse | Klangtest |
| Keine bestätigten Entities, nur dunkle Gestalten | ✓ | nur die Gestalt (greift nie an) | Test |
| Ausgang über eine flackernde Wand | ◐ | Naht nach Level 1 am Ostende; das Flackern ist nur angedeutet | Test (Reise) |
| Vorräte | ＋ | 4–6 Funde (Mandelwasser, selten Batterie), an Wänden statt auf Möbeln, weil es keine Möbel gibt | Test (Vorräte) |

## Level 1 – „Das Lagerdeck“

| Merkmal laut Quelle | Stand | Umsetzung / Grund | Geprüft |
|---|---|---|---|
| Große Lager- und Garagenhallen aus Beton | ✓ | drei Parkdecks auf drei Höhen, zwei Lagerhallen, Hochregale | Bild |
| Gänge mit Büros und Nebenräumen | ✓ | Gang-Labyrinth mit Kammern, altes Büro (Fund auf dem Schreibtisch) | Bild |
| Ziegelräume, Krankenzimmer, gepolsterte Zellen | ✓ | eigene Raumtypen | Karte |
| Kisten mit Vorräten | ✓ | Kisten an festen Punkten, 8–10 Funde je Lauf | Test |
| Grüne Notausgangsschilder | ✓ | mit eigener Batterie, brennen im Stromausfall | Bild |
| Leuchten fallen aus, im Dunkeln Gefahr | ✓ | Stromausfall als Welle über die Stromkreise; danach das Lächeln | Test |
| Wasserpfützen, Kondenswasser | ✓ | Pfützen mit Tropfen | Bild |
| Bodenloch nach Level 19 | ＋ | eigene Verbindung (`LEVEL_SELECTION.md` Abschnitt 2) | Test (Reise) |
| Bewohnte Basen und Gruppen | ⚠ | nicht übernommen (Weg B, keine Gruppen oder Orte aus dem Wiki) | – |

## Level 2 – „Die Rohrgänge“

| Merkmal laut Quelle | Stand | Umsetzung / Grund | Geprüft |
|---|---|---|---|
| Enge Versorgungstunnel, Ziegel und Beton | ✓ | 3 m breite Gänge, Ziegelzonen nach Osten | Bild |
| Rohre an Wand und Decke, Maschinen | ✓ | Rohrbündel, Ventile, Manometer | Bild |
| Leuchtstoffröhren in Reihe: ein Feld geht gemeinsam aus | ✓ | jedes Feld hat einen eigenen Stromkreis | Test |
| Röhren weiß oder orange | ◐ | Röhrenleuchten (Zone „tube“); die Farbe wechselt nicht feldweise | Bild |
| Meist verschlossene Stahltüren, dahinter manchmal stilles Nichts | ✓ | Stahltüren als Wandausstattung (nur an echten Wänden), Räume „Stilles Dunkel“ | Test (Halt) |
| Heiße Abschnitte | ✓ | kosten Ausdauer | Karte |
| 45-Grad-Abzweige, Längen in 5-m-Schritten | ⚠ | Die Kachelwelt kennt nur rechte Winkel; Abschnitte sind gerade | – |
| Rollwagen, Fässer, Werkzeug, Klappstühle | ◐ | Fässer, Kisten, Regale; keine Klappstühle | Bild |
| Lagerkammer | ＋ | Stahlregal mit Fund auf dem Regalboden | Bild |

## Level 3 – „Das Umspannwerk“

| Merkmal laut Quelle | Stand | Umsetzung / Grund | Geprüft |
|---|---|---|---|
| Ziegelhallen in wechselnden Farben, ungleich breit | ✓ | rote, gelbe und graue Ziegelzonen | Bild |
| Schwache Röhren, manche hängen einzeln | ✓ | hängende Röhrenleuchten (Zone „hanging“) | Bild |
| Schalträume, Kabel, Schaltkästen, Funken | ✓ | Schalträume mit Sicherungskästen, Funken als Warnung | Bild |
| Rohre mit schwarzer Flüssigkeit | ◐ | Rohre vorhanden, Flüssigkeit nur im Klang | – |
| Fließbänder, Kesselräume | ✗ | nicht gebaut | – |
| Sanctums: wie Kathedralen, klassische Säulen, eine große Statue, Zeichen | ✓ | 5,6 m hohe Andachtsräume, vier kannelierte Säulen, Kronleuchter, Statue; Zeichen nicht lesbar ausgeführt | Bild |
| Unzerstörbare Gitter | ✓ | Transformatorhallen hinter Gittern | Bild |
| Stimmen in bestimmten Zonen | ✓ | „Raum der Stimmen“ (Halluzination, kostet Verstand) | Karte |
| Aufzug nach Level 4 | ✓ | braucht eine Sicherung | Test (Reise) |

## Level 4 – „Das stille Stockwerk“

| Merkmal laut Quelle | Stand | Umsetzung / Grund | Geprüft |
|---|---|---|---|
| Leeres Bürogeschoss, kaum Möbel | ✓ | Großraumbüros mit Möbelabdrücken im Teppich (Fandom) | Bild |
| Geschwärzte Fenster | ✓ | die meisten Fenster | Bild |
| Fenster mit Regen und Nebel (Fandom) | ✓ | einige Fenster; keines lässt sich öffnen | Bild |
| Nicht geschwärzte Fenster sind Fallen | ✗ | nicht umgesetzt (Ruheebene) | – |
| Wasserspender, reichlich Mandelwasser | ✓ | Teeküchen; 15–16 Funde je Lauf, die meisten im Spiel | Test (Vorräte) |
| Treppen führen ins selbe Stockwerk zurück (Fandom) | ✗ | die Treppe führt nach Level 5 | – |

## Level 5 – „Das Hotel“

| Merkmal laut Quelle | Stand | Umsetzung / Grund | Geprüft |
|---|---|---|---|
| Hotel der 1920/30er, Haupthalle mahagonirot und gold, Marmor, Walnuss | ✓ | Haupthalle mit Marmor, Teppich und Goldtönen | Bild |
| Haupthalle stark möbliert (Regale, Schränke, Sofas, Gemälde) | ✓ | Bücherregale, Schränke, Gemälde, Sitzgruppen mit Tischchen | Karte |
| Kronleuchter, Wandleuchter wie Kerzenhalter | ✓ | Kronleuchter, Wandleuchten, Deckenkugeln statt Rasterleuchten | Bild |
| Messingschilder mit Zimmernummern | ✓ | | Bild |
| Ballsaal mit einem unfertigen Spiel auf dem Tisch | ◐ | Spieltisch mit Karten. ⚠ Wiki: Mahjong; Karten sind eine eigene Auslegung | Karte |
| Jazz aus alten Lautsprechern, Partygeräusche hinter Wänden | ✓ | eigene Komposition, Murmeln hinter Wänden | Klangtest |
| Kesselraum: heiß, Dampf | ✓ | kostet Ausdauer, führt hinab | Karte |
| Motten aus dunklen Zimmern | ✓ | Nester in dunklen Zimmern | Test |
| Porträts, Möbel unverrückbar im Flur (Fandom) | ✓ | | Karte |
| Funde | ✓ | auf Nachttischen, in Regalen, auf Tischchen; keine auf dem Boden | Test |

## Level 6 – „Das Dunkel“

| Merkmal laut Quelle | Stand | Umsetzung / Grund | Geprüft |
|---|---|---|---|
| Enge Gänge, glatter kalter Beton | ✓ | | Karte |
| Völlige Dunkelheit, mitgebrachtes Licht funktioniert nicht | ✓ | die Lampe gibt hier kein Licht | Bild (schwarz, wie gewollt) |
| Still wie schalltot; Huschen, Atmen, Flüstern | ✓ | trockener Raumklang, Halluzinationen ohne Angriff | Klangtest |
| Treppe zu Level 7, zu finden über leises Wellenrauschen | ✓ | echte Klangquelle, jetzt mit Schallweg um Ecken (`AUDIO_DESIGN.md`) | Test (Schallausbreitung) |
| Tasten an Wänden | ＋ | Wände in Armlänge zeichnen sich schwach ab (Spielbarkeit) | – |

## Level 7 – „Das Meer unter der Decke“

| Merkmal laut Quelle | Stand | Umsetzung / Grund | Geprüft |
|---|---|---|---|
| Raum mit Bücherregal, Couchtisch, Stuhl, Röhre, nassem Teppich | ✓ | genau diese Ausstattung, Wasser pfützentief (4,53 m Spiegel) | Bild |
| Tür etwa 4,5 m über dem Meer, endloses Meer unter Betondecke | ✓ | | Bild |
| Kaltes Wasser | ✓ | kostet Gesundheit | Test |
| Großes lichtscheues Wesen in der Tiefe | ✓ | kommt nur bei langem Verweilen im tiefen Wasser; Licht vertreibt es kurz | Test |
| Was vor der Tür steht, wird hineingezogen | ✗ | nicht umgesetzt | – |
| Weiter nach Level 37 durch Tauchen | ＋ | eigene Verbindung | Test (Reise) |

## Level 19 – „Der Dachboden“ (Nebenweg)

| Merkmal laut Quelle | Stand | Umsetzung / Grund | Geprüft |
|---|---|---|---|
| Verwinkelter feuchter Dachboden, Dielen, Sparren | ✓ | gestufte Dachschrägen | Bild |
| Rosa Dämmwolle | ✓ | | Bild |
| Warmes, feuerartiges Glühen durch Dielen | ✓ | Glühen erholt den Verstand | Karte |
| Alte Möbel, Kisten mit Essbarem | ✓ | Truhen und Kisten; Funde auf der Truhe oder in Ecken | Test |
| Erinnerungen verändern sich, Übelkeit | ◐ | nur als Halluzination (Spieluhr) | – |
| Eingang über Löcher aus Level 1, beschädigte Tür nach Level 5 | ✓ | | Test (Reise) |

## Level 37 – „Die Becken“ (Ziel)

| Merkmal laut Quelle | Stand | Umsetzung / Grund | Geprüft |
|---|---|---|---|
| Weiße Fliesen, makellos | ✓ | | Bild |
| Großteils geflutet, lauwarmes Wasser, leichtes Kräuseln | ✓ | Gänge knöcheltief, tiefe Becken mit Treppen | Bild |
| Säulen im Wasser | ✓ | | Bild |
| Licht aus seltsamen Winkeln, manche Bereiche dunkel | ✓ | Licht teils von unten aus dem Wasser | Bild |
| Klang bricht ab, Hall verhält sich seltsam | ✓ | Hall mit hartem Abbruch | Klangtest |
| Keine Entities, das Wasser entspannt | ✓ | erholt den Verstand | Karte |
| Ende der Reise im Becken mit Licht von unten | ＋ | eigene Ergänzung | Test (Reise) |

---

## Bekannte Lücken (zusammengefasst)

- Level 0: Die Umgebung verändert sich nicht ungesehen (feste Karte). Die flackernde Wand ist nur angedeutet. Rote Räume sind nicht ausweglos.
- Level 2: Keine 45-Grad-Abzweige (Kachelwelt). Röhrenfarbe nicht feldweise.
- Level 3: keine Fließbänder und Kesselräume; die schwarze Flüssigkeit ist nur hörbar.
- Level 4: Fallen-Fenster und Endlostreppe nicht umgesetzt.
- Level 5: Karten statt Mahjong.
- Level 7: Der Sog vor der Tür fehlt.
- Level 19: Die Veränderung von Erinnerungen ist nur als Halluzination angedeutet.
