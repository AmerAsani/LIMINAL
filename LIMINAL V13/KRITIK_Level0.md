# Kritikrunden Level 0 (Abnahmeschranke d)

Je Runde Laufweg (`python tools/qa/walkway.py level0 --round rN --rooms 15 --seed 3`; Runde 1 mit `--rooms 25`), Bögen
`docs/screens/level0/wegbilder_rN_*.jpg`. Ab Runde 2 dazu alle Erscheinungen und Fundstücke im Spiel
(`python tools/qa/creatureshots.py level0 --round rN`, Bögen `wesen_rN_*.jpg`, Einzelbilder `build/qa/creatures/level0/rN/`):
Gestalt am Gangende (Licht der Ebene, Lampe, 7 m, 3 m), Halluzinationen (Gestalt im Dunkeln, Stromausfall des Raums),
Leuchtstab (knicken, werfen, am Boden, nur sein Licht) und Beil (Schwung) in der Hand, die Notizen dort, wo das Spiel sie
hinlegt (Lampe aus/an), und die Zonen (Säulenfeld, Bögen, Lochfeld, Dunkelzone mit Lampe, roter Raum, vergilbtes Zimmer,
Naht). Fundstücke am Boden: `python tools/qa/itemsheet.py --round l0_rN --only glowstick,hatchet,note`
(`docs/screens/props/items/l0_rN/`). Maßstab: `LEVEL0_STANDARD.md`.

## Runde 1 (Struktur nach V13.3-Anpassung, Ausstattung V13.2)

Laufweg 212 m durch 25 Räume, 54 Bilder. Befunde:
- **Die Ebene trifft den Standard:**
  - eintöniges Gelb mit Fußleiste;
  - feuchter Teppich;
  - abgehängte Decke mit Rasterleuchten, teils dunkle Felder;
  - Unterzüge über den Durchbrüchen;
  - Steckdosen und Lüftungsgitter;
  - Raumfolgen ohne Wahrzeichen, Sichtachsen über mehrere Räume (Median 25,5 m).
- Kleiner Befund: Die Wände wirken auf 3–4 m glatt. Das Wiki nennt nur „mono-yellow wallpaper“. Die Struktur bleibt; die Tapetenprüfung gehört zur Ausstattungsrunde (Agent Level 0).
- **Werkzeug:** Bilder vor Wandecken (041, 046, 050) entstehen, wenn in allen Richtungen bis ±75° eine Wand näher als 6 m steht.
- **Keine Änderung an der Struktur.**

## Runde 2 (Abnahme: Laufweg, Erscheinungen, Fundstücke; Basis 84cffb6)

Laufweg 205 m durch 24 Räume, 52 Bilder (`wegbilder_r2_1.jpg`, `_2.jpg`, `wegkarte_r2.png`); Wesen und Fundstücke
`wesen_r2_1.jpg` … `_3.jpg`; Fundstücke am Boden `docs/screens/props/items/l0_r2/glowstick.jpg`, `hatchet.jpg`, `note.jpg`.

Befunde:
1. **Tapete (Befund aus Runde 1):** Gleicher Weg wie Runde 1 (Bilder 000, 001, 008, 009 nebeneinander): Mit der Tapete aus
   der Ausstattungsrunde (`wallpaper_l0.py`, 53-cm-Bahnen) zeigen die Wände auf 3–4 m Stoßfugen und leicht verschiedene
   Bahnen; in Runde 1 waren sie glatt wie gestrichen. Aus 1 m bleibt die Raufaser sichtbar. Der Befund ist erledigt.
2. **Teppich:** Mittelwert der unteren Bildzeilen aller 52 Bilder (76, 66, 35), Farbton 45°, wie die festen Ansichten von
   V12 (`01_start.jpg` 75, 66, 37). Im Kontaktbogen wirkt er gräulich, ist aber bräunlich wie in LOOK_0. Keine Änderung.
3. **Sicherungspunkte:** Am Start, im langen Flur, im Säulenfeld, in den Bögen, vor dem Lochfeld, im Osten und an der
   Naht hängt je eine grüne Sicherheitsleuchte an der Decke (`ghost_1`, `zones_7`). Das Modell trägt beidseitig das
   Notausgang-Piktogramm (`PROPS_Level1.md`). Das ist das Notausgangsschild an der Decke, das der Auftrag bemängelt.
   LOOK_0 („keine Schilder“) und `STRUKTURTYP.md` (0) verbieten es. `signs.py` zählte es nicht mit und meldete deshalb
   0. Zwei davon sieht man in der ersten Minute (Start, Flur), und jede ist ein Wahrzeichen (LEVEL0_STANDARD, Abschnitt 2).
