# Kreatur: Ambassadors (Entity 141) – als Wissen in Level 5.3

Quellen (Backrooms-Wiki auf Wikidot, **CC BY-SA 3.0**, gelesen 02.11.2026, eigene Worte, keine Zitate, keine Bilder):
[Entity 141 – „Ambassadors“](https://backrooms-wiki.wikidot.com/entity-141) Rev. 13 (18.02.2026, Autor VivamusLudio) und
[Level 5.3 „Promethei Bibliotheca“](https://backrooms-wiki.wikidot.com/level-5-3) Rev. 40 (24.08.2026, Autor Praetor3005, Abschnitt
Generalities). Im Spiel heißen sie nicht „Ambassadors“: „rote Lichter“ (E-039).

## Was die Seiten sagen

- **Level 5.3:** Die Bücher der Bibliothek enthalten vor allem Wissen über die Backrooms, u. a. über die Ambassadors. Gesehen wurden
  sie in der Bibliothek nicht.
- **Entity 141:** ein intelligentes Wesen, das Wanderer an einen Ort lockt, den es „Zuhause“ nennt; als feindlich und körperlos
  eingestuft. Es erscheint als schwebendes, helles rotes Licht, meist als Würfel, manchmal länglich, und gibt spürbar Wärme ab. Es hebt
  oder dämpft Gefühle (Überlebende waren seltsam ruhig, wo sie Angst haben sollten), ahmt bekannte Stimmen nach – hörbar künstlich,
  wie aus einer Maschine –, liest oberflächlich Gedanken (Liebste, Leidenschaften, Ängste) und nutzt sie; schwebt schnell; ob es durch
  Wände geht, ist unbestätigt. Wer eines berührte, blieb unverletzt, nur die Stelle der Haut wurde grau. Lebensraum Level 11.3, gesehen
  vor allem auf Level 11 und Umgebung; ein Aufnahmegerät eines M.E.G.-Mitglieds fand man auf Level 4. Rat: nie allein gehen, roten
  Lichtern misstrauen, vertrauten Stimmen nicht glauben, Gefühle nicht entscheiden lassen.
- **Geprüft (Auftrag):** Die Entity-141-Seite nennt Level 5 und Level 5.3 **nicht**. Sie treten auf der Ebene also nicht auf – darum
  kein Wesen, sondern Wissen.

## Umsetzung (V13.3): als Wissen, laut Seite nicht anwesend

- **Lesepult** (`lore_lectern`, Kartenobjekt im Obergeschoss, `tools/levels/level5_3.py` am Ende; Block für die Kollision): dunkler
  Nussbaum, Sockel mit Klauenfüßen, gedrechselte Säule, schräges Brett (20°) mit Buchleiste. Darauf ein großes **Folio**: links eine
  eingeklebte Tafel – ein dunkler Gang mit Fluchtpunkt, darin ein **rot leuchtender Würfel** auf Augenhöhe (eigenes Leuchten, roter Schein)
  und ein Mensch, der auf ihn zugeht; rechts Text in fremder Schrift mit Bleistiftnotizen am Rand. Daneben ein altes **Diktiergerät**
  (Chromgitter, Kassettenfenster, rote Leuchte – die Seite nennt eine Aufnahme) und ein Bleistift in der Falz.
- **Fund** „Folio mit roter Tafel“ (`data/notes.json`, mode `fund`, nur dort): ein Zettel mit der Übersetzung der Randnotizen (eigene
  Worte, alles aus der Seite: rotes Licht, Würfel oder länglich, Wärme, bekannte Stimmen wie aus einer Maschine, Gedanken lesen, Ruhe statt
  Angst, Versprechen eines Zuhauses, wer mitgeht, kommt nicht wieder, graue Haut nach Berührung, der Rat der Seite). Modell `lore_slip`
  (liegt schräg auf der Textseite), Spawnpunkt `trace`/`lore` mit Tag `e141` (Lage über Tag `p<x>,<y>`), gelegt von
  `session_library.cpp`.
- **Das Diktiergerät** (Auslegung: Stimmen nachahmen): wer das Folio liest, dem klickt das Gerät von selbst an – Bandrauschen, eine Stimme,
  die man kennt, aber zu glatt, hart gestuft wie aus einer Maschine: „Komm mit. Ich zeig dir, wo ich jetzt wohne.“ (Untertitel; einmal,
  nur dieser Rechner; `lib_dicta`, `synth_gentleman.cpp`), beim ersten Mal der Rat „Vertrauten Stimmen nicht glauben“.
- **Mehrspieler:** gewöhnliche Notiz (der Host entscheidet das Aufheben); Klang je Rechner.

## Interpretation (begründet)

- Bibliotheksform: Folio mit Tafel und Randnotizen, weil die Bücher meist in fremden Sprachen geschrieben sind (Level 5.3) und eine
  Übersetzung lesbar macht, was die Seite sagt.
- Das Diktiergerät verbindet die Aufnahme der Seite mit dem Nachahmen bekannter Stimmen; das Wesen selbst erscheint nicht.

## Wow-Check

„Im Kerzenlicht liegt auf dem Pult ein Folio, aus dessen Tafel ein roter Würfel wirklich glüht – und als ich den Zettel lese, klickt das
Diktiergerät daneben an und eine Stimme, die ich kenne, lädt mich zu sich nach Hause ein.“ Ehrliche Antwort: kein „IIIH“-Schreck (das Wesen
ist nicht da), sondern ein leises, gezieltes Grauen – passend zu Wissen über ein Wesen, das nicht auf der Ebene lebt.

## Kritikrunden (Bilder `docs/screens/creatures/l53_wissen/`, gemeinsam mit dem Katalogschrank – KREATUR_Macchina.md)

**Runde 1** – `w1_bogen.jpg`. *Schlecht:* auf dem Pult lag ein Holzkreuz über dem Folio; der Zettel nicht zu sehen; das Diktiergerät ein
schwarzer Strich; der Fuß ein dünner Notenständer. *Änderung:* Sockel mit Klauenfüßen, kräftige Säule, größerer roter Würfel, größeres
Diktiergerät, Zettel heller und größer.

**Runde 2** – `w2_bogen.jpg`, `w2_pult_vergleich_w1_w2.jpg`. *Schlecht:* jetzt lag eine Holzplatte auf dem Folio – die Hilfe, die nur
neue Punkte schräg stellt, markierte mit `v.tag`; die Fase löschte die Marken, der Fuß wurde mitgedreht und lag oben. *Änderung:* neue
Punkte über eine Menge erkennen.

**Runde 3** – `w3_bogen.jpg`. *Gut:* Pult richtig: schräges Brett, Folio mit glühendem Würfel und Textseite, gedrechselte Säule.
*Schlecht:* der Zettel fehlte weiter – im Protokoll „prop:lore_folio unbekannt“ (falscher Modellname). *Änderung:* `lore_slip`; Test
prüft die Modelle der Funde.

**Runde 4** – `w4_pult_nah.jpg`. *Gut:* der Zettel ist da und hebt sich ab. *Schlecht:* er deckt die Tafel mit dem Würfel zu (Blender-y ist
im Spiel −y). *Änderung:* Lage gespiegelt, auf die Textseite; Höhe auf die Seiten gesenkt (schwebte 8 mm).

**Runde 5 (Abnahme)** – `w5_pult_schrank_nah.jpg`, `w5_pult_nah.jpg`, `w5_pult_nah_lampe.jpg`, `w5_pult_mittel.jpg`,
`w5_obergeschoss_fern.jpg`. Tafel mit glühendem Würfel links, Zettel auf der Textseite rechts, Diktiergerät mit roter Leuchte am Rand.

## Tests (`gameplay_test`, `testV133Level53Gentleman`)

Wissen in der Karte (Lesepult, Spawnpunkt `e141`); Fund liegt bereit; Modell des Funds vorhanden; Text ohne Wiki-Namen (E-039).

## Restmängel

- Die kleine Gestalt auf der Tafel ist aus der Nähe nur ein dunkler Strich.
