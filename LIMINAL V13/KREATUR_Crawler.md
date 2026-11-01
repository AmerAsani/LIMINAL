# Kreatur: Crawler (Entity 17) – der Pilz

Quellen (Wikidot, CC BY-SA 3.0, eigene Worte, kein Bild der Seiten):
- [entity-17](https://backrooms-wiki.wikidot.com/entity-17) „Crawlers“, Rev. 40 (01.06.2025); Autor unbekannt, Neufassung Uberspective; als „veraltet“ markiert.
- [level-1](https://backrooms-wiki.wikidot.com/level-1) Rev. 83 (07.10.2026, Praetor3005 und DivineAtlas), Wesenliste.
- Gelesen am 27.10.2026 (Rohtext `scratchpad/v13/wiki/raw4/entity-17.txt`, Zusammenfassung `wiki/level_1.md` Abschnitt 6).

## Was die Seiten sagen

- Crawler heißen alle, die ein bestimmter, leicht anomaler Pilz befallen hat. Er wächst sehr aggressiv und liebt warme, dunkle, feuchte Orte.
- Auf Menschen geht er **nur über Flüssigkeiten** über (Blut, Speichel, Urin, Schweiß; Wunden abdecken). Kleine Tiere und Insekten stecken sich durch Berührung an und tragen ihn weiter.
- Erkennbar an einem **leuchtend weißen Belag**, ähnlich einem Kernkeulen-Pilz, der Kellerspinnen überzieht; er will den ganzen Körper einhüllen.
- **Zeitleiste beim Menschen** (zusammen etwa 12 Minuten): Stadium 1 (3 min) Juckreiz; Stadium 2 (1 min) kleine, flaumige weiße Flecken, viel Speichel, großer Hunger; Stadium 3 (30 s) der ganze Körper wird überzogen, Fieber; Stadium 4 (7 min 30 s) Aggression, der Pilz wächst ins Gehirn, Augen blutunterlaufen, Haare und Nägel wachsen rasend; Stadium 5 (unbegrenzt) die Befallenen trennen die untere Körperhälfte ab und kriechen auf den Armen, der Pilz steuert den Körper.
- **Kein bekanntes Gegenmittel.** Rat: Befallene sofort isolieren, misstrauisch gegenüber Tieren und Menschen sein.
- Level 1: keine unversiegelten Flüssigkeiten trinken oder berühren, Infizierte meiden.

## Umsetzung (Level 1)

Kein neues Wesen mit eigener Rolle (Auftrag: keine neue `Presence::Role`) – der Pilz ist eine **Gefahr der Umgebung** mit Ansteckung und Krankheitsverlauf (`gameplay/phenomena.*`, `gameplay/session_phenomena.cpp`).

- **Befall an feuchten Stellen** (Karte, `tools/levels/level1.py`, nur Platzierungen mit eigenem Zufall): drei Stellen in den Aquila-Hallen an der Wand unter den tropfenden Rohren – Myzel an der Wand (`fungus_wall`), befallene Pfütze davor (`fungus_floor` über einer Pfütze der Karte mit Spiegelung); ein verpilzter Rattenkadaver unter einem Krankenbett (`fungus_floor@2`); das **Lager eines Infizierten** in einem Zimmer der Gänge (`fungus_camp`, `fungus_wall@2`).
- **Modelle** (`tools/blender/models/models_l1_phenomena.py`): wattige Polster aus vielen weichen, überlappenden Kügelchen (oben heller), Myzelfäden, die vom Rand ins Umland ausfransen, Fruchtkörper mit keulenförmigem, an der Spitze ockerfarbenem Kopf, **überzogene Kellerspinnen** an der Wand (Leib und lange Beine unter Flaum, an den Gelenken dicker), Flocken auf dem Wasser. Das Lager: ein Schlafsack, aus dem **nur noch zwei Beine** in Hose und Stiefeln ragen; wo der Rumpf sein müsste, ein Polster aus Pilz und Keulen; eine Schleifspur mit **Handabdrücken** führt vom Lager weg (Stadium 5 der Seite); offene Mandelwasserflaschen, ein Deckel, Zettel, Kerzenstummel; an der Wand Kratzer durch den Belag und eine Strichliste.
- **Ansteckung**: im Befall stehen oder waten – nach 4 s Kontakt (Waten in der Pfütze zählt voll, Streifen an der Wand halb; außerhalb geht der Zähler mit 0,5/s zurück). Der erste Kontakt warnt („Weißer Flaum auf Wand und Wasser …“). **Offenes Mandelwasser** (`almond_open`, neues Item: 2 Flaschen an den Befallsorten) gibt +20 % Verstand und steckt sofort an.
- **Verlauf** nach der Zeitleiste des Wikis (180/60/30/450 s; Tod nach 12 min, eigene Ursache `pilz`, Todesschirm „DER PILZ HAT DICH“): Stadium 1 Kratzen (Klang), Stadium 2 weiße Flecken auf der eigenen Hand (Viewmodel, Shader `skinMarks`), Zucken des Blicks; Stadium 3 die Hand ist überzogen, Fieberpuls, langsamer; Stadium 4 der Rand der Sicht wächst wattig-weiß zu, feine Fäden kriechen zur Mitte, Doppelbild, dumpfer Klang, Flüstern; der Verstand schwindet zusätzlich.
- **Gegenmittel** (Interpretation): versiegeltes Mandelwasser heilt Stadium 1–2 und hält in Stadium 3–4 den Pilz 60 s an.
- **Infizierte meiden**: als Spur umgesetzt – das Lager mit Zettel („Hab aus der offenen Flasche getrunken …“, einmal je Lauf) und die Schleifspur; ein befallener Wanderer als Figur bräuchte eine neue Rolle (Wunsch, siehe unten).

## Interpretation

- Gegenmittel Mandelwasser (das Wiki kennt keines): sonst wäre jede Ansteckung ein sicherer Tod ohne Spielentscheidung. Früh heilt es, spät verschafft es Zeit.
- Ansteckung durch Waten/Stehen im Befall: die Seite nennt nur Flüssigkeiten – die befallene Pfütze ist die Flüssigkeit.
- Stadium 5 als Tod des Spielers; die abgetrennten Beine im Lager zeigen, was die Seite beschreibt, ohne die Figur zu zeigen.

## Zahlen

| Wert | Größe |
|---|---|
| Kontakt bis zur Ansteckung | 4 s (Waten ×1, Wand ×0,5), Abbau 0,5/s |
| Stadien | 180 s / 60 s / 30 s / 450 s, danach Tod (`pilz`, Rolle −2) |
| Verstand zusätzlich | 0,02 / 0,06 / 0,12 / 0,20 %/s je Stadium (angehalten: halb) |
| Tempo | Stadium 3 ×0,93, Stadium 4 ×0,88 |
| Mandelwasser | heilt Stadium ≤ 2, sonst 60 s Pause |
| offenes Mandelwasser | +20 % Verstand, steckt an |
| Bild | Haut (Flecken/Belag) 0,38 → 1; Rand der Sicht ab Stadium 3, Doppelbild ab 0,6 |
| Klang dumpf | 0,35 in Stadium 4 |

## Klang (`audio/synth_phenomena.cpp`)
`itch_scratch` (Fingernägel, kurze Striche; alle 9–20 s geteilt durch das Stadium), `fever_pulse` (zwei schwere Herzschläge, Blutrauschen; Beginn Stadium 3), in Stadium 4 `hal_whisper` mit dem Zucken; der Zettel raschelt (`paper`).

## Gleichgewicht
Vorräte unverändert: `almond_open` steht in keiner Vorratstabelle (eigene Spawnpunkte), `supply_balance_test` grün. Befall nur an 5 Stellen, nie auf einem Weg, den man gehen muss; 4 s Kontakt verlangen Absicht oder Unachtsamkeit. Der Verlauf lässt 4 min Zeit für ein Heilmittel.

## Mehrspieler
Der Host rechnet Kontakt, Stadien und Tod für alle (Werte der Gäste über ihre Figuren); der Tod geht mit Rolle −2 über das bestehende Todesereignis (der Gast zeigt „DER PILZ HAT DICH“). Der Gast rechnet für sich nur Bild und Klang aus seiner Lage und seinem Trinken (Vorhersage, keine Entscheidung). Protokoll unverändert; `coop_test --quick` 169/0.

## Wow-Check
Unter dem tropfenden Rohr blüht etwas Weißes an der Wand – erst nah erkennt man die eingesponnenen Spinnen, und im Zimmer dahinter ragen nur noch zwei Beine aus dem Schlafsack, von denen Handabdrücke zur Tür kriechen. Ehrliche Antwort: unheimlich, nicht „ganz okay“ – das Lager trägt die Szene; die Wandpolster allein wirken aus der Ferne noch etwas wie Blüten (Restmangel).

## Kritikrunden
Bilder: `docs/screens/creatures/l1_erscheinungen/` (`r<N>_pilz_*`, `r<N>_flasche`).

| Runde | Befund | Änderung |
|---|---|---|
| 1 (`r1_pilz_wand_nah_lampe.jpg`, `r1_pilz_lager.jpg`, `r1_pilz_kadaver.jpg`) | Flaum mit Moos-Textur ist **grün**; der Feuchtfleck ein harter, flacher Ausschnitt; die Pfütze schwarz; Keulen dunkle Stäbchen; im Lager die Beine nicht lesbar. | Flaum ohne Moos (weiß), Fleck mit Betontextur, Wasser als Pfütze der Karte (Spiegelung), größere, helle Keulen. |
| 2 (`r2_pilz_wand_nah_lampe.jpg`, `r2_pilz_arm.jpg`) | Weiß, aber die Kegel wirken wie Papierfetzen; Fleck und Läufer weiter Ausschnitte (schwarze Balken); Kadaverhof ein schwarzes Loch; der Pilzrand der Sicht blockig gestreift. | Polster aus weichen Kügelchen (Watte), Wandfleck entfernt, kleiner heller Hof, Sichtrand neu: Watte aus Rauschen, Fäden radial (Polarkoordinaten). |
| 3 (`r3_pilz_wand_nah_lampe.jpg`, `r3_pilz_bild_stadium4.jpg`) | Kolonie liest sich als Pilz; Stadium 4: weißer Rand mit Fäden wirkt. Wand noch „Blütensträuße“, Hand mit orangen Flecken. | Ableger um jedes Polster, Hof der Flecken weniger orange, Handabdrücke an der Schleifspur. |
| 4 (`r4_pilz_wand_fern.jpg`, `r4_pilz_lager.jpg`) | Lager trägt: Beine in Hose, Pilz an der Taille, Handabdrücke. Fern ist der Befall klein. | Dichter Wulst an der Bodenkante, mehr Polster unten. |
| 5–6 (`r5_pilz_halle3_nah.jpg`, `r6_pilz_halle2_nah.jpg`, `r6_pilz_arm_stadium3.jpg`) | In einer Halle stand die Variante mit Strichliste (gehört ins Lager); Stadium 2 nur wenige Pünktchen. | Hallen nur Variante 0/1; Flecken in Stadium 2 etwas größer (0,38). |
| 7 (`r7h_pilz_hinweis.jpg`, `r7h_pilz_stadium2_meldung.jpg`, `r7h_pilz_tod.jpg`) | Mit HUD: Hinweis beim Betreten, Meldung Stadium 2, Todesschirm „DER PILZ HAT DICH“ mit Hinweis. | Abgenommen. |

## Tests (`gameplay_test`, „V13.3: Erscheinungen in Level 1“)
Orte aus der Karte (≥ 4 Befallsstellen, ≥ 2 Flaschen, 1 Lager); 2 s im Befall nur Hinweis, nach 9 s Stadium 1; Stadium 2 nach 3 min mit Haut und Bild; Mandelwasser heilt Stadium 2, hält Stadium 3 nur an (Zeit steht 5 s); Stadium 3 langsamer, Verstand sinkt; nach 12 min Tod (Ursache `pilz`, Rolle −2); Wiedereinstieg ohne Pilz; offenes Mandelwasser gibt Verstand und steckt an; Zettel im Lager.

## Restmängel und Wünsche
- Wunsch (spätere Rolle): ein **Crawler im Stadium 5** als Figur (kriechender Oberkörper, Krallen) – „Infizierte meiden“ wäre dann auch als Begegnung spielbar.
- Mitspieler sehen die Haut der anderen nicht verpilzt (kein Kanal ohne Protokolländerung).
- Wandpolster bis 31 000 Dreiecke je Stelle (nur fünf Stellen, im Chunk zusammengefasst); Bildrate nicht eigens gemessen.
