# LIMINAL V13 – Fortschritt

Wiedereinstieg: diese Datei zuerst lesen, dann `DECISIONS.md` (V13 ab E-019).
Auftrag: „LIMINAL – V13: Finger-Fix, Kreaturen-Neubau, Texturen, Bugfixing, Qualitätsupdate, Multiplayer (nur wenn nachweislich sicher)“.
Vorgeschichte: `docs/archiv/PROGRESS_V12.md`, `docs/archiv/REPORT_V12.md`.

## Fertig
- [x] V13 aus V12 (Stand `e8481b6`) kopiert, Branch `liminal-v13`. V12 eingefroren (Manifest `Sicherungen/v12_frozen_2026-10-26.txt`, 25 550 Dateien mit SHA-256). Version 13 (eigene Einstellungen, Spielstände, Protokolle; Einstellungen aus V12 werden nur gelesen). Build frisch, Tests grün (gameplay 351/0, audio 106/0, worldgen 29 658/0, supply OK).

- [x] Teil 1: Hand und Taschenlampe. Analyse und vier Kritikrunden in `HAND_ANALYSE.md`, Bilder in `docs/screens/viewmodel/v13_*`, Entscheidung E-020.
  - Neue Hand: Käfig mit Subdivision, Skelett mit 21 Knochen, Griff gelöst, Posen `grip` und `press`.
  - Engine: Skin und Posen aus glTF; Schaltfolge mit Daumendruck; Ärmel mit Bündchen; Debug-Kamera `vmcam`, Ausblenden `vmdebug`.
  - Tests: gameplay 357/0.
  - Hilfsbefehle:
    - Hand bauen: `blender -b --factory-startup --python tools/blender/build_models.py -- vm_hand_l vm_flashlight vm_flashlight_on`;
    - Vorschau: `tools/blender/preview_hand.py -- <ordner> rest|grip|final|press`.

- [x] Teil 2: Kreaturen (E-021). Alle sechs Wesen neu in Blender (`tools/blender/creatures/`): Hound 6, Smiler 5, Clump 7, Gestalt 5, Deathmoth 4, Nest 4 Kritikrunden; Vorher/Nachher aus denselben Kameras (`docs/screens/creatures/`). Commits `137ca9e`, `fffc771`.
  - Bauen: `blender -b --factory-startup --python tools/blender/creatures/build.py -- <wesen> --preview <ordner>`, danach `cmake --build build/Release` (kopiert data/); im Protokoll muss `Quelle Blender` stehen.
- [x] Teil 3: Texturen – `TEXTUR_AUDIT.md`, Kachelbruch (E-022), Leistung (G-Buffer +0,06–0,18 ms) und Grafikspeicher (939–1003 MB). Commit `fcfaede`.
- [x] Teil 6: Mehrspieler bleibt aus (E-024): Bedrohungsmodell, Entscheidung, Prüfschicht `src/net` mit 72 Prüfungen und Fuzzing, `tools/check_no_network.py`; Starter-Signatur (E-023). Commits `e72ba4d`, `3285333`.

- [x] Teil 4: Qualitätsupdate – `QUALITAET_LEVEL.md` (feste Kameras, Lampe, Wesen in ihren Ebenen, Karten geprüft, Lore-Abgleich mit den Wikidot-Revisionen vom 26.10.).
- [x] Teil 5: Bugsuche – `BUGLISTE.md` (Smoke-Test aller Ebenen und Menüs, Speichern/Laden, fünf Auflösungen, Bildzeiten, Speicher); B-003 und B-004 behoben.
- [x] Abschluss: `REPORT_V13.md`; V12-Manifest geprüft (25 550 Dateien, 0 verändert), V10 ohne Änderung; Tests gameplay 357/0, audio 106/0, worldgen 29 658/0, supply OK, net 72/0.

