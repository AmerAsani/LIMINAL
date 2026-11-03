# Kreatur: der Horcher – die „anderen bösartigen Menschenähnlichen“ der Gänge von Level 188

Quellen (Backrooms-Wiki auf Wikidot, **CC BY-SA 3.0**, gelesen 03.11.2026, eigene Worte, keine Zitate, keine Bilder):
[Level 188 „The Courtyard of Windows“](https://backrooms-wiki.wikidot.com/level-188) Rev. 69 (24.10.2026; ursprünglich 1000dumplings,
neu geschrieben von Kai4C), Abschnitte *Corridors*, *Warning: Neurologic Agents*, Logs #2, #4 und #6. Die Seite nennt in den Gängen
neben den Facelings im rosa Kleid und den Skin-Stealern „andere bösartige menschenähnliche Lebensformen“ – **ohne Namen, ohne Link,
ohne Beschreibung**. Der Horcher ist deshalb eine **gekennzeichnete Auslegung** (Auftrag V13.3 Teil 4: „als begründete Auslegung
bauen“). Name im Spiel: „Horcher“ (eigene Benennung); im Code `Listener`.

## Was die Seite sagt (eigene Worte)

- **Corridors:** klinisch weiße, grell beleuchtete Gänge über mehrere Etagen um den Hof, Treppenhäuser, die wandern; die **meisten Türen
  sind von innen verschlossen**; zugängliche Räume sind leere weiße Betonquadrate. In den Gängen leben ungebetene Bewohner: Facelings im
  rosa Kleid, Skin-Stealer und andere bösartige menschenähnliche Wesen.
- **Log #2:** lautes, **rhythmisches Hämmern**, Rascheln, Knarren, Schritte – ohne dass jemand zu sehen ist.
- **Log #4:** ein Schatten **tritt hinter Fred aus einer Tür**; ein anderer ist zu schnell, um ihn zu fassen.
- **Log #6:** Musik, Panik, schweres Atmen; Fred flüstert über etwas, das er nicht sieht.
- **Warnung (Isolationsprotokoll):** wer nicht entkommt, soll Sehen, Hören und Berühren abschirmen, **still** bleiben und auf nichts
  reagieren.

## Auslegung (begründet)

Aus diesen vier Belegen ist ein Wesen gebaut, das **hört statt sieht**: Es wohnt hinter den verschlossenen Türen (die meisten Türen sind
von innen verschlossen – es ist das, was sie verschließt), es **klopft** (das rhythmische Hämmern aus Log 2) und **lauscht**, ob jemand
antwortet; wer antwortet, hinter dem **tritt es aus einer Tür** (Log 4). Wer **still bleibt**, dem passiert nichts (die Schutzregel der
Seite wird zur Spielregel). Gestalt: menschenähnlich, aber nicht menschlich – unterscheidbar von Faceling (Mensch ohne Gesicht) und
Skin-Stealer (Menschenhaut über einem Wesen): ein **ausgezehrter, kreideweißer Leib** (weiß wie die Gänge), gebückt; **statt eines Kopfes
ein riesiges Ohr** (0,62 m) am Ende eines viel zu langen Halses; der Gehörgang ist der Mund (nass, rot, zwei Ringe kleiner gelber Zähne);
im Ohrläppchen ein **verkümmertes Gesicht** mit geschlossenen Augen und zugewachsenem Mund; der rechte Arm zu einer **Klopf-Faust**
ausgewachsen (Unterarm dicker als ein Oberschenkel), die Knöchel schwielig, aufgeplatzt, **mit Splittern dunklen Mahagonis** von den
Türen; der linke Arm dünn mit zu langen Fingern und gelben Nägeln, mit dem es die Muschel lenkt. Alles darüber hinaus ist Auslegung.

## Umsetzung

**Rolle/Entity 21 `Listener`** (hinter dem Hausherrn; `src/anim/creature.hpp` Namensraum `hr`, `src/gameplay/presence.hpp`),
Kin-Eintrag `listener` (`world::KIN_LISTENER`), Level-JSON `presence.listener: { every: 70, rooms: [gang, zelle, treppenhaus] }`.

