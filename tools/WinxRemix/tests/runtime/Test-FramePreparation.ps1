param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,
      [ValidateSet('x86','x64')][string]$Platform='x86',
      [ValidateSet('Scheduler','LightSnapshot','LightPayload','LightWorldObserver')][string]$Suite='Scheduler')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../..'))
# These classes use standard C++ and shared recovered arithmetic, without
# executing game instructions or requiring renderer/Windows API declarations.
$testStem=switch($Suite){'Scheduler'{'test_frame_scheduler'}'LightSnapshot'{'test_scene_light_snapshot'}'LightPayload'{'test_light_payload_source'}'LightWorldObserver'{'test_light_world_observer'}}
$build=Join-Path $root "artifacts/WinxRemix/frame-preparation-tests/$Name"
if (Test-Path -LiteralPath $build) { throw 'Use a fresh evidence directory' }
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'MSVC tools not found' }
$environmentScript=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$sourceRoot=Join-Path $build 'source'
$snapshot=Join-Path $sourceRoot 'tools/WinxRemix'
New-Item -ItemType Directory -Path $snapshot -Force | Out-Null
. (Join-Path $root 'tools/WinxRemix/tests/Copy-SourceSnapshot.ps1')
Copy-WinxRemixSourceSnapshot -RepositoryRoot $root -ToolDestination $snapshot
Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $snapshot 'tests/runtime')
$testArea=switch($Suite){'Scheduler'{'runtime'}'LightSnapshot'{'scene'}'LightPayload'{'lighting'}'LightWorldObserver'{'lighting'}}
$source=Join-Path $snapshot "tests/$testArea/$testStem.cpp"
$commands=@"
@echo off
call "$environmentScript" $Platform
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MT /O2 /W4 /WX /I"$snapshot" "$source" /Fe:$testStem.exe
exit /b %errorlevel%
"@
$commandFile=Join-Path $build 'build.cmd'
[IO.File]::WriteAllText($commandFile,$commands,[Text.Encoding]::ASCII)
Push-Location $build
try {
    & $env:ComSpec /d /c $commandFile *> (Join-Path $build 'build.log')
    if ($LASTEXITCODE -ne 0) { throw "Frame preparation build failed; see $build/build.log" }
    $exe=Join-Path $build "$testStem.exe"
    $process=New-Object System.Diagnostics.Process
    $process.StartInfo.FileName=$exe
    $process.StartInfo.WorkingDirectory=$build
    $process.StartInfo.UseShellExecute=$false
    $process.StartInfo.CreateNoWindow=$true
    $process.StartInfo.RedirectStandardOutput=$true
    $process.StartInfo.RedirectStandardError=$true
    if (-not $process.Start()) { throw 'Failed to start owned CPU fixture' }
    $stdout=$process.StandardOutput.ReadToEndAsync()
    $stderr=$process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit(10000)) {
        Stop-Process -InputObject $process -Force
        $process.WaitForExit()
        throw 'Owned CPU fixture exceeded 10 seconds'
    }
    $exitCode=$process.ExitCode
    [IO.File]::WriteAllText((Join-Path $build 'result.json'),$stdout.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $build 'stderr.log'),$stderr.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
    $process.Dispose()
    if ($exitCode -ne 0) { throw "Frame preparation fixture failed ($exitCode); see $build/stderr.log" }
    $result=Get-Content -LiteralPath (Join-Path $build 'result.json') -Raw | ConvertFrom-Json
    if ($result.status -ne 'PASS') { throw 'Frame preparation fixture did not report PASS' }
    $inputs=@(Get-ChildItem -LiteralPath $sourceRoot -File -Recurse | ForEach-Object {
        @{file=$_.FullName.Substring($sourceRoot.Length+1).Replace('\','/');sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
    })
    @{schema=1;suite=$Suite;platform=$Platform;gpu=$false;gameExecuted=$false;sourceInputs=$inputs;
      exeSha256=(Get-FileHash -LiteralPath $exe).Hash;result=$result} |
        ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $build 'verification.json') -Encoding UTF8
    Get-Content -LiteralPath (Join-Path $build 'result.json')
} finally { Pop-Location }
