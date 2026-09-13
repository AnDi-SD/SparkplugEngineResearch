param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,[switch]$SkipRun)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$build=Join-Path $root "local-data/rtx-remix/native-update-tests/$Name"
if(Test-Path -LiteralPath $build){throw 'Use a fresh evidence directory'}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $installation){throw 'MSVC x86 tools not found'}
$environmentScript=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$files=@('research/rtx-remix/winx_native_update_source.h','research/rtx-remix/test_native_update.cpp','research/rtx-remix/Test-NativeUpdate.ps1','Sparkplug/Analysis/PC/SparkplugAbi.h','Sparkplug/Analysis/PC/SparkBaseAbi.h')
$manifest=@()
foreach($relative in $files){
  $source=Join-Path $root $relative
  $target=Join-Path (Join-Path $build 'source') $relative
  New-Item -ItemType Directory -Path (Split-Path $target) -Force | Out-Null
  Copy-Item -LiteralPath $source -Destination $target
  $manifest+=@{path=$relative;sha256=(Get-FileHash -LiteralPath $target).Hash}
}
$manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $build 'source-manifest.json') -Encoding UTF8
$source=Join-Path $build 'source/research/rtx-remix/test_native_update.cpp'
$commands=@"
@echo off
call "$environmentScript" x86
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MT /O2 /W4 "$source" /Fe:test_native_update.exe /link user32.lib /MANIFEST:EMBED /MANIFESTUAC:"level='asInvoker' uiAccess='false'"
exit /b %errorlevel%
"@
$commandFile=Join-Path $build 'build.cmd'
[IO.File]::WriteAllText($commandFile,$commands,[Text.Encoding]::ASCII)
Push-Location $build
try{
  & $env:ComSpec /d /c $commandFile *> (Join-Path $build 'build.log')
  if($LASTEXITCODE -ne 0){throw "Native update build failed; see $build/build.log"}
  if($SkipRun){Write-Output "Built CPU native update fixture: $build";return}
  $exe=Join-Path $build 'test_native_update.exe'
  $process=New-Object System.Diagnostics.Process
  $process.StartInfo.FileName=$exe
  $process.StartInfo.WorkingDirectory=$build
  $process.StartInfo.UseShellExecute=$false
  $process.StartInfo.CreateNoWindow=$true
  $process.StartInfo.RedirectStandardOutput=$true
  $process.StartInfo.RedirectStandardError=$true
  if(-not $process.Start()){throw 'Failed to start owned CPU fixture'}
  $stdoutTask=$process.StandardOutput.ReadToEndAsync()
  $stderrTask=$process.StandardError.ReadToEndAsync()
  $timedOut=-not $process.WaitForExit(30000)
  if($timedOut){Stop-Process -InputObject $process -Force;$process.WaitForExit()}
  $exitCode=$process.ExitCode
  [IO.File]::WriteAllText((Join-Path $build 'result.json'),$stdoutTask.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
  [IO.File]::WriteAllText((Join-Path $build 'stderr.log'),$stderrTask.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
  $process.Dispose()
  if($timedOut){throw 'Owned CPU fixture exceeded 30-second outer bound (internal watchdog 25 seconds)'}
  if($exitCode -ne 0){throw "Native update fixture failed ($exitCode); see $build/stderr.log"}
  $result=Get-Content -LiteralPath (Join-Path $build 'result.json') -Raw | ConvertFrom-Json
  if($result.status -ne 'PASS'){throw 'Native update fixture did not report PASS'}
  @{schema=1;gpu=$false;nativeGameCodeExecuted=$false;watchdogSeconds=25;outerTimeoutSeconds=30;exeSha256=(Get-FileHash -LiteralPath $exe).Hash;result=$result} |
    ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $build 'verification.json') -Encoding UTF8
  Get-Content -LiteralPath (Join-Path $build 'result.json')
}catch{
  @{schema=1;status='FAIL';error=$_.Exception.Message;gpu=$false;nativeGameCodeExecuted=$false} |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $build 'failure.json') -Encoding UTF8
  throw
}finally{Pop-Location}
