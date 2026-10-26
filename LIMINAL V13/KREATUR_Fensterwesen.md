# Kreatur: Fensterwesen / Windows (Entity 2)

Quelle: [backrooms-wiki.wikidot.com/entity-2](https://backrooms-wiki.wikidot.com/entity-2), Revision 36, gelesen am 26.10.2026 (Abgleich) und erneut beim Bau.
- Neufassung: DrAkimoto; ursprünglicher Eintrag eines anonymen Fandom-Nutzers; Idee: u/glassydude101 (Reddit).
- Lizenz CC BY-SA 3.0.
- Zusammenfassung in eigenen Worten; kein Bild der Seite verwendet.

## Was die Seite sagt

**Gestalt:**
- Ein Fensterwesen ist die körperliche Erscheinung eines räuberischen Gedankenraums. Es bildet sich auf fast jeder Fläche, meist in einem gewöhnlichen Fenster, und nimmt die Form an, die zur Ebene passt.
- Hinter der Scheibe liegt eine verschwommene, verzerrte Landschaft. Sie ist für jeden Betrachter anders und passt selten zu dem, was hinter der Wand wirklich liegt.
- Ein besetztes Fenster enthält einen dunklen, gesichtslosen Umriss eines Menschen. Zeugen meinen darin jemanden zu erkennen, den sie kennen.
- Fängt es jemanden, bildet es einen körperlichen Arm aus, der greift und hineinzieht.

**Verhalten:**
- Lockt mit Flüstern (oft als tröstlich beschrieben) und mit Handzeichen.
- Wer näher als etwa 1–2 m kommt, wird gegriffen und hineingezogen.
- Einmal entstanden, bleibt es an seinem Ort, bis es vergeht.

**Ausweg (Seite):** nicht nähern; ruhig gehen und sich der eigenen Wirklichkeit versichern; keinem plötzlichen Drang nachgeben.

**Ebenen (Abgleich):** Level 188 (der Hof der Fenster), Level 2 (am Gangende).

## Umsetzung

**Modell** (`tools/blender/creatures/window.py`, Skelett `wn`, `data/models/ent_window_l0..l2.glb`):
- Platte mit der Landschaft (eigene Texturebene „Ansicht“: Horizont, Hügel, Felder, Weg, Baumgruppen, verschwommen wie hinter nassem Glas). Sie liegt vor der dunklen Rückwand des Fensters der Ebene; Rahmen, Sprosse und Bank kommen vom Fenster von Level 188.
- Umriss: flache, verschmolzene Formen (Hüfte, Brust, schräge Schulterlinie, Hals, Kopf), schwarz und matt (Material „Umbra“).
- Zwei runde, körperliche Arme mit langen, dünnen Fingern (die Seite: ein körperlicher Arm).

**Bewegung** (`CreatureAnimator::updateWindow`):
- Der Umriss wiegt sich kaum merklich und atmet; er wendet den Kopf dem Betrachter zu und legt ihn schief.
- Locken: linker Oberarm seitlich, Unterarm aufrecht, die Hand winkt zu sich heran.
- Greifen: der Umriss lehnt sich aus dem Fenster, der rechte Arm schießt heraus und streckt sich bis gut 2 m (länger als ein Menschenarm).

**Verhalten** (`Presence::thinkWindow`, `placeWindows`):
- Level 188: drei Fensterwesen in dunklen Fenstern ohne Jalousie im Erdgeschoss, einmal je Ebene gesetzt (Level-JSON `presence.window.count`).
- Flüstern unter 11 m, je näher, desto öfter; jedes Flüstern zehrt am Verstand.
- Winken, sobald man es auf 14 m sieht.
- Unter 1,9 m vor der Öffnung fährt der Arm heraus (Ton „Sog“). Wer nach 0,7 s noch unter 2,3 m davor steht, wird hineingezogen (tödlich). Wer sofort zurücktritt, entkommt; danach 4 s Ruhe.

**Klang:** `window_whisper_0..3` (warm, langsam, nah), `window_pull`.

**Mehrspieler:** Der Host entscheidet, die Gäste sehen Winken und Greifen über die Gesten der Momentaufnahme (Protokoll v3).

**Eigene Auslegung:** die Abstände (Flüstern 11 m, Greifen 1,9 m, 0,7 s), drei Fenster, der Arm, der sich streckt; dass der Umriss sich zum Greifen herauslehnt.

**Nicht umgesetzt:**
- Level 2 (Fensterwesen am Gangende): die Rohrgänge haben keine Fenster; ein eigenes Fenster an Gangenden wäre ein neues Ausstattungsstück.
- „Neugeborene“ und „sterbende“ Fenster (die Seite: entstehen aus Fluchtwunsch, verschieben beim Vergehen Menschen aus der Wirklichkeit) – ohne Spielsystem für Wünsche und Verschiebungen.

## Gleichgewicht

| Wert | Zahl |
|---|---|
| Flüstern | unter 11 m, alle 1,3–5 s, Verstand −0,4 × Nähe |
| Winken | Sicht unter 14 m |
| Greifen | unter 1,9 m vor der Öffnung (seitlich ±1 m); tödlich nach 0,7 s, wenn noch unter 2,3 m |
| Ausweg | gehend (3 m/s) 2,1 m in 0,7 s – zurücktreten reicht |

## Kritikrunden (Spielkamera, Level 188)

Bilder: `docs/screens/fensterwesen/`.

| Runde | Befund | Änderung |
|---|---|---|
| 1 (`r1_umriss_unsichtbar.jpg`) | Landschaft und winkende Hand gut; der Umriss fast unsichtbar – das Rauchmaterial der Gestalt dünnt Ränder aus, bei einer flachen Form ist alles Rand (B-015). Der greifende Arm sah wie ein Stock aus. | Eigenes Material „Umbra“ (schwarz, matt, fest); Arme ruhig dichter am Körper. |
| 2 (`r2_umriss_sichtbar.jpg`) | Der Umriss steht im Fenster – unheimlich. Mängel: kantige „Schulterpolster“ (ein waagrechter Balken); der Arm fuhr zu langsam aus. | Schräge Schulterlinie und runde Kappen; Arm schneller (Feder 7). |
| 3 (`r3_schultern_griff.jpg`) | Schultern gut. Der Arm zielte auf das Auge – zur Kamera hin verkürzt, kaum zu sehen; der Umriss kippte beim Herauslehnen aus dem Bild. | Ziel die Brust (0,75 m unter dem Auge); weniger kippen, weiter heraus. |
| 4 (`r4_lehnt_heraus.jpg`) | Der Umriss lehnt sich heraus, beide Arme greifen nach unten aus dem Fenster; von der Seite deutlich ein Arm aus dem Fenster. Abgenommen. | – |

**Bleibt offen:** Der Umriss ist bewusst flach (ein Schatten hinter Glas); von der Seite gesehen ist er dünn.

## Tests (`gameplay_test`)

Level 188: drei Fensterwesen; auf 4 m Flüstern, nichts weiter; zu nah → der Arm fährt heraus; sofort zurück → entkommen; stehen bleiben → hineingezogen (tödlich).
