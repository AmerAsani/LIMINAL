# LIMINAL V11 – Verhalten der Entities

Grundlage sind die Wiki-Beschreibungen (Quellen in `LEVEL_SELECTION.md` und `docs/ENTITAETEN.md`). Im Spiel tragen die Kreaturen keine Wiki-Namen (`LICENSE_REVIEW.md`). In diesem Dokument steht der Wiki-Name in Klammern, damit die Herkunft nachvollziehbar bleibt.

---

## 1. Gemeinsame Regeln

### 1.1 Wahrnehmung (für alle gleich berechnet)
| Reiz | Wie er entsteht | Wer reagiert |
|---|---|---|
| **Lärm** | Stillstand 0, langsam gehen (Taste *Leise*) 0,15, gehen 0,45, rennen 1,0, Landung nach Sprung 0,8, Platschen in Pfützen +0,4, fallen gelassene oder geworfene Dinge 0,6 am Aufprallort, Türen 0,5. Der Lärm breitet sich über die Kachelwelt aus (Weglänge durch offene Kacheln, nicht durch Wände) | Hund, Bündel, Lächeln |
| **Licht** | eingeschaltete Taschenlampe (Kegel, Richtung) und die Helligkeit der Kachel, auf der der Spieler steht | Lächeln, Hund, Motten, Wesen in der Tiefe |
| **Blick** | Spieler sieht die Kreatur: im Sichtfeld (≈ 50° neben der Blickmitte), freie Sichtlinie, und die Kreatur ist beleuchtet oder leuchtet selbst | Lächeln, Hund, Gestalt |
| **Haltung** | Spieler wendet sich zu (Blick auf die Kreatur), steht, geht auf sie zu, wendet sich ab, rennt weg | Hund |

### 1.2 Wo Kreaturen erscheinen (semi-prozedural, `WORLD_GENERATION.md`)
Jede Karte enthält Spawnpunkte je Kreatur:
- **Bündel:** Lüftungsgitter und dunkle Kreuzungen;
- **Hund:** dunkle Lagerboxen, ferne Deckecken;
- **Motten:** Nester an den Türen bestimmter dunkler Zimmer;
- **Lächeln:** Dunkelzonen und Lichtfelder, die ausfallen können;
- **Gestalt:** Gangmündungen mit langer Sichtachse.

Zur Laufzeit wählt die Regie unter diesen Punkten einen, der die Fairnessregeln erfüllt. Welche Nester und welche Lichtfelder in einem Lauf aktiv sind, bestimmt der Lauf-Seed.

### 1.3 Regie (Spannung und Ruhe)
- Eine Spannungsgröße steigt bei Begegnungen und sinkt in ruhigen Phasen.
- Nach jeder Begegnung folgt eine **Ruhezeit** von mindestens 90–180 s. Erst danach darf die nächste Gefahr auftreten.
- Höchstens **eine** angreifende Kreatur gleichzeitig. Ein Mottenschwarm zählt als eine.
- Level 4 und Level 37 sind ohne Angriffe. Level 0, 6 und 19 haben keine echten Kreaturen.