4. **Bögen:** Im Durchgang unter dem Bogen liegt eine helle, glatte Platte statt Teppich (`zones_2`; Zeilen 640–690:
   Mittel 72/65/43, Streuung 3,5 gegen 6 im Teppich). Ursache: Die Bögen sind Durchgänge der Art „wide“ (wegen des
   Sturzes bei 2,35 m), und `doorStyle` gibt solchen Türen eine Schwelle und einen glatten Rahmen in der Sockelfarbe.
5. **Stromausfall (Halluzination 2):** Der Raum wird dunkel, aber die Leuchtflächen der Rasterleuchten bleiben weiß
   (`blackout_2`: Leuchten 166 von 255, Decke 35). Ursache: Die Fläche behält 3 % ihres Lichts. Nach der
   Belichtungsanpassung im dunklen Raum ist das noch hell, und es liest sich wie „Lampen an, Raum dunkel“.
6. **Dunkelzone:** Am Wandeck zum Nachbarraum flackern helle, senkrechte Schlieren auf dem Wasser, auch ohne Lampe
   (`zones_4`: 260 Bildpunkte über 60; Prüfstand aus drei Blickwinkeln: sie wandern mit der Wand). Alle Nachbarräume sind
   unbeleuchtet. Ursache: Die Ersatzspiegelung der Decke (`ssr.hlsl`, `ceilingMirror`) prüft nur die Zelle unter dem
   Treffpunkt. Der Strahl geht bis zu 45 m weit durch Wände und spiegelt die Leuchten ferner Räume.
7. **Gestalt** (`gestalt_1` … `_5`): Aus 19 m sofort „da steht jemand“; aus 7 und 3 m ein Mensch ohne Gesicht mit zu langen
   Armen, die Beine lösen sich in Fasern auf. Im Lampenlicht kaum heller. Sie entspricht dem Briefing. Der Rand wirkt
   bei 3 m gesprenkelt. Das ist ein Werkzeugfehler: Das Bild entstand 3 Bilder nach dem Versetzen, und das TAA hatte die
   Rauchkante noch nicht gemittelt. Im Bild mit Lampe (90 Bilder später) ist der Rand weich.
8. **Halluzination 3 (Gestalt im Dunkeln):** Auf den Bildern ist keine Gestalt zu sehen. Laut Protokoll steht sie
   19 m entfernt am Ende eines Gangs, **hinter** dem Spieler. Das ist richtig (niemand erscheint im Blickfeld). Das
   Werkzeug sah aber nicht hin.
9. **Fundstücke:**
   - Leuchtstab in der Hand und am Boden: Das grüne Licht am Boden und an der Decke stimmt (`hand_2` … `_4`;
     Prüfstand: die Decke über dem Stab ist grün, die Decke 6 m entfernt schwarz, also kein Rest-Licht in der Hand).
   - Beil am Boden: Im Licht der Ebene ist der Kopf grauer Stahl und der Stiel Esche mit Maserung. Mit streifender Lampe
     ist der Kopf ganz schwarz (`hatchet.jpg`, Zeile 3): Metall mit 100 % Metallanteil hat ohne Spiegelbild kein
     Streulicht.
   - Notizen: Alle drei liegen dort, wo das Spiel sie hinlegt, am Sockel, und sind lesbar (`notes_1` … `_6`).
10. **Zonen:**
    - Säulenfeld gleichmäßig hell, Lochfeld mit sauberen schwarzen Gruben, roter Raum, vergilbtes Zimmer, Naht mit
      Beton dahinter: wie in LOOK_0.
    - Dunkelzone ohne Lampe schwarz, mit Lampe nasser Beton.
    - Die grüne Leuchte an der Naht: siehe Befund 3.

