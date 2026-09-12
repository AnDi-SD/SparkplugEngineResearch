param([ValidatePattern('^[A-Za-z0-9_-]+$')][string]$Name='runtime-v1')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$run=Join-Path $root "local-data/rtx-remix/direct-camera-live/$Name"
if(Test-Path -LiteralPath $run){throw 'Use a fresh runtime name'}
$game=Join-Path $root 'local-data/Winx Club'
$stockRenderer=Join-Path $game '.trex/d3d9.dll'
if((Get-FileHash -LiteralPath $stockRenderer).Hash -ne 'F7C310821AA98BCDFDEC120330B0A89457B7C5EBA58D21464AF32639611C809F'){throw 'Unexpected stock renderer'}
New-Item -ItemType Directory -Path $run | Out-Null
$trex=Join-Path $run '.trex'
New-Item -ItemType Directory -Path $trex | Out-Null
Get-ChildItem -LiteralPath (Join-Path $game '.trex') -File | Where-Object { $_.Extension -eq '.dll' -and $_.Name -notlike '*.remix-*' } | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination $trex}
Copy-Item -LiteralPath (Join-Path $game '.trex/usd') -Destination $trex -Recurse
Copy-Item -LiteralPath (Join-Path $root 'local-data/rtx-remix/direct-camera-bridge-build/x86/src/client/d3d9.dll') -Destination (Join-Path $run 'd3d9.dll')
Copy-Item -LiteralPath (Join-Path $root 'local-data/rtx-remix/direct-camera-bridge-build/x64/src/server/NvRemixBridge.exe') -Destination (Join-Path $trex 'NvRemixBridge.exe')
$bridge=@'
exposeRemixApi = True
clientChannelMemSize = 96MB
startupTimeout = 100
commandRetries = 100
ackTimeout = 10
commandTimeout = 100
infiniteRetries = False
disableTimeouts = False
logLevel = Debug
logAllCommands = True
logApiCalls = True
'@
[IO.File]::WriteAllText((Join-Path $trex 'bridge.conf'),$bridge,[Text.Encoding]::ASCII)
$rtx=@'
rtx.enableRaytracing = True
rtx.enableNearPlaneOverride = False
rtx.useVertexCapture = True
rtx.localtonemap.exposure = 1
'@
[IO.File]::WriteAllText((Join-Path $run 'rtx.conf'),$rtx,[Text.Encoding]::ASCII)
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$source=Join-Path $PSScriptRoot 'live_camera.cpp'
Copy-Item -LiteralPath $source -Destination $run
$include=Join-Path $root 'local-data/rtx-remix/upstream/dxvk-remix/public/include'
$commands=@"
@echo off
call "$vcvars" x86
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MT /O2 /W4 /I"$include" live_camera.cpp /Fe:live_camera.exe /link user32.lib
exit /b %errorlevel%
"@
[IO.File]::WriteAllText((Join-Path $run 'build.cmd'),$commands,[Text.Encoding]::ASCII)
Push-Location $run
try { & $env:ComSpec /d /c build.cmd; if($LASTEXITCODE -ne 0){throw 'Live helper build failed'} } finally {Pop-Location}
$files=@('live_camera.exe','live_camera.cpp','d3d9.dll','.trex/NvRemixBridge.exe','.trex/d3d9.dll','.trex/bridge.conf','rtx.conf')
$manifest=@($files | ForEach-Object { $path=Join-Path $run $_; [ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath $path).Hash;bytes=(Get-Item -LiteralPath $path).Length} })
[ordered]@{status='prepared only; not launched';runtime=$run;files=$manifest} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $run 'prepared.json') -Encoding UTF8
Write-Output "Prepared only: $run"
