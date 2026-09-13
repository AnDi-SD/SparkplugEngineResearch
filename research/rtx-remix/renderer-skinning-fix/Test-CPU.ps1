param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$build=Join-Path $root "local-data/rtx-remix/renderer-skinning-fix-tests/$Name"
& python (Join-Path $PSScriptRoot 'prepare_cpu.py') $build
if ($LASTEXITCODE -ne 0) { throw 'Source snapshot preparation failed' }
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'MSVC tools not found' }
$vcvars=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$commands=@"
@echo off
call "$vcvars" x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MT /O2 /arch:AVX2 /DNOMINMAX /DWIN32_LEAN_AND_MEAN /I"$build/source/include" /I"$build/source/test-platform" /I"$build/source/src/dxvk/shaders" "$build/test_skin_strides.cpp" /Fe:test_skin_strides.exe /link user32.lib
exit /b %errorlevel%
"@
[IO.File]::WriteAllText((Join-Path $build 'build.cmd'),$commands,[Text.Encoding]::ASCII)
Push-Location $build
try {
    & $env:ComSpec /d /c (Join-Path $build 'build.cmd') *> (Join-Path $build 'build.log')
    if ($LASTEXITCODE -ne 0) { throw "CPU build failed; see $build/build.log" }
    $process=New-Object System.Diagnostics.Process
    $process.StartInfo.FileName=Join-Path $build 'test_skin_strides.exe'
    $process.StartInfo.WorkingDirectory=$build
    $process.StartInfo.UseShellExecute=$false
    $process.StartInfo.CreateNoWindow=$true
    $process.StartInfo.RedirectStandardOutput=$true
    $process.StartInfo.RedirectStandardError=$true
    if (-not $process.Start()) { throw 'Owned CPU fixture failed to start' }
    $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit(10000)) { Stop-Process -InputObject $process -Force;throw 'Owned CPU fixture exceeded 10 seconds' }
    $code=$process.ExitCode
    [IO.File]::WriteAllText((Join-Path $build 'result.json'),$stdout.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $build 'stderr.log'),$stderr.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
    $process.Dispose()
    if ($code -ne 0) { throw "CPU fixture failed ($code); see stderr.log" }
    $result=Get-Content -LiteralPath (Join-Path $build 'result.json') -Raw | ConvertFrom-Json
    if ($result.status -ne 'PASS') { throw 'CPU fixture did not report PASS' }
    Get-Content -LiteralPath (Join-Path $build 'result.json')
} finally { Pop-Location }
