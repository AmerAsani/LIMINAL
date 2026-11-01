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

**Modell** (`tools/blender/creatures/nguithrxurh.py`, Skelett `ng` mit 59 Knochen): eine Walzenspinne mit 16 Gliedmaßen – zwölf Laufbeine (dick an der Hüfte, dünn auslaufend, dunkle Bänder und knotige Gelenke, abstehende Borsten), zwei dicke, beinartige Pedipalpen mit blassem Haftkissen, zwei massige Cheliceren (dunkler Rücken, glänzend dunkle Scheren mit Zahnreihen). Gewölbtes Prosoma mit dunklem Schild und Augenhügel (zwei kleine, schwarz glänzende Augen), weicher, gegliederter Hinterleib mit dunklen Rückenschilden; sandig rotbraun, dicht behaart (Material `FM_ENT_SOLIFUGE`, Pelz- und Faserebene). Dazu der Faden aus den Spinnwarzen (nur beim Abseilen sichtbar). Größe je Ebene über den Maßstab: Level 1 2,0 (36 cm, „groß“), Level 5 1,5, Level 2 0,95 (die Seite: im Mittel 18 cm).

**Netzkugeln** (`nguithrxurh_web.py`, Variante 1; `nguithrxurh_web_moth.py`, Variante 2 für Level 2): sechs birnenförmige Kugeln (Radius 9–13 cm) aus matter, klumpiger Seide mit wenigen Rissen, durch die der milchige Stoff glänzt, umwickelt, mit abstehendem Flaum, unten heraushängenden Fäden und Tropfen (`FM_ENT_SILK`, `FM_ENT_SEDATIVE`); jede an einem Faden, der durch die Decke reicht (die Länge passt sich im Spiel der Deckenhöhe an: Unterkante knapp über Kopfhöhe). An der Decke Speichen, Ringe und Seidenlappen zwischen den Ankern, lose Fäden. Je Kugel eine Pfütze mit Spritzern und Seidenfetzen (verborgen, bis sie platzt). In Level 2 zwei eingesponnene Deathmoths im Geflecht.

**Bewegung** (`src/anim/creature_nguithrxurh.cpp`):
- *Ruhe* (an der Decke, kopfüber): reglos, nur der Hinterleib pumpt; ab und zu tippt ein Pedipalpus, die Cheliceren putzen; „bauen“: läuft langsam ein Stück zu einer anderen Stelle am Netz.
- *Wahrnehmen*: Pedipalpen und vorderste Beine heben sich (Drohhaltung der Walzenspinnen), der Körper richtet sich vorn auf.
- *Licht*: erstarrt und drückt sich flach an die Fläche; länger angeleuchtet zirpt sie und *huscht* (sehr schneller Wechselgang zweier Gruppen zu sechs Beinen) ins Dunkel.
- *Angriff*: seilt sich am Faden ab (Kopf voran, Beine angezogen, dreht sich langsam), kommt am Boden in Stößen, kaut (Cheliceren sägen abwechselnd), Biss: Stoß nach vorn, Scheren weit auf; der Zugriff springt ins Gesicht.
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

**Interpretation** (die Seite sagt es nicht): Aussehen und Farben (nach echten Walzenspinnen), die Zahlen (Reichweiten, Dosis, Abklingen), die Lichtreaktion (die Seite nennt nur kalte, dunkle Orte für Jungtiere), die Laute, die Netze in Gängen, das Verhalten satter Spinnen, die eingesponnenen Motten.

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

„Über dem Gang hängen schmutzig-graue, birnenförmige Seidensäcke; man bleibt stehen, es knipst leise, etwas platzt vor den Füßen – das Bild wird träge und dumpf, und dann lässt sich eine handtellergroße, borstige Walzenspinne mit offenen Scheren kopfüber genau vor dem Gesicht herab.“

Ehrliche Antwort je Runde: Runde 1 „ganz okay“ bis „harmlos“ (Holzspinne, Christbaumkugeln) – neu gebaut. Ab Runde 4/5: ja – die rotbraune, borstige Spinne mit dunklen Scheren aus der Nähe und das Abseilen vor dem Gesicht im betäubten, dunklen Bild sind der Schreck; die Säcke wirken wie Eikokons.

