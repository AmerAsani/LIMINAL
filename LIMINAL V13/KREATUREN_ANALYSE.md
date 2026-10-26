# Kreaturen – Analyse (V13, Teil 2)

Stand: 26.10.2026. Grundlage sind der Quellcode von V13 (bis hierher gleich V12), das Protokoll eines Laufs mit allen Wesen und Aufnahmen aus festen Kameras (`docs/screens/creatures/vorher/`).

## 1. Woher die Modelle kommen

| Schritt | Ort | Was passiert |
|---|---|---|
| Skelett | `src/anim/creature.cpp` (`entitySkeleton`) | Je Wesen Gelenke der Ruhepose und Knochen (Start-/Endgelenk, Elternknochen, Stärke). Smiler 28 Knochen, Hound 60, Deathmoth 32, Gestalt 14, Nest 2, Bündel 27 |
| Form | `src/render/entity_models.cpp` (`buildEntity`) | Distanzfeld aus Ellipsoiden, Kegeln, Kästen mit weicher Vereinigung, dazu Rauschen als Verschiebung. Vernetzt mit `render/sculpt.cpp`, je Stufe vereinfacht |
| Gewichte | ebenda (`toPart`) | aus dem Abstand jedes Punkts zu den Knochen (erlaubte Knochen je Teil) |
| Material | `src/render/texture_entity.cpp`, `shaders/mesh_gbuffer.hlsl` (`shadeEntity`) | Prozedurale Texturen (Haut, Haar, Horn, Zahn, Flügel, Chitin, Fell, Nest, Mikro-Normalen), triplanar in der Ruhepose. Dazu je Punkt Höhlung/Kante (uv) |
| Bewegung | `src/anim/creature_anim.cpp` | Prozedural: Gelenkpositionen je Bild (Gang, Galopp, Hinken, Pirschen, Knurren, Flug, Haar-Physik, Ziehen des Bündels) |
| Hochladen | `src/render/renderer.cpp` (`startFigureGeneration`) | Bei **jedem Start** im Hintergrund neu gebaut (23 s), danach als `ent_<name>_l0..l2` angemeldet |

**Zur Frage „neues Modell oder alter Pfad, Cache, Fallback?“:**
- Es gibt **nur einen Pfad**. `data/models/` enthält keine Datei `ent_*` (geprüft: 0 Dateien).
- Es gibt **keinen Cache auf der Platte**: Modelle werden nie gespeichert, nur im Speicher gebaut.
- Es gibt **keinen Fallback**: ein anderes Modell für Wesen existiert nicht.
- Die Wesen in V10, V11 und V12 waren deshalb immer das Ergebnis desselben Codes.
- Was in V11/V12 „neu“ wirkte, waren Änderungen an Parametern in `entity_models.cpp`. Genau das zählt laut Auftrag nicht als Neubau.

## 2. Nachweis zur Laufzeit

Für den Neubau zeigt das Protokoll jetzt, welche Quelle jedes Wesen hat und welches Netz gezeichnet wird:

```
Wesen Smiler:  L0 10 Teile, 120 146 Dreiecke | Quelle prozedural (render/entity_models.cpp) | Inhalt 27ef45ecd161ae82
Wesen Hound:   L0 18 Teile, 148 174 Dreiecke | Quelle prozedural (...) | Inhalt 31e122c32bc68d28
Wesen Deathmoth: L0 9 Teile, 61 208 Dreiecke | ... | Inhalt 79a204260ad95dfa
Wesen Gestalt: L0 1 Teil,   31 228 Dreiecke | ... | Inhalt 36c5d6d260ef9bb3
Wesen Nest:    L0 1 Teil,  113 666 Dreiecke | ... | Inhalt c9689c534e09d42f
Wesen Buendel: L0 11 Teile, 142 052 Dreiecke | ... | Inhalt 7f84e37e10071be1
Wesen im Bild: Netz ent_hound_l0 = ID 133 ...
```

- **Inhalt** ist ein FNV-Hash über Punkte und Indizes aller Stufen.
- Kommt ein Wesen aus Blender, steht dort `Blender: ent_<name>_l0.glb (FNV …), …` mit dem Hash der Datei.
- `Wesen im Bild` meldet einmal je Netz, welches beim Zeichnen tatsächlich aufgelöst wurde.

## 3. Befund je Wesen (Vorher-Bilder)

| Wesen | Befund |
|---|---|
| Smiler | Rauchsäule mit **Comic-Gesicht**: runde Augen und ein vollkommen gleichmäßiger Bogen gleicher Zähne. Der Körper ist eine glatte Röhre mit Zungen. Er wirkt wie ein Aufkleber, nicht wie ein Wesen. |
| Hound | Aus Kegeln zusammengesetzt: Röhrenbeine, Rippen als einzelne Stäbe. Wirkt wie ein Skelett aus Rohren. Seitlich wie ein Hund aus Draht, ohne Muskeln, Sehnen und Haut über den Knochen. Das Haar hängt in Bündeln. |
| Deathmoth | Formen erkennbar, aber das Wesen ist im Bild winzig (nach Wikidot hat das Weibchen 6–7 cm Körperlänge). Rüssel und Schwanzborsten stammen aus der Fandom-Fassung, Wikidot nennt beim Weibchen Mandibeln und Legebohrer. |
| Gestalt | Schaufensterpuppe aus Kapsel und Kugelkopf, gleichmäßig schwarz, unten zerfasert. |
| Nest | Torbogen aus Blasen und Wülsten mit Ästen. Keine Schichtung, kein Harz, keine Fasern, keine eingeschlossenen Reste. |
| Bündel | Rosa Schläuche ohne Gelenke, Hände oder Muskeln; glatt glänzend. Wirkt wie eine Spinne aus Würstchen. |

