param([ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$build=Join-Path $root 'local-data/rtx-remix/test-surface-resources'
if ($Name) {
  $build=Join-Path $root "local-data/rtx-remix/surface-resource-tests/$Name"
  if (Test-Path -LiteralPath $build) { throw 'Use a fresh evidence directory' }
}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'MSVC x86 tools not found' }
$environmentScript=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$source=Join-Path $PSScriptRoot 'test_surface_resources.cpp'
$include=Join-Path $root 'local-data/rtx-remix/upstream/dxvk-remix/public/include'
New-Item -ItemType Directory -Path $build -Force | Out-Null
if ($Name) {
  $snapshot=Join-Path $build 'source/research/rtx-remix'
  New-Item -ItemType Directory -Path $snapshot | Out-Null
  Get-ChildItem -LiteralPath $PSScriptRoot -File | Where-Object { $_.Extension -in '.h','.cpp' } | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $snapshot }
  Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'third-party') -Destination $snapshot -Recurse
  $abiSnapshot=Join-Path $build 'source/Sparkplug/Analysis/PC'
  New-Item -ItemType Directory -Path $abiSnapshot -Force | Out-Null
  Copy-Item -LiteralPath (Join-Path $root 'Sparkplug/Analysis/PC/SparkplugAbi.h') -Destination $abiSnapshot
  Copy-Item -LiteralPath (Join-Path $root 'Sparkplug/Analysis/PC/SparkBaseAbi.h') -Destination $abiSnapshot
  $layoutSnapshot=Join-Path $build 'source/Sparkplug/Code/SparkplugPC'
  New-Item -ItemType Directory -Path $layoutSnapshot -Force | Out-Null
  Copy-Item -LiteralPath (Join-Path $root 'Sparkplug/Code/SparkplugPC/spPCVertexDeclarationElements.h') -Destination $layoutSnapshot
  $source=Join-Path $snapshot 'test_surface_resources.cpp'
}
$commands=@"
@echo off
call "$environmentScript" x86
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MT /O2 /W4 /I"$include" "$source" /Fe:test_surface_resources.exe /link user32.lib
if errorlevel 1 exit /b %errorlevel%
test_surface_resources.exe > result.json
exit /b %errorlevel%
"@
$commandFile=Join-Path $build 'test.cmd'
[IO.File]::WriteAllText($commandFile,$commands,[Text.Encoding]::ASCII)
Push-Location $build
try {
  & $env:ComSpec /d /c $commandFile
  if ($LASTEXITCODE -ne 0) { throw 'Surface resource tests failed' }
  Get-Content -LiteralPath (Join-Path $build 'result.json')
} finally { Pop-Location }
