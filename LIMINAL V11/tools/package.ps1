# LIMINAL V10 - Release-Paket
#
#   powershell -ExecutionPolicy Bypass -File tools\package.ps1            (baut Release und packt)
#   powershell -ExecutionPolicy Bypass -File tools\package.ps1 -NoBuild   (nur packen)
#
# Ergebnis:
#   Release\LIMINAL V10\    startfertiger Spielordner (LIMINAL.exe starten)
#   Release\LIMINAL_V10.zip derselbe Ordner als ZIP zum Weitergeben
#
# Das Paket enthaelt alles, was das Spiel braucht: das Spiel (statisch gebaut, keine
# eigenen DLLs noetig), Daten, Level, Prefabs, kompilierte Shader, Icon und
# Lizenzhinweise. Tests und Werkzeuge sind nicht enthalten.
#
# V9: LIMINAL.exe im Paket ist direkt das Spiel (LiminalGame.exe). Der fruehere Starter
# (launcher\, prueft das System und bietet die Microsoft-Laufzeit zum Herunterladen an)
# wird nicht mehr mitgeliefert: Smart App Control stufte ihn wegen dieser Download- und
# Admin-Funktion als verdaechtig ein und blockierte ihn. Das Spiel meldet Startfehler selbst.

param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Bin = Join-Path $Root 'build\Release\bin'
$OutRoot = Join-Path $Root 'Release'
$Out = Join-Path $OutRoot 'LIMINAL V10'
$Zip = Join-Path $OutRoot 'LIMINAL_V10.zip'

if (-not $NoBuild) {
    & powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'build.ps1')
    if ($LASTEXITCODE -ne 0) { throw 'Build fehlgeschlagen' }
}

# Pflichtdateien pruefen, bevor irgendetwas kopiert wird
$required = @('LiminalGame.exe', 'data\levels\level0.json', 'data\items.json',
              'shaders\lighting_CSMain.cso', 'assets\liminal.ico')
foreach ($r in $required) {
    if (-not (Test-Path (Join-Path $Bin $r))) { throw "Im Build fehlt: $r (erst tools\build.ps1 ausfuehren)" }
}

if (Test-Path $Out) { Remove-Item -Recurse -Force $Out }
New-Item -ItemType Directory -Force $Out | Out-Null

Copy-Item (Join-Path $Bin 'LiminalGame.exe') (Join-Path $Out 'LIMINAL.exe')
foreach ($d in 'data', 'shaders', 'assets') {
    Copy-Item -Recurse (Join-Path $Bin $d) (Join-Path $Out $d)
}

# Kurzanleitung fuer Spieler (die ausfuehrliche README bleibt beim Quellcode)
$readme = @'
LIMINAL V10
===========

Starten: LIMINAL.exe doppelklicken - das ist direkt das Spiel.

Fehlt etwas (z. B. Direct3D 11), meldet das Spiel es beim Start in einem Fenster.
Den frueheren Starter mit Systempruefung gibt es seit V9 nicht mehr im Paket:
Smart App Control (Windows 11) hatte ihn faelschlich als verdaechtig blockiert.

Voraussetzungen
  * Windows 10 oder 11 (64 Bit)
  * Grafikkarte mit Direct3D 11 und aktuellem Treiber
    (ohne passende Grafikkarte: langsamer Software-Modus)

Steuerung (vollstaendig im Spiel: H oder F1; Belegung in data\input.json)
  W A S D       gehen                Maus / Pfeile   umsehen
  Shift         sprinten             Leertaste       springen
  E             aufnehmen / lesen    F               Item benutzen
  T             Taschenlampe         ESC -> Notizbuch  gefundene Notizen
  E am Aufzug   rufen, drinnen fahren (mit allen Sicherungen: hinaus)
  E am Kasten   Sicherung herausnehmen
  Q             Item fallen lassen   Strg+Q          ganzen Stapel fallen lassen
  1-9, Mausrad  Schnellleiste        Tab             Inventar
  M             grosse Karte         F5              Ego / Schulterkamera
  + / -         Laufgeschwindigkeit  F3 oder I       Debug-Anzeige
  V             Darstellung (Modern / Terminal / ASCII / Monochrom)
  L             Leistungsmodus       B               Head-Bobbing
  P oder F12    Screenshot           Alt+Enter       Vollbild / Fenster
  ESC           Menue: Einstellungen, Hilfe, Speichern
  Gamepad (XInput) wird ebenfalls unterstuetzt (B springen, R3 Kamera).