**Gemeinsame Ursachen:**
1. Die Formen bestehen aus wenigen Grundkörpern. Anatomie (Muskelbäuche, Sehnen, Gelenkköpfe, Fettpolster, Hautfalten) fehlt.
2. Die Netze sind sehr fein (31k–148k Dreiecke in Stufe 0), aber ohne Formdetail.
3. Gewichte nach Abstand verzerren bei starken Beugungen.
4. Material: nur triplanare Kacheln; kein Durchscheinen, Feuchte nur über die Rauheit.

## 4. Neuer Weg (Entscheidung E-021)

- **Formen in Blender** (`tools/blender/creatures/`, Hintergrundmodus):
  - Jedes Wesen ist eine eigene Anatomie aus Dutzenden geformter Teile. Muskeln, Sehnen und Knochenpunkte sind einzeln gesetzt.
  - Die Teile werden als Distanzfeld mit weichen Übergängen ausgewertet (numpy).
  - Hautdetail kommt als Verschiebung dazu: Falten, Adern, Poren, Räude, Harzschichten.
  - Vernetzt wird mit OpenVDB bei 1,5–3 mm Voxelgröße; die drei Stufen entstehen durch Reduzieren.
- **Gewichte:** Jedes Formteil gehört zu einem Knochen. Ein Punkt erhält die Gewichte der Teile, die ihn formen, gemischt nach ihrem Abstand. So bleibt die Haut an der Anatomie, nicht am nächsten Knochen.
- **Skelett:**
  - Die Knochen sind genau die Knochen des Spiels (`--dump-skeletons` → `skeletons.json`, Namen `B00`…).
  - Die prozedurale Bewegung des Spiels (Gang, Atmung, Gewichtsverlagerung, Kopfbewegung, Zustandswechsel aus V11/V12, `anim/creature.cpp`) treibt die neuen Körper an. Ergebnis siehe Abschnitt 5.
- **Material:**
  - Je Punkt Höhlung, Kante und Feuchte.
  - Die Wesen-Haut bekommt das Durchscheinen der Hand (Shader) und eine nasse, glänzende Variante.
- **Import:** `ent_<name>_l0..l2.glb` mit Skin. Das Spiel nimmt sie, wenn alle drei Stufen da sind, sonst das alte Modell. Das Protokoll weist die Quelle nach.
- **Briefings:** `KREATUR_<Name>.md` aus den Wikidot-Seiten (Stand 26.10.2026), in eigenen Worten.

## 5. Ergebnis (26.10.2026)

| Wesen | Runden | Modell (Dreiecke L0 / L1 / L2) | Quelle im Protokoll | Briefing mit Kritikrunden |
|---|---|---|---|---|
| Hound | 6 | 56 000 / 16 000 / 4 660 | `Quelle Blender: ent_hound_l0.glb` | `KREATUR_Hound.md` |
| Smiler | 5 | 42 000 / 13 000 / 3 800 | `Quelle Blender: ent_smiler_l0.glb` | `KREATUR_Smiler.md` |
| Clump | 7 | 80 000 / 22 000 / 6 670 | `Quelle Blender: ent_clump_l0.glb` | `KREATUR_Clump.md` |
| Gestalt | 5 | 24 000 / 8 000 / 2 500 | `Quelle Blender: ent_gestalt_l0.glb` | `KREATUR_Gestalt.md` |
| Deathmoth | 4 (zwei in der Vorschau, drei im Spiel) | 34 000 / 10 000 / 3 000 | `Quelle Blender: ent_moth_l0.glb` | `KREATUR_Deathmoth.md` |
| Nest | 4 | 38 000 / 11 000 / 3 190 | `Quelle Blender: ent_nest_l0.glb` | `KREATUR_Deathmoth.md` (Abschnitt Nest) |

