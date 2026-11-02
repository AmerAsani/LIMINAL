# Kreatur: der Hausherr (Entity 18) in Level 5.3 – Besuch in der Bibliothek

Quellen (Backrooms-Wiki auf Wikidot, **CC BY-SA 3.0**, gelesen 02.11.2026, eigene Worte, keine Zitate, keine Bilder):
[Level 5.3 „Promethei Bibliotheca“](https://backrooms-wiki.wikidot.com/level-5-3) Rev. 40 (24.08.2026, Autor Praetor3005; Abschnitt
Behavior und die Kurzgeschichte am Ende) und [Entity 18 – „The Beast of Level 5“](https://backrooms-wiki.wikidot.com/entity-18) Rev. 54
(13.08.2026, Autor ratscrapz, Idee eines unbekannten Fandom-Autors). In Level 5 ist dasselbe Wesen schon als Stimme, Bildnisse, Büro
und Arm gebaut ([KREATUR_Gastgeber.md](KREATUR_Gastgeber.md)). Im Spiel heißt er nicht: im Code `Gentleman`, im Spiel „Hausherr“
(Name des Wesens), in den Untertiteln „eine ruhige Männerstimme“ bzw. „der Herr im Frack“. „Gentleman“ und „Beast“ stehen nur hier (E-039).

## Was die Seiten sagen

- **Level 5.3, Behavior:** Die Bibliothek (der „Library Mind“) ist dem Personal des Hotels feindlich; Dokumente legen nahe, dass das
  Personal sie abgeschottet hat. Die Ziele des „Beast of Level 5“ bleiben rätselhaft; vermutlich schützt er die Geheimnisse des Hotels vor
  den Menschen. Zorn zeigt die Bibliothek, indem Regale einstürzen; Aufmerksamkeit, indem Bücher fallen.
- **Level 5.3, Schlussgeschichte:** Der Herr geht wie gewohnt durch sein Reich und bemerkt am Ende eines Flurs eine kleine Holztür.
  Er erkennt den Ort, den er nie wiedersehen wollte, öffnet, tritt ein und mustert die endlosen Gänge voller Regale – angewidert. Aus dem
  nächsten Regal fällt ihm ein Notizbuch vor die Füße: Skizzen und kurze Notizen über sein Personal. Er hebt es auf, sieht es kurz durch
  und wirft es, ohne hinzusehen, hinter sich quer über den Gang. Er spricht zur Bibliothek: Er wisse, warum sie wieder da sei; es werde
  nicht gelingen, wie beim letzten Mal; sie sei hier nicht willkommen, er werde ihre Hoffnung für die Menschen wieder zerschlagen. Die
  einzige Antwort ist Stille. Er dreht sich um und geht mit demselben unbewegten Gesicht wie immer – das diesmal seinen Ärger verbirgt.
  Wissen mache die Menschen hier mutig und eigensinnig; das Personal sorge dafür, dass die Gäste einen angenehmen Aufenthalt haben –
  für immer.
- **Entity 18:** höflich, förmlich, nennt das Hotel sein Reich; sieht alles, was dort geschieht; riecht Angst und nutzt sie; zeigt sich
  nur, wem er will – wer ihn aus dem Augenwinkel sieht, den hat er schon bemerkt. Ein Aussehen beschreibt der Text nicht.

## Umsetzung

**Rolle/Entity 20 `Gentleman`** (hinter Samantha; `src/anim/creature.hpp` Namensraum `gm`, `src/gameplay/presence.hpp`),
Kin-Eintrag `gentleman` (`world::KIN_GENTLEMAN`), Level-JSON `presence.gentleman: { every: 150, count: 1 }`.

**Orte (Karte, `tools/levels/level5_3.py` am Ende, eigener Zufall):** Spawnpunkte `trace`/`gm` – `door` hinten im alten Eingangsgang
(aus dem Hotel), `stand` sein Platz in der großen Halle an der Südwand (Blick nach Westen, rechts von ihm das Regal). Gelesen von
`gameplay/library_l53.*`, an die Präsenz übergeben (`Presence::gentScene`).

**Ablauf** (`src/gameplay/presence_gentleman.cpp`; Zustände `gm::S_*`):
1. **Besuch** – einmal je Besuch der Ebene, frühestens nach `every` Sekunden (×0,85–1,25), wenn ein Spieler unten in der Halle steht
   (6–34 m vom Platz) und niemand im alten Gang ist (später auch ohne Halle). Er erscheint außer Sicht hinten im Gang.
2. **Geräusch vor Sicht** – weit hinten im Gang eine schwere alte Tür (`gm_door`), dann gemessene Schritte harter Ledersohlen auf Parkett
   (`gm_step_*`, 1,45 m/s). Er steht im Halbdunkel der alten Tür, dann tritt er ins Kerzenlicht; die Regale knarren rundum, als er die
   Halle betritt (`lib_creak` – die Bibliothek bemerkt ihn).
3. **Szene** – an seinem Platz: er mustert Gänge und Galerien, seufzt (`gm_sigh`), erkennt den Ort (Untertitel). Das Notizbuch fällt aus
   dem Regal rechts vor seine Füße (`gm_bookdrop`); er verbeugt sich steif aus der Hüfte (bis 80°, der Kopf bleibt waagrecht), der rechte
   Arm entfaltet sich zum Boden; er hebt es auf, liest (die linke Hand blättert, `gm_pages`), im gemalten Gesicht öffnet sich ein Riss
   (`gm_crack`); er wirft es über die Schulter hinter sich, ohne hinzusehen (`gm_throw`) – es fliegt im Bogen 4,6 m und landet (`gm_land`).
4. **Ansprache** – drei Sätze in den Raum, beide Arme halb geöffnet (Untertitel in eigenen Worten); dazwischen **Stille**: die Klangbetten
   der Bibliothek verstummen (`gm_hush`, Kanal `glow` → `Library53::hush` → `app.cpp` setBedDuck/setQuiet).
5. **Rückzug** – er dreht sich gemessen um und geht durch den Gang zurück; ungesehen am Ende (oder nach 10 s: das Licht flackert) ist er
   fort (`gm_gone`). Fern und 2 s ungesehen ist er ebenfalls fort.
6. **Zorn der Bibliothek** – sobald er 9 m von seinem Platz ist: Holz ächzt lange, ein Brett bricht, Bücher stürzen in Kaskaden
   (`lib_wrath`); vor dem Regal rechts von seinem Platz liegen zwei Haufen herabgestürzter Bücher (`books_fallen`, `session_library.cpp`).
7. **Das Notizbuch** wird dort, wo es landet, ein **Fund** („Notizbuch mit Skizzen“: Pagen, Zimmermädchen, Portier mit demselben glatten
   Gesicht, Notizen, dass sie Gäste nicht gehen lassen; auf der letzten Seite ein großer Mann im Frack mit leerem Rahmen statt Gesicht).

**Störung** (nur in der Szene, einmal je Besuch): wer ihm näher als 3 m kommt, ihm 2,2 s ins Gesicht leuchtet (nach 0,6 s eine Mahnung
„Nehmen Sie das Licht herunter“) oder das Notizbuch aufhebt, solange er da ist – er bringt zu Ende, was er tut (Buch fällt, aufheben,
lesen, werfen), dann:
- **Wahrnehmen** – der Kopf dreht sich langsam bis 166° (der Hals streckt sich dabei), der Körper bleibt; „Ein Gast. Hier unten.“ –
  dann wendet er sich zu (2,4 s).
- **Jagen** – er kommt: 1,7 m/s, solange das Ziel ihn ansieht, 4,0 m/s, sobald es wegsieht (der Spieler geht 3, rennt 5,7). Ihn
  ansehen, während er dich ansieht, zehrt am Verstand (−1/s bis 10 m). Ab 16 m oder 5 s ohne Sicht lässt er ab.
- **Ankündigung** – ab 2,1 m bleibt er stehen; der Arm hebt sich Glied für Glied zum Gesicht (1,5 s), Gelenke knacken, „Erlauben Sie.“
  (beim ersten Mal der Rat „Abstand!“). Wer über 3,4 m weicht oder aus der Sicht geht, ist gerettet.
- **Griff** – noch unter 2,5 m: ein langer Schritt bis 0,85 m, die Handfläche 38 cm vor dem Auge, die Finger wachsen (Maßstab 1,18) und
  greifen um das Bild; der Blick wird in sein Gesicht gezogen; „Vergessen Sie, was Sie hier gelesen haben.“ – 14 Schaden (× Schwierigkeit),
  −28 Verstand; **nie tödlich**.
- **Revers** – er zieht die Revers glatt („Wie Sie wünschen. Genießen Sie Ihren Aufenthalt.“) und geht (Rückzug wie oben).

**Modell** (`tools/blender/creatures/gentleman.py`, `data/models/ent_gentleman_l0..l2.glb`, 42 000 / 14 000 / 5 000 Dreiecke):
Rumpf, Kopf, Beine und Hände aus dem Baukasten der Menschenwesen (`human.py`) auf einem Ersatz-Zweibeiner-Skelett, Knochen auf `gm`
umgeschrieben. 2,1 m, schmal, Hals eine Handbreit zu lang; Frack aus dunklem Kammgarn mit Revers und Rockschößen bis in die Kniekehlen
(eigene Knochen, schwingen nach), Weste ochsenblutrot mit vier Goldknöpfen und Uhrkette, gestärkte Hemdbrust, hoher Kragen, schwarze
Seidenkrawatte mit fallenden Enden, Hose mit Bügelfalte, Lackschuhe mit langer spitzer Kappe. **Arme mit drei Gliedern** (zwei Ellbogen,
beide mit Stauchfalten), sie hängen bis unter die Knie; Hände wachsbleich, Finger um ein Glied zu lang, gelbe Nägel, Siegelring am
kleinen Finger rechts, Manschettenknöpfe. **Das Gesicht wie gemalt**: glatte, ovale Fläche, flache Nase, keine Lider, gemalte Brauen
(zu dünn, zu gerade) und Lippen, ein Netz aus Rissen im Firnis (Craquelure, Verschiebung); die **nassen, echten Augen** sitzen lidlos in
Löchern mit rohem, nassem Rand. Von der rechten Schläfe über das Auge der **Riss** (Knochen 7, Maßstab 0..1): rohes Fleisch quillt aus dem
Firnis, die Ränder blättern auf. Das **Notizbuch** (Knochen 26): schwarzes Leder, Seitenblock.

**Materialien** (`FM_ENT_GM_*`, `world_renderer.cpp`): Frack Kammgarn stumpf (Rauheit 1,0), Weste ochsenblutrot mit leichtem
Seidenglanz (0,5), Hemd Baumwolle (0,55), Lackschuhe Spiegelglanz (0,1), Gesicht Zahnschmelz-Ebene (vergilbt, Flecken) mit
Haarriss-Mikroebene (0,4), Hände Menschenhaut wachsbleich (0,45), Haar pomadisiert (0,28), Augen nass (0,03/0,05), Riss rohes nasses
Fleisch (0,12), Gold Metall (0,3), Notizbuch Leder (0,55).

**Skelett und Bewegung** (`src/anim/creature_gentleman.cpp`): 35 Gelenke, 27 Knochen. Gang gemessen und aufrecht (lange Schritte,
kaum Hub, Rumpf eine Spur zurück), die Hände auf dem Rücken verschränkt (alert 0) oder die zu langen Arme hängen und pendeln (alert 1);
Umsehen ohne Blickziel (Gänge, Galerien); Seufzer (shiver); Wahrnehmen bis 166° (perceive); in das Licht sehen statt zurückzucken
(light); Verbeugung und Notizbuch über den Kanal `fly` (Phase/6: fällt, liegt, aufheben, lesen, Wurf und Flug, liegt hinter ihm);
Ansprache (plea, talk – der Kiefer bewegt sich kaum); Riss (growl → Maßstab Knochen 7); Griff (reach/reachAt, strike → Maßstab der
Hand); Revers (wave). Arme: FABRIK über drei Glieder aus der jeweiligen Grundhaltung (so bleibt die Beugerichtung erhalten).
Animationen nach Auftrag: **Idle** (Umsehen, Seufzer), **Wahrnehmen** (Kopf bis 166°), **Jagen** (Kommen, Arme frei), **Angriff**
(Ankündigung, Griff), **Rückzug** (Revers, Umdrehen, Gehen), **Verschwinden** (Gangende: ungesehen fort bzw. Flackern).

**Klang** (`src/audio/synth_gentleman.cpp`, Klangnummern ab 700 über `Presence::Event Mimic`, `gameplay/library_l53.cpp`): ruhige,
tiefe Stimme (f0 ~92 Hz), kaum Zittern, leicht doppelt (zweite Stimme 0,4 % verstimmt), mit Hall der Halle; Schritte (Absatz, Sohle,
manchmal ein nachgebendes Brett); Halswirbel knacken beim Wahrnehmen; Gelenke knacken und Stoff spannt sich bei der Ankündigung; der
Griff geflüstert mit kaltem Atem; Firnis knistert; Notizbuch: Aufschlag, Blättern, Wurf, Landung; Tür fern (Klinke, Angel); Bibliothek:
Regale knarren, Stille (Druckabfall, Ton im Ohr), Zorn (Ächzen, Brett bricht, Kaskade).

**Mehrspieler:** Der Host rechnet alles (Regie, Szene, Störung, Schaden für das Ziel als `Hit`/`Whisper`). Die Puppe beim Gast bekommt
Rolle 20, Zustand und die Kanäle (`fly` Notizbuch, `glow` Stille, `reach`/`strike` Griff, `perceive`, `plea`, `wave`, `talk`, `growl`);
Sätze kommen über `kinNetRoute` als benannte Klänge mit Untertitel. Der Fund (Notizbuch) entsteht auf jedem Rechner aus der Puppe
(Phase 5..6 → 0) an derselben Stelle der Karte; der Bücherhaufen aus dem Weg der Puppe; Aufheben entscheidet der Host wie bei allen
Notizen. Protokoll unverändert.

**Skript:** `presence gentleman` (erzwingen), `gent <quick|next|hold|pose|book|place|provoke|stand|state|yaw> <wert>`,
`near gentleman <abstand>`, `figure gentleman <abstand>`.

## Interpretation (begründet)

- **Gestalt:** Keine der Seiten beschreibt ihn. Gebaut ist, was die Seiten tragen: ein Herr („Gentleman“, förmlich, höflich) mit
  „unbewegtem Gesicht, das er immer zeigt“ – als gemaltes Bildnis-Gesicht, weil er laut Entity 18 die Bildnisse des Hotels benutzt und in
  Level 5 das Bildnis im Büro ein abgekratztes Gesicht trägt. Der Ärger, den das Gesicht verbirgt, wird als Riss im Firnis sichtbar. Die
  Arme mit drei Gliedern folgen dem Arm aus dem Sessel in Level 5 (dort Auslegung von „Gastgeber“ und „Biest“).
- **Begegnung statt Erzählung:** Die Schlussgeschichte wird als Szene gespielt, die der Spieler beobachten kann – Tür, Blick in die Gänge,
  fallendes Notizbuch, Wurf ohne hinzusehen, Ansprache, Stille, Abgang. Ort: die Seite nennt eine kleine Holztür am Ende eines Flurs; das ist
  in der Karte die alte Tür des Eingangsgangs (der Gang gehört zum Hotel).
- **Zorn der Bibliothek:** Behavior nennt einstürzende Regale als Zorn und Feindschaft gegen das Personal; nach seinem Besuch fällt sie aus.
- **Gefahr:** Die Seiten nennen keine Gewalt; Entity 18 sieht alles, riecht Angst und nutzt sie. Darum kein tödlicher Angriff: nur wer ihn
  stört, wird bemerkt, gejagt und gegriffen – vor allem der Verstand leidet. „Wer ihn aus dem Augenwinkel sieht, den hat er schon bemerkt“
  → schnell, sobald man wegsieht; der Kopf, der sich zu weit dreht.
- **Notizbuch als Fund:** Die Seite sagt, was darin steht (Skizzen und Notizen über sein Personal); der Wortlaut ist eigen; das Gesicht als
  leerer Rahmen auf der letzten Seite ist Auslegung (die Bildnisse).

## Zahlen

| Wert | |
|---|---|
| Besuch | einmal je Ebene, ab `every` 150 s (×0,85–1,25), Spieler in der Halle 6–34 m vom Platz, niemand im Gang |
| Gehen | 1,45 m/s (Gang 24 m, Halle 10 m → etwa 24 s bis zum Platz) |
| Szene | Umsehen 5,6 s, Fall 0,55 s, Aufheben 2,4 s, Lesen 3,2 s, Wurf 1,25 s (+0,5 s), Ansprache 22,6 s (Stille 11,4–16,2 s) |
| Störung | näher als 3 m, Licht 2,2 s (bis 12 m), Notizbuch genommen |
| Wahrnehmen | 2,4 s, Kopf bis 166° |
| Jagen | 1,7 m/s gesehen, 4,0 m/s ungesehen; Blick −1 Verstand/s (bis 10 m); lässt ab ab 16 m oder 5 s ohne Sicht |
| Ankündigung | ab 2,1 m, 1,5 s; Ausweichen über 3,4 m |
| Griff | unter 2,5 m: Schritt bis 0,85 m, 14 Schaden × Schwierigkeit (Health heilt nach), −28 Verstand, nie tödlich, einmal je Besuch |
| Zuerst gesehen | −1 Verstand (FirstSight) und −2,4 (er sieht alles) |

Gemessen im `gameplay_test` (1,9 m vor ihm, Schwierigkeit „medium“): bemerkt nach 0,02 s, Ankündigung nach 2,45 s, Griff nach 3,97 s;
Gesundheit 100 → 86,0 (heilt nach), Verstand 99 → 62,6.

## Wow-Check

„IIIH, was ist das?“ – Aus dem Halbdunkel der alten Tür tritt ein über zwei Meter großer Herr im Frack, dessen Gesicht ein gesprungenes,
gemaltes Porzellanbildnis ist, in dem echte, nasse Augen ohne Lider rollen; er bückt sich nicht, er klappt in der Hüfte zusammen, der
Arm faltet sich an zwei Ellbogen zum Boden – und wenn man ihn stört, dreht sich der Kopf über die Schulter nach hinten, der Riss im
Gesicht klafft rot, und die viel zu lange Hand wächst einem ins Gesicht. Ehrliche Antwort: nicht „ganz okay“ – das gemalte, rissige
Gesicht mit lebendigen Augen und der Kopf, der sich nach hinten dreht, sind der Schreck; der Körper im Frack ist bewusst ruhig.

## Kritikrunden

Bilder: `docs/screens/creatures/gentleman/`.

**Runde 0 (Blender)** – `r0_blender_puppe_bogen.jpg`. *Schlecht:* aus Ellipsoiden gesetzt eine Schaufensterpuppe – gestapelte Eier,
Kugelgelenke, die Weste als roter Bauch 2,5 cm vor dem Frack, die Revers als schwarze Zungen in der Öffnung. *Änderung:* Rumpf, Kopf,
Gesicht, Beine, Hände aus `human.py` auf einem Ersatz-Zweibeiner, Knochen umgeschrieben (`r0_blender_mensch_bogen.jpg`); Weste innen,
Revers flach auf der Brust, Polster kleiner.

**Runde 1 (Spiel)** – `r1_szene_bogen.jpg`, `r1_studio_bogen.jpg`, `r1_gesicht_nah.jpg`. *Gut:* die Szene läuft ganz durch (Tür, Platz,
Wurf, Fund, Zorn); im Halbdunkel der Tür eine Silhouette. *Schlecht:* die Szene geschah außerhalb des Bilds (Blickrichtung); das Gesicht
ein gewöhnliches Menschengesicht, im Kerzenlicht braun, im Lampenlicht eine Puppe – „ganz okay“; die Hemdbrust ein weißes Ei.
*Änderung:* Gesicht als gemaltes Bildnis (glatt, flache Nase, ohne Lider, gemalte Brauen und Lippen, Craquelure, Zahnschmelz-Ebene),
Krawatte mit fallenden Enden; Studio-Skript mit freier Kamera an seinem Platz.

**Runde 2** – `r2_bogen.jpg`, `r2_gesicht_nah.jpg`, `r2_posen_bogen.jpg`. *Gut:* das Porzellangesicht mit roten Lippen ist unheimlich;
Hände auf dem Rücken, Ansprache mit offenen Armen. *Schlecht:* die Risse machen das Gesicht klumpig wie Teig; nur die Iris war zu sehen;
im Lampenlicht überstrahlt; der Frack glänzte wie Satin; das schwarze Notizbuch vor dem Frack unsichtbar. *Änderung:* schärfere, feinere
Rissnetze (Gesicht mit 0,9-mm-Voxeln), Augen weiter vor, Albedo des Gesichts 0,72, Frack stumpf, Lesehaltung höher (vor dem Gesicht).

**Runde 3** – `r3_gesicht_nah.jpg`, `r3_bogen.jpg`, `r3_ablauf_bogen.jpg`. *Gut:* Ablauf aus fester Kamera: Umsehen, Blick zum Buch,
steife Verbeugung mit Arm zum Boden, Lesen mit blätternder Hand, Wurf über die Schulter, Ansprache – alles nach der Seite. *Schlecht:*
Augen noch dunkle Punkte; der Riss klein. *Änderung:* Augen weiter vor (Weiß sichtbar), Riss breiter mit größeren Schuppen. (Dabei im
Test gefunden: die Zustände 9/10 der Störung bedeuten in der Präsenz für alle Wesen „abgehen“/„tödlicher Zugriff“ – das Bemerken
entfernte ihn; jetzt 11–15.)

**Runde 4** – `r4_wahrnehmen_griff_bogen.jpg`, `r4_riss_wissen_nach_bogen.jpg`. *Gut:* der Kopf dreht sich zum Spieler hinter ihm über
den Rücken (von vorn sieht man den Hinterkopf auf dem zugewandten Körper); Notizbuch als Fund, Bücherhaufen nach dem Zorn. *Schlecht:*
der Griff erreichte aus 1,9 m das Gesicht nicht (Arm 1,1 m) – aus der Spielersicht hing die Hand vor seiner Brust. *Änderung:* langer
Schritt bis 0,85 m im Griff, Arm hebt sich schon in der Ankündigung zum Gesicht.

**Runde 5 (Abnahme)** – `r5_griff_spielersicht_bogen.jpg`, `r5_bogen.jpg`, `r5_griff_riss_nach_bogen.jpg`, `r5_wahrnehmen_riss.jpg`,
`r5_tuer_halbdunkel_bogen.jpg`, `r5_ablauf_bogen.jpg`; beste Ansichten `r5_tuer_halbdunkel.jpg` (Silhouette in der Tür),
`r5_aufheben.jpg` (Verbeugung), `r5_wurf.jpg` (das Buch fliegt), `r5_ansprache.jpg`, `r5_wahrnehmen_spieler.jpg`,
`r5_griff_spieler.jpg` (die Hand vor dem Gesicht, darüber sein rissiges Gesicht), `r5_riss_nah.jpg`, `r5_nah.jpg`/`r5_nah_lampe.jpg`,
`r5_mittel.jpg`/`r5_mittel_lampe.jpg`, `r5_fern.jpg`, `r5_notizbuch_fund.jpg`, `r5_zorn_buecherhaufen.jpg`. Zwischenstand Runde 5:
die Hand fuhr beim Griff durch die Kamera und füllte das Bild braun – Handfläche 38 cm vor dem Auge, Finger 1,18, Hände heller. Danach
abgenommen.

## Tests (`gameplay_test`, `testV133Level53Gentleman`)

Szene und Wissen in der Karte; Kin-Eintrag; drei Funde; kein Wiki-Name in Spieltext, Untertiteln und Namen; Sätze mit Untertitel und Rat;
Funde und Modelle vorhanden; **Besuch**: Regie (Spieler in der Halle), Auftritt im Gang, Tür vor Sicht, Gang an seinen Platz, Seufzer,
Notizbuch fällt, Wurf, Landung, Fund 4,6 m hinter ihm, drei Sätze mit Stille, Stille > 0,9, Rückzug, Zorn (Klang, Bücherhaufen), kein
Angriff ohne Störung, einmal je Besuch; **Störung** (1,9 m): bemerkt, Ankündigung 1,5 s vor dem Griff, Sätze, Schaden ohne Tod, viel
Verstand, danach Rückzug; Netzzustand Rolle 20 und Puppe beim Gast mit Zustand; **Ausweichen** in der Ankündigung rettet; **Licht** im
Gesicht: Mahnung, dann bemerkt.

## Restmängel

- Der Frack hat keine Nadelstreifen (der Ärmel in Level 5 hat sie): die Kreaturenmaterialien sind Triplanar-Ebenen ohne Streifen.
- Das gemalte Gesicht zeigt die Craquelure als Relief und Fleckenebene; gemalte Schatten (Wangenrot, Lichter) gibt es nicht.
- Kommt ein Gast erst nach der Ansprache dazu (Puppe nie in der Ansprache gesehen), fehlt bei ihm der Fund „Notizbuch“ (der Host hat
  ihn; Aufheben über den Host). Wer während der Ansprache dazukommt, bekommt ihn.
- Der Wurf trifft keine Hindernisse: der Platz in der Karte ist so gewählt, dass 4,6 m hinter ihm freier Boden ist.
