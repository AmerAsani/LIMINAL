# LIMINAL V11 – Engine-Entscheidung

Stand: 25.10.2026. Grundlage: der tatsächliche Code von V10 (in V11 kopiert, unverändert gebaut und getestet), die offiziellen Lizenzseiten von Epic und Unity, Release-Berichte zu Unreal Engine 5.7 und Unitys Render-Pipeline-Strategie 2026. Quellen am Ende.

**Ergebnis in einem Satz:** V11 bleibt bei der eigenen C++20/Direct3D-11-Engine. Die Schwächen von V10 liegen fast vollständig im Weltaufbau, in den Inhalten und im Klang, nicht im Renderer. Eine Migration würde diese Probleme nicht lösen, dafür aber Monate kosten. Außerdem lässt sie sich in der vorhandenen Umgebung gar nicht durchführen (siehe 4).

---

## 1. Ausgangsbasis: was V10 tatsächlich ist

Der eigene Build von V11 (unveränderte V10-Kopie) läuft durch. `gameplay_test` 193/0, `audio_test` 53/0 und `supply_balance_test` sind grün.

| Bereich | Bestand in V10 (gemessen am Code) | Bewertung |
|---|---|---|
| Umfang | ~32 200 Zeilen C++20 in 11 Modulen, ~2 400 Zeilen HLSL, 5 Testprogramme | Mittelgroß, modular, baut ohne Visual Studio (portable llvm-mingw-Toolchain) |
| Renderer | Deferred: G-Buffer → GTAO → kachelbasierte Compute-Beleuchtung (bis 2048 Lichter) → SSIL (1 Bounce) → SSR → TAA → Auto-Belichtung → Bloom → Tonemapping. Volumetrisches Licht, Parallax-Mapping, BC1/BC5, dynamische Auflösung, Qualitätsstufen, GPU-Zeitmessung je Pass | **Solide.** Moderne Grundtechniken sind vorhanden. 1080p Ultra auf der RTX 5070 Laptop: ~7,5 ms GPU |
| Schatten | Strahlverfolgung durch ein Höhenfeld der Kachelwelt, dazu Kapselschatten für Figuren und Kontaktschatten | Passt zur Kachelwelt. Freie Modelle (Requisiten) werfen aber keine echten Schatten |
| Weltgeometrie | 2,5D-Kachelwelt (1 m), je Kachel Boden- und Deckenhöhe. Räume sind Rechtecke aus einem Sektor-Teiler, der **bitgenau** dem Python-Generator von V4 folgt. Alle Flächen sind achsenparallel | **Hauptursache für den „leeren Kisten“-Eindruck:** Räume ohne Einrichtung, nur Wand, Boden, Decke und wenige Quader-„Einbauten“ |
| Modelle | Nur prozedural: SDF-Modellierung (Ellipsoide, Kegel, Boxen, weich verschmolzen) → Surface Nets → QEM-Vereinfachung, LOD-Stufen, Skinning mit eigenen Skeletten | **Technisch brauchbar.** Schwach sind Formgebung, Proportionen und Materialien |
| Animation | Prozedural: Gelenkpositionen, Zwei-Knochen-IK, Gangzyklen, Ketten-Physik für Haar | Brauchbar. Es fehlen Gewichtsverlagerung, Atmung und Bewegungsabsichten |
| Klang | Eigener WASAPI-Mischer (64 Stimmen), Synthese beim Start: gefiltertes Rauschen plus Sinus mit Exponentialhüllkurven, ein globaler Freeverb, linearer Pegelabfall bis zur Reichweite, **ständig aktive Sättigung** im Ausgang | **Schwächster Teil.** Billiger Klang durch Methode, nicht durch fehlende Engine-Funktionen (Details in `AUDIO_DESIGN.md`) |
| KI | Eine Klasse `Presence` (1 142 Zeilen) für alle Entities, Dijkstra-Distanzfeld 81 × 81, Sichtkegel und Lichtprüfung | Funktioniert, ist aber für mehr Entities zu monolithisch |
| Werkzeuge | Skriptbefehle, Bildschirmfotos mit festem Zeitschritt, `layout_stats`, Testprogramme. Kein Editor | Für textbasierte Entwicklung gut, für Künstler unbrauchbar |
| Wartbarkeit | Klare Modulgrenzen (`world` kennt den Renderer nicht usw.), aber einige sehr große Dateien (`app.cpp` 1 464, `synth.cpp` 1 459, `session.cpp` 1 344 Zeilen) | Gut genug, muss beim Ausbau aufgeteilt werden |

