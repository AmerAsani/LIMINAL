# Kreatur: Samantha (Entity 26), Level 5

Quellen: [Entity 26 – „Samantha“](https://backrooms-wiki.wikidot.com/entity-26) Rev. 22 (13.12.2025, Autor bunsolvd; Hinweis
„veraltet“, Tag to-rewrite) und [Level 5](https://backrooms-wiki.wikidot.com/level-5) Rev. 184 (07.08.2026; Discord13, Stretchsterz,
ratscrapz), beide Backrooms-Wiki auf Wikidot, **CC BY-SA 3.0**. Alles in eigenen Worten, kein Bild der Seite, keine Zitate. Im Spiel heißt
sie nur „die Katze“ (E-039: kein Wiki-Name im Spieltext).

## Was die Seite sagt

- **Gestalt:** eine gewöhnlich aussehende Hauskatze (weiblich), schwarz-weiß, grüne Augen; klein für eine erwachsene Katze (manche halten
  sie für ein Kätzchen). Kräftiger Kiefer mit ungewöhnlich gezackten, sägeartigen Zähnen, die Haut und Fleisch mühelos zerreißen. Niemand,
  der ihr begegnet ist, hält sie für eine normale Katze. Ein Einzelwesen.
- **Handel:** Sie bringt Reisende dazu, ihr Fleisch zu geben, und bietet dafür eine „psychische Lesung“; deren Güte hängt von der Güte des
  Fleischs ab (Steak, guter Fisch, Hähnchen: ausführlich; Dosenfleisch, Sardinen: ärgerlich und vage).
- **Ohne Fleisch:** wer ihr nichts gibt – auch wer nichts hat –, gegen den wird sie äußerst gewalttätig: kratzen, beißen, krallen, schwere
  Verletzungen; bisher keine bekannten Todesfälle.
- **Fähigkeiten:** spricht mindestens drei Sprachen gewandt; Hellsehen, Gedankenlesen, Teleportation.
- **Rat:** Fleisch geben, ruhig und höflich sprechen, sie nicht reizen, nicht beleidigen.
- **Level 5:** wandert gelegentlich durch die Haupthalle; wer sich nähert, soll hochwertiges Fleisch mitbringen.

## Umsetzung

**Rolle und Skelett:** eigene Rolle `Presence::Role::Samantha` und `anim::Entity::Samantha` (beide Nr. 19, hinter Jerry), Kin
`samantha` (`KIN_SAMANTHA`). Skelett `sa` (`src/anim/creature.hpp`, `creature_samantha.cpp`): 37 Gelenke, 27 Knochen – Becken, Rücken,
Brust, Hals, Kopf, **Kiefer mit Gelenk weit hinten unter dem Ohr**, Zunge, zwei Ohren, vier Beine zu je drei Knochen, Schwanz in fünf
Gliedern.

**Modell** (`tools/blender/creatures/samantha.py`, `data/models/ent_samantha_l0..l2.glb`, 16 000 / 5 600 / 1 900 Dreiecke): kleine
Katze (Schulterhöhe 19 cm, mit Schwanz 0,5 m), Frack: schwarzes Fell (`FM_ENT_SM_FUR`, kurz, seidig), weißer Latz, weißes Maul und
Socken (`FM_ENT_SM_FURW`), große grüne Augen mit leichtem Eigenleuchten (`FM_ENT_SM_EYE`) und **kleinen, runden Menschenpupillen**,
rosa Nase, Ballen, Innenohren (`FM_ENT_SM_NOSE`), weiße Schnurrhaare (`FM_ENT_SM_WHISKER`). Im Maul: oben und unten je Seite eine
Reihe sägeartiger Zähne bis tief in den Rachen, dazu eine zweite, innere Reihe, zu lange Fangzähne; nasses, dunkelrotes Zahnfleisch
und Gaumen (`FM_ENT_SM_GUM`), die Mundhöhle ist aus Kopf und Unterkiefer ausgeschnitten; die Krallen bleiben ausgefahren.

**Bewegung** (`CreatureAnimator::updateSamantha`): sitzt auf den Keulen (Schwanz um die Pfoten), geht im Katzengang (diagonal
versetzt, Schwanz als Fragezeichen), legt den Kopf schief – weiter, als eine Katze es tut –, spricht mit dem Kiefer in Silben und
arbeitender Zunge, buckelt mit gesträubtem Fell (Maßstab der Rumpf- und Schwanzknochen) und flachen Ohren, reißt den Kiefer bis gut
80 Grad auf, springt mit gestrecktem Rumpf und Krallen voraus und hängt am Gesicht (Vorderpfoten hauen, Hinterbeine treten), frisst
mit Kopfrucken und Kauen (eine Pfote hält das Fleisch).

