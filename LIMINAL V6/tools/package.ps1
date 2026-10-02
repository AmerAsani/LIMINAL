# LIMINAL V6 - Release-Paket
#
#   powershell -ExecutionPolicy Bypass -File tools\package.ps1            (baut Release und packt)
#   powershell -ExecutionPolicy Bypass -File tools\package.ps1 -NoBuild   (nur packen)
#
# Ergebnis:
#   Release\LIMINAL V6\     startfertiger Spielordner (LIMINAL.exe starten)
#   Release\LIMINAL_V6.zip  derselbe Ordner als ZIP zum Weitergeben
#
# Das Paket enthaelt alles, was das Spiel braucht: beide Programme (statisch gebaut,
# keine eigenen DLLs noetig), Daten, Level, Prefabs, kompilierte Shader, Icon und
# Lizenzhinweise. Tests und Werkzeuge sind nicht enthalten.

param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Bin = Join-Path $Root 'build\Release\bin'
$OutRoot = Join-Path $Root 'Release'
$Out = Join-Path $OutRoot 'LIMINAL V6'
$Zip = Join-Path $OutRoot 'LIMINAL_V6.zip'

if (-not $NoBuild) {
    & powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'build.ps1')
    if ($LASTEXITCODE -ne 0) { throw 'Build fehlgeschlagen' }
}

# Pflichtdateien pruefen, bevor irgendetwas kopiert wird
$required = @('LIMINAL.exe', 'LiminalGame.exe', 'data\levels\level0.json', 'data\items.json',
              'shaders\lighting_CSMain.cso', 'assets\liminal.ico')
foreach ($r in $required) {
    if (-not (Test-Path (Join-Path $Bin $r))) { throw "Im Build fehlt: $r (erst tools\build.ps1 ausfuehren)" }
}

if (Test-Path $Out) { Remove-Item -Recurse -Force $Out }
New-Item -ItemType Directory -Force $Out | Out-Null

Copy-Item (Join-Path $Bin 'LIMINAL.exe'), (Join-Path $Bin 'LiminalGame.exe') $Out
foreach ($d in 'data', 'shaders', 'assets') {
    Copy-Item -Recurse (Join-Path $Bin $d) (Join-Path $Out $d)
}

# Kurzanleitung fuer Spieler (die ausfuehrliche README bleibt beim Quellcode)
$readme = @'
LIMINAL V6
==========

Starten: LIMINAL.exe doppelklicken.

Der Starter prueft Windows, Grafik (Direct3D 11) und die Microsoft C-Laufzeit
und startet dann das Spiel. Fehlt etwas, erklaert er, was zu tun ist, und bietet
- wo moeglich - die offizielle Installation von Microsoft an (nur nach Rueckfrage).

Voraussetzungen
  * Windows 10 oder 11 (64 Bit)
  * Grafikkarte mit Direct3D 11 und aktuellem Treiber
    (ohne passende Grafikkarte: langsamer Software-Modus)

Steuerung (vollstaendig im Spiel: H oder F1; Belegung in data\input.json)
  W A S D       gehen                Maus / Pfeile   umsehen
  Shift         sprinten             Leertaste       springen
  E             Item aufnehmen       F               Item benutzen
  Q             Item fallen lassen   Strg+Q          ganzen Stapel fallen lassen
  1-9, Mausrad  Schnellleiste        Tab             Inventar
  M             grosse Karte         F5              Ego / Schulterkamera
  + / -         Laufgeschwindigkeit  F3 oder I       Debug-Anzeige
  V             Darstellung (Modern / Terminal / ASCII / Monochrom)
  L             Leistungsmodus       B               Head-Bobbing
  P oder F12    Screenshot           Alt+Enter       Vollbild / Fenster
  ESC           Menue: Einstellungen, Hilfe, Speichern
  Gamepad (XInput) wird ebenfalls unterstuetzt (B springen, R3 Kamera).

Neu in V6: weitlaeufigere Welt (Passagen, Lichthoefe, lange Sichtachsen), raeumlicher
Klang (jede Leuchtstoffroehre summt einzeln, Raeume hallen, Waende daempfen, ferne
Geraeusche), glaubwuerdiger Getraenkeautomat mit Glasfront, neue Grafikgeneration
(GTAO, indirektes Licht, Lichtschaechte, Parallax-Oberflaechen, Gebrauchsspuren),
runde Minimap, Strichmaennchen-Koerper, Springen, Ausdauer, Items fallen lassen,
seltenere Energy Bars.

Grafik passt sich automatisch an: Beim ersten Start misst das Spiel im Hauptmenue
kurz die Leistung und waehlt die passende Stufe (Einstellungen -> Grafik & Anzeige ->
Grafikqualitaet -> Automatisch). Auf Laptops mit zwei Grafikchips nutzt es die
staerkere Grafikkarte. Bei Rucklern: Dynamische Aufloesung einschalten.

Alte Spielstaende (V3/V4/V5) laden weiter ihre gewohnte Welt.

Spielstaende und Einstellungen
  %APPDATA%\LIMINAL\saves_v6\       (Spielstaende aus V5/V4 werden gefunden und als V6-Stand fortgefuehrt)
  %APPDATA%\LIMINAL\settings_v6.json (beim ersten Start aus V5 uebernommen)
  Screenshots: Bilder\LIMINAL
  Protokoll:   %LOCALAPPDATA%\LIMINAL\logs\liminal_v6.log

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
LIMINAL V6 wurde mit llvm-mingw (https://github.com/mstorsjo/llvm-mingw) gebaut.
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