Änderungen:
- **Sicherungspunkte ohne Leuchte** (`tools/levels/level0.py`, `safe_point`): Die Punkte bleiben (Speichern, Meldung beim
  Vorbeigehen). Die 7 Sicherheitsleuchten mit Piktogramm entfallen. `signs.py` weist solche Leuchten jetzt je Ebene aus
  („Sicherheitsleuchten mit Piktogramm“; Level 0: 0). Die Grenze anderer Ebenen bleibt unverändert.
- **Bogen-Durchgänge mit Teppich und Putz:** Neue Tür-Art `style: room_a|room_b` in der Karte (`generator.cpp`: Boden und
  Laibung wie im Raum a bzw. b). `arch_door` setzt sie für alle 8 Bögen; Sturz und Bogenmodell bleiben.
- **Stromausfall:** Die Leuchtfläche erlischt quadratisch mit dem Ausfall (`gbuffer.hlsli`). Das gilt für alle Ebenen;
  geändert wird nur die Darstellung, nicht der Ablauf.
- **Ersatzspiegelung:** Der Strahl wird Zelle für Zelle von der Wasserfläche bis zur Decke verfolgt (DDA, höchstens 96
  Zellen). An einer Wand, an einer tieferen Decke oder an einem höheren Boden gilt das Umgebungslicht der Wand
  (`ssr.hlsl`).
- **Werkzeug:** Die Gestalt wird erst 40 Bilder nach dem Versetzen aufgenommen. Bei Halluzination 3 dreht sich die Kamera
  zur Gestalt (`face gestalt`), im Dunkeln und danach im Licht. Neue Ansicht `zones_8` blickt in den Bogen-Durchgang.

## Runde 3 (`wesen_r3_1.jpg` … `_3.jpg`, Laufweg `wegbilder_r3_*`: 205 m, 24 Räume, 52 Bilder)

Prüfung der Änderungen aus Runde 2:
- **Laufweg:** Gleiche Strecke wie Runde 2. Mittlere Bildabweichung je Bild höchstens 0,15 von 255 (Bild 001: dort hing
  die Leuchte am Startpunkt), Median 0,04. Die neue Spiegelung verändert die Vorzimmer nicht.
- **Bögen** (`zones_2`, neue Ansicht `zones_8`): Der Teppich läuft durch den Bogen, die Laibung ist Putz wie Bogen und
  Wand. Die dunkle Platte hinter dem rechten Bogen ist die Treppe zum höheren Süden (Prüfstand mit Lampe: zwei
  Teppichstufen). Kein Fehler.
- **Stromausfall** (`blackout_2`, `_3`): Die Leuchten sind im Ausfall dunkelgrau und nicht mehr weiß. Der Gang hinter
  der Öffnung bleibt hell (anderer Stromkreis). So soll ein Ausfall des Raums aussehen.
- **Dunkelzone** (`zones_4`, `_4_lampe`): ohne Lampe ganz schwarz (0 Bildpunkte über 60; vorher 260), mit Lampe nasser
  Beton ohne Schlieren.
- **Naht** (`zones_7`): ohne grüne Leuchte. Der Blick geht durch die gelbe Naht auf den Beton von Level 1.
- **Gestalt** (`gestalt_3`, `_4`): Der Rand ist jetzt auch ohne Lampe weich.
- **Halluzination 3** (`ghost_1` … `_3`): Nach dem Ausfall steht die Gestalt in 19 m Abstand als Umriss in der hellen
  Öffnung am Ende des dunklen Säulenfelds. Im zurückgekehrten Licht steht sie noch dort, mit Lampe kaum heller. Das ist
  die stärkste Szene der Ebene. Die Farben sind bei Verstand 12 blass (gewollte Wirkung des niedrigen Verstands).

Neue Befunde:
1. **Beil in der Hand** (`hand_5`, `_6`; Prüfstand `docs/screens/props/items/l0_beil/swing_vorher.jpg`, 8 Bilder über 0,6 s): Beim Ausholen
   füllt eine riesige Hand die rechte obere Ecke, und die Finger wirken wie ein Schlauch mit Holzmaserung (der Stiel
   direkt vor dem Auge). Die Ursache: Die Hand steht beim Ausholen 0,1 m vor und 0,27 m neben dem Auge. Beim Hieb sieht
   man nur einen dunklen Stummel, nämlich den Nacken des Kopfs von hinten. Als Beil ist das nicht zu erkennen. Das
   betrifft das Beil auf allen Ebenen (Fund in Level 1).
