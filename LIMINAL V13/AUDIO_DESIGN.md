# LIMINAL V11 – Sounddesign

> Abschnitte 1–5 beschreiben Analyse, Ziel und Technik. Abschnitt 6 listet am Ende die tatsächlich umgesetzten Klänge mit ihrer Herkunft. Abschnitt 7 hält fest, was gemessen wurde und was nicht.

## 1. Warum V10 billig klingt (Befund im Code)

| Befund | Stelle in V10 | Wirkung |
|---|---|---|
| Fast jeder Klang ist **Sinus plus gefiltertes Rauschen mit Exponentialhüllkurve**. Beispiele: ein Klopfen besteht aus zwei Sinustönen (170/410 Hz), Metall aus fünf festen Sinustönen | `audio/synth.cpp` (`distant`, `step`, `elevDing` …) | Synthetisch, „Computerspiel 1995“. Keine Körper, keine Materialien |
| **Schritte:** Bandpass-Rauschen plus Tiefpass-„Wumms“. Ein Fersen-Anschlag, kein Abrollen, keine Schuhsohle, kein Untergrund-Korn | `step()` | Klingt wie Klatschen auf Rauschen |
| **Ambiences:** 12-s-Schleifen aus gefiltertem Rauschen. Ereignisse kommen in festen Abständen (z. B. alle 1,45 s ein Klacken) | `ambience()` | Monoton, die Wiederholung ist nach Sekunden hörbar |
| **Ein globaler Freeverb** für alle Quellen, Eingang ×0,015, Ausgang ×3 | `AudioSystem::Reverb` | Metallisches Klingeln der Kammfilter, keine Frühreflexionen, kein Raumgefühl |
| **Linearer Pegelabfall** bis zur Reichweite: `(1 − d/r)²` | `mix()` | Quellen „poppen“ am Rand herein; nah zu leise, fern zu abrupt |
| **Dauerhafte Sättigung im Ausgang:** `x / (1 + 0,35·|x|)` auf jedes Sample | Ende von `mix()` | Verzerrt **alles**, auch leise Klänge. Matschiger, flacher Gesamtklang |
| **Lineare Interpolation** beim Tonhöhenwechsel | `sampleAt` | Aliasing, raue Höhen |
| Richtung nur über Panorama, Laufzeitdifferenz und einen Tiefpass von hinten | `mix()` | Kein Kopfschatten, wenig Außer-Kopf-Gefühl |
| **Verdeckung** als Zahl, die der Aufrufer schätzt. Schall geht durch Wände statt um Ecken | `soundscape.cpp`, `app.cpp` | Kreaturen hinter einer Wand klingen „durch die Wand“ statt „aus der Tür“ |
| Keine Variation beim Abspielen außer Tonhöhe | `playAt` | Maschinengewehr-Effekt bei Schritten |

## 2. Klangästhetik

- **Akustischer Realismus mit Zurückhaltung.** Jeder Klang hat eine glaubwürdige Quelle, ein Material und einen Ort. Es gibt keine Musik als Angstmacher und keine lauten Stinger.
- **Stille ist Material.** Ambiences sind leise. Zwischen Ereignissen gibt es bewusste Pausen. Nach einem großen Ereignis wird es stiller, nicht lauter. **Umgesetzt:** Während einer Begegnung treten die Klangbetten zurück (bis 50 %), damit das Wesen zu hören ist. Ist sie vorbei, bleibt es neun Sekunden fast still (Betten −75 %, die letzten vier Sekunden blenden auf), und ferne Einzelklänge pausieren so lange (`app.cpp`, „Nachstille“).
- **Der Raum klingt mit.** Kleine Zimmer sind trocken, lange Flure haben Flatterecho, Decks und Hallen tragen weit, die Becken in Level 37 brechen den Hall unnatürlich ab.
- **Nicht alles ist sichtbar.** Viele Klänge haben eine echte Position außer Sicht: ein Rolltor drei Hallen weiter, ein Rohr über der Decke.
- **Lesbare Gefahr.** Jede Kreatur hat eine eigene Klangsignatur, die man nach einer Begegnung wiedererkennt. Gefahr kündigt sich immer hörbar an, bevor sie ankommt.

