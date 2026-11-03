# LEVELSPEC 5.3 – „Promethei Bibliotheca“

- **Quelle:** https://backrooms-wiki.wikidot.com/level-5-3
- **Revision:** 40, zuletzt geändert am 24.08.2026, 13:03. Abgerufen am 26.10.2026.
- **Autor:** Praetor3005 (laut Zitierblock der Seite).
- **Bildnachweise der Seite:** Falkenpost (Pixabay), Sylvain Brison (Unsplash), Jez Timms (Unsplash), tuku (Pixabay). Diese Bilder werden nicht verwendet.
- **Lizenz:** CC BY-SA 3.0. Nur Fakten in eigenen Worten, keine Texte und keine Bilder (`LICENSES.md`).

## 1. Was das Wiki belegt (eigene Worte)

**Allgemein**
- Eine elegante Bibliothek, ein Teilbereich des „Terror Hotel“ (Level 5), 2016 zufällig entdeckt.
- Sie scheint ein Bewusstsein zu haben, den „Library Mind“. Er tauscht Wissen gegen Erinnerungen der Besucher.
- Seine Bücher enthalten diese Erinnerungen. Wer Wissen nutzen will, um anderen zu schaden, bekommt keine Hilfe.

**Bau**
- Hunderte Gänge in alle Richtungen, praktisch endlos.
- Sieben Stockwerke, verbunden durch Holztreppen.
- An einigen Wänden steht „Promethei Bibliotheca“.

**Material**
- Wände aus Marmor mit einigen Teilen aus Granit.
- Säulen aus einer unbekannten Art Tuffstein.
- Eisengitter an einem Gang, der zu einer Leere führt; das Metall ähnelt den Gittern von Level 3. Die Kronleuchter ebenso.

**Bilder und Bildunterschriften**
- Ein Gang mit drei Stockwerken Regalen, Säulen und Kandelabern um sie herum.
- Reihen alter, staubiger Bücher.
- Ein älterer Gang im Dunkeln mit gemalten Wappen an der Decke (Bücher der Lost Hall Society).
- Über den Regalen hängen Gemälde im Stil der Renaissance.

**Zustand**
- Bücher voller Staub, aber unversehrt.
- Manche Gänge älter und abgenutzter als das sonst makellose Hotel.
- Im Eingangsgang war die Tapete alt und abgeblättert; die Tür an seinem Ende sehr alt.

**Wesen und Gefahren**
- Der Library Mind spricht nicht. Er lässt Bücher fallen oder Regale einstürzen, wenn er zornig ist.
- Nach ein bis zwei Stunden beginnt die Bibliothek, Erinnerungen zu nehmen, meist die neuen oder „wichtigen“. Die Kopfschmerzen nehmen mit dem Verlust zu.
- Herabfallende Bücher und Regale können verletzen.
- Erwähnt werden außerdem Entity 18 („the Gentleman“) in einer Erzählung, M.E.G. (Stützpunkt „Housekeeping“), der Ariane Circle mit seinem Hermes-Team, die Lost Hall Society und das Personal des Hotels.

**Ein- und Ausgänge**
- **Eingang:** ein neu gefundener Gang nahe dem Stützpunkt „Housekeeping“ in Level 5. Er gilt als unzuverlässig.
- **Ausgang:** Manche Gänge enden blind an einer Holztür. Wer hindurchgeht, steht wieder am Eingang der Bibliothek.

## 2. Was LIMINAL bewusst ergänzt

- **Maße** (die Seite nennt keine):
  - Große Halle 40 × 57 m, 10,2 m hoch, mit drei Geschossen Regalen (je 3 m) und Galerien bei 3 und 6 m.
  - Gänge 3 m breit und 4,6 m hoch, mit 3 m hohen Regalen.
  - Begehbar sind drei Höhen: Boden, +3 m und +6 m.
- **Galerien:** Schmiedeeiserne Geländer mit Messinghandlauf, Kerzen auf den Handläufen. Das Wiki nennt Stockwerke mit Regalen, aber keine Galerien; die Galerien sind die Lesart von „drei Stockwerke Regale“ im Bild.
- **Boden:** dunkles Fischgrätparkett, in den alten Gängen Nussbaum (die Seite nennt keinen Boden). So sind auch die Treppen aus Holz.
- **Eiserne Kronleuchter:** weiß gestrichen und rostig wie die Gitter von Level 3 (Wiki: „ebenso die Kronleuchter“).
- **Bilder:** eigene, einfache Gemälde im Stil der Renaissance (Landschaft mit Tempel; Arkade mit Lesendem) und eigene Wappen. Keine Vorlage aus dem Wiki.
- **Buchrücken und Tuffstein:** eigene, prozedurale Texturen (`tools/textures/generate.py`).
- **Die Leere:** ein lichtloser, schwarzer Raum mit dichtem schwarzem Dunst hinter einem Gitter. Die Seite beschreibt die Leere nicht.
- **Erinnerungsverlust:** als langsamer Verlust an Verstand in allen Bibliotheksräumen (0,04 je Sekunde, in alten Gängen 0,05). Nach rund 35–40 Minuten ist der Verstand aufgebraucht.
- **Herabfallende Bücher:** als Geräusch im Raum und als heruntergefallene Bücher auf dem Boden, nicht als Treffer.

## 3. Konflikte und Grenzen

- **Sieben Stockwerke:** Die Engine kennt je Kachel nur einen Boden. Begehbar sind drei Höhen; die Halle zeigt drei Geschosse Regale. Mehr Stockwerke übereinander lassen sich nicht begehen (technische Grenze, `DECISIONS.md` E-013).
- **Library Mind:** Er handelt nicht (kein Handel mit Erinnerungen). Nur fallende Bücher und der langsame Verlust; V13.3: Stille während der Ansprache des Hausherrn und sein Zorn danach (Bücher stürzen aus dem Regal).
- **Bücher lesen:** nur die zwei Funde des Wissens (V13.3: Zettel im Folio, Karteikarten) und das Notizbuch des Hausherrn; die übrigen Funde liegen auf den Regalböden.
- **Gruppen** (M.E.G. Housekeeping, Ariane Circle, Hermes, Lost Hall Society, Originals): nicht dargestellt.
- **Entity 18:** V13.3: als Besuch nach der Schlussgeschichte gebaut (einmal je Ebene, Szene in der Halle, Notizbuch als Fund, Stille, Zorn der Bibliothek) – `KREATUR_Gentleman.md`.
- **Ambassadors (Entity 141), Macchina (Entity 189):** V13.3: als Wissen (laut Seiten nicht anwesend) – Lesepult mit Folio und Katalogschrank im Obergeschoss, zwei Funde zum Lesen – `KREATUR_Ambassadors.md`, `KREATUR_Macchina.md`.
- **Unzuverlässiger Eingang:** Der Gang aus Level 5 führt immer hierher.