**Verhalten** (`src/gameplay/presence_samantha.cpp`; Level-JSON `presence.samantha`: `every` 300 s, `chance` 0,75, `count` 3,
`rooms` ["flur"]):
- **Auftreten:** selten, höchstens dreimal je Besuch, nur in den Gängen der Haupthalle, ungesehen 10–25 m Weg entfernt („teleportiert“).
- **Vor Sicht:** sie murmelt in fremden Sprachen (Französisch, Italienisch, Englisch – Untertitel) und schnurrt, wenn man nah ist.
- **Ansprechen:** sieht sie einen Spieler (9 m, freie Sicht), setzt sie sich ihm zugewandt und spricht ihn an – in seiner Sprache
  (Untertitel); sie fordert Fleisch. Bleibt er stehen, kommt sie ein paar Schritte näher.
- **Tausch:** hat der Spieler Fleisch (`steak`, `dosenfleisch`) und kommt bis 1,5 m heran, nimmt sie das beste Stück aus dem Rucksack
  (Gedankenlesen), legt es sich vor die Pfoten, frisst es (3,6 s) und gibt dann die Lesung.
- **Lesung:** gutes Fleisch – der nächste Weg hinaus (Name des Ortes, Himmelsrichtung, Schritte), eine Warnung vor dem Mottenstock,
  wenn einer da ist, und ein Satz über die Angst des Spielers; die Stelle des Ausgangs erscheint auf der Karte. Dosenfleisch – nur
  eine grobe Richtung, ärgerlich („Billiges Zeug …“). Danach geht sie und ist fort, sobald niemand hinsieht.
- **Ohne Fleisch:** wer ohne Fleisch näher als 2,2 m kommt oder 5 s in 4 m bleibt, wird gewarnt (Buckel, Fauchen, „Mit leeren
  Händen?“ – 1,6 s); bleibt er innerhalb 4,5 m, springt sie ins Gesicht: 2 s Kratzen und Beißen (alle 0,45 s ein Treffer, je 7
  Punkte × Schwierigkeit) – **nie tödlich**: ab 15 Gesundheit nur noch Kratzen ohne Schaden. Der Blick wird zu ihrem Gesicht gerissen.
  Danach rennt sie davon und ist fort.
- **Reizen:** Lampe 2 s ins Gesicht oder Rennen auf sie zu – erst eine Mahnung („Nimm das Licht aus meinem Gesicht …“), beim zweiten
  Mal die Warnung. Wer ihr das Fleisch während des Fressens wegnimmt, wird ebenfalls gewarnt.

**Klang** (`src/audio/synth_hotel.cpp`): ruhige Frauenstimme (f0 um 205 Hz) aus der kleinen Katze, darunter Schnurren als Rauheit, vor
dem ersten Satz ein Katzentriller; Murmeln leiser, gehaucht, mit anderer Satzmelodie je Sprache; Schnurren (25 Hz); Fauchen mit den
Worten der Warnung; Kampfschrei; Biss (Knurren, Reißen, nass); Fressen (nasses Reißen, Kauen, Knacken); Papier beim Nehmen.
Saetze als Klangnummern ab 300 (`hotel::samLine`, Untertitel und einmaliger Rat der Reisenden).

**Fleisch** (`data/items.json`): „Rohes Steak“ (Gesundheit +6, auf silbernen Tellern des Zimmerservice vor Zimmertüren – die Seite
nennt Zimmerservice des Homely Hotel) und „Dosenfleisch“ (+8 Gesundheit, +3 Verstand; in den Kesselräumen – Auslegung: Vorrat der
Arbeiter). Modelle `meat_steak`, `meat_tin` (`models_l5_wesen.py`), Symbole in `texture_gen.cpp`. Orte: Spawnpunkte `trace`/`meat`
(5 Steaks, 3 Dosen; `tools/levels/level5.py`, eigener Zufall am Ende).

**Mehrspieler:** Der Host rechnet alles; das Ziel ist der nächste Spieler (dessen Rucksack, dessen Gesundheit). Die Puppe beim Gast
bekommt Rolle 19, Lage, Zustand und die Kanäle (crouch, alert, talk, jaw, growl, shiver, reach, hang, strike, bait, perceive, wave,
tilt, light). Sätze und Laute als Klangnummern: für die Gäste wird das Figurereignis durch den genauen Klang mit Untertitel ersetzt
(`kinNetRoute`). Fleisch nehmen, hinlegen, fressen: Inventar und Fund beim Host (Weltereignisse Dropped/Picked); die Lesung kommt als
persönliche Meldung. Protokoll unverändert.

