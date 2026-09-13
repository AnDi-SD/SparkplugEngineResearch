param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$build=Join-Path $root "local-data/rtx-remix/renderer-skinning-fix-tests/$Name"
& python (Join-Path $PSScriptRoot 'prepare_gpu.py') $build
if ($LASTEXITCODE -ne 0) { throw 'GPU source snapshot preparation failed' }
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'MSVC tools not found' }
$vcvars=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$commands=@"
@echo off
call "$vcvars" x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MT /O2 /arch:AVX2 /DNOMINMAX /DWIN32_LEAN_AND_MEAN /I"$build/source/vulkan-headers" /I"$build/source" /I"$build/source/include" /I"$build/source/src/dxvk/shaders" "$build/test_gpu_skinning.cpp" /Fe:test_gpu_skinning.exe /link "$build/vulkan-1.lib" user32.lib
exit /b %errorlevel%
"@
[IO.File]::WriteAllText((Join-Path $build 'build.cmd'),$commands,[Text.Encoding]::ASCII)
Push-Location $build
try {
    & $env:ComSpec /d /c (Join-Path $build 'build.cmd') *> (Join-Path $build 'build.log')
    if ($LASTEXITCODE -ne 0) { throw "GPU build failed; see $build/build.log" }
    $process=New-Object System.Diagnostics.Process
    $process.StartInfo.FileName=Join-Path $build 'test_gpu_skinning.exe'
    $process.StartInfo.Arguments='"'+(Join-Path $build 'gpu_skinning.spv')+'"'
    $process.StartInfo.WorkingDirectory=$build
    $process.StartInfo.UseShellExecute=$false
    $process.StartInfo.CreateNoWindow=$true
    $process.StartInfo.RedirectStandardOutput=$true
    $process.StartInfo.RedirectStandardError=$true
    if (-not $process.Start()) { throw 'Owned GPU fixture failed to start' }
    $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
    $timer=[Diagnostics.Stopwatch]::StartNew();$samples=[Collections.Generic.List[object]]::new()
    $reason='completed';$peakWorking=0L;$peakPrivate=0L
    while (-not $process.HasExited) {
        $process.Refresh();$shellProcess=Get-Process -Id $PID
        $working=$process.WorkingSet64+$shellProcess.WorkingSet64
        $private=$process.PrivateMemorySize64+$shellProcess.PrivateMemorySize64
        $peakWorking=[Math]::Max($peakWorking,$working);$peakPrivate=[Math]::Max($peakPrivate,$private)
        $samples.Add([pscustomobject]@{milliseconds=$timer.ElapsedMilliseconds;workingBytes=$working;privateBytes=$private})
        if ([Math]::Max($working,$private) -gt 900MB) { $reason='memory-bound';Stop-Process -InputObject $process -Force;break }
        if ($timer.ElapsedMilliseconds -gt 120000) { $reason='time-bound';Stop-Process -InputObject $process -Force;break }
        Start-Sleep -Milliseconds 100
    }
    $process.WaitForExit();$code=$process.ExitCode
    [IO.File]::WriteAllText((Join-Path $build 'events.jsonl'),$stdout.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $build 'stderr.log'),$stderr.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
    @{reason=$reason;exitCode=$code;milliseconds=$timer.ElapsedMilliseconds;limitBytes=900MB;peakWorkingBytes=$peakWorking;peakPrivateBytes=$peakPrivate;samples=$samples} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $build 'memory.json') -Encoding UTF8
    $process.Dispose()
    if ($reason -ne 'completed' -or $code -ne 0) { throw "GPU fixture failed ($reason, $code); see stderr.log and memory.json" }
    $events=@(Get-Content -LiteralPath (Join-Path $build 'events.jsonl') | ForEach-Object { $_ | ConvertFrom-Json })
    $result=$events[-1]
    if ($result.status -ne 'PASS' -or $result.dispatches -ne 36 -or -not $result.resourcesDestroyed) { throw 'GPU fixture did not finish every dispatch and cleanup' }
    $result | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $build 'result.json') -Encoding UTF8
    $result | ConvertTo-Json
} finally { Pop-Location }
