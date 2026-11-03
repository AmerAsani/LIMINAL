# Kritikrunden Level 5 (Abnahmeschranke d)

Laufweg `python tools/qa/walkway.py level5 --round rN --rooms 18 --seed 3` (ab Runde 2 `--rooms 15`), Bögen
`docs/screens/level5/wegbilder_rN_*.jpg`; Wesen ab Runde 2 `python tools/qa/creatureshots.py level5 --round rN`, Bögen
`docs/screens/level5/wesen_rN_*.jpg`.
Grundlage: `LEVELSPEC_5.md` (Wikidot Level 5).

## Runde 1 (Struktur, Ausstattung V13.2)

Laufweg 1 002 m durch 18 Räume, 251 Bilder. Geprüft wurden die Bögen 1 und 4.

Befunde:
- **Hotelgänge wie im Wiki:** Damasttapete, dunkle Täfelung, antike Kerzenleuchter, Porträts, Türen, roter Teppich, Sofas und Beistelltische.
- **Halle:** heller Marmor mit Kronleuchtern.
- **Schwarze Bilder** (032, 132, 133, 137, 138, 143): Der Weg führt in unbeleuchtete Zimmer.
  - Das passt zum Wiki: Motten schwärmen aus dunklen Zimmern; die meisten Zimmer sind leer.
  - Eingerichtete Zimmer haben Tischlampe und Deckenlicht (`level5.py`).
- **Keine Änderung.** Die übrigen Bögen 2, 3, 5–7 werden in Runde 2 mit der neuen Ausstattung geprüft.

## Runde 2 (Abnahme: Laufweg und alle Wesen, Basis cb3d808)