## V13 Fortsetzung (V13.2) – Auftrag „Offene Punkte, Mottennest, Entity-Abgleich, Multiplayer, GitHub-Upload“
Wiedereinstieg: diesen Abschnitt lesen, dann `DECISIONS.md` ab E-025.

- [x] Schritt 0: GitHub-Sicherung lokal vorbereitet (E-025, `../GITHUB_UPLOAD.md`): `origin` ist öffentlich, daher kein Push; Branches `liminal-v11`/`liminal-v12`, Tags `v11`/`v12`, README, `.gitignore`, Prüfungen. Befehle für ein privates Repository stehen bereit.
- [x] Entity-Abgleich: `ENTITY_ABGLEICH.md` (zehn Ebenenseiten, Entity-Übersicht, Entity-Seiten; Revisionen vom 26.10.2026). Nest belegt (E-026), Reihenfolge der fehlenden Wesen (E-027). Smiler reagiert auf Lärm.
- [x] B-002: Texturen Ebene für Ebene, Ladespitze 3,1 GB → 0,68 GB (2048 px: 11,1 → 1,3 GB), Messung je Ebene in `BUGLISTE.md`.
- [x] B-001: Ärmel am gebeugten Ellbogen (E-028): Ruhepose 54 Grad, Dual Quaternions, Lampenhaltung von außen; Test und Bilder (`docs/screens/b001/`).
- [x] Animationen der Wesen (Hound, Smiler, Clump, Gestalt, Deathmoth; `ANIMATIONEN.md`) und Nest-Neuentwurf in fünf Runden (E-029, `docs/screens/nest/`)
- [x] Fehlende Wiki-Wesen (E-031): Faceling (fünf Varianten, vier Verhaltensweisen je Ebene), Fensterwesen (Level 188), Skin-Stealer (wahre Gestalt, getarnt), Wretch (träge, schnell, flehend), Death Rat (mit Fell, haarlos, Schrei-Ereignis). Je Wesen Briefing `KREATUR_<Name>.md` mit mindestens drei Kritikrunden (`docs/screens/<wesen>/`), Blender-Modell, Skelett, Bewegung (`ANIMATIONEN.md`), Verhalten nach Wikidot (Level-JSON `presence.<wesen>`), Klang, Mehrspieler (Protokoll v3), Tests (gameplay 446/0). Bugs B-012 bis B-016. Commit `4734414` und folgende.
  - Bilder im Spiel: Skriptbefehl `near <wesen> <abstand>` (Spieler mit freier Sicht vor das Wesen), `presence <wesen>` (erzwingen), `figstate wave|talk|plea|strike|variant`.
  - Prüfhilfe Licht: Einstellung `debug_view` (3 Normalen, 1 direktes Licht, 9 Albedo).
- [x] Mehrspieler als Beta (E-030): Transport, Protokoll v2 (V13.2 später v3: Gesten der neuen Wesen), Host/Gast, Wesen für mehrere Spieler, Menüs, Wiederverbinden, Koop-Stand; Tests in `MULTIPLAYER_TESTS.md` (Transport 52/0, Koop 169/0 + 5/0, Netz-Fuzzing 300 s 2/0, Sanitizer, zwei Prozesse, Langlauf 60 min 9/0, 30 min mit Speicherproben 9/0 – kein fortlaufendes Leck). Bugs B-005 bis B-012. Im Koop läuft der Klang im Menü weiter.
- [x] Modellier-Analyse der übrigen Wesen (`KREATUREN_ANALYSE.md` 6): Smiler-Kopf verbessert (Vorher/Nachher aus 1 m), Bündel-Versuch verworfen, Hound/Gestalt/Motte geprüft ohne Änderung.
- [x] Abschluss: alle Tests grün (gameplay 446, audio 106, worldgen 29 658, supply OK, net 72, Transport 52, Koop 169 vollständig), V12-Manifest 0 Abweichungen, V10 unverändert, `REPORT_V13_2.md`, README, `../GITHUB_UPLOAD.md` (Nachtrag 6), Tag `v13`. Kein Upload (E-025).

