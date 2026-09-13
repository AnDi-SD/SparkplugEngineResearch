param(
  [Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,
  [ValidateRange(1,4)][int]$MaximumBones=2,
  [switch]$BuildOnly,[switch]$RunBuilt,
  [string]$ClientDll,[string]$ServerExecutable,
  [ValidatePattern('^[A-Fa-f0-9]{64}$')][string]$ClientSha256,
  [ValidatePattern('^[A-Fa-f0-9]{64}$')][string]$ServerSha256,
  [string]$RendererDirectory,
  [ValidatePattern('^[A-Fa-f0-9]{64}$')][string]$RendererSha256
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$run=Join-Path $root "local-data/rtx-remix/skinning-fixtures/$Name"
if($BuildOnly -and $RunBuilt){throw 'BuildOnly and RunBuilt are exclusive'}
if(Test-Path -LiteralPath (Join-Path $run 'launch.json')){throw 'This fixture has already been launched; preserve its evidence'}
if(-not $RunBuilt){
  if(Test-Path -LiteralPath $run){throw 'Use a fresh evidence name'}
  New-Item -ItemType Directory -Path $run | Out-Null
  $files=@('test_remix_skinning.cpp','test_remix_material.cpp','winx_surface_material.h','winx_material_channels.h','winx_material_channel_assets.h','analyze_remix_skinning.py')
  foreach($file in $files){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination $run}
  Copy-Item -LiteralPath $PSCommandPath -Destination $run
  $include=Join-Path $run 'include/remix'
  New-Item -ItemType Directory -Path $include -Force | Out-Null
  Get-ChildItem -LiteralPath (Join-Path $root 'local-data/rtx-remix/upstream/dxvk-remix/public/include/remix') -File | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination $include}
  $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
  $installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
  if(-not $installation){throw 'MSVC x86 tools not found'}
  $vcvars=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
  $commands=@"
@echo off
call "$vcvars" x86
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MT /O2 /W4 /I"$run/include" "$run/test_remix_skinning.cpp" /Fe:test_remix_skinning.exe /link user32.lib gdi32.lib
exit /b %errorlevel%
"@
  $command=Join-Path $run 'build.cmd';[IO.File]::WriteAllText($command,$commands,[Text.Encoding]::ASCII)
  Push-Location $run
  try{& $env:ComSpec /d /c $command *> (Join-Path $run 'build.log');if($LASTEXITCODE -ne 0){throw 'Skin fixture build failed; see build.log'}}finally{Pop-Location}
  $hashes=@(Get-ChildItem -LiteralPath $run -File -Recurse | Where-Object { $_.Extension -in '.h','.cpp','.exe','.ps1','.py' } | ForEach-Object {
    @{path=$_.FullName.Substring($run.Length+1);sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
  })
  @{schema=1;status='BUILT';hashes=$hashes;nativeGameCodeExecuted=$false} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $run 'build.json') -Encoding UTF8
}
if($BuildOnly){Write-Output "Built isolated skin fixture: $run";return}
$build=Get-Content -LiteralPath (Join-Path $run 'build.json') -Raw | ConvertFrom-Json
foreach($file in $build.hashes){if((Get-FileHash -LiteralPath (Join-Path $run $file.path)).Hash -ne $file.sha256){throw "Frozen fixture changed: $($file.path)"}}
if(-not $ClientDll -or -not $ServerExecutable -or -not $ClientSha256 -or -not $ServerSha256){throw 'Explicit paired skin-wire binaries and reviewed SHA256 values are required'}
if((Get-FileHash -LiteralPath $ClientDll).Hash -ne $ClientSha256 -or (Get-FileHash -LiteralPath $ServerExecutable).Hash -ne $ServerSha256){throw 'Paired bridge hash mismatch'}
if(Get-Process WinxClub,WinxClubDebug,NvRemixBridge,test_remix_material,test_remix_skinning -ErrorAction SilentlyContinue){throw 'Another game or fixture is running'}
$game=Join-Path $root 'local-data/Winx Club';$server=Join-Path $run '.trex'
$runtime=Join-Path $game '.trex'
if($RendererDirectory){
  $runtime=[IO.Path]::GetFullPath($RendererDirectory)
  if(-not $RendererSha256 -or (Get-FileHash -LiteralPath (Join-Path $runtime 'd3d9.dll')).Hash -ne $RendererSha256){throw 'Explicit reviewed renderer hash required'}
  if(-not (Test-Path -LiteralPath (Join-Path $runtime 'usd') -PathType Container)){throw 'Renderer runtime must include its USD dependencies'}
}elseif($RendererSha256){throw 'RendererSha256 requires RendererDirectory'}
New-Item -ItemType Directory -Path $server | Out-Null
Get-ChildItem -LiteralPath $runtime -File | Where-Object {$_.Extension -eq '.dll' -and $_.Name -notlike '*.remix-*'} | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination $server}
Copy-Item -LiteralPath (Join-Path $runtime 'usd') -Destination $server -Recurse
Copy-Item -LiteralPath $ClientDll -Destination (Join-Path $run 'd3d9.dll')
Copy-Item -LiteralPath $ServerExecutable -Destination (Join-Path $server 'NvRemixBridge.exe')
if($RendererDirectory -and (Get-FileHash -LiteralPath (Join-Path $server 'd3d9.dll')).Hash -ne $RendererSha256){throw 'Renderer changed during bundle copy'}
$runtimeHashes=@(Get-ChildItem -LiteralPath $server -File -Recurse | ForEach-Object {
  @{path=$_.FullName.Substring($server.Length+1);sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
})
[IO.File]::WriteAllText((Join-Path $server 'bridge.conf'),"clientChannelMemSize = 192MB`r`nexposeRemixApi = True`r`n",[Text.Encoding]::ASCII)
$config=@'
rtx.enableRaytracing = True
rtx.useVertexCapture = True
rtx.vertexColorIsBakedLighting = False
rtx.debugView.composite.compositeViewIdx = 0
rtx.debugView.debugViewIdx = 23
rtx.localtonemap.exposure = 1
rtx.localtonemap.shadows = 1
rtx.upscalerType = 0
rtx.resolutionScale = 1
rtx.forceCameraJitter = False
rtx.enableRayReconstruction = False
'@
[IO.File]::WriteAllText((Join-Path $run 'rtx.conf'),$config,[Text.Encoding]::ASCII)
$priorConfig=$env:DXVK_RTX_CONFIG_FILE;$priorAudit=$env:REMIX_BRIDGE_INSTANCE_AUDIT
try{
  $env:DXVK_RTX_CONFIG_FILE=Join-Path $run 'rtx.conf';$env:REMIX_BRIDGE_INSTANCE_AUDIT=Join-Path $run 'server-instance-audit.jsonl'
  $process=Start-Process -FilePath (Join-Path $run 'test_remix_skinning.exe') -ArgumentList @('--max-bones',"$MaximumBones") -WorkingDirectory $run -WindowStyle Hidden -PassThru
  @{schema=1;pid=$process.Id;started=(Get-Date).ToString('o');maximumBonesPerVertex=$MaximumBones;clientSha256=$ClientSha256;serverSha256=$ServerSha256;
    rendererSha256=(Get-FileHash -LiteralPath (Join-Path $server 'd3d9.dll')).Hash;rendererDirectory=$runtime;runtimeHashes=$runtimeHashes;gameAssetsUsed=$false;nativeGameCodeExecuted=$false} |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $run 'launch.json') -Encoding UTF8
  Write-Output "Skin fixture launched PID $($process.Id): $run"
  # The shell can yield while this process wait runs. Outer process bound keeps
  # failures reviewable; no other process is ever killed by this wrapper.
  $timeout=-not $process.WaitForExit(150000)
  if($timeout){Stop-Process -InputObject $process -Force;$process.WaitForExit()}
  $exit=$process.ExitCode
  @{pid=$process.Id;exited=(Get-Date).ToString('o');exitCode=$exit;timeout=$timeout} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'exit.json') -Encoding UTF8
  if($timeout -or $exit -ne 0){throw 'Skin fixture failed; output is preserved'}
  $rows=@(Get-Content -LiteralPath (Join-Path $run 'fixture.jsonl') | ForEach-Object {$_ | ConvertFrom-Json})
  if($rows[-1].event -ne 'complete'){throw 'Missing complete fixture event'}
  $rows[-1] | ConvertTo-Json -Compress
}finally{$env:DXVK_RTX_CONFIG_FILE=$priorConfig;$env:REMIX_BRIDGE_INSTANCE_AUDIT=$priorAudit}
