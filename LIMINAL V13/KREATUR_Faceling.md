# Kreatur: Faceling (Entity 9)

Quelle: [backrooms-wiki.wikidot.com/entity-9](https://backrooms-wiki.wikidot.com/entity-9), Revision 39, gelesen am 26.10.2026; dazu die Ebenenseiten von Level 2, 3, 188 und 903 (`ENTITY_ABGLEICH.md`).
- Ursprünglich Jo und Higher Being Love Cereal!; erste Neufassung 1000dumplings; jetzige Neufassung Nikuchan.
- Lizenz CC BY-SA 3.0.
- Zusammenfassung in eigenen Worten; kein Bild der Seite verwendet.

## Was die Seite sagt

**Gestalt:**
- Menschenähnlich im Körperbau; die Organe sind fehlgebildet und unterentwickelt.
- Gewöhnliche Facelings haben kein Gesicht. Haar, Hautton, Kleidung, Körperbau, Geschlecht und scheinbares Alter sind sehr verschieden.
- „Entwickelte“ haben teilweise oder ganz, aber fehlgebildete Züge: halb verwachsene Nasenlöcher, kleine, kurzsichtige Augen, einen Mund ohne Lippen, der oft offen steht, platte Ohren mit viel Ohrenschmalz (hören schlecht), keine Brauen und Wimpern, dafür mehr Körper- und Gesichtshaar.
- Erwachsene sind körperlich schwächer als die meisten erwachsenen Menschen.

**Verhalten:**
- Meist friedlich. Sie ahmen menschliches Leben, Bräuche und Feste nach, oft ohne den Sinn zu verstehen.
- Untereinander freundlich; sie winken Menschen, die sie kennen.
- Gereizt durch kleine Störungen, Angriffe, Bedrängen in engen Räumen und Schaden an ihresgleichen: dann Ausbrüche – sie greifen den an, der sie gereizt hat. Weil sie schwächer sind, entkommt man meist leicht.
- Sprechen nicht; Gesten, Nicken, Schrift; manche machen undeutliche Vokallaute.

**Ebenen (Ebenenseiten):**
- Level 2: intelligent, in Gruppen, lassen einen in Ruhe, wenn man sie nicht reizt.
- Level 3: alle feindselig und mit Steinwerkzeugen bewaffnet, „entwickelt“.
- Level 188: einer im rosa Kleid in den Gängen; tödlich (tötete ein Expeditionsmitglied mit Hieben und Stichen).
- Level 903: friedlich, leben neben Menschen.

## Umsetzung

**Modelle** (`tools/blender/creatures/faceling*.py` auf dem Zweibeiner-Baukasten `human.py`, Skelett `bp`, 1,70 m):
- `ent_faceling` – Mann, graues Hemd, dunkle Hose, kurzes Haar, glattes Gesicht ohne Züge.
- `ent_faceling_b` – Frau, dunklere Haut, langes Haar, blaue Bluse, Jeans.
- `ent_faceling_c` – älterer, schwerer Mann, graues Haar, olivgrüne Jacke.
- `ent_faceling_evo` – entwickelt: ein kleines Auge, halb verwachsene Nase, offener Mund ohne Lippen mit Zähnen, platte Ohren, Bart, behaarte Unterarme, zerrissenes Hemd, geschlagener Stein in der rechten Hand.
- `ent_faceling_dress` – rosa Kleid, langes Haar, nackte Arme und Schienbeine.
- Haut mit eigener Texturebene „Mensch“ (Poren, Röte, Leberflecken).

**Bewegung** (`updateBiped`, Haltung „Faceling“): menschlicher Gang mit Anlauf, Abrollen und Gewichtsverlagerung; Armschwung; Kopf mit eigener Blickrichtung; Lauschen (Kopf schief). Gesten: winken; Hand vor das Gesicht im Lampenlicht (nur ruhig); zuschlagen mit dem Stein (Ausholen über den Kopf, Hieb nach vorn unten); zugreifen mit beiden Händen.

**Verhalten** (`thinkFaceling`, Level-JSON `presence.faceling.mode`):

| Ebene | Art | Verhalten |
|---|---|---|
| 903 | friedlich (`peaceful`), Gruppen 2–4 | gehen umher, stehen; wer etwa 2,5 s in ihrer Nähe gesehen wird, dem winken sie (dann 35–60 s nicht wieder); laufen einem nicht in den Weg |
| 2 | reizbar (`irritable`), Gruppen 2–4 | lassen einen in Ruhe. Reiz: Lampe im Gesicht (unter 9 m), Bedrängen (unter 1,3 m), Anrempeln im Sprint. Ab 2,5 s Reiz: Ausbruch – laut, kommt mit 2,3 m/s, fuchtelt, 8 Health je Schlag; die Gruppe macht mit (65 %); nach 10 s oder mit 12 m Abstand ruhig |
| 3 | bewaffnet (`armed`), Gruppen 1–3, Variante „entwickelt“ | kurzsichtig: sieht einen erst auf 5 m (8 m, wenn man ihn anleuchtet); schwerhörig: hört Sprinten erst auf 6 m. Dann 2,7 m/s, Hiebe mit dem Stein: 0,5 s ausholen, Treffer 22 Health, 1,2 s Pause. Verliert einen aus den Augen (5 s, über 6 m) |
| 188 | tödlich (`lethal`), allein, rosa Kleid, nur in Gang, Zelle, Treppenhaus | sieht sie einen in den Gängen, bleibt sie 1,6 s stehen und neigt den Kopf (Ankündigung), dann kommt sie (1,5 m/s), unter 9 m rennend (3,7 m/s); erreicht sie einen: tödlich. Verlässt man die Gänge (in den Hof) oder ist 6 s außer Sicht, lässt sie ab |

**Klang:** gedämpfte Vokale hinter geschlossener Haut (`faceling_hum_0..3`), aufgebracht schneller und höher (`faceling_outburst_0..1`), der Stein (`stone_swing`), Treffer (`hit_body`); Schritte nach Untergrund.

**Mehrspieler:** Host entscheidet; Gesten (winken, zuschlagen) und Varianten in der Momentaufnahme; Treffer gelten dem jeweiligen Spieler.

**Eigene Auslegung:** die Lampe im Gesicht als „kleine Störung“; dass sich der Faceling im Kleid an die Gänge hält und im Hof ablässt; die Zahlen.

**Nicht umgesetzt:** Kinder, polygonale, Schatten- und Erinnerungs-Facelings (die Seite nennt sie, die Ebenen des Spiels nicht); Verständigung über Gesten und Schrift.

## Kritikrunden

Bilder: `docs/screens/faceling/`.

| Runde | Befund | Änderung |
|---|---|---|
| Blender | Grundform gut; der Rock des Kleids reichte als Kugel bis zum Boden (runde Kegelenden). | Rock gerade abgeschnitten. |
| Bewegung (`bewegung_zustaende.jpg`) | Beim Losgehen blieb der Standfuß stehen – Spagat, Becken am Boden (B-016). | Anlauf: hinterer Fuß tritt zuerst; Ausgleichsschritt; Ferse hebt sich; Becken sinkt höchstens 7 cm. |
| 1 (`r1_level903_alle_gleich.jpg`) | Winken funktioniert; alle vier sehen gleich aus – die Seite betont Vielfalt. | Zwei weitere Varianten (Frau, älterer Mann), zufällig je Faceling. |
| 2 (`r2_level903_varianten_level2.jpg`, `r2_level2_lampe.jpg`) | Level 903 verschieden, mehrere winken. Level 2: die Lampe reizte nicht (B-013: die Sichtprüfung begann in der Hand an der Wand); die braune Jacke wirkte im Lampenlicht wie Haut. | Licht auch vom Auge geprüft, zusätzlich die Brust; Jacke dunkles Oliv. |
| 3 (`r3_level3_entwickelt.jpg`, `r3_level188_rosa_kleid.jpg`) | Level 3: zwei Entwickelte im dunklen Zellentrakt, einer schlägt zu (Treffer). Mangel: zwischen den Hieben hob er die Hand vors Gesicht (Lichtgeste). Level 188: sie steht am Gangende, neigt den Kopf, dann der tödliche Zugriff mit dem leeren Gesicht im Bild. | Lichtgeste nur bei ruhigen Facelings. |

**Hinweis:** Nahe an der Lampe wirkt dunkler Stoff fast weiß (`lampe_stoff_nah.jpg`). Die Debug-Ansichten (`lampe_stoff_debug.jpg`: Normalen, direktes Licht, Albedo) zeigen richtige Albedo und Normalen – es ist die Stärke der Lampe auf 1,8 m (Abstandsquadrat), wie bei allen nahen Gegenständen; keine Änderung.

## Tests (`gameplay_test`)

Level 903: Variante, mit Sicht, winkt, kein Schlag. Level 2: ohne Reiz Ruhe; Lampe → Ausbruch, laut; leichte Schläge, nicht tödlich; mit Abstand ruhig. Level 3: entwickelt; auf 8 m unbemerkt; nah Angriff, Stein, Schaden. Level 188: Kleid, in den Gängen, Ankündigung, tödlich; im Hof lässt sie ab.
