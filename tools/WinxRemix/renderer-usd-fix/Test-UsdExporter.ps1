param(
  [Parameter(Mandatory=$true)][string]$Executable,
  [Parameter(Mandatory=$true)][string]$Python,
  [Parameter(Mandatory=$true)][string]$UsdDirectory,
  [Parameter(Mandatory=$true)][string]$Output,
  [ValidateSet('Skin','DefaultJoints','Dynamic','VariableSkin','VariableSkinDefaultJoints','SignedAdapter','SignedAdapterStatic')][string]$Scenario='Skin',
  [ValidateRange(10,600)][int]$MaximumSeconds=90,
  [ValidateRange(256,8192)][int]$MaximumWorkingSetMiB=1024
)
$ErrorActionPreference='Stop'
foreach($file in @($Executable,$Python)){
  if(-not (Test-Path -LiteralPath $file -PathType Leaf)){throw 'Existing fixture executable and compatible USD Python are required'}
}
$Executable=(Resolve-Path -LiteralPath $Executable).Path
$Python=(Resolve-Path -LiteralPath $Python).Path
$UsdDirectory=(Resolve-Path -LiteralPath $UsdDirectory).Path
$Output=[IO.Path]::GetFullPath($Output).TrimEnd([IO.Path]::DirectorySeparatorChar,[IO.Path]::AltDirectorySeparatorChar)
if(Test-Path -LiteralPath $Output){throw 'Preserve previous results; choose a fresh output directory'}
if(-not (Test-Path -LiteralPath (Join-Path $UsdDirectory 'lib/python/pxr') -PathType Container)){throw 'USD distribution with Python bindings required'}
$memoryScript=Join-Path $PSScriptRoot '../Get-SystemMemory.ps1'
$initial=& $memoryScript
if($initial.availablePhysicalBytes -lt 512MB -or $initial.availableCommitBytes -lt 2GB){throw 'Insufficient starting reserve for the CPU exporter check'}
New-Item -ItemType Directory -Path $Output | Out-Null
Copy-Item -LiteralPath $Executable -Destination (Join-Path $Output 'test_usd_exporter.exe')
$signedAdapter=$Scenario -in @('SignedAdapter','SignedAdapterStatic')
$variableSkin=$Scenario -in @('VariableSkin','VariableSkinDefaultJoints')
$defaultJoints=$Scenario -in @('DefaultJoints','VariableSkinDefaultJoints')
$readerName=if($signedAdapter){'read_usd_signed_adapter.py'}elseif($Scenario -eq 'Dynamic'){'read_usd_dynamic_buffers.py'}elseif($variableSkin){'read_usd_variable_skin.py'}else{'read_usd_exporter.py'}
$fixtureSource=if($signedAdapter){'test_usd_signed_adapter.cpp'}elseif($Scenario -eq 'Dynamic'){'test_usd_dynamic_buffers.cpp'}elseif($variableSkin){'test_usd_variable_skin.cpp'}else{'test_usd_exporter.cpp'}
foreach($name in @('Test-UsdExporter.ps1',$readerName,$fixtureSource)){
  Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $Output
}
if($signedAdapter){
  $repositoryRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
  $dependencies=@{}
  foreach($relative in @('tools/WinxRemix/winx_skin_packet_gpu.h','tools/WinxRemix/winx_skin_packet.h',
      'Sparkplug/Analysis/PC/spFixedShaderSkinning.h','Sparkplug/Code/SparkplugPC/spPCVertexDeclarationElements.h')){
    $inputFile=Join-Path $repositoryRoot $relative
    $copy=Join-Path (Join-Path $Output 'adapter-dependencies') $relative
    New-Item -ItemType Directory -Path (Split-Path -Parent $copy) -Force | Out-Null
    Copy-Item -LiteralPath $inputFile -Destination $copy
    $dependencies[$relative]=(Get-FileHash -LiteralPath $copy).Hash
  }
  $dependencies | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Output 'adapter-dependencies.json') -Encoding UTF8
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot '../ProcessLifetime.ps1') -Destination $Output
. (Join-Path $Output 'ProcessLifetime.ps1')
$prior=@{}
foreach($key in @('PATH','PYTHONHOME','PYTHONPATH','PYTHONNOUSERSITE','PYTHONDONTWRITEBYTECODE','PXR_WORK_THREAD_LIMIT','PXR_PLUGINPATH_NAME','DXVK_LOG_PATH')){
  $prior[$key]=[Environment]::GetEnvironmentVariable($key,'Process')
}
Add-Type @'
using System.Runtime.InteropServices;
public static class UsdExporterErrorMode {
  [DllImport("kernel32.dll")] public static extern uint SetErrorMode(uint mode);
}
'@
$oldErrorMode=[UsdExporterErrorMode]::SetErrorMode(0x8003)
$maximumBytes=[long]$MaximumWorkingSetMiB*1MB
$operations=[Collections.Generic.List[object]]::new()
function Invoke-BoundedUsdProcess([string]$Phase,[string]$File,[string]$Arguments){
  $p=$null;$signaled=$false;$forced=$false;$limited=$false;$timedOut=$false;$failure=$null;$exitCode=$null
  $peakWorking=0L;$peakPrivate=0L;$samples=[Collections.Generic.List[object]]::new()
  $watch=[Diagnostics.Stopwatch]::StartNew();$identity=$null
  try{
    $p=Start-Process -FilePath $File -ArgumentList $Arguments -WorkingDirectory $Output -WindowStyle Hidden -PassThru `
       -RedirectStandardOutput (Join-Path $Output ($Phase+'-stdout.log')) -RedirectStandardError (Join-Path $Output ($Phase+'-stderr.log'))
    $null=$p.Handle
    $identity=@{pid=$p.Id;path=$File;observedPath=$p.Path;started=$p.StartTime.ToString('o')}
    while(-not (Wait-OwnedProcess $p 100)){
      $p.Refresh();$self=Get-Process -Id $PID
      $working=$p.WorkingSet64+$self.WorkingSet64;$private=$p.PrivateMemorySize64+$self.PrivateMemorySize64;$self.Dispose()
      $peakWorking=[Math]::Max($peakWorking,$working);$peakPrivate=[Math]::Max($peakPrivate,$private)
      $available=& $memoryScript
      $samples.Add(@{milliseconds=$watch.ElapsedMilliseconds;working=$working;private=$private;available=$available})
      $limited=$working -gt $maximumBytes -or $private -gt $maximumBytes -or $available.availablePhysicalBytes -lt 256MB -or $available.availableCommitBytes -lt 512MB
      $timedOut=$watch.Elapsed.TotalSeconds -gt $MaximumSeconds
      if($limited -or $timedOut){$forced=$true;Stop-OwnedProcess $p;break}
    }
  }catch{$failure=$_.Exception.Message}
  finally{
    if($p){
      try{
        if(-not (Wait-OwnedProcess $p 0)){$forced=$true;Stop-OwnedProcess $p}
        $signaled=Wait-OwnedProcess $p 0;$exitCode=Get-OwnedProcessExitCode $p
      }catch{$failure=([string]$failure+' Cleanup: '+$_.Exception.Message).Trim()}
      finally{$p.Dispose()}
    }
  }
  $result=@{phase=$Phase;identity=$identity;signaled=$signaled;exitCode=$exitCode;forced=$forced;memoryLimited=$limited;timedOut=$timedOut;failure=$failure;
    remaining=@();milliseconds=$watch.ElapsedMilliseconds;peakWorkingBytes=$peakWorking;peakPrivateBytes=$peakPrivate}
  if($identity -and -not $signaled){$result.remaining=@($identity.pid)}
  $result.status=if($signaled -and $exitCode -eq 0 -and -not $forced -and -not $limited -and -not $timedOut -and -not $failure){'PASS'}else{'FAIL'}
  $operations.Add($result)
  $samples | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $Output ($Phase+'-memory.json')) -Encoding UTF8
  return $result
}
$failure=$null;$verdict=$null
try{
  $env:PATH=(Join-Path $UsdDirectory 'lib')+';'+(Join-Path $UsdDirectory 'bin')+';'+(Split-Path -Parent $Python)+';'+$env:PATH
  $env:PYTHONHOME=Split-Path -Parent $Python;$env:PYTHONPATH=$null;$env:PYTHONNOUSERSITE='1';$env:PYTHONDONTWRITEBYTECODE='1'
  $env:PXR_WORK_THREAD_LIMIT='2';$env:PXR_PLUGINPATH_NAME=Join-Path $UsdDirectory 'lib/usd';$env:DXVK_LOG_PATH=$Output
  @{scenario=$Scenario;initial=$initial;maximumOwnedBytes=$maximumBytes;minimumPhysicalBytes=256MB;minimumCommitBytes=512MB;maximumSeconds=$MaximumSeconds;
    executableSha256=(Get-FileHash -LiteralPath $Executable).Hash;pythonSha256=(Get-FileHash -LiteralPath $Python).Hash;usdDirectory=$UsdDirectory;
    helperSha256=(Get-FileHash -LiteralPath (Join-Path $Output 'ProcessLifetime.ps1')).Hash} |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $Output 'profile.json') -Encoding UTF8
  $exportArguments=if($Scenario -eq 'SignedAdapterStatic'){'captures --single-frame'}elseif($defaultJoints){'captures --default-joints'}else{'captures'}
  $export=Invoke-BoundedUsdProcess 'export' (Join-Path $Output 'test_usd_exporter.exe') $exportArguments
  $export | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $Output 'execution.json') -Encoding UTF8
  Copy-Item -LiteralPath (Join-Path $Output 'export-stdout.log') -Destination (Join-Path $Output 'stdout.log')
  Copy-Item -LiteralPath (Join-Path $Output 'export-stderr.log') -Destination (Join-Path $Output 'stderr.log')
  if($export.status -ne 'PASS'){throw 'CPU exporter failed; output preserved'}
  $reader=Join-Path $Output $readerName;$report=Join-Path $Output 'verdict.json'
  $arguments='"'+$reader+'" --run-directory "'+$Output+'" --usd-directory "'+$UsdDirectory+'" --output "'+$report+'"'
  if($defaultJoints){$arguments+=' --default-joints'}
  if($Scenario -eq 'SignedAdapterStatic'){$arguments+=' --single-frame'}
  $read=Invoke-BoundedUsdProcess 'read' $Python $arguments
  if(Test-Path -LiteralPath $report){$verdict=Get-Content -LiteralPath $report -Raw -Encoding UTF8 | ConvertFrom-Json}
  if($read.status -ne 'PASS'){throw 'USD readback failed; output preserved'}
  if(-not $verdict -or $verdict.status -ne 'PASS'){throw 'USD readback contract failed'}
}catch{$failure=$_.Exception.Message}
finally{
  $null=[UsdExporterErrorMode]::SetErrorMode($oldErrorMode)
  foreach($key in $prior.Keys){[Environment]::SetEnvironmentVariable($key,$prior[$key],'Process')}
}
$result=@{status=$(if($failure){'FAIL'}else{'PASS'});scenario=$Scenario;failure=$failure;operations=@($operations.ToArray());
  checks=$(if($verdict){$verdict.checks.Count}else{0});scope='CPU exporter and USD readback only. No GPU/capturer/texture writer/game qualification.'}
$result.failedChecks=if($verdict){@($verdict.checks | Where-Object {-not $_.passed}).Count}else{$null}
$result | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $Output 'result.json') -Encoding UTF8
$result | ConvertTo-Json -Depth 6
if($failure){throw $failure}
