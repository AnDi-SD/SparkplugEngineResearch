param([ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name=('scene-lights-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')), [switch]$SkipRun)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$toolRoot=$PSScriptRoot
$dependencyInclude = & (Join-Path $toolRoot 'Prepare-Dependencies.ps1') -Offline
$build=Join-Path $root "local-data/rtx-remix/scene-light-tests/$Name"
if (Test-Path -LiteralPath $build) { throw 'Use a fresh evidence directory' }
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'MSVC x86 tools not found' }
$environmentScript=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$include=Join-Path $root 'local-data/rtx-remix/upstream/dxvk-remix/public/include'
$snapshot=Join-Path $build 'source/tools/WinxRemix'
New-Item -ItemType Directory -Path $snapshot -Force | Out-Null
Get-ChildItem -LiteralPath $toolRoot -File | Where-Object { $_.Extension -in '.h','.cpp' } | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $snapshot }
Copy-Item -LiteralPath (Join-Path $dependencyInclude 'third-party') -Destination $snapshot -Recurse
foreach ($relative in @('Sparkplug/Analysis/PC/SparkplugAbi.h','Sparkplug/Analysis/PC/SparkBaseAbi.h','Sparkplug/Analysis/PC/spNodeTransformMath.h','Sparkplug/Analysis/PC/spFixedShaderSkinning.h','Sparkplug/Analysis/PC/spRenderNodeMath.h','Sparkplug/Analysis/PC/spColorMath.h','Sparkplug/Code/SparkplugPC/spPCVertexDeclarationElements.h','Sparkplug/Code/SparkplugDX/spPCTextureStateMapping.h','Sparkplug/Code/SparkplugDX/spPCMaterialStateMapping.h','Sparkplug/Code/SparkplugDX/spPCLightPayload.h','Sparkplug/Code/SparkplugDX/spPCDXVertexBytes.h')) {
    $target=Join-Path (Join-Path $build 'source') $relative
    New-Item -ItemType Directory -Path (Split-Path $target) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $root $relative) -Destination $target
}
$source=Join-Path $snapshot 'test_scene_lights.cpp'
# Own linked registry buffers and recording light API; no device or game.
$commands=@"
@echo off
call "$environmentScript" x86
if errorlevel 1 exit /b %errorlevel%
cl /I"$dependencyInclude" /nologo /std:c++17 /EHsc /MT /O2 /W4 /I"$include" "$source" /Fe:test_scene_lights.exe /link user32.lib
exit /b %errorlevel%
"@
$commandFile=Join-Path $build 'build.cmd'
[IO.File]::WriteAllText($commandFile,$commands,[Text.Encoding]::ASCII)
Push-Location $build
try {
    & $env:ComSpec /d /c $commandFile *> (Join-Path $build 'build.log')
    if ($LASTEXITCODE -ne 0) { throw "Scene light build failed; see $build/build.log" }
    if ($SkipRun) { Write-Output "Built scene light fixture: $build"; return }
    Copy-Item -LiteralPath $PSCommandPath -Destination $build
    $exe=Join-Path $build 'test_scene_lights.exe'
    $process=New-Object System.Diagnostics.Process
    $process.StartInfo.FileName=$exe
    $process.StartInfo.WorkingDirectory=$build
    $process.StartInfo.UseShellExecute=$false
    $process.StartInfo.CreateNoWindow=$true
    $process.StartInfo.RedirectStandardOutput=$true
    $process.StartInfo.RedirectStandardError=$true
    if (-not $process.Start()) { throw 'Failed to start owned scene light fixture' }
    $stdoutTask=$process.StandardOutput.ReadToEndAsync()
    $stderrTask=$process.StandardError.ReadToEndAsync()
    $started=[Diagnostics.Stopwatch]::StartNew();$limited=$false;$peakWorking=0L;$peakPrivate=0L
    while (-not $process.WaitForExit(100)) {
        $process.Refresh();$own=Get-Process -Id $PID
        $working=$process.WorkingSet64+$own.WorkingSet64;$private=$process.PrivateMemorySize64+$own.PrivateMemorySize64
        $peakWorking=[Math]::Max($peakWorking,$working);$peakPrivate=[Math]::Max($peakPrivate,$private)
        if ($working -gt 900MB -or $private -gt 900MB -or $started.Elapsed.TotalSeconds -gt 15) {
            $limited=$true;Stop-Process -InputObject $process -Force;$process.WaitForExit();break
        }
    }
    $exitCode=$process.ExitCode
    [IO.File]::WriteAllText((Join-Path $build 'result.log'),$stdoutTask.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $build 'stderr.log'),$stderrTask.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
    @{exitCode=$exitCode;limited=$limited;elapsedMilliseconds=$started.ElapsedMilliseconds;
      sampledPeakWorkingBytes=$peakWorking;sampledPeakPrivateBytes=$peakPrivate;gpu=$false;nativeGameCodeExecuted=$false;
      executableSha256=(Get-FileHash -LiteralPath $exe).Hash} |
      ConvertTo-Json | Set-Content -LiteralPath (Join-Path $build 'execution.json') -Encoding UTF8
    $process.Dispose()
    if ($limited -or $exitCode -ne 0) { throw "Scene light fixture failed ($exitCode); see $build/stderr.log" }
    Get-Content -LiteralPath (Join-Path $build 'result.log')
    Get-Content -LiteralPath (Join-Path $build 'execution.json')
} finally { Pop-Location }
