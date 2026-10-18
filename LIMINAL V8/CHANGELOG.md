# LIMINAL V8 – Änderungen gegenüber V7

In V7 waren die Level getrennte Welten. Ein Notausgang blendete ab und lud die nächste. V8 legt alle Ebenen in **eine zusammenhängende Welt**: Man kommt ohne Ladebild von den Büros ins Tiefgeschoss und ins Hotel, oft ohne es sofort zu merken. Alte Spielstände behalten ihre getrennte Welt, bitgenau gleich.

## Kurzfassung

- **Eine Welt, keine Level-Wahl:** Jeder Lauf beginnt in den Büros (Level 0). Das Menü „Neues Spiel“ hat keine Level-Zeile mehr.
- **Schichten statt Ladebildschirme:** Büros, dann Tiefgeschoss (Level 1), dann Hotel (Level 2), dann wieder von vorn. Die Grenzen sind verwirbelt, mit Zungen, Buchten und Taschen.
- **Nahtstellen:** Die Zonen beider Ebenen mischen sich. Sieben neue Übergangsräume verbinden genau die Ebenen, die dort aufeinandertreffen.
- **Entdecken:** Erst fällt weit hinten eine Tür ins Schloss, viel später erscheint leise der Name der neuen Ebene.
- **Aufzüge:** Sie fahren eine Schicht tiefer, ohne Schnitt oder Überblendung, und merken sich den Rückweg.
- **Stabiler bei langen Läufen:** Die Materialbank kann nicht mehr überlaufen. Seit V5 war das bei sehr langen Läufen möglich.

---

## Die Welt

- `data/world.json` beschreibt die Welt:
  - Ebenen und ihre Reihenfolge
  - Schichten: Radius der Büros 380 m, je weitere Schicht 520 m
  - Verwirbelung: Domain-Warping 250 m, Taschen 160 m
  - Ruhezone um den Start: 170 m
  - Nahtbreite, Übergangsräume, Aufzüge
- Die Ebenen selbst kommen **unverändert** aus `data/levels/*.json` (mit ihrem V6-Aufbau). Im Katalog der Welt tragen sie Präfixe (`level2/ballsaal`), Lichtstile gelten je Ebene, Vorräte und Fundstücke ebenso.
- **Generator:**
  - Das Zonenfeld kennt die Schichten. Die Zonen der Ebene, die hier liegt, setzen sich durch.
  - Raumtypen kommen nur aus den Ebenen, die an der Stelle wirklich liegen.
  - Passagen und Fluchten folgen den Regeln der jeweiligen Ebene.
- **Übergangsräume** (erscheinen nur, wo genau die Ebenen zusammentreffen, die sie verbinden):

  | Naht | Räume |
  |---|---|
  | Büros ↔ Tiefgeschoss | Lastenflur, Abgang (abgesenktes Treppenhaus), Schleuse |
  | Tiefgeschoss ↔ Hotel | Personaltreppe, Wirtschaftsgang (Schleuse) |
  | Hotel ↔ Büros | Dienstflur, Konferenzbereich (Schleuse) |

- **Gemessen** mit `layout_stats --world` (2 Seeds, 25 × 25 Sektoren):
  - Level 1 beginnt im Median nach etwa 400 m (240–550 m je nach Richtung).
  - Level 2 beginnt im Median nach etwa 870 m (720–1050 m).
  - Bis 200 m ist alles Level 0.

## Entdecken

- Die Ebene gilt erst als betreten, wenn ihr Raum dort klar überwiegt (über 70 %) und man 14 Sekunden in ihr unterwegs ist. An den Nahtstellen selbst passiert nichts.
- Beim ersten Schritt in eine **unbekannte** Ebene fällt etwa 30 m hinter dir eine Tür ins Schloss (ferner, verhallter Klang).
- Zonen einer noch unbekannten Ebene verraten sich nicht: Ihr Name erscheint weder als Einblendung noch unter der Minimap.
- Bei der Erkenntnis erscheint mittig, gesperrt gesetzt und langsam ein- und ausgeblendet „LEVEL 1“ mit „Das Tiefgeschoss“ darunter, dazu ein tiefer Akkord mit fernem Schlag.
- Laufbuch: entdeckte Ebenen und **Übergänge** (statt Notausgängen), die aktuelle Ebene im Pausenmenü und auf dem Todesbildschirm.

