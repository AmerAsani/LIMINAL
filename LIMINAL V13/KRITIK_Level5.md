# Kritikrunden Level 5 (Abnahmeschranke d)

Laufweg `python tools/qa/walkway.py level5 --round rN --rooms 18 --seed 3`, Bögen `docs/screens/level5/wegbilder_rN_*.jpg`.
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