## Kritikrunden

Bilder: `docs/screens/creatures/nguithrxurh/`.

| Runde | Was ist schlecht | Was ändert sich | Bilder |
|---|---|---|---|
| Blender | Die Vorschau zeigt die Spinne kaum (der 3-m-Faden macht das Bild riesig); die Netzkugeln sind glatte Kugeln an Fäden. | Prüfung im Spiel. | `r0_blender.jpg` |
| 1 (Level 1) | Spinne zu klein (wirkt wie ein Insekt), Beine wie glatte Holzstäbe, einfarbig hell, Cheliceren unscheinbar. Netz: glänzende weiße Kugeln mit Löchern – Christbaumkugeln, Lampen; die Decke darüber leer. | Spinne in Level 1 Maßstab 2,0; Beine dick-dünn, dunkle Gelenkringe, mehr Borsten; größere Cheliceren; dunkle Rückenschilde. Netz neu: birnenförmig, matt, grauer, Flaum, Fäden, nur schmale Risse, Seidenlappen an der Decke. | `r1_spinne_bogen.jpg`, `r1_c_detail.jpg`, `r1_netz_bogen.jpg`, `r1_netz_lampe.jpg` |
| 2 | Größer und gefleckt, aber die Borsten sind unsichtbar (0,45 mm), die Beine noch glatt, die Augen weiße Comic-Punkte. | Borsten 0,8 mm und zahlreicher, dunkle Bänder über ein Drittel der Glieder, Augen schwarz glänzend. | `r2_spinne_bogen.jpg`, `r2_c_detail.jpg` |
| 3 (Netz im Spiel) | Die Säcke wirken jetzt wie Eikokons (`r3_b_motten`), im Lampenlicht noch etwas sauber. Kolonie im Spiel: erste Kugel, dann alle; das Bild wird betäubt dunkel und träge (`r3_g_alle`). | Netz belassen. | `r3_netz_kolonie_bogen.jpg`, `r3_b_motten.jpg`, `r3_d_lampe.jpg`, `r3_f_erste_kugel.jpg`, `r3_g_alle.jpg` |
| 4 | Borstig und gebändert – aber aus der Nähe ein „Gesicht“: zwei große schwarze Augen über hellen, glatten Backen (den Cheliceren), der Körper wie Plastik. | Augen klein auf dunklem Hügel, dunkler, beborsteter Rücken der Cheliceren, Haarrelief am Körper, Farbe dunkler und rotbrauner. | `r4_spinne_bogen.jpg`, `r4_c_detail.jpg` |
| 5 | Rotbraun, haarig, dunkle Scheren – eine echte Walzenspinne (`r5_c_detail`). Abseilen im betäubten Bild: sie hängt kopfüber vor dem Gesicht (`r5_abseilen_nah`) – stark. Schwäche: die Pfützen wirken wie helles Papier. | Betäubungsstoff dunkler und nass. Abgenommen. | `r5_spinne_bogen.jpg`, `r5_c_detail.jpg`, `r5_d_seite.jpg`, `r5_abseilen_fressen_bogen.jpg`, `r5_abseilen_nah.jpg` |

## Tests (`gameplay_test`, `testV133GrowlerNguithr`)

Level 1: Kolonie an der Decke mit Spinnen; genau darunter wird eine Kugel abgeschnitten und platzt über einem (Dosis 0,2–0,5); wer bleibt, über dem fallen alle, die Spinnen kommen, man wird gefressen; wer nach der ersten Kugel weggeht, ist sicher und die Betäubung klingt ab; im Lampenlicht zirpt eine Spinne und huscht weg; betäubt geht man langsam. Level 2: Netz mit eingesponnenen Deathmoths; satt lassen sie Menschen in Ruhe.

## Restmängel

- Die Pfützen haben in der fernen Stufe eckige Ränder (flache Teile vereinfachen schlecht).
- Spinnen-Puppen beim Gast: das Abseilen wird über `hang` und die Position übertragen; die Drehung am Faden rechnet jeder selbst.
- Im Lampenlicht wirken die Säcke noch etwas sauber.
