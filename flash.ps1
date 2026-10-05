$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
$firmwareFile = Join-Path $PSScriptRoot 'build\CarReady.ino.uf2'
if (!(Test-Path -LiteralPath $firmwareFile)) { throw 'Run build.ps1 first.' }
$bootVolumes = @(Get-Volume | Where-Object { $_.FileSystemLabel -eq 'RPI-RP2' -and $_.DriveLetter })
if ($bootVolumes.Count -ne 1) { throw 'Connect exactly one Pico while holding BOOTSEL, then run again.' }
$bootRoot = "$($bootVolumes[0].DriveLetter):\"
$infoFile = Join-Path $bootRoot 'INFO_UF2.TXT'
if (!(Test-Path -LiteralPath $infoFile) -or (Get-Content -LiteralPath $infoFile -Raw) -notmatch 'Board-ID: RPI-RP2') { throw 'Pico boot drive could not be verified.' }
Copy-Item -LiteralPath $firmwareFile -Destination $bootRoot
Write-Host 'UF2 transferred. Run python car_tool.py --verify to confirm the running firmware.'