## Aufzüge

- **Bauweise:**
  - Ein Kern aus 4 × 3 Kacheln an einer freien Wand, nur in Räumen ab 6 × 7 m, nie vor Türen, Treppen, Leuchten oder Einbauten.
  - Davor eine **begehbare Kabine** von 2 × 2 m: Stahlverkleidung, Deckenlicht (echte Lichtquelle), Handlauf, Tastenfeld.
  - Zargen, Sturz, Ruftaster und eine **rote LED-Etagenanzeige** innen und außen.
- **Ablauf:**
  1. Rufen (E): Gong, die zwei Türflügel gleiten in den Kern.
  2. In der Kabine (E): Die Türen schließen, die Kabine ruckt an, Motor und Seile brummen, sie hält.
  3. Bei 1,7 s Fahrt wird der Spieler in eine **baugleiche Kabine** der nächsten Schicht versetzt, mit gleicher Lage und gleichem Blick. Die Welt dort lädt im geschlossenen Kasten. TAA-Verlauf und Körper werden zurückgesetzt, damit kein Nachbild entsteht.
  4. Ankunft: Gong, die Türen öffnen sich.
- **Ziel:** auf dem Strahl vom Start durch den Aufzug in die nächste Schicht. Dort wird der nächstgelegene Aufzug gesucht, sodass die Richtung erhalten bleibt.
- **Rückweg:** Beide Richtungen werden gemerkt und gespeichert. Derselbe Aufzug fährt zurück.
- **Geschlossene Türen** lassen niemanden hinein.
- Minimap und Karte zeigen Aufzüge hellblau, die Hilfe erklärt sie.
- **Neue Klänge:** Gong, Schiebetür, Fahrt.

## Weiteres

- **Notizen:** Zehn neue für die Welt. Die Notizen über Notausgänge gelten nur noch für getrennte Level (`"mode"` in `data/notes.json`).
- **Notausgänge:** Gibt es in der Welt nicht mehr. Alte Stände (V7) behalten sie.
- **Materialbank:**
  - Ist sie voll (16 368 Einträge), wird das ähnlichste vorhandene Material verwendet, statt mit „Zu viele Materialien“ abzubrechen. Das betraf seit V5 sehr lange Läufe in jedem Level.
  - In der Welt werden Raumfarben gröber gestuft: 10 004 statt bis zu 16 368 Materialien für 17 × 17 Sektoren.
- **Diagnose:** Bleibt das Nachladen der Welt hängen, schreibt `World::preload` ins Protokoll, welcher Chunk und welche Sektoren betroffen sind.
- **Werkzeuge:**
  - `layout_stats --world` liefert Karte, Entfernungen, Übergangsräume, Aufzüge und Beispielpositionen.
  - Neuer Skriptbefehl `pos x y [blick]`.

## Kompatibilität

