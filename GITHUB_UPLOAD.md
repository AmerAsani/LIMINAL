# LIMINAL – Sicherung auf GitHub (V11, V12, V13)

Stand: 27.10.2026 (Abschluss V13.2). Entscheidung E-025 in `LIMINAL V13/DECISIONS.md`.

## 1. Befund

| Prüfung | Ergebnis |
|---|---|
| Vorhandenes Remote | `origin` = `https://github.com/AmerAsani/LIMINAL` |
| Sichtbarkeit dieses Repositorys | **öffentlich** (GitHub-API ohne Anmeldung: `"private": false`, `"visibility": "public"`). Enthält V1–V10 auf `main`. |
| GitHub CLI (`gh`) | nicht installiert |
| Anmeldung | nicht geprüft und nicht benutzt (siehe 2) |
| Git LFS | installiert (3.7.1); alle LFS-Objekte (9 ZIP-Dateien) liegen lokal vor, `git lfs fsck` OK |

## 2. Was getan wurde – und was bewusst nicht

- **Nicht nach `origin` hochgeladen.** Der Auftrag verlangt ein **privates** Repository, solange die Lizenzfrage offen ist. `origin` ist öffentlich; ein Push von V11–V13 dorthin würde sie veröffentlichen. Das Repository auf privat umzustellen, würde die seit Langem öffentlichen V1–V10 verbergen und ist eine Kontoeinstellung – beides entscheidest du.
- **Kein neues Repository angelegt.** Ohne GitHub CLI geht das nur über die Web-Oberfläche oder die API mit einem Zugangstoken. Zugangsdaten werden nicht ausgelesen oder eingegeben (Auftrag). Außerdem ist der Name `liminal` belegt: GitHub unterscheidet bei Repository-Namen nicht nach Groß- und Kleinschreibung (`LIMINAL` existiert). Vorschlag: `liminal-privat`.
- **Lokal vollständig vorbereitet:**

| Branch | Tag | Commit | Inhalt |
|---|---|---|---|
| `liminal-v11` | `v11` | `42529a2` | V11-Ordner (287 Dateien, bisher nie committet; jede Datei bytegleich mit dem lokalen Ordner geprüft), README, `.gitignore` |
| `liminal-v12` | `v12` | `60557d7` | Stand `e8481b6` plus README und `.gitignore`; Ordner `LIMINAL V12` unverändert |
| `liminal-v13` | `v13` | Tag `v13` (Abschluss V13.2, `git rev-parse v13^{}`) | V13 mit Fortsetzung V13.2 (Berichte `REPORT_V13.md`, `REPORT_V13_2.md`) |

- **Bestehende Historie unverändert:** Die Commits wurden nur angehängt (kein Rebase, kein Force, kein Löschen).
- **V10:** wird nicht neu hochgeladen. Er steckt allerdings in der gemeinsamen Historie aller Branches (seit Commit `c701337` auf `main`, öffentlich auf `origin`). Ihn aus den Branches zu entfernen, hieße die Historie umzuschreiben – das ist ausgeschlossen.

## 3. Prüfungen vor dem Hochladen

