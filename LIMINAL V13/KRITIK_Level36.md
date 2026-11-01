# Kritikrunden Level 36 (Abnahmeschranke d)

Laufweg `python tools/qa/walkway.py level36 --round rN --rooms 15 --seed 3`, Bögen `docs/screens/level36/wegbilder_rN_*.jpg`.
Grundlage: Wikidot Level 36 (keine Wesen).

## Runde 1 (Struktur mit den vier ergänzten Bereichen, Ausstattung V13.2)

Laufweg 481 m durch 15 Räume, 121 Bilder.

Befunde:
1. **Wie im Wiki:** Halle mit Terrazzo, Check-in-Schalter mit „36“, Abflugtafeln, Gänge zu den Gates. Meeting Place mit roter Damasttapete und blauen Sitzreihen.
2. **Sicherheitskontrolle leer** (018–020): Die Modelle `xray_scanner`, `security_gate` und `tray_stack` fehlen noch; man sieht nur die verdeckten Kollisionsblöcke als dunkle Flächen am Boden.
   - In der Lounge fehlen ebenso `lounge_chair` und `lounge_table`.
   - Das gehört zum Ausstattungs-Agenten für Level 36 und steht in seinem Auftrag.
3. **Check-in-Schalter:** schwarze Kästen ohne Detail. Auch das baut der Ausstattungs-Agent neu.

Änderung: keine an der Struktur. Runde 2 folgt nach der Ausstattung.
