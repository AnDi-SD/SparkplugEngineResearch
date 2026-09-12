param(
    [ValidateSet('x86','x64')][string]$Platform = 'x86',
    [ValidateRange(1,4)][int]$Workers = 1
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$source = Join-Path $root 'local-data/rtx-remix/direct-camera-bridge-work/bridge'
$build = Join-Path $root "local-data/rtx-remix/direct-camera-bridge-build/$Platform"
$buildTools = Join-Path $root 'local-data/rtx-remix/direct-camera-build-tools'
$baseRevision = & git -C (Split-Path $source -Parent) rev-parse HEAD
if ($baseRevision -ne 'b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4') { throw 'Unexpected bridge source revision' }
if (-not (Test-Path -LiteralPath (Join-Path $buildTools 'bin/ninja.exe'))) { throw 'Workspace Ninja/Meson packages are missing' }
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'MSVC tools not found' }
$vcvars = Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
New-Item -ItemType Directory -Path $build -Force | Out-Null
$target = if ($Platform -eq 'x86') { 'd3d9' } else { 'NvRemixBridge' }
$setup = if (Test-Path -LiteralPath (Join-Path $build 'build.ninja')) { '--reconfigure' } else { '' }
$commands = @"
@echo off
call "$vcvars" $Platform
if errorlevel 1 exit /b %errorlevel%
set "PYTHONPATH=$buildTools"
set "PATH=$buildTools\bin;%PATH%"
python -m mesonbuild.mesonmain setup $setup --backend ninja --buildtype release -Denable_tests=true "$build" "$source"
if errorlevel 1 exit /b %errorlevel%
python -m mesonbuild.mesonmain compile -C "$build" -j $Workers $target test_remix_api_camera_$Platform
exit /b %errorlevel%
"@
$commandPath = Join-Path $build 'build-camera-bridge.cmd'
[IO.File]::WriteAllText($commandPath, $commands, [Text.Encoding]::ASCII)
$logPath = Join-Path $build ('build-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '.log')
& $env:ComSpec /d /c $commandPath 2>&1 | Tee-Object -FilePath $logPath
if ($LASTEXITCODE -ne 0) { throw "Bridge build failed: $LASTEXITCODE; log $logPath" }
Write-Output "Build complete: $build"