Neu in V10: Entities aus den Backrooms-Wikis.
  * Welche Entity wo lebt, wie sie aussieht und wie sie sich verhaelt, folgt den Beschreibungen
    im Backrooms Wiki (Wikidot, Fandom). Quellen und Auslegung: docs\ENTITAETEN.md im Quellcode.
  * Innenraeume (Level 0): keine bestaetigten Entities - nur dunkle Gestalten am Ende der Gaenge.
    Sie veraendern sich nie, solange du hinsiehst. Sieh weg, und sie sind fort.
  * Tiefgeschoss (Level 1): Das Licht faellt manchmal aus. Im Dunkeln der Smiler - nur weisse
    Augen und ein langes Laecheln voller spitzer Zaehne. Halte den Blick, geh langsam zurueck,
    mach die Lampe aus, renn nicht. Der Hound - auf allen vieren, mager, langes schwarzes Haar -
    hoert dich rennen und sieht deine Lampe. Blickkontakt schuechtert ihn eine Weile ein.
  * Hotel (Level 5): Deathmoth-Nester an den Tueren dunkler Zimmer. Licht scheucht den Schwarm
    auf, mit Lampe stechen sie zu. Lampe aus und weg: Sie kehren zum Nest zurueck.
  * Fair: Niemand erscheint vor deinen Augen oder direkt neben dir; jeder Angriff kuendigt sich an.
  * Eigene Modelle (Distanzfelder), eigene Skelette und Animation (Gang auf allen vieren mit
    Hinken, Flug mit zwei Fluegelpaaren, Rauch, Haar mit Traegheit), Texturen bis 4096 px.
  * Dein eigener Koerper ist ein Mensch (Kapuzenjacke, Jeans) - oder wieder das Strichmaennchen.
  * Einstellungen -> Grafik & Anzeige: Entities (Alle / Ohne Angriffe / Aus), Figurendetails,
    Koerperdarstellung.
  * Die Entities beruhen auf Beschreibungen des Backrooms Wiki (backrooms-wiki.wikidot.com,
    CC BY-SA 3.0) und des Backrooms Wiki auf Fandom (backrooms.fandom.com, CC BY-SA). Eigene
    Umsetzung, kein offizielles Projekt dieser Wikis.

Seit V9: Es gibt einen Weg hinaus.
  * Tief in jeder Ebene haengen brummende Sicherungskaesten mit orange flackerndem Licht.
    Nimm aus jeder Ebene eine Sicherung (Bueros, Tiefgeschoss, Hotel) und setze alle drei
    in einem Aufzug ein: Er faehrt weiter als je zuvor - ins Licht. Danach ein Abspann mit
    deinem Laufbuch (und du kannst weiter erkunden).
  * Pausenmenue zeigt, welche Sicherungen fehlen; die Karte markiert gesehene Kaesten.
  * Neue Notizen mit Hinweisen (die wichtigsten findet man zuerst), leise Einleitung.

Seit V8: EINE zusammenhaengende Welt statt getrennter Level. Jeder Lauf beginnt in den
Bueros (Level 0); wer weitergeht, kommt ohne Ladebild ins Tiefgeschoss (Level 1), dahinter ins
Hotel (Level 2) - und noch weiter draussen beginnt es von vorn. Uebergaenge ueber Lastenflure,
Abgaenge, Personaltreppen und seltene Aufzuege, die sich den Rueckweg merken. Keine Level-Wahl
mehr im Menue.

Seit V7: Taschenlampe mit Batterien (echter Lichtkegel mit Schatten), Level 2 "Das Hotel",
Notizen anderer Wanderer mit Notizbuch, Halluzinationen bei schwindender Sanity (abschaltbar),
Laufbuch.