## 3. Technik der Wiedergabe (Mischer V11)

| Maßnahme | Umsetzung |
|---|---|
| Abstand | Abstandsgesetz 1/d ab einer Referenzdistanz (je Klangart), weich auf null bis zur Reichweite |
| Luftdämpfung | abstandsabhängiger Tiefpass: ferne Quellen verlieren Höhen |
| Richtung | Laufzeitdifferenz (Woodworth-Modell), frequenzabhängiger Kopfschatten je Ohr (Hochregal-Filter nach Brown/Duda), leichte Dämpfung der Höhen hinter dem Kopf |
| Verdeckung und Schallweg | **Umgesetzt (`gameplay/sound_field`):** Vom Ohr aus wird ein Wegefeld über die offenen Kacheln gerechnet (Dijkstra, acht Nachbarn, 40 m je Richtung, neu bei Kachelwechsel, sonst alle 0,5 s). Gibt es einen offenen Weg in Reichweite, kommt der Schall auf diesem Weg: Die hörbare Richtung ist die letzte Stelle des Wegs, die das Ohr direkt erreicht (Beugung an der Kante). Der Mischer dämpft und verzögert nach der Weglänge; je stärker der Schall abknickt, desto dumpfer. Ohne Weg geht er durch die Wand: stark gedämpft (Verdeckung mindestens 0,85, nur Tiefen). Das gilt für Ausstattungsklänge, Einzelereignisse, Kreaturen (Schritte, Knurren, Ankündigung, Dauerklang) und alle Ereignisse mit Ort; die Leuchten behalten den geraden Strahl |
| Raumhall | Feedback-Delay-Netzwerk (8 Leitungen, Hadamard-Matrix) mit frequenzabhängiger Nachhallzeit (tief, mittel, hoch), leicht modulierte Verzögerungen gegen Klingeln. Dazu Frühreflexionen aus den Raummaßen. Weiche Übergänge zwischen Räumen |
| Abstand zum Hall | Je weiter eine Quelle, desto größer der Hallanteil gegenüber dem Direktschall |
| Busse und Regler | Gesamt, Effekte (Welt), Atmosphäre, Kreaturen, Oberfläche. Getrennte Lautstärkeregler |
| Dynamik | sanftes Ducking der Atmosphäre bei lauten Ereignissen, echter Begrenzer mit Vorausschau statt Dauersättigung, Gleichspannungsfilter |
| Variation | jedes Ereignis aus mehreren Varianten, nie zweimal dieselbe hintereinander. Dazu zufällig Tonhöhe ±3 %, Pegel ±1,5 dB, leichte Klangfarbe |
| Tonhöhe | kubische Hermite-Interpolation statt linearer |
| Stimmen | 96 Stimmen mit Priorität; leise und ferne Stimmen werden zuerst ersetzt |
| Ausfallsicherheit (V13.3) | gestoppte, unhörbare Schleifen sind sofort frei (vorher belegten sie ihre Stimme für immer, B-017); Mischer mit MMCSS-Priorität; Neuverbinden bei Gerätewechsel, Formatwechsel, Geräteverlust und stummem Treiber; Watchdog mit Stille-Erkennung am Master-Ausgang. Einzelheiten, Messungen und Tests: `AUDIO_BUG.md` |

## 4. Klangerzeugung (Syntheseset V11)

Die ausschließlich programmatische Erzeugung bleibt (siehe Abschnitt 8). Die Bausteine sind jetzt physikalisch inspiriert und geschichtet:

- **Kontakt und Resonanz (modal):** Ein Stoß ist ein kurzer Kraftimpuls, dessen Dauer von der Härte abhängt. Er regt eine Bank gedämpfter Resonanzen an: Metall mit inharmonischen, lang klingenden Moden; Holz mit wenigen, schnell gedämpften Moden; Beton mit dichten, sehr kurzen Moden; Glas hell und lang. Die Modenfrequenzen streuen je Variante.
- **Reibung und Schaben:** Rauschen durch Resonatoren, moduliert von der Bewegungsgeschwindigkeit (Rolltore, Türen, Schleifen).
- **Granulare Texturen:** viele kurze Körner mit zufälligem Abstand und Ton (Wasser, Regen, Sand unter der Sohle, Haar, Stoff, Staub).
- **Tropfen:** Blasenresonanz nach Minnaert, ein steigender Ton, dazu Aufprall und Hall in der Pfütze.
- **Leuchten:** Brummen der Drossel (100 Hz und Obertöne, Magnetostriktion), Gasentladungszischen im Takt der Halbwellen, Starterklicken, Flackern synchron zum Licht.
- **Elektrik:** Funken als dünne Folge scharfer Impulse durch helle Resonatoren, Lichtbogen-Knistern, Schütz-Klacken (Mechanik und Netzbrummen).
- **Luft und Maschinen:** Lüfter mit Blattfrequenz und Turbulenz, Rohrgrollen (braunes Rauschen durch tiefe Rohrmoden), Dampf (Zischen mit Druckabfall).
- **Stimmen der Kreaturen:** Quelle-Filter-Modell. Stimmlippenimpulse mit Jitter, Shimmer und Unterharmonischen (für Knurren) laufen durch Formantfilter, dazu Atemrauschen. Atmung und Hecheln haben einen eigenen Rhythmus.
- **Ferne:** Klänge werden offline mit einer erzeugten Raumantwort gefaltet (lange, diffuse Fahne) statt mit einfachen Kammfiltern.
- **Schleifen:** Atmosphären sind 60–90 s lang und haben langsame, zufällige, nicht periodische Modulation. Die Naht ist leistungsgleich überblendet.

## 5. Klangkulissen je Ebene

| Ebene | Grundbett (leise) | Punktquellen | Seltene Ereignisse | Raumklang |
|---|---|---|---|---|
| 0 Vorzimmer | Raumrauschen, entferntes Lüften | jede Leuchte brummt einzeln | Anschwellen des Brummens, Kratzen in der Wand, ferne Tür | trocken bis mittel, Teppich dämpft |
| 1 Lagerdeck | tiefes Gebäudegrollen, Lüfter | Leuchtbänder, Tropfen in Pfützen, Rohrklopfen | Rolltor rasselt, Rohrschlag, Wagen tickt; Stromausfall als näher kommende Schützwelle | groß und hart (T60 ≈ 2,5 s) |
| 2 Rohrgänge | Rohrrauschen, Wärme | Ventile zischen, Maschinen surren | Dampfstoß, Erdstoß, Klopfen aus einem Lüftungsgitter | eng, metallisch |
| 3 Umspannwerk | Netzbrummen 50/100 Hz, Trafo-Summen | Schaltkästen, Funken, Rohre mit schwarzer Flüssigkeit | Lampe platzt, Lichtbogen, Stimmen (Halluzinationszone) | mittel, hallige Hallen |
| 4 Stilles Stockwerk | Regen an Scheiben (überall), leises Rohrgrollen | Wasserspender gluckert | fast keine | weich, leichtes Echo |
| 5 Hotel | stille Flure, sehr ferne Musik aus alten Lautsprechern | Wandleuchten summen leise | Partygemurmel hinter einer Wand, eine Tür fällt zu | gedämpft (Teppich), Säle hallig |
| 6 Dunkel | fast Stille, eigener Atem | Wellenrauschen (Ziel) | Atmen, Huschen, Flüstern (Halluzination) | trocken, wie schalltot |
| 7 Meer | Wasser an Beton, Weite | Tropfen von der Decke | tiefes Grollen, Wellen aus der Tiefe | riesig, diffus |
| 19 Dachboden | Holz knarzt, Wärme | Knistern des Glühens unter den Dielen | ferne Stimmen, ein Spieluhrton (Halluzination) | trocken, Holz |
| 37 Becken | Wasser plätschert | Überläufe | — | kurz, mit abruptem Abbruch |

## 6. Umgesetzte Klänge und Herkunft

**Herkunft:** Alle 326 Klänge sind im Programm erzeugt (`src/audio/synth_*.cpp`, Bausteine in `synth_kit` und `dsp`). Es gibt keine fremden Dateien, Samples oder Aufnahmen. `ATTRIBUTION.md` bleibt für Klänge deshalb leer. Die Musik im Hotel (`hotel_music`) ist eine eigene, einfache Komposition.