**Skript:** `presence samantha <abstand>`, `near samantha <abstand>`, `figure samantha <abstand>`, `sam <quick|hold|pose|next|state|yaw>
<wert>`, `give steak`.

## Interpretation (begründet)

- **Sprache:** Sie spricht den Spieler in seiner Sprache an, weil sie Gedanken liest; vor Sicht murmelt sie in anderen Sprachen (die
  Seite: mindestens drei Sprachen). Ihre Stimme ist eine ruhige Frauenstimme (die Seite nennt keine).
- **Lesung als Spielwert:** „Hellsehen“ wird zum Weg hinaus und zur Warnung vor dem Stock – greifbar, ohne zu viel zu verraten.
- **Unhöflich:** Lampe ins Gesicht und Rennen auf sie zu gelten als Reizen (das Spiel hat keine Gesprächsauswahl).
- **Nicht tödlich:** „schwere Verletzungen, keine Toten“ – sie hört auf, bevor man stirbt.
- **Körper:** Die runden Menschenpupillen, der zu weit aufreißende Kiefer und die Doppelreihe Sägezähne sind die Auslegung von „hält
  niemand für eine normale Katze“ und „gezackte Zähne“.
- **Fleisch:** Steaks beim Zimmerservice (die Seite nennt Essen und Zimmerservice des Homely Hotel), Dosen im Kesselraum.

## Zahlen und Gleichgewicht

| Wert | |
|---|---|
| Auftritt | alle 300 s × 0,7–1,4, Chance 0,75, höchstens 3 je Besuch; 10–25 m Weg, ungesehen |
| Bemerken | 9 m, freie Sicht |
| Tausch | ≤ 1,5 m mit Fleisch; Fressen 3,6 s; Güte Steak 1,0, Dosenfleisch 0,3 |
| Warnung | ohne Fleisch < 2,2 m oder 5 s in 4 m; 1,6 s; Flucht > 4,5 m rettet |
| Angriff | Sprung 0,32 s, 2 s am Gesicht, Treffer alle 0,45 s, je 7 × Schwierigkeit; nie unter 15 Gesundheit |
| Gemessen (Test, Medium) | ohne Fleisch aus 1,6 m: Warnung nach 0,05 s, erster Biss nach 2,25 s, Gesundheit 40 → 27,9 |

Steaks gibt es fünf, Dosen drei – genug für drei Begegnungen, aber man muss sie suchen; wer Fleisch selbst isst, hat keins für sie.

## Wow-Check

„IIIH, was ist das für ein Vieh?“ – Mitten im leeren Hotelflur sitzt eine kleine schwarz-weiße Katze, sieht dich mit runden
Menschenpupillen an und fragt dich mit der ruhigen Stimme einer Frau, ob du ihr etwas mitgebracht hast; hast du nichts, reißt ihr Maul
bis hinter die Augen auf, voller Sägezähne, und sie springt dir ins Gesicht. Ehrliche Antwort: aus der Ferne harmlos (so will es die
Seite), unheimlich durch Stimme und Pupillen, der Sprung mit dem Sägemaul ist der IIIH-Moment – nicht „ganz okay“.

## Kritikrunden

Bilder: `docs/screens/creatures/samantha/` (Bögen `rN_sam_bogen.jpg`, Abläufe `rN_samflow_bogen.jpg`, `rN_samtrade_bogen.jpg`).

**Runde 0 (Probe)** – Die Beine waren nicht gesetzt (alle Pfoten zum Ursprung der Welt): Katze lag flach, Beine nach hinten.
*Änderung:* Pfoten der Ruhepose in `snap()` wie bei Ratte und Hound.

**Runde 1** – `r1_sam_bogen.jpg`. *Schlecht:* erkennbar eine Katze, aber niedlich; offen hängt das weiße Kinn wie ein Lappen herab,
von den Zähnen sieht man nur zwei Fänge; die Abläufe im Spiel fehlten (erzwungenes Auftreten scheiterte am Start in der Aufzugshalle).
*Änderung:* Rückfall für erzwungene Auftritte (hinter dem Spieler, Skripte und Tests).

**Runde 2** – `r2_sam_bogen.jpg`. *Schlecht:* Das Maul offen ein schwarzes Loch mit zwei weißen Platten (die Zahnreihen verschmolzen);
Katzenschlitze lesen sich niedlich. *Änderung:* runde, kleine Menschenpupillen; Unterkiefer schwarz, nur die Kinnspitze weiß; Zähne
länger, schärfer, mit Lücken, zweite innere Reihe; Mundhöhle ausgeschnitten, damit Gaumen und Zahnfleisch frei liegen.