Seit V6: weitlaeufigere Welt (Passagen, Lichthoefe, lange Sichtachsen), raeumlicher
Klang (jede Leuchtstoffroehre summt einzeln, Raeume hallen, Waende daempfen, ferne
Geraeusche), glaubwuerdiger Getraenkeautomat mit Glasfront, neue Grafikgeneration
(GTAO, indirektes Licht, Lichtschaechte, Parallax-Oberflaechen, Gebrauchsspuren),
runde Minimap, sichtbarer eigener Koerper, Springen, Ausdauer, Items fallen lassen,
seltenere Energy Bars.

Grafik passt sich automatisch an: Beim ersten Start misst das Spiel im Hauptmenue
kurz die Leistung und waehlt die passende Stufe (Einstellungen -> Grafik & Anzeige ->
Grafikqualitaet -> Automatisch). Auf Laptops mit zwei Grafikchips nutzt es die
staerkere Grafikkarte. Bei Rucklern: Dynamische Aufloesung einschalten.

Alte Spielstaende (V3 bis V9) laden weiter ihre gewohnte Welt.

Spielstaende und Einstellungen
  %APPDATA%\LIMINAL\saves_v10\       (Spielstaende aus V9/V8/V7/V6/V5/V4 werden gefunden und als V10-Stand fortgefuehrt)
  %APPDATA%\LIMINAL\settings_v10.json (beim ersten Start aus V9 uebernommen)
  Screenshots: Bilder\LIMINAL
  Protokoll:   %LOCALAPPDATA%\LIMINAL\logs\liminal_v10.log

Den Ordner bitte vollstaendig lassen. Fuer den Desktop eine Verknuepfung auf
LIMINAL.exe anlegen.

Hinweis: Das Programm ist nicht digital signiert. Windows SmartScreen oder
Smart App Control koennen den Start deshalb blockieren ("Weitere Informationen"
-> "Trotzdem ausfuehren" bzw. Datei in der Windows-Sicherheit zulassen).
'@
Set-Content -Path (Join-Path $Out 'LIESMICH.txt') -Value $readme -Encoding UTF8

# Lizenzhinweise der mitgelinkten Laufzeitbibliotheken (LLVM libc++/libunwind, mingw-w64)
$lic = Join-Path $Out 'licenses'
New-Item -ItemType Directory -Force $lic | Out-Null
$tcLicense = Join-Path $Root '.toolchain\llvm-mingw\LICENSE.TXT'
if (Test-Path $tcLicense) { Copy-Item $tcLicense (Join-Path $lic 'llvm-mingw_LICENSE.txt') }
$notice = @'
LIMINAL V10 wurde mit llvm-mingw (https://github.com/mstorsjo/llvm-mingw) gebaut.
Statisch eingebunden sind Teile der LLVM-Laufzeit (libc++, libc++abi, libunwind,
compiler-rt; Apache License 2.0 mit LLVM Exceptions) und der mingw-w64-Laufzeit
(https://www.mingw-w64.org, ZPL/MIT-artige Lizenzen). Die Lizenztexte stehen in
llvm-mingw_LICENSE.txt.
'@
Set-Content -Path (Join-Path $lic 'THIRD_PARTY_NOTICES.txt') -Value $notice -Encoding UTF8

# Nichts Unerwuenschtes mitliefern
Get-ChildItem $Out -Recurse -File -Include *.pdb, *.ilk, *.lib, *.a, *.obj, *.o, *_test.exe | Remove-Item -Force

if (Test-Path $Zip) { Remove-Item -Force $Zip }
Compress-Archive -Path $Out -DestinationPath $Zip -CompressionLevel Optimal

$files = Get-ChildItem $Out -Recurse -File
$size = ($files | Measure-Object Length -Sum).Sum
Write-Host ("Paket: {0}  ({1} Dateien, {2:N1} MB)" -f $Out, $files.Count, ($size / 1MB))
Write-Host ("ZIP:   {0}  ({1:N1} MB)" -f $Zip, ((Get-Item $Zip).Length / 1MB))
