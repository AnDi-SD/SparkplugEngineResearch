param(
  [Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,
  [switch]$SkipRun,
  [switch]$RunOnly
)
$ErrorActionPreference='Stop'
if ($SkipRun -and $RunOnly) { throw 'SkipRun and RunOnly are mutually exclusive' }
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$build=Join-Path $root "local-data/rtx-remix/native-camera-tests/$Name"
$binary=Join-Path $build 'test_native_camera.exe'
$manifestPath=Join-Path $build 'build.json'
if (-not $RunOnly) {
  if (Test-Path -LiteralPath $build) { throw 'Use a fresh evidence directory, or RunOnly for a previously built fixture' }
  $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
  $installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
  if (-not $installation) { throw 'MSVC x86 tools not found' }
  $environmentScript=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
  $include=Join-Path $root 'local-data/rtx-remix/upstream/dxvk-remix/public/include'
  New-Item -ItemType Directory -Path $build | Out-Null
  $snapshot=Join-Path $build 'source/research/rtx-remix'
  New-Item -ItemType Directory -Path $snapshot | Out-Null
  Get-ChildItem -LiteralPath $PSScriptRoot -File | Where-Object { $_.Extension -in '.h','.cpp' } | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $snapshot
  }
  Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'third-party') -Destination $snapshot -Recurse
  Copy-Item -LiteralPath $PSCommandPath -Destination $build
  $abiSnapshot=Join-Path $build 'source/Sparkplug/Analysis/PC'
  New-Item -ItemType Directory -Path $abiSnapshot -Force | Out-Null
  foreach ($header in 'SparkplugAbi.h','SparkBaseAbi.h') {
    Copy-Item -LiteralPath (Join-Path $root "Sparkplug/Analysis/PC/$header") -Destination $abiSnapshot
  }
  $layoutSnapshot=Join-Path $build 'source/Sparkplug/Code/SparkplugPC'
  New-Item -ItemType Directory -Path $layoutSnapshot -Force | Out-Null
  Copy-Item -LiteralPath (Join-Path $root 'Sparkplug/Code/SparkplugPC/spPCVertexDeclarationElements.h') -Destination $layoutSnapshot
  $source=Join-Path $snapshot 'test_native_camera.cpp'
  $commands=@"
@echo off
call "$environmentScript" x86
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MT /O2 /W4 /I"$include" "$source" /Fe:test_native_camera.exe /link user32.lib
exit /b %errorlevel%
"@
  $commandFile=Join-Path $build 'build.cmd'
  [IO.File]::WriteAllText($commandFile,$commands,[Text.Encoding]::ASCII)
  Push-Location $build
  try {
    & $env:ComSpec /d /c $commandFile *> (Join-Path $build 'build.log')
    $buildExit=$LASTEXITCODE
  } finally { Pop-Location }
  $hashes=@(Get-ChildItem -LiteralPath (Join-Path $build 'source') -File -Recurse | ForEach-Object {
    [ordered]@{path=$_.FullName.Substring($build.Length+1);sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
  })
  $manifest=[ordered]@{
    schema=1;kind='native-camera-adapter';utc=[DateTime]::UtcNow.ToString('o');architecture='x86';buildExitCode=$buildExit
    exeSha256=$(if (Test-Path -LiteralPath $binary) { (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash } else { $null })
    sdkInclude=$include;sdkHeaderSha256=(Get-FileHash -LiteralPath (Join-Path $include 'remix/remix_c.h') -Algorithm SHA256).Hash
    sources=$hashes;gameEvidence=$false
  }
  $manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
  if ($buildExit -ne 0) { Get-Content -LiteralPath (Join-Path $build 'build.log');throw "Camera fixture build failed: $buildExit" }
}
if (-not (Test-Path -LiteralPath $manifestPath)) { throw 'Existing build manifest is required' }
$manifest=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.buildExitCode -ne 0 -or (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash -ne $manifest.exeSha256) {
  throw 'Fixture binary does not match successful frozen build manifest'
}
if ($SkipRun) {
  [ordered]@{status='BUILT_NOT_RUN';build=$build;exeSha256=$manifest.exeSha256;gpuRun=$false} | ConvertTo-Json
  return
}
$stdout=Join-Path $build 'run.stdout.json'
$stderr=Join-Path $build 'run.stderr.txt'
$resultPath=Join-Path $build 'run.json'
if ((Test-Path -LiteralPath $stdout) -or (Test-Path -LiteralPath $stderr) -or (Test-Path -LiteralPath $resultPath)) {
  throw 'Run evidence already exists; use a fresh build name rather than overwrite an earlier result'
}
$watch=[Diagnostics.Stopwatch]::StartNew()
$process=Start-Process -FilePath $binary -WorkingDirectory $build -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
$null=$process.Handle
$completed=$process.WaitForExit(30000)
if (-not $completed) { $process.Kill();$process.WaitForExit() }
$watch.Stop();$process.Refresh()
$run=[ordered]@{
  schema=1;utc=[DateTime]::UtcNow.ToString('o');pid=$process.Id;completed=$completed;exitCode=$process.ExitCode
  durationSeconds=$watch.Elapsed.TotalSeconds;exeSha256=$manifest.exeSha256;gameEvidence=$false
}
$run | ConvertTo-Json | Set-Content -LiteralPath $resultPath -Encoding UTF8
Get-Content -LiteralPath $stdout
if (-not $completed -or $process.ExitCode -ne 0) { Get-Content -LiteralPath $stderr;throw 'Native camera adapter integration failed (see bounded run evidence)' }
$result=Get-Content -LiteralPath $stdout -Raw | ConvertFrom-Json
if ($result.status -ne 'PASS') { throw 'Fixture exited without a PASS result' }
