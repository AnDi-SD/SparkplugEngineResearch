param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$work=Join-Path $root "local-data/rtx-remix/bridge-shutdown-tests/$Name"
if(Test-Path -LiteralPath $work){throw 'Fresh process fixture directory required'}
if(Get-Process WinxClub,WinxClubDebug,NvRemixBridge,test_emission_layers,cl,link -ErrorAction SilentlyContinue){throw 'Wait for graphics/build work'}
$initial=& (Join-Path $root 'tools/WinxRemix/Get-SystemMemory.ps1')
if($initial.availablePhysicalBytes -lt 512MB -or $initial.availableCommitBytes -lt 1GB){throw 'Insufficient reserve for small native build'}
New-Item -ItemType Directory -Path $work | Out-Null
Copy-Item -LiteralPath $PSCommandPath -Destination $work
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'test_process_detach.cpp') -Destination (Join-Path $work 'test.cpp')
Copy-Item -LiteralPath (Join-Path $root 'tools/WinxRemix/ProcessLifetime.ps1') -Destination $work
. (Join-Path $work 'ProcessLifetime.ps1')
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $installation){throw 'MSVC required'}
$vcvars=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$commands=@"
@echo off
call "$vcvars" x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MT /O2 /W4 /DOWN_DLL /LD test.cpp /Fodetach_dll.obj /Fe:detach_gate.dll
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MT /O2 /W4 test.cpp /Fodetach_exe.obj /Fe:test_process_detach.exe
exit /b %errorlevel%
"@
$command=Join-Path $work 'build.cmd';[IO.File]::WriteAllText($command,$commands,[Text.Encoding]::ASCII)
Push-Location $work
try{& $env:ComSpec /d /c $command *> (Join-Path $work 'build.log');if($LASTEXITCODE -ne 0){throw 'Build failed; preserve state'}}finally{Pop-Location}
$hashes=@(Get-ChildItem -LiteralPath $work -File | Where-Object {$_.Extension -in @('.cpp','.ps1','.exe','.dll','.cmd')} | ForEach-Object {@{name=$_.Name;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
$hashes | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $work 'source.json') -Encoding UTF8
$rows=@();$failure=$null
try{
 foreach($mode in @('release','terminate')){
  $id=[Guid]::NewGuid().ToString('N');$enteredName='Local\WinxRemixEntered_'+$id;$releaseName='Local\WinxRemixRelease_'+$id
  $entered=[Threading.EventWaitHandle]::new($false,[Threading.EventResetMode]::ManualReset,$enteredName)
  $release=[Threading.EventWaitHandle]::new($false,[Threading.EventResetMode]::ManualReset,$releaseName)
  $p=$null;$row=@{mode=$mode;forced=$false};$watch=[Diagnostics.Stopwatch]::StartNew()
  try{
   $exe=Join-Path $work 'test_process_detach.exe'
   $p=Start-Process -FilePath $exe -WorkingDirectory $work -ArgumentList @($enteredName,$releaseName) -WindowStyle Hidden -PassThru
   $null=$p.Handle
   $row.pid=$p.Id;$row.path=$p.Path;$row.started=$p.StartTime.ToString('o')
   if(-not $entered.WaitOne(10000)){throw 'Own DLL did not enter its exit gate'}
   $row.pendingNativeExitCode=Get-OwnedProcessExitCode $p -AllowRunning
   $row.pendingNativeSignaled=Wait-OwnedProcess $p 0
   $row.pendingManagedHasExited=$p.HasExited
   $row.pendingWorkingBytes=$p.WorkingSet64;$row.pendingPrivateBytes=$p.PrivateMemorySize64
   if($row.pendingWorkingBytes -gt 128MB -or $row.pendingPrivateBytes -gt 128MB){throw 'Own small process exceeded its budget'}
   $rejected=$false
   try{$null=Get-OwnedProcessExitCode $p}catch{$rejected=$true}
   $row.strictExitReadRejected=$rejected
   if($row.pendingNativeExitCode -ne 7 -or $row.pendingNativeSignaled -or -not $rejected){throw 'Exit gate counterexample missing'}
   if($mode -eq 'release'){$null=$release.Set();if(-not (Wait-OwnedProcess $p 10000)){throw 'Released process did not finish'}}
   else{$row.forced=$true;Stop-OwnedProcess $p -ExitCode 7 -Milliseconds 10000}
   $row.finalSignaled=Wait-OwnedProcess $p 0;$row.finalExitCode=Get-OwnedProcessExitCode $p
   if(-not $row.finalSignaled -or $row.finalExitCode -ne 7){throw 'Final native process state mismatch'}
   $row.status='PASS'
  }catch{$row.failure=$_.Exception.Message;$row.status='FAIL';throw}
  finally{
   if($p){if(-not (Wait-OwnedProcess $p 0)){$row.cleanupForced=$true;Stop-OwnedProcess $p -ExitCode 7};$p.Dispose()}
   $entered.Dispose();$release.Dispose();$row.milliseconds=$watch.ElapsedMilliseconds;$rows+=$row
   $row | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $work ($mode+'.json')) -Encoding UTF8
  }
 }
  foreach($kind in @('baseline','candidate')){
    $raw=[IO.File]::ReadAllText((Join-Path $work 'ProcessLifetime.ps1'))
    if($kind -eq 'baseline'){
      # Reproduce only the old terminal-state decision. All other helper code
      # and every Win32 operation remain the same as the candidate.
      $current=@'
      if (!TerminateProcess(process, code)) {
        int error = Marshal.GetLastWin32Error();
        // The process can finish between the first wait and termination.
        // A real signal establishes completion even when termination lost that race.
        if (Wait(process, 0)) return;
        throw new Win32Exception(error);
      }
'@
      $normalized=$raw.Replace("`r`n","`n");$current=$current.Replace("`r`n","`n")
      if(([regex]::Matches($normalized,[regex]::Escape($current))).Count -ne 1){throw 'Reviewed termination boundary changed; update race oracle'}
      $raw=$normalized.Replace($current,'      if (!TerminateProcess(process, code)) throw new Win32Exception();')
    }
    $match=[regex]::Match($raw,"(?s)Add-Type @'\r?\n(.*?)\r?\n'@")
    if(-not $match.Success){throw 'Public helper C# boundary missing'}
    $cs=$match.Groups[1].Value.Replace('namespace WinxRemix {',('namespace WinxRemixRace_'+$kind+' {'))
    $insertion=@'
    [DllImport("kernel32.dll", SetLastError=true)]
    private static extern bool SetEvent(IntPtr handle);
    public static IntPtr OwnReleaseEvent;
'@
    $cs=$cs.Replace('public static class ProcessLifetime {',('public static class ProcessLifetime {'+"`n"+$insertion))
    $needle='      if (!TerminateProcess(process, code))'
    if(([regex]::Matches($cs,[regex]::Escape($needle))).Count -ne 1){throw 'Single native termination boundary required'}
    # Deterministic scheduler boundary: after real initial wait reports running,
    # allow the own DLL cleanup to finish before the real TerminateProcess call.
    # All public Stop logic/native calls remain unchanged; only timing is injected.
    $hook=@'
      if (!SetEvent(OwnReleaseEvent)) throw new Win32Exception();
      if (!Wait(process, 10000)) throw new TimeoutException("Own exit gate failed");
'@
    $cs=$cs.Replace($needle,($hook+"`n"+$needle))
    [IO.File]::WriteAllText((Join-Path $work ($kind+'.cs')),$cs,[Text.UTF8Encoding]::new($false))
    Add-Type -TypeDefinition $cs
    $type=('WinxRemixRace_'+$kind+'.ProcessLifetime') -as [type]
    $id=[Guid]::NewGuid().ToString('N');$enteredName='Local\WinxRemixRaceEntered_'+$id;$releaseName='Local\WinxRemixRaceRelease_'+$id
    $entered=[Threading.EventWaitHandle]::new($false,[Threading.EventResetMode]::ManualReset,$enteredName)
    $release=[Threading.EventWaitHandle]::new($false,[Threading.EventResetMode]::ManualReset,$releaseName)
    $p=$null;$row=@{mode=('exit-race-'+$kind);kind=$kind;threw=$false;cleanupForced=$false;sourceSha256=(Get-FileHash -LiteralPath (Join-Path $work ($kind+'.cs'))).Hash}
    try{
      $p=Start-Process -FilePath (Join-Path $work 'test_process_detach.exe') -WorkingDirectory $work -ArgumentList @($enteredName,$releaseName) -WindowStyle Hidden -PassThru
      $null=$p.Handle;$row.pid=$p.Id;$row.started=$p.StartTime.ToString('o')
      if(-not $entered.WaitOne(10000)){throw 'Own DLL did not enter cleanup'}
      $row.beforeSignaled=Wait-OwnedProcess $p 0
      if($row.beforeSignaled){throw 'Race precondition was not reached'}
      $type.GetField('OwnReleaseEvent').SetValue($null,$release.SafeWaitHandle.DangerousGetHandle())
      try{$type.GetMethod('Stop').Invoke($null,@($p.Handle,[uint32]1,[uint32]10000))}
      catch{
        $row.threw=$true;$errorObject=$_.Exception
        while($errorObject.InnerException){$errorObject=$errorObject.InnerException}
        $row.errorType=$errorObject.GetType().FullName;$row.errorMessage=$errorObject.Message
        if($errorObject -is [ComponentModel.Win32Exception]){$row.nativeError=$errorObject.NativeErrorCode}
      }
      $row.afterSignaled=Wait-OwnedProcess $p 0;$row.exitCode=Get-OwnedProcessExitCode $p
      if(-not $row.afterSignaled -or $row.exitCode -ne 7){throw 'Own process did not finish normally'}
      if($kind -eq 'baseline'){
        if(-not $row.threw -or $row.nativeError -ne 5){throw 'Baseline completion race was not reproduced'}
      }elseif($row.threw){throw 'Candidate still reports a false cleanup error'}
      $row.status='PASS'
    }finally{
      if($p){if(-not (Wait-OwnedProcess $p 0)){$row.cleanupForced=$true;Stop-OwnedProcess $p};$p.Dispose()}
      $type.GetField('OwnReleaseEvent').SetValue($null,[IntPtr]::Zero)
      $entered.Dispose();$release.Dispose();$rows+=$row
      $row | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $work ($kind+'.json')) -Encoding UTF8
    }
  }
}catch{$failure=$_.Exception.Message}
$remaining=@(Get-Process test_process_detach -ErrorAction SilentlyContinue | Where-Object {$_.Path -eq (Join-Path $work 'test_process_detach.exe')} | ForEach-Object {$_.Id})
$report=@{status=$(if($failure -or $remaining.Count -ne 0){'FAIL'}else{'PASS'});failure=$failure;cases=$rows;remaining=$remaining;initial=$initial;scope='Own Windows DLL exit gate: strict completion, normal release, forced cleanup, and real native exit race with one injected scheduling boundary in copied public C# Stop. No GPU/game/Remix DLL loaded.'}
$report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $work 'result.json') -Encoding UTF8
$report | ConvertTo-Json -Depth 5
if($report.status -ne 'PASS'){throw 'Process lifetime regression failed'}
