# Kreatur: Jerry (Entity 7), Level 2

Quellen: [Entity 7 – „Jerry“](https://backrooms-wiki.wikidot.com/entity-7) Rev. 14 (Autor Sariastuff, ursprünglich 1000dumplings) und
[Level 2](https://backrooms-wiki.wikidot.com/level-2) Rev. 93 (Abschnitt Entities), beide Backrooms-Wiki auf Wikidot, CC BY-SA 3.0. Alles
in eigenen Worten, keine Bilder und keine Zitate des Wikis. Im Spieltext heißt er nie „Jerry“ – nur „der blaue Vogel“ (LICENSE_REVIEW:
keine benannten Einzelfiguren im Spiel; der Name steht nur in Code, Protokoll und Dokumenten).

## Was die Seite sagt

- **Gestalt:** ein blauer Papagei, verglichen mit dem Hyazinth-Ara. Er spricht und denkt wie ein erwachsener Mensch (den Namen hat er sich
  offenbar selbst gegeben). Ob er gut fliegt, frisst oder mehr kann, ist umstritten.
- **Wirkung:** Wer ihm begegnet, kann umprogrammiert werden – kurz starr, dann eine Geste der Ehrfurcht; danach gehört er zu den
  Followers of Jerry. Was es auslöst, ist unbekannt. Wer ihm nah genug kommt, um ihn anzufassen, ist verloren (Video 2019); wer nach dem
  ersten Blick sofort wegläuft, überlebt (Videos 2014 und Level 2, 2018 – dort stand er auf einem **rostigen Rohr**).
- **Angriff:** Er springt Feinde mit einem Kreischen an (Video 2022).
- **Krankheit:** Die Followers raubten Medikamente und plünderten eine Tierstation – man vermutet, er bekommt menschliche Krankheiten.
- **Followers:** Menschen; bemalen ihre Bauten blau und mit Vogelbildern, kleben Fotos von ihm an Wände, malen Anhänger, die ihn
  anbeten, und locken Neue mit Schriften und falschen Tagebüchern, die Sicherheit versprechen. Auf Level 2 zeichneten sie Karten, um
  Menschen anzulocken.
- **Level 2:** an seinen Raum in der Basis der Followers gebunden; wen er oder seine Anhänger bemerken, der entkommt nicht. Im großen
  Stromausfall entführten Kultisten Menschen im Dunkeln; seit das Licht zurück ist, ist der Kult vor allem anderswo aktiv.

## Umsetzung

**Modell** (`tools/blender/creatures/jerry.py`, `data/models/ent_jerry_l0..l2.glb`, 60 000 / 19 000 / 6 500 Dreiecke): Hyazinth-Ara in
echter Größe (1 m vom Scheitel bis zur Schwanzspitze) auf einem 6-cm-Rohr; kobaltblaues Gefieder, Unterseite der Schwingen und des
Schwanzes schwarzgrau; nackter gelber Augenring und gelbe Hautsichel am Unterschnabel; riesiger schwarzer Hakenschnabel mit beweglichem
Oberschnabel; dicke schwarze Zunge mit gelbem Streifen; dunkle Wendezehen (zwei vorn, zwei hinten) mit schwarzen Krallen um das Rohr.
**Interpretation (Körperhorror innerhalb des Wikis):** krank und gerupft – die Brust fast ganz kahl, dazu Kehle, Halsseite, Bauch und
Flanken: blass grau-rosa Haut mit Gänsehaut aus Federfollikeln, Schorf, blaugraue Blutkiele und abstehende, halb ausgerissene Federn am
Rand (Papageien, die krank sind, rupfen sich so). Die Augen sind **Menschenaugen** im Vogelgesicht (er denkt wie ein Mensch): weißer
Augapfel, blasse graugrüne Iris, schwere graue Lider; die Pupille ist ein eigener Knochen und verengt sich („pinning“ – echte
Erregung bei Papageien).

**Materialien:** `FM_ENT_JY_*` (Gefieder seidig 0,38, Unterseite stumpf 0,6, Haut 0,62 mit Durchscheinen, Blutkiele wachsig 0,3,
Horn 0,42, gelbe Haut 0,4, Iris nass 0,03 mit schwachem Eigenglanz – sie fängt das Kerzenlicht, man sieht, wohin er blickt);
Augapfel `ent_tooth`, Rachen `ent_pg_mouth`.

**Skelett und Animation** (`anim::jy`, `src/anim/creature_jerry.cpp`, 68 Gelenke, 57 Knochen): Rumpf, Hals in zwei Gliedern, Kopf,
Ober- und Unterschnabel, Zunge, Pupillen (Maßstab), Halskrause (Maßstab), je Flügel Oberarm, Unterarm, Hand, vier Hand- und drei
Armschwingen, je Bein Oberschenkel, Unterschenkel, Lauf und vier Zehen zu zwei Gliedern, Schwanz in zwei Gliedern.
- *Idle:* Atmen, aufgeplustert, Schwanzwippen, ein Fuß greift nach; der Kopf springt ruckartig von Ziel zu Ziel und hält dann still
  (Papageienkopf); Putzen (Kopf unter den Flügel, an die Brust, Schnabel knabbert), Schnabelknirschen, Flügelschütteln.
- *Wahrnehmen:* Kopf reißt herum (9 rad/s), schlank und aufrecht, Pupillen verengen sich; er sieht mit einem Auge, dann wieder frontal.
- *Fixieren (Jagen):* beugt sich vor, Halskrause gesträubt, spricht – Schnabel in Silben, die Zunge rollt, die Kehle hebt sich,
  die Pupillen pulsieren; Kopf schief.
- *Angriff:* Warnung – Flügel weit gespreizt, Schnabel offen (Kreischen); Unterwerfung – er bleibt mit gespreizten Flügeln sitzen und
  spricht, die Kamera versinkt in seinem Gesicht; Sprung – er fliegt in 0,25 s vom Rohr vor das Gesicht, Fänge nach vorn, Schnabel auf.
- *Rückzug:* wendet sich ab, plustert sich auf, putzt sich; nach einem Sprung flattert er in 1 s zurück aufs Rohr.
- *Tod/Verschwinden:* er stirbt nicht. Ist man weit weg (über 110 m Weg) und er 15 s ungesehen, ist er nicht da (Avatar-Hypothese der
  Seite: niemand weiß, ob es immer derselbe Vogel ist); kommt man zurück, sitzt er wieder auf dem Rohr.

**Sein Raum** (`tools/levels/level2.py`, `tools/blender/models/models_l2_jerry.py`): eine Kammer mit nur einer Tür in den Ziegelgängen
(182, 28), weit vom Start, nahe dem Weg zur Tür nach Level 3. Blau gestrichen (Zone `jerry`, „Blauer Raum“), Röhre gedimmt (0,45,
flackert), warmes Kerzenlicht vom Opfertisch. `jerry_perch`: rostige Steigleitung mit Abzweig in 1,52 m, Absperrschieber mit rotem
Handrad, die Oberseite blank gewetzt, Kotkrusten und Kleckse darunter, ausgefallene blaue Federn, Daunen, aufgeknackte Nussschalen,
Saaten. `jerry_mural` (Rückwand, hinter ihm): der Vogel mit ausgebreiteten Flügeln und Strahlen, kniende Gestalten mit erhobenen Armen,
blaue Handabdrücke, herablaufende Farbe, zwei eingeklebte Fotos. `jerry_shrine` (Seitenwand): zwei Kisten unter blauem Tuch,
brennende Kerzenstummel, Blechteller mit Nüssen und Saaten, Becher Wasser, Glas mit blauen Federn, gerahmtes Foto, Flugblätter, Dose
blauer Farbe, Kniekissen mit zwei Mulden. `jerry_poster`: Fotos eines blauen Papageis mit Klebeband und ein blauer Pfeil zur Tür; v1:
nur Pfeil und Vogelzeichen – vor der Tür und zwei Räume weiter (auch im Vorraum der Tür nach Level 3: der Kult lenkt Neue zu ihm).
Drei Notizen (`data/notes.json`, Level 2): ein Flugblatt der Followers („Ordnung“), die Warnung eines Wanderers („Hinter der blauen
Tür“), ein Tagebuchrest („Fotos“).

**Verhalten** (`src/gameplay/presence_jerry.cpp`, Rolle/Entity/Kin 18, Level-JSON `presence.jerry` mode `room`):

| Zahl | Wert |
|---|---|
| Sehen | freie Sicht bis 17 m, weites Blickfeld (nur direkt hinter ihm nicht); im Dunkeln ohne Lampe bemerkt er nur, wer näher als 8 m ist oder Lärm macht |
| Blickkontakt | sein Kopf zeigt auf dich (28°) **und** du siehst ihm ins Gesicht (20°) **und** er ist zu sehen (Licht oder Lampe) |
| Bann | wächst 0,25/s, aus der Nähe bis 0,5/s (unter 2 m); nach der Warnung halb so schnell; klingt ab mit 0,3/s (er sieht dich, du ihn nicht) bzw. 0,55/s (außer Sicht) |
| Stufen | 0,3 Klingeln und Flüstern im Kopf (Verstand −2) · 0,45 der Blick wird zu ihm gezogen, Schritte bis 55 % langsamer, Bild eng und farblos, alles dumpf · 0,55 zweites Klingeln · 0,75 Warnung: Flügel auf, Kreischen · 1,0 Unterwerfung (tödlich) |
| Zeit bis zur Unterwerfung | bei festem Blick aus 3,2 m etwa 3,5 s ab dem Betreten (gemessen im Test, siehe unten); Warnung knapp 1 s vorher |
| Sprung | wer bis auf 1,6 m herankommt: Kreischen und Flügel, nach 0,55 s Sprung ins Gesicht (tödlich), wenn man noch näher als 2,0 m ist |
| Stimme | murmelt alle 6–12 s (hörbar bis 26 m Weg – man hört ihn vor der Tür); fixiert er dich, spricht er alle 2,2–3,6 s |
| Schaden | nur die beiden tödlichen Zugriffe; der Bann zehrt am Verstand |

**Ausweg (fair):** Geräusch vor Sicht (Murmeln hinter der Tür), Zeichen vor der Tür (Fotos, Pfeile, blaue Tür), Notizen, das
Klingeln als erste Stufe, die Warnung knapp 1 s vor dem Ende. Wegsehen und Umdrehen lässt den Bann sofort abklingen; Lampe aus im
Halbdunkel: man sieht ihm nicht in die Augen. Er verlässt das Rohr nie (außer zum Sprung).

**Klang** (`src/audio/synth_jerry.cpp`, alles synthetisch): `jerry_talk_0..5` ruhige Männerstimme (f0 112–132 Hz, Silben mit wechselnden
Vokalen, Zisch-, Verschluss- und Nasallauten, fallende Satzmelodie) – durch die Syrinx etwas gepresst (Formanten +10 %, feiner
Schnarrton), **keine verständlichen Worte**; `jerry_mutter_0..2` dasselbe leiser, gehaucht, hinter Wänden; `jerry_screech_0..1` Ara-Schrei
(rau, Periodenverdopplung); `jerry_flap_0/1` schütteln bzw. ausbreiten und schlagen; `jerry_preen`, `jerry_grind`; `jerry_spell_on`
(Klingeln schwillt, tiefes Anschwellen, ein geflüsterter Satz) und die Schleife `jerry_spell` (Klingeln mit Schwebung und viele leise
geflüsterte Stimmen); `jerry_subdue` (ganz nah gesprochen, Flügelschlag, Herzschlag). Ereignisse `Speak`, `Flap`, `Preen`, `Spell`
(neu, hinten angefügt), `Screech`, `Announce`, `Attack`.

**Mehrspieler:** Der Host rechnet; der Bann wird je Spieler geführt (`jerryGaze`, alle Spieler mit ihrem Blick), die Lähmung der Gäste
wendet der Host in `pawnInput` an. Die Puppe beim Gast trägt Rolle 18, Lage, Zustand und die Kanäle (jaw, alert, crouch, growl, reach,
fly, wave, perceive, lean, tilt, talk, hang); im Kanal `glow` steht der Bann ihres Ziels – daraus Blickzug, enges Bild und Klang beim
Gast. Stimme, Flügel und Putzen hören alle Gäste; Klingeln und Warnung nur das Ziel. Protokoll unverändert.

## Interpretation (begründet)

- **Blickkontakt als Auslöser:** Die Seite lässt offen, was die Umprogrammierung auslöst. Die Videos zeigen: ansehen und sofort gehen
  überlebt man, näher kommen nicht. Das Spiel macht daraus eine erlernbare Regel – der gegenseitige Blick wirkt, Wegsehen rettet.
- **Tödlich statt Gefolgschaft:** Ein unterworfener Spieler ist im Spiel verloren (wie bei den anderen tödlichen Wesen, F4: Wiedereinstieg
  am Sicherungspunkt). Die Followers selbst sind Menschen und erscheinen nicht – nur ihre Spuren (erlaubte Ausnahme).
- **Krank, gerupft, Menschenaugen:** die Krankheit leitet sich aus der Seite ab (Medikamente, Tierstation); Menschenaugen aus „denkt wie
  ein Mensch“. Beides ist Auslegung.
- **Brennende Kerzen:** Laut Level 2 ist der Kult heute vor allem anderswo – die Kerzen zeigen, dass noch jemand kommt.
- **Eins zugleich:** Die Seite verwirft mehrere Jerrys nicht ganz (Avatar-Hypothese) – im Spiel gibt es einen, und er ist nicht immer da.

## Gleichgewicht

Der Raum liegt abseits des Hauptwegs, aber nahe der Tür nach Level 3; die Wegweiser zeigen hinein. Wer neugierig hineinsieht, hat bei
festem Blick gut drei Sekunden, bei kurzem Hinsehen keinen Schaden; das Klingeln kommt nach etwa einer Sekunde Blickkontakt. Ohne Lampe
im Halbdunkel ist er nur ein Umriss – sicherer, aber man sieht ihn kaum. Der Opfertisch liegt so, dass man ihm nicht zu nahe kommen muss.

## Wow-Check

„IIIH, was ist das für ein Vieh?“ – Ein Papagei, der hinter einer blauen Tür mit ruhiger Männerstimme vor sich hin redet; drinnen sitzt er
krank und halb nackt gerupft unter seinem eigenen Götzenbild, dreht den Kopf mit einem Ruck herum und sieht dich mit **Menschenaugen** an –
und während er spricht, klingelt es in deinen Ohren, das Bild wird eng und du kannst nicht mehr wegsehen. Ehrliche Antwort: unheimlich und
eklig, nicht „ganz okay“ – getragen von Stimme, Blick und Bann; das Modell allein ist eher verstörend-krank als monströs (Restmängel).

## Kritikrunden

Bilder: `docs/screens/creatures/jerry/`.

**Runde 0 (Studio, Vorarbeit)** – `r0_studio_a_vorn.jpg`, `r0_studio_d_offen.jpg`, `r0_studio_e_kopf.jpg`: Grundform liest sich als Ara
(Augenring, Hakenschnabel), die Flügel spreizen sich; im Studio ohne Rohr sitzt er auf dem Boden – nur für Formfragen.

**Runde 1 (im Raum)** – `r1_a_tuer.jpg`, `r1_b_mittel.jpg`, `r1_d_nah.jpg`. *Schlecht:* Der Raum ist schwarz, Jerry ein schwarzer Umriss;
von vorn schmal und hoch wie eine Menschengestalt (Flügel wie hängende Arme), der Kopf zu klein; die kahle Stelle ist eine runde Scheibe
wie ein Aufkleber; die Kotfäden unter dem Rohr stehen gleich lang wie ein Kamm. Das Wandbild und die blaue Tür wirken stark.
*Änderung:* brennende Kerzen auf dem Opfertisch mit warmem Licht, breitere Brust und Schultern, voller Bauch bis auf die Zehen, Kopf
größer, ausgefranste kahle Stellen, Kot als Krusten und kurze Läufe.

**Runde 2** – `r2_b_mittel.jpg`, `r2_c_lampe.jpg`, `r2_d_nah.jpg`, `r2_e_raum.jpg`. *Schlecht:* Mit der Lampe erkennbar, aber wie eine
Figur aus dem Andenkenladen; die kahlen Stellen goldbraun wie Münzen; die Augen aus 2 m nicht zu sehen. *Änderung:* Brust fast ganz kahl
(Kehle, Halsseite dazu), doppelt so viele, längere Blutkiele, Haut grauer, Augen größer, gelber Ring breiter, nasser Glanz in der Iris.

**Runde 3 (Ablauf)** – `r3_b_lampe_fix.jpg`, `r3_c_bann.jpg`, `r3_d_warnung.jpg`. *Gut:* Der Bann nimmt die Farbe, die Warnung mit
gespreizten Flügeln vor seinem gemalten Götzenbild spiegelt dessen Pose. *Schlecht:* zu schnell (2,2 s) – man sieht ihn nicht einmal an;
im Lichtkegel immer noch fast schwarz. *Änderung:* Bann 0,25/s statt 0,32/s (nach der Warnung halb so schnell, knapp 1 s zum Wegsehen),
die Kamera versinkt beim Zugriff in seinem Gesicht (Bildwinkel bis 40 %), das Bild wird mit dem Bann enger, helleres Kobalt, Kerzenlicht
heller und weiter.

**Runde 4 (Nahaufnahme)** – `r4_s_gesicht.jpg`, `r4_s_brust.jpg`, `r4_s_auge.jpg`, `r4_c_kill.jpg`. *Schlecht:* Gefieder glatt wie Plüsch,
die kahlen Stellen wie aufgenähtes Leder, Knopfaugen; der Schnabel glänzt wie Plastik. *Gut:* Der Zugriff – gespreizte Flügel, Kamera
im Gesicht – wirkt wie ein Kreuz. *Änderung:* Menschenaugen (weißer Augapfel, blasse Iris, Lider), zerzauste Federn am Rand der kahlen
Stellen, größere Federn mit Kanten und struppigen Stellen, Haut auf der Menschenhaut-Ebene, Schnabel stumpfer.

**Runde 5** – `r5_studio_s_gesicht.jpg`, `r5_studio_s_brust.jpg`, `r5_studio_s_auge.jpg`. *Gut:* Die gerupfte Brust liest sich jetzt
als nackte Vogelhaut mit Kielen. *Schlecht:* lachsrosa; die Lider decken die Augen fast ganz zu (böse Schlitze statt Menschenblick);
die Iris gelb wie ein Vogelauge. *Änderung:* Lider geöffnet, Iris blass graugrün, Haut grauer.

**Runde 6** – `r6_s_auge_seite.jpg`, `r6_s_drohen.jpg`, `r6_b_lampe.jpg`, `r6_c_bann.jpg`. *Gut:* Im Profil ein weißes Menschenauge mit
dunkler Pupille im blauen Vogelkopf; das Drohen mit dunklen Schwingen. *Schlecht:* Im Raum aus 3,4 m ohne Lampe nur ein Umriss.
*Änderung:* Röhre im Raum 0,45 statt 0,3 (bleibt Halbdunkel).

**Runde 7 (Abnahme im Raum)** – `r7_a_tuer.jpg`, `r7_b_dunkel.jpg`, `r7_c_lampe.jpg`. Die blaue Tür ist von weitem ein Zeichen; ohne Lampe
sitzt der Vogel als blauer Umriss vor seinem Götzenbild, mit Lampe liest man die gerupfte Brust. Bleibt: von vorn sind die seitlichen
Augen kaum zu sehen – er dreht den Kopf, um mit einem Auge zu blicken (Animation).

## Tests (`gameplay_test`, `testV133Jerry`)

TESTS_PLATZ

## Restmängel

- Der Blickkontakt ist eine Näherung (Kopfrichtung aus der Präsenz, zwei Kegel); die Animation läuft ihm leicht nach.
- Beim Gast trägt `glow` nur den Bann des nächsten Ziels; greift er zwei Gäste zugleich an, sieht der fernere sein Klingeln nicht (Bann und
  Tod rechnet der Host trotzdem richtig).
- Keine verständlichen Worte (Synthese); kein Untertitel.
- Aus über 3 m ist er vor allem Umriss und Gefieder – die Menschenaugen wirken erst nah oder im Profil.
- Beim weiten Spreizen dehnen sich die Wurzeln der Schwingen (Skinning an geteilten Gelenken).
- Die Followers erscheinen nicht (Menschen); die Avatar-Hypothese ist nur als „nicht immer da“ umgesetzt.

## Wünsche an die Karte

Keine – Raum, Sitzrohr, Wandbild, Opfertisch und Wegweiser setzt `level2.py` am Ende mit eigenem Zufallsgenerator (Anordnung
unverändert; einzige Änderung an Bestehendem: Zone und Licht der einen Kammer).
