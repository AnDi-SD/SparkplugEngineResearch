param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,[switch]$SkipRun)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$dependencyInclude = & (Join-Path $PSScriptRoot 'Prepare-Dependencies.ps1') -Offline
$build=Join-Path $root "local-data/rtx-remix/resource-ownership-tests/$Name"
if(Test-Path -LiteralPath $build){throw 'Use a fresh evidence directory'}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $installation){throw 'MSVC x86 tools not found'}
$environmentScript=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$include=Join-Path $root 'local-data/rtx-remix/upstream/dxvk-remix/public/include'
$snapshot=Join-Path $build 'source/tools/WinxRemix'
New-Item -ItemType Directory -Path $snapshot -Force | Out-Null
Get-ChildItem -LiteralPath $PSScriptRoot -File | Where-Object {$_.Extension -in '.h','.cpp'} | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination $snapshot}
Copy-Item -LiteralPath (Join-Path $dependencyInclude 'third-party') -Destination $snapshot -Recurse
foreach($relative in @('Sparkplug/Analysis/PC/SparkplugAbi.h','Sparkplug/Analysis/PC/SparkBaseAbi.h','Sparkplug/Analysis/PC/spNodeTransformMath.h','Sparkplug/Analysis/PC/spRenderNodeMath.h','Sparkplug/Analysis/PC/spColorMath.h','Sparkplug/Code/SparkplugPC/spPCVertexDeclarationElements.h','Sparkplug/Code/SparkplugDX/spPCTextureStateMapping.h','Sparkplug/Code/SparkplugDX/spPCMaterialStateMapping.h','Sparkplug/Code/SparkplugDX/spPCLightPayload.h','Sparkplug/Code/SparkplugDX/spPCDXVertexBytes.h')) {
  $target=Join-Path (Join-Path $build 'source') $relative
  New-Item -ItemType Directory -Path (Split-Path $target) -Force | Out-Null
  Copy-Item -LiteralPath (Join-Path $root $relative) -Destination $target
}
Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $build 'Test-ResourceOwnership.ps1')
$newSource=Join-Path $snapshot 'test_resource_ownership.cpp'
$pressureSource=Join-Path $snapshot 'test_surface_resources.cpp'
$commands=@"
@echo off
call "$environmentScript" x86
if errorlevel 1 exit /b %errorlevel%
cl /I"$dependencyInclude" /nologo /std:c++17 /EHsc /MT /O2 /W4 /I"$include" "$newSource" /Fe:test_resource_ownership.exe /link user32.lib
if errorlevel 1 exit /b %errorlevel%
cl /I"$dependencyInclude" /nologo /std:c++17 /EHsc /MT /O2 /W4 /I"$include" "$pressureSource" /Fe:test_surface_resources.exe /link user32.lib
exit /b %errorlevel%
"@
$commandFile=Join-Path $build 'build.cmd'
[IO.File]::WriteAllText($commandFile,$commands,[Text.Encoding]::ASCII)
Push-Location $build
try {
  & $env:ComSpec /d /c $commandFile *> (Join-Path $build 'build.log')
  if($LASTEXITCODE -ne 0){throw "Resource ownership build failed; see $build/build.log"}
  if($SkipRun){Write-Output "Built two CPU fixtures: $build";return}
  $results=@()
  foreach($test in @('test_resource_ownership','test_surface_resources')) {
    $exe=Join-Path $build "$test.exe"
    $process=New-Object System.Diagnostics.Process
    $process.StartInfo.FileName=$exe
    $process.StartInfo.WorkingDirectory=$build
    $process.StartInfo.UseShellExecute=$false
    $process.StartInfo.CreateNoWindow=$true
    $process.StartInfo.RedirectStandardOutput=$true
    $process.StartInfo.RedirectStandardError=$true
    if(-not $process.Start()){throw 'Failed to start owned CPU fixture'}
    $stdoutTask=$process.StandardOutput.ReadToEndAsync()
    $stderrTask=$process.StandardError.ReadToEndAsync()
    $timeout=-not $process.WaitForExit(30000)
    if($timeout){$process.Kill();$process.WaitForExit()}
    $exitCode=$process.ExitCode
    [IO.File]::WriteAllText((Join-Path $build "$test-result.json"),$stdoutTask.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $build "$test-stderr.log"),$stderrTask.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
    @{pid=$process.Id;exe=$exe;timeout=$timeout;exitCode=$exitCode} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $build "$test-process.json") -Encoding UTF8
    $process.Dispose()
    if($timeout){throw "Owned CPU fixture exceeded 30 seconds: $test (output preserved)"}
    if($exitCode -ne 0){throw "CPU fixture failed ($exitCode): $test (output preserved)"}
    $result=Get-Content -LiteralPath (Join-Path $build "$test-result.json") -Raw | ConvertFrom-Json
    if($result.status -ne 'PASS'){throw "CPU fixture did not report PASS: $test"}
    $results+=@{test=$test;exeSha256=(Get-FileHash -LiteralPath $exe).Hash;result=$result}
  }
  $hashes=@(Get-ChildItem -LiteralPath (Join-Path $build 'source') -Recurse -File | ForEach-Object {@{path=$_.FullName.Substring($build.Length+1);sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
  @{schema=1;status='PASS';gpu=$false;nativeGameCodeExecuted=$false;eachRunBoundSeconds=30;results=$results;sourceHashes=$hashes} |
    ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $build 'verification.json') -Encoding UTF8
  $results | ForEach-Object {$_.result | ConvertTo-Json -Compress}
} finally {Pop-Location}
