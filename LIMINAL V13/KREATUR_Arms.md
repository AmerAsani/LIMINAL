# Kreatur: Arms / „Les Bras“ (Entité 15-FR)

Quellen (beide CC BY-SA 3.0, Zusammenfassung in eigenen Worten, kein Bild verwendet; gelesen am 27.10.2026):
- [fr-backrooms-wiki.wikidot.com/entite-15-fr](https://fr-backrooms-wiki.wikidot.com/entite-15-fr) „Entité 15-FR – Les Bras“, Revision 44 (20.01.2024), von Praetor3005, Skizze Aliichat (navhae). Die eigentliche Wesensseite liegt im französischen Zweig des Wikidot-Wikis; auf dem englischen Wikidot gibt es sie nicht (der Link der Level-1-Tabelle führt dort auf eine leere Seite).
- [backrooms-wiki.wikidot.com/level-1](https://backrooms-wiki.wikidot.com/level-1), Revision 83 (07.10.2026), Praetor3005 und DivineAtlas: erster Eintrag der häufigen Wesen von Level 1.

## Was die Seiten sagen

**Gestalt:** lange, verdrehte, verwesende Gliedmaßen mit drei scharfen Krallen am Ende; nur ein Teil des Körpers wurde je gesehen – ein (vielleicht mehrere) sehr lange Arme. Graue Haut (belegt durch verweste, leicht verbrannte Hautfetzen nach Angriffen). Die Krallen umschließen leicht einen menschlichen Oberkörper; ihre Länge kennt niemand (zu schnell).

**Verhalten:**
- Lebt in Lüftungskanälen meist dunkler Innen-Ebenen und kommt nur heraus, wenn es darunter dunkel ist – bei Stromausfällen wie in Level 1 oder in von Natur aus dunklen Ebenen.
- Zweiter Auslöser: Lärm unterhalb der Lüftung im Dunkeln. Dann hört man im Kanal dumpfe Bewegungen – Klopfen, Krallenkratzen, leichtes Streifen; leise (darum gefährlich), aber erkennbar als dumpfes Schaben an Metall.
- Hat es eine Beute gewählt, kommt es aus einer nahen Öffnung (auch das Öffnen ist sehr leise), packt den Wanderer plötzlich und zieht ihn in den Schacht; man hört Kampf- und Schmerzensschreie und Schläge gegen die Schachtwände, die sich rasch entfernen.
- Trifft starkes, direktes Licht das Wesen, bevor es zupackt, gleitet es rasch zurück und stößt einen heiseren, todesschreiartigen Laut aus; unter dem Gitter liegen danach verbrannte, teils rauchende Hautfetzen.
- Level 1: kommt bei Stromausfällen durch Lüftungen und Risse; auf Geräusche von hinten oder oben achten, bei Verdacht Licht darauf richten.
- Rat der Seite: leise sein, das Wesen anleuchten, Geräusche aus Lüftungen nicht überhören, dunkle Innen-Ebenen meiden.

**Nicht gesagt:** Länge des Arms und der Krallen in Zahlen, Tempo, was mit den Opfern geschieht.

## Umsetzung

**Modell** (`tools/blender/creatures/arms.py`, Skelett `anim/creature_arms.cpp`, 33 Knochen): ein Glied von 4,2 m (14 Glieder zu 0,3 m), dazu Hand und Krallen – das meiste liegt im Schacht. Grau, verwesend: verdreht wie ausgewrungen (drei Sehnenstränge laufen spiralig, eine Windung je 1,3 m), drei knotige Gelenke (zu lang für einen Ellbogen – **Interpretation**), 16 Löcher in der Haut, darunter nasses dunkelrotes Fleisch (`ent_nestflesh`), in einigen blanke Sehnen (`ent_tooth`), abgelöste Hautlappen. Eine knotige Hand mit drei Fingern, jeder mit einer 24 cm langen, eingehakten Hornkralle (`ent_horn`). Dazu: ein Riss in der Decke (gebrochener Beton `ent_concrete` um ein dunkles Loch, Risse), drei verbrannte Hautfetzen (`ent_darkskin`) und vier Rauchfahnen (`FM_ENT_SMOKE`, Rauchrand). Material der Haut `FM_ENT_ROTSKIN` (graugrün, speckig).

**Bewegung** (`CreatureAnimator::updateArms`): die Kette folgt einem Weg – im Schacht waagrecht, durch die Öffnung hinab, im Bogen zum Ziel; sie gleitet heraus und hinein, ohne sich zu strecken. Jedes Glied ist ein Stück weiter verdreht und windet sich langsam; eine Welle läuft den Arm entlang (er „tastet“). Ruhe: im Schacht. Wahrnehmen: die Hand hängt herab und tastet zum Lärm. Jagen: zusammengezogen über dem Opfer, die Krallen weit gespreizt (Ausholen). Angriff: blitzschnell zur Brust, die Krallen schließen sich darum. Rückzug: gleitet zurück. Tod/Verschwinden (Licht): der Arm windet sich zuckend, die Krallen schlagen, er schnellt zurück; die Fetzen lösen sich, flattern zu Boden und rauchen.

**Verhalten** (`gameplay/presence_arms.cpp`, Level-JSON `presence.arms`: `every` Abstand der Auftritte im Dunkeln, `chance` Anteil, mit dem es ohne Lüftung in der Nähe durch einen Riss kommt):
- Lager: Deckenlüftungen der Karte (`ceiling_vent`, auch die Lager des Bündels), Rohrgänge (Rohre an der Decke); sonst ein Riss in der Decke eines dunklen Gangs oder Raums (nie im Blick entstanden). Nur unter Decken bis 3,4 m.
- Nur im Dunkeln (Kachellicht < 0,13 oder Stromausfall): erst dann erscheint es, 3–16 m Weg entfernt.
- Hören: Lärm im Dunkeln, Weg < Lärm × 14 m + 0,5 (rennen 14,5 m, gehen 6,8 m, schleichen 2,6 m; still: nie), geworfene Dinge.
- Schaben im Schacht 2,4 s und länger (Ankündigung, alle 0,75–1,3 s); weiterer Lärm (0,4 s) oder Nähe unter 3 m: die Öffnung öffnet sich (sehr leise), der Arm gleitet heraus (1,45 m) und tastet zum Lärm.
- Zugriff: Spieler in Reichweite (Öffnung – Brust 2,85 m) und laut oder näher als 1,6 m → Krallen spreizen (0,55 s Ankündigung, heiseres Rasseln) → Zuschlagen; wer dann in Reichweite ist, wird in den Schacht gezogen (**tödlich**); sonst schnappen die Krallen ins Leere.
- Licht: liegt der Lichtkegel der Lampe 0,25 s (beim Ausholen 0,2 s, beim Zuschlagen 0,15 s) auf Hand oder Unterarm: Schrei, Rückzug, Fetzen, Rauch; 30–50 s Ruhe.
- Wird es hell (Strom zurück) oder bleibt es 10 s still und der Spieler ist weiter als 4 m weg: langsam zurück in den Schacht.

**Klang** (`audio/synth_arms.cpp`): alles aus einem Blechschacht (kurzer, hohler Metallnachhall). `arms_scrape_0..3` (dumpfes Schaben und Klopfen an Blech, Krallen, Streifen; leise), `arms_vent` (kaum hörbares Knarzen), `arms_hiss` (Krallen klicken auseinander, Sehnen knarzen, heiseres Rasseln aus dem Schacht), `arms_snap`, `arms_burn` (heiserer Todesschrei aus dem Schacht, zischende Haut, Zurückschnellen), `arms_retract`, `arms_take` (Schreck: Krallen, dann Schläge gegen die Schachtwände und ein erstickter Schrei, die sich entfernen). **Eigene Stimme**: das heisere, wortlose Rasseln hinten im Schacht.

**Mehrspieler:** KI beim Host; die Puppe beim Gast bekommt Lage (Öffnung, Richtung des Schachts), Zustand und die Kanäle reach (wie weit heraus), jaw (Krallen zu), alert (Ausholen), light (verbrennt), shiver (Rauch), hang (eigener Riss), perceive, up (Normale der Öffnung).

**Nicht umgesetzt:** Wandlüftungen (`vent_grille`) als Lager; mehrere Arme zugleich; der Kult und die „Mains“ (Entité 16-FR).

## Gleichgewicht

| Wert | Zahl |
|---|---|
| Auftritt | Level 1, nur im Dunkeln; frühestens alle 40 s Dunkelheit (±30 %), einer zur Zeit |
| Hören | Lärm × 14 m + 0,5 m Weg |
| Ankündigung | Schaben ≥ 2,4 s, Ausholen 0,55 s |
| Reichweite | 2,85 m von der Öffnung zur Brust (heraus höchstens 3,0 m) |
| Schaden | Zugriff tödlich |
| Licht | 0,25 s im Kegel (12 m) genügen |

## Wow-Check

„Im Stromausfall schabt es über einem im Blechschacht, dann hängt plötzlich ein grauer, verdrehter Arm mit drei Krallen aus der Decke und tastet nach einem – wer die Lampe hebt, hört einen heiseren Schrei und sieht nur noch rauchende Hautfetzen am Boden.“ Ehrliche Antwort: **ja** – im Dunkeln mit Klang und im Lichtkegel; im vollen Licht wirkt die Haut etwas zu hell und glatt.

## Kritikrunden

Bilder: `docs/screens/creatures/arms/` (Blender-Vorschau `blender_r1.jpg`, `blender_endstand.jpg`).

| Runde | Bilder | Was ist schlecht | Was ändert sich |
|---|---|---|---|
| 1 | `r1_dunkel_und_testlicht.jpg` (Stromausfall ohne und mit Lampe, Prüflicht nah/mittel) | Im Dunkeln (gewollt) nichts zu sehen; der Riss ein weißer, gleichmäßiger Kranz; im Prüflicht ist der Arm elfenbeinfarben und glatt wie ein Rohr, die Finger dünn wie Vogelbeine, keine Wunden. | Haut graugrün ohne rötliches Durchscheinen; Finger dicker mit Knöcheln und Muskel, Krallen breiter; Riss unregelmäßig (Stücke fehlen, weiter gestreut); mehr und größere Wunden. |
| 2 | `r2_testlicht.jpg` (mittel, nah von unten, seitlich, ohne Licht) | Knotiger, grauer; die Krallen wirken; Wunden noch unsichtbar, die Haut zu glatt. | Verschiebung doppelt (Querfalten, Beulen, abgelöste Flächen), Wunden tiefer und im sichtbaren Teil, Sehnenstränge dicker. |
| 3 | `r3_brennen_zugriff.jpg` (Prüflicht; Lampe an: Verbrennen und Rückzug; Zugriff) | Das Verbrennen geht (Schrei, Rückzug); Wunden fehlen weiter – Ursache: die Vereinfachung auf 45 000 Dreiecke glättet sie weg (Risskranz und Loch nahmen den Großteil). Der Zugriff ist nur Rot und Dunkel. | Stufe 0 auf 70 000 Dreiecke, Gewichte: Arm 1,4, Fleisch 1,0, Riss 0,35, Loch 0,12, Rauch 0,15. |
| 4 | `r4_nah_lod.jpg` (mittel, nah, Ausschnitt beim Verbrennen, Boden) | Wunden jetzt sichtbar (rotes Fleisch), Haut fleckig; der Riss eine Krone aus gleichen Klötzen; Fetzen und Rauch am Boden nicht zu sehen. | Riss aus schiefen Brocken verschiedener Größe und einer abgeblätterten Putzschicht; Fetzen 8 cm mit Glut; Rauch größer. |
| 5 | `r5_riss_fetzen_fern.jpg` (Lampe trifft den Arm, Rückzug, Boden im Prüflicht, fern im Gang; Vorführung im Ganglicht) | Fetzen liegen am Boden (klein, dunkel); im Ganglicht wirkt der Arm überzeugend (Wunden, Krallen); der Rauch fehlt ganz: der Rauchrand des Shaders dünnt nach den Maßen des Smilers aus (Ruhepose unter 0,7 m = vollständig verworfen). | Rauchfahnen in der Ruhepose um z = 1 m nahe der Achse – dort dünnt der Shader nur teilweise aus. |
| 6 | `r6_rauch_ganglicht.jpg` (Verbrennen, Rauch über den Fetzen, Boden, Rückzug) | Rauch steigt sichtbar auf (gerastert, zieht nach); die Glut der Fetzen ist kaum zu sehen. | Abgenommen; Restmängel unten. |

## Restmängel

- Im nahen, hellen Licht wirkt die Haut zu hell und glatt; im Spiel sieht man den Arm aber fast nur im Dunkeln und im Lichtkegel, der ihn verbrennt.
- Die Glut der Fetzen leuchtet kaum.
- Der Zugriff zeigt (wie bei allen Wesen) vor allem den roten, dunklen Schirm.
- Wandlüftungen (`vent_grille`) werden nicht genutzt; ohne Deckenlüftung in der Nähe kommt es durch einen eigenen Riss (der nie im Blick entsteht).

## Tests (`gameplay_test`, `testV133DullerArms`)

Level 1: im Licht (60 s) keine Arms; im Stromausfall Arms in einer Lüftung oder einem Riss über dem Gang, die Öffnung in der Decke; wer darunter steht: erst spreizen sich die Krallen (Ankündigung), dann wird er in den Schacht gezogen (tödlich); die Lampe auf dem Arm: Schrei (Verbrennen), Rückzug, kein Zugriff.

## Wünsche an die Karte

Deckenlüftungen (`ceiling_vent`) in den Gängen und Rohrgängen von Level 1 (heute nur an den Lagern des Bündels) – dann kämen die Arms öfter aus echten Lüftungen statt aus Rissen.