Hilfsskripte (Scratchpad `v13`): `b.sh [gameplay|all]` (bauen + Tests), `mem_sweep.ps1` / `mem_trace.ps1` (Speicher), `elbow_tp.ps1` (Kamera um den Spieler, Skriptbefehle `figcam` ohne Vorführung in der Schulteransicht, `egopose 1`), `elbow_shots.ps1` (`vmcam`), `crop.py`.

## V13.3 Teil 3 – Arm der Lampenhand
- [x] Analyse und fünf Kritikrunden in `HAND_ANALYSE.md` (Abschnitt „V13.3: Arm“), Entscheidung E-032, Bilder `docs/screens/viewmodel/v13_3_*` (je Runde Spielerkamera in 12–13 Zuständen, Debug-Kamera um den Arm, Nahansichten der Hand, Armkette von außen; Vergleiche `v13_3_vergleich_*.jpg`).
  - Unterarm 27 cm, Handgelenk 30° gestreckt (Finger punktgleich), Faust im Ego-Bild 43° gedreht: Ellbogen auf statt 16 cm über Schulterhöhe.
  - Ärmel mit dem Querschnitt einer Jacke, Raffung, Bündchen mit Kante, Falten; Jacke in Stufe 0 feiner.
  - Tests: gameplay 446/0 (B-001 grün), audio 106/0.
  - Hilfen (Scratchpad `v13_3arm`): `armgeo.py`/`armtable.py` (Rechnung der Armkette), `arm_shots.ps1 -P rN [-Wand]` (Ego und Debug), `fig_shots.ps1 -P fN [-Ego 0]` (von außen), `glbdiff.py` (Punktvergleich der glb), `vergleich.py`.

## Als Nächstes (Vorschläge für V14)
- Bewegung der Wesen auf die neue Anatomie abstimmen.
- ORM-Texturen komprimieren (≈ 360 MB VRAM), Ladespitze senken (B-002).
- Rechte Hand mit dem neuen Handmodell; Ellbogen der Kapuzenjacke (B-001).

## Bekannte Fehler
- Rechte Hand (Gesten) ist weiter die prozedurale Hand.

## V13.3 (PROMPT_V13_3.md) – Stand 27.10.2026, unterbrochen (Nutzungsgrenze)

Fertig:
- Werkzeuge: `tools/qa/` (game.py, propdump.py, propsheet.py Kontaktboegen 4 Winkel x 50 cm/2 m x Licht/Lampe, validate_models.py); Engine: freie Kamera (cam/camat), testlight, worldlight, propdump, --level-file, --script @datei, Varianten name@N.glb, r=/m= je Material.
- Praesenz aufgeteilt (presence_agent.hpp, presence_kin.cpp), feste Materialien 160, Koop kRoles 40, Ereignisarten im Koop bis 127 (Fehler: Scream/Swing/Pull kamen als Squeak an).
- Wiki-Recherche (nur Wikidot) aller Ebenen und Wesen: Notizen im Scratchpad wiki/ (noch in STRUKTURTYP.md/ENTITY_ABGLEICH.md zu uebertragen).

Laeuft in eigenen Worktree-Branches (Agenten): Mehrspieler (Teil 5), Audio-Fehler (Teil 6), Arm (Teil 3), Ausstattung je Ebene (Level 1 A/B, 2, 3, 0+Fundstuecke, 5, 5.3+188, 903+37, 36). Ergebnisse pruefen und zusammenfuehren.

