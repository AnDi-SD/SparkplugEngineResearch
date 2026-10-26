param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,
      [string]$SourceDirectory,
      [ValidateNotNullOrEmpty()][ValidateSet('failure-existing','failure-new','changed-target','changed-backup','postprepare-failure','success-handoff','after-game-error')][string[]]$Cases=@('failure-existing','failure-new','changed-target','changed-backup','postprepare-failure','success-handoff','after-game-error'))
$ErrorActionPreference='Stop'
if(-not $Cases.Count){throw 'At least one fixture case is required'}
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../..'))
$source=if($SourceDirectory){[IO.Path]::GetFullPath((Join-Path $root $SourceDirectory))}else{[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))}
$work=Join-Path $root ('local-data/rtx-remix/launch-config-tests/'+$Name)
if(Test-Path -LiteralPath $work){throw 'Fresh fixture directory required'}
New-Item -ItemType Directory -Path $work | Out-Null
$rows=@()
function Require([bool]$value,[string]$message){if(-not $value){throw $message}}
function Hash([string]$path){(Get-FileHash -LiteralPath $path).Hash}
function Invoke-Case([string]$mode){
  $caseRoot=Join-Path $work $mode
  $tool=Join-Path $caseRoot 'tools/WinxRemix'
  $game=Join-Path $caseRoot 'game'
  $context=[pscustomobject]@{run=(Join-Path $caseRoot ('local-data/rtx-remix/runs/'+$mode));target=(Join-Path $game '.trex/bridge.conf');mode=$mode;gameCalls=0;handoffs=0}
  New-Item -ItemType Directory -Path $tool,(Join-Path $tool 'scripts/runtime'),(Join-Path $game '.trex'),(Join-Path $game 'Media/Saved') | Out-Null
  Microsoft.PowerShell.Management\Copy-Item -LiteralPath (Join-Path $source 'Start-Probe.ps1') -Destination $tool
  Microsoft.PowerShell.Management\Copy-Item -LiteralPath (Join-Path $source 'scripts/runtime/Restore-LiveConfig.ps1') -Destination (Join-Path $tool 'scripts/runtime')
  foreach($file in @('WinxClub.exe','d3d9.dll','d3d9.remix-original.dll','rtx.conf','winx.ini','user.conf','.trex/NvRemixBridge.exe','.trex/d3d9.dll','Media/Saved/Saved00.wxs')){
    [IO.File]::WriteAllText((Join-Path $game $file),'fixture bytes',[Text.UTF8Encoding]::new($false))
  }
  $original=[byte[]](239,187,191,35,32,111,108,100,13,10)
  if($mode -ne 'failure-new'){[IO.File]::WriteAllBytes($context.target,$original)}
  $before=$(if(Test-Path -LiteralPath $context.target){Hash $context.target}else{$null})
  # These mocks exercise launcher failure ordering; no process/UI/device runs.
  function Get-Process { param($Name,[switch]$ErrorAction) return @() }
  function Wait-Process { throw 'Fixture must never wait for a real process' }
  $copyMock={
    param([string]$LiteralPath,[string]$Destination,[switch]$Recurse,[switch]$Force)
    if($context.mode -eq 'postprepare-failure' -and $Destination -eq (Join-Path $context.run 'bridge.conf')){throw 'planned-preenvironment-error'}
    Microsoft.PowerShell.Management\Copy-Item @PSBoundParameters
  }.GetNewClosure()
  Set-Item -Path Function:Copy-Item -Value $copyMock
  $startMock={
    param([string]$FilePath,[string]$WorkingDirectory,[string]$WindowStyle,[switch]$PassThru,[object[]]$ArgumentList)
    if($FilePath -eq 'powershell'){$context.handoffs++;return [pscustomobject]@{Id=424243}}
    $context.gameCalls++
    if($context.mode -eq 'changed-target'){[IO.File]::WriteAllText($context.target,'independent target')}
    if($context.mode -eq 'changed-backup'){[IO.File]::WriteAllText((Join-Path $context.run 'bridge.conf.before'),'independent backup')}
    if($context.mode -eq 'after-game-error'){
      Microsoft.PowerShell.Management\Remove-Item -LiteralPath (Join-Path $WorkingDirectory 'd3d9.remix-original.dll')
      return [pscustomobject]@{Id=424242}
    }
    if($context.mode -eq 'success-handoff'){return [pscustomobject]@{Id=424242}}
    throw 'planned-launch-error'
  }.GetNewClosure()
  Set-Item -Path Function:Start-Process -Value $startMock
  $errorRecord=$null;$envBefore=[Environment]::GetEnvironmentVariable('WINX_REMIX_BACKEND','Process')
  try{& (Join-Path $tool 'Start-Probe.ps1') -Name $mode -GameDirectory $game -Backend remix -LiveConfig -NoDrawTrace | Out-Null}catch{$errorRecord=$_}
  Require ([Environment]::GetEnvironmentVariable('WINX_REMIX_BACKEND','Process') -eq $envBefore) 'Environment was not restored'
  $state=Get-Content -Encoding UTF8 -LiteralPath (Join-Path $context.run 'bridge-config-state.json') -Raw | ConvertFrom-Json
  if($mode -in @('success-handoff','after-game-error')){
    Require ($context.handoffs -eq 1 -and $context.gameCalls -eq 1) 'Successful creation lost restore-worker handoff'
    Require ((Hash $context.target) -eq $state.temporarySha256) 'Synchronous rollback touched a created game'
    Require (-not(Test-Path -LiteralPath (Join-Path $context.run 'bridge-config-restored.json'))) 'Rollback ran after game creation'
    Require (($mode -eq 'success-handoff') -eq ($null -eq $errorRecord)) 'Post-creation error result changed'
  }else{
    $expected=if($mode -eq 'postprepare-failure'){'planned-preenvironment-error'}else{'planned-launch-error'}
    Require ($errorRecord -and $errorRecord.Exception.Message -match $expected) 'Original launch error was lost'
    Require ($context.handoffs -eq 0) 'Failure unexpectedly started a restore worker'
    if($mode -eq 'changed-target'){
      Require ([IO.File]::ReadAllText($context.target) -eq 'independent target') 'Changed target was overwritten'
      Require ($errorRecord.Exception.Data.Contains('WinxRemixConfigRollbackError')) 'Rollback refusal missing from original error'
    }elseif($mode -eq 'changed-backup'){
      Require ((Hash $context.target) -eq $state.temporarySha256) 'Changed backup was restored'
      Require ($errorRecord.Exception.Data.Contains('WinxRemixConfigRollbackError')) 'Backup refusal missing from original error'
    }elseif($mode -eq 'failure-new'){
      Require (-not(Test-Path -LiteralPath $context.target)) 'New temporary config survived launch failure'
    }else{Require ((Hash $context.target) -eq $before) 'Original config bytes were not restored'}
  }
  @{case=$mode;status='PASS';mockGameCalls=$context.gameCalls;mockRestoreHandoffs=$context.handoffs;error=$(if($errorRecord){$errorRecord.Exception.Message}else{$null});noRealProcess=$true}
}
foreach($mode in $Cases){
  try{$rows+=Invoke-Case $mode}catch{$rows+=@{case=$mode;status='FAIL';error=$_.Exception.ToString()};break}
}
$passed=@($rows|Where-Object {$_.status -eq 'PASS'}).Count
@{status=$(if($passed -eq $Cases.Count){'PASS'}else{'FAIL'});passed=$passed;total=$Cases.Count;rows=$rows;
  launcherSha256=(Hash (Join-Path $source 'Start-Probe.ps1'));restoreSha256=(Hash (Join-Path $source 'scripts/runtime/Restore-LiveConfig.ps1'));
  scope='Actual launcher and restore scripts copied into own temporary root; process commands mocked; no game/UI/GPU/installation mutation.'} |
  ConvertTo-Json -Depth 7 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $work 'result.json')
Get-Content -Encoding UTF8 -LiteralPath (Join-Path $work 'result.json')
if($passed -ne $Cases.Count){throw 'Launch config rollback fixture failed; evidence retained'}