- **Neubau, keine Parameteränderung:** Jedes Wesen ist ein neues Modell aus `tools/blender/creatures/<wesen>.py`; der alte Bau in `render/entity_models.cpp` wird nur noch genutzt, wenn eine `.glb` fehlt (Nachweis im Protokoll je Start).
- **Silhouetten:** Mensch auf allen vieren (Hound), Kopf ohne Körper in Rauch (Smiler), Knäuel aus Gliedern (Clump), stehender Mensch ohne Gesicht (Gestalt), Insekt (Motte), gewachsene Masse an einer Tür (Nest) – keine gemeinsame Grundanatomie.
- **Material:** Haut der Wesen mit Durchscheinen (Shader wie die Hand), Höhlung und Kanten je Punkt, nasse Glanzstellen; neu `ent_silhouette` (Gestalt, schluckt Licht), `ent_darkskin` (Smiler), `ent_resin` (Nest).
- **Bewegung:** Die neuen Körper laufen mit der bestehenden prozeduralen Bewegung; in den Aufnahmen im Gang (`lauf1`, `lauf2` je Wesen) keine Durchdringung und kein Reißen gesehen. **Nicht neu geschrieben** wurde die Bewegung selbst (Gang, Atmung, Gewichtsverlagerung, Zustandswechsel stammen aus V11/V12). Das ist ein offener Punkt für eine spätere Version: die Bewegungen auf die neue Anatomie abstimmen (z. B. Fingerbewegung beim Clump, Lidschlag der Hound-Lider).
- **Vorher/Nachher aus denselben Kameras:** `docs/screens/creatures/vorher/` und `docs/screens/creatures/<wesen>/nachher_*.jpg`; die Wesen in ihren Ebenen: `docs/screens/level_wesen.jpg`.

## 6. Nachprüfung V13.2 (ehrliche Bewertung der übrigen Modelle)

Aus denselben Kameras wie in Teil 2 (`creature_shots.ps1`: nah 1,8 m, mittel 4 m, fern 10 m, Seite, in Bewegung, Lampe im Dunkeln, dunkel), Level 0, nach den Bewegungsrunden von V13.2. Bilder: `docs/screens/analyse_v13_2/<wesen>.jpg`. Das Nest ist in V13.2 neu entworfen (E-029) und hier nicht noch einmal bewertet.

| Wesen | Was trägt | Was schwach ist (ehrlich) | Maßnahme |
|---|---|---|---|
| Hound | Die Silhouette „Mensch auf allen vieren“ liest sich aus jeder Entfernung; das Haar verdeckt das Gesicht, schwingt nach; im Lampenlicht fahl und abgemagert. | Aus der Nähe wirken Hände und Füße am Boden schmal und glatt; die vordere Zahnreihe sieht noch etwas wie ein Gebiss aus (offen seit V13). | Keine Änderung in V13.2. Für V14: Krallen und Fingerglieder schärfer, Zähne ungleichmäßiger. |
| Smiler | Im Dunkeln nur Augen und Lächeln – genau wie die Seite es beschreibt; die Rauchzungen lösen den Körper auf. | **Im hellen Raum** wirkt der Kopf wie eine glatte, glänzende schwarze Kugel – fast comicartig; die dunkle Masse hat kaum Oberfläche. Im Spiel erscheint er nur im Dunkeln oder im Stromausfall, deshalb fällt es dort nicht auf; in einer Vorführung im Licht schon. | Keine Änderung in V13.2 (fünf Runden in V13 auf die Dunkelheit abgestimmt). Für V14: poröse, unregelmäßige Kopfoberfläche, Rand der Masse unruhiger – im Licht prüfen. |
| Clump (Bündel) | Knäuel aus Armen und zwei Beinen, der lange Arm hebt sich ab; im Lampenlicht klebrig glänzend. | Die Glieder sind zu gleichmäßig dick und glatt – aus der Mitte wirkt es wie Teile von Schaufensterpuppen; kaum Muskeln, Sehnen, Falten an den Gelenken; das Maul darunter ist aus den Standardkameras nicht zu sehen. | Keine Änderung in V13.2. Für V14: Muskelzüge und Gelenkfalten je Glied, verschiedene Hauttöne stärker, Maul aus Augenhöhe sichtbar. |
| Gestalt | Ein schwarzer, gesichtsloser Mensch, der Licht schluckt; unten zerfasert. | Von der Seite sehr dünn; im Lampenlicht bleibt sie schwarz, aber die Form ist nur ein Umriss ohne Tiefe – das ist gewollt (unbestätigte Gestalt), wirkt aber nah etwas flach. | Keine Änderung (gewollt). |
| Deathmoth | Fliegend als Motte lesbar, in den Kritikrunden von V13 aus der Nähe detailliert (Flügelmuster, Fühler). | Mit 45 cm Spannweite auf 4 m im hellen Raum kaum zu sehen; im Spiel sitzen sie im Dunkeln und fliegen im Lampenlicht – dort sichtbar. | Keine Änderung. |

**Neue Wesen (V13.2):** eigene Bewertungen und Kritikrunden in den Briefings `KREATUR_Faceling.md`, `KREATUR_SkinStealer.md`, `KREATUR_Wretch.md`, `KREATUR_DeathRat.md`, `KREATUR_Fensterwesen.md`. Zweibeiner teilen einen **Baukasten** (`tools/blender/creatures/human.py`: Kopf, Hals, Rumpf, Arme, Hände, Beine, Füße, Gesicht, Haar mit Maßen aus dem Skelett) – keine geteilte Grundanatomie eines anderen Wesens: jedes Wesen hat eigene Maße (Skelett), Dicken, Haut, Züge und Kleidung.
