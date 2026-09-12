param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,[switch]$System)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$run=Join-Path $root "local-data/rtx-remix/material-fixtures/$Name"
if (Test-Path -LiteralPath $run) { throw 'Use a fresh name to preserve evidence' }
if (Get-Process WinxClub,WinxClubDebug,NvRemixBridge,test_remix_material -ErrorAction SilentlyContinue) { throw 'Another game or fixture is running' }
New-Item -ItemType Directory -Path $run | Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'MSVC x86 tools not found' }
$environmentScript=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$source=Join-Path $PSScriptRoot 'test_remix_material.cpp'
$include=Join-Path $root 'local-data/rtx-remix/upstream/dxvk-remix/public/include'
Copy-Item -LiteralPath $source -Destination $run
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'winx_surface_material.h') -Destination $run
Copy-Item -LiteralPath $PSCommandPath -Destination $run
$source=Join-Path $run 'test_remix_material.cpp'
$commands=@"
@echo off
call "$environmentScript" x86
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MT /O2 /W4 /I"$include" "$source" /Fe:test_remix_material.exe /link user32.lib gdi32.lib
exit /b %errorlevel%
"@
$commandFile=Join-Path $run 'build.cmd'
[IO.File]::WriteAllText($commandFile,$commands,[Text.Encoding]::ASCII)
Push-Location $run
try {
  & $env:ComSpec /d /c $commandFile
  if ($LASTEXITCODE -ne 0) { throw 'Fixture compilation failed' }
} finally { Pop-Location }
if (-not $System) {
  $game=Join-Path $root 'local-data/Winx Club'
  Copy-Item -LiteralPath (Join-Path $game 'd3d9.remix-original.dll') -Destination (Join-Path $run 'd3d9.dll')
  $server=Join-Path $run '.trex'
  New-Item -ItemType Directory -Path $server | Out-Null
  Get-ChildItem -LiteralPath (Join-Path $game '.trex') -File | Where-Object {
    $_.Extension -in '.dll','.exe' -and $_.Name -notlike '*.remix-*'
  } | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $server }
  Copy-Item -LiteralPath (Join-Path $game '.trex/usd') -Destination $server -Recurse
  [IO.File]::WriteAllText((Join-Path $server 'bridge.conf'),"clientChannelMemSize = 192MB`r`nexposeRemixApi = True`r`n",[Text.Encoding]::ASCII)
}
$config=@'
rtx.enableRaytracing = True
rtx.useVertexCapture = True
rtx.vertexColorIsBakedLighting = False
rtx.debugView.composite.compositeViewIdx = 0
rtx.debugView.debugViewIdx = 0
rtx.localtonemap.exposure = 1
rtx.localtonemap.shadows = 1
rtx.enableEmissiveBlendModeTranslation = True
rtx.enableEmissiveBlendEmissiveOverride = True
rtx.emissiveBlendOverrideEmissiveIntensity = 1
'@
[IO.File]::WriteAllText((Join-Path $run 'rtx.conf'),$config,[Text.Encoding]::ASCII)
$previousConfig=$env:DXVK_RTX_CONFIG_FILE
try {
  $env:DXVK_RTX_CONFIG_FILE=Join-Path $run 'rtx.conf'
  $arguments=@{FilePath=(Join-Path $run 'test_remix_material.exe');WorkingDirectory=$run;WindowStyle='Normal';PassThru=$true}
  if ($System) { $arguments.ArgumentList='--system' }
  $process=Start-Process @arguments
  @{pid=$process.Id;system=$System.IsPresent;started=(Get-Date).ToString('o');sourceSha256=(Get-FileHash -LiteralPath $source).Hash;executableSha256=(Get-FileHash -LiteralPath $arguments.FilePath).Hash} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'launch.json') -Encoding UTF8
  if (-not $process.WaitForExit(150000)) { $process.Kill();throw 'Owned fixture exceeded 150 seconds' }
  if ($process.ExitCode -ne 0) { throw "Fixture failed: $($process.ExitCode); see $run/fixture.jsonl" }
  Get-Content -LiteralPath (Join-Path $run 'fixture.jsonl')
} finally { $env:DXVK_RTX_CONFIG_FILE=$previousConfig }
