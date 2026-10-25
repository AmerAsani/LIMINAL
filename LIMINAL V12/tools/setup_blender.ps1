# V12: Blender 4.5.14 LTS (portabel) fuer die Modell-Pipeline nach .toolchain\blender holen und pruefen.
# Quelle: offizieller Download-Server der Blender Foundation. Die Pruefsumme stammt aus der .sha256-Datei
# derselben Veroeffentlichung und ist hier fest eingetragen (DECISIONS.md E-003).
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$tc = Join-Path $root '.toolchain'
$dl = Join-Path $tc 'blender_dl'
$dst = Join-Path $tc 'blender'
$name = 'blender-4.5.14-windows-x64.zip'
$url = "https://download.blender.org/release/Blender4.5/$name"
$sha = 'b9533d2397ac1984db4466fb23a7a4649391cca93f6e84209f9bcc60d071c8b9'
$exe = Join-Path $dst 'blender-4.5.14-windows-x64\blender.exe'
if (Test-Path $exe) { Write-Host "Blender vorhanden: $exe"; exit 0 }
New-Item -ItemType Directory -Force $dl, $dst | Out-Null
$zip = Join-Path $dl $name
if (-not (Test-Path $zip)) { Invoke-WebRequest -Uri $url -OutFile $zip }
$h = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLower()
if ($h -ne $sha) { throw "Pruefsumme falsch: $h" }
& "$env:SystemRoot\System32\tar.exe" -xf $zip -C $dst
& $exe -b --version | Select-Object -First 1
