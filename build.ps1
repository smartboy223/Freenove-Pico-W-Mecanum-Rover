$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
& python prepare_dashboard.py
if ($LASTEXITCODE -ne 0) { throw 'Dashboard generation failed' }
& python prepare_wifi.py
if ($LASTEXITCODE -ne 0) { throw 'Wi-Fi configuration is incomplete or invalid' }
& arduino-cli compile --fqbn rp2040:rp2040:rpipicow:flash=2097152_1048576 --libraries project-libraries --output-dir build firmware\CarReady
if ($LASTEXITCODE -ne 0) { throw 'Firmware build failed' }