### 1.4 Fairness (prüfbar, Tests in `gameplay_test`)
| Nr. | Regel |
|---|---|
| F1 | Keine Kreatur erscheint im Blickfeld oder näher als 15 m Weg. Erscheinen ist nur außer Sicht möglich |
| F2 | Jeder Angriff hat eine Ankündigung von mindestens 1,5 s, hörbar **und** sichtbar (Bellen und Ducken, das Lächeln öffnet sich mit steigendem Ton, der lange Arm holt aus, Grollen und Wellen) |
| F3 | Wer die Regel der Kreatur befolgt, wird nicht angegriffen. Die Regeln stehen in Abschnitt 2 |
| F4 | **Geändert am 25.10.2026 (Entscheidung des Nutzers):** Der Zugriff von Lächeln und Hund ist **tödlich**, mit einem echten Schreck (siehe Abschnitt 4). Fair bleibt es durch F2 (Ankündigung), F3 (Regel befolgen schützt) und F5 (Regeln erfahrbar) sowie durch den Wiedereinstieg: Es geht am letzten Sicherungspunkt weiter, mit voller Gesundheit. Inventar und Funde bleiben, der Stromausfall ist vorbei, und es folgen 90 s Schonzeit. Die Motten stechen weiter nur (5 % je Stich). |
| F5 | Jede Regel ist im Spiel erfahrbar. Notizen derselben Ebene deuten sie an, bevor man der Kreatur begegnet. Beim ersten Treffer erklärt ein Hinweis in eigenen Worten, was geholfen hätte |
| F6 | Halluzinationen richten nie Schaden an. Sie überdecken nie echte Warnklänge und erscheinen nie in der Richtung einer echten Gefahr |
| F7 | Kreaturen bleiben in ihrem Lebensraum: das Lächeln nie im Licht, das Bündel nie im offenen Deck, wenn es sich verstecken kann. Kreaturen bewegen sich nie durch Wände |

---

## 2. Die Kreaturen

### 2.1 Das Lächeln (Entity 3)
- **Lebensraum:**
  - Level 1: nur während eines Stromausfalls (F);
  - Level 2: in ausgefallenen Lichtfeldern;
  - Level 3: im Dunkeln, mit gedämpftem Leuchten (W: verbirgt das Leuchten hinter Lidern).
- **Wahrnehmung:** Licht (Taschenlampe bis 25 m, auch von der Seite), Lärm bis 15 m. Betritt nie Kacheln mit Licht ≥ 0,25.
- **Regel für den Spieler:** Ruhe bewahren, Blick halten, langsam zurück ins Licht. Taschenlampe aus oder **eingeschaltet ablegen** (Köder, W: Licht als Köder wegwerfen).

| Zustand | Verhalten | Übergang |
|---|---|---|
| Erscheinen | Außer Sicht im Dunkeln. Zuerst öffnen sich nur die Augen, ein tiefes, sehr leises Dröhnen beginnt | → Beobachten |
| Beobachten | Blickkontakt: rührt sich nicht. Ungesehen: treibt durch dunkle Kacheln näher (1,2 m/s) | Licht der Lampe gesehen → Gelockt; Panik → Erregt; 30–45 s ohne Reiz → Erlöschen |
| Gelockt | zieht zur Lichtquelle. Liegt die Lampe am Boden, geht es dorthin und verharrt | Lampe aus → Beobachten |
| Erregt | Spieler rennt in < 10 m oder macht lauten Lärm. Das Lächeln öffnet sich weit, der Ton steigt (Ankündigung 1,5 s) | weiter Panik → Zugriff; Spieler steht still und hält Blick → Beobachten |
| Zugriff | Schaden, dann zerfällt es ins Dunkel | → Ruhe |
| Erlöschen | Licht kehrt zurück, Spieler steht auf heller Kachel, kein Reiz | fort |

### 2.2 Der Hund (Entity 8)
- **Lebensraum:** Level 1 (scheu, F), Level 3 (lauernd, W), selten Level 5, Level 4 nur fern und ohne Angriff.
- **Wahrnehmung:**
  - Lärm (Gehör sehr gut, F);
  - Licht (zieht zum Licht, F);
  - Sicht: Kegel 120°, 25 m, Spieler beleuchtet.
- **Regel für den Spieler:** Ihm zuwenden und stehen bleiben oder einen Schritt auf ihn zu (W: Blickkontakt schüchtert ein; F: sich groß machen). Nicht wegrennen, nicht den Rücken zeigen. Licht aus und langsam zurückziehen, dann verliert er die Spur.