---

## 2. Bewertungsmatrix

Skala 1 (schlecht) bis 5 (sehr gut), bezogen auf **LIMINAL**, nicht auf Spiele allgemein. „Eigene Engine“ meint den Stand *nach* den in Abschnitt 5 geplanten Investitionen. Wo der heutige Stand abweicht, steht das dabei.

| Kriterium | Eigene Engine (V10 → V11) | Unreal Engine 5.7 | Unity 6 (URP) |
|---|---|---|---|
| Grafikqualität, PBR, Licht | 3 (heute 3: PBR und Deferred vorhanden, keine Schattenkarten) | **5** (Lumen, Nanite, Virtual Shadow Maps, MegaLights in Beta, Substrate produktionsreif) | 4 (URP gut, HDRP nur noch Wartung laut Unity-Strategie 2026) |
| Volumetrik, Nebel | 3 (vorhanden) | 5 | 3–4 |
| Animation, Skelette, Kreaturen | 3 (prozedural, eigene Skelette) | **5** (Control Rig, IK Rig, Motion Matching, ML Deformer) | 4 (Mecanim, Animation Rigging) |
| Prozedurale Welt, nahtlose Übergänge | **4** (unendliche, deterministische Kachelwelt mit Streaming, Schichten und Nahtstellen existiert und ist getestet) | 3 (PCG-Framework in 5.7 produktionsreif, aber für Außenwelten gedacht. Unendliche Innenlabyrinthe müssten neu gebaut werden) | 2 (keine eingebaute Lösung) |
| KI, Navigation | 3 (Distanzfelder auf der Kachelwelt) | 5 (NavMesh, StateTree, Mass) | 4 (NavMesh, Behavior-Pakete) |
| Räumlicher Klang, Akustik | 3 nach Umbau (heute 2) | 4 (MetaSounds, Occlusion, Convolution Reverb, Submixe) | 2–3 (Grundfunktionen. Für Profi-Klang braucht es FMOD oder Wwise mit eigener Lizenz) |
| Streaming, LOD, Speicher | 4 (Chunk-Streaming, LOD der Figuren, Texturstufen) | 5 (World Partition, Nanite) | 4 |
| Werkzeuge, Debugging, Profiling | 2 (Skripte, GPU-Timer, Testprogramme, kein Editor) | 5 (Editor, Insights, RenderDoc-Integration) | 5 |
| Wartbarkeit, Erweiterbarkeit | 3 | 4 (große Codebasis, aber Standard) | 4 |
| Entwicklungstempo **für Inhalte mit Künstlern** | 2 | 5 | 4 |
| Entwicklungstempo **im textbasierten Arbeitsablauf dieses Projekts** | **5** | 1 (Blueprints, Materialien, Level und Assets sind Binärdateien `.uasset`/`.umap`, die nur der Editor sinnvoll bearbeitet) | 2 (Szenen und Prefabs sind YAML, aber Inspektor-zentriert) |
| Migrationsaufwand | — | Sehr hoch: kompletter Neubau aller ~32 000 Zeilen in UE-C++/Blueprints, inklusive Generator, Spielstände und Tests | Sehr hoch: kompletter Neubau in C# |
| Lizenzkosten | Keine (eigener Code, LLVM/mingw-Laufzeiten mit freizügigen Lizenzen) | 0 bis 1 Mio. USD Bruttoumsatz je Produkt, darüber 5 % Lizenzgebühr (Epic-Store-Umsätze ausgenommen) | Personal kostenlos bis 200 000 USD Umsatz und Finanzierung. Pro 2 310 USD je Platz und Jahr (ab 12.01.2026) |
| Abhängigkeiten | Windows, D3D11 | Epic-Konto, Launcher, ~60–100 GB Installation, EULA | Unity-Konto, Hub, Editor-Version |
| Risiko für ein stabiles Release | Mittel: eigene Bugs, aber volle Kontrolle | Mittel bis hoch: Neubau, Lernkurve, Performance-Fallen (Lumen/Nanite auf Laptop-GPUs) | Mittel bis hoch: Neubau, Strategiewechsel bei Render-Pipelines |

