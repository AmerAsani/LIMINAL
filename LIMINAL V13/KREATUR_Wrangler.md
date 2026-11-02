# Kreatur: Wrangler (Entity 75)

Quelle: [backrooms-wiki.wikidot.com/entity-75](https://backrooms-wiki.wikidot.com/entity-75) („Wranglers“), Revision 41 (letzte Bearbeitung 07.08.2026), gelesen am 27.10.2026; dazu die Ebenenseiten [Level 1](https://backrooms-wiki.wikidot.com/level-1) (Rev. 83), [Level 2](https://backrooms-wiki.wikidot.com/level-2) (Rev. 93) und [Level 5](https://backrooms-wiki.wikidot.com/level-5) (Rev. 184).
- Geschrieben von Stretchsterz und DrainedWell.
- Lizenz CC BY-SA 3.0 (alle vier Seiten).
- Zusammenfassung in eigenen Worten; kein Bild der Seite verwendet, keine wörtlichen Zitate.

## Was die Seite sagt

**Gestalt:**
- Große, schlangenartige Wesen, die ihren Leib auf viele Arten verdrehen können.
- Haut feucht und klebrig, der Körper durchscheinend – man sieht die Innereien. Mit dem Alter wird die Haut grauer und undurchsichtiger.
- Augen leuchten hellweiß; damit sehen sie im Dunkeln.
- Männchen: ein menschenähnlicheres Gesicht mit breitem Lächeln, rauere Haut. Weibchen: eher wurmartig, statt des Mauls Zangen.
- Erwachsene sind 10 bis 90 Meilen lang (etwa 16 bis 145 km). Die Eier wachsen wie Samen am Leib des Weibchens.
- Ein toter Riese (Level 8) war innen so weit wie ein Tunnel; das Fleisch zerfiel pflanzenartig; darin Skelette, menschliche Gegenstände, Vorräte.

**Verhalten:**
- Sie graben sich durch den Boden, der Leib dreht sich dabei wie ein Bohrer; junge Tiere graben körperlich und laut, ältere „graben“ per Noclip.
- Der Anblick des Leibs versetzt Wanderer in einen fast hypnotischen Zustand und lockt sie an.
- Großer Hunger: Männchen fressen alles auf ihrem Weg, auch Menschen. Weibchen greifen nicht an und ziehen sich zurück, sobald man sie bemerkt – außer sie sind trächtig.
- Das Weibchen legt die Samen in Boden oder Wände; daraus wächst ein Ast mit vielen Jungen, die es füttert. Wer die Jungen stört, den greift die Mutter an.
- Rumpelnde Erschütterungen kündigen sie an; beim Graben hört man sie kommen.

**Ausweg (Seite):** bei rumpelnden Erschütterungen den Bereich räumen; nicht mit ihnen interagieren; die Jungen nicht anfassen.

**Nicht gesagt:** wie sie auf Licht oder Lärm reagieren; eigene Laute; Tempo, Schaden, Ablauf eines Angriffs.

**Ebenen:**
- Level 1: Tabelle der häufigen Wesen – sie graben sich durch Wände und hypnotisieren mit dem Körper; vibrieren Wände, Decke oder Boden, leise gehen.
- Level 2: vor allem Männchen graben sich durch das Level und brechen plötzlich in einen Gang, um die nächste Beute zu schnappen; viele alte Vermisstenfälle; Junge wurden dort nie gesehen.
- Level 5: eine einzelne Meldung aus dem Kesselraum.

## Umsetzung

Ein Wesen dieser Länge sieht man nie ganz. Zu sehen sind Abschnitte – und genau daraus wird die Begegnung gebaut.

**Ein Durchzug (`presence_wrangler.cpp`, `creature_wrangler.cpp`):**
1. **Beben (5,5–8 s):** Rumpeln aus der Tiefe, das näher kommt (Dauerklang `wrangler_rumble` und einzelne Stöße), die Kamera ruckelt, der Staub in der Luft beginnt zu rieseln, die Leuchten im Raum des Spielers flackern (bis zum Ausfall).
2. **Durchbruch:** im Gang quer von Wand zu Wand (der Leib reicht in beide Wände und taucht dahinter in den Boden), in Hallen als hoher Buckel aus dem Boden, in Level 1 manchmal als Bogen, der von der Decke hängt. An den Durchstoßstellen Kränze: aufgeworfene Platten, ein dunkler Spalt, Risse, Brocken, nasse Flecken, Schleimfäden, Staubhäufchen. Die Welt bleibt heil (die Seite: ältere graben per Noclip) – das Durchbrechen zeigen Modelle und Effekte.
3. **Vorbeiziehen (12–18 s, 1,25–1,75 m/s):** 30 starre Ringe laufen einzeln die Bahn entlang und springen tief im Boden an ihren Anfang zurück (endlos). Jeder Ring dreht sich mit seinem Weg weiter (Bohrer), Wellen von Stauchen und Strecken laufen nach vorn (Peristaltik). Die Haut ist durchscheinend: in dünnen Stellen sieht man dunkle Darmstücke, knöcherne Reifen, pflanzenartige Faserstränge und – einmal je Durchzug – Verschlucktes (halb verdauter Brustkorb mit Schädel, Rucksack, Arm, Stiefel).
4. **Versinken (≈5 s):** der Leib sinkt in die Fläche, das Rumpeln entfernt sich.

**Spüren und Zugriff (Männchen, trächtige Weibchen):** Erschütterungen füllen eine Aufmerksamkeit; ab 1 hat es einen bemerkt. Dann **zweite Ankündigung (1,8 s):** der Leib hält an und bebt, an der nächsten Wand (bis 2,4 m, in 1 m Höhe) oder im Boden neben dem Spieler knackt es, Risse und Brocken erscheinen, tief wird Luft durch Zähne eingesogen. Wer jetzt still steht oder leise geht (in den letzten 0,7 s nicht laut), den verliert es (langes Ausatmen, der Leib zieht weiter). Wer laut bleibt, dem folgt die Stelle; der Kopf bricht heraus (0,25 s) und schnappt (Zuschnappen nach 0,18–0,48 s, Reichweite des Halses 3,7 m) – **tödlich** („aus dem Gang geschnappt“, Kamera reißt zum Gesicht).

**Weibchen:** greift nie an; wird es 1,2 s angesehen, zieht es sich schnell zurück und versinkt.

**Sog (Hypnose):** Ist der Leib im Blick (Ringe bis 17 m, Blickkegel 30°), baut sich ein Bann auf (+0,4/s, fällt mit 0,5/s): der Blick wird zum Leib gezogen, man wird bis zur Hälfte langsamer, der Verstand sinkt, ein tiefer schwebender Ton (`wrangler_drone`) schwillt an. Ausweg: wegsehen, gegensteuern.

**Jungtiere (Level 1, selten):** einmal je Ebene an einer Wand außer Sicht – aus einem geplatzten Samen wächst ein faseriger Ast, an dem sechs Junge (gut 0,5 m, nackte blasse Haut, geringelt, Kopfknolle mit zwei Leuchtpunkten und rundem Zahnmund) hängen und sich winden. Nähert man sich (3,5 m), werden sie unruhig und fiepen (Warnung). Wer sie anfasst (näher als 1 m) oder sie lange anleuchtet, ruft die Mutter: kurzes, starkes Beben (2,4 s), ihr Kopf bricht bei den Jungen heraus; wer dann noch in 9 m Umkreis ist, wird geschnappt. Danach 45 s Ruhe.

**Modell (`tools/blender/creatures/wrangler*.py`):** gemeinsames Skelett (48 Knochen: Wurzel, 30 Ringe, 4 Verschlucktes, 6 Hals, Schädel, Unterkiefer, 2 Zangen, 3 Kränze).
- Ring (4 Arten, auf je jeden vierten Ringknochen geklont – `build.py` kann jetzt `clones` und `tris`): durchscheinende Hauthülle (3,2 cm, innen und außen), hinten schmal (steckt im vorigen Ring), vorn ein Wulst, Gewinde, Ringfurchen, Runzeln; innen Darm mit Aussackungen und paarigen Organen, C-förmiger Knochenreifen (an ihm sieht man die Drehung), Faserstränge, Nervenknoten. Die Nachbarringe gehen in die Verdeckung ein – dunkle Falte, wo ein Ring im anderen steckt.
- **Männchen:** ein riesiges Menschengesicht auf dem Schlangenkopf (1,3 m breit): innen hochgezogene, flehende Brauen über einem Grinsen bis an die Seiten des Kopfs, die Lippen von zu vielen Menschenzähnen zurückgezogen (zwei Reihen à 17, Eckzähne länger, Lücken, eine zweite schiefe Reihe dahinter), Zahnfleisch, tiefe Nasolabialfalten, Krähenfüße, Stirnfalten, tiefe Augenhöhlen mit kleinen, weiß leuchtenden Augen; graue, raue Haut. Der Unterkiefer klappt hinten auf (weite Mundhöhle).
- **Weibchen:** wurmartiger, durchscheinender Kopf mit Ringfurchen, runder wulstiger Mund mit zwei Zahnkränzen, vier Taster, zwei schwere, sichelförmige, innen gezahnte Zangen (Chitin), darin sichtbar der dunkle Hirnknoten und die Zangenmuskeln.
- **Trächtig:** dazu Buckel voller Grübchen am Leib, in jedem ein dunkler Same.
- Stufen 118 000 / 50 000 / 15 000 Dreiecke (Jungtiere 14 000 / 5 000 / 1 900). Abnahme Level 2: Stufe 0 von 160 000 auf
  118 000 gesenkt (Grenze der Modellprüfung 120 000), alle drei Formen neu gebaut; Indexlisten mit `tools/qa/repair_glb.py`
  bereinigt (entartete Dreiecke im Augenbereich, gedrehter Umlaufsinn nach dem Vereinfachen).

**Materialien** (`FM_ENT_WR_*`): Haut (Männchen grau, dichter; Weibchen blass rosa, klarer) – im Mesh-Shader gerastert ausgedünnt (Bits 16–23 = Deckkraft, schräg dichter), dazu dünne Stellen (zwei gedrehte Rauschoktaven, dort klarer und glänzender), in der Mitte schimmert das dunkle Innere durch, der Rand leuchtet; raue Kopfhaut, Darm (nass, dunkelrot), Faserstränge (olivbraun), Knochen, Schleim (glänzend, gerastert), Samen, Staub, Risse, Zangen, Schutt.

**Bewegung (Animationen):**
- Ruhe/Idle: Durchzug mit Bohrerdrehung, Peristaltik, seitlichem Schwung der Bahn.
- Wahrnehmen: der Leib hält an und zittert, der Kopf (falls draußen) pendelt suchend.
- Jagen/Ankündigung: Beben, Risse an der Durchbruchstelle (der Kranz erscheint, bevor der Kopf kommt).
- Angriff: Kopf bricht heraus, Hals folgt aus der Fläche (unter Wänden taucht er in den Boden), Maul bzw. Zangen auf, Zuschnappen; tötend: der Kopf vor der Kamera.
- Rückzug: Kopf zieht sich zurück, der Leib versinkt.
- Tod/Verschwinden: Versinken mit Mahlen und Saugen. (Getötet werden kann er nicht.)
- Jungtiere: hängen, pendeln, krümmen sich – unruhig, wenn man näher kommt oder sie anleuchtet.

**Klang (`synth_wrangler.cpp`, Auslegung):** `wrangler_rumble` (Unterbass, Mahlen von Beton, Knirschen der Bewehrung, rieselnder Kies), `wrangler_slide` (nasses, schweres Gleiten im Takt der Peristaltik, Knarren der Haut), `wrangler_drone` (Sog), `wrangler_tremor_*`, `wrangler_breach_*`, `wrangler_groan_*` (Stöhnen des Männchens – ein menschliches „oh“ aus einem viel zu großen Rachen), `wrangler_squelch_*`, `wrangler_sense_*` (Knacken, Einatmen durch Zähne), `wrangler_burst_m*`/`_f*` (Brüllen bzw. Zischen und Zangenschlag), `wrangler_snatch`, `wrangler_sink_*`, `wrangler_settle`, `wrangler_clack_*`, `wrangler_young_*`.

**Mehrspieler:** die KI läuft beim Host; die Puppe beim Gast bekommt alles über die vorhandenen Kanäle (Protokoll unverändert): `lean` = halbe Spannweite/16 m, `tilt` = Höhe/4 m (negativ: von der Decke), `crouch` = gerades Stück/10 m, `wave` = halbe Gangbreite/5 m, `hang` = wie weit heraus, `fly` = Tempo/4 m/s, `alert`/`shiver`/`growl` = angehalten, Zittern, Beben, `reach`/`strike`/`jaw` = Kopf, Zuschnappen, Maul/Zangen, `fwd`+`bait` = Durchbruchstelle (Richtung, Abstand/32 m), `up` = Normale der Fläche, `plea` = Jungtiere, `variant` 0–3. Beben, Staub und Sog rechnet jeder Rechner selbst aus Lage und Kanälen.

**Eigene Auslegung (Interpretation):** Erschütterung als Sinn (Rennen, Springen, laute Schritte nah am Leib, Berühren; geworfene Dinge lenken ab); die zweite Ankündigung und der Ausweg „still werden“; Licht ist ihm gleich (die Seite sagt nichts; seine Augen sehen im Dunkeln); der tödliche Zugriff; Dicke des Leibs 1,5 m (im Spiel muss er durch Gänge passen); die Ringe als sichtbare Abschnitte; Innenleben und Verschlucktes nach dem toten Riesen; die Gesichtszüge des Männchens; die Gestalt der Jungen; alle Zahlen und Klänge.

**Nicht umgesetzt:** der tote Riese mit Pyroil (Level 8 ist nicht im Spiel); Junge, die mit der Mutter ziehen; körperliches Graben junger Tiere.

## Gleichgewicht

| Wert | Zahl |
|---|---|
| Auftritt | Level 1 ~320 s (40 % Weibchen, davon 40 % trächtig; einmal Jungtiere), Level 2 ~260 s (85 % Männchen), Level 5 ~900 s, nur im Kesselgang/Kesselraum (Männchen); erster nach ~160 s; bei laufender Hound-/Smiler-Jagd 4-mal langsamer; Ort 7–17 m Weg, bevorzugt ~11 m vor dem Spieler |
| Ablauf | Beben 5,5–8 s, Durchzug 12–18 s, Versinken ~5 s; bricht nie auf einem Spieler durch (wartet bis 6 s, zieht sonst woanders durch) |
| Sinne | Rennen/Landen bis 24 m: +1,15/s; Gehen bis 6 m: +0,6/s, bis 11 m: +0,25/s; leise gehen nur bis 1,5 m: +0,35/s; Berühren +4/s; still −0,45/s; geworfenes Ding bis 14 m: +0,9 × Stärke (dorthin schnappt es); Schwelle 1 |
| Zugriff | Ankündigung 1,8 s (Mutter 1,3 s); schnappt, wer in den letzten 0,7 s laut war; Reichweite 3,7 m ab der Durchbruchstelle; tödlich (wie Smiler, Hound, Bündel) |
| Sog | +0,4/s, −0,5/s; bis −50 % Tempo, Blickzug 1,1·Bann²/s, Verstand −0,45·Bann/s |
| Beben | Kamera bis 1,1 cm / 0,4°, Staub fällt bis 0,9 m/s, Leuchten flackern alle 0,6–1,4 s |

## Wow-Check

„Der Gang bebt, das Licht flackert, Staub rieselt – dann schiebt sich Ring um Ring ein nasser, durchscheinender Leib aus der Wand, in dem ein halb verdauter Brustkorb vorbeizieht; wer jetzt rennt, dem bricht neben sich ein riesiges, flehend grinsendes Menschengesicht mit viel zu vielen Zähnen aus dem Beton.“ – Ehrliche Antwort: ja, das Gesicht und das sichtbare Innenleben erzeugen das „IIIH“; der Leib allein wirkt aus großer Entfernung ruhiger (Restmangel unten).

## Kritikrunden

Bilder: `docs/screens/creatures/wrangler/`.

| Runde | Befund (was ist schlecht) | Änderung | Bild |
|---|---|---|---|
| 0 Blender | Hals (1,1 Mio.) und Kopf fraßen das Dreiecksbudget – die 30 Ringe blieben mit je ~60 Punkten übrig; das Gesicht wie eine Kartoffel mit Kulleraugen und Strichmund. | Dreieckziele je Gruppe (`tris`, vor dem Klonen), Stufen 160/50/15 Tsd.; Gesicht neu: tiefe Höhlen mit kleinen Leuchtaugen, flehende Brauen, Grinsen mit zurückgezogenen Lippen, zwei Zahnreihen, Zahnfleisch, Falten. | `r0_blender.jpg` |
| 1 Level 2 | Der Leib ein flacher, blass-sandiger „Sandsack“, das Raster wirkt wie Netzgewebe; er steigt so langsam, dass er 2 s nach dem Durchbruch noch halb im Boden steckt; die Skriptkamera stand mitten in der Bahn. | Haut fast deckend mit dunklem Kern und hellem Rand, dünne Stellen; Feder des Durchbruchs 1,4 → 2,6/s; nie auf einem Spieler durchbrechen. | `r1_level2.jpg` |
| 2 Level 1 | In der Halle liest sich der Buckel als Riesenwurm – aber der Staubkranz ist ein weißer Donut, die Hound-Haut zeigt gemalte Kratzerstriche, der Bogen liegt flach. | Kranz neu (aufgeworfene Platten, dunkler Spalt, Risse, nasse Flecken, Schleimfäden, Häufchen), Menschenhaut-Textur, Bogen 1,45 m hoch. | `r2_level1_halle.jpg` |
| 3 Level 1 | Eckige, achsparallele Flecken auf der Haut (Wertrauschen), Männchen zu rosa, die Kopf-Kamera steckte im Kopf. Gut: im Licht sieht man Darm und Reifen durch die Haut. | Zwei gedrehte Rauschoktaven, graue Tönung des Männchens, Vorführkopf bricht zum Betrachter hin heraus. | `r3_level1_haut.jpg` |
| 4 Porträts | Leuchtaugen und Zähne lesen sich schon als Umriss; das Gesicht ist unter Deckenlicht zu dunkel, der Leib zu rot (Kern- und Streulicht). | Kopfhaut heller, Kerntönung weniger rot. | `r4_portraets.jpg` |
| 5 Brut, trächtig | Die Samenbuckel des trächtigen Weibchens wirken (Grübchen, dunkle Samen); die Jungen sind riesige rosa Bänder, die Wandrisse sehen aus wie Spinnenbeine; Schutt zu hell gegen Beton. | Junge neu (kürzer, geringelt, Kopfknolle, Zahnmund, stärker gekrümmt), dünnere, zackige Risse, Schutt und Staub dunkler. | `r5_brut_traechtig.jpg` |
| 6 Level 2 Gang | Quer durch den Gang von Wand zu Wand mit herausgebrochenen Platten – liest sich; im Beben fällt das Licht aus (stark). | (Schutt dunkler, s. Runde 5) | `r6_level2_gang.jpg` |
| 7 Kesselraum, Brut, Zugriff | Level 5: der Leib im Ziegelgang des Kesselraums wirkt sehr gut. Die Jungen noch immer flache, durchscheinende Bänder. Fehler: wer geradeaus davonsprintet, läuft dem Zugriff davon (die Stelle folgte ihm nur bis zum Ausbruch). | Junge mit deckender, blasser Nackthaut; die Stelle folgt bis zum Zuschnappen, Hals 3,7 m. | `r7_kessel_brut_zugriff.jpg` |
| 8 Abnahme | Junge jetzt geringelte, blasse Würmer mit Kopfknolle, die sich am Ast krümmen; Zugriff im Spiel: der Kopf bricht neben dem rennenden Spieler aus der Gangwand. Erst zeigte die Todeskamera nur eine Augenhöhle bzw. den Schädel von oben. | Zugriff endet auf Augenhöhe gut 1 m vor der Kamera: das ganze flehend grinsende Gesicht mit offenem Zahnmaul füllt das Bild. Abgenommen. | `r8_abnahme.jpg`, `best_zugriff.jpg`, `best_ausbruch_wand.jpg`, `best_kesselraum.jpg`, `best_halle.jpg`, `best_jungtiere.jpg`, `best_gesicht_lampe.jpg` |

## Tests (`gameplay_test`, `testV133Wrangler`)

Level 2: erscheint und bebt zuerst (Rumpeln, Beben am Spieler, flackerndes Licht), bricht durch, zieht vorbei, versinkt; wer still steht, wird nicht gespürt. Wer beim Beben rennt, den spürt es; mindestens 1,5 s später wird er geschnappt (tödlich). Wer nach der Warnung still steht, den verliert es. Das Weibchen greift auch bei Rennen nicht an und zieht sich zurück, wenn man es ansieht. Der Anblick bannt und zieht den Blick. Level 1: Jungtiere an einer Wand; sie fiepen; berührt kommt die Mutter (Ankündigung) und holt den, der bleibt. Level 5: außerhalb des Kesselraums keiner; im Kesselgang zieht einer durch, nur im Kesselgang/Kesselraum.

## Restmängel

- Der Leib aus 30 starren Ringen: in sehr engen Kurven sieht man die Überlappung der Wülste; aus großer Entfernung wirkt der Leib ruhiger als aus der Nähe.
- Die gerasterte Haut flimmert in Bewegung leicht (TAA mischt bewegte Wesen nur über wenige Bilder).
- Läuft man durch den Leib hindurch, steht die Kamera kurz in einem Ring (er spürt einen dann sofort – in der Regel folgt der Zugriff).
- Wünsche an die Karte: in Level 2 dickere Wände an einigen Gängen (der Leib taucht hinter dünnen Wänden im Nachbargang in den Boden – folgerichtig, aber selten sichtbar).
