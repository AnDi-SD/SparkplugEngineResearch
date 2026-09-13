param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name, [switch]$SkipRun)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$dependencyInclude = & (Join-Path $PSScriptRoot 'Prepare-Dependencies.ps1') -Offline
$build=Join-Path $root "local-data/rtx-remix/native-vertex-capture-tests/$Name"
if (Test-Path -LiteralPath $build) { throw 'Use a fresh evidence directory' }
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'MSVC x86 tools not found' }
$environmentScript=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$include=Join-Path $root 'local-data/rtx-remix/upstream/dxvk-remix/public/include'
$dependencies=@(
    'Sparkplug/Analysis/PC/SparkplugAbi.h',
    'Sparkplug/Analysis/PC/SparkBaseAbi.h',
    'Sparkplug/Analysis/PC/spNodeTransformMath.h',
    'Sparkplug/Analysis/PC/spRenderNodeMath.h',
    'Sparkplug/Analysis/PC/spColorMath.h',
    'Sparkplug/Code/SparkplugPC/spPCVertexDeclarationElements.h',
    'Sparkplug/Code/SparkplugDX/spPCDXVertexBytes.h',
    'Sparkplug/Code/SparkplugDX/spPCTextureStateMapping.h',
    'Sparkplug/Code/SparkplugDX/spPCMaterialStateMapping.h',
    'Sparkplug/Code/SparkplugDX/spPCLightPayload.h'
)
foreach ($relative in $dependencies) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $relative))) { throw "Required frozen source missing: $relative" }
}
$snapshot=Join-Path $build 'source/tools/WinxRemix'
New-Item -ItemType Directory -Path $snapshot -Force | Out-Null
Get-ChildItem -LiteralPath $PSScriptRoot -File | Where-Object { $_.Extension -in '.h','.cpp' } |
    ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $snapshot }
Copy-Item -LiteralPath (Join-Path $dependencyInclude 'third-party') -Destination $snapshot -Recurse
Copy-Item -LiteralPath $PSCommandPath -Destination $snapshot
foreach ($relative in $dependencies) {
    $target=Join-Path (Join-Path $build 'source') $relative
    New-Item -ItemType Directory -Path (Split-Path $target) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $root $relative) -Destination $target
}
$source=Join-Path $snapshot 'test_native_vertex_capture.cpp'
$commands=@"
@echo off
call "$environmentScript" x86
if errorlevel 1 exit /b %errorlevel%
cl /I"$dependencyInclude" /nologo /std:c++17 /EHsc /MT /O2 /W4 /I"$include" "$source" /Fe:test_native_vertex_capture.exe /link user32.lib
exit /b %errorlevel%
"@
$commandFile=Join-Path $build 'build.cmd'
[IO.File]::WriteAllText($commandFile,$commands,[Text.Encoding]::ASCII)
Push-Location $build
try {
    & $env:ComSpec /d /c $commandFile *> (Join-Path $build 'build.log')
    if ($LASTEXITCODE -ne 0) { throw "Native vertex capture build failed; see $build/build.log" }
    if ($SkipRun) { Write-Output "Built CPU native vertex capture fixture: $build"; return }
    $exe=Join-Path $build 'test_native_vertex_capture.exe'
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
    $timedOut=-not $process.WaitForExit(35000)
    if ($timedOut) { Stop-Process -InputObject $process -Force; $process.WaitForExit() }
    $exitCode=$process.ExitCode
    [IO.File]::WriteAllText((Join-Path $build 'result.json'),$stdoutTask.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $build 'stderr.log'),$stderrTask.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
    $process.Dispose()
    if ($timedOut) { throw 'Owned CPU fixture exceeded outer 35-second bound (internal watchdog is 30 seconds)' }
    if ($exitCode -ne 0) { throw "Native vertex capture fixture failed ($exitCode); see $build/stderr.log" }
    $result=Get-Content -LiteralPath (Join-Path $build 'result.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($result.status -ne 'PASS' -or $result.gpu -ne $false -or $result.nativeGameCodeExecuted -ne $false -or $result.comMethodsCalled -ne 0) {
        throw 'Native vertex capture fixture did not report bounded CPU PASS'
    }
    $artifacts=@(Get-ChildItem -LiteralPath (Join-Path $build 'source') -File -Recurse | ForEach-Object {
        @{ path=$_.FullName.Substring($build.Length+1).Replace('\','/'); bytes=$_.Length; sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
    })
    @{ schema=1; gpu=$false; nativeGameCodeExecuted=$false; watchdogSeconds=30; outerTimeoutSeconds=35;
       exeSha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash; result=$result; sourceArtifacts=$artifacts } |
        ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $build 'verification.json') -Encoding UTF8
    Get-Content -LiteralPath (Join-Path $build 'result.json') -Encoding UTF8
} finally { Pop-Location }
