# Kritikrunden Level 2 (Abnahmeschranke d)

Je Runde: Laufweg (`python tools/qa/walkway.py level2 --round rN --rooms 18 --seed 3`), Bögen
`docs/screens/level2/wegbilder_rN_*.jpg`, Karte `wegkarte_rN.png`; Befunde, Änderungen, Nachweis in der nächsten Runde.
Grundlage: `LEVELSPEC_2.md` (Wikidot Level 2).

## Runde 1 (Struktur nach dem Umbau, Ausstattung noch V13.2 plus Macchina-Platzhalter)

Laufweg 201 m durch 25 Räume, 51 Bilder.

Befunde:
1. **Wartungstunnel wie im Wiki:** kreidiger, schlammbrauner Beton, Maschinen und Rohrbündel an einer oder beiden Wänden, viele verschlossene Türen in Weiß, Grau und Creme sowie aus Holz, Warnschilder, Röhren in Weiß und Orange.
2. **Brownout schwarz** (Bilder 018–028): Röhren sichtbar, aber kein Licht auf Wänden und Boden.
   - Das Wiki beschreibt manche Bereiche als dunkel. Ein Brownout ist aber gedimmt, nicht aus.
   - Violette Notlichter (Wiki: seit dem Ausfall vereinzelt) fehlen dort.
3. **Werkzeug:** In 1–2 m engen Tunneln zeigen Bilder an Biegungen die Wand. Das liegt am Querschnitt, nicht an einem Fehler.

Änderungen:
- Brownout-Röhren 0,35 → 0,45, tote Gruppen 55 % → 45 %, Flackern bleibt.
- Violette Notlampen in Brownout-Kreuzungen mit 50 % statt 12 %.

Nachtrag Runde 1 (Prüfung):
- Nach der Änderung meldete die Raummetrik 3 und dann 58 verstellte Tunnel. Ursache war ein veralteter Ausstattungs-Abzug (`build/qa/props_level2.json` von 16:22, die Karte von 17:45).
- `room_metrics.py` erzeugt den Abzug jetzt selbst neu, wenn die Karte neuer ist. Mit frischem Abzug: 0 von 181 verstellt.
- Zusätzlich setzt `furnish_tunnel` in Tunneln unter 3 m keine Tür mehr gegenüber einer Maschine.

## Runde 2 (mit der neuen Ausstattung: 30 Klassen, Varianten der Maschinen)

Laufweg 208 m durch 28 Räume, 52 Bilder (`wegbilder_r2_*`). Befunde:
1. **Maschinen verschiedener Epochen gemischt**, wie im Wiki: genietete Schalttafel, beiges Steuerpult, grauer Schrank, Verteiler aus Gusseisen, Bakelit und Kunststoff; Rohrbündel über 2,13 m (001–015, 029–035).
2. **Dunkle Strecken** (018–027): ausgefallene Leuchtengruppen und ein Leerraum, wie im Wiki (Reihenschaltung, lichtlose Leerräume).
3. **Brownout:** Noch zu dunkel geplant; die Zone selbst liegt nicht auf diesem Weg.

Änderung: Brownout-Grundlicht 0,02 → 0,045, Nebel 0,06 → 0,035, Röhren 0,45 → 0,55, tote Gruppen 45 % → 35 %.

## Runde 3 (gleicher Weg, `wegbilder_r3_*`)

Nachweis: Die dunklen Strecken liegen in den Räumen 10, 129, 53, 139 und 105 (Beton-Tunnel ohne Leuchten, tote Gruppen) und im Leerraum. Die Zonenprüfung zeigt: kein Brownout-Raum auf dem Weg. Keine weitere Änderung an der Struktur. Die Runden 4 und 5 folgen mit den Level-2-Wesen.