**Gewichtung** für LIMINAL, nach den Vorgaben im Auftrag: Grafik 20 %, Animation 15 %, Klang 20 %, Welt/Übergänge 15 %, Tempo im realen Arbeitsablauf 15 %, Risiko (Migration und Release) 15 %. Für das Risiko gilt: eigene Engine 4, Unreal 2, Unity 2.

| Kriterium (Gewicht) | Eigene Engine | Unreal 5 | Unity 6 |
|---|---|---|---|
| Grafik (20 %) | 3 | 5 | 4 |
| Animation (15 %) | 3 | 5 | 4 |
| Klang (20 %) | 3 | 4 | 2,5 |
| Welt/Übergänge (15 %) | 4 | 3 | 2 |
| Tempo im realen Arbeitsablauf (15 %) | 5 | 1 | 2 |
| Risiko (15 %) | 4 | 2 | 2 |
| **gewichtete Summe** | **3,60** | 3,45 | 2,80 |

Der Abstand zu Unreal ist klein und hängt fast nur an zwei Zeilen: Arbeitsablauf und Risiko. Unreal gewinnt deutlich bei Grafik, Animation und Werkzeugen. Die eigene Engine gewinnt bei Welt, Arbeitsablauf und Risiko. Den Ausschlag geben deshalb die Punkte in Abschnitt 3 und 4.

---

## 3. Wo die Probleme von V10 wirklich liegen

Die Beschwerden über V10 lassen sich auf konkrete Ursachen zurückführen. Für jede ist geprüft, ob eine andere Engine sie lösen würde.

| Beschwerde | Ursache im Code | Löst Unreal/Unity das? |
|---|---|---|
| Generische, leere Räume | Generator erzeugt Rechtecke mit Wand, Boden, Decke. Es gibt keine Ausstattungsebene (Rohre, Leuchtenkörper, Schilder, Pfützen, Kisten, Autos …) und keine Raumkomposition | **Nein.** Auch in UE muss jemand die Räume entwerfen und die Requisiten bauen. PCG verteilt nur, was es gibt |
| „Roblox“-Kreaturen | Grundformen zu einfach, Proportionen schwach, Materialien einfarbig, Smiler als Cartoon-Gesicht auf schwarzem Klumpen | **Nein**, nicht ohne Künstler oder gekaufte Modelle. Mit Fab-Assets ja, aber Fab-Inhalte sind seit 2025 überwiegend kostenpflichtig und für Backrooms-Kreaturen passend kaum zu finden |
| Billiger Klang | Synthese aus Sinus plus Rauschen mit Exponentialhüllkurven, 12-s-Schleifen mit periodischen Ereignissen, ein Freeverb für alles, linearer Abfall, Dauer-Sättigung | **Teilweise.** MetaSounds und Convolution Reverb sind besser als der heutige Mischer, aber die Klänge selbst müssten trotzdem jemand erzeugen oder lizenzieren |
| Künstliche Beleuchtung | Lampen ohne Leuchtenkörper, keine Schatten von Requisiten, gleichmäßige Lichtraster | Teilweise (Lumen, VSM). Ein besseres Lichtdesign braucht es trotzdem |
| Systeme passen nicht zusammen | `Presence` und `GameSession` als große Sammelklassen | Nein, das ist Architektur, nicht Engine |

**Folgerung:** Das meiste, was V10 billig wirken lässt, ist Inhalt und Design. Eine Engine liefert Werkzeuge, aber keine fertigen Backrooms-Räume, keine Kreaturen und keine Klänge.

---

## 4. Warum kein Vergleichsprototyp in Unreal oder Unity gebaut wurde

Der Auftrag sieht einen Vergleichsprototyp vor, falls die Entscheidung unklar ist. Folgende harte Hindernisse stehen dem entgegen:

1. **Keine der Engines ist installiert** (geprüft: kein Epic Games Launcher, kein Unity Hub, kein Blender).
2. Beide brauchen zur Installation ein **Benutzerkonto mit Anmeldung** (Epic Games Launcher bzw. Unity Hub). Konten anlegen oder mich anmelden darf ich nicht. Das müsstest du selbst tun.
3. Unreal braucht 60–100 GB Platz und lädt mehrere Stunden.
4. Selbst installiert wäre ein UE-Prototyp in diesem Arbeitsablauf kaum zu bauen. Materialien, Blueprints, Level und importierte Assets sind Binärdateien, die nur der Editor sinnvoll erstellt. Ich arbeite über Code und Textdateien.

