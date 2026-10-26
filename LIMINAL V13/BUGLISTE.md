# LIMINAL V13 – Bugliste

Stand 26.10.2026. Reihenfolge der Bearbeitung: Abstürze und Blocker zuerst, dann Sicherheit, dann Darstellung. Jeder Eintrag: Fundort, Schwere, Ursache, Stand, Nachweis.

| Nr. | Bereich | Schwere | Befund | Stand |
|---|---|---|---|---|
| B-001 | Spielerfigur (Schulteransicht) | gering | Die Kapuzenjacke reißt am stark gebeugten Ellbogen auf (Skinning der Figur). Nur in der Schulteransicht (F5) sichtbar. | offen |
| B-002 | Speicher beim Start | gering | Beim Laden steigt der Arbeitsspeicher kurz auf rund 3,1 GB (danach 470–620 MB). Ursache: alle 91 Texturebenen werden mit Mip-Stufen erst unkomprimiert im Speicher aufgebaut (Farbe 4, Normalen 2, ORM 4 Byte je Pixel ≈ 1,3 GB), dann in BC1/BC5 umgewandelt und hochgeladen; dazu die Bilddateien und die Texturen der Wesen. | offen – Empfehlung: Ebenen einzeln erzeugen, komprimieren und freigeben; auf Rechnern mit 8 GB unkritisch. |
| B-003 | Starter (`LIMINAL.exe`) | Sicherheit, mittel | Die Signaturprüfung des heruntergeladenen Visual-C++-Pakets akzeptierte jede gültige Signatur, nicht nur die von Microsoft. Das Paket wird mit Administratorrechten ausgeführt. | **behoben** (E-023): Unterzeichner muss „Microsoft Corporation“ sein, Widerrufsprüfung der Kette. Nachweis `MULTIPLAYER_TESTS.md` Abschnitt 4. |
| B-004 | Protokoll | gering | Bei jedem ersten Start zwölf Warnungen „JSON-Datei nicht gefunden“ für Dateien, die fehlen dürfen (ältere Einstellungsversionen, die absichtlich entfernte `world.json` der alten Welt). Echte Warnungen gingen darin unter. | **behoben**: `loadJsonFile(path, optional)` schweigt bei fehlenden optionalen Dateien. |

## Prüfungen (Teil 5)

| Prüfung | Werkzeug | Ergebnis |
|---|---|---|
| Hauptmenü: Einstellungen (Maus, Grafik, Audio), Neues Spiel, zurück | Skript `smoke.ps1` (`expect`-Prüfungen der Oberfläche) | alle Seiten erreicht, keine Fehlermeldung |
| Je Ebene (10): Laden, Gehen, Rennen (Ausdauer), Lampe an/aus, Inventar, Karte, Pause, Notizbuch, Werte setzen, Speichern über das Pausemenü, Beenden | `smoke.ps1` | alle 10 Ebenen: alle Prüfungen bestanden, Ausgang 0, keine Warnung oder Fehlermeldung im Protokoll |
| Spielstand laden | `smoke.ps1` (`--save-file`, Werte direkt nach dem Laden) | alle 10: Gesundheit und Verstand wie gespeichert (37,9 → 38; 61). Der spätere Anstieg auf 48 ist die normale Erholung. |
| Übergänge zwischen allen Ebenen, Halt aller Objekte, Vorräte, Wesen | `gameplay_test` | siehe Abschlussbericht |
| Bildzeiten je Ebene (1600 × 900, Qualität 2) | Befehl `perf` | GPU 1,8–5,7 ms; das Bild ist auf 60 Hz begrenzt |
| Grafikspeicher, Arbeitsspeicher | Befehl `perf` (neu: DXGI 1.4, Prozessspeicher) | VRAM 935–1012 MB; RAM 470–620 MB im Spiel, Spitze beim Laden 3,1 GB (B-002) |
| Karten | `tools/levels/<ebene>.py` | alle 10 bestanden, Inhalt identisch |
| Auflösungen | `ui_shots.ps1`: Titel, Pause, HUD, Inventar, Karte bei 1280 × 720, 1920 × 1080, 2560 × 1440, 1024 × 768 (4:3), 3440 × 1440 (21:9) | alle 25 Bilder vollständig, nichts abgeschnitten oder überlappend (`docs/screens/ui/v13_aufloesungen.jpg`) |

