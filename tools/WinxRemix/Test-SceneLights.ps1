param()
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$dependencyInclude = & (Join-Path $PSScriptRoot 'Prepare-Dependencies.ps1') -Offline
$build=Join-Path $root 'local-data/rtx-remix/test-scene-lights'
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'MSVC x86 tools not found' }
$environmentScript=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$source=Join-Path $PSScriptRoot 'test_scene_lights.cpp'
$include=Join-Path $root 'local-data/rtx-remix/upstream/dxvk-remix/public/include'
New-Item -ItemType Directory -Path $build -Force | Out-Null
$commands=@"
@echo off
call "$environmentScript" x86
if errorlevel 1 exit /b %errorlevel%
cl /I"$dependencyInclude" /nologo /std:c++17 /EHsc /MT /O2 /W4 /I"$include" "$source" /Fe:test_scene_lights.exe /link user32.lib
if errorlevel 1 exit /b %errorlevel%
test_scene_lights.exe
exit /b %errorlevel%
"@
$commandFile=Join-Path $build 'test.cmd'
[IO.File]::WriteAllText($commandFile,$commands,[Text.Encoding]::ASCII)
Push-Location $build
try {
    & $env:ComSpec /d /c $commandFile
    if ($LASTEXITCODE -ne 0) { throw "Scene light contract tests failed: $LASTEXITCODE" }
} finally { Pop-Location }