| Zustand | Verhalten | Übergang |
|---|---|---|
| Streifen | weit entfernt (30–50 m), hörbar durch Krallen und Hecheln | Lärm ≥ Schwelle oder Licht gesehen → Aufmerksam |
| Aufmerksam | hebt den Kopf, lauscht, leises Knurren | Spieler weiter wahrnehmbar → Pirschen; sonst → Streifen |
| Pirschen | geduckt, nutzt Deckung, stoppt, wenn angesehen | Spieler wendet sich zu (< 10 m) → Eingeschüchtert; Spieler wendet sich ab oder rennt (< 12 m) → Ansturm |
| Eingeschüchtert | weicht zurück, knurrt. Auf Level 1 flieht er nach zwei Einschüchterungen (scheu) | Blick hält zu lange (> 8 s, Level 3) → Pirschen |
| Ansturm | Bellen und Sprung-Ducken (Ankündigung 1,2–1,5 s), dann Galopp mit Hinken | Spieler dreht sich während der Ankündigung um und steht → Eingeschüchtert; sonst → Biss |
| Biss | Schaden, dann Rückzug | → Rückzug |
| Verloren | winselt, zieht ab | fort |

### 2.3 Das Bündel (Entity 5, neu)
- **Lebensraum:** Level 2 (Kreuzungen, Lüftungsgitter, W/F), Level 3 (größer), selten Level 1 (W: Tabelle der Entities).
- **Wahrnehmung:** **nur Gehör** (W: jagt nach Geräusch). Blind für Licht und Blick.
- **Regel für den Spieler:**
  - leise sein, mindestens 2,5 m Abstand halten (W: etwa 8 Fuß);
  - nicht rennen;
  - einen Gegenstand werfen, um es wegzulocken.
- **Warnzeichen:** ein Arm, der aus einem Gitter hängt (F: Köderarm). Rhythmisches Klopfen an Rohren (F: lockt mit Klopfen).

| Zustand | Verhalten | Übergang |
|---|---|---|
| Lauern | versteckt in Gitter oder dunkler Ecke. Ein Arm hängt, Finger zucken | Lärm in Hörweite → Lauschen |
| Lauschen | Arm zieht sich zurück, zwei, drei Schläge an Rohr oder Wand | weiter Lärm → Jagen; 10 s Stille → Lauern |
| Jagen | fällt heraus und zieht sich Hand über Hand zum **Ort des letzten Geräuschs**, nicht zum Spieler | neuer Lärm → Ziel wechselt; am Ziel nichts → Suchen |
| Suchen | tastet mit Gliedern, verharrt | Lärm in < 2,5 m → Ausholen; 20 s Stille → Rückkehr |
| Ausholen | der längste Arm holt aus (Ankündigung 1,0–1,5 s) und zielt auf die Stelle, die es gehört hat; lauter Lärm verrät die neue Stelle, leise Schritte nicht | Spieler entfernt sich leise über 2,5 m → Suchen; ist er beim Zugreifen mehr als 1 m von der gehörten Stelle weg, geht der Griff ins Leere → Suchen; sonst → Greifen |
| Greifen | Schaden, dann Rückzug in den nächsten Schacht | → Rückkehr |

### 2.4 Die Motten (Entity 4)
- **Lebensraum:** Level 5. Nester an den Türen dunkler Zimmer.
- **Wahrnehmung:** Licht (W: helles Licht zieht an), Nähe unter 4 m.
- **Regel für den Spieler:** Lampe aus, Abstand. Ohne Licht kehren sie zum Nest zurück.

| Zustand | Verhalten | Übergang |
|---|---|---|
| Sitzen | reglos an Wand und Decke, Flügel flach | Licht trifft sie oder Spieler < 4 m → Aufwärmen |
| Aufwärmen | Flügel zittern (Ankündigung 1,5 s), leises Schwirren | → Schwarm |
| Schwarm | umkreist die Lichtquelle; bei eingeschalteter Lampe sticht einzeln eine zu (kleiner Schaden) | Lampe aus → Kreisen |
| Kreisen | kreist um Deckenleuchten | 15–25 s → Heimkehr |
| Heimkehr | zurück ins Nest, durch Türen, nie durch Wände | → Sitzen |

