param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name, [switch]$SkipRun)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$dependencyInclude = & (Join-Path $PSScriptRoot 'Prepare-Dependencies.ps1') -Offline
$build=Join-Path $root "local-data/rtx-remix/selected-submit-tests/$Name"
if (Test-Path -LiteralPath $build) { throw 'Use a fresh evidence directory' }
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'MSVC x86 tools not found' }
$environmentScript=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$include=Join-Path $root 'local-data/rtx-remix/upstream/dxvk-remix/public/include'
$snapshot=Join-Path $build 'source/tools/WinxRemix'
New-Item -ItemType Directory -Path $snapshot -Force | Out-Null
Get-ChildItem -LiteralPath $PSScriptRoot -File | Where-Object { $_.Extension -in '.h','.cpp' } | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $snapshot }
Copy-Item -LiteralPath (Join-Path $dependencyInclude 'third-party') -Destination $snapshot -Recurse
foreach ($relative in @('Sparkplug/Analysis/PC/SparkplugAbi.h','Sparkplug/Analysis/PC/SparkBaseAbi.h','Sparkplug/Analysis/PC/spNodeTransformMath.h','Sparkplug/Analysis/PC/spRenderNodeMath.h','Sparkplug/Analysis/PC/spColorMath.h','Sparkplug/Code/SparkplugPC/spPCVertexDeclarationElements.h','Sparkplug/Code/SparkplugDX/spPCTextureStateMapping.h','Sparkplug/Code/SparkplugDX/spPCMaterialStateMapping.h','Sparkplug/Code/SparkplugDX/spPCLightPayload.h','Sparkplug/Code/SparkplugDX/spPCDXVertexBytes.h')) {
    $target=Join-Path (Join-Path $build 'source') $relative
    New-Item -ItemType Directory -Path (Split-Path $target) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $root $relative) -Destination $target
}
$source=Join-Path $snapshot 'test_selected_submit.cpp'
# Ordinary CPU fixture executable on owned records; no original game functions.
$commands=@"
@echo off
call "$environmentScript" x86
if errorlevel 1 exit /b %errorlevel%
cl /I"$dependencyInclude" /nologo /std:c++17 /EHsc /MT /O2 /W4 /I"$include" "$source" /Fe:test_selected_submit.exe /link user32.lib
exit /b %errorlevel%
"@
$commandFile=Join-Path $build 'build.cmd'
[IO.File]::WriteAllText($commandFile,$commands,[Text.Encoding]::ASCII)
Push-Location $build
try {
    & $env:ComSpec /d /c $commandFile *> (Join-Path $build 'build.log')
    if ($LASTEXITCODE -ne 0) { throw "Selected submit build failed; see $build/build.log" }
    if ($SkipRun) { Write-Output "Built CPU selected submit fixture: $build"; return }
    $exe=Join-Path $build 'test_selected_submit.exe'
    $process=New-Object System.Diagnostics.Process
    $process.StartInfo.FileName=$exe
    $process.StartInfo.WorkingDirectory=$build
    $process.StartInfo.UseShellExecute=$false
    $process.StartInfo.CreateNoWindow=$true
    $process.StartInfo.RedirectStandardOutput=$true
    $process.StartInfo.RedirectStandardError=$true
    if (-not $process.Start()) { throw 'Failed to start owned CPU fixture' }
    $stdoutTask=$process.StandardOutput.ReadToEndAsync()
    $stderrTask=$process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit(35000)) {
        Stop-Process -InputObject $process -Force
        $process.WaitForExit()
        throw 'Owned CPU fixture exceeded outer 35-second bound'
    }
    $exitCode=$process.ExitCode
    [IO.File]::WriteAllText((Join-Path $build 'result.json'),$stdoutTask.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $build 'stderr.log'),$stderrTask.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
    $process.Dispose()
    if ($exitCode -ne 0) { throw "Selected submit fixture failed ($exitCode); see $build/stderr.log" }
    $result=Get-Content -LiteralPath (Join-Path $build 'result.json') -Raw | ConvertFrom-Json
    if ($result.status -ne 'PASS') { throw 'Selected submit fixture did not report PASS' }
    @{ schema=1; gpu=$false; nativeGameCodeExecuted=$false; outerTimeoutSeconds=35; exeSha256=(Get-FileHash -LiteralPath $exe).Hash; result=$result } |
        ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $build 'verification.json') -Encoding UTF8
    Get-Content -LiteralPath (Join-Path $build 'result.json')
    # Regression uses this exact source snapshot, without another working-tree copy.
    $directSource=Join-Path $snapshot 'test_independent_submit.cpp'
    $directCommands=$commands.Replace($source,$directSource).Replace('test_selected_submit.exe','test_independent_submit.exe')
    $directCommandFile=Join-Path $build 'build-direct.cmd'
    [IO.File]::WriteAllText($directCommandFile,$directCommands,[Text.Encoding]::ASCII)
    & $env:ComSpec /d /c $directCommandFile *> (Join-Path $build 'build-direct.log')
    if ($LASTEXITCODE -ne 0) { throw "Direct regression build failed; see $build/build-direct.log" }
    $directExe=Join-Path $build 'test_independent_submit.exe'
    $directProcess=New-Object System.Diagnostics.Process
    $directProcess.StartInfo.FileName=$directExe
    $directProcess.StartInfo.WorkingDirectory=$build
    $directProcess.StartInfo.UseShellExecute=$false
    $directProcess.StartInfo.CreateNoWindow=$true
    $directProcess.StartInfo.RedirectStandardOutput=$true
    $directProcess.StartInfo.RedirectStandardError=$true
    if (-not $directProcess.Start()) { throw 'Failed to start owned CPU direct regression' }
    $directOut=$directProcess.StandardOutput.ReadToEndAsync()
    $directErr=$directProcess.StandardError.ReadToEndAsync()
    if (-not $directProcess.WaitForExit(35000)) {
        Stop-Process -InputObject $directProcess -Force
        $directProcess.WaitForExit()
        throw 'Owned CPU direct regression exceeded outer 35-second bound'
    }
    $directExit=$directProcess.ExitCode
    [IO.File]::WriteAllText((Join-Path $build 'direct-result.json'),$directOut.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $build 'direct-stderr.log'),$directErr.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
    $directProcess.Dispose()
    if ($directExit -ne 0) { throw "Direct regression failed ($directExit); see $build/direct-stderr.log" }
    $directResult=Get-Content -LiteralPath (Join-Path $build 'direct-result.json') -Raw | ConvertFrom-Json
    if ($directResult.status -ne 'PASS') { throw 'Direct regression did not report PASS' }
    @{ schema=1; gpu=$false; nativeGameCodeExecuted=$false; sameSourceSnapshot=$true; outerTimeoutSeconds=35; exeSha256=(Get-FileHash -LiteralPath $directExe).Hash; result=$directResult } |
        ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $build 'direct-verification.json') -Encoding UTF8
    Get-Content -LiteralPath (Join-Path $build 'direct-result.json')
} finally { Pop-Location }
