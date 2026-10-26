# LIMINAL V13 – Bugliste

Stand 26.10.2026. Reihenfolge der Bearbeitung: Abstürze und Blocker zuerst, dann Sicherheit, dann Darstellung. Jeder Eintrag: Fundort, Schwere, Ursache, Stand, Nachweis.

| Nr. | Bereich | Schwere | Befund | Stand |
|---|---|---|---|---|
| B-001 | Spielerfigur (Schulteransicht) | gering | Die Kapuzenjacke reißt am stark gebeugten Ellbogen auf (Skinning der Figur). Nur in der Schulteransicht (F5) sichtbar. | offen |
| B-003 | Starter (`LIMINAL.exe`) | Sicherheit, mittel | Die Signaturprüfung des heruntergeladenen Visual-C++-Pakets akzeptierte jede gültige Signatur, nicht nur die von Microsoft. Das Paket wird mit Administratorrechten ausgeführt. | **behoben** (E-023): Unterzeichner muss „Microsoft Corporation“ sein, Widerrufsprüfung der Kette. Nachweis `MULTIPLAYER_TESTS.md` Abschnitt 4. |
