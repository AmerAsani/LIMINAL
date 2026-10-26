# Kreatur: Die Gestalt (Level 0)

Quelle: [backrooms-wiki.wikidot.com/level-0](https://backrooms-wiki.wikidot.com/level-0), gelesen am 26.10.2026.
- Autoren der Neufassung: DivineAtlas, Robert Goerman, DrAkimoto.
- Lizenz CC BY-SA 3.0.
- Eigene Worte.

## Was die Seite sagt

- Ob es in Level 0 Wesen gibt, ist unbekannt; direkt gemeldet ist keines.
- Wanderer berichten von **dunklen Gestalten**, die ihnen zwischen den Gängen folgen.
- Viele fühlen sich beobachtet (Skopophobie).
- Unbestätigte Geräusche: Flüstern, bekannte Stimmen, Kratzen hinter den Wänden.
- Ob Halluzination oder Phänomen, ist offen.
- Peripheral Shift: Wo man nicht hinsieht, verändert sich der Raum.

## Umsetzung

**Grundsatz:** Ein Wahrnehmungsphänomen, kein Tier: weit weg, dunkel, ohne Einzelheiten. Es verändert sich nie, während man hinsieht (wie V10–V12).

**Silhouette:**
- Ein Mensch, aber etwas zu lang: Arme reichen bis zu den Knien, der Hals ist lang.
- Die Schultern hängen, der Kopf ist leicht schief geneigt.
- Natürliche Haltung (Gewicht auf einem Bein, eine Hüfte tiefer), keine Puppe.
- Die Ränder lösen sich wie Rauch, unten zerfasert die Gestalt.

**Form:**
- Keine Gesichtszüge, keine Kleidung, keine Finger.
- Aber erkennbare Masse: Schlüsselbein- und Schulterlinie, Rippenbogen, Knie.
- So wirkt sie wie ein Mensch im Gegenlicht, nicht wie ein Kegel.

**Material:** tiefschwarz und matt; ein schmaler Rauchrand. Im Lampenlicht kaum heller (schluckt Licht).

**Bewegung:**
- Steht. Kaum merkliches Atmen in den Schultern.
- Der Kopf neigt sich minimal.
- Die Fasern unten treiben.

**Abgrenzung:** einziges stehendes, menschliches Wesen ohne Detail; kein Körper aus Haut.

## Prüfkriterien

- Aus der Ferne sofort „da steht jemand“, nicht „da steht eine Puppe“.
- Keine Blockformen: Schultern, Arme und Kopf mit natürlicher Masse und Neigung.
- Rand weich, kein hartes Schwarz-auf-Gelb.

## Kritikrunden (Vorschau in Blender und Spielkamera)

Bilder: `docs/screens/creatures/gestalt/` (vorher: `docs/screens/creatures/vorher/gestalt_*`). Modell: `tools/blender/creatures/gestalt.py`, Material `ent_silhouette` (E-021).

| Runde | Befund | Änderung |
|---|---|---|
| 1 (Blender) | Neu gebaut: Mensch im Kontrapost (Gewicht rechts, linkes Knie locker), lange Arme, langer Hals, schiefer Kopf mit Hinterkopf, Kinn und Profil; Schlüsselbeine, Rippenbogen, Becken. Mängel: Schlüsselbeine und Kopfnicker standen wie Stäbe ab, der Brustkorb war ein Ei, die Fasern unten sahen aus wie Wurzeln. | Hals kräftiger, Schlüsselbeine und Kopfnicker weich eingebettet, Brustkorb flacher, neun dünnere, ungleich lange Fasern je Strang. |
| 2 (Spiel) | „Da steht jemand“ – auch aus der Ferne. Aber: der Unterkörper wurde wie beim Smiler ausgedünnt (das Rauchmaterial dünnt mit dem Abstand vom Smiler-Gesicht aus), unter den Armen Lücken, im Lampenlicht rotbraune Haut sichtbar. | Eigenes Material `ent_silhouette`: Rauchrand ohne das Ausdünnen um das Smiler-Gesicht; Albedo 0,25 % (schluckt Licht); Füllung unter den Achseln. |
| 3 (Spiel) | Im Lampenlicht kaum heller, Umriss eines Menschen. Falten an Schlüsselbein und Achsel erschienen als Löcher (der Rauchrand dünnte auch innere Flächen mit seitlicher Neigung aus). | Für `ent_silhouette` dünnt der Rand nur bei streifendem Blick aus. |
| 4 (Spiel) | Obere Brust geschlossen: keine Lücken mehr. Aber beim Vergleich mit dem Vorher-Bild wirkte der Rand jetzt hart schwarz auf Gelb – gegen das Briefing („Rand weich“). Die Löcher aus Runde 3 waren Lücken im Modell, nicht der Rauchrand. | Rand wieder breiter ausdünnen (zwischen Smiler und Runde 3). |
| 5 (Spiel) | Weicher, rauchiger Rand, der Körper dicht und ohne Lücken. Aus der Ferne „da steht jemand“, aus der Nähe ein Mensch ohne Gesicht im Gegenlicht, im Lampenlicht kaum heller, unten in Fasern aufgelöst. Abgenommen. | – |
