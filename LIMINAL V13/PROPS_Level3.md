# Ausstattung Level 3 „Electrical Station“ (V13.3 Teil 1)

Alle Objektklassen der Ebene, die nicht anderen Ebenen gehören, in Blender neu (`tools/blender/models/models_l3.py`),
Grundlage `LEVELSPEC_3.md`/`LOOK_3.md` (Wikidot Level 3: alte elektrische Anlage aus Ziegelgängen, weiße rostige
Gitter, Rohre mit schwarzer Flüssigkeit und Schlammlachen, Generatorräume, Fertigungsstraßen mit Förderbändern,
Fenstern mit rotem Ziegel und Gitterrosttreppen, Kesselräume, Sanktuarien mit griechisch-römischen Säulen, einer großen
Engelsstatue auf einem Sockel mit unentzifferten Zeichen und Bildern vom Engel, manche übermalt; Schädel, Teile von
Zügen, verwitterte Frauenstatuen ohne Kopf und Arme, weiße Tuchbahnen; Aufzug nach Level 5).

**Bauweise:**
- Eigener Geometrie-Kern im Modul (`Kit`: Geometrie je Material in einem bmesh), Bausteine: Kasten mit Fase, Zylinder,
  Drehkörper (auch Teilbögen), Rohr/Profil entlang eines Pfads mit rotationsminimierenden Rahmen, Prisma, gebogene
  Pfade mit Radius, Catmull-Rom-Pfade, Profile mit Gehrung (Bilderrahmen, Sockelgesimse).
- Normteile: Sechskantschrauben/Muttern, Nieten, Schlitzschrauben, Vorschweißflansche mit Dichtung, Handräder,
  Schweißraupen, Bügelschellen, Profilschienen.
- Gebrauchsspuren als eigene Flächen 0,2–1,5 mm über der Oberfläche: Rost, Abplatzer, Läufer, Öl, Staub, Ruß, Kalk,
  Griffspuren, Kratzer; auf Zylindern als gewölbte Flecken (`cdecal`, `cpoly`).
- Schrift und Zeichen aus dem Zeichenatlas des Spiels (`look=sign`/`stencil` mit UV): Schrankbeschriftung „UV-3“,
  „0/I“ am Hauptschalter, Skalen „A/V/KW“, Etagenziffern, „AGGREGAT 2“, Schablone „L3-07“, Warndreieck, Zettel.
- Skulpturen (Engel, Torso, Schädel) mit dem Distanzfeld-Werkzeug der Wesen (`creatures/sdf.py`, OpenVDB) aus
  anatomischen Teilen, eigenen Feldern (Gewand als Loft mit Falten, Flügel als gehauene Platte, Bruchflächen) und
  Verwitterung (Poren, Unebenheit), danach Reduktion und Materialien nach Höhlung (Schmutz in Falten, Staub oben).
- Tücher mit der Stoffsimulation von Blender (Tuch fällt über verdeckte Möbel bzw. hängt an einer Latte).
- Rauheit/Metallizität je Material über `r=`/`m=`.

<!-- KLASSEN -->

<!-- VARIANTEN -->

<!-- RUNDEN -->

<!-- PRUEFUNGEN -->
