# Kreatur: Ouroboros-Arbeiter (Backrooms Remodeling Co., Level 1 – Sektor Ouroboros)

Quellen (nur backrooms-wiki.wikidot.com, Lizenz jeweils CC BY-SA 3.0):
- [level-1](https://backrooms-wiki.wikidot.com/level-1), Revision 83, gelesen am 27.10.2026: Sektor Ouroboros und Abschnitt „Sonst auf der Seite genannt“ (die Seite verlinkt die Arbeiter auf die Remodeling Co).
- [backrooms-remodeling-co](https://backrooms-wiki.wikidot.com/backrooms-remodeling-co), Revision 29 (zuletzt bearbeitet 28.06.2025), gelesen am 01.11.2026.
- Zusammenfassung in eigenen Worten; kein Bild der Seiten verwendet, keine wörtlichen Zitate.

## Was die Seiten sagen

**Level 1 (Sektor Ouroboros):**
- Der kleinste bekannte Sektor, dauernd eine Baustelle. Schattenhafte Gestalten in Overalls reparieren dort seit mindestens Jahrzehnten, ohne dass ein Fortschritt zu sehen ist.
- Sie beachten Lebende nicht. Gefährlich sind ihre Maschinen: sie verletzen manchmal Menschen. Rat der Seite: meiden.
- Gefahrenliste der Seite: Unfälle durch die Maschinen der Bauarbeiter.

**Backrooms Remodeling Co:**
- Menschenähnlich, aber unterschiedlich menschlich – von ganz menschlich bis kaum noch zu erkennen. Gesichter fast ohne Züge, große weiße, runde Augen.
- Einheitliche Arbeitskleidung mit Barett und Metallabzeichen; das Metall zeigt den Rang (Kupfer ganz unten, dann Silber, Gold). Am Gürtel tragen sie Geräte, die die Wirklichkeit um sie festhalten.
- Einzelne Abweichungen: einer läuft auf sechs Beinen wie eine Spinne, einer geht auf den Knöcheln wie ein Gorilla; ein besonders kräftiger hat einen Forscher erschreckt.
- Friedlich, keine Waffen; ihre Umbauten aber enden oft in Katastrophen und Toten. Einer klagt, dass die Umbauten nie wirklich gelingen.
- Wird einer von ihnen verletzt oder getötet, bemerken die anderen es nicht, wie in Trance; wer sie darauf hinweist, wird feindselig hinausgedrängt.
- Sprache nach Rang: die Untersten nur kurze Sätze, manche nur kehlige Laute oder Grunzen.

## Umsetzung

**Modell** (`tools/blender/creatures/ouro_parts.py` mit `ouroboros.py`, `ouroboros_heavy.py`, `ouroboros_wall.py`, `ouroboros_scaffold.py`; ein Skelett `anim::ob`, vier Varianten, `data/models/ent_ouroboros*_l0..l2.glb`):
- *Geselle* (Variante 0, 2,05 m): ein Schatten (Material schluckt Licht) in einem Overall aus Segeltuch mit Latz, Trägern über Kreuz, flachen Knieschonern mit Zementstaub, Gummistiefeln mit Profil; Barett mit Kupferabzeichen (unterster Rang) und drei kleine Anhänger am Gürtel (die Geräte der Seite). Eigene Anatomie: überlange, ausgezehrte Arme mit einem **zweiten Ellbogen**, der falsch herum knickt (Falte und Höcker), lange gekrümmte Finger; langer, vorgeneigter Hals, der Kopf ein kleines, längliches Ei mit hohlem Gesicht, das Kinn hängt und zieht einen Faden; zwei große, blasse, ungleich große Scheibenaugen in Mulden.
- *Brecher* (Variante 1): massig (× 1,38), ein Buckel zwischen den Schultern, der Kopf sitzt tief davor; geht auf den Knöcheln (Seite: der Gorilla).
- *Mauer* (Variante 2): sieben Lagen Hochlochziegel (36,5 × 24 × 24,9 cm, Lochbild, Nut und Feder), sechs lose Steine der Lage im Bau, vier Brocken, Palette mit Steinen, Mörtelkübel, Absperrband mit Warnleuchte am Boden.
- *Fahrgerüst* (Variante 3): 3,4 m, Rohre, Bretter, Rollen, Warnleuchte; kippt um die Kante.
- Werkzeuge als Knochen in der Hand (Hammer, Kelle, Stein, Brenner mit Schlauch, Presslufthammer mit Meißel und Luftschlauch, Brechstange), Rauchfahnen, Funken.
- Materialien `FM_ENT_OB_*` (`world_renderer.cpp`): Schatten und Overall metallisch ohne Streulicht (im Lampenlicht bleiben sie Schatten, nur Falten und Nähte schimmern), Rauchrand `kMatOuroSmoke` (der Rand zerfasert, in kurzen Stößen fallen waagrechte Bänder aus – die Gestalt flackert, je Figur zu eigener Zeit). Werkzeug, Gerüst und Mauer sind echt und fest – die Gefahr.

**Bewegung** (`src/anim/creature_ouroboros.cpp`):
- *Ruhe*: steht gebeugt, der Kopf hängt vor die Füße, kaum merkliches Wiegen.
- *Gehen/Tragen*: schwere Schritte, der Geselle mit pendelnden langen Armen, der Brecher auf den Knöcheln; Steine werden mit beiden Händen vor dem Bauch getragen.
- *Arbeit* (in Schleife, je Aufgabe): Stein heben und setzen, mit der Kelle nachklopfen; hämmern; knien und schweißen (Funken springen über den Boden); Brechstange ansetzen und ziehen; Presslufthammer (Stöße schütteln den ganzen Körper); Gerüst aufrichten; die fertige Mauer begutachten (alle halten inne).
- *Wahrnehmen*: keines – der Kopf folgt nur der Arbeit, nie dem Spieler. Im Lampenlicht steigt Rauch von Schultern, Scheitel und Rücken auf und kräuselt nach hinten weg; der Kopf ruckt kaum merklich (wie ein Bild, dem Bilder fehlen).
- *Vergehen*: der Schatten sinkt in den Boden, an der Stelle steigt Rauch auf; *Erscheinen* umgekehrt.

**Verhalten** (`src/gameplay/presence_ouroboros.cpp`, Level-JSON `data/levels/level1.json`, `presence.ouroboros: { group: [5,5], rooms: ["ouroboros"] }`):
- *Baustelle*: nur im Sektor Ouroboros (Raumtyp `ouroboros`), Mauer und Gerüst auf freien Flächen in fester Reihenfolge (bei Host und Gast gleich), nie im Blick eines Spielers angelegt. Fünf Arbeiter: Maurer, Klopfer, Brecher, Schweißer, Träger.
- *Mauer* (die Ouroboros-Schleife): bauen (42 Steine) → begutachten (7 s Stille) → Warnung (2,8 s Piepen, Warnleuchte) → einreißen (Brechstange, die oberen Lagen kippen nach hinten) → Presslufthammer (Kompressor läuft 1,6 s an, dann je Lage 3,2 s) → aufräumen (9 s) → von vorn.
- *Gerüst*: steht 45–85 s (Schweißen in Stößen: Zünden 1 s als Ankündigung, Lichtbogen 5–9 s, Pause 3–6 s) → knirscht und wankt 2,8 s mit piepender Warnleuchte → kippt (Fallphysik um die Kante) → liegt 5 s → zwei Arbeiter richten es in 8 s auf.
- *Der Spieler geht sie nichts an*: kein Ausweichen, kein Blick; wer im Weg steht, wird beiseitegeschoben (Schub 1,6 m/s seitlich, 0,7 m/s vorwärts, nur gegen die Wand nicht), mit kaltem Luftzug und einer dumpfen Stimme dicht am Ohr. Licht und Lärm ändern nichts an ihrem Tun.
- *Unfall*: erschlägt das Gerüst einen Arbeiter, versinkt er; die anderen bemerken es nicht und arbeiten weiter (Remodeling Co); nach 20–30 s tritt ein neuer aus dem Dunkel des Sektors (nicht im Blick).
- *Gefahr nur durch Maschinen, nur im abgesperrten Feld, jede angekündigt*: Gerüst (Fallstreifen 3,8 × 1,9 m), einreißende Lagen (Streifen hinter der Mauer), Presslufthammer-Splitter und Funkenflug (Absperrband). Außerhalb des Bands nie Schaden.

**Klang** (`src/audio/synth_ouroboros.cpp`, Zuordnung `app.cpp`): `ouro_step_0..2` (Gummistiefel, Splitt), `ouro_tap_*` (Hammer auf Ziegel), `ouro_trowel_*`, `ouro_set_*` (Stein ins Mörtelbett), `ouro_pry`, `ouro_break_*`, `ouro_ignite`, `ouro_weld_*` (Brummen, Knistern), `ouro_jack_*` (22-Hz-Stöße), `ouro_warn_0..2` (drei Pieptöne / Kompressor läuft an / Warnleuchte des Gerüsts), `ouro_creak_*`, `ouro_crash_0/1`, `ouro_murmur_0..5` (dumpfe Stimmen ohne Worte, wie durch Stoff – der Tonfall kurzer Sätze), `ouro_shove`, `ouro_dissolve`. Stimmen alle 7–16 s von einem zufälligen Arbeiter.

**Mehrspieler:** die KI läuft beim Host, die Gefahren treffen jeden Spieler (auch Gäste) nach seiner Kennung; den Schub rechnet jeder Rechner für seinen eigenen Spieler (`shoveFor`). Die Puppen beim Gast bekommen Rolle 16, Variante, Lage und die vorhandenen Kanäle: Arbeiter – Aufgabe (`state`), Phase (`strike`), Ziel (`tilt` = Höhe/2,5 m, `lean` = seitlich), setzen/heben (`alert`), Lichtbogen bzw. Beben (`shiver`), Vergehen (`fly`), Leuchten (`glow`); Mauer – `reach` (gesetzt), `growl` (abgetragen), `jaw` (Kippen), `alert` (Brocken), `shiver` (Hammer läuft), `glow`; Gerüst – `tilt`, `shiver`, `glow`. Protokoll unverändert.

**Interpretation** (die Seiten sagen es nicht): die Mauer, die gebaut und gleich wieder eingerissen wird (Bild für „seit Jahrzehnten ohne Fortschritt“); das kippende Gerüst, Funkenflug und Presslufthammer als die „Maschinen“; Absperrband und Ankündigungen; Schub statt Ausweichen; der zweite Ellbogen und der hängende Kopf (die Seite nennt nur, dass manche kaum menschlich sind); Rauch, Flackern und Versinken (das „Schattenhafte“ der Level-1-Seite); dass ein Arbeiter vom eigenen Gerüst erschlagen werden kann; alle Zahlen.

**Nicht umgesetzt:** der spinnenartige Sechsbeiner und „Toaster“ der Remodeling-Co-Seite (gehören zur Firma, nicht zum Sektor Ouroboros); feindseliges Hinausdrängen, wenn man sie auf einen Toten hinweist (das Spiel hat keine Gespräche); Sprache mit Worten.

## Gleichgewicht

| Wert | Zahl |
|---|---|
| Auftritt | Level 1, nur Sektor Ouroboros (150–184 × 99–122); immer da, sobald der Sektor geladen ist; 5 Arbeiter |
| Sinne | keine – sie nehmen Lebende nicht wahr |
| Tempo | Geselle 1,05 m/s, Brecher 0,8 m/s (gehen 3 m/s) |
| Schaden | Gerüst 30 (nach 2,8 s Knirschen und Piepen); einreißende Lage 18 (nach 2,8 s Warnung); Presslufthammer 3 je 0,35 s (nach 1,6 s Kompressor); Funken 2 je 0,5 s (nach 1 s Zünden, nur am Brenner im Band) |
| Ausweg | außerhalb des Absperrbands bleiben; bei Piepen oder Knirschen aus dem Band treten |

## Wow-Check

„Pechschwarze, ausgezehrte Gestalten im Overall, mit großen blassen Scheibenaugen und Armen, die ein zweites Mal falsch herum knicken, mauern im Halbdunkel eine Wand und reißen sie wieder ein – sie gehen durch einen hindurch, als wäre man Luft, und wenn das Gerüst einen von ihnen erschlägt, versickert er im Boden, und die anderen arbeiten weiter.“

Ehrliche Antwort je Runde: Runde 1–2 „albern“ (Micky-Maus-Ohren am Brecher, Maiskolben-Presslufthammer, im Lampenlicht weiße Schaufensterpuppen) – deshalb Material, Arme und Haltung neu. Runde 3–4 „ganz okay“ (glatte Puppe, Rauch wie Drähte) – Arme, Augen, Rauch neu gebaut. Ab Runde 5/7: ja – im Halbdunkel lesen sie sich als Schatten, die arbeiten; der Gleichmut (kein Blick, Schub, der erschlagene Kollege versinkt unbeachtet) und die Arme machen das Grauen, nicht ein Angriff. Der Brecher ist von vorn bei der Arbeit eine dunkle Masse; auf den Knöcheln gehend wirkt er (`r7_brecher_knoechelgang.jpg`).

## Kritikrunden

Bilder: `docs/screens/creatures/ouroboros_arbeiter/` (je Runde ein Bogen `rN_bogen.jpg`: nah, mittel, fern, Lampe im Dunkeln, Schweißen, Gerüst kippt, Presslufthammer, Aufräumen, Vorführungen; ab Runde 4 Licht/Rauch und Vergehen).

| Runde | Was ist schlecht | Was ändert sich | Bilder |
|---|---|---|---|
| 1 | Brecher beim Presslufthammer mit zwei runden schwarzen „Ohren“ über dem Kopf (die Ellbogen stehen hoch – der Pol hing am Brustkorb, beim Bücken zeigte er nach oben); Presslufthammer gelb mit Rippen wie ein Maiskolben; Knieschoner leuchten wie Augen; im Lampenlicht werden die Arbeiter zu hellgrauen Schaufensterpuppen. Baustelle mit Band und Gerüst im Halbdunkel stark. | Ellbogen-Pol in Körperachsen (außen, hinten, unten); Gehäuse dunkler Guss; Staub dunkel; Overall dunkler. | `r1_bogen.jpg`, `r1_k_brecher_presslufthammer.jpg`, `r1_d_lampe_dunkel.jpg`, `r1_i_geselle_haemmert.jpg` |
| 2 | Im Lampenlicht weiter Schaufensterpuppen – auch mit Albedo 0,02 (`r2_lampe_stoff_grau.jpg`); der Brecher trägt jetzt einen schwarzen „Hut“ (Ellbogen seitlich auf Schulterhöhe). Beim Suchen gefunden: die Kennung des Rauchrands lag in denselben Bits wie die Ebene – der Rauchrand griff nie. | Ebenenbits gelöscht (Rauchrand und Flackern wirken jetzt); Overall und Staub metallisch ohne Streulicht (`r2_lampe_metallisch.jpg`: Schatten, nur Nähte schimmern); Presslufthammer: Ellbogen nach hinten unten, der Zickzack nimmt die Länge auf. | `r2_bogen.jpg`, `r2_d_lampe_dunkel.jpg`, `r2_k_brecher_presslufthammer.jpg`, `r2_lampe_stoff_grau.jpg`, `r2_lampe_metallisch.jpg` |
| 3 | Im Lampenlicht bleiben sie Schatten (`r3_d_lampe_dunkel`); der Brecher gebückt und kompakt. Aber: die Ziegel sehen aus wie graue Lederkissen; von hinten ist der Geselle eine glatte Puppe mit Röhrenarmen, die Augen klein. | Ziegel röter; Unterarme dünner, das zweite Gelenk mit deutlichem Höcker, große runde Augen (Remodeling Co) – Modelle neu. | `r3_bogen.jpg`, `r3_d_lampe_dunkel.jpg`, `r3_k_brecher_presslufthammer.jpg`, `r3_a_nah.jpg`, `r3_f_geruest_kippt.jpg` |
| 4 | Augen lesen sich, der Zickzack-Arm auch. Der Rauch im Licht sind 6,5 mm dünne Fäden – wie Drähte (`r4_m_licht_rauch`); beim Vergehen bleiben Halme wie dürres Gras (`r4_n_vergehen`). | Je Fahne eine dicke Fahne (2,6 cm) und zwei Schleier; der Rauch wächst beim Vergehen stärker. | `r4_bogen.jpg`, `r4_m_licht_rauch.jpg`, `r4_n_vergehen.jpg`, `r4_i_geselle_haemmert.jpg` |
| 5 | Rauch sichtbar, steigt aber steif senkrecht wie Hörner; Vergehen als Rauchsäulen besser; Ziegel im Lampenlicht weiter Kissen (`r5_d_lampe_dunkel`). | Fahnen kräuseln nach hinten weg, mehr Drift; Ziegel auf der Zahn-Ebene statt Horn (die Rillen machten das Leder). | `r5_bogen.jpg`, `r5_m_licht_rauch.jpg`, `r5_n_vergehen.jpg`, `r5_d_lampe_dunkel.jpg` |
| 6 | Ziegel jetzt orange, porig, kantig (`r6_d_lampe_dunkel`, `r6_g_presslufthammer`). Aber: ein vom Gerüst erschlagener Arbeiter zerfällt Glied für Glied in schwebende Blöcke (Kopf, Schulterstange, Rumpf) – wie eine kaputte Puppe (`r6_g_presslufthammer`); dabei tauchten alle sechs Werkzeuge auf. Fern (`r6_c_fern`) und nah (`r6_a_nah`) liest sich die Baustelle gut, Schweißen mit kaltem Lichtbogen (`r6_e_schweissen`). | Vergehen = der Schatten sinkt in den Boden, der Rauch steigt an der Stelle auf (Erscheinen umgekehrt); Verblassen nur noch verkleinernd (verborgene Werkzeuge bleiben verborgen). | `r6_bogen.jpg`, `r6_d_lampe_dunkel.jpg`, `r6_g_presslufthammer.jpg`, `r6_a_nah.jpg`, `r6_c_fern.jpg`, `r6_e_schweissen.jpg`, `r6_l_gehen.jpg` |
| 7 | Versinken liest sich als Schatten, der in den Boden abläuft, die Arme greifen noch nach oben, zuletzt nur Rauch (`r7_vergehen_versinkt.jpg`). Brecher auf den Knöcheln: gebückter Koloss mit hängendem Kopf und langem Arm – wirkt; bei der Arbeit im Licht eine schwarze Masse mit Rauchgeweih (`r7_brecher_knoechelgang.jpg`). Abgenommen. | – | `r7_vergehen_versinkt.jpg`, `r7_brecher_knoechelgang.jpg` |

## Tests (`gameplay_test`, `testV133Ouroboros`)

Level 1: die Baustelle entsteht außer Sicht, Mauer und Gerüst im Sektor Ouroboros, fünf Arbeiter im Sektor (Raumrechteck), sieben Wesen; mit Lampe 1,3 m neben dem Klopfer sieht er nie zum Spieler, sein Blick trifft die Mauer (≥ 90 % der Bilder), er hämmert weiter, die Mauer wächst, kein Schaden durch die Arbeiter; der Träger weicht nicht aus und schiebt den Spieler (> 0,15 m), ohne Schaden; das Gerüst knirscht ≥ 2,5 s vor dem Aufschlag, die Warnleuchte piept, neben dem Band kein Schaden, im Fallstreifen Treffer genau beim Aufschlag und erst nach der Ankündigung; Schweißen: neben dem Band treffen die Funken nicht, am Brenner erst ≥ 0,9 s nach dem Zünden und nur wenig; Mauer: Einreißen mit Brechstange und Aufschlag, der Kompressor läuft ≥ 1,5 s vor dem ersten Stoß, außerhalb des Bands kein Schaden, innen trifft der Presslufthammer erst, wenn er läuft. Level 2: keine Arbeiter, auch erzwungen nicht.

## Restmängel

- Der Brecher ist von vorn bei der Arbeit eine dunkle Masse ohne lesbares Gesicht (die Augen liegen unter dem Buckel).
- Unter der Taschenlampe werden nichtmetallische Wesenstoffe allgemein sehr hell (gefunden am Overall, Ursache in der Beleuchtung nicht geklärt); hier mit metallischem Schatten umgangen.
- Die Ziegel wirken aus der Nähe etwas schwammig porig; die Mauer läuft nur entlang der Achsen (vier Richtungen).
- Das Flackern (waagrechte Bänder) sieht man nur in Bewegung, nicht auf Standbildern.