| Teil | Datei |
|---|---|
| Skelett, Bewegung (drei Stützen: Faust, zwei Füße; Ohr schwenkt, lenkt, knittert; Aufbäumen, Schlag) | `src/anim/creature_listener.cpp` |
| Modell (Blender, 3 Stufen 44 000 / 15 000 / 5 200 Dreiecke) | `tools/blender/creatures/listener.py` → `data/models/ent_listener_l0..l2.glb` |
| Materialien `FM_ENT_HR_*` (Haut, Ohr, Gehörgang, Schwiele, Wunde, Nagel, Ader, Splitter) | `src/render/world_renderer.hpp/.cpp`, `entity_models.cpp` |
| Regie (Klopfen, Lauschen, Antwort, Heraustreten), Denken (Lauschen, Schleichen, Jagen, Ankündigung, Schlag, Licht, Rückzug) | `src/gameplay/presence_listener.cpp` |
| Klangnummern, Untertitel | `src/gameplay/listener_l188.hpp` (ab 800, `kinLine`) |
| Klänge | `src/audio/synth_listener.cpp` (`addListenerSounds`) |
| Orte (Spawnpunkte `trace/hr`: `knock` vor jeder verschlossenen Tür, `emerge` hinter jeder Zellentür) | `tools/levels/level188.py` (am Ende ergänzt), `gameplay/courtyard_l188.*`, `session_courtyard.cpp` |
| Bilder, Skriptbefehl `horcher <quick|next|emerge|hold|pose|place|state|yaw> <wert>` | `tools/qa/shots_l188.py`, `src/app/app.cpp` |

**Ablauf.** Wer in den Gängen ist (Gang, Zellen, Treppenhaus), hört nach 17–31 s hinter einer verschlossenen Tür 2,5–15 m entfernt
klopfen – 3 bis 5 schwere Schläge in einem von drei Takten (gleichmäßig, zwei-und-drei, schleppend). Danach lauscht es 3,5 s.
- **Stille** (stehen, schleichen weiter als 2,3 m): hinter der Tür entfernt sich etwas; nach drei stillen Runden lässt es 112 s ab.
- **Antwort** (gehen/rennen bis 16 m von der Tür, an die Tür treten, ein Aufprall nahe der Tür): ein nasses Saugen hinter dem Holz; 2,4–3,6 s
  später geht **hinter dir** eine Zellentür auf (Klinke, Knarren) – es tritt aus einer Zelle, die du nicht siehst, 4–18 m entfernt, lieber
  hinter dir. Es entfaltet sich tief geduckt, das Ohr voran.
- **Draußen** ist es blind und hört: Rennen 28 m, harte Landung 20 m, Gehen 9 m, Schleichen 2,3 m, Stillstehen nichts; Aufpralle geworfener
  Dinge und der **Knall der Wurzelpistole** (Laermereignis, siehe `KREATUR_Wurzelpistole.md`) locken es nach Lautstärke. Es schleicht zum Ort
  des Geräuschs (1,3 m/s, Brummen), bei Lärm oder näher als 4 m jagt es (4,4 m/s, Pfeifen durch den Gehörgang – langsamer als ein rennender
  Mensch). Am Ort tastet die Faust über den Boden und klopft kurz.
- **Ankündigung** bis 2,4 m (nur solange es dich in der letzten Sekunde gehört hat): es richtet sich auf, die Faust hoch über dem Kopf, drei
  Schläge in die Luft, ein steigendes Zischen (1,3 s) – dann fährt die Faust auf den Ort, an dem es dich **zuletzt gehört** hat. Wer seitdem
  leise mehr als 1 m weg ist, wird verfehlt (die Faust schlägt auf den Beton); sonst 34 Schaden × Schwierigkeit. Ganz nah (unter 0,9 m)
  ertastet die dünne Hand auch, wer still steht.
- **Licht:** das Ohr ist dünne Haut über Knorpel – 0,35 s Lampe darauf (bis 9 m): es kreischt, der Rand knittert, die Hand deckt den
  Gehörgang, 1,6 s hält es inne (auch in der Ankündigung: kein Schlag). Dreimal in einer Minute – dann gewöhnt es sich (60 s).
- **Grenzen:** in den Hof folgt es nicht (es bleibt an der Tür und lauscht). Nach 25 s Stille oder wenn das Ziel 5 s im Hof ist, zieht es
  sich durch die nächste Zellentür zurück und ist fort, sobald ungesehen; hört es auf dem Rückweg etwas in 9 m, kehrt es um.
- **Takt:** Facelings und Skin-Stealer in 18 m haben Vorrang (es klopft nicht, während sie nahe sind); es gibt immer nur einen.

**Mehrspieler.** Der Host rechnet Regie und Denken; die Puppe beim Gast bekommt Rolle 21, Lage, Zustand und die Kanäle crouch, alert,
perceive, shiver, jaw, growl, reach, wave, strike, light über `Presence::NetState` (Protokoll unverändert). Klänge als Klangnummern über
`kinNetRoute`.

## Zahlen

