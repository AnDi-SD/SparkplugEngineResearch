param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,
      [ValidateSet('x86','x64')][string]$Platform='x64')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$build=Join-Path $root "local-data/rtx-remix/fixed-lighting-tests/$Name"
if (Test-Path -LiteralPath $build) { throw 'Choose a fresh result directory' }
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'MSVC tools not found' }
$environmentScript=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$files=@('tools/WinxRemix/winx_skin_packet.h','tools/WinxRemix/test_fixed_lighting.cpp',
    'Sparkplug/Analysis/PC/spFixedShaderLighting.h',
    'Sparkplug/Analysis/PC/spFixedShaderSkinning.h','Sparkplug/Code/SparkplugPC/spPCVertexDeclarationElements.h')
$hashes=@()
foreach ($relative in $files) {
    $destination=Join-Path (Join-Path $build 'source') $relative
    New-Item -ItemType Directory -Path (Split-Path $destination) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $root $relative) -Destination $destination
    $hashes+=@{path=$relative;sha256=(Get-FileHash -LiteralPath $destination).Hash}
}
$hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $build 'sources.json') -Encoding UTF8
Copy-Item -LiteralPath $PSCommandPath -Destination $build
$source=Join-Path $build 'source/tools/WinxRemix/test_fixed_lighting.cpp'
$commands=@"
@echo off
call "$environmentScript" $Platform
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MT /O2 /W4 "$source" /Fe:test_fixed_lighting.exe
exit /b %errorlevel%
"@
$commandFile=Join-Path $build 'build.cmd'
[IO.File]::WriteAllText($commandFile,$commands,[Text.Encoding]::ASCII)
Push-Location $build
try {
    & $env:ComSpec /d /c $commandFile *> (Join-Path $build 'build.log')
    if ($LASTEXITCODE -ne 0) { throw "Packet build failed; see $build/build.log" }
    $process=New-Object Diagnostics.Process
    $process.StartInfo.FileName=Join-Path $build 'test_fixed_lighting.exe'
    $process.StartInfo.WorkingDirectory=$build
    $process.StartInfo.UseShellExecute=$false
    $process.StartInfo.CreateNoWindow=$true
    $process.StartInfo.RedirectStandardOutput=$true
    $process.StartInfo.RedirectStandardError=$true
    if (-not $process.Start()) { throw 'Cannot start Fixed lighting tests' }
    $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
    $peakWorking=0L;$peakPrivate=0L;$started=[Diagnostics.Stopwatch]::StartNew();$limited=$false
    while (-not $process.WaitForExit(100)) {
        $process.Refresh();$own=Get-Process -Id $PID
        $working=$own.WorkingSet64+$process.WorkingSet64;$private=$own.PrivateMemorySize64+$process.PrivateMemorySize64
        $peakWorking=[Math]::Max($peakWorking,$working);$peakPrivate=[Math]::Max($peakPrivate,$private)
        if ($working -gt 900MB -or $private -gt 900MB -or $started.Elapsed.TotalSeconds -gt 30) {
            $limited=$true;Stop-Process -InputObject $process -Force;$process.WaitForExit();break
        }
    }
    $exitCode=$process.ExitCode
    [IO.File]::WriteAllText((Join-Path $build 'result.json'),$stdout.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $build 'stderr.log'),$stderr.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
    @{exitCode=$exitCode;limited=$limited;elapsedMilliseconds=$started.ElapsedMilliseconds;
      sampledPeakWorkingBytes=$peakWorking;sampledPeakPrivateBytes=$peakPrivate;platform=$Platform;
      executableSha256=(Get-FileHash -LiteralPath $process.StartInfo.FileName).Hash} |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $build 'execution.json') -Encoding UTF8
    $process.Dispose()
    if ($limited -or $exitCode -ne 0) { throw "Fixed lighting tests failed; see $build" }
    Get-Content -LiteralPath (Join-Path $build 'result.json')
} finally { Pop-Location }