2. **Beilkopf im Lampenlicht schwarz** (Befund 9 aus Runde 2).

Änderungen:
- **Schwung des Beils** (`body.cpp`, nur die Darstellung; der Treffer rechnet weiter mit Auge und Blick):
  - ausgeholt 0,24 m vor, 0,1 m über und 0,3 m neben dem Auge (vorher 0,1 / 0,08 / 0,27);
  - der Kopf ist 0,5 rad um den Stiel gekantet.
  - Ergebnis (`l0_beil/swing_nachher2.jpg`, 5 Bilder): Beim Ausholen sieht man oben rechts Eschenstiel und Stahlkopf in normaler Größe,
    beim Hieb die graue Wange des Kopfs vor dem Teppich.
- **Beilkopf:** Metallanteil 0,6 statt 1,0 (`world_renderer.cpp`). Damit streut der geschmiedete Kopf etwas Licht. Vorher
  und nachher in der Lampenzeile: `docs/screens/props/items/l0_r2/hatchet.jpg` → `l0_r3/hatchet.jpg`.

## Runde 4 (`wesen_r4_1.jpg` … `_3.jpg`, Laufweg `wegbilder_r4_*`: 205 m, 24 Räume, 52 Bilder)

- **Laufweg:** Gegenüber Runde 3 höchstens 0,48 von 255 mittlere Abweichung je Bild (Median 0,05). Nur das Rauschen von
  TAA und Leuchtenflackern ändert sich.
- **Erscheinungen und Zonen:** Alle Bilder außer `hand_5`/`_6` gleichen denen aus Runde 3 (mittlere Abweichung unter 1).
  Der Stand aus Runde 3 hält.
- **Klang der Ebene** (Prüfstand: 450 s Spielzeit stillstehend bei Verstand 30, `audiolog 60`): 17 Einzelklänge
  (Kratzen, Flüstern, Stimmen, Tropfen, ferne Geräusche, Brummen) neben den 10 Schleifen. Keine Meldung
  „Klang fehlt“, also lösen alle Namen aus `level0.json` auf (`hal_scratch_0..2`, `hal_whisper_0..5`, `hal_voices`). Die
  Regie stellte nach gut einer Minute selbst eine Gestalt in 17 m Abstand an ein Gangende (Protokoll). Die
  Phantomschritte und der Ausfall sind in `gameplay_test` geprüft („Stromausfall: Licht geht aus / kommt zurück“, „bei
  klarem Verstand keine Halluzinationen“).

Befunde:
1. **Beil in der Hand** (`hand_5`, `_6`): Die Riesenhand ist weg. Die Hand ist aber offen, und der Stiel steht etwa
   10 cm neben ihr. Ursache: Der Griffpunkt lag am Handgelenk, nicht in den gekrümmten Fingern.
2. **Sicherungspunkt „Der lange Flur“:** Beim Vorbeigehen erscheint „Sicherer Ort · Der lange Flur“.
   `STRUKTURTYP.md` verlangt, diese Benennung als hervorgehobenen Ort zu entfernen. Der Raum hat seit der Strukturrunde
   keinen Namen mehr, der Sicherungspunkt trug ihn noch.

Änderungen:
- **Griff:** Der Stiel liegt 7,5 cm vom Handgelenk in Richtung Knöchel (`body.cpp`), also in der Faust
  (`docs/screens/props/items/l0_beil/swing_griff.jpg`). Er kommt beim Ausholen unten aus der Faust, und beim Hieb
  hält die Hand das Stielende.
- **Sicherungspunkt** heißt „Flur“ (`level0.py`; die Kennung `flur` bleibt, damit Spielstände gültig bleiben).
- **Tests nach Runde 3:** gameplay 981/0, audio 121/0, coop --quick 209/0, net 72/0, worldgen 29658/0, supply OK.

## Runde 5 (Abnahme: alle Erscheinungen `wesen_r5_1.jpg` … `_3.jpg`, Laufweg `wegbilder_r5_*`: 205 m durch 24 Räume, 52 Bilder)

Gleiche Strecke wie in den Runden 2–4, alle Erscheinungen und Fundstücke, dazu neu die Naht bis an die flackernde Wand
nach Level 1 (`zones_9`, mit Lampe `zones_9_lampe`). Fundstücke am Boden `docs/screens/props/items/l0_r5/`.