| Wert | |
|---|---|
| Klopfen | `every` 70 s: erste Runde nach 17–31 s, nach Antwort 35–56 s, nach Stille 21–35 s, nach drei stillen 112 s; Tür 2,5–15 m, gleiche Etage |
| Lauschen | 3,5 s nach dem letzten Schlag; Antwort: Lärm ≥ 0,4 bis 16 m, an der Tür unter 1,3 m, Aufprall bis 6 + 10 × Stärke m |
| Heraustreten | 2,4–3,6 s nach der Antwort, Zellentür 4–18 m, ungesehen (sonst bis 8 s weiter versuchen), 0,8 m/s heraus |
| Hören | Rennen 28 m, Landung 20 m, Gehen 9 m, Schleichen 2,3 m; Aufprall 30 × Stärke m; Hand unter 0,9 m nach 0,4 s |
| Tempo | Schleichen 1,3 m/s, Jagen 4,4 m/s (Spieler: Gehen 3, Rennen 5,7) |
| Ankündigung / Schlag | ab 2,4 m, 1,3 s; Treffer, wenn das Ziel unter 1 m vom zuletzt gehörten Ort und unter 2,7 m vom Horcher steht; 34 × Schwierigkeit |
| Licht | 0,35 s bis 9 m → 1,6 s Innehalten; ab dem dritten Mal in 60 s 60 s Gewöhnung |
| Rückzug | 25 s Stille, Ziel 5 s im Hof; fort bei der Tür, sobald 0,5 s ungesehen (spätestens 14 s) |

Gemessen im `gameplay_test` (Spieler geht 2,2 m vor ihm auf und ab, Schwierigkeit „medium“): Ankündigung nach 0,68 s, Treffer nach
2,32 s (1,64 s nach der Ankündigung), Gesundheit 100 → 66 (heilt nach). Klopfen in einer Runde: 5 Schläge.

## Klang (`synth_listener.cpp`)

Klopfen: schwere, knochige Faust auf massivem Mahagoni von der anderen Seite (Holzkörper, Blatt schwingt im Rahmen, Klinke klappert nach,
nasser Anteil der aufgeplatzten Knöchel), drei Varianten. Lauschen: eingesogene Luft durch den Gehörgang, nasses Schmatzen und Klicken.
Stille: Schritte, die sich hinter der Tür entfernen. Tür: Klinke, langsames Knarren. Entfalten: Gelenke, Knöchel auf Beton. Gehen: Faust
(schwer, Knochen auf Beton), dann zwei nackte Füße (klatschend), vier Varianten. Stimme: kein Mund im Gesicht – pfeifend und blubbernd aus
dem Gehörgang (Kehlsack, Klicken): Atmen, Brummen beim Schleichen, Schnalzen beim Hören, Pfeifen beim Jagen. Ankündigung: drei Schläge in
die Luft, steigendes Zischen. Schlag: Treffer bzw. Faust auf Beton. Licht: hohes Kreischen, Knorpel knistert. Rückzug: eine Tür schließt
sich leise. Untertitel ohne Wiki-Namen, Rat der Reisenden beim Klopfen („Nicht antworten …“) und bei der Ankündigung.

## Gleichgewicht

Fair: niemand erscheint im Blickfeld (Zellentür ungesehen, 4 m Mindestabstand); jedes Erscheinen kündigt sich an (Klopfen → Saugen → Tür
hinter dir), jeder Schlag 1,3 s vorher (Aufbäumen, drei Schläge, Zischen) und trifft nur, wo es dich zuletzt gehört hat. Die Gegenwehr ist
die der Seite: still sein; leise weggehen; Licht auf das Ohr. Das Wesen ist langsamer als ein Rennender, aber Rennen ist laut.

## Wow-Check

„IIIH, was ist das?“ – Hinter einer verschlossenen Tür klopft es, du hältst die Luft an; dann geht hinter dir eine Zellentür auf, und
aus dem Spalt schiebt sich zuerst ein **riesiges, rosiges Ohr**, dann ein kreideweißer, gebückter Leib, der auf einer übergroßen,
blutig aufgeschlagenen Faust geht; der Gehörgang ist ein nasses rotes Loch voller kleiner gelber Zähne, und im Ohrläppchen schläft ein
winziges Gesicht. Ehrliche Antwort: nicht „ganz okay“ – das Ohr als Kopf, das sich nach jedem Geräusch dreht, und die Faust, die
sich in der Ankündigung über dem Ohr aufrichtet, sind der Schreck.

## Kritikrunden

Bilder: `docs/screens/creatures/horcher/` (Bögen aus `tools/qa/shots_l188.py`; je Runde nah/mittel/fern, mit und ohne Lampe, Posen
Lauschen, Schleichen, Ankündigung, Licht, Jagen, Schlag, im Gang der Ebene).