### 2.5 Die Gestalt (Level 0, unbestätigte „dunkle Gestalten“)
- Erscheint nur weit weg (> 14 m), nur dort, wo man gerade nicht hinsieht.
- Verändert sich nie im Blick. Sieht man weg und wieder hin, ist sie fort.
- Greift nie an. Wirkung nur auf den Verstand.

### 2.6 Etwas in der Tiefe (Level 7)
- **Lebensraum:** tiefes Wasser von Level 7.
- **Wahrnehmung:** Aufenthalt im tiefen Wasser; Licht schreckt es ab (W).
- **Regel für den Spieler:** nicht lange im tiefen Wasser bleiben. Wenn es kommt, mit der Lampe nach unten leuchten oder ins flache Wasser bzw. zur Wand.

| Zustand | Verhalten | Übergang |
|---|---|---|
| Fern | nichts | > 20 s im tiefen Wasser → Kreisen |
| Kreisen | fernes Grollen, Dünung, ein riesiger Schatten tief unten | > 40 s → Aufsteigen; Spieler verlässt das Tiefe → Fern |
| Aufsteigen | Grollen wird lauter, das Wasser darunter wird dunkler (Ankündigung ≥ 6 s) | Lampe nach unten → Abgeschreckt; sonst → Stoß |
| Stoß | Schaden, Wasser wirft den Spieler herum | → Abgeschreckt |
| Abgeschreckt | taucht ab, 120 s Pause | → Fern |

---

## 3. Zusammenspiel der Werte

| Wert | sinkt durch | steigt durch |
|---|---|---|
| **Verstand** (Sanity) | Dunkelheit ohne Licht in 3 m (langsam); Sichtung einer Kreatur; Treffer; Level 6 ständig; Level 19 lange am Glühen | helle Bereiche (langsam); Mandelwasser; das Wasser von Level 37 (deutlich); Level 4 (Ruhe); Level 19 kurz am Glühen |
| **Ausdauer** | Rennen, Springen, heiße Bereiche (Level 2 Dampf, Level 5 Kesselraum), Schwimmen | Gehen und Stehen |
| **Gesundheit** | Angriffe, kaltes Wasser in Level 7, Dampfstöße | langsam von selbst, solange der Verstand über 50 % liegt; Energieriegel |
| **Licht** (Batterie) | eingeschaltete Lampe | Batterien. In Level 6 funktioniert die Lampe nicht |

- **Halluzinationen** nehmen mit sinkendem Verstand zu. Je Ebene gibt es eigene: Kratzen in der Wand (0), Stimmen in den Zonen von Level 3, Atmen, Schritte und Flüstern (6), eine Spieluhr unter den Dielen (19).
- Sie sind am Klang erkennbar: Sie kommen nie näher wie echte Schritte, haben keine echte Schallausbreitung (klingen „im Kopf“, ohne Raumhall der Umgebung) und brechen ab, wenn man sich ihnen zuwendet.
- Der Verstand ändert **nicht**, wie gefährlich Kreaturen sind. Die Regie bleibt fair, auch wenn der Spieler schlecht dran ist.

---

## 4. Stand der Umsetzung (Prototyp Level 1)

Umgesetzt ist **eine** Begegnung, vollständig nach den Regeln oben: der Stromausfall auf Level 1 mit dem Lächeln.

**Ablauf im Spiel:**
1. **Der Stromausfall kommt als Welle.** Er beginnt am fernsten Eck des Decks, höchstens 48 m entfernt, und läuft Feld um Feld (8 m) auf den Spieler zu, mit dumpfem Klacken eines Schützes je Feld. Licht und Brummen verlöschen mit der Front (`Haunt::powerCut`, Shader `blackoutFactorAt`, `Soundscape`). Erreicht die Welle den Spieler, folgt das Aus. Die Grünschilder, die Wartungsflure mit eigenem Strom und die Glut im Bodenloch bleiben hell.
2. **Erst nach der Welle (7–10 s) erscheint das Lächeln.** Es erscheint im Dunkeln, außer Sicht und mindestens 15 m Weg entfernt (F1). Zuerst öffnen sich die Augen.
3. **Wer hinsieht, hält es fest.** Ungesehen treibt es durch das Dunkel näher (1,25 m/s, nie ins Licht), und Zähne klicken dabei.
4. **Rennt man in seiner Nähe (unter 10 m), öffnet es sich weit** (`smiler_open`, Ankündigung, F2). Für 1,5 s bewegt es sich nicht.
   - Bleibt man stehen und sieht hin, schließt es sich wieder (F3).
   - Rennt man weiter, greift es zu (4,2 m/s).
