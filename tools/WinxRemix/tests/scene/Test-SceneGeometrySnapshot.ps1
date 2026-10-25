param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,
      [ValidateSet('x86','x64')][string]$Platform='x86')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../..'))
$memory=& (Join-Path $PSScriptRoot '../../scripts/runtime/Get-SystemMemory.ps1')
if($memory.availablePhysicalBytes -lt 512MB){throw 'The CPU fixture build requires 512 MiB of available physical memory'}
$build=Join-Path $root "artifacts/WinxRemix/scene-geometry-snapshot-tests/$Name"
if(Test-Path -LiteralPath $build){throw 'Use a fresh evidence directory'}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $installation){throw 'MSVC tools not found'}
$environmentScript=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$snapshot=Join-Path $build 'source/tools/WinxRemix'
New-Item -ItemType Directory -Path $snapshot | Out-Null
. (Join-Path $root 'tools/WinxRemix/tests/Copy-SourceSnapshot.ps1')
Copy-WinxRemixSourceSnapshot -RepositoryRoot $root -ToolDestination $snapshot
Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $snapshot 'tests/scene')
$source=Join-Path $snapshot 'tests/scene/test_scene_geometry_snapshot.cpp'
$commands=@"
@echo off
call "$environmentScript" $Platform
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MT /O2 /W4 /WX "$source" /Fe:test_scene_geometry_snapshot.exe
exit /b %errorlevel%
"@
$commandFile=Join-Path $build 'build.cmd'
[IO.File]::WriteAllText($commandFile,$commands,[Text.Encoding]::ASCII)
Push-Location $build
try{
    & $env:ComSpec /d /c $commandFile *> (Join-Path $build 'build.log')
    if($LASTEXITCODE -ne 0){throw "Scene geometry snapshot build failed; see $build/build.log"}
    $exe=Join-Path $build 'test_scene_geometry_snapshot.exe'
    $process=New-Object System.Diagnostics.Process
    $process.StartInfo.FileName=$exe
    $process.StartInfo.WorkingDirectory=$build
    $process.StartInfo.UseShellExecute=$false
    $process.StartInfo.CreateNoWindow=$true
    $process.StartInfo.RedirectStandardOutput=$true
    $process.StartInfo.RedirectStandardError=$true
    if(-not $process.Start()){throw 'Failed to start owned CPU fixture'}
    $stdout=$process.StandardOutput.ReadToEndAsync()
    $stderr=$process.StandardError.ReadToEndAsync()
    if(-not $process.WaitForExit(10000)){
        Stop-Process -InputObject $process -Force
        $process.WaitForExit()
        throw 'Owned CPU fixture exceeded 10 seconds'
    }
    $exitCode=$process.ExitCode
    [IO.File]::WriteAllText((Join-Path $build 'result.json'),$stdout.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $build 'stderr.log'),$stderr.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
    $process.Dispose()
    if($exitCode -ne 0){throw "Scene geometry snapshot fixture failed ($exitCode); see $build/stderr.log"}
    $result=Get-Content -LiteralPath (Join-Path $build 'result.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if($result.status -ne 'PASS'){throw 'Scene geometry snapshot fixture did not report PASS'}
    $inputs=@(Get-ChildItem -LiteralPath $snapshot -File | ForEach-Object {
        @{file=$_.Name;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
    })
    @{schema=1;platform=$Platform;gpu=$false;gameExecuted=$false;sourceInputs=$inputs;
      exeSha256=(Get-FileHash -LiteralPath $exe).Hash;result=$result} |
        ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $build 'verification.json') -Encoding UTF8
    Get-Content -LiteralPath (Join-Path $build 'result.json') -Encoding UTF8
}finally{Pop-Location}
