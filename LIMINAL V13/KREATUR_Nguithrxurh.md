# Kreatur: Nguithr'xurh (Entity 16)

Quelle: [backrooms-wiki.wikidot.com/entity-16](https://backrooms-wiki.wikidot.com/entity-16), Revision 39 (zuletzt bearbeitet 17.10.2025), gelesen am 27.10.2026. Die Seite trägt den Hinweis „veraltet“ (Tag to-rewrite).
- Ursprünglicher Autor unbekannt, überarbeitet von zakkun_.
- Lizenz CC BY-SA 3.0.
- Dazu die Ebenenseiten: Level 1 (Rev. 83), Level 2 (Rev. 93), Level 5 (Rev. 184), alle CC BY-SA 3.0.
- Zusammenfassung in eigenen Worten; kein Bild der Seite verwendet, keine wörtlichen Zitate.

## Was die Seite sagt

**Gestalt:** spinnenartig, 16 Gliedmaßen, wissenschaftlich als Walzenspinne (Solifugae) eingeordnet; die Größe schwankt stark – im Mittel etwa 10 cm breit und 18 cm lang (die Beschreibung nennt sie zugleich „groß“ und wegen der Größe gefährlich). Es gibt Jungtiere. Farbe, Behaarung, Augen: keine Angabe.

**Verhalten:**
- Versteckt sich an Decken. Baut kugelförmige Netze, füllt sie mit einem Betäubungsstoff und hängt sie mit zusätzlichen Fäden an die Decke – auch, wenn keine Beute da ist.
- Der Stoff lähmt nicht ganz, dämpft aber das Denken stark. Kleine Beute (Insekten): spritzt ihn direkt und frisst.
- Menschen: bleibt völlig reglos. Steht jemand genau unter den Fallen, schneidet sie zuerst eine einzelne Kugel ab, die platzt und den Stoff abgibt; die meisten merken sie kaum, fühlen sich aber seltsam. Mit wachsender Wirkung nimmt das Opfer immer weniger wahr; dann lässt sie alle Kugeln zugleich fallen, bis das Ziel auf nichts mehr reagiert, lässt sich herab und frisst es bei lebendigem Leib.
- Danach: an die nächste Decke, eine Ruhephase, dann neue Fallen. Langsam; jagt nicht hinterher – wer die Fallen meidet, ist praktisch sicher. Eine „Familie“ (mehrere Tiere) wird erwähnt; ein M.E.G.-Team lagerte darunter, zwei starben.
- Der Name: ein noch betäubter Explorer hat sich vertippt (eigentlich „Sleeper Spider“).
- Level 2: nur kleinere Tiere, die vor allem Insekten und Deathmoths fressen; verstecken sich an Deckenteilen hinter Maschinen; Menschen selten (ausgehungert oder kühn); seit dem Blackout zahlreicher (kalt, dunkel).

**Klang, Licht, Lärm:** die Seite sagt nichts.

**Ausweg (Seite):** Decken nach kugelförmigen Netzsäcken absuchen und meiden; auf jedes Unwohlsein achten; ab und zu den Körper schütteln; nicht lange unter einer Decke stehen bleiben; Level 1: nie unter den Netzen hindurchgehen, selbst wenn man umkehren muss.

**Ebenen:** Level 1 (häufig: insektenartig an Decken bei kugelförmigen Netzen), Level 2 (kleine, geringe Gefahr), Level 5 (nur genannt). Die Entity-Seite nennt dazu Level 3 – die Level-3-Seite nicht.

## Umsetzung

**Modell** (`tools/blender/creatures/nguithrxurh.py`, Skelett `ng` mit 51 Knochen; Neubau ab Runde 6, siehe Kritikrunden): eine Walzenspinne (Solifugae) – acht Laufbeine (das vordere Paar dünn und ohne Krallen, es tastet wie Fühler; das hinterste mit den fächerförmigen Malleoli unter der Hüfte), zwei lange, dicke, beinartige Pedipalpen mit blassem Haftende, zwei mächtige zweigliedrige Cheliceren (hoher, behaarter Grundteil; oben ein fester, unten ein beweglicher Finger, beide dunkel, seitlich flach, hakig gekrümmt, mit großen Zähnen; in Ruhe einen Spalt offen, im Biss klappt der untere Finger weit auf). Gewölbter Kopfschild mit kleinem dunklem Augenhügel und zwei kleinen dunklen Augen; weicher Hinterleib mit zehn Ringen und dunklen Querbändern, zur Spitze schmaler. Matt und sandig: Hinterleib und Beine hellbraun bis ocker, Kopf, Cheliceren und Taster rötlicher, die Endglieder der Taster dunkler; dunklere Gelenke (kurze Manschetten); dicht behaart mit rund 14 600 echten, dünnen, spitzen Haaren (lange, gebogene Borsten aus zwei Abschnitten, kurzer Flaum als dreiseitige Spitzen). Die Glieder sind durchgehende, sich verjüngende, leicht gebogene Röhren: am Ende jedes Glieds eine leichte Verdickung, am Gelenk ein schmaler Ring mit halbem Gewicht auf beiden Knochen (reißt beim Beugen nicht auf), dazu Gliederungen ohne eigenen Knochen (Patella/Tibia, Tarsenglieder). Beine, Taster, Gelenke und Haare sind fertige Netze je Stufe (`lod_mesh` in `build.py`) – beim Ausdünnen waren dünne Glieder und Borsten vorher zu flachen Dreiecken zerfallen. Stufen: 104 000 / 17 000 / 5 000 Dreiecke (die ferne Stufe ohne Haare). Materialien `FM_ENT_SOLIFUGE`, `_HEAD`, `_DARK`, `_JOINT`, `_HAIR`. Dazu der Faden aus der Hinterleibsspitze (nur beim Abseilen sichtbar). Größe je Ebene über den Maßstab: Level 1 2,0 (36 cm, „groß“), Level 5 1,5, Level 2 0,95 (die Seite: im Mittel 18 cm).

**Netzkugeln** (`nguithrxurh_web.py`, Variante 1; `nguithrxurh_web_moth.py`, Variante 2 für Level 2): sechs birnenförmige Kugeln (Radius 9–13 cm) aus matter, klumpiger Seide mit wenigen Rissen, durch die der milchige Stoff glänzt, umwickelt, mit abstehendem Flaum, unten heraushängenden Fäden und Tropfen (`FM_ENT_SILK`, `FM_ENT_SEDATIVE`); jede an einem Faden, der durch die Decke reicht (die Länge passt sich im Spiel der Deckenhöhe an: Unterkante knapp über Kopfhöhe). An der Decke Speichen, Ringe und Seidenlappen zwischen den Ankern, lose Fäden. Je Kugel eine Pfütze mit Spritzern und Seidenfetzen (verborgen, bis sie platzt). In Level 2 zwei eingesponnene Deathmoths im Geflecht.

**Bewegung** (`src/anim/creature_nguithrxurh.cpp`):
- *Ruhe* (an der Decke kopfüber, am Boden tief): der Hinterleib pumpt, die Pedipalpen tasten (leichtes Wiegen) und tippen ab und zu mit dem Haftende auf, die Tastbeine zittern und tippen, die unteren Finger zucken; „bauen“: läuft langsam ein Stück zu einer anderen Stelle am Netz (im Lampenlicht erstarrt sie auch dabei).
- *Wahrnehmen*: Pedipalpen (nach vorn oben) und Tastbeine heben sich (Drohhaltung der Walzenspinnen), der Körper richtet sich vorn auf.
- *Licht*: erstarrt und drückt sich flach an die Fläche; länger angeleuchtet zirpt sie und *huscht* (sehr schneller Wechselgang auf drei Beinpaaren in zwei Dreiergruppen, Taster und Tastbeine voraus) ins Dunkel.
- *Angriff*: seilt sich am Faden ab (Kopf voran, Beine angezogen, dreht sich langsam), kommt am Boden in Stößen, kaut (die Cheliceren sägen abwechselnd, die unteren Finger kauen), Biss: Stoß nach vorn, die unteren Finger klappen weit auf; der Zugriff springt ins Gesicht.
- *Rückzug*: zurück unter den Faden und hinauf an die Decke.
- *Netz*: die Kugeln pendeln kaum merklich und zittern, wenn eine Spinne darüberläuft; abgeschnitten fallen sie mit dem Faden, zerplatzen am Boden (0,1 s), die Pfütze breitet sich aus.

**Verhalten** (`src/gameplay/presence_nguithrxurh.cpp`, Level-JSON `presence.nguithrxurh`):
- Eine Kolonie (Netz und 2–4 Spinnen) an einer Decke von 2,45–3,9 m, 9–26 m Weg entfernt, nie im Blick; bevorzugt in Gängen (Level 1: man muss hindurch), in Level 2 über Maschinen.
- *Erste Kugel*: wer 0,7 s genau unter einer Kugel steht (0,85 m), über dem schneidet eine Spinne sie ab (leises Knipsen) – sie fällt und platzt (Dosis 0,33: man fühlt sich seltsam).
- *Alle Kugeln*: ist man nach 2,8 s noch da (1,9 m) oder benommen in der Nähe (2,6 m), fallen alle übrigen kurz nacheinander (je 0,24).
- *Fressen*: ab Betäubung 0,45 seilen sich die Spinnen nacheinander ab und kommen (0,75 m/s in Stößen) – nur bis 3,5 m vom Netz; erst wenn man auf nichts mehr reagiert (ab 0,8), frisst eine (tödlich, Zugriff ins Gesicht). Erholt man sich (unter 0,3) oder geht weg (über 4 m), kehren sie zurück.
- *Betäubung* (je Spieler): Tempo × (1 − 0,75 × Stärke), ab 0,5 kein Sprinten; Bild dunkler, weichgezeichnet, entsättigt, träge schwankend; Klang dumpf, Klingen in den Ohren. Sie hält 6 s an und klingt dann mit 0,028/s ab – in Bewegung schneller, jeder Sprung −0,05 („den Körper schütteln“).
- *Licht*: angeleuchtet erstarrt sie; nach 1,3 s zirpt sie und huscht 1–1,7 m weg vom Licht (danach 4 s Pause).
- *Hunger*: Level 1 immer, Level 5 70 %, Level 2 25 % – satt lassen sie Menschen in Ruhe.
- *Neue Kugeln*: alles gefallen und 45 s niemand näher als 6 m (ungesehen) – sie bauen neue.
- *Geräusch vor Sicht*: leises Zirpen der Cheliceren im Umkreis von 11 m alle 6–13 s.

**Klang** (`src/audio/synth_nguithrxurh.cpp`; die Seite sagt nichts – Walzenspinnen stridulieren mit den Cheliceren): `ngui_skitter_0..2` (viele winzige Krallen-Tipper, Borsten), `ngui_chitter_0..1` (Raspeln, leise und laut), `ngui_snip` (Knipsen, der Faden schnellt), `ngui_burst_0..1` (weiches, nasses Ploppen, Spritzen, Zischen), `ngui_daze` (Schleife: Klingen in den Ohren, gedehnter Atem), `ngui_feed`.

**Mehrspieler:** die KI läuft beim Host. Netz-Puppe: die Lage der Kugeln rechnet der Gast selbst aus der Mitte (dieselbe Formel), die Fallzeit je Kugel kommt in den Kanälen `jaw alert crouch growl reach fly`, das Zittern in `shiver`. Spinnen-Puppe: Lage an Decke oder Wand (`up`, `fwd`), Größe (`bait`), Abseilen (`hang`), Kauen (`jaw`), Biss (`reach`), Drohen (`perceive`), Licht (`light`). Platzt eine Kugel auf einem Gast, rechnet der Host die Wirkung, das Bild der Betäubung der Gast selbst (Ereignis `SacBurst` mit Ort).

**Interpretation** (die Seite sagt es nicht): Aussehen und Farben (nach echten Walzenspinnen), die Zahlen (Reichweiten, Dosis, Abklingen), die Lichtreaktion (die Seite nennt nur kalte, dunkle Orte für Jungtiere), die Laute, die Netze in Gängen, das Verhalten satter Spinnen, die eingesponnenen Motten. Die Zahl der Gliedmaßen: die Seite nennt 16, ordnet sie aber als Walzenspinne ein – umgesetzt ist der Körperbau echter Walzenspinnen (8 Laufbeine, 2 Taster, 2 Cheliceren; Vorgabe der Abnahme nach Runde 5).

**Nicht umgesetzt:** Jagd auf Insekten und Deathmoths als Handlung (nur als eingesponnene Motten im Netz); die Ruhephase nach dem Fressen (der Spieler ist dann tot); das Sekret als Arznei.

## Gleichgewicht

| Wert | Zahl |
|---|---|
| Auftritt | Level 1 ~170 s (3–4 Spinnen, groß, hungrig), Level 2 ~160 s (2–3, klein, 25 % hungrig), Level 5 ~260 s (2–3, 70 %) |
| Auslöser | 0,7 s genau unter einer Kugel (0,85 m); 2,8 s Bedenkzeit nach der ersten |
| Betäubung | erste Kugel 0,33, jede weitere 0,24; hält 6 s, dann −0,028/s (bewegt −0,048/s, Sprung −0,05) |
| Tödlich | erst ab 0,8 Betäubung und wenn eine Spinne an den Füßen ist (Abseilen etwa 4–5 s, Weg 1–3 m) |
| Ausweg | Netze meiden; nach der ersten Kugel sofort weg (sie folgen nicht über 3,5–4 m); Springen schüttelt den Stoff ab |

## Wow-Check

„Über dem Gang hängen schmutzig-graue, birnenförmige Seidensäcke; man bleibt stehen, es knipst leise, etwas platzt vor den Füßen – das Bild wird träge und dumpf, und dann lassen sich über einem zottige, ockerbraune Walzenspinnen an Fäden herab, die mächtigen Kieferklauen mit den schwarzen Haken voran.“

Ehrliche Antwort je Runde: Runde 1 „ganz okay“ bis „harmlos“ (Holzspinne, Christbaumkugeln) – neu gebaut. Runde 5 hatte ich abgenommen; der Koordinator nicht, zu Recht (siehe Runde 5): im Spiel waren die Beine getrennte, glänzende Kapseln und die Borsten flache Dreiecke. Neubau ab Runde 6: Runden 6–9 „nein“ (Teig, Würstchen, Stecken, Wattestäbchen), Runde 10 „fast“ (das Abseilen im betäubten Bild ist der Schreck, von vorn wirken die Finger noch wie glatte Stoßzähne), Runde 11 „ja“: zottig und matt, gegliederte Beine, von vorn behaarte Kieferklauen mit schwarzen Haken, am Faden ein haariger Schatten über dem Gesicht.

## Kritikrunden

Bilder: `docs/screens/creatures/nguithrxurh/`.

| Runde | Was ist schlecht | Was ändert sich | Bilder |
|---|---|---|---|
| Blender | Die Vorschau zeigt die Spinne kaum (der 3-m-Faden macht das Bild riesig); die Netzkugeln sind glatte Kugeln an Fäden. | Prüfung im Spiel. | `r0_blender.jpg` |
| 1 (Level 1) | Spinne zu klein (wirkt wie ein Insekt), Beine wie glatte Holzstäbe, einfarbig hell, Cheliceren unscheinbar. Netz: glänzende weiße Kugeln mit Löchern – Christbaumkugeln, Lampen; die Decke darüber leer. | Spinne in Level 1 Maßstab 2,0; Beine dick-dünn, dunkle Gelenkringe, mehr Borsten; größere Cheliceren; dunkle Rückenschilde. Netz neu: birnenförmig, matt, grauer, Flaum, Fäden, nur schmale Risse, Seidenlappen an der Decke. | `r1_spinne_bogen.jpg`, `r1_c_detail.jpg`, `r1_netz_bogen.jpg`, `r1_netz_lampe.jpg` |
| 2 | Größer und gefleckt, aber die Borsten sind unsichtbar (0,45 mm), die Beine noch glatt, die Augen weiße Comic-Punkte. | Borsten 0,8 mm und zahlreicher, dunkle Bänder über ein Drittel der Glieder, Augen schwarz glänzend. | `r2_spinne_bogen.jpg`, `r2_c_detail.jpg` |
| 3 (Netz im Spiel) | Die Säcke wirken jetzt wie Eikokons (`r3_b_motten`), im Lampenlicht noch etwas sauber. Kolonie im Spiel: erste Kugel, dann alle; das Bild wird betäubt dunkel und träge (`r3_g_alle`). | Netz belassen. | `r3_netz_kolonie_bogen.jpg`, `r3_b_motten.jpg`, `r3_d_lampe.jpg`, `r3_f_erste_kugel.jpg`, `r3_g_alle.jpg` |
| 4 | Borstig und gebändert – aber aus der Nähe ein „Gesicht“: zwei große schwarze Augen über hellen, glatten Backen (den Cheliceren), der Körper wie Plastik. | Augen klein auf dunklem Hügel, dunkler, beborsteter Rücken der Cheliceren, Haarrelief am Körper, Farbe dunkler und rotbrauner. | `r4_spinne_bogen.jpg`, `r4_c_detail.jpg` |
| 5 | Rotbraun, haarig, dunkle Scheren – ich hielt sie für eine echte Walzenspinne (`r5_c_detail`), das Abseilen im betäubten Bild für stark (`r5_abseilen_nah`). **Vom Koordinator abgelehnt:** die Beine sind glänzende, getrennte Kapseln mit Punkten, die an den Gelenken lose schweben; die Borsten flache Dreiecksblätter; das Gesicht eine Comic-Platte mit zwei Punktaugen; zwölf statt acht Beine. Ursache: beim Ausdünnen auf die Stufen zerfielen die dünnen Glieder und Borsten zu flachen Dreiecken; die glänzenden „Würstchen“ waren die dunklen Bänder aus dem Panzer-Material (Flecken, glänzend). | Neubau ab Runde 6. | `r5_spinne_bogen.jpg`, `r5_c_detail.jpg`, `r5_d_seite.jpg`, `r5_abseilen_fressen_bogen.jpg`, `r5_abseilen_nah.jpg` |
| 6 (Neubau) | 8 Laufbeine + 2 Taster, Beine als durchgehende Röhren (fertige Netze je Stufe, nicht ausgedünnt), matt: die Glieder hängen jetzt zusammen. Aber: einheitlich blass wie Mehl, die Haare dünne Dornen in großem Abstand, die Cheliceren von vorn zwei glatte Würstchen, die Augen zwei schwarze Perlen auf heller Platte (Comic), die Haftenden weiße Kugeln, Taster beim Drohen senkrecht. | Viermal so viele, feinere Haare; dunkle Gelenkmanschetten; ocker statt blass; dunkle Querbänder auf dem Hinterleib; Augenhügel dunkel, Augen kleiner; Cheliceren kürzer, höher, mit Buckel; Taster beim Drohen nach vorn oben. | `r6_figur_bogen.jpg` |
| 7 | Wärmer, Gelenke und Querbänder wirken, behaarter. Aber: die Finger von vorn zwei glatte Möhren (rund, ohne Zähne und Spalt), die Beine steife, gerade Stecken, die dunklen Manschetten wie Bambus. | Finger seitlich flach, kurz, stark gekrümmt, große Zähne; Knie höher (gebeugt); Glieder leicht gebogen; Gelenke in eigenem, helleren Braun. | `r7_figur_bogen.jpg` |
| 8 | Die Finger sind jetzt hakige Scheren (`r8_detail_biss` links), die Beine gebogen und lebendig. Aber: alles teigig hell, wie ein Latexspielzeug; Flaum zu dünn; die Taster enden in Wattestäbchen. | Zweifarbig: Kopf, Cheliceren und Taster rötlich-ocker, Hinterleib und Beine sandig, Endglieder der Taster dunkler; kurzer Flaum als dreiseitige Spitzen (drei Dreiecke) – so passen rund 14 600 Haare in die Nahstufe; Haftenden kleiner. | `r8_figur_bogen.jpg`, `r8_detail_biss.jpg` |
| 9 | Silhouette jetzt zottig (Hinterleib, Beine). Der Staub des Materials machte alles grau-beige; die Ansätze der Finger sahen von vorn aus wie Haut. | Weniger Staub (satteres Ocker); Finger ganz dunkel, in Ruhe einen Spalt offen; Borsten auf den Fingern; Haftenden sandfarben. | `r9_auswahl.jpg` |
| 10 (Kolonie, Licht, Ferne) | Netzkugeln mit den neuen Knochen in Ordnung; Kolonie im Spiel: erste Kugel, alle Kugeln, dann lassen sich die Spinnen als zottige Schatten an Fäden über dem betäubten Opfer herab – stark. Fern (7 m), mit und ohne Lampe, im Dunkeln mit Lampe: liest sich als haariges Tier. Schwäche: von vorn ganz nah wirken die Finger noch wie glatte Stoßzähne. | Kiel oben auf dem festen Finger, Zähne größer, außen eine Reihe kleiner Höcker, Zähne und Hakenspitzen fast schwarz, längere Borsten auf den Fingern. | `r10_figur_bogen.jpg`, `r10_kolonie_bogen.jpg` |
| 11 | Von vorn: behaarte Kieferklauen mit dunklen, gekrümmten Fingern und schwarzen Haken (`r11_gesicht`); von der Seite zottig, gegliederte Beine mit dunkleren Gelenken (`r11_seite`); Biss mit offenem unterem Finger, Kauen (`r11_biss`, `r11_kauen`); im Dunkeln mit Lampe (`r11_dunkel_lampe`); Abseilen über dem Gesicht (`r11_abseilen_nah`); Netze und Motten unverändert gut (`r11_kolonie_netz`). Abgenommen. | – | `r11_auswahl.jpg`, `r11_figur_bogen.jpg`, `r11_gesicht.jpg`, `r11_detail.jpg`, `r11_seite.jpg`, `r11_biss.jpg`, `r11_kauen.jpg`, `r11_dunkel_lampe.jpg`, `r11_fern_lampe.jpg`, `r11_abseilen_nah.jpg`, `r11_kolonie_netz.jpg`, `r11_blender.jpg` |

## Tests (`gameplay_test`, `testV133GrowlerNguithr`)

Level 1: Kolonie an der Decke mit Spinnen; genau darunter wird eine Kugel abgeschnitten und platzt über einem (Dosis 0,2–0,5); wer bleibt, über dem fallen alle, die Spinnen kommen, man wird gefressen; wer nach der ersten Kugel weggeht, ist sicher und die Betäubung klingt ab; im Lampenlicht zirpt eine Spinne und huscht weg; betäubt geht man langsam. Level 2: Netz mit eingesponnenen Deathmoths; satt lassen sie Menschen in Ruhe.

## Restmängel

- Die Finger der Cheliceren sind aus der Nähe noch recht glatt (die Zähne sieht man von der Seite, von vorn nur die Haken).
- Die Haare sind in der Nahstufe echte Strähnen (bis 6 m); in der mittleren Stufe bleiben 12 %, in der fernen keine – dort trägt nur die Pelz-Textur. Die Nahstufe hat 104 000 Dreiecke (davon 78 000 Haare), etwa so viel wie der Growler.
- Die Haftenden der Taster sind nur angedeutet (blasser Wulst), nicht ausstülpbar.
- Die Pfützen haben in der fernen Stufe eckige Ränder (flache Teile vereinfachen schlecht).
- Spinnen-Puppen beim Gast: das Abseilen wird über `hang` und die Position übertragen; die Drehung am Faden rechnet jeder selbst.
- Im Lampenlicht wirken die Säcke noch etwas sauber.
