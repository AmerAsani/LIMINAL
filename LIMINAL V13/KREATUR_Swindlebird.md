# Kreatur: Swindlebird (Entity 192), Level 2

Quellen: [Entity 192 – „Swindlebirds“](https://backrooms-wiki.wikidot.com/entity-192) Rev. 17 (Autor Aether48) und
[Level 2](https://backrooms-wiki.wikidot.com/level-2) Rev. 93 (Abschnitt Entities), beide Backrooms-Wiki auf Wikidot, CC BY-SA 3.0.
Alles in eigenen Worten, keine Bilder und keine Zitate. Im Spiel heißt er nur „ein blauer Vogel“.

## Was die Seite sagt

- **Gattung:** taubenähnliche Vögel, schlau, fressen Beeren und Körner, nisten am Boden unter Zelten aus Stöckchen und Laub. Vier Arten
  nach Farbe, mit gleichem Knochenbau; die rote hat drei Beingelenke und gleitet ohne Gangbild.
- **Die blaue Art:** am seltensten; sieht aus wie ein gewöhnlicher Papagei (Kopfform, Gefieder, Schnabel). Die äußersten Federn tragen
  ein unbekanntes Mittel, das bei Hautkontakt leichtes Delirium auslöst – Angreifer lassen ab, bevor sie ihn verletzen.
- **Level 2:** eine einzige, alte Meldung über einen blauen Swindlebird, nie bestätigt; die Entity-Seite ergänzt eine Meldung von
  2015 über einen „ungewöhnlich mächtigen“ blauen auf Level 2, Untersuchung offen.

## Umsetzung

**Modell** (`models_l2_rand.py`: `sb_body`, `sb_head`, `sb_wing_in@0/1`, `sb_wing_out@0/1`, `sb_tail`, `sb_shin`, `sb_tars`,
`sb_foot`, zusammen rund 9 500 Dreiecke): papageienhafter Kopf mit Hakenschnabel, nacktem hellem Augenring und gelber Iris,
Halskrause und steifer Schopf; plumper Taubenkörper mit flach anliegenden, kleinen Federn in Reihen (Fabric, Rauheit 0,75–0,9,
tiefes Kobaltblau); nass glänzende Perlen und Läufe des Mittels auf den äußeren Federn (Rauheit 0,08); Flügel in Arm- und
Handschwingen; Beine mit **drei Gliedern** (der gemeinsame Knochenbau der Gattung), Papageienfuß mit zwei Zehen vorn, zwei hinten.
Im Spiel 1,6-fach groß (Ara-Größe, etwa 0,45 m) – die Meldung nennt ihn „ungewöhnlich mächtig“.

**Bewegung** (starre Kette in `apparitions_l2.cpp`): aufrecht wie ein Papagei am Boden (Rumpf 45°), die Beine eingeknickt, das
dritte Gelenk deutlich; Kopf dreht und kippt, putzt sich zwischendurch (Schnabel ins Gefieder); bemerkt man ihn, sieht er einen mit
einem Auge an; im Flug Flügel weit ausgebreitet, gut 4 Schläge/s, Beine angezogen, Schwanz gestreckt.

**Verhalten** (ohne Rolle; Level-JSON `apparitions.swindle`, `swindle_chance` 0,5, `swindle_after` 240 s, `swindle_span` 900 s):
- 28 mögliche Stellen am Ende langer gerader Tunnelstücke (`level2.py`). Je Lauf mit 50 % **eine** Sichtung, frühestens nach 4 min,
  spätestens nach 19 min auf der Ebene.
- Er taucht nur auf, wo **niemand hinsieht** und der Spieler freie Sicht durch den Tunnel hat (11–28 m), im Profil zum Spieler.
- Gesehen: ein leises Gurren (fast eine Silbe). Sieht man ihn länger an (0,9–1,8 s), kommt man näher als 8 m oder leuchtet ihn mit der
  Lampe an, fliegt er fort (Flattern) – weg, sobald ihn niemand mehr sieht oder er um eine Ecke ist.
- Sieht man kurz weg, ist er **einfach nicht mehr da** – ohne Flug, ohne Laut.
- Danach: „War da ein blauer Vogel? Da ist nichts. Keine Feder, kein Laut, nichts, was es beweist.“ Keine Spur, kein Fund, kein
  Eintrag; die Sichtung kommt im Lauf nie wieder. Nie gesehen (40 s): still fort, eine spätere Sichtung bleibt möglich.
- Berührung (näher als 0,7 m; er lässt es nicht zu): leichtes Delirium als Meldung („Die Wände atmen.“).

**Klang** (`synth_l2_rand.cpp`): `sb_coo` (tiefes Gurren, dann ein halber Papageienlaut), `sb_flutter` (Flügelschläge beim Auffliegen).

**Mehrspieler:** Der Host setzt die Sichtung an und entscheidet Flucht und Verschwinden mit den Blicken aller Spieler; Gäste bekommen
Auftauchen (Stelle), Flug (Richtung) und Verschwinden als Weltereignis `l2rand`. Die Meldung „Keine Feder“ rechnet jeder für sich.

**Skript:** `rand swindle <abstand> [winkel] [nr]` (erzwingt die Sichtung), `randstate sb_flee|sb_t|sb_clear <wert>`.

## Interpretation (begründet)

- „Unbestätigt“ wird zur Spielregel: Er lässt sich nicht beweisen – keine Feder, kein Foto, kein Notizbucheintrag; man kann nicht
  näher als 8 m heran, nicht lange hinsehen, und wer wegsieht, hat ihn verloren.
- „Ungewöhnlich mächtig“ zeigt sich nur in der Größe; Kräfte nennt die Seite nicht.
- Kein Nest: Bodennester gehören zur Gattung allgemein, auf Level 2 ist nur ein einzelner Vogel gemeldet.

## Zahlen und Gleichgewicht

| Wert | |
|---|---|
| Sichtung | 50 % je Lauf, einmal, 4–19 min |
| Abstand beim Auftauchen | 11–28 m, freie Sicht |
| Flucht | 0,9–1,8 s angesehen, < 8 m, Lampe |
| Weg beim Wegsehen | nach 0,35 s |
| Flug | 7,5 m/s, steigt auf 1,5 m |

Harmlos; eine Erscheinung, kein Gegner.

## Wow-Check

„Am Ende eines dunklen Rohrtunnels sitzt ein viel zu großer, nasser, kobaltblauer Papagei auf dreigliedrigen Beinen und sieht dich mit
einem Auge an – du blinzelst, und da ist nichts.“ Ehrliche Antwort: kein Monster, aber unheimlich falsch am Ort – mehr will die
Seite nicht (unbestätigt, neutral). Für ein „IIIH“ sorgen Größe, das dritte Beingelenk und dass er sich nicht festhalten lässt.

## Kritikrunden (Bilder `docs/screens/creatures/l2_randwesen/`)

**Runde 1** – `r1_sb_bogen.jpg`, `r1_sb_nah.jpg`. *Schlecht:* Körper wie ein Zapfen aus Stacheln, Beine lange gerade Stelzen, der Vogel
steht zu hoch; ein Bild kam zu früh (Kamera ohne Wartebild). *Änderung:* kleinere, flach anliegende Federn in 13 Reihen; Beine
eingeknickt, Rumpf aufrechter; Kamera mit Wartebild.

**Runde 2** – `r2_sb_bogen.jpg`, `r2_sb_vorn.jpg`. *Gut:* liest sich als Papagei, die drei Beingelenke zickzacken sichtbar. *Schlecht:*
zu klein – auf 12 m nur ein Punkt; Perlen des Mittels wie weiße Sterne; Beine gestreift wie eine Feder. *Änderung:* 1,6-fach groß,
weniger und blauere Perlen, Beinschuppen heller.

**Runde 3** – `r3_sb_bogen.jpg`, `r3_sb_mittel.jpg`. *Gut:* auf 2,6 m und im Tunnel deutlich, im Flug ausgebreitet. *Schlecht:*
Gefieder glatt wie Spielzeugplastik, Blau zu rein. *Änderung:* Federn als Stoff, Rauheit 0,75–0,9, tieferes Kobalt.

**Runde 4** – `r4_sb_bogen.jpg`, `r4_sb_vorn.jpg`, `r4_sb_nah.jpg`. *Gut:* stumpfes Gefieder mit Schuppenmuster, nasse Perlen nur noch
als Glanz, der Kopf putzt sich. Kleinigkeit: von vorn wirkt der Bauch etwas gezackt – bleibt (Federspitzen).

**Runde 5 (aus den Augen des Spielers)** – `r5_pov_bogen.jpg`, `r5_pov_sb_15m.jpg`. *Gut:* im Tunnel sofort als fremder, blauer Vogel
zu erkennen, ohne dass Einzelheiten zu sehen sind – genau so viel, wie eine unbestätigte Sichtung braucht. Abnahme.

## Tests (`gameplay_test`, `testV133L2Rand`)

Stelle mit langem Blick; taucht auf, wo niemand hinsieht (11–28 m, ungesehen); angesehen fliegt er fort (Flattern); fort, keine Spur
(Meldung); nur eine Sichtung je Lauf; Näherkommen (8 m) → Flucht; kurz weggesehen → fort ohne Flug, ohne sich zu bewegen; Koop:
Sichtung beim Gast.

## Restmängel

- Im Dunkeln (Stromausfall) sieht man ihn ohne Lampe nicht – und mit Lampe fliegt er; das ist gewollt, nimmt aber manchen Läufen die
  Sichtung.
- Keine Rote, Grüne oder Violette Art (Level 2 meldet nur die blaue).
