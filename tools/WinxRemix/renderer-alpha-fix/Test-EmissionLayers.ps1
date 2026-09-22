param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,
      [Parameter(Mandatory=$true)][string]$RuntimeDirectory,
      [Parameter(Mandatory=$true)][string]$Renderer,
      [Parameter(Mandatory=$true)][ValidatePattern('^[A-Fa-f0-9]{64}$')][string]$RendererSha256,
      [string]$ClientDll,[string]$ServerExecutable,
      [ValidatePattern('^[A-Fa-f0-9]{64}$')][string]$ClientSha256,
      [ValidatePattern('^[A-Fa-f0-9]{64}$')][string]$ServerSha256)
# Own Remix-only fixture; the native helper retains its system-mode flag.
$System=$false
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$overrideBridge=$ClientDll -or $ServerExecutable -or $ClientSha256 -or $ServerSha256
if($overrideBridge){
  if(-not $ClientDll -or -not $ServerExecutable -or -not $ClientSha256 -or -not $ServerSha256){throw 'Explicit matching bridge binaries and both SHA256 values required'}
  if((Get-FileHash -LiteralPath $ClientDll).Hash -ne $ClientSha256 -or (Get-FileHash -LiteralPath $ServerExecutable).Hash -ne $ServerSha256){throw 'Bridge override hash mismatch'}
}
if(Get-Process WinxClub,WinxClubDebug,NvRemixBridge,floor_blend,test_emission_layers,test_remix_material -ErrorAction SilentlyContinue){throw 'Another graphics process belongs to its owner'}
$run=Join-Path $root "local-data/rtx-remix/material-fixtures/$Name"
if(Test-Path -LiteralPath $run){throw 'Fresh evidence directory required'}
$memoryScript=Join-Path $root 'tools/WinxRemix/Get-SystemMemory.ps1';$initial=& $memoryScript
if($initial.availablePhysicalBytes -lt 512MB -or $initial.availableCommitBytes -lt 5GB){throw 'Insufficient starting reserve'}
New-Item -ItemType Directory -Path $run | Out-Null
Copy-Item -LiteralPath $PSCommandPath -Destination $run
Copy-Item -LiteralPath (Join-Path $root 'tools/WinxRemix/ProcessLifetime.ps1') -Destination $run
. (Join-Path $run 'ProcessLifetime.ps1')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'test_emission_layers.cpp') -Destination $run
$helpers=[IO.File]::ReadAllText((Join-Path $root 'tools/WinxRemix/test_remix_material.cpp'))
$main=$helpers.IndexOf('int main(');if($main -lt 0){throw 'Fixture helper boundary missing'}
[IO.File]::WriteAllText((Join-Path $run 'fixture_helpers.h'),$helpers.Substring(0,$main),[Text.UTF8Encoding]::new($false))
foreach($file in @('winx_surface_instance.h','winx_surface_material.h','winx_material_channels.h','winx_material_channel_assets.h')){
  Copy-Item -LiteralPath (Join-Path $root "tools/WinxRemix/$file") -Destination $run
}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $installation){throw 'MSVC tools missing'}
$environmentScript=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$include=Join-Path $root 'local-data/rtx-remix/upstream/dxvk-remix/public/include'
$commands=@"
@echo off
call "$environmentScript" x86
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MT /O2 /W4 /I"$include" test_emission_layers.cpp /Fe:test_emission_layers.exe /link user32.lib gdi32.lib
exit /b %errorlevel%
"@
$commandFile=Join-Path $run 'build.cmd';[IO.File]::WriteAllText($commandFile,$commands,[Text.Encoding]::ASCII)
Push-Location $run
try{& $env:ComSpec /d /c $commandFile *> (Join-Path $run 'build.log');if($LASTEXITCODE -ne 0){throw 'Build failed; preserved build.log'}}finally{Pop-Location}
$runtime=[IO.Path]::GetFullPath((Join-Path $root $RuntimeDirectory))
$pins=@{'d3d9.dll'='4BB7BB4F83E91EC1104C392E760FD6CEB2201D08F164957F89682EC128BE783E';'.trex/NvRemixBridge.exe'='A99FEF75C2465A51E684FE613E579B8CC20ECDBF4D0DE3F30D024B9DC5618E4F';'.trex/d3d9.dll'='8DF7B48BF70E2F8A918B8A5E6D285438F437955E2E10245EBF4C5E698C41C448'}
if(-not $System){
  foreach($key in $pins.Keys){if((Get-FileHash -LiteralPath (Join-Path $runtime $key)).Hash -ne $pins[$key]){throw 'Frozen runtime changed'}}
  Copy-Item -LiteralPath (Join-Path $runtime 'd3d9.dll') -Destination $run
  $server=Join-Path $run '.trex';New-Item -ItemType Directory -Path $server | Out-Null
  Get-ChildItem -LiteralPath (Join-Path $runtime '.trex') -File | Where-Object {$_.Extension -in '.dll','.exe'} | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination $server}
  Copy-Item -LiteralPath (Join-Path $runtime '.trex/usd') -Destination $server -Recurse
  [IO.File]::WriteAllText((Join-Path $server 'bridge.conf'),"clientChannelMemSize = 32MB`r`nexposeRemixApi = True`r`n",[Text.Encoding]::ASCII)
}
if($Renderer){
  if($System -or -not $RendererSha256){throw 'Explicit pinned renderer required'}
  $rendererPath=[IO.Path]::GetFullPath((Join-Path $root $Renderer))
  if((Get-FileHash -LiteralPath $rendererPath).Hash -ne $RendererSha256){throw 'Candidate renderer changed'}
  Copy-Item -LiteralPath $rendererPath -Destination (Join-Path $run '.trex/d3d9.dll')
}
if($overrideBridge){
  Copy-Item -LiteralPath $ClientDll -Destination (Join-Path $run 'd3d9.dll')
  Copy-Item -LiteralPath $ServerExecutable -Destination (Join-Path $run '.trex/NvRemixBridge.exe')
}
$config=@'
rtx.enableRaytracing = True
rtx.fallbackLightMode = 0
rtx.enableStochasticAlphaBlend = True
rtx.tonemappingMode = 0
rtx.tonemap.tonemappingEnabled = False
rtx.autoExposure.enabled = False
rtx.bloom.enable = False
rtx.enableFog = False
rtx.volumetrics.enable = False
rtx.tonemap.colorGradingEnabled = False
rtx.compositePrimaryDirectDiffuse = False
rtx.compositePrimaryDirectSpecular = False
rtx.compositePrimaryIndirectDiffuse = False
rtx.compositePrimaryIndirectSpecular = False
rtx.compositeSecondaryCombinedDiffuse = False
rtx.compositeSecondaryCombinedSpecular = False
rtx.useVertexCapture = True
rtx.vertexColorIsBakedLighting = False
rtx.debugView.composite.compositeViewIdx = 0
rtx.debugView.debugViewIdx = 0
rtx.localtonemap.exposure = 1
rtx.localtonemap.shadows = 1
rtx.initializer.asyncShaderPrewarming = False
rtx.graphicsPreset = 4
rtx.integrateIndirectMode = 0
rtx.upscalerType = 2
rtx.resolutionScale = 1
rtx.enableRayReconstruction = False
'@
[IO.File]::WriteAllText((Join-Path $run 'rtx.conf'),$config,[Text.Encoding]::ASCII)
[IO.File]::WriteAllText((Join-Path $run 'dxvk.conf'),"dxvk.numCompilerThreads = 1`r`n",[Text.Encoding]::ASCII)
@{renderer=$Renderer;rendererSha256=$RendererSha256;initial=$initial;maximumOwnedBytes=4GB;runtimePins=$pins;system=[bool]$System;minimumPhysicalBytes=256MB;minimumCommitBytes=512MB;seconds=110;bridgeOverride=[bool]$overrideBridge;actualClientSha256=(Get-FileHash -LiteralPath (Join-Path $run 'd3d9.dll')).Hash;actualServerSha256=(Get-FileHash -LiteralPath (Join-Path $run '.trex/NvRemixBridge.exe')).Hash} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $run 'profile.json') -Encoding UTF8
$previousRtx=$env:DXVK_RTX_CONFIG_FILE;$previousDxvk=$env:DXVK_CONFIG_FILE
$clientForced=$false;$exitPendingSince=$null;$process=$null;$ownedServers=@();$serverRecords=@{};$serverPath=[IO.Path]::GetFullPath((Join-Path $run '.trex/NvRemixBridge.exe'));$samples=@();$peakWorking=0L;$peakPrivate=0L;$limited=$false;$timedOut=$false
try{
  $env:DXVK_RTX_CONFIG_FILE=Join-Path $run 'rtx.conf';$env:DXVK_CONFIG_FILE=Join-Path $run 'dxvk.conf'
  $args=@{FilePath=(Join-Path $run 'test_emission_layers.exe');WorkingDirectory=$run;WindowStyle='Hidden';PassThru=$true}
  if($System){$args.ArgumentList='--system'}
  $process=Start-Process @args;$null=$process.Handle;$started=[Diagnostics.Stopwatch]::StartNew()
  @{pid=$process.Id;path=$process.Path;started=$process.StartTime.ToString('o');system=[bool]$System;sha256=(Get-FileHash -LiteralPath $args.FilePath).Hash} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'launch.json') -Encoding UTF8
  while(-not (Wait-OwnedProcess $process 100)){
    $process.Refresh()
    $reportedExit=Get-OwnedProcessExitCode $process -AllowRunning
    if($reportedExit -ne 259){
      if($null -eq $exitPendingSince){$exitPendingSince=$started.ElapsedMilliseconds}
      if($started.ElapsedMilliseconds-$exitPendingSince -gt 15000){$clientForced=$true;Stop-OwnedProcess $process;break}
    }
    $own=Get-Process -Id $PID;$working=$process.WorkingSet64+$own.WorkingSet64;$private=$process.PrivateMemorySize64+$own.PrivateMemorySize64
    $observedServers=@(Get-Process NvRemixBridge -ErrorAction SilentlyContinue | Where-Object {$_.Path -eq $serverPath -and $_.StartTime -ge $process.StartTime})
    foreach($child in $observedServers){if(-not $serverRecords.ContainsKey($child.Id)){$null=$child.Handle;$serverRecords[$child.Id]=$child}}
    $ownedServers=@($serverRecords.Values)
    foreach($child in $ownedServers){$child.Refresh();if(-not (Wait-OwnedProcess $child 0)){$working+=$child.WorkingSet64;$private+=$child.PrivateMemorySize64}}
    $peakWorking=[Math]::Max($peakWorking,$working);$peakPrivate=[Math]::Max($peakPrivate,$private);$available=& $memoryScript
    $samples+=@{milliseconds=$started.ElapsedMilliseconds;workingBytes=$working;privateBytes=$private;available=$available;serverPids=@($ownedServers | ForEach-Object {$_.Id})}
    if($working -gt 4GB -or $private -gt 4GB -or $available.availablePhysicalBytes -lt 256MB -or $available.availableCommitBytes -lt 512MB){$limited=$true;$clientForced=$true;Stop-OwnedProcess $process;break}
    if($started.Elapsed.TotalSeconds -gt 110){$timedOut=$true;$clientForced=$true;Stop-OwnedProcess $process;break}
  }
  $cleanup=@();foreach($child in $ownedServers){$normal=(Wait-OwnedProcess $child 10000);if(-not $normal -and $child.Path -eq $serverPath){Stop-OwnedProcess $child};$cleanup+=@{pid=$child.Id;exitedWithinWait=$normal;exitCode=(Get-OwnedProcessExitCode $child)}}
  $result=@{clientSignaled=(Wait-OwnedProcess $process 0);clientForced=$clientForced;exitCode=(Get-OwnedProcessExitCode $process);memoryLimited=$limited;timedOut=$timedOut;milliseconds=$started.ElapsedMilliseconds;peakPrivateBytes=$peakPrivate;peakWorkingBytes=$peakWorking;servers=$cleanup;remaining=@(if(-not (Wait-OwnedProcess $process 0)){$process.Id};Get-Process NvRemixBridge -ErrorAction SilentlyContinue | Where-Object {$_.Path -eq $serverPath} | ForEach-Object {$_.Id})}
  $result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $run 'execution.json') -Encoding UTF8
  $samples | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $run 'memory.json') -Encoding UTF8
  $result | ConvertTo-Json -Depth 4
  $serverFailed=(@($cleanup | Where-Object {$_.exitCode -ne 0 -or -not $_.exitedWithinWait}).Count -ne 0)
  if($clientForced -or $limited -or $timedOut -or (Get-OwnedProcessExitCode $process) -ne 0 -or $serverFailed -or $cleanup.Count -eq 0 -or $result.remaining.Count -ne 0){throw 'Owned fixture or bridge failed; preserve result and use a fresh state'}
}finally{
  $env:DXVK_RTX_CONFIG_FILE=$previousRtx;$env:DXVK_CONFIG_FILE=$previousDxvk
  $finalRows=@();$finalErrors=@()
  $retained=@(if($process){@{role='client';process=$process}};foreach($child in $ownedServers){@{role='server';process=$child}})
  foreach($entry in $retained){
    $owned=$entry.process;$row=@{role=$entry.role;pid=$owned.Id;forcedInFinally=$false;signaled=$false}
    try{
      if(-not (Wait-OwnedProcess $owned 0)){
        $row.forcedInFinally=$true
        if($entry.role -eq 'client'){$clientForced=$true}
        Stop-OwnedProcess $owned
      }
      $row.signaled=Wait-OwnedProcess $owned 0;$row.exitCode=Get-OwnedProcessExitCode $owned
    }catch{$row.error=$_.Exception.Message;$finalErrors+=$row.error}
    finally{$owned.Dispose();$finalRows+=$row}
  }
  @{clientForced=$clientForced;processes=$finalRows;errors=$finalErrors;remaining=@($finalRows | Where-Object {-not $_.signaled} | ForEach-Object {$_.pid})} |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $run 'final-cleanup.json') -Encoding UTF8
  if($finalErrors.Count){throw 'Owned process cleanup failed; see final-cleanup.json'}
}