**Gruppen** (je Gruppe mehrere Varianten `name_0 … name_n`; das Audiosystem wählt zufällig und nie zweimal dieselbe):
- **Spieler:** Schritte auf Beton, Pfütze, Teppich (trocken und nass), Fliesen, Marmor, Metall, Holz, Wasser; Sprung, Landung, Atemzüge, Keuchen, Herzschlag, Aufnehmen, Essen, Trinken, Batterie, Papier, Lampe an und aus, Menü.
- **Welt:** Leuchten (Leuchtstoffröhre, Industrie- und Leuchtband, Not- und Wandleuchte), Klicken, Anlaufen, Platzen, Funken, Lichtbogen, Trafo, Schütze (`breaker_far`), Strom aus und an, Tropfen, Wasser, Rohre (Klopfen, Grollen, Fließen), Dampf, Lüfter, Maschinen, Türen, Rolltore, Aufzug, Wagen-Ticken, Knarzen der Konstruktion, Erdstoß, Regen am Glas.
- **Kreaturen:**
  - Lächeln: Dauerklang, Zähne, Öffnen als Ankündigung, Zugriff, Verklingen.
  - Schreck beim tödlichen Zugriff (`scare_smiler`, `scare_hound`, -15 LUFS, die lautesten Klänge des Spiels):
    - Druckstoß im Unterbass und Rauschschlag im selben Moment;
    - darüber der gesättigte Schrei (Lächeln: zwei verstimmte, steigende Stimmen mit dem glasigen Ton; Hund: Brüllen mit Unterharmonischen, Kieferschnappen, Krallen);
    - danach Ohrenklingen und dumpfes Nachgrollen.
  - Hund: Knurren, Bellen, Winseln, Hecheln, Schritte, Schnüffeln, Schnappen, Ansturm.
  - Bündel; Motten; Gestalt; Tiefe.
  - Halluzinationen: Flüstern ohne Wörter, Kratzen, Atem, Stimmen.
- **Ebenen:** Klangbetten je Ebene (24 kHz, 30–40 s, nahtlos), ferne Ereignisse (gefaltet mit erzeugtem Nachhall), Spieluhr, Hotelmusik, Partygemurmel, ferne Wellen.

**Im Spiel verdrahtet (Level 1, `app/soundscape.cpp`, Abschnitt `audio` in `data/levels/level1.json`):**
- **Klangbetten:** Gebäudegrollen (in Decks, Rampen, Hallen voll, sonst leiser), Lüftung (überall), Rohre (in Gängen und Kammern). Bei Stromausfall werden sie bis 70 % leiser, denn die Lüftung steht.
- **Leuchten** als eigene Quellen (die nächsten acht). Leuchtbänder und Hallenstrahler klingen wie Industrieleuchten. Beim Stromausfall verstummen sie mit der Welle, Feld um Feld.
- **Stellen der Ausstattung** als Dauerquellen: Glut im Bodenloch, Schaltkästen, Rohrleitungen. Tropfen fallen in echte Pfützen der Karte, Wagen ticken.
- **Einzelereignisse** in Abständen aus der Level-Datei: Rohrklopfen unter der Decke, ferne Rolltore, Klirren, Knarzen.
- **Raumklang je Raumtyp:** Deck T60 2,4 s, Lagerhalle 2,8 s, Rampe 1,8 s, Gang 0,9 s, Rohrgang 1,3 s, Gummizelle 0,25 s. Die Frühreflexionen folgen den Maßen des Raums und dem Standort des Hörers (bei Bewegung viermal je Sekunde nachgeführt).
- **Schritte** je Untergrund; in einer Pfütze der Ausstattung platscht es.
- **Kreaturen:**
  - Das Lächeln hat einen Dauerklang an seinem Ort. Kommt es näher, klicken Zähne. Die Ankündigung (`smiler_open`) erklingt vor jedem Zugriff, beim Beruhigen folgt `smiler_fade`.
  - Der Hund klingt über Schritte, Hecheln, Knurren, Winseln und Bellen.
