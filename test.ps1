$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
New-Item -ItemType Directory -Force -Path (Join-Path $PSScriptRoot 'build') | Out-Null
$nativeCpp = Get-Command g++ -ErrorAction SilentlyContinue
if ($nativeCpp) {
    & $nativeCpp.Source -std=c++17 -Wall -Wextra tests/pilot_state_test.cpp -o build/pilot-state-test.exe
    if ($LASTEXITCODE -ne 0) { throw 'State-machine test compilation failed' }
    & (Join-Path $PSScriptRoot 'build/pilot-state-test.exe')
} else {
    $vsLocator = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (!(Test-Path -LiteralPath $vsLocator)) { throw 'Install a native C++ compiler: g++ or Visual Studio C++ Build Tools.' }
    $vsRoot = & $vsLocator -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (!$vsRoot) { throw 'Visual Studio C++ Build Tools were not found.' }
    $vcSetup = Join-Path $vsRoot 'VC/Auxiliary/Build/vcvars64.bat'
    $batchFile = Join-Path $PSScriptRoot 'build/test-host.cmd'
    @"
@echo off
call "$vcSetup" >nul
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 tests\pilot_state_test.cpp /Febuild\pilot-state-test.exe /Fobuild\pilot-state-test.obj
if errorlevel 1 exit /b 1
build\pilot-state-test.exe
"@ | Set-Content -LiteralPath $batchFile -Encoding ascii
    & cmd /d /c "`"$batchFile`""
}
if ($LASTEXITCODE -ne 0) { throw 'State-machine tests failed' }
