param(
  [Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,
  [ValidateRange(1,4)][int]$MaximumBones=2,
  [ValidateSet(1,24)][int]$Subdivisions=1,
  [ValidateRange(256,16384)][int]$MaximumWorkingSetMiB=1024,
  [ValidateRange(128,8192)][int]$MinimumAvailableMemoryMiB=512,
  [switch]$HalfResolution,
  [switch]$SignedWeights,
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
  $files=@('test_remix_skinning.cpp','test_remix_material.cpp','winx_surface_material.h','winx_material_channels.h','winx_material_channel_assets.h','analyze_remix_skinning.py','Get-SystemMemory.ps1')
  foreach($file in $files){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination $run}
  foreach($relative in @('tools/WinxRemix/winx_skin_packet.h','tools/WinxRemix/winx_skin_packet_remix.h','tools/WinxRemix/winx_skin_packet_gpu.h',
      'Sparkplug/Analysis/PC/spFixedShaderSkinning.h','Sparkplug/Code/SparkplugPC/spPCVertexDeclarationElements.h')) {
    $target=Join-Path (Join-Path $run 'source') $relative
    New-Item -ItemType Directory -Path (Split-Path $target) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $root $relative) -Destination $target
  }
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
cl /nologo /std:c++17 /EHsc /MT /O2 /W4 /I"$run/include" /I"$run/source/tools/WinxRemix" "$run/test_remix_skinning.cpp" /Fe:test_remix_skinning.exe /link user32.lib gdi32.lib
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
if($HalfResolution -and -not (Get-Content -LiteralPath (Join-Path $run 'test_remix_skinning.cpp') -Raw).Contains('--half-resolution')){throw 'This frozen fixture predates HalfResolution; build a fresh run'}
if($SignedWeights -and -not (Get-Content -LiteralPath (Join-Path $run 'test_remix_skinning.cpp') -Raw).Contains('--signed-weights')){throw 'This frozen fixture predates SignedWeights; build a fresh run'}
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
[IO.File]::WriteAllText((Join-Path $server 'bridge.conf'),"clientChannelMemSize = 32MB`r`nexposeRemixApi = True`r`n",[Text.Encoding]::ASCII)
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
rtx.initializer.asyncShaderPrewarming = False
rtx.graphicsPreset = 4
rtx.integrateIndirectMode = 0
'@
if($HalfResolution){$config=$config.Replace('rtx.upscalerType = 0','rtx.upscalerType = 2').Replace('rtx.resolutionScale = 1','rtx.resolutionScale = 0.5')}
[IO.File]::WriteAllText((Join-Path $run 'rtx.conf'),$config,[Text.Encoding]::ASCII)
[IO.File]::WriteAllText((Join-Path $run 'dxvk.conf'),"dxvk.numCompilerThreads = 1`r`n",[Text.Encoding]::ASCII)
$priorConfig=$env:DXVK_RTX_CONFIG_FILE;$priorAudit=$env:REMIX_BRIDGE_INSTANCE_AUDIT;$priorDxvkConfig=$env:DXVK_CONFIG_FILE
$memoryScript=Join-Path $run 'Get-SystemMemory.ps1'
$beforeMemory=& $memoryScript
$beforeMemory | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'memory-before.json') -Encoding UTF8
if ([Math]::Min($beforeMemory.availablePhysicalBytes,$beforeMemory.availableCommitBytes) -lt ([long]$MinimumAvailableMemoryMiB*1MB)) {
  throw 'Insufficient available system memory for the declared reserve'
}
try{
  $env:DXVK_RTX_CONFIG_FILE=Join-Path $run 'rtx.conf';$env:REMIX_BRIDGE_INSTANCE_AUDIT=Join-Path $run 'server-instance-audit.jsonl'
  $env:DXVK_CONFIG_FILE=Join-Path $run 'dxvk.conf'
  $arguments=@('--max-bones',"$MaximumBones",'--subdivisions',"$Subdivisions")
  if($HalfResolution){$arguments+=@('--half-resolution','1')}
  if($SignedWeights){$arguments+=@('--signed-weights','1')}
  $process=Start-Process -FilePath (Join-Path $run 'test_remix_skinning.exe') -ArgumentList $arguments -WorkingDirectory $run -WindowStyle Hidden -PassThru
  $null=$process.Handle # Retain exit status even when the child exits between samples.
  @{schema=1;pid=$process.Id;started=(Get-Date).ToString('o');maximumBonesPerVertex=$MaximumBones;subdivisions=$Subdivisions;clientSha256=$ClientSha256;serverSha256=$ServerSha256;
    rendererSha256=(Get-FileHash -LiteralPath (Join-Path $server 'd3d9.dll')).Hash;rendererDirectory=$runtime;runtimeHashes=$runtimeHashes;
    maximumWorkingSetMiB=$MaximumWorkingSetMiB;minimumAvailableMemoryMiB=$MinimumAvailableMemoryMiB;compilerThreads=1;shaderPrewarming=$false;clientChannelMiB=32;graphicsPreset=4;indirectMode=0;
    halfResolution=[bool]$HalfResolution;upscalerType=$(if($HalfResolution){2}else{0});resolutionScale=$(if($HalfResolution){0.5}else{1});
    signedWeights=[bool]$SignedWeights;
    gameAssetsUsed=$false;nativeGameCodeExecuted=$false} |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $run 'launch.json') -Encoding UTF8
  Write-Output "Skin fixture launched PID $($process.Id): $run"
  # Sample our shell, client and server from this unique fixture directory.
  # A bounded failure gets a new run; no unrelated game/server is stopped.
  $timer=[Diagnostics.Stopwatch]::StartNew();$peak=0L;$peakPrivate=0L;$memoryExceeded=$false;$systemPressure=$false;$timeout=$false
  $memory=@();$ownedServers=@();$serverPath=Join-Path $server 'NvRemixBridge.exe'
  while(-not $process.WaitForExit(100)){
    $process.Refresh()
    if($process.HasExited){break}
    $ownedServers=@(Get-Process NvRemixBridge -ErrorAction SilentlyContinue | Where-Object {$_.Path -eq $serverPath -and $_.StartTime -ge $process.StartTime})
    $shellProcess=Get-Process -Id $PID
    $workingSet=[long]$process.WorkingSet64+$shellProcess.WorkingSet64
    $private=[long]$process.PrivateMemorySize64+$shellProcess.PrivateMemorySize64
    foreach($child in $ownedServers){$child.Refresh();if(-not $child.HasExited){$workingSet+=$child.WorkingSet64;$private+=$child.PrivateMemorySize64}}
    $peak=[Math]::Max($peak,$workingSet)
    $peakPrivate=[Math]::Max($peakPrivate,$private)
    $available=& $memoryScript
    $memory+=@{elapsedSeconds=[Math]::Round($timer.Elapsed.TotalSeconds,3);workingSetBytes=$workingSet;privateBytes=$private;
      availablePhysicalBytes=$available.availablePhysicalBytes;availableCommitBytes=$available.availableCommitBytes;serverPids=@($ownedServers | ForEach-Object {$_.Id})}
    $systemPressure=[Math]::Min($available.availablePhysicalBytes,$available.availableCommitBytes) -lt ([long]$MinimumAvailableMemoryMiB*1MB)
    $memoryExceeded=[Math]::Max($workingSet,$private) -gt ([long]$MaximumWorkingSetMiB*1MB)
    $timeout=$timer.Elapsed.TotalSeconds -ge 150
    if($memoryExceeded -or $systemPressure -or $timeout){
      if($memoryExceeded -or $systemPressure){
        Stop-Process -InputObject $process -Force;$process.WaitForExit()
        foreach($child in $ownedServers){if(-not $child.HasExited -and $child.Path -eq $serverPath){Stop-Process -InputObject $child -Force;$child.WaitForExit()}}
        break
      }
      [void]$process.CloseMainWindow()
      if(-not $process.WaitForExit(3000)){Stop-Process -InputObject $process -Force;$process.WaitForExit()}
      foreach($child in $ownedServers){
        if(-not $child.WaitForExit(5000) -and $child.Path -eq $serverPath){Stop-Process -InputObject $child -Force}
      }
      break
    }
  }
  @{peakWorkingSetBytes=$peak;peakPrivateBytes=$peakPrivate;includesShell=$true;sampleMilliseconds=100;maximumWorkingSetMiB=$MaximumWorkingSetMiB;minimumAvailableMemoryMiB=$MinimumAvailableMemoryMiB;systemPressure=$systemPressure;exceeded=$memoryExceeded;samples=$memory} |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $run 'memory.json') -Encoding UTF8
  $exit=$process.ExitCode
  @{pid=$process.Id;exited=(Get-Date).ToString('o');exitCode=$exit;timeout=$timeout;memoryExceeded=$memoryExceeded;systemPressure=$systemPressure} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'exit.json') -Encoding UTF8
  if($timeout -or $memoryExceeded -or $systemPressure -or $exit -ne 0){throw 'Skin fixture failed or exceeded its bound; output is preserved'}
  # The bridge is a separate process. Its inherited handles and runtime cleanup
  # can outlive the client; client exit alone does not close this operation.
  $cleanup=@();$cleanupFailed=$false
  $remainingServers=@(Get-Process NvRemixBridge -ErrorAction SilentlyContinue | Where-Object {$_.Path -eq $serverPath -and $_.StartTime -ge $process.StartTime})
  foreach($child in $remainingServers){
    $null=$child.Handle
    $clean=$child.WaitForExit(10000)
    if(-not $clean){$cleanupFailed=$true;if($child.Path -eq $serverPath){Stop-Process -InputObject $child -Force;$child.WaitForExit()}}
    $cleanup+=@{pid=$child.Id;normalExit=$clean;exitCode=$child.ExitCode}
  }
  @{clientExitCode=$exit;servers=$cleanup;cleanupFailed=$cleanupFailed;remaining=@(Get-Process NvRemixBridge -ErrorAction SilentlyContinue | Where-Object {$_.Path -eq $serverPath} | ForEach-Object {$_.Id})} |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $run 'cleanup.json') -Encoding UTF8
  if($cleanupFailed){throw 'Owned bridge did not finish after client exit; cleanup evidence preserved'}
  $rows=@(Get-Content -LiteralPath (Join-Path $run 'fixture.jsonl') | ForEach-Object {$_ | ConvertFrom-Json})
  if($rows[-1].event -ne 'complete'){throw 'Missing complete fixture event'}
  $rows[-1] | ConvertTo-Json -Compress
}finally{$env:DXVK_RTX_CONFIG_FILE=$priorConfig;$env:REMIX_BRIDGE_INSTANCE_AUDIT=$priorAudit;$env:DXVK_CONFIG_FILE=$priorDxvkConfig}
