param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,[string]$SourceDirectory,[ValidateSet('Wait','RemoteHandle')][string]$Kind='Wait')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$work=Join-Path $root "local-data/rtx-remix/bridge-shutdown-tests/$Name"
if(Test-Path -LiteralPath $work){throw 'Fresh process fixture directory required'}
if(-not $SourceDirectory){$SourceDirectory=Join-Path $root 'local-data/rtx-remix/upstream/dxvk-remix'}
& python (Join-Path $PSScriptRoot 'Prepare-ChildShutdownTest.py') $Name --source $SourceDirectory --kind $Kind
if($LASTEXITCODE -ne 0){throw 'Process fixture preparation failed'}
if(Test-Path -LiteralPath (Join-Path $work 'launch.json')){throw 'Executed process fixture must remain immutable'}
if(Test-Path -LiteralPath (Join-Path $work 'build.log')){throw 'A new preparation is required after any build attempt'}
$source=Get-Content -Encoding UTF8 -LiteralPath (Join-Path $work 'source.json') -Raw | ConvertFrom-Json
foreach($entry in $source.files.PSObject.Properties){if((Get-FileHash -LiteralPath (Join-Path $work $entry.Name)).Hash -ne $entry.Value){throw 'Frozen source changed'}}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $installation){throw 'MSVC tools required'}
$vcvars=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$commands=@"
@echo off
call "$vcvars" x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MT /O2 /W4 test_shutdown.cpp /Fe:test_shutdown.exe
exit /b %errorlevel%
"@
$command=Join-Path $work 'build.cmd';[IO.File]::WriteAllText($command,$commands,[Text.Encoding]::ASCII)
Push-Location $work
try{& $env:ComSpec /d /c $command *> (Join-Path $work 'build.log');if($LASTEXITCODE -ne 0){throw 'Build failed; preserve this preparation'}}finally{Pop-Location}
$memoryScript=Join-Path $root 'tools/WinxRemix/Get-SystemMemory.ps1'
$initial=& $memoryScript
if($initial.availablePhysicalBytes -lt 512MB -or $initial.availableCommitBytes -lt 512MB){throw 'Insufficient reserve for process fixture'}
$exe=Join-Path $work 'test_shutdown.exe'
if(Get-Process test_shutdown -ErrorAction SilentlyContinue){throw 'Another process fixture belongs to its owner'}
$process=$null;$children=@{};$samples=@();$limited=$false;$timedOut=$false;$peakPrivate=0L;$peakWorking=0L;$failure=$null;$result=$null
try{
  $process=Start-Process -FilePath $exe -WorkingDirectory $work -WindowStyle Hidden -PassThru;$null=$process.Handle
  $started=$process.StartTime;$timer=[Diagnostics.Stopwatch]::StartNew()
  @{pid=$process.Id;path=$exe;started=$started.ToString('o');sha256=(Get-FileHash -LiteralPath $exe).Hash;initial=$initial;maximumOwnedBytes=384MB;minimumPhysicalBytes=256MB;minimumCommitBytes=512MB;seconds=60} |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $work 'launch.json') -Encoding UTF8
  while(-not $process.WaitForExit(50)){
    $process.Refresh();if($process.HasExited){break};$shell=Get-Process -Id $PID
    $private=$process.PrivateMemorySize64+$shell.PrivateMemorySize64;$working=$process.WorkingSet64+$shell.WorkingSet64
    $observed=@(Get-Process test_shutdown -ErrorAction SilentlyContinue | Where-Object {$_.Id -ne $process.Id -and $_.Path -eq $exe -and $_.StartTime -ge $started})
    foreach($child in $observed){if(-not $children.ContainsKey($child.Id)){$null=$child.Handle;$children[$child.Id]=$child}}
    foreach($child in $children.Values){$child.Refresh();if(-not $child.HasExited){$private+=$child.PrivateMemorySize64;$working+=$child.WorkingSet64}}
    $available=& $memoryScript;$peakPrivate=[Math]::Max($peakPrivate,$private);$peakWorking=[Math]::Max($peakWorking,$working)
    $samples+=@{milliseconds=$timer.ElapsedMilliseconds;privateBytes=$private;workingBytes=$working;available=$available}
    $limited=$private -gt 384MB -or $working -gt 384MB -or $available.availablePhysicalBytes -lt 256MB -or $available.availableCommitBytes -lt 512MB
    $timedOut=$timer.Elapsed.TotalSeconds -ge 60
    if($limited -or $timedOut){Stop-Process -InputObject $process -Force;$process.WaitForExit();break}
  }
}catch{$failure=$_.Exception.Message}finally{
  if($process -and -not $process.HasExited){Stop-Process -InputObject $process -Force;$process.WaitForExit()}
  $cleanup=@()
  foreach($child in $children.Values){$child.Refresh();$forced=$false
    if(-not $child.HasExited -and $child.Path -eq $exe){$forced=$true;Stop-Process -InputObject $child -Force;$child.WaitForExit()}
    $cleanup+=@{pid=$child.Id;exitCode=$child.ExitCode;forcedByWrapper=$forced};$child.Dispose()
  }
  if($process){
    $remaining=@(Get-Process test_shutdown -ErrorAction SilentlyContinue | Where-Object {$_.Path -eq $exe} | ForEach-Object {$_.Id})
    $result=@{pid=$process.Id;exitCode=$process.ExitCode;milliseconds=$timer.ElapsedMilliseconds;peakPrivateBytes=$peakPrivate;peakWorkingBytes=$peakWorking;memoryLimited=$limited;timedOut=$timedOut;failure=$failure;children=$cleanup;remaining=$remaining}
    $result | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $work 'execution.json') -Encoding UTF8
    $samples | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $work 'memory.json') -Encoding UTF8
    $process.Dispose()
  }
}
$result | ConvertTo-Json -Depth 4
if(-not $result -or $failure -or $result.exitCode -ne 0 -or $limited -or $timedOut -or $result.remaining.Count){throw 'Owned process fixture failed; preserve all results'}

& python (Join-Path $work 'Analyze-ChildShutdown.py') $work
if($LASTEXITCODE -ne 0){throw 'Process fixture qualification failed'}
