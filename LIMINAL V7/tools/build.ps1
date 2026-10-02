# LIMINAL V7 - Build
#
#   powershell -ExecutionPolicy Bypass -File tools\build.ps1            (Release)
#   powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Debug
#   powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Clean
#
# Ergebnis: build\<Konfiguration>\bin\  (startbar, enthaelt data\, shaders\, assets\)

param(
    [switch]$Debug,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Tc = Join-Path $Root '.toolchain'

if (-not (Test-Path (Join-Path $Tc 'llvm-mingw\bin\clang++.exe'))) {
    Write-Host 'Toolchain fehlt - richte sie ein ...'
    & powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'setup_toolchain.ps1')
    if ($LASTEXITCODE -ne 0) { throw 'Toolchain-Setup fehlgeschlagen' }
}

$env:PATH = "$Tc\llvm-mingw\bin;$Tc\cmake\bin;$Tc\ninja;$env:SystemRoot\System32;$env:SystemRoot"
$Config = if ($Debug) { 'Debug' } else { 'Release' }
$BuildDir = Join-Path $Root "build\$Config"

if ($Clean -and (Test-Path $BuildDir)) { Remove-Item -Recurse -Force $BuildDir }

& cmake -S $Root -B $BuildDir -G Ninja `
    "-DCMAKE_BUILD_TYPE=$Config" `
    "-DCMAKE_TOOLCHAIN_FILE=$Root\cmake\toolchain-llvm-mingw.cmake" `
    "-DCMAKE_MAKE_PROGRAM=$Tc\ninja\ninja.exe"
if ($LASTEXITCODE -ne 0) { throw 'CMake-Konfiguration fehlgeschlagen' }

& cmake --build $BuildDir --parallel -- -k 0
if ($LASTEXITCODE -ne 0) { throw 'Build fehlgeschlagen' }

Write-Host "Fertig: $BuildDir\bin"
