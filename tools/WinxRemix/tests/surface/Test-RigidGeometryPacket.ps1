param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,
      [ValidateSet('x86','x64')][string]$Platform='x86')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../..'))
$memory=& (Join-Path $PSScriptRoot '../../scripts/runtime/Get-SystemMemory.ps1')
if($memory.availablePhysicalBytes -lt 512MB){throw 'The CPU fixture build requires 512 MiB of available physical memory'}
$build=Join-Path $root "artifacts/WinxRemix/rigid-geometry-packet-tests/$Name"
if(Test-Path -LiteralPath $build){throw 'Use a fresh evidence directory'}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $installation){throw 'MSVC tools not found'}
$environmentScript=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$snapshot=Join-Path $build 'source/tools/WinxRemix'
New-Item -ItemType Directory -Path $snapshot | Out-Null
. (Join-Path $root 'tools/WinxRemix/tests/Copy-SourceSnapshot.ps1')
Copy-WinxRemixSourceSnapshot -RepositoryRoot $root -ToolDestination $snapshot
Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $snapshot 'tests/surface')
$runtimeSnapshot=Join-Path $snapshot 'scripts/runtime'
New-Item -ItemType Directory -Path $runtimeSnapshot -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'tools/WinxRemix/scripts/runtime/Get-SystemMemory.ps1') -Destination $runtimeSnapshot
foreach($file in @('Sparkplug/Analysis/PC/SparkplugAbi.h','Sparkplug/Analysis/PC/SparkBaseAbi.h',
                   'Sparkplug/Analysis/PC/spColorMath.h',
                   'Sparkplug/Code/SparkplugDX/spPCTextureStateMapping.h',
                   'Sparkplug/Code/SparkplugDX/spPCMaterialStateMapping.h')){
    $destination=Join-Path $build ('source/'+$file)
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $root $file) -Destination $destination
}
$source=Join-Path $snapshot 'tests/surface/test_rigid_geometry_packet.cpp'
$commands=@"
@echo off
call "$environmentScript" $Platform
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MT /O2 /W4 /WX "$source" /Fe:test_rigid_geometry_packet.exe
exit /b %errorlevel%
"@
$commandFile=Join-Path $build 'build.cmd'
[IO.File]::WriteAllText($commandFile,$commands,[Text.Encoding]::ASCII)
Push-Location $build
try{
    & $env:ComSpec /d /c $commandFile *> (Join-Path $build 'build.log')
    if($LASTEXITCODE -ne 0){throw "Rigid geometry packet build failed; see $build/build.log"}
    $exe=Join-Path $build 'test_rigid_geometry_packet.exe'
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
    if($exitCode -ne 0){throw "Rigid geometry packet fixture failed ($exitCode); see $build/stderr.log"}
    $result=Get-Content -LiteralPath (Join-Path $build 'result.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if($result.status -ne 'PASS' -or $result.scope -ne 'owned rigid geometry conversion packet only' -or $result.checks -lt 1){
        throw 'Rigid geometry packet fixture did not report its expected PASS result'
    }
    $sourceRoot=Join-Path $build 'source'
    $inputs=@(Get-ChildItem -LiteralPath $sourceRoot -File -Recurse | ForEach-Object {
        @{file=$_.FullName.Substring($sourceRoot.Length+1).Replace('\','/');sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
    })
    @{schema=1;platform=$Platform;gpu=$false;gameExecuted=$false;sourceInputs=$inputs;
      exeSha256=(Get-FileHash -LiteralPath $exe).Hash;result=$result} |
        ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $build 'verification.json') -Encoding UTF8
    Get-Content -LiteralPath (Join-Path $build 'result.json') -Encoding UTF8
}finally{Pop-Location}