**Runde 1** – `r1.jpg`. *Gut:* das Ohr liest sich von vorn sofort als Ohr, der rote Gehörgang mit Zähnen wie ein Auge/Mund; der lange Hals
und der gebückte Leib. *Schlecht:* die Haut braun gefleckt wie ein Tarnmuster (Texturebene `EL_PALE` + grobe Mulden in der Höhlung); der
Klopf-Arm ein gleich dicker Zylinder mit einem Quader als Faust – liest sich wie eine Säule/Prothese; das Ohr braun wie Leder.
*Änderung:* Haut feiner und flacher verschoben, Höhlung schwächer; Faust als gewölbte Mittelhand mit vier geschwollenen Knöcheln,
eingeschlagenen Fingern, Daumen, Sehnen; Unterarm mit Muskelbäuchen unter dem Ellbogen und schmalem Handgelenk; Ohr heller.

**Runde 2** – `r2.jpg`. *Gut:* Faust und Unterarm lesen sich als Arm; Rippen und Wirbel. *Schlecht:* mit der Menschenhaut-Ebene
(`EL_HUMAN`) ein rosiger, glatter Leib – eine nackte Puppe; Schwielen dunkel wie ein Huf. *Änderung:* Haut kreidig, trocken, rissig
(`EL_TOOTH` + `ML_CRAZE`, Albedo 0,80), Schwielen gelblich-grau.

**Runde 3** – `r3.jpg`. *Gut:* kreideweiß wie die Gänge, das rosige Ohr setzt sich ab; Ankündigung (Faust über dem Ohr) und Licht
(Hand deckt den Gehörgang) lesen sich. *Schlecht:* die Füße nackt und sauber; das Auftrittsbild zeigte eine leere Wand (Kamera). Beim
Test gefunden: es blieb in der Zellentür stecken (Türkacheln gehören keinem Raum – „nicht in den Hof“ verbot sie). *Änderung:* Hornhaut
an Ferse und Ballen; nur der Hof ist verboten; eigene Auftrittsszene.

**Runde 4** – `r4.jpg`, `r4_auftritt.jpg`. *Gut:* Auftritt wie gewollt: aus der Zellentür schiebt sich zuerst der Rand des Ohrs, dann
entfaltet es sich geduckt in den Gang. *Schlecht:* im Hintergrund die Wurzelpistole im Gangboden als schwarze Spinne (siehe
`KREATUR_Wurzelpistole.md`). *Änderung:* dort.

**Runde 5 (Abnahme)** – `r5.jpg`, `r5_auftritt.jpg`; beste Ansichten `r5.jpg` Bilder 1 (Lauschen im Ebenenlicht), 4 (Ankündigung),
5 (Licht auf dem Ohr), 7 (fern im Gang), `r5_auftritt.jpg` Bild 2/3 (das Ohr in der Tür). Ohne neuen Mangel abgenommen; Restmangel siehe
unten.

## Tests (`gameplay_test`, `testV133Level188`; schnell: `gameplay_test data level188`)

Orte in der Karte (5 Klopftüren, 3 Zellentüren), Kin-Eintrag, Modelle, kein Wiki-Name in Spieltext/Untertiteln/Namen (E-039),
Klänge mit Untertitel und Rat; **Stille**: eine Runde, 3–5 Schläge, es lässt ab, niemand kommt; **Antwort** (gehen): Saugen, Heraustreten
aus einer Zelle, Tür vor Sicht, mindestens 4 m; **Knall der Wurzelpistole** lockt es dorthin; **Jagd**: Ankündigung, 1,3 s später Treffer,
nicht sofort tödlich; Netzzustand Rolle 21, Puppe beim Gast mit Zustand; **Ausweichen** in der Ankündigung (leise weg) → verfehlt;
**Licht** auf dem Ohr → Kreischen, Innehalten; **Hof**: dort klopft es nicht.

## Restmängel

- Ganz nah (unter 1,3 m, mit Lampe) wirkt das Rissnetz der Kreidehaut eher marmoriert als rissig (Mikroebene `ML_CRAZE` ist für Lack
  gemacht).
- Die Gänge sind grell beleuchtet (Seite) – ein Halbdunkel für den ersten Auftritt gibt es nur in der Zelle; der Schreck kommt hier aus
  dem Klang (Klopfen, Saugen, Tür hinter dir), nicht aus dem Licht.
- Etagen: die Karte hat nur Erdgeschoss und Treppenabsatz; auf dem Absatz klopft es (verschlossene Tür), heraustreten kann es nur unten.