- **Eigene Dateien:** `settings_v8.json` (beim ersten Start aus V7 übernommen), `saves_v8\`, `liminal_v8.log`.
- **Ältere Spielstände:**
  - V7-Stände erscheinen mit „(V7)“ und laden ihr getrenntes Level, mit Notausgängen.
  - Ältere Stände (V6, V5, V4) laden wie bisher.
  - `worldgen_test` bestätigt: Die Welt aller alten Stände ist bitgenau gleich.
- **Getrennte Level:** gibt es nur noch für alte Stände und zum Testen (`--level`).

## Tests

| Test | Ergebnis |
|---|---|
| `gameplay_test` (neu: Welt, Determinismus, Schichten, Übergangsräume, Start in Level 0, Aufzug hin und zurück, Verbindung im Spielstand) | 119 Prüfungen, 0 Fehler |
| `worldgen_test` (alter Aufbau bitgenau) | 29 658 Prüfungen, 0 Fehler |
| `layout_stats --world --check` (Start nur Level 0, Ebenen nicht zu nah, Übergangsräume vorhanden) | OK |
| `layout_stats --check` / `--finds --check` (getrennte Level wie V7) | OK |
| `supply_balance_test` | OK |
| `audio_test` | 39 Prüfungen, 0 Fehler |

Die neuen Inhalte wurden zusätzlich im Spiel geprüft (Aufnahmen):
- Start in den Büros
- Nahtstellen (Lastenflur, Abgang, Schleuse)
- Tiefgeschoss und verzögerter Ebenen-Titel
- Aufzug (Rufen, Kabine, Fahrt, Ankunft eine Schicht tiefer)
- Hotel

---

# LIMINAL V7 – Änderungen gegenüber V6

V7 gibt dem Erkunden ein Ziel. Bisher war die Welt endlos und ziellos. Jetzt gibt es einen Weg nach draußen, oder zumindest nach woanders, und Dinge, die man unterwegs findet. Die Engine ist dieselbe. Alles Neue ist prozedural (keine Bild- oder Tondateien) und so gebaut, dass alte Spielstände ihre bitgenau gleiche Welt behalten.

## Kurzfassung

- **Taschenlampe mit Batterien:** echter Lichtkegel mit Schatten und sichtbarem Strahl im Dunst, in der linken Hand des Strichmännchens. Batterie-Anzeige neben der Schnellleiste.
- **Notausgänge zur nächsten Ebene:** grünes Leuchtschild, Stahltür mit Panikstange. Büros → Tiefgeschoss → Hotel → zurück. Inventar, Werte, Lampe, Notizbuch und Laufbuch kommen mit.
- **Neues Level 2 „Das Hotel“:** endlose Teppichflure mit Wandleuchten, Ballsäle, Lobbys, Personalbereich und ferne Fahrstuhlmusik
- **Notizen anderer Wanderer** (36 Texte) und ein **Notizbuch** im Pausenmenü
- **Halluzinationen** bei schwindender Sanity: Schritte hinter dir, Stromausfälle, „der Andere“ am Ende des Gangs (abschaltbar)
- **Laufbuch:** Zeit, Strecke, Räume, Ebenen, Notausgänge, Notizen, Halluzinationen; im Pausenmenü und auf dem Todesbildschirm

---

## Taschenlampe

- Taste **T**, Gamepad Steuerkreuz oben. Die linke Hand hält die Lampe, die rechte bleibt frei für Gesten.
- Der Renderer kennt dafür neue **Zusatzleuchten je Bild**. Sie stehen vor den Weltleuchten in der Lichtliste. Spotlicht-Profil:
  - heller Kern, weicher Ring, schwaches Streulicht
  - eigene Schatten
  - sichtbarer Strahl im volumetrischen Dunst
  - wirkt auch in Partikeln (Staub im Strahl)
- **Batterie:**
  - Start mit 70 %. Eine volle Ladung reicht eingeschaltet etwa 5 Minuten.
  - Unter 15 % flackert das Licht, unter 5 % wird es schwach.
  - Batterien laden mit F 50 % nach.
- Die Anzeige neben der Schnellleiste erscheint, wenn die Lampe an ist oder die Batterie zur Neige geht.

## Notausgänge, Ebenen und Level 2

- Notausgänge gibt es erst weit vom Start (Level 0 ab etwa 120 m) und dann mit wachsender Entfernung häufiger. Im Mittel liegt der nächste 140–200 m entfernt (`layout_stats --finds`).
- Sie sitzen in einer freien Wand: keine Ecke, keine Tür dahinter, keine Wandleuchte daneben.
- Das Schild beleuchtet Wand und Boden davor grün.
- Wer ein Schild sieht, bekommt einen Hinweis. Minimap und Karte zeigen den Ausgang grün, außerhalb des Ausschnitts als Richtungsmarke am Rand.
- **E** öffnet die Tür: Klang von Panikstange und Tür, Abblende ins Schwarze, neue Ebene mit eigenem Seed. Danach Titel der Ebene und Autosave.
- **Level 2 „Das Hotel“**, nur aus Daten:
  - **Zonen:** Hotelflure (Teppich, Tapete, Wandleuchten), Säle und Lobby (helle Fliesen, warmes Deckenlicht), Personalbereich (Kacheln, Neonlicht)
  - **Räume:** Hotelzimmer, Suiten, Minibar, Wäscherei, Wäschelager, Galerien, endlose Hotelflure, Ballsaal, Lobby, Speisesaal, Treppenhaus, „Flur der Zimmer“, zu hohe Zimmer, die Leere, dunkle Zimmer
  - **Weitläufiger V6-Aufbau** wie bei den anderen Ebenen (`layout_stats --check` besteht):

    | Messwert (Level 2, 5 Seeds × 25 Sektoren) | kompakt | V6-Aufbau |
    |---|---|---|
    | Räume je Hektar | 99 | 58 |
    | Median-Raumgröße | 45 m² | 81 m² |
    | Längste Sichtachse (Mittel) | 20 m | 27 m |
    | Versorgungsräume je Hektar | 6,2 | 4,0 |

  - **Lichtstile mit eigenem Klang** (`"hum"`): Die Glühlampen des Hotels summen nicht.
  - **Atmosphäre je Zone**:
    - Hotelflure: synthetische, leiernde Fahrstuhlmusik, gedämpft wie durch Wände
    - Säle: dieselbe Musik, noch ferner
    - Personalbereich: eine polternde Waschtrommel
- Neue Leveldaten-Felder: `next_level`, `finds` (Häufigkeit von Notizen, Batterien und Notausgängen), `light_styles.*.hum`.

## Notizen und Notizbuch

- Zettel liegen selten auf dem Boden (etwa jeder 20. Raum), am liebsten am Rand.
- **E** hebt sie auf und zeigt sie als Papier vor dem abgedunkelten Spiel.
- 36 Texte in `data/notes.json`, manche nur in bestimmten Ebenen. Bevorzugt erscheinen Notizen, die du noch nicht kennst.
- Das **Notizbuch** (ESC → Notizbuch) listet alle gelesenen Notizen. Es wird gespeichert und auf neue Ebenen mitgenommen.

## Halluzinationen

| Sanity | Erscheinung |
|---|---|
| < 55 % | Schritte hinter dir, im Takt der eigenen, positioniert im Raum. Umdrehen lässt sie verstummen. |
| < 35 % | Stromausfall im aktuellen Raum: Leuchten zucken, gehen aus, kommen stotternd zurück. Mit Relais-Klacken, Starter-Ticken und verstummendem Summen. |
| < 25 % | „Der Andere“: ein gestrecktes, gesichtsloses Strichmännchen am Ende des Gangs, das dich ansieht. Es verschwindet in einem Flackern, wenn du näher kommst oder zu lange hinsiehst. |

- Rein subjektiv: keine Wirkung auf Werte, nichts wird gespeichert, eigene Zufallsquelle.
- Abschaltbar: *Grafik & Anzeige → Halluzinationen*.
- **Technik:** Der Renderer dunkelt Leuchten, Leuchtflächen und Umgebungslicht eines Raums über seinen Flacker-Seed ab. Die Klanglandschaft lässt dessen Leuchten verstummen.

## Laufbuch

- Pausenmenü und Todesbildschirm zeigen:
  - Spielzeit und Strecke
  - betretene Räume
  - Ebenen und Notausgänge
  - Notizen
  - Halluzinationen
- Wird gespeichert und beim Ebenenwechsel mitgenommen.

## Grafik, Klang, Bedienung

- **Neue Netze und Texturen:**
  - Taschenlampe (Metallgehäuse, leuchtendes Glas)
  - Monozelle „1,5 V“
  - Notizzettel (vergilbt, liniert, Handschrift, Kaffeefleck)
  - Notausgang: Tür mit Sicke, Aufkleber und Abrieb; Zarge; Panikstange; Leuchtschild mit Fluchtweg-Piktogramm und Pfeil
- **Neue Klänge:** Lampenschalter, Batteriewechsel, Papier, Notausgangstür, Stromausfall und -rückkehr, „der Andere“, drei Hotel-Atmosphären
- **Hilfe und Hinweiszeile** erklären Taschenlampe, Notizen, Notausgänge und Halluzinationen.
- **Neue Testskript-Befehle:** `sanity N`, `haunt 1|2|3`

## Kompatibilität

- Eigene Dateien: `settings_v7.json`, `saves_v7\`, `liminal_v7.log`, `benchmark_v7.txt`.
- Beim ersten Start werden die V6-Einstellungen übernommen (nur lesend).
- V6-Spielstände erscheinen mit „(V6)“ und werden als V7-Stand fortgeführt. Die Lampe startet dann mit 70 %, Notizbuch und Laufbuch beginnen leer.
- Fundstücke und Notausgänge verbrauchen keine Zufallszahlen des Generators. Die Welt aller alten Stände bleibt bitgenau gleich (`worldgen_test`: 29 658 Prüfungen, 0 Fehler).
- V6 und alle älteren Versionen bleiben unverändert und lassen sich parallel spielen.

## Tests

| Test | Ergebnis |
|---|---|
| `gameplay_test` (neu: Taschenlampe, Fundstücke, Notizen, Batterie, Laufbuch, Ebenenwechsel, Halluzinationen) | 103 Prüfungen, 0 Fehler |
| `worldgen_test` (alter Aufbau bitgenau) | 29 658 Prüfungen, 0 Fehler |
| `layout_stats --check` (alle drei Ebenen) | OK |
| `layout_stats --finds --check` (in jedem Seed ein Notausgang) | OK |
| `supply_balance_test` | OK |
| `audio_test` | 39 Prüfungen, 0 Fehler |

**Benchmark** (1920×1080, Qualität Hoch, RTX-5070-Laptop): 60 FPS bei 4,0 ms GPU-Zeit. Das Hotel braucht 4,9 ms.

---

# LIMINAL V6 – Änderungen gegenüber V5

V6 ist die bisher größte Überarbeitung von LIMINAL. Die Engine ist dieselbe (C++20, eigener Direct3D-11-Renderer, alles prozedural, keine Bild- oder Tondateien). Fast jedes System ist erweitert oder neu.

## Kurzfassung

- **Weitläufigere Welt:** halb so viele Räume je Fläche, doppelt so große Räume, breite Passagen, Lichthöfe, offene Übergänge, 50 % längere Sichtachsen
- **Räumlicher Klang:** Jede Leuchtstoffröhre summt einzeln und hörbar aus ihrer Richtung. Räume hallen passend zu Größe und Material. Wände dämpfen. Ferne Geräusche im Gebäude.
- **Neue Grafikgeneration:** GTAO, indirektes Licht, Lichtschächte mit Schatten, Kontaktschatten, Parallax-Oberflächen, Gebrauchsspuren, saubere Spiegelungen, neue Farbabstimmung
- **Grafik passt sich der Hardware an:** automatische Qualitätswahl per Messung, dynamische Auflösung, Texturkompression, Wahl der stärkeren GPU auf Laptops
- **Glaubwürdiger Getränkeautomat** mit Glasfront und echter Tiefe dahinter
- **Eigener Körper:** ein animiertes Strichmännchen in der Ego-Perspektive, dazu Schulterkamera (F5)
- **Runde Erkundungs-Minimap**, die nur zeigt, was du wirklich gesehen hast
- **Springen, Ausdauer, Items fallen lassen**, deutlich seltenere Energy Bars
- **Alte Spielstände bleiben gültig:** V3-, V4- und V5-Stände laden ihre bitgenau gleiche Welt.

---

## Welt

| Messwert (Level 0, 5 Seeds × 25 Sektoren) | V5 | V6 |
|---|---|---|
| Räume je Hektar | 99 | 51 |
| Median-Raumgröße | 36 m² | 80 m² |
| Fläche in Kleinsträumen (< 40 m²) | 15 % | 2,5 % |
| Weitläufige Bodenfläche (≥ 3 m zur Wand) | 33 % | 48 % |
| Längste Sichtachse (Mittel) | 20 m | 30 m |
| Versorgungsräume je Fläche | 100 % | 87 % |

- Größere Räume, breitere und höhere Flure, viel weniger Kleinsträume
- **Passagen** als Zwischenräume auf den Hauptachsen: Säulengang mit Nischen, hohe Wandelhalle oder breiter Gang
- **Fluchten:** Flure gehen offen ineinander über, große Hallen öffnen sich breit zum Flur.
- **Lichthöfe** (10–18 m hoch) und mehr Dunst in großen Bereichen: Wie weit ein Raum reicht, ist oft nicht sofort zu erkennen.
- Der neue Aufbau ist eine Daten-Überlagerung (`layout_v6` in den Leveldateien). Der alte Aufbau bleibt bitgenau erhalten und wird für alte Spielstände verwendet.

## Klang und Atmosphäre

- **Leuchten als einzelne 3D-Quellen:**
  - 100-Hz-Summen eines magnetischen Vorschaltgeräts (50-Hz-Netz) mit Obertönen, feinem Zischen der Röhre, langsamen Schwankungen und gelegentlicher Unruhe
  - vier Varianten; Industrieleuchten brummen tiefer, Notleuchten schnarren
  - Lautstärke nach Nähe, hörbare Richtung
  - flackernde Röhren knacken synchron zum Bild
  - dezent: etwa 64 % des alten, raumfüllenden Brummens
- **Raumakustik:**
  - Nachhallzeit nach Sabine aus Volumen, Fläche und Materialien (Teppich, Akustikdecke, Beton, Fliesen)
  - Nachhall-Bus nach Freeverb-Prinzip
- **Räumliches Hören:**
  - Laufzeitunterschied zwischen den Ohren
  - Kopfschatten für Quellen hinter dir
  - Luftdämpfung über die Entfernung
  - Verdeckung durch Wände: leiser und dumpfer, über das Kachelraster berechnet
- **Ferne Geräusche,** selten und zur Zone passend: Tür, Metall, Grollen, Klopfen, Dampf, Knarzen. Mit sinkendem Verstand häufiger.

## Grafik

- **GTAO** (Ground-Truth Ambient Occlusion) mit Mehrfachreflexion und Glanzlicht-Verdeckung, statt SSAO
- **Indirektes Licht** im Bildraum (ein Bounce)
- **Volumetrisches Licht** als eigener Durchgang: analytisches Streulicht je Leuchte, Schatten aus dem Höhenfeld (Lichtschächte durch Türen und zwischen Säulen), Abstrahlrichtung, treibender Dunst
- **Kontaktschatten** für Items, den eigenen Körper und feine Kanten
- **Parallax-Occlusion-Mapping:** echte Tiefe in Fugen, Mörtel, Gittern und Deckenrastern
- **Gebrauchsspuren**, weltfest:
  - Wasserflecken mit Rändern an Decken
  - Schmutz am Wandfuß und Staub in Ecken
  - Schrammen und Ruß
  - Wasserläufe an Wänden
  - feuchte Stellen im Teppich
- **Spiegelungen:** Streifenartefakte bei flachem Blick behoben (seit V5)
- **Staub in der Luft**, kontrastadaptives **Nachschärfen**
- **Farbabstimmung:** Split-Toning (kühle Schatten, warme Lichter), Halation, filmähnliches Korn
- **Getränke- und Snackautomat** als eigenes Modell:
  - Lackgehäuse mit Chromrahmen und Leuchtschild
  - Bedienfeld mit LED-Anzeige, Tastenfeld, Münzschlitz und Geldscheinprüfer, Ausgabeklappe
  - Kratzer, Rost, Aufkleberrest
  - **Glasfront mit Interior Mapping:** Fächer mit Flaschen, Dosen und Snacks, die sich mit dem Blickwinkel verschieben; die Scheibe spiegelt den Raum.
- Kisten aus verwittertem Fichtenholz, Theken mit echtem Furnier

## Leistung und Skalierung

- **Automatische Qualität** bei Neuinstallation:
  - Schätzung aus der Grafikkarte, dann Messung der echten GPU-Zeit im Hauptmenü
  - Gewählt wird die höchste Stufe mit Reserve für 60 FPS.
  - Gemessen wird neu, wenn sich Grafikkarte oder Auflösung ändern.
- **Stärkere GPU auf Laptops:** Bei zwei Grafikchips nimmt LIMINAL jetzt die leistungsstarke GPU (DXGI 1.6). Vorher lief es oft auf der integrierten Grafik. `--gpu sparsam` wählt die sparsame.
- **Dynamische Auflösung** hält die Ziel-Bildrate.
- **Stufen** Niedrig / Mittel / Hoch / Ultra nach Aufwand, nicht nach einem bestimmten PC. Ultra darf teuer sein.
- **Neue Optionen:** Beleuchtungsqualität (Zahl gleichzeitig gerechneter Leuchten), Partikel, Nachbearbeitung, Fenstergröße, Texturkompression
- **Texturkompression** BC1/BC5: etwa 45 % weniger Grafikspeicher. Große Texturen sind auf Karten mit wenig Speicher gesperrt.
- **GPU-Zeitmessung** je Durchgang in der Debug-Anzeige (F3) und im Benchmark
- Messwerte (1920 × 1080, GPU-Zeit je Bild):

  | Grafikkarte | Niedrig | Mittel | Hoch | Ultra |
  |---|---|---|---|---|
  | NVIDIA GeForce RTX 5070 Laptop | – | – | 4,4 ms | 7,6 ms |
  | Intel Arc 140T (integriert) | 4,1 ms | 9,5 ms | 11,4 ms | – |

## Spiel

- **Strichmännchen-Körper** in der Ego-Perspektive:
  - prozedurale Animation für Gehen, Sprinten, Springen, Fallen und Landen
  - Hände greifen zu und führen Items zum Mund.
  - Die Kamera sitzt im Kopf.
- **Schulterkamera** mit Wandkollision (F5)
- **Runde Minimap**, die nur Erkundetes zeigt (Sichtlinien durch das Kachelraster), und große Karte (M). Das Erkundete wird gespeichert.
- **Springen** mit Nachsicht an Kanten. Treppen hinab ohne Mini-Stürze.
- **Ausdauer:** begrenzter Sprint, Erschöpfung, Keuchen
- **Items fallen lassen** (Q, Strg+Q für den ganzen Stapel). Sie bleiben in der Welt liegen und werden gespeichert.
- **Energy Bars** nur noch etwa ein Drittel so häufig, dafür stärker und als seltener Fund markiert. Die Sanity je Versorgungsraum bleibt gleich (geprüft).

## Oberfläche und Komfort

- gestapelte, weich eingeblendete Meldungen
- Aufnehmen-Hinweis mit echter Tastenbelegung
- Überblendungen beim Start, Laden und Wechsel ins Hauptmenü
- neues Grafikmenü mit Hilfetexten zu jeder Option

## Behobene Fehler

- Streifen in Bodenspiegelungen bei flachem Blick
- Treppen hinab: Die Figur fiel jede Stufe einzeln (mit Landegeräusch).
- Zonennamen mit Umlauten wurden falsch großgeschrieben.
- Die Kartenaktualisierung hing von der Bildrate ab.
- Beim Fortsetzen alter Spielstände wurde „(V4)“ Teil des Namens.
- Auf Laptops mit zwei Grafikchips lief das Spiel auf der schwächeren GPU.

## Kompatibilität

- Spielstände aus V3, V4 und V5 werden gefunden und als V6-Stand fortgeführt. Ihre Welt bleibt bitgenau gleich.
- Neue Spiele nutzen den V6-Aufbau. Der Spielstand merkt sich das (`"layout": 6`).
- Eigene Dateien: `settings_v6.json`, `saves_v6\`, `liminal_v6.log`. Beim ersten Start werden die V5-Einstellungen übernommen (nur lesend). V5 und V6 lassen sich parallel spielen.

## Für Entwickler

- **Neue Module:**
  - `app/soundscape`: Leuchten, Raumakustik, ferne Klänge
  - `app/autotune`: Grafik nach Hardware
  - `gameplay/body`: Strichmännchen
  - `gameplay/explore`: Erkundungskarte
  - `render/gpu_timer`, `render/texture_compress`, `render/texture_vending`
- **Neue Tests:**
  - `audio_test`: Klang ohne Audiogerät
  - `layout_stats`: Weltstruktur alt gegen neu
  - `supply_balance_test`: Item-Balance, beide Weltstrukturen
  - `gameplay_test`: Sprung, Treppen, Karte, Körper, Ausdauer, Ablegen
- `worldgen_test` prüft weiterhin, dass der alte Aufbau bitgenau der V4-Welt entspricht.
- **Leveldaten:** `layout_v6` überlagert Zonen und Raumtypen, ergänzt Raumtypen und steuert Passagen und offene Übergänge (`structure`). Details in der README unter *Erweitern → Neues Level*.
