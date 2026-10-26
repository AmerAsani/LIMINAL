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
  - Die prozedurale Bewegung des Spiels treibt die neuen Körper weiter an. Sie wird je Wesen nachgeschärft (Haltung, Atmung, Gewichtsverlagerung).
- **Material:**
  - Je Punkt Höhlung, Kante und Feuchte.
  - Die Wesen-Haut bekommt das Durchscheinen der Hand (Shader) und eine nasse, glänzende Variante.
- **Import:** `ent_<name>_l0..l2.glb` mit Skin. Das Spiel nimmt sie, wenn alle drei Stufen da sind, sonst das alte Modell. Das Protokoll weist die Quelle nach.
- **Briefings:** `KREATUR_<Name>.md` aus den Wikidot-Seiten (Stand 26.10.2026), in eigenen Worten.