Als Naechstes: Raummetrik-, Schilder-, Aehnlichkeits- und Wegbilder-Werkzeuge; STRUKTURTYP.md, BIOM_AUDIT.md, LEVEL0_STANDARD.md; Pfuetzen/Wasser im Shader; pipe_run-Verlauf in den Karten (1-m-Stuecke, Boegen); Wesen-Agenten (Duller, Arms, Growler, Nguithr'xurh, Wrangler, Hound-Rudel, grosser Clump, Praetorian/Motten L2/L3, Smiler-Leuchtstab, Cannibal Cuisine, Fenster L2, Samantha+Fleisch, Ants, Woodlins/Watchers als Interpretation, Ratten-Fallen, purpurne Wesen, Faceling-Varianten, Randwesen); dann Abnahmeschranke je Ebene (1, 2, 3, 0, 5, 5.3, 188, 903, 37, 36), AUFWAND.md, Tests, Upload, REPORT_V13_3.md, SELBSTKONTROLLE.md.

Bekannte Fehler: coop_test --quick meldete unter hoher Last (20 parallele Agenten) einmal „zu schnelle Uhr: 32 Eingaben verworfen“ – ohne Last erneut pruefen. 52 GLB mit Befunden in validate_models.py (werden von den Ausstattungs-/Wesen-Arbeiten behoben).

### V13.3 Zwischenstand 2 (27.10.2026, erneut Nutzungsgrenze)
Fertig seit Stand 1: Raummetrik/Schilder/Aehnlichkeit/Laufweg/Kartenbild/Spot-Werkzeuge (`tools/qa/`), `LEVEL0_STANDARD.md`, `STRUKTURTYP.md`, `BIOM_AUDIT.md`; Level 1: Sektoren Ouroboros, Garden, Fabled, Decken >= 2,8 m, Sichtachsen 36,8 m, Rohre aus 1-m-Stuecken; Pfuetzen als Boden-Abziehbild, Deckenspiegelung im SSR, klares Wasser (Vorher/Nachher `docs/screens/props/wasser/`); Arm (Teil 3) zusammengefuehrt (E-032).
Laufen/unterbrochen (Worktrees, mit SendMessage fortsetzen): Mehrspieler, Audio, Ausstattung L1 A/B, L2, L3, L0+Fundstuecke, L5, L5.3+188, 903+37, 36, L1-Sektoren; Wesen Duller+Arms, Growler+Nguithr'xurh, Wrangler, Smiler-Leuchtstab+Skin-Stealer-Ruf, Level-1-Erscheinungen (Garden, Ouroboros-Arbeiter, Crawler, Jerry).
Als Naechstes: Ergebnisse pruefen und zusammenfuehren; Level-1-Abnahme (Kritikrunden, Laufweg nachher, AUFWAND.md); weitere Wesen (Hound-Rudel, grosser Clump, Praetorian/Motten L2/L3, Cannibal Cuisine, Fenster L2, Samantha, Ants, Woodlins/Watchers, Ratten-Fallen, purpurne Wesen, Faceling-Varianten); Level 2, 3, 0, 5, 5.3, 188, 903, 37, 36.

### V13.3 Zwischenstand 3 (27.10.2026, Nutzungsgrenze)
Neu: Level 1 Teil B (Blender-Modelle) uebernommen und mit Runden 3-5 abgeschlossen (`PROPS_Level1_B.md`); propsheet stellt den Spieler in jede Kabine (vorher schwarze Bilder bei fernen Kabinen); `reviewsheet.py`, `smoke.py` (Level 1: GPU 3,6 ms, RAM-Spitze 730 MB, 0 Fehler).
Level 2 als Naechstes (Befund): 68 von 220 Raeumen verstellt (Maschinen an der Tunnelwand + Trichterwagen im Gleistunnel lassen < 0,9 m), Hallen (Anlage, Packlager, Bueroblock) zu gross gegenueber dem Tunnelnetz (S 0,78) – Hallen verkleinern, Tunnelnetz nach Osten erweitern, Maschinen nur wo >= 1,0 m frei bleibt, Brownout- und Macchina-Bereich ergaenzen.
Agenten: alle ausser „Props Level 1 Blender-Modelle“ (vom Nutzer gestoppt, Arbeit uebernommen) laufen bzw. sind unterbrochen; mit SendMessage fortsetzen.


### V13.3 Zwischenstand 4 (27.10.2026, ca. 18 Uhr)
- **Level 1 fertig bis auf Schranke c (Wesen) und g (Aufwand):**
  - Struktur: alle zehn Ebenen grün, Raummetrik mit korrigierter Messung (E-035).
  - Ausstattung: Teil A, Teil B und die Sektoren zusammengeführt, dazu das neue Modell `arcade_gothic`.
  - 7 Kritikrunden (`KRITIK_Level1.md`); Laufweg 836 m durch 18 Räume; Smoke-Test grün (GPU 4,1 ms, max 8,3; RAM 808 MB; 0 Fehler).
- **Wesen:** Duller und Arms zusammengeführt (466 Prüfungen, 0 Fehler); Duller-Haut auf Dunkelgrau korrigiert; 29 Deckenlüftungen in den Gängen.
- **Agenten:**
  - laufen: Mehrspieler `aa6ecde9fd4b8ff11`, Growler+Nguithr'xurh `aed8bb58cf363c623`, Wrangler `a40d77c4c87074e60`;
  - unterbrochen, in dieser Reihenfolge fortsetzen: Smiler-Leuchtstab+Skin-Stealer-Ruf `a2a8250ed528a5ff1`, L1-Erscheinungen `a8e9e99616db9fe9e` (dazu Entity 99 „The Game Master“ als Gemälde, Wikidot Rev. 39), Audio `a1a310e23cd626c63`, dann die Ausstattung L2, L3, L0, L5, 5.3+188, 903+37, 36.
- **Level 2:** Kritikrunde 1 erledigt (Brownout); die weiteren Runden folgen nach der Ausstattung.
- **Offene Wesen-Wellen** (ENTITY_ABGLEICH: noch Status „fehlt/bewusst weggelassen“):
  - Hound-Rudel und großer Clump;
  - Deathmoths in Level 2 und 3;
  - Cannibal Cuisine und Windows;
  - Samantha und Ants;
  - Woodlins und Watchers;
  - Death-Rat-Fallen;
  - Faceling-Varianten;
  - Randwesen;
  - Wow-Check der vorhandenen Wesen.

### V13.3 Zwischenstand 5 (27.10.2026, Nutzungsgrenze)
- Kritikrunde 1 (Struktur) erledigt für Level 2, 3, 0 und 5: `KRITIK_Level2.md`, `KRITIK_Level3.md`, `KRITIK_Level0.md`, `KRITIK_Level5.md`.
- `room_metrics.py` erneuert veraltete Ausstattungs-Abzüge selbst.
- Agenten wegen der Nutzungsgrenze angehalten (mit SendMessage fortsetzen; ihre Arbeit liegt committet in den Worktree-Zweigen):
  - Mehrspieler `aa6ecde9fd4b8ff11`: war beim 60-Minuten-Langlauf;
  - Growler+Nguithr'xurh `aed8bb58cf363c623`: war beim gameplay_test;
  - Wrangler `a40d77c4c87074e60`: war beim Blender-Bau.
- Danach in dieser Reihenfolge:
  - Smiler/Skin-Stealer `a2a8250ed528a5ff1`;
  - L1-Erscheinungen + Entity 99 `a8e9e99616db9fe9e`;
  - Audio `a1a310e23cd626c63`;
  - Ausstattung L2, L3, L0, L5, 5.3+188, 903+37, 36.
- Offen bleiben die Kritikrunden 1 für Level 5.3, 188, 903, 37 und 36.

### V13.3 Zwischenstand 6 (01.11.2026, Nutzungsgrenze)
- **Zusammengeführt:**
  - Wrangler;
  - Growler und Nguithr'xurh (die Spinne nach meiner Ablehnung neu gebaut, Runden 6–11);
  - Tests danach: gameplay 525/0, audio 106/0, coop 169/0, net 72/0, worldgen 29658/0.
- **Kritikrunde 1 für alle zehn Ebenen erledigt:**
  - 5.3: Gang-Kandelaber;
  - 188: Treppe im hohen Treppenhaus;
  - 903 und 37: keine Änderung;
  - 36: Modelle der Sicherheitskontrolle und Lounge fehlen (Ausstattungs-Agent L36).
- **Entscheidungen und Werkzeuge:**
  - E-036: Laufweg bei Ebenen mit wenigen Räumen;
  - E-037: Zeilenenden;
  - Faceling-Test L2 robust gemacht.
- **Angehalten** (Nutzungsgrenze), fortsetzen mit Workflow resumeFromRunId bzw. SendMessage:
  - Workflow `wf_47b840e5-0cf` (L1-Kette 1: Smiler-Köder/Skin-Stealer-Ansprache, dann Crawler/Garden/Entity 99/Spuren);
  - Workflow `wf_8f47b53b-d0c` (L1-Kette 2: Ouroboros-Arbeiter Index 16, dann Plague Goblins Index 17);
  - MP-Agent `aa6ecde9fd4b8ff11` (war im 60-Minuten-Langlauf).
- **Danach:**
  - Ausstattungs-Agenten L2 `acc032bb367994ffc`, L3 `a6fe00bab51c07b16`, L0 `a1b72bb19ca0e0be3`, L5 `a0e27f49b0224c52f`, 5.3+188 `aa6c5b5bc78be7eb1`, 903+37 `a3d9507a3a2648424`, 36 `a400115d30c290821`;
  - Audio `a1a310e23cd626c63`;
  - Wesen-Wellen L2/L3/L5+ (Deathmoths, Jerry, Cannibal Cuisine, Windows, Photoshop, Swindlebird, Macchina, Death-Rat-Fallen, purpurne Wesen, Red Knight, Samantha, Ants, Woodlins, Watchers, Entity 18, Ambassadors, Entity 39);
  - Wow-Check der vorhandenen Wesen.

### V13.3 Zwischenstand 7 (02.11.2026)
- **Ausstattung aller zehn Ebenen gebaut und zusammengeführt:** L0 mit Fundstücken, L1, L2 mit Varianten, L3, L5, 5.3, 188, 903, 37 und 36 (je PROPS_*.md).
- **Level 1 abgenommen** (AUFWAND.md).
- **Wesen und Netz:**
  - alle Level-1-Wesen und Ouroboros-Arbeiter im Spiel; Goblins-Rolle 17;
  - Multiplayer (Teil 5) fertig, 60-min-Langlauf, ohne Beta-Label;
  - Träger- und Spät-Beitritt-Fehler behoben.
- **Tests:** gameplay 647/0, coop 209/0, audio 106/0, net 72/0, worldgen 29658/0, supply OK.
- **Kritikrunden:** Level 2 bis Runde 3, Level 3 bis Runde 2 (Aufzug 2 m).
- **Laufend:**
  - Workflow `wf_25ac8506-59d` (Level-2-Wesen: Fenster, Motten, Ratten, Spuren → Jerry Rolle 18 → Randwesen);
  - Workflow `wf_35cb0125-472` (Level 3: Wretch, Skin-Stealer → purpurne Wesen);
  - Audio-Agent `a1a310e23cd626c63` (Teil 6).
- **Danach:**
  - Level 3: Death Rats (Fallen) und Deathmoths/Praetorianer, Red Knight (Entity 71) prüfen;
  - Level 5 und später: Samantha, Ants, Woodlins, Watchers, Entity 18 (Stimme, Gentleman), Ambassadors, Entity 39, Wrangler-Kesselraum (erledigt);
  - Faceling-Varianten;
  - Wow-Check der vorhandenen Wesen;
  - Hound-Rudel und großer Clump.
- **Offener Befund für Level 903:** Verdeckte Blöcke werfen Schatten (`visualTile`, V6), deshalb bleibt die Wagenseite im Lampenlicht dunkel. Lösung in der Engine oder per Blockschalter prüfen.
