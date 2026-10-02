# LIMINAL V5 - portable Build-Toolchain einrichten
#
# Laedt ausschliesslich offizielle Releases von GitHub und entpackt sie nach
# "<Projekt>\.toolchain\". Keine Installation, keine Adminrechte, keine
# Aenderung am System. Entfernen = Ordner .toolchain loeschen.
#
#   llvm-mingw  (Clang/LLD, C++20, Windows-Header inkl. Direct3D 11, WASAPI)
#   CMake       (Build-Beschreibung)
#   Ninja       (Build-Ausfuehrung)
#
# Aufruf:  powershell -ExecutionPolicy Bypass -File tools\setup_toolchain.ps1

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$Root = Split-Path -Parent $PSScriptRoot
$Dest = Join-Path $Root '.toolchain'
$Cache = Join-Path $Dest '_downloads'

$Packages = @(
    @{ Name = 'llvm-mingw'; Url = 'https://github.com/mstorsjo/llvm-mingw/releases/download/20260922/llvm-mingw-20260922-ucrt-x86_64.zip'; Dir = 'llvm-mingw' },
    @{ Name = 'cmake';      Url = 'https://github.com/Kitware/CMake/releases/download/v4.4.3/cmake-4.4.3-windows-x86_64.zip';               Dir = 'cmake' },
    @{ Name = 'ninja';      Url = 'https://github.com/ninja-build/ninja/releases/download/v1.13.2/ninja-win.zip';                           Dir = 'ninja' }
)

New-Item -ItemType Directory -Force -Path $Cache | Out-Null

foreach ($p in $Packages) {
    $target = Join-Path $Dest $p.Dir
    if (Test-Path $target) {
        Write-Host "  $($p.Name): bereits vorhanden"
        continue
    }
    $zip = Join-Path $Cache ([IO.Path]::GetFileName($p.Url))
    if (-not (Test-Path $zip)) {
        Write-Host "  $($p.Name): lade $($p.Url)"
        Invoke-WebRequest -Uri $p.Url -OutFile "$zip.part" -UseBasicParsing
        Move-Item "$zip.part" $zip
    }
    Write-Host "  $($p.Name): entpacke"
    $tmp = Join-Path $Dest "_extract_$($p.Dir)"
    if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp }
    Expand-Archive -Path $zip -DestinationPath $tmp
    # Archive enthalten meist einen einzigen Wurzelordner -> diesen hochziehen
    $items = @(Get-ChildItem $tmp)
    if ($items.Count -eq 1 -and $items[0].PSIsContainer) {
        Move-Item $items[0].FullName $target
        Remove-Item -Recurse -Force $tmp
    } else {
        Move-Item $tmp $target
    }
}

Remove-Item -Recurse -Force $Cache
Write-Host "Toolchain bereit in $Dest"