- **Laufweg:** Gegenüber Runde 4 höchstens 1,6 von 255 mittlere Abweichung (flackernde Leuchten). Die Vorzimmer tragen den
  Standard aus Runde 1 weiter:
  - Gelb mit Fußleiste und Tapetenbahnen, feuchter Teppich, Rasterdecke mit toten Feldern;
  - Unterzüge, Steckdosen, Lüftungsgitter;
  - Sichtachsen über mehrere Räume, kein Wahrzeichen und kein Schild.
- **Gestalt:** aus 19 m „da steht jemand“, aus 7 und 3 m ein Mensch ohne Gesicht mit weichem Rand. Mit Lampe kaum heller,
  die Beine lösen sich in Fasern auf. Wie das Briefing, unverändert seit Runde 3.
- **Halluzinationen:**
  - Gestalt im Dunkeln: Umriss in der hellen Öffnung am Ende des dunklen Säulenfelds.
  - Raum ohne Licht: Leuchten dunkelgrau, der Nachbargang bleibt hell.
  - Klang (Runde 4): alle Klänge vorhanden.
- **Fundstücke:**
  - Leuchtstab: knicken und werfen; grünes Licht am Boden und an der Decke darüber.
  - Beil (`hand_5`, `_6`, `l0_beil/swing_griff.jpg`): Der Stiel liegt in der Faust, beim Ausholen Kopf und Stiel in
    normaler Größe. Am Boden ist der Kopf auch im Lampenlicht grauer Stahl.
  - Notizen: drei Fundorte am Sockel, aus 1,3 m lesbar, mit und ohne Lampe.
- **Zonen:**
  - Säulenfeld, Bögen mit Teppich im Durchgang, Lochfeld mit sauber geschnittenen Gruben;
  - Dunkelzone schwarz, mit Lampe nasser Beton ohne Schlieren;
  - roter Raum, vergilbtes Zimmer;
  - Naht: Die Tapete geht in Beton über, am Ende steht die Betonkammer mit der flackernden Wand nach Level 1.
    Weder grüne Leuchte noch Schild.
- **Prüfungen:**
  - Raummetrik grün (Decke min/Median 2,70/2,90 m, Sicht Median 25,5 m, P90 46,5 m);
  - Ähnlichkeit S 0,891, Ausreißer 14,2 %;
  - Schilder 0, Sicherheitsleuchten mit Piktogramm 0;
  - Modellprüfung 33 Modelle der Ebene 0 Fehler, Halt der Ausstattung 1050 Objekte 0 ohne Halt;
  - Smoke-Test grün (GPU 3,14 ms, max 3,80 ms; `docs/metrics/smoke_level0.json`);
  - Tests: gameplay 981/0, audio 121/0, coop --quick 209/0, net 72/0, worldgen 29658/0, supply OK. Ein erster
    coop-Lauf hatte 1 Fehler im Lauf mit schlechtem Netz (6,4 m Ortsabweichung), die Wiederholung lief ohne Fehler.

Keine weiteren Änderungen. Level 0 ist abgenommen (Eintrag in `AUFWAND.md`).

Restpunkte (bewusst nicht geändert):
- **Rest-Licht am Boden:** Der geworfene Leuchtstab färbt die Decke bis etwa 4 m weit grün. Das ist physikalisch
  plausibel (Licht von unten trifft die Decke steil) und gehört zum Köder-Entwurf von Level 1 (`KREATUR_Smiler.md`).
- **Finger beim Hieb:** Die Hand des Beils schließt sich nicht ganz um den Stiel (Fingerkrümmung 0,85 wie bei allen
  gehaltenen Dingen). Die Faust der Lampenhand ist ein eigenes Modell. Eine zweite Faust für die rechte Hand wäre ein
  neues Asset. Der Schwung dauert 0,72 s.
- **Streiflicht im Prüfstand:** Der ungeknickte Leuchtstab (`l0_r5/glowstick.jpg`, Zeile 3) und der Teppich zeigen unter
  der Prüflampe (8° Erhebung, 1 m) Schattenstreifen. Am Teppich treten sie mit der Spielerlampe aus Augenhöhe nicht auf
  (`notes_2`, `_4`, `_6`). Den ungeknickten Stab unter der Spielerlampe zeigt kein Bild: Er liegt nur in Level 1–3.
