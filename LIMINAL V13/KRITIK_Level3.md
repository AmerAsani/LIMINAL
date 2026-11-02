# Kritikrunden Level 3 (Abnahmeschranke d)

Je Runde Laufweg (`python tools/qa/walkway.py level3 --round rN --rooms 18 --seed 3`), Bögen `docs/screens/level3/wegbilder_rN_*.jpg`.
Grundlage: `LEVELSPEC_3.md` (Wikidot Level 3).

## Runde 1 (Struktur nach dem Umbau, Ausstattung V13.2)

Laufweg 201 m durch 27 Räume, 51 Bilder. Befunde:
1. **Ziegelgänge wie im Wiki:** roter Hauptgang mit schwarzen Rohren an einer Wand, Verteilern und Engelsbildern; Netze
   in grauem, hellem und braunem Ziegel.
2. **Neue Gangnetze über lange Strecken unlesbar** (009–011, 014–015, 022–027, 032–034, 050): Röhren nur alle 5 m in
   engen Gängen, davon 25 % tot; das Wiki will „sehr dunkle Bereiche“, aber nicht ganze Netze ohne Licht.
3. **Graue Netze kahl** (020–046): ohne Rohre und Kästen auf langen Stücken.
Änderung: Röhren in allen Gängen alle 4 m, 20 % tot (dunkle Bereiche bleiben durch tote Gruppen und Kammern).
Punkt 3 wird mit der neuen Ausstattung (Agent Level 3) in Runde 2 geprüft.

## Runde 2 (mit der neuen Ausstattung: 24 Klassen, 38 Modelle)

Laufweg 201 m durch 27 Räume, 51 Bilder (`wegbilder_r2_*`). Befunde:
1. **Gänge lesen sich als Level 3:** schwarze Rohre mit Ventilen und Flanschen an einer Wand, Kabelleitern unter der Decke, Engelsbilder, Verteiler, Gitterfenster. Die grauen Netze sind seit Runde 1 hell genug (020–035).
2. **Zwei dunkle Strecken im roten Hauptgang** (009–011, 014–015): tote Röhren, im Wiki ausdrücklich „sehr dunkle Bereiche“. Bleibt so.
3. **Aufzug nach Level 5:** Das Aufzugstor ist für 1,4 m gebaut, die Öffnung war 1 m; die Laibungen steckten in der Wand (Restmangel aus `PROPS_Level3.md`).

Änderung: Die Öffnung ist jetzt 2 m breit, das Tor auf 1,35 skaliert wie in Level 5.
