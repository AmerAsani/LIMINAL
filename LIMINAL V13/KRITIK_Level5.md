# Kritikrunden Level 5 (Abnahmeschranke d)

Laufweg `python tools/qa/walkway.py level5 --round rN --rooms 18 --seed 3`, Bögen `docs/screens/level5/wegbilder_rN_*.jpg`.
Grundlage: `LEVELSPEC_5.md` (Wikidot Level 5).

## Runde 1 (Struktur, Ausstattung V13.2)

Laufweg 1 002 m durch 18 Räume, 251 Bilder. Geprüft wurden die Bögen 1 und 4.

Befunde:
- **Hotelgänge wie im Wiki:** Damasttapete, dunkle Täfelung, antike Kerzenleuchter, Porträts, Türen, roter Teppich, Sofas und Beistelltische.
- **Halle:** heller Marmor mit Kronleuchtern.
- **Schwarze Bilder** (032, 132, 133, 137, 138, 143): Der Weg führt in unbeleuchtete Zimmer.
  - Das passt zum Wiki: Motten schwärmen aus dunklen Zimmern; die meisten Zimmer sind leer.
  - Eingerichtete Zimmer haben Tischlampe und Deckenlicht (`level5.py`).
- **Keine Änderung.** Die übrigen Bögen 2, 3, 5–7 werden in Runde 2 mit der neuen Ausstattung geprüft.
