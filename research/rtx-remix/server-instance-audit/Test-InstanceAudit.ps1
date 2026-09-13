param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$dir=Join-Path $root "local-data/rtx-remix/server-instance-audit/$Name"
if(Test-Path -LiteralPath $dir){throw 'Use a fresh evidence directory'}
New-Item -ItemType Directory -Path (Join-Path $dir 'source') -Force | Out-Null
$header=Join-Path $root 'local-data/rtx-remix/direct-camera-bridge-work/bridge/src/server/instance_audit.h'
Copy-Item -LiteralPath $header,(Join-Path $PSScriptRoot 'test_instance_audit.cpp') -Destination (Join-Path $dir 'source')
Copy-Item -LiteralPath $PSCommandPath -Destination $dir
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $installation){throw 'MSVC tools missing'}
$vcvars=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$commands=@"
@echo off
call "$vcvars" x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MT /O2 /W4 "$dir\source\test_instance_audit.cpp" /Fe:test_instance_audit.exe
exit /b %errorlevel%
"@
[IO.File]::WriteAllText((Join-Path $dir 'build.cmd'),$commands,[Text.Encoding]::ASCII)
Push-Location $dir
try {
  & $env:ComSpec /d /c (Join-Path $dir 'build.cmd') *> (Join-Path $dir 'build.log')
  if($LASTEXITCODE -ne 0){throw "CPU build failed: $dir/build.log"}
  $p=New-Object Diagnostics.Process
  $p.StartInfo.FileName=Join-Path $dir 'test_instance_audit.exe'
  $p.StartInfo.Arguments='"'+$dir+'"'
  $p.StartInfo.WorkingDirectory=$dir
  $p.StartInfo.UseShellExecute=$false;$p.StartInfo.CreateNoWindow=$true
  $p.StartInfo.RedirectStandardOutput=$true;$p.StartInfo.RedirectStandardError=$true
  if(-not $p.Start()){throw 'Owned CPU fixture failed to start'}
  $out=$p.StandardOutput.ReadToEndAsync();$err=$p.StandardError.ReadToEndAsync()
  $timeout=-not $p.WaitForExit(30000)
  if($timeout){$p.Kill();$p.WaitForExit()}
  [IO.File]::WriteAllText((Join-Path $dir 'result.json'),$out.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
  [IO.File]::WriteAllText((Join-Path $dir 'stderr.log'),$err.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
  $code=$p.ExitCode
  @{pid=$p.Id;exe=$p.StartInfo.FileName;timeout=$timeout;exitCode=$code;gpu=$false;nativeCode=$false} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $dir 'process.json') -Encoding UTF8
  $p.Dispose()
  if($timeout -or $code -ne 0){throw "CPU fixture failed; preserved $dir"}
  $result=Get-Content -LiteralPath (Join-Path $dir 'result.json') -Raw | ConvertFrom-Json
  if($result.status -ne 'PASS'){throw 'CPU fixture did not report PASS'}
  $rows=@(Get-Content -LiteralPath (Join-Path $dir 'sequence.jsonl') | ForEach-Object {$_ | ConvertFrom-Json})
  $intervals=@($rows | Where-Object {$_.event -eq 'interval'})
  if(($intervals | Measure-Object -Property apiCalls -Sum).Sum -ne $result.apiCalls){throw 'JSONL interval totals lost API calls'}
  if(($intervals | Measure-Object -Property rendererApiSuccess -Sum).Sum -ne $result.success){throw 'JSONL success conservation failed'}
  if(($intervals | Measure-Object -Property rendererApiErrors -Sum).Sum -ne $result.errors){throw 'JSONL error conservation failed'}
  $first=$intervals[0]
  if(-not $first.complete -or -not $first.qualifiedOrder -or $first.apiCalls -ne 3 -or $first.earlySuccess -ne 1 -or $first.earlyErrors -ne 1 -or $first.firstDrawPrimitiveCount -ne 0){throw 'First actual JSON interval differs from fixture sequence'}
  if(-not ($intervals | Where-Object {$_.boundary -eq 'registration_boundary' -and $_.apiCalls -eq 1 -and -not $_.complete})){throw 'Preregistration API return was not emitted as incomplete'}
  if(-not ($intervals | Where-Object {$_.boundary -eq 'queue_exit_unexpected' -and $_.apiCalls -eq 1 -and -not $_.complete})){throw 'Queue tail is missing or falsely complete'}
  $cap=@(Get-Content -LiteralPath (Join-Path $dir 'cap.jsonl') | ForEach-Object {$_ | ConvertFrom-Json})
  if($cap[-1].event -ne 'cap' -or (Get-Item -LiteralPath (Join-Path $dir 'cap.jsonl')).Length -gt 2048){throw 'Bounded output cap failure'}
  $hashes=@(Get-ChildItem -LiteralPath $dir -Recurse -File | Where-Object {$_.Extension -ne '.obj'} | ForEach-Object {@{path=$_.FullName.Substring($dir.Length+1);sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
  @{status='PASS';result=$result;jsonIntervalCount=$intervals.Count;jsonConservation=$true;gpu=$false;maxSeconds=30;artifacts=$hashes} |
    ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $dir 'verification.json') -Encoding UTF8
  Get-Content -LiteralPath (Join-Path $dir 'result.json')
} finally {Pop-Location}
