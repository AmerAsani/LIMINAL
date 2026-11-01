# Kritikrunden Level 188 (Abnahmeschranke d)

Laufweg `python tools/qa/walkway.py level188 --round rN --rooms 15 --seed 3` (7 erreichbare Räume, E-036), Bögen
`docs/screens/level188/wegbilder_rN_*.jpg`. Grundlage: Wikidot Level 188 (Rev. 69).

## Runde 1 (Struktur, Ausstattung V13.2)

Laufweg 214 m durch alle 6 Räume, 54 Bilder.

Befunde:
1. **Hof wie im Wiki:** Atrium mit Mittelpfad, Birkenkreis aus Holzplanken, Fensterwände, darunter erleuchtete Fenster mit Gestalten.
2. **Fehler: Treppe ohne Kopfhöhe** (Bilder 019, 024, 039, 044).
   - Die Treppe stieg im 3 m hohen weißen Gang auf 3 m. Oben ragte der Kopf über die Wände; man sah den Hof und schwarzen Himmel.
   - Ursache: Die Engine legt Treppen in den unteren Raum, und dessen Decke bleibt gleich.
3. **Werkzeug:** Der Laufweg verlangte 15 Räume, die Ebene hat aber nur 6. Er lief deshalb im Kreis (6,8 km, 1 696 Bilder).

Änderungen:
- Die Treppe steigt jetzt in einem eigenen, 6 m hohen Treppenhaus zu einem Treppenabsatz auf 2,7 m. Die verschlossene Tür (Wiki) liegt am Absatz.
- `walkway.py` verlangt höchstens die erreichbaren Räume.

## Runde 2 (gleicher Weg)

207 m durch 7 Räume, 43 Bilder (`wegbilder_r2_*`). Nachweis: Die Treppe liegt im hohen Treppenhaus (020–023), oben bleibt Kopfhöhe, kein Blick nach draußen. Keine weiteren Befunde.
