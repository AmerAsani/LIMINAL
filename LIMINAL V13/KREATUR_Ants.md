# Kreatur: Kolonie (Entity 89, „Ants“), Level 5

Quellen: [Entity 89 – „Ants“](https://backrooms-wiki.wikidot.com/trimmed:entity-89) Rev. 35 (13.05.2025, Autor ratscrapz; im Mass Trimming
Project archiviert – `entity-89` selbst ist leer, Level 5 verlinkt genau diese Seite) und [Level 5](https://backrooms-wiki.wikidot.com/level-5)
Rev. 184 (Abschnitt Entities: „Berichte über Entity 89“), beide Wikidot, **CC BY-SA 3.0**. Eigene Worte, keine Bilder. Im Spiel ohne
Namen: „etwas Schwarzes, Flauschiges“.

## Was die Seite sagt

- Viele kleine, flauschige, runde Wesen (2–6 cm), immer als Kolonie – Dutzende bis Hunderte an einem Ort.
- Sie scheinen Licht zu schlucken und wirken tiefschwarz; dicht gedrängt sehen sie viel größer aus, als sie sind – das schreckt größere
  Wesen ab.
- Ungestört hocken sie in Ecken, am liebsten in den dunkelsten eines Raums.
- Sie kommen niemandem von selbst nahe – außer man kommt näher als etwa drei Fuß (0,9 m) oder reizt sie: dann klettern sie schnell an der
  Person hoch, krabbeln auf ihr herum, haften sich an und beißen wie Blutegel. Höchstens leichte Blutergüsse, nicht tödlich.
- Gegenmittel: einfach abzupfen. Sie wandern ohne Muster von Level zu Level.
- Level 5: „es gab Berichte“ – ohne Ort.

## Umsetzung

**Ohne eigene Rolle** (Weltobjekt, `src/gameplay/hotel_l5.*`, Sitzung `session_hotel.cpp`): eine Kolonie ist ein Haufen einzelner
Tierchen, die das Spiel jedes für sich bewegt.

**Orte:** die dunkelste Ecke jedes dunklen Zimmers und eines Teils der leeren und eingerichteten Zimmer (die Ecke am weitesten von
Tür und Lampe; Spawnpunkte `trace`/`ants`, 31 Ecken). Je Lauf wohnt nur in gut einem Drittel eine Kolonie (`colony_chance` 0,35 – sie
wandern: je Lauf anderswo).

**Modell** (`ant_ball@0..3`, `models_l5_wesen.py`): flauschige Kugel (im Spiel 2–6 cm), dichte, weiche Haarbüschel, oben länger; unten
eine nasse Saugscheibe mit einem Kranz winziger Haken (Biss wie ein Blutegel – Auslegung). Im Spiel bekommen alle Tierchen das feste
Material `FM_ENT_UMBRA` (schwarz, stumpf): der Haufen bleibt im Lichtkegel schwarz – er schluckt das Licht.

**Haufen:** 120–170 Tierchen, dicht gedrängt in einem Viertelkreis von gut 0,4 m in der Ecke, bis 0,38 m hoch – ein schwarzer,
pelziger Hügel, der wie ein großes Wesen wirkt. In Ruhe ein leises Gedränge; ist jemand näher als 2,4 m, wird es unruhig (Rascheln).

**Auslöser:** näher als 0,9 m an den Haufen (plus seine Ausdehnung) – gut acht von zehn Tierchen strömen über den Boden (2–2,8 m/s, rollend,
hüpfend) zu den Füßen und klettern an den Beinen hoch bis an den Rumpf (jedes bis zu einer eigenen Höhe), krabbeln dort herum.
Wer schneller als sie wegläuft (über 3,4 m vom Haufen), dem folgen die, die noch am Boden sind, nicht.

**Am Körper:** leichter Dauerschaden wie Blutegel (0,065 Gesundheit je Tierchen und s – mit 80 Tierchen gut 5/s, knapp über der
Erholung des Körpers), nie unter 30 Gesundheit; das Krabbeln zehrt am Verstand (0,006 je Tierchen und s). Man sieht sie an den eigenen
Beinen, wenn man an sich heruntersieht; am Bildrand kriechen schwarze, flauschige Kugeln herauf (HUD, dichter, je mehr dran sind).
Hinweis „Abzupfen“ (Benutzen-Taste).

**Abzupfen:** Benutzen-Taste ohne Ziel – je Griff ein Büschel (14 Tierchen); sie fallen herunter und rollen in ihre Ecke zurück. Danach
ruht die Kolonie 12 s.

**Klang** (`synth_hotel.cpp`): Schwall winziger, weicher Schritte und Fell an Fell (wie Regen auf Filz); leises Rascheln in der Ecke;
Abzupfen (Haare reißen, feuchtes Ploppen, Trippeln am Boden).

**Mehrspieler:** Der Host entscheidet Auslösen (welche Kolonie, welcher Spieler) und Abzupfen (wer, wie viele) und meldet es als
Weltereignis Dropped „l5hotel“ (Kennzahl); die Gäste bewegen die Tierchen nach denselben Regeln selbst. Das Abzupfen des Gasts kommt als
Interact mit fester Kennzahl (`l5ants:pick`), Schaden und Verstand rechnet der Host. Protokoll unverändert.

**Skript:** `hotel colony <abstand> [winkel] [nr]`, `hotelforce colony <nr>`.

## Interpretation (begründet)

- Keine Lichtreaktion (die Seite nennt keine) – das Lichtschlucken ist ihr Bild, nicht ihr Verhalten.
- Die Saugscheibe mit Haken erklärt „beißen wie Blutegel“; Beine, Kopf, Augen nennt die Seite nicht – das Modell hat keine.
- Der Schaden ist so bemessen, dass er die Erholung knapp übersteigt: „nicht wirklich gefährlich“, aber man will sie loswerden.
- „Sie wandern zwischen den Ebenen“ wird zu: je Lauf andere Ecken.

## Zahlen

| Wert | |
|---|---|
| Kolonien | ~11 von 31 Ecken je Lauf |
| Tierchen | 120–170, 2–6 cm |
| Unruhe / Auslöser | 2,4 m / 0,9 m (+ Haufen) |
| Am Körper | 0,065 Gesundheit/(Tier·s), nie unter 30; Verstand 0,006/(Tier·s) |
| Abzupfen | 14 je Griff (Test: alle mit höchstens 12 Griffen) |

## Wow-Check

„IIIH, was ist das für ein Vieh?“ – In der dunkelsten Ecke liegt ein pelziger schwarzer Haufen, den die Lampe nicht aufhellt; ein
Schritt zu nah, und er zerfällt in hundert flauschige Kugeln, die über den Boden auf dich zurollen und an deinen Beinen hochkrabbeln,
während es am Bildrand schwarz heraufkriecht. Ehrliche Antwort: eklig und beklemmend (wie es die Seite will: unangenehm, nicht tödlich),
nicht „ganz okay“.

## Kritikrunden

Bilder: `docs/screens/creatures/kolonie/`.

**Runde 1** – `r1_colony_bogen.jpg`. *Schlecht:* Ohne Lampe ist das Zimmer schwarz (gut so), mit Lampe ein kleiner, lockerer Haufen
grauer Kugeln – weder groß noch schwarz; am Körper und im HUD kaum etwas zu sehen. *Änderung:* größer, höher, 95–140 Tierchen.

**Runde 2** – `r2_colony_bogen.jpg`. *Schlecht:* immer noch grau (Glanz im Lampenlicht, Stoffmaterial). *Änderung:* festes
Material `FM_ENT_UMBRA` (schwarz, stumpf); Krabbeln am Bildrand mehr, größer, dunkler.

**Runde 3** – `r3_colony_bogen.jpg`. *Gut:* ein schwarzer, pelziger Haufen in der Ecke; die Kugeln an den Beinen und am Bildrand sind zu
sehen. *Schlecht:* der Haufen ist an den Rändern zu locker gestreut. *Änderung:* 120–170 Tierchen, zur Mitte dichter.

**Runde 4** – `r4_colony_bogen.jpg`. Dichter Hügel, wirkt wie ein Wesen. *Schlecht:* Der Dauerschaden war im Test unsichtbar – die
Erholung des Körpers (4/s) fraß ihn auf. *Änderung:* 0,065 je Tier und s.

**Runde 5 (Abnahme)** – `r5_colony_bogen.jpg`; beste Ansichten `r5_colony_b_lampe_nah.jpg` (Haufen im Lichtkegel),
`r5_colony_c_stroemt.jpg` (sie strömen heran), `r5_colony_e_hud.jpg` (an den Beinen, Bildrand). Abgenommen.

## Tests (`gameplay_test`, `testV133Level5Hotel`)

Orte in der Karte; Kolonien nur in einem Teil der Ecken; ≥ 100 Tierchen; aus 2,5 m keine Reaktion, unter 0,9 m klettern sie hoch
(≥ 40 am Spieler); Hinweis „Abzupfen“; leichter Dauerschaden trotz Erholung; Abzupfen mit höchstens 12 Griffen; nie tödlich.

## Restmängel

- Durch das feste Schwarz sind Saugscheibe und Haken im Spiel nicht zu sehen (nur im Modell).
- Das Abschrecken größerer Wesen ist nur ein Bild (der Haufen wirkt groß); die Wesen des Spiels meiden die Ecken nicht.