| Prüfung | Ergebnis |
|---|---|
| Geheimnisse (GitHub-, AWS-, OpenAI-, Google-, Slack-Tokens, private Schlüssel, `.env`, `.pem`, `.key`, `id_rsa`, Zugangsdateien) in allen Branches und im V11-Ordner | **nichts gefunden** |
| Persönliche Pfade (`C:\Users\…`, Benutzername) und E-Mail-Adresse in Dateien | **nichts gefunden** (Commit-Metadaten enthalten wie bisher Name und E-Mail des Git-Kontos) |
| Dateigröße | größte Datei 6,8 MB; nichts über 50 MB (Warngrenze) oder 100 MB (Sperrgrenze) |
| Größe | Pack 267 MB (+ lose Objekte der Fortsetzung, etwa 40 MB vor dem Packen); Branch `liminal-v13` 629 MB unkomprimiert; `LIMINAL V13` 1 312 Dateien im Branch |
| Lizenzen | Texturen ambientCG CC0 (Lock mit Prüfsummen), eigene Modelle, eigene Klänge; Wiki-Inhalte nur in eigenen Worten (CC BY-SA 3.0). Abgleich: `LIMINAL V13/LICENSES.md`. Keine Dateien mit unklarer Lizenz gefunden. |
| `.gitignore` | ergänzt: Build-Ausgaben (`**/build/`, `**/Release/`), Toolchain (`**/.toolchain/`), Python-Zwischenstände, Protokolle, Absturzabbilder, Sicherungskopien, lokale Einfrier-Manifeste (`/Sicherungen/`) |
| Git LFS | Nur ZIP-Dateien laufen über LFS (`.gitattributes`). Die Spielassets (Modelle, Texturen) liegen seit V12 als normale Git-Objekte in der Historie. Sie nachträglich in LFS zu verschieben (`git lfs migrate`), würde die Historie umschreiben – ausgeschlossen. Bei der jetzigen Größe (größte Datei 6,8 MB) ist das unkritisch. |
| Dateizahl V12 | Freeze-Manifest: 25 550 Dateien. Davon im Tag `v12`: **838** – jede mit derselben SHA-256 wie im Manifest (0 Abweichungen). Nicht im Repository: `.toolchain\` 24 184 (Compiler, CMake, Ninja, Blender, heruntergeladene Textur-ZIPs – fremde Werkzeuge, lädt `tools\build.ps1` neu), `build\` 514 (Build-Ausgabe), 14 Python-Zwischenstände in `tools\`. |
| Spiel-Builds | nicht im Repository; als Release-Anhang vorgesehen (siehe 4) |

## 4. Befehle für dich

In PowerShell oder Git Bash, im Ordner `C:\Users\AmerA\Downloads\LIMINAL`:

```
# 1) Privates Repository anlegen – im Browser: https://github.com/new
#    Name: liminal-privat, Sichtbarkeit: Private, ohne README/Lizenz/.gitignore
#    oder mit GitHub CLI (vorher: winget install GitHub.cli; gh auth login):
gh repo create AmerAsani/liminal-privat --private --description "LIMINAL – private Sicherung V11 bis V13"

# 2) Als zweites Remote eintragen (origin bleibt unverändert)
git remote add privat https://github.com/AmerAsani/liminal-privat.git

# 3) LFS-Objekte zuerst, dann Branches und Tags (kein --force)
git lfs push --all privat
git push privat main liminal-v11 liminal-v12 liminal-v13
git push privat v11 v12 v13

# 4) Prüfen: Branches und Tags auf dem Remote, Commit-Kennungen wie lokal
git ls-remote --heads --tags privat
git rev-parse liminal-v11 liminal-v12 liminal-v13 v11^{} v12^{} v13^{}

# 5) Dateizahlen gegen das Remote (nach git fetch privat)
git fetch privat
git ls-tree -r --name-only privat/liminal-v12 "LIMINAL V12" | find /c /v ""
git ls-tree -r --name-only privat/liminal-v11 "LIMINAL V11" | find /c /v ""

# 6) Spielpaket als privater Release-Anhang (V13)
powershell -ExecutionPolicy Bypass -File "LIMINAL V13\tools\package.ps1"
gh release create v13 "<Pfad zum ZIP aus package.ps1>" --repo AmerAsani/liminal-privat --title "LIMINAL V13" --notes-file "LIMINAL V13\REPORT_V13_2.md"
```

Erwartet nach Schritt 4: drei `refs/tags/v1x` und die vier Branches mit denselben Kennungen wie lokal. Erwartet nach Schritt 5: 838 Dateien unter `LIMINAL V12`, 287 unter `LIMINAL V11`.

**Nicht ausführen:** `git push origin liminal-v1x` – `origin` ist öffentlich.

## 5. Stand des Hochladens

| Version | Lokal vorbereitet | Auf GitHub (privat) |
|---|---|---|
| V11 | ja (`v11`) | nein – Befehle oben |
| V12 | ja (`v12`) | nein – Befehle oben |
| V13 | ja (`v13`, Abschluss V13.2) | nein – Befehle oben |

## 6. Nachtrag Abschluss V13.2 (27.10.2026)

- **Erneut geprüft vor dem Tag `v13`:** keine Geheimnisse (Tokens, Schlüssel, `.env`), keine persönlichen Pfade, keine E-Mail-Adresse in den Dateien von `LIMINAL V13`; größte Datei im Repository weiterhin 6,8 MB; die neuen Modelle (`data/models/ent_*.glb`) zusammen 9,8 MB, eigene Werke (Blender-Skripte im Repository); einzige neue Fremdquelle ist ENet 1.3.18 (MIT, `LICENSES.md`).
- **`liminal-v13` regelmäßig mitpushen** (Auftrag) war nicht möglich: es gibt kein privates Remote, und `origin` ist öffentlich (E-025). Der lokale Stand ist vollständig committet.
- **Was du ausführen musst:** Abschnitt 4 unverändert (Schritte 1–6). Nach Schritt 4 erwartet: Branches `main`, `liminal-v11`, `liminal-v12`, `liminal-v13` und Tags `v11`, `v12`, `v13` mit denselben Kennungen wie lokal.