Laufweg `python tools/qa/walkway.py level5 --round r2 --rooms 15 --seed 3`: 687 m durch 15 Räume, 172 Bilder
(`wegbilder_r2_1.jpg` … `_5.jpg`, `wegkarte_r2.png`). Die Zimmer gehen einzeln von sehr langen Gängen ab; darum ist der Weg
für 15 Räume dreimal so lang wie verlangt (E-036 greift nicht: 80 Räume, alle erreichbar). Wesen: `tools/qa/creatureshots.py
level5 --round r2` – neu die Tabelle für Level 5 (16 Läufe: Samantha im Gang, ohne Fleisch, mit Fleisch; Kolonie, Woodlins,
Watcher, Gastgeber mit Bildnis, Büro und Ablauf; Motten-Männchen an den Birnen, Nest und Weibchen, Hound, Skin-Stealer,
Death Rats, Growler, Nguithr'xurh, Wrangler), je im Licht der Ebene und mit Lampe; Bögen `wesen_r2_1.jpg` … `_5.jpg`.

Befunde:
1. **Laufweg**: Die Gänge lesen sich als Hotel nach LOOK_5 (Damast, Nussbaum-Täfelung, Kerzenleuchter alle 4 m, Bildnisse,
   Sofas, Sessel, Bücherregale, Türen mit Messingschild, Stuckgesims); Licht warm, Rot ist die Tapete, nicht das Licht.
   Die schwarzen Bilder (032, 056, 094, 101/102, 132/133, 137/138, 143/144, 171) sind dunkle Zimmer (Wiki: Motten aus dunklen
   Zimmern) – gewollt. Aufzugshalle (000–003) weißer Marmor mit Kronleuchter; Beverly Room (071–082) groß und leer, der
   Mahjong-Tisch unter dem Kronleuchter, an den Wänden Kerzenleuchter – gut. Keine Neonschilder, kein Schmutz.
2. **Samantha** (samantha 3–5): das Weiß (Latz, Maul, Blesse) ist im Kerzenlicht grau-braun wie bei einer Maus. Offen zeigt
   das Maul von vorn das Kinn und zwei Stapel bleicher Zähne, kein Rot: der Kopf bleibt beim Aufreißen gesenkt, die Öffnung
   zeigt nach unten. Klein im Gang (samantha 1, samflow 1) – gewollt (Seite: kleine Katze). Tausch und Lesung mit Untertitel,
   Sprung (samflow 3) laufen.
3. **Woodlins** (woodlin 1–5): aus 3 bis 7 m ein dunkles, heiles Holzpüppchen neben dem Regal – Marionette. Der dünne Riss im
   Kopf ist erst ab 1,5 m mit Lampe zu sehen.
4. **Watcher** (watcher 1–6): aus dem Gang (schräg) sieht man an der Laibung nur einen schmalen, hellen Streifen mit fünf
   dunklen Punkten – liest sich wie ein Stab oder Thermometer, nicht als Kopf. Das Gesicht dreht höchstens 40° zum Spieler.
   Der Hieb (watcher 6) liest sich.
5. **Gastgeber** (host 1–5, hostflow 1–4): der Arm ist von der Tür ein gerader, schwarzer Stab, der auf den Spieler zeigt
   (Schulter, Ellbogen und Hand auf einer Linie; gleichmäßiger Kreis, Ringe, gerade Streifen) – „Rohr“. Die Augen in den
   Bildnissen sind schon aus 2,2 m nur Pünktchen, aus 3,6 m unsichtbar. Das Büro ist so hell wie ein Gang (Deckenlicht des
   leeren Zimmers), die grüne Bankerlampe fällt kaum auf. Das abgekratzte Gesicht im Bildnis des Büros liest sich als helle
   Vase auf dem Hals.
6. **Kolonie** (colony 1–4): im dunklen Zimmer ohne Lampe nichts (gewollt: sie schlucken Licht), mit Lampe schwarze, flaumige
   Kugeln in der Ecke, an den Beinen – gut.
7. **Gut**: Motten-Männchen an der Glühbirne des Kesselgangs (moth 2), Skin-Stealer in der Tür eines dunklen Zimmers,
   Growler (Pfeifenreiniger, KREATUR_Growler.md), Nguithr'xurh-Kugeln an der Decke der Aufzugskabine, Wrangler-Leib im
   Kesselgang.
8. **Werkzeug**: Nest kam nicht (die Regie ist ruhig gestellt, `presence nest` ist in Level 5 die Männchen-Art); Hound stand
   hinter dem Scherengitter der Aufzugskabine; Death Rats nur eine kleine Ratte im dunklen Eckbild; Wrangler im Kesselraum
   ohne Lampe schwarz.

Änderungen:
- **Samantha**: Weiß heller und einen Hauch kühler (`FM_ENT_SM_FURW` 1,55/1,55/1,6 – die Fellebene dunkelt etwa um die Hälfte);
  Zahnfleisch satter und nasser (`FM_ENT_SM_GUM`); beim Aufreißen hebt sich der Kopf um gut ein Drittel des Kieferwinkels
  (`creature_samantha.cpp`), die Öffnung zeigt nach vorn; Zähne dünner, spitzer, länger (`samantha.py`, neu gebaut).
- **Woodlins** (`wl_body`): der Kopf ist gespalten wie ein Scheit – zwei Hälften klaffen nach oben auseinander (V, je 0,32 rad),
  dazwischen quillt rosiges, nasses Fleisch mit Adern und Saft; rohes Holz und Splitter an den Bruchkanten; der Spalt läuft
  über Hals und Brust bis in den Bauch (heller Strich auch von weitem). Fleisch rosiger (vorher fahl wie Wachs).
- **Watcher** (`wa_head`, `wa_face`, `hotel_l5.cpp`): Lider wulsten um jede Höhle aus dem Gesicht (im Profil eine Reihe
  Knoten), Unterkiefer vorstehend, Wangenknochen spitz; 16 lange, verklebte Strähnen über die Schläfen; der Kopf kommt beim
  Lugen bis knapp vor die Wandflucht und dreht das Gesicht bis 72° zum Spieler.
- **Gastgeber**: Ärmel aus Stoff (`_sleeve`: Querschnitt breiter als hoch, unten hängt der Stoff, diagonale Stauchfalten an den
  Gelenken, Zugfalten, Nadelstreifen und Naht auf der Stofffläche, drei Hornknöpfe, Manschette mit Knopf); der Arm im Zickzack –
  erster Ellbogen hoch über der Lehne zur Seite, zweiter seitlich auf dem Tisch, der Unterarm quert zur Hand (Glieder
  0,5/0,9/0,9 m, `kE18Arm` 0,7); Augen der Bildnisse 12,5 statt 9 mm mit wundem, rotem Rand und nass schimmerndem Weiß; Büro:
  Deckenlicht auf ein Fünftel (`level5.py`, am Ende, ohne Zufall – Karte sonst gleich, geprüft); Bildnis im Büro: gemalt
  bleiben Haar, Ohren, Kinnlinie und Hals, nur das Gesicht ist unregelmäßig bis auf die Leinwand abgekratzt, Nagelkratzer
  laufen über den Rand.
- **Werkzeug**: Nest an der Tür eines dunklen Zimmers am Hauptgang erzwungen (`hiveat 54 68`), Hound aus dem Hauptgang,
  Ratten zusätzlich nah mit Lampe, Wrangler im Kesselraum mit Lampe.

## Runde 3 (`wesen_r3_1.jpg` … `_4.jpg`: die geänderten Wesen und die neuen Läufe; Laufweg unverändert)

Läufe: Samantha (Gang, ohne Fleisch), Woodlins, Watcher, Gastgeber (Bildnis, Büro, Ablauf), Nest, Hound, Death Rats,
Wrangler. Dazu eine Seitenansicht des Arms im Büro (Probe aus 1,6–2 m, Lampe an und aus).

Befunde:
1. **Samantha**: das aufgerissene Maul zeigt jetzt nach vorn – rotes Zahnfleisch, Gaumen, Zunge (samantha 4–5, samflow 3:
   der Sprung mit offenem Rachen vor dem Gesicht). Schlecht: die Zähne lesen sich als wenige lange, bleiche Blätter (wie
   Bananenscheiben) statt als Säge; das Weiß ist im Kerzenlicht immer noch mittelgrau, erst mit Lampe hell.
2. **Woodlins**: aus 1,6 m mit Lampe ein rosa Fleischbausch auf dem gespaltenen Kopf – besser, aber aus 3,5 m nur ein rosa
   Tupfen auf einer dunklen Puppe. Der Spalt muss weiter klaffen, das Fleisch mehr quellen.
3. **Watcher**: der Kopf kommt weiter vor und liegt schief, liest sich aber weiter als schmaler, langer Gegenstand mit einer
   Reihe Knöpfe (Fernbedienung, Flöte): zu schmal für ein Gesicht, der Mund ist im Schrägblick nicht zu sehen, die dünnen
   Strähnen verschwinden im Dunkel.
4. **Gastgeber**: von der Tür jetzt ein Zickzack (Ellbogen hoch über der Lehne, zweiter auf dem Tisch, Unterarm zur Hand) –
   man zählt die Glieder; das Bildnis im Büro zeigt einen gemalten Kopf mit abgekratztem Gesicht (keine Vase mehr); die
   Augen der Bildnisse leuchten aus 2,2 und 3,6 m als zwei helle, nasse Punkte (host 1–2, hostflow 1); das Büro ist dunkel,
   die grüne Lampe führt (host 3). Schlecht: der Ärmel glänzt mittelgrau wie ein Schlauch – mit den vielen Falten fängt die
   glatte Wolle (Rauheit 0,45) überall Glanzlichter.
5. **Nest**: an der Tür des dunklen Zimmers, im Lampenlicht Schichten, Harz, Säulen, Wucherung über der Tür – stark. Die
   Weibchen kamen nicht heraus (2 m vor der Tür, Lampe 6 s).
6. **Hound** im Hauptgang: bleich, abgemagert, kommt auf einen zu – gut; mit Lampe greift er sofort an (rotes Bild).
   **Death Rats**: eine Ratte in der Ecke des Kesselvorraums, klein; nah mit Lampe stand der Spieler plötzlich in der
   Aufzugshalle (die Ratte war fortgelaufen). **Wrangler** im Kesselraum: nur der Raum (er steckt bis zum Ausbruch im
   Boden – wie in Level 2 gewollt).

Änderungen:
- **Samantha**: Zähne als Säge – 14 je Reihe, kurz, dreieckig, dicht (statt 9 langer), innere Reihe jeder zweite; Weiß 1,85.
- **Woodlins**: Hälften 0,42 rad auseinander (vorher 0,32), Fleisch quillt über den Scheitel (größer, neun Wülste),
  sechs Gewebefäden durchhängend quer durch den Spalt, Saft in neun langen Bahnen über Gesicht und Hals, sechs über Brust und
  Bauch.
- **Watcher**: Kopf und Gesichtsteile 1,3-fach breiter (`WA_WIDE`), Mund weiter und offen (Rachen, 18 längere Zähne), ein
  Vorhang aus 48 nassen, schwarzen Strähnen um Seiten und Hinterkopf bis unter das Kinn.
- **Gastgeber**: Stoff stumpf (Rauheit 1,0 statt 0,45).
- **Werkzeug**: Hound mit Lampe aus 6 m (das Bild vor dem Angriff), Ratten nah mit Lampe gleich nach dem Auftritt, Nest:
  zweiter Schritt in die Tür.

## Runde 4 (alle Wesen `wesen_r4_1.jpg` … `_5.jpg`, 16 Läufe; Laufweg unverändert)

Befunde:
1. **Samantha**: das Weiß ist im Kerzenlicht hellgrau, mit Lampe weiß – liest sich als Frack-Katze. Das Maul offen und rot.
   Schlecht: von vorn sieht man jede Zahnreihe der Länge nach – je Seite ein langes, bleiches Blatt neben der Schnauze
   (samantha 4–5); die Fänge stehen über die Lippe hinaus.
2. **Woodlins**: aus 3,5 m ein dunkles Holzwesen mit aufgeplatztem, rosa Kopf (woodlin 1), nah das Fleisch zwischen den
   Hälften, Fäden, Saft über Hals und Brust (woodlin 3–4) – kein Spielzeug mehr. Die Glieder bleiben gedrechselte Stuhlbeine
   (gewollt: Wesen aus Möbelholz).
3. **Watcher**: aus der Nähe jetzt ein Gesicht – fünf Augen in wunden Wülsten, ein breiter, offener Mund mit gelben Zähnen,
   Haar an der Seite (watcher 5–6). Schlecht: der kahle, graue Schädel liest sich mit dem langen Kinn wie eine Erdnuss oder
   ein Totem, nicht wie ein Menschenkopf.
4. **Gastgeber**: der Ärmel dunkel und stumpf, Falten an den Ellbogen, Zickzack über der Lehne – liest sich als zu langer
   Arm im Anzug (host 4, hostflow 3). Büro dunkel, die grüne Lampe führt; Augen der Bildnisse auch aus 3,6 m (host 2) – gut.
5. **Nest und Weibchen**: in der Tür sticht der Schwarm (nest 5, roter Rand: Treffer); die Motten selbst sitzen zu nah am
   Auge und sind dunkel (wie in Level 3, Darstellung dort schon gerichtet). Innen Harzschichten, Gelege rechts.
6. **Gut**: Hound im Gang mit Lampe (hound 2), Ratte nah im Lampenkegel (rat 3), Skin-Stealer, Growler, Nguithr'xurh,
   Motten-Männchen an der Birne. Wrangler: in der Vorführung ohne Ausbruch nur der glatte, blasse Rücken im Kesselgang
   (Ausbruch und Zugriff sind in `KREATUR_Wrangler.md` Runde 7 belegt; im Kesselraum steckt er bis dahin im Boden).

Änderungen:
- **Samantha** (`samantha.py`): die Zähne neigen sich nach innen in den Rachen (Richtung −0,55 quer statt +0,1) – von vorn ein
  gezackter Rand rund um die Öffnung statt zweier Blätter; Fänge kürzer und nach innen.
- **Watcher** (`wa_face`): nasses, schwarzes Haar deckt Scheitel und Hinterkopf, zehn Strähnen kleben über den Wangen bis zum
  Kiefer (die Augensäule bleibt frei).
- Modellprüfung der Ebene: `ent_nest_l1/l2` (Bestand) und die neu gebauten `ent_samantha_l0..l2` mit `repair_glb.py`
  bereinigt (Umlaufsinn nach dem Vereinfachen).

## Runde 5 (Abnahme: alle Wesen `wesen_r5_1.jpg` … `_5.jpg`, Laufweg `wegbilder_r5_*`: 687 m durch 15 Räume, 172 Bilder)

Zwischenprobe (Samantha, Watcher nach Runde 4): die Zähne lasen sich immer noch als zwei lange Blätter. Ursache: die
Seitenreihen reichten 6 cm nach hinten und lagen außen an den Wangen – mit gehobenem Kopf sieht man sie der Länge nach,
gleich wie dünn die Zähne sind. *Änderung:* Reihen in die Mundhöhle (3,6 cm, nach hinten kleiner, nach innen geneigt), vorn je
Kiefer sieben spitze Schneidezähne zwischen den Fängen. Danach die ganze Tabelle und der Laufweg.

Befunde:
1. **Samantha**: offen ein roter Rachen mit gezacktem Zahnring (samantha 4–5), beim Fauchen und Sprung im Gang
   (samflow 3) das Sägemaul vor dem Gesicht; Weiß hellgrau im Kerzenlicht, weiß mit Lampe; Tausch und Lesung mit Untertitel
   (samtrade 1–4). Abgenommen.
2. **Woodlins**: der aufgeplatzte, rosa Kopf liest sich aus 3,5 und 7 m (woodlin 1, 5), nah Fleisch, Fäden, Saft (woodlin 3–4);
   nach dem Wegsehen ist es fort (woodlin 6, ins Möbel gehuscht). Abgenommen.
3. **Watcher**: schief um die Türkante ein langes, graues Gesicht mit nassem Haar, fünf Augen in wunden Wülsten und breitem
   Grinsen (watcher 5–6); aus 7 m ein heller Fleck mit dunklen Punkten an der Laibung (watcher 1) – das Gefühl, beobachtet zu
   werden, ohne dass man ihn erkennt. Hieb mit dem langen Arm (watcher 6). Abgenommen.
4. **Gastgeber**: Bildnis-Augen aus 2,2 und 3,6 m (host 1–2, hostflow 1), das dunkle Büro mit grüner Lampe und dem Arm im
   Zickzack über der Lehne (host 3, hostflow 2), Hand zum Handschlag (hostflow 3), Handel (hostflow 4). Nah von vorn ist der
   Unterarm verkürzt (er kommt auf einen zu) – gewollt. Abgenommen.
5. **Übrige Wesen**: Kolonie (schwarz im Dunkeln, mit Lampe Flaumkugeln, an den Beinen), Motten-Männchen an der Birne, Nest
   an der Tür mit Weibchen im Stock (nest 5: Stiche), Hound im Gang, Skin-Stealer in der Tür, Death Rat im Lampenkegel,
   Growler, Nguithr'xurh in der Aufzugskabine, Wrangler im Kesselgang – alle im Spiel und in ihrem Raum.
6. **Laufweg** wie Runde 2 (Bilder gleich bis auf Rauschen: mittlere Abweichung 0,19 Graustufen, höchstens 0,6) – das Büro
   liegt nicht am Weg; Raummetrik, Ähnlichkeit, Schilder grün.

Restpunkte (in `AUFWAND.md`): das Weiß der Katze ist im Kerzenlicht hellgrau; die Hand des Gastgebers wirkt nah wie eine
Gliederpuppe; der Watcher ist erst ab etwa 4 m als Gesicht zu lesen; der Wrangler zeigt in der Vorführung ohne Ausbruch nur den
glatten, blassen Rücken (Ausbruch in `KREATUR_Wrangler.md`).