- **Stromausfall:** Die Schütze fallen mit dumpfem Klacken von fern nach nah (`breaker_far`, bis 90 m hörbar). Erreicht die Welle den Spieler, folgt `power_off`; kehrt das Licht zurück, `power_on`.

## 7. Messungen und Grenzen

**Messungen:**
- `gameplay_test` (Abschnitt „Schallausbreitung“): In Level 0, 1, 2 und 5 werden zwölf Fälle gesucht, in denen eine Wand zwischen Ohr und Quelle liegt und eine Tür daneben. In allen kommt der Schall durch eine Öffnung (nicht aus der Wand), heller als durch die Wand, mit gemessenem Umweg. Ist die Reichweite kürzer als der Umweg, bleibt er dumpf. Bei freier Sicht ist die Quelle unverändert.
- `audio_test`: 106 Prüfungen, 0 Fehler. Geprüft werden:
  - Pegel und Spitzen jedes Klangs, Schleifennähte, Varianten;
  - Abstandsdämpfung, Kopfschatten, Verdeckung, Nachhallzeit und Frühreflexionen;
  - Begrenzer, Ducking und Stimmenvergabe.
- `tests/sound_export.exe <Ordner>` schreibt alle Klänge als WAV und dazu `messung.txt` (Lautheit, Spitze, Schwerpunkt, Naht).
- Spieltest per Skript (`audioinfo`): je Raum Leuchten, Betten, Stellen und Raumklang. Unbekannte Klangnamen meldet das Audiosystem einmal im Protokoll (`Klang fehlt: …`). Im Probelauf durch Deck, Bodenloch, Rohrgang und Lagerhalle, mit Lächeln, Hund und Stromausfall, gab es keine solche Meldung.

**Grenzen:**
- Ich kann die Klänge nicht hören. Ob sie gut klingen, entscheidet dein Ohr. `sound_export` ist dafür da.
- Der Schallweg ist zweidimensional über die Kachelwelt; Höhenunterschiede (Treppen, Bodenloch) gehen nur als Zuschlag ein. Geschlossene Türblätter gibt es in den Karten kaum; Stahl- und Hoteltüren an Wänden sind Ausstattung und lassen keinen Schall durch.
- Die Nachstille ist im Code nicht automatisch getestet (nur durchgesehen und im Spiel ohne Fehlermeldung gelaufen).
- Alle zehn Ebenen haben einen Abschnitt `audio` (Betten, Stellen, Ereignisse, Raumklang). Hören konnte ich davon nichts.

## 8. Synthese oder Aufnahmen?

- **Bewertung:** Für Schritte auf echten Untergründen, Stimmen und Wasser sind gute Aufnahmen (eigene oder aus professionellen Bibliotheken) der Synthese überlegen. Bei Brummen, Elektrik, Lüftung, Maschinen, Hall und abstrakten Kreaturenstimmen ist hochwertige Synthese gleichwertig und hat Vorteile: unendlich viele Varianten, keine Wiederholung, keine Lizenzfragen, winzige Dateigröße.
- **Entscheidung für V11:** Es wird weiter synthetisiert, aber mit den Verfahren aus Abschnitt 4. Fremde Klangdateien werden nicht stillschweigend importiert.
- **Weg zu Aufnahmen:** Der Ersatzmechanismus bleibt erhalten. `data/sounds/<name>.wav` ersetzt einen erzeugten Klang, und `<name>_<n>.wav` liefert weitere Varianten. Falls du später Aufnahmen verwenden möchtest (z. B. CC0-Aufnahmen oder eigene Aufnahmen), braucht jede Datei einen Eintrag in `ATTRIBUTION.md` mit Quelle, Urheber, Lizenz und Änderungen.
- **Ehrliche Grenze:** Ich kann die Klänge nicht selbst hören. Geprüft wird über Messungen (Pegel, Spektrum, Schleifennähte, Nachhallzeit, Wiederholung, Clipping). Das Werkzeug `sound_export` schreibt alle Klänge als WAV-Dateien, damit du sie anhören kannst.
