param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,
      [ValidateSet('x86','x64')][string]$Platform='x86')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
# Verify pinned headers offline; no game installation or dependency download.
$null = & (Join-Path $PSScriptRoot 'Prepare-Dependencies.ps1') -Offline
$build=Join-Path $root "artifacts/WinxRemix/resource-registry-tests/$Name"
if (Test-Path -LiteralPath $build) { throw 'Use a fresh evidence directory' }
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'MSVC tools not found' }
$environmentScript=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$include=Join-Path $root 'local-data/rtx-remix/upstream/dxvk-remix/public/include'
$snapshot=Join-Path $build 'source'
New-Item -ItemType Directory -Path $snapshot -Force | Out-Null
foreach ($file in @('winx_remix_resource_registry.h','test_remix_resource_registry.cpp','Test-RemixResourceRegistry.ps1','dependencies.json')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination $snapshot
}
$headerTarget=Join-Path $snapshot 'remix/remix_c.h'
New-Item -ItemType Directory -Path (Split-Path $headerTarget) -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $include 'remix/remix_c.h') -Destination $headerTarget
$source=Join-Path $snapshot 'test_remix_resource_registry.cpp'
$commands=@"
@echo off
call "$environmentScript" $Platform
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MT /O2 /W4 /WX /I"$snapshot" "$source" /Fe:test_remix_resource_registry.exe
exit /b %errorlevel%
"@
$commandFile=Join-Path $build 'build.cmd'
[IO.File]::WriteAllText($commandFile,$commands,[Text.Encoding]::ASCII)
Push-Location $build
try {
    & $env:ComSpec /d /c $commandFile *> (Join-Path $build 'build.log')
    if ($LASTEXITCODE -ne 0) { throw "Resource registry build failed; see $build/build.log" }
    $exe=Join-Path $build 'test_remix_resource_registry.exe'
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
    if ($exitCode -ne 0) { throw "Resource registry fixture failed ($exitCode); see $build/stderr.log" }
    $result=Get-Content -LiteralPath (Join-Path $build 'result.json') -Raw | ConvertFrom-Json
    if ($result.status -ne 'PASS') { throw 'Resource registry fixture did not report PASS' }
    $inputs=@(Get-ChildItem -LiteralPath $snapshot -File -Recurse | ForEach-Object {
        @{file=$_.FullName.Substring($snapshot.Length+1).Replace('\','/');sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
    })
    @{schema=1;platform=$Platform;gpu=$false;gameExecuted=$false;sourceInputs=$inputs;
      exeSha256=(Get-FileHash -LiteralPath $exe).Hash;result=$result} |
        ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $build 'verification.json') -Encoding UTF8
    Get-Content -LiteralPath (Join-Path $build 'result.json')
} finally { Pop-Location }