**Runde 3 (Abläufe)** – `r3_samflow_bogen.jpg`, `r3_samtrade_bogen.jpg`. *Gut:* Ansprechen mit Untertitel, Tausch (sie zieht das Steak
aus dem Rucksack, legt es vor die Pfoten, frisst), Warnung und Sprung laufen. *Schlecht:* Beim Sprung hängt sie über dem Kopf – man sieht
den Bauch, nicht das Gesicht; nach der Lesung faucht sie den Spieler an („Mit leeren Händen?“ – er hatte ja gerade gegeben).
*Änderung:* Sprungziel tiefer, der Blick wird zu ihrem Gesicht gerissen; nach der Lesung geht sie (Abschiedssatz).

**Runde 4** – `r4_samflow_bogen.jpg`, `r4_samtrade_bogen.jpg`. *Gut:* Der Sprung zeigt das aufgerissene Sägemaul vor der Kamera, Krallen
voraus – das ist der Moment. Die Lesung steht als Satz da. *Schlecht:* Die Lesung beginnt klein und der Ortsname hängt ohne Komma im
Satz. *Änderung:* Satzbau der Lesung („„Ein langer, alter Gang … – im Norden, gut 90 Schritte von hier.““).

**Runde 5 (Abnahme)** – `r5_sam_bogen.jpg`, `r5_samflow_bogen.jpg`, `r5_samtrade_bogen.jpg`; beste Ansichten `r5_samflow_d_angriff.jpg`,
`r5_samtrade_d_lesung.jpg`, `r5_samflow_b_fragt.jpg`, `r5_sam_d_maul_lampe.jpg`. Sprung, Tausch, Lesung und Abgang stimmen; das Gesicht
mit Menschenpupillen und Sägemaul trägt. Abgenommen mit den Restmängeln unten.

## Tests (`gameplay_test`, `testV133Level5Hotel`)

Kin und Räume, Fleisch als Fund, Untertitel ohne Wiki-Namen; erscheint ungesehen im Gang; Murmeln vor Sicht; Tausch Steak (Güte 1,
ausführliche Lesung, kein Angriff, Fressgeräusch), Abgang; Tausch Dosenfleisch (vage Lesung); ohne Fleisch Warnung vor dem Biss
(> 1,5 s), Schaden, nie tödlich; Zurückweichen in der Warnung rettet; Rolle 19 im Netzzustand und als Puppe beim Gast.

## Abnahme Level 5 (03.11.2026, `KRITIK_Level5.md` Runden 2–5)

Bilder: `docs/screens/level5/wesen_r2_*` … `wesen_r5_*` (Einzelbilder `build/qa/creatures/level5/rN/samantha_*`, `samflow_*`,
`samtrade_*`).
- **Runde 2**: Weiß im Kerzenlicht grau-braun wie eine Maus; offen zeigt das Maul nach unten (man sieht das Kinn). *Änderung:*
  `FM_ENT_SM_FURW` heller und kühler, Zahnfleisch satter; der Kopf hebt sich beim Aufreißen um gut ein Drittel des
  Kieferwinkels (`creature_samantha.cpp`) – die Öffnung zeigt nach vorn.
- **Runde 3–4**: Rachen rot und offen, aber die Zahnreihen lesen sich als bleiche Blätter neben der Schnauze. *Änderung:*
  dünnere, dann nach innen geneigte Zähne; Weiß 1,85.
- **Runde 5**: Ursache gefunden – die Seitenreihen reichten 6 cm nach hinten und lagen außen an den Wangen; mit gehobenem Kopf
  sah man sie der Länge nach. *Änderung:* Reihen in der Mundhöhle (3,6 cm, hinten kleiner, nach innen), vorn je Kiefer sieben
  spitze Schneidezähne – von vorn ein gezackter Ring um den roten Rachen (`r5`, samantha 4–5, samflow 3). Abgenommen.

## Restmängel

- Das Weiß ist im Kerzenlicht hellgrau, erst im Lampenlicht weiß (warmes, schwaches Licht; heller wirkte es wie Seide).
- Die Karte zeigt den Ausgang nach der Lesung nur dem Host bzw. Einzelspieler; der Gast bekommt den Satz (die Karte ist je Rechner).
- Aus Augenhöhe ist sie klein im Bild (wie es die Seite will); der Schrecken liegt in Stimme und Sprung, nicht in der Silhouette.
