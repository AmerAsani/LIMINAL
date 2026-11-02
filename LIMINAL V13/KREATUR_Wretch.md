# Kreatur: Wretch (Entity 15)

Quelle: [backrooms-wiki.wikidot.com/entity-15](https://backrooms-wiki.wikidot.com/entity-15), Revision 54, gelesen am 26.10.2026 (die Seite ist als „wird überarbeitet“ markiert); dazu die Ebenenseiten von Level 1, 2 und 3.
- Geschrieben von Stretchsterz und etoisle.
- Lizenz CC BY-SA 3.0.
- Zusammenfassung in eigenen Worten; kein Bild der Seite verwendet.

## Was die Seite sagt

**Gestalt:**
- Rotbraune, trockene, schuppige Haut mit Löchern und Pusteln, aus denen ein dicker, rotbrauner Saft sickert.
- Blutunterlaufene Augen; im dritten Stadium lösen sich die Lider auf – die Augen bleiben offen und rollen.
- Zähne und Nägel fallen im dritten Stadium aus und wachsen an anderen Stellen nach, in ungewöhnlicher Zahl und Größe.
- Haar fällt im zweiten Stadium aus. Oft bis auf die Knochen abgemagert – dahinter große Kraft.
- Kleidung und Habe bleiben oft am Körper, meist stark beschädigt.
- Im dritten Stadium läuft ständig brauner Schlamm aus Mund und Öffnungen.

**Der „Wretched Cycle“:** Hunger, Durst und Schlafmangel verwandeln einen Menschen über Wochen bis Monate. Stadium 1: Juckreiz, Wesensänderung. Stadium 2: Haut und Muskeln lösen sich, Löcher und Pusteln; Sprache nur noch Bitten um Wasser, Essen, Hilfe; Mandelwasser hilft. Stadium 3: Lippen und Lider lösen sich, Schlamm; nicht mehr zu retten.

**Verhalten:**
- Meist wenig klug; selten menschliches Verhalten bis hin zu Werkzeugen und Waffen.
- Übermenschlich stark (brechen durch Wände), manche schwächer.
- Von schlurfend langsam bis schneller als ein Mensch.
- In Gruppen wie ein Schwarmgeist.
- Gurgeln und Fauchen: tiefe, langsame Laute heißen meist ein trägerer, hohe ein klügerer oder schnellerer Wretch.
- Halten sich im Schatten, wo Menschen zusammenkommen.

**Ausweg (Seite):** essen, trinken, schlafen; am Laut die Gefahr erkennen; den Schlamm meiden; einen im dritten Stadium nicht retten wollen.

**Nicht gesagt:** ob Licht oder Lärm sie anziehen; wie sie angreifen.

**Ebenen:** Level 1 (aggressive, untote Menschengestalten), Level 2 (fast überall; griffen 2014 die Survivors an), Level 3 (selten, behalten den Körper, rennen, tragen Waffen, sprechen in Bruchstücken).

## Umsetzung

**Modell** (`wretch.py`, Zweibeiner-Baukasten, 1,73 m): ausgezehrt (Rippen als Grate unter der Haut, eingefallener Bauch, knotige Knie, eingefallene Wangen); rotbraune Haut (Pusteln und Löcher in der Verschiebung, Schuppen); große Augäpfel ohne Lider; Mund ohne Lippen mit zu vielen, schiefen Zähnen, dazu nachgewachsene Zähne durch die Wange und unter dem Kinn; brauner Schlamm aus dem Mund, Saftbahnen über den Körper; zerrissene Hose und Hemdreste.

**Bewegung** (Haltung „Wretch“): gebeugt, der Kopf hängt vor (Hals 0,55 rad); schlurft, das rechte Bein zieht nach (Zehen schleifen, Becken sackt ein); ruckartige Kopfbewegungen; rasselnder Atem; Sprint weit vorgebeugt mit schlackernden Armen; Flehen halb aufgerichtet mit vorgestreckten, zitternden Händen; Gurgeln mit zitterndem Kiefer.

**Verhalten** (`thinkWretch`, Level-JSON `presence.wretch`: `mode` shambler/sprinter/mixed, `chance` = Anteil im zweiten Stadium):
- Erscheint bevorzugt im Dunkeln.
- Sieht (18 m, wenn man Licht hat, sonst 7 m) und hört (Sprinten 16 m bzw. 24 m beim schnellen; Gehen 6 m).
- Träge: jagt mit 1,35 m/s, Schläge 34 Health, tiefes Gurgeln.
- Schnell: jagt in Stößen – 4–6 s mit 4,9 m/s, dann 2–3 s nach Luft ringend (die Gelegenheit zu entkommen); Schläge 26 Health; hohes Fauchen.
- Lässt ab, wenn er einen 7 s (schnell: 4 s) weder sieht noch hört, oder über 34 m.
- Zweites Stadium: fleht (Laute), folgt langsam auf 3 m, greift nie an.

**Klang:** `wretch_gurgle_0..2` (tief, nass), `wretch_snarl_0..2` (hoch, rau), `wretch_plead_0..2` (heiser, zwei, drei Silben, fallend, rasselnd einatmen).

**Eigene Auslegung:** Sehen und Hören wie die anderen Jäger; Schaden statt sofortigem Tod (ein Unterschied zu Smiler, Hound, Bündel); Sprint in Stößen; die Zahlen.

**Nicht umgesetzt:** Heilung mit Mandelwasser; Ansteckung durch den Schlamm; Mutationen (Flügel, zusätzliche Glieder, die Masse aus Augen und Mündern); Horden mit Schwarmgeist. (Waffen und Sprachfetzen der Level-3-Wretches: V13.3, Abschnitt unten.)

## Gleichgewicht

| Wert | Zahl |
|---|---|
| Auftritt | Level 1 ~260 s (gemischt), Level 2 ~240 s (gemischt, 30 % flehend), Level 3 ~300 s (schnell); erster nach ~90 s |
| Schaden | 34 (träge) / 26 (schnell) je Schlag × Schwierigkeit (stalker_hit/35); Pause 1,8 / 1,3 s |
| Tempo | 1,35 m/s; schnell 4,9 m/s in Stößen (Spieler: gehen 3, sprinten 5,7 m/s) |

## Kritikrunden

Bilder: `docs/screens/wretch/`.

| Runde | Befund | Änderung |
|---|---|---|
| Blender | Rippen als riesige Stäbe wie ein Akkordeon. | Rippen als dünne Grate auf der Brustkorb-Ellipse; Zähne an falschen Stellen ergänzt (Wiki). |
| 1 (`r1_bewegung_zustaende.jpg`) | Haltung gebeugt, Kopf vor, Schlurfen und Sprint lesbar; Flehen und Zugreifen deutlich. | – |
| 2 (`r2_level2.jpg`) | Im Lampenlicht: lidlose Augen, Zähne, Schlamm – sehr unheimlich; Treffer mit roten Rändern. Die Hose wirkt im nahen Licht weiß (wie bei allen Stoffen, siehe Faceling). | – |
| 3 (`r3_level3_schnell.jpg`) | Schnell, im Dunkeln zuerst nur Umriss mit hellen Augen, dann im Licht greifend. Abgenommen. | – |

## Tests (`gameplay_test`)

Level 2: er sieht einen und jagt; der träge gurgelt tief; ein schwerer Schlag, nicht sofort tödlich; im zweiten Stadium fleht er und schlägt nicht. Level 3: der schnelle faucht hoch.

## V13.3: Level 3 – Waffen, Sprachfetzen, selten

**Quellen** (Wikidot, CC BY-SA 3.0, eigene Worte, keine Bilder, keine Zitate):
- [level-3](https://backrooms-wiki.wikidot.com/level-3), Rev. 18 (26.09.2026; u/M654z, Neufassung Natedagreat563), Abschnitt der klugen Wesen/Wretches und Autopsiebild.
- [entity-15](https://backrooms-wiki.wikidot.com/entity-15), Rev. 54 (07.08.2026; Stretchsterz und etoisle), als „wird neu geschrieben“ markiert.
- [level-1](https://backrooms-wiki.wikidot.com/level-1), Rev. 83 (07.10.2026; Praetor3005 und DivineAtlas), Wesenliste (Rat zu Wretches).

**Was die Seiten sagen:**
- Level 3: Wretches sind dort selten (wer unvorbereitet kommt, überlebt kaum lange genug). Sie behalten den Körperbau – das Fleisch verrottet, zerfließt aber nicht –, tragen meist noch Kleidung und sind darum schwerer zu erkennen. Selbst im dritten Stadium rennen sie und benutzen Waffen. Sie verständigen sich kaum: unverständliches Kauderwelsch oder ein, zwei gepresste Wörter. Das Autopsiebild zeigt schweren Wundbrand.
- Entity 15: selten menschlich klug bis zum Herstellen und Führen von Waffen; im dritten Stadium lösen sich Lider, **Lippen** und Knorpel auf; hohe Laute heißen meist ein kluger oder schneller Wretch; im zweiten Stadium bitten sie nur um Wasser, Essen und Hilfe.
- Level 1: nicht mit ihnen interagieren, **auch wenn man ihre Stimme versteht**.
- **Nicht gesagt:** welche Waffen; wie sie angreifen; Licht und Lärm.

**Umsetzung**
- **Modelle** (`tools/blender/creatures/wretch_l3.py`, Varianten `ent_wretch_pipe`, `ent_wretch_board`, `ent_wretch_blade`, gleiches Skelett): der Wretch aus V13.2 mit geschlossener rechter Faust, einer offenen, zerrissenen olivgrünen Arbeitsjacke (links ab dem Unterarm, rechts am Ellbogen abgerissen, ausgefranste Ränder), blutigen Läufern auf der Jacke und einer Hand voll Blut am rechten Ärmel, langer Hose mit aufgerissenen Knien, nur einem Gummistiefel (der linke Fuß nackt), Wundbrand (schwarz, nass) an den letzten Fingergliedern links und an den Zehen. Der Saft der Haut nur, wo sie frei liegt (Brust in der offenen Jacke, Hals, Mund). Je eine selbst gebaute Waffe, an den Handknochen gebunden:
  - **Rohrstück** (1,04 m, 42 mm, verzinkt): gusseiserner Winkel als Kopf mit Gewinde, Rostkruste an Winkel und Schnittende, schwarzes Isolierband am Griff, Blut am Kopf.
  - **Nagelbrett** (0,9 m): sieben durchgeschlagene, teils krumme, rostige Nägel, gesplittertes Ende, geschnitzter Griff mit Stoffstreifen, Blut auf der Schlagseite.
  - **Blechklinge** aus der Tür eines Schaltschranks: mit der Zange gezackt geschnitten, eine Seite noch lackiert (abgeplatzt), Griff mit schwarzem Kabel umwickelt, Kupferadern stehen heraus.
- **Animation** (`anim/creature_kin_l3.cpp`, nach der Bewegung der Zweibeiner; Variante über `extra`): je Haltung Griff, Waffenrichtung und Handfläche, der Arm folgt (IK), der Handknochen dreht sich mit; beidhändig beim Rohr und Brett (die linke Hand unter der rechten am Griff).
  - Gehen: das Rohr **schleift hinter ihm über den Boden** (Spitze am Boden, Handfläche nach außen), das Brett liegt auf der Schulter, die Klinge tief vor dem Oberschenkel (zuckt).
  - Rennen: Rohr nachgezogen, Brett auf der Schulter, Klinge vorgestreckt.
  - Locken (`plea`): halb aufgerichtet, die freie Hand bittend vor, **die Waffe hinter dem Bein** versteckt.
  - Ausholen (`strike` 0–0,5) und Schlag (0,5–1): Rohr beidhändig über den Kopf, dann von oben nach vorn unten durch; Brett hinter die rechte Schulter, dann waagrecht nach links gefegt; Klinge auf Schulterhöhe zurückgezogen (geduckt, die freie Hand greift vor), dann vorgestoßen. Am Ende des Ausholens zittert die Waffe.
- **Verhalten** (`gameplay/presence_kin_l3.cpp`, `thinkWretchL3`; Level-JSON `presence.wretch.mode = "armed"`; Zustände 0 lauert, 5 lockt, 2 jagt, 6 holt aus, 7 schlägt, 8 weicht zurück, 4 lässt ab):
  - **Selten:** ein Auftritt etwa alle 480 s (vorher 300 s), höchstens einer; erscheint bevorzugt im Dunkeln; Waffe zufällig.
  - **Geräusch vor Sicht:** beim Gehen schleift/klopft/klappert die Waffe (alle 1,3–2,2 s, Klinge 2,2–3,6 s, hörbar bis 26 m); dazu grundlos Gurgeln und Fetzen.
  - **Locken** (Interpretation des Wiki-Rats): sieht er einen aus mehr als 5,5 m (und hört ihn nicht), bleibt er im Halbdunkel stehen, die Waffe versteckt, und bittet in **verständlichen Fetzen** („…hel’…“, „…’ater…“, „…’lease…“, „…go… ho’…“); er kriecht bis auf gut 6 m heran – nah genug, dass man ihn ansprechen könnte. **Wer näher als 4,5 m kommt, ihn anspricht (E), ihn mit der Lampe anleuchtet (1,4 s), rennt oder Lärm macht – oder nach 7–10 s –, auf den stürmt er los** (hohes Fauchen: der kluge, schnelle). Wer sich still außer Sicht zurückzieht, bekommt Vorsprung.
  - **Jagen:** in Stößen wie der schnelle Wretch (4–6 s mit 4,9 m/s, 2–3 s nach Luft ringend); dabei verstümmelte Fetzen.
  - **Angriff mit Ankündigung:** ab 2,3/2,4/2,7 m (Rohr/Brett/Klinge) holt er aus – **Schrei** („NO—!“, „DIE!“ oder ohne Wort) und Ausholklang –, 0,72/0,62/0,52 s; dann der Schlag (0,40/0,36/0,30 s) mit Ausfall (1,2/1,0/3,0 m/s). Getroffen wird bei 45 % des Schlags, wer dann noch in Reichweite (2,05/2,15/2,35 m) vor ihm steht: 32/30/24 Health × Schwierigkeit. **Wer beim Ausholen zurückweicht, dem schlägt er ins Leere** (das Rohr kracht auf den Beton).
  - **Rückzug:** nach jedem Schlag weicht er 0,8–1,0 s einen Schritt zurück und ringt nach Luft; lässt nach 4 s ohne Sicht/Gehör oder über 34 m ab.
- **Ansprechen** (`presence_lure.cpp`, `address`): ein bewaffneter Wretch antwortet nicht – angesprochen stürmt er los. Die Anzeige „E Ansprechen“ bleibt für alle menschenähnlichen Gestalten gleich (sie verrät nichts).
- **Klang** (`audio/synth_kin_l3.cpp`, alles synthetisch): ein kleiner Formant-Synthesizer nach Klatt (englische Laute mit Betonung, Koartikulation über 35 ms, Locus-Formanten). Die Stimme des Wretch: hoch (185–225 Hz), gepresst, rau (Amplitudenmodulation um 30 Hz), Unterharmonische, viel Hauch; **ohne Lippen** – p, b, m, w, f werden zu einem feuchten Schnalzen mit Blase; Wörter brechen ab, davor und danach rasselndes Einatmen, Schlamm brodelt (`wretch_frag_0..7`: 0–3 verständlicher, 4–6 verstümmelt – „…hun… gry…“, „…it… itches…“, „…don’t… look…“ –, 7 Kauderwelsch). Schreie `wretch_cry_0..2` (300–420 Hz, hell). Waffen je Art: Schleifen (`wretch_drag_*`: Rohr-Stick-Slip auf Beton mit Nachklang des Rohrs, Brett klopft und die Nägel kratzen, Blech flattert und das Kabel klappert), Ausholen (`wretch_raise_*`), Fehlschlag auf den Boden (`wretch_miss_*`: Rohr klingt lang nach) und Treffer (`wretch_hit_*`). Untertitel der Fetzen in englischen Resten; beim ersten Fetzen der Rat des Wikis als Hinweis: „Ein Wretch spricht. Nicht antworten, nicht näher gehen – auch wenn man ihn versteht.“
- **Mehrspieler:** der Host rechnet; die Puppe beim Gast bekommt Variante (Waffe), Zustand und die vorhandenen Kanäle (`strike` für Ausholen/Schlag, `plea` für das Locken, `crouch`, `alert`, `growl`, `talk`). Klänge tragen eine Nummer im Ereignis (`gameplay/kin_l3.hpp`); für die Gäste wird daraus das vorhandene Klang-Ereignis mit Namen – alle hören denselben Fetzen und dieselbe Waffe; Untertitel und Hinweis hängen am Klangnamen. Protokoll unverändert.

**Interpretation:** die drei Waffen und ihre Zahlen; das Locken (aus „nicht interagieren, auch wenn man die Stimme versteht“ und der klugen Level-3-Form abgeleitet); dass die fehlenden Lippen die Laute verstümmeln; die Kleidung (Arbeitsjacke, ein Stiefel); Licht als Verrat beim Locken.

| Wert | Zahl |
|---|---|
| Auftritt Level 3 | ~480 s (0,7–1,3 ×), einer zur Zeit, bevorzugt im Dunkeln |
| Sinne | sieht 18 m (Spieler mit Licht) / 7 m; hört Rennen 24 m (Weg), Gehen 6 m |
| Locken | ab > 5,5 m; kriecht bis ~6 m; Angriff bei < 4,5 m, Ansprechen, Lampe 1,4 s, Lärm, außer Sicht 2,5 s, nach 7–10 s |
| Jagen | 4,9 m/s in Stößen (4–6 s), 2–3 s Atemnot (0,5 m/s); Spieler: gehen 3, sprinten 5,7 m/s |
| Rohr / Brett / Klinge | Ausholen ab 2,3 / 2,4 / 2,7 m, 0,72 / 0,62 / 0,52 s; Schlag 0,40 / 0,36 / 0,30 s; Reichweite 2,05 / 2,15 / 2,35 m; 32 / 30 / 24 Health |
| Rückzug | 0,8–1,0 s rückwärts (0,9 m/s), dann 0,35 s Pause |

**Wow-Check:** Aus dem dunklen Hauptgang kommt erst das Schaben eines Rohrs über Beton und ein heiseres, lippenloses „…hel’… hel’…“ – wer der Stimme zwei Schritte entgegengeht, sieht die Gestalt in der zerrissenen Arbeitsjacke das Rohr mit beiden Händen über den Kopf reißen, hört den Schrei und hat eine halbe Sekunde. Ehrlich: Wow für Klang vor Sicht, das versteckte Rohr beim Locken und das beidhändige Ausholen; im vollen Lampenlicht wirkt das Gesicht (Modell V13.2) etwas zu sauber.

### Kritikrunden (Level 3)

Bilder: `docs/screens/creatures/l3_wretch/`.

| Runde | Befund | Änderung |
|---|---|---|
| 1 Blender (`r1_blender_rohr.jpg`, `r1_blender_brett.jpg`, `r1_blender_klinge.jpg`) | Erste Vorschau (nicht aufbewahrt): Jacke prall wie ein Pullover, Blut als große orange Ovale wie Knöpfe, die Saftläufer des Grundmodells stachen als Stäbe durch die Hose, der Wundbrand saß als schwarze Klötze vor den Fingern und unter dem Fuß. | Jacke dünner (1,7 cm), Ärmel abgerissen mit Fransen; Blut als Läufer auf der Jackenoberfläche; Saft nur auf freier Haut; Wundbrand auf den letzten Fingergliedern (Geometrie der Hand) und den Zehen. |
| 2 Haltungen im Spiel (`r2_haltungen.jpg`; `r2_ausholen_zu_frueh_aufgenommen.jpg`) | Alle neun Haltungen lesbar: Rohr schleift, Brett auf der Schulter, Rohr über dem Kopf, Brett fegt, Klinge sticht. Das scheinbar halbe Ausholen auf Brusthöhe war nur zu früh aufgenommen (Feder der Schlaghaltung). Dunkle Rostflecken mitten auf dem Rohr wirkten wie Löcher. | Rost als dünne Kruste an Winkel, Gewinde und Schnittende. |
| 3 Begegnung im Dunkeln (`r3_begegnung_dunkel.jpg`) | Ohne Lampe nur Umriss, Augen und das erhobene Brett – gut. Prüfung: ein lockender Wretch ließ sich nie ansprechen – er stürmte schon bei 6 m los, das Ansprechen reicht nur 6 m (die Wiki-Warnung lief ins Leere). | Locken ab 5,5 m, er kriecht bis gut 6 m heran; Angriff erst unter 4,5 m – Ansprechen ist möglich und wird bestraft. |
| 4 Angriff live (`r4_angriff_live.jpg`) | Mehrere Schläge hintereinander, Treffer mit rotem Rand; im Dunkeln am Boden kaum lesbar. Die Klinge verschwand beim Ausholen hinter der Hüfte. | Klinge beim Ausholen auf Schulterhöhe zurückgezogen (sichtbar). |
| 5 Endstand (`r5_fern_dunkel.jpg`, `r5_mittel_lockt_lampe.jpg`, `r5_nah_rohr_holt_aus.jpg`, `r5_nah_klinge_holt_aus.jpg`, `r5_bewegung_rohr_schleift.jpg`, `r5_brett_fegt.jpg`; Übersichten `r5_uebersicht_gang.jpg`, `r5_uebersicht_bewegung.jpg`) | Im Hauptgang (Ziegel, Rohre): fern im Dunkeln eine Ahnung, mit Lampe eine kleine Gestalt; mittel lockt er bleich mit versteckter Waffe; nah das Rohr über dem Kopf – stark; in Bewegung schleift das Rohr sichtbar hinter ihm. | – |
| Klang (`klang_r1_spektrogramm.jpg` → `klang_r5_spektrogramm.jpg`) | Erste Fassung: Stimmen nur bis 3,5 kHz (die Kaskade fiel steil ab, klang wie durch ein Rohr), Schrei dunkel (Schwerpunkt 680 Hz), Brett-Schleifen nur Zischen (7,2 kHz). | Höhere Formanten parallel, Präsenz-Anhebung; Schrei auf „AA“ mit +9 dB ab 1,8 kHz (jetzt 1,3–1,7 kHz); Schleifen tiefer (Rohr 4,0, Brett 5,5 kHz). |

### Tests (`gameplay_test`, „V13.3: Level 3 – Wretch mit Waffe, Skin-Stealer im Mantel“)
Klangnummern mit Namen (auch für Gäste), Untertitel, Hinweis; nur Wretch/Skin-Stealer tragen Nummern; Level-JSON (bewaffnet, ≥ 400 s); Level 1 unverändert. Wretch: trägt eine Waffe; lockt von weitem mit verständlichen Fetzen; wer näher kommt, auf den stürmt er (hohes Fauchen); vor dem Schlag Ausholen und Schrei; wer zurückweicht, dem schlägt er ins Leere; wer stehen bleibt, wird schwer getroffen (nicht tödlich), zwischen Ausholen und Treffer ≥ 0,5 s; Treffer-Klang je Waffe; danach weicht er zurück; die Waffe schleift beim Gehen; Ansprechen beim Locken und Lampe ins Gesicht lösen den Angriff aus. Der bisherige Level-3-Test („der schnelle faucht hoch“) bleibt grün.