Der Prototyp (Phase 4) entsteht deshalb nur in der eigenen Engine. Bewertet wird er an konkreten, nachprüfbaren Zielen statt an Marketingversprechen: Bildschirmfotos, gemessene Frame-Zeiten, Klangmessungen (Pegel, Spektrum, Schleifennähte, Nachhallzeiten) und exportierte WAV-Dateien zum Anhören.

---

## 5. Entscheidung und was sie verlangt

**V11 wird mit der eigenen Engine entwickelt.** Damit die Entscheidung nicht nur „weiter wie bisher“ bedeutet, investiert V11 gezielt in die Teile, die laut Abschnitt 3 die Qualität begrenzen:

1. **Ausstattungsebene (Dressing):** Echte Modelle (Leuchtenkörper, Rohre, Kabeltrassen, Pfeiler mit Fasen, Schilder, Kisten, Regale, Fahrzeuge, Pfützen, Bewehrung …) werden nach Regeln je Raumtyp platziert und je Chunk zu wenigen Zeichenaufrufen zusammengefasst.
2. **Raumkomposition statt Zufallsrechtecke:** Level-spezifische Aufbauregeln, Sichtachsen, Orientierungspunkte und erzählende Details.
3. **Wasser:** eine eigene Oberfläche mit Spiegelung und Brechung, dazu Schwimmen (Level 37 und 7, Pfützen in Level 1).
4. **Klang-Engine neu:** physikalisch plausibler Abstand und Luftdämpfung, kopfbezogene Richtungsfilter, Feedback-Delay-Netzwerk-Hall mit Frühreflexionen je Raumgröße, Schallweg durch Türen statt durch Wände, Busse, ein echter Begrenzer, Variation bei jedem Abspielen. Dazu ein neues Syntheseset aus geschichteten, physikalisch inspirierten Bausteinen.
5. **Kreaturen neu:** neue Formen, Schichtmaterialien und Bewegungen, eine Entity-Architektur mit Zustandsautomaten und gemeinsamer Wahrnehmung.

**Nachtrag (semi-prozeduraler Weltaufbau, `WORLD_GENERATION.md`):** V11 baut die Ebenen jetzt als feste, entworfene Karten mit variabler Belegung. Das erhöht den Wert eines visuellen Level-Editors, also einen Vorteil von Unreal und Unity. Die Entscheidung bleibt dennoch bestehen, weil die Karten als Textdaten entworfen, automatisch geprüft und als Draufsicht dargestellt werden. Sobald Karten von Menschen in einem Editor gebaut werden sollen, gehört dieser Punkt zur Neubewertung.

**Wann die Entscheidung neu zu prüfen ist:** Sobald das Projekt mit menschlichen 3D-Künstlern oder einem Budget für lizenzierte Assets weiterarbeitet. Dann kehren sich Abschnitt 3 und 4 um, und Unreal Engine 5 ist für Grafik, Animation und Werkzeuge klar die stärkere Grundlage. Ein Umstieg sollte dann **zwischen** zwei Versionen geplant werden, nicht mitten in einer.

---

## 6. Quellen

- Unreal Engine Lizenz (offizielle Seite, abgerufen 25.10.2026): kostenlos bis 1 Mio. USD Bruttoproduktumsatz, darüber 5 %, Umsätze im Epic Games Store lizenzgebührenfrei – https://www.unrealengine.com/license
- Unreal Engine 5.7 (November 2025): PCG produktionsreif, MegaLights Beta, Substrate produktionsreif – https://www.unrealengine.com/news/unreal-engine-5-7-is-now-available, https://cgpress.org/archives/unreal-engine-5-7-released.html
- Megascans nicht mehr kostenlos ab 2025 – https://www.cgchannel.com/2024/10/epic-games-has-made-megascans-free-to-all-but-only-until-the-end-of-2024/
- Unity Preise und Runtime-Fee-Ende (offizielle Seite): Personal bis 200 000 USD, Splash-Screen in Unity 6 optional, Runtime Fee am 12.09.2024 gestrichen, Pro 2 310 USD/Platz/Jahr ab 12.01.2026 – https://unity.com/products/pricing-updates
- Unity Render-Pipeline-Strategie 2026: HDRP nur Wartung, Fokus URP – https://unity.com/topics/render-pipelines-strategy-for-2026