5. **Der Zugriff ist tödlich** (F4, geändert): Es folgt ein Schreck.
   - Die Kamera reißt zum Gesicht herum (0,1 s), und das Gesicht fährt bis knapp vor die Linse.
   - Dazu Zittern, Zoom, ein weißer Blitz, roter Rand und ein eigener Schrei (`scare_smiler`, beim Hund `scare_hound`).
   - Nach 1 s kommt der harte Schnitt ins Schwarz, mit Ohrenklingen.
   - Nach 2,4 s erscheint der Todesschirm. Er sagt, was geholfen hätte, und führt mit „Weiter“ zum letzten Sicherungspunkt.
6. **Kehrt das Licht zurück, erlischt es.**

**Weitere Regeln:**
- Auf Level 1 kommt das Lächeln **nur** mit dem Stromausfall. Außerhalb davon erscheint dort nur der Hund.
- **F5:** Die Notizen „Wenn die Schütze fallen“ und „Hinter den Rolltoren“ liegen auf Level 1 als erste. Sie beschreiben die Regeln in eigenen Worten.
- **Zeiten:** Abstand und Dauer der Stromausfälle stehen in `data/levels/level1.json` (`audio.blackout`): alle 140–260 s, 25–45 s lang. Der erste kommt früher.

**Geprüft (`gameplay_test`, auf der Karte von Level 1):**
- das Erscheinen nach dem Stromausfall, außer Sicht;
- Stillstand unter Blickkontakt, Näherkommen ungesehen, Zugriff;
- die Ankündigung mindestens 1,5 s vor dem Zugriff, ohne Treffer währenddessen;
- das Beruhigen durch Stehen und Hinsehen;
- der tödliche Zugriff mit Schreck (Blick zum Gesicht, unter 1,2 m vor dem Auge, Zoom);
- der Wiedereinstieg am Sicherungspunkt, eine Minute Ruhe danach, ein neuer Sicherungspunkt durch Vorbeigehen und im Spielstand.

**Umgesetzt in V11 (Stand 25.10.2026):**
- **Das Bündel (2.3)** mit allen Zuständen:
  - Lager sind die Deckenschächte der Karten von Level 1–3; Level 1 hat einen Anteil von 0,35 (`presence.clump`), dort zieht es seltener ein.
  - Lärm als Reiz für das Bündel (Abschnitt 1): still, leise gehen (Taste C), gehen, rennen, Landung, Pfütze und Wasser, geworfene Dinge am Aufprallort.
  - Der Griff ist tödlich (F4).
- **Die neuen Modelle** nach `ART_DIRECTION.md` 5.2: das Lächeln (Rauchsäule, Lider, Nadelzähne), der Hund (nasses Haar), das Bündel.
- **Geprüft (`gameplay_test`, Karte von Level 2):**
  - still stehen bleibt unbemerkt;
  - ein Wurf lockt es aus dem Schacht zum Aufprall, am still stehenden Spieler vorbei;
  - Lärm in Armlänge: Ausholen; leise weg: der Griff geht ins Leere; weiter Lärm: tödlich.

**Noch nicht umgesetzt:** die eingeschaltet abgelegte Lampe als Köder; der Hund hört weiterhin nur Rennen und nahes Gehen (nicht das ganze Lärm-Modell); das größere Bündel auf Level 3.
