param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$build=Join-Path $root "local-data/rtx-remix/light-ownership-tests/$Name"
if(Test-Path -LiteralPath $build){throw 'Use a fresh evidence directory'}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $installation){throw 'MSVC x86 tools not found'}
$vcvars=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$source=Join-Path $build 'source'
New-Item -ItemType Directory -Path (Join-Path $source 'third-party') -Force | Out-Null
foreach($relative in @('winx_scene_lights.h','test_light_ownership.cpp','third-party/remix_light_conversion.h')) {
  Copy-Item -LiteralPath (Join-Path $PSScriptRoot $relative) -Destination (Join-Path $source $relative)
}
Copy-Item -LiteralPath $PSCommandPath -Destination $build
$apiInclude=Join-Path $root 'local-data/rtx-remix/upstream/dxvk-remix/public/include'
$commands=@"
@echo off
call "$vcvars" x86
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MT /O2 /W4 /I"$apiInclude" "$source\test_light_ownership.cpp" /Fe:test_light_ownership.exe
exit /b %errorlevel%
"@
[IO.File]::WriteAllText((Join-Path $build 'build.cmd'),$commands,[Text.Encoding]::ASCII)
Push-Location $build
try {
  & $env:ComSpec /d /c (Join-Path $build 'build.cmd') *> (Join-Path $build 'build.log')
  if($LASTEXITCODE -ne 0){throw "Build failed; preserved $build/build.log"}
  $process=New-Object System.Diagnostics.Process
  $process.StartInfo.FileName=Join-Path $build 'test_light_ownership.exe'
  $process.StartInfo.WorkingDirectory=$build
  $process.StartInfo.UseShellExecute=$false
  $process.StartInfo.CreateNoWindow=$true
  $process.StartInfo.RedirectStandardOutput=$true
  $process.StartInfo.RedirectStandardError=$true
  if(-not $process.Start()){throw 'Failed to start owned CPU fixture'}
  $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
  $timeout=-not $process.WaitForExit(30000)
  if($timeout){$process.Kill();$process.WaitForExit()}
  [IO.File]::WriteAllText((Join-Path $build 'result.json'),$stdout.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
  [IO.File]::WriteAllText((Join-Path $build 'stderr.log'),$stderr.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
  @{pid=$process.Id;exitCode=$process.ExitCode;timeout=$timeout;exe=$process.StartInfo.FileName;gpu=$false} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $build 'process.json') -Encoding UTF8
  if($timeout -or $process.ExitCode -ne 0){throw 'Owned fixture failed; result/stderr/process preserved'}
  $result=Get-Content -LiteralPath (Join-Path $build 'result.json') -Raw | ConvertFrom-Json
  if($result.status -ne 'PASS'){throw 'Fixture did not report PASS'}
  $hashes=@(Get-ChildItem -LiteralPath $source -Recurse -File | ForEach-Object {@{path=$_.FullName.Substring($build.Length+1);sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
  @{status='PASS';result=$result;maxSeconds=30;gpu=$false;nativeGameCodeExecuted=$false;sourceHashes=$hashes;
    executableSha256=(Get-FileHash -LiteralPath $process.StartInfo.FileName).Hash} |
    ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $build 'verification.json') -Encoding UTF8
  $process.Dispose();$result | ConvertTo-Json -Compress
} finally {Pop-Location}
