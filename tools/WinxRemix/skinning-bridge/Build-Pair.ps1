param(
  [Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,
  [Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$TestName
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$source=Join-Path $root 'local-data/rtx-remix/direct-camera-bridge-work'
$evidenceRoot=Join-Path $root 'local-data/rtx-remix/skinning-bridge'
$test=Join-Path $evidenceRoot $TestName
$output=Join-Path $evidenceRoot $Name
$tools=Join-Path $root 'local-data/rtx-remix/direct-camera-build-tools'
$verified=Get-Content -Raw -LiteralPath (Join-Path $test 'result.json') | ConvertFrom-Json
if($verified.status -ne 'PASS' -or -not $verified.identicalWire){throw 'Passing real serializer tests required'}
if(Test-Path -LiteralPath $output){throw 'Use a fresh build directory'}
foreach($entry in $verified.sourceHashes){if((Get-FileHash -LiteralPath (Join-Path $source $entry.path)).Hash -ne $entry.sha256){throw "Source changed after tests: $($entry.path)"}}
New-Item -ItemType Directory -Path $output,(Join-Path $output 'before') -Force | Out-Null
Copy-Item -LiteralPath $PSCommandPath -Destination $output
Copy-Item -LiteralPath (Join-Path $test 'source-hashes.json') -Destination $output
$installed=@('local-data/Winx Club/d3d9.dll','local-data/Winx Club/d3d9.remix-original.dll','local-data/Winx Club/.trex/NvRemixBridge.exe','local-data/Winx Club/.trex/d3d9.dll') | ForEach-Object {@{path=$_;sha256=(Get-FileHash -LiteralPath (Join-Path $root $_)).Hash}}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$binaries=@()
foreach($platform in @('x86','x64')){
  $build=Join-Path $root "local-data/rtx-remix/direct-camera-bridge-build/$platform"
  $target=if($platform -eq 'x86'){'src/client/d3d9.dll'}else{'src/server/NvRemixBridge.exe'}
  $binary=Join-Path $build $target;$fileName=Split-Path $binary -Leaf
  Copy-Item -LiteralPath $binary -Destination (Join-Path (Join-Path $output 'before') $fileName)
  $commands=@"
@echo off
call "$vcvars" $platform
if errorlevel 1 exit /b %errorlevel%
set "PYTHONPATH=$tools"
set "PATH=$tools\bin;%PATH%"
"$tools\bin\ninja.exe" -C "$build" -j 1 $target
exit /b %errorlevel%
"@
  $cmdFile=Join-Path $output "build-$platform.cmd"
  [IO.File]::WriteAllText($cmdFile,$commands,[Text.Encoding]::ASCII)
  & $env:ComSpec /d /c $cmdFile *> (Join-Path $output "build-$platform.log")
  if($LASTEXITCODE -ne 0){throw "Pair build failed: $platform; preserved log and previous binary"}
  Copy-Item -LiteralPath $binary -Destination $output
  Copy-Item -LiteralPath (Join-Path $build 'version.h') -Destination (Join-Path $output "version-$platform.h")
  $binaries+=@{platform=$platform;file=$fileName;sha256=(Get-FileHash -LiteralPath (Join-Path $output $fileName)).Hash;previousSha256=(Get-FileHash -LiteralPath (Join-Path (Join-Path $output 'before') $fileName)).Hash}
}
foreach($entry in $verified.sourceHashes){if((Get-FileHash -LiteralPath (Join-Path $source $entry.path)).Hash -ne $entry.sha256){throw "Source changed during build: $($entry.path)"}}
foreach($entry in $installed){if((Get-FileHash -LiteralPath (Join-Path $root $entry.path)).Hash -ne $entry.sha256){throw "Installed file changed independently during build: $($entry.path)"}}
$report=@{status='BUILT_NOT_INSTALLED_OR_RUN';baseRevision='b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4';wireVersion='remix-main-skinwire-v1';requiresMatchingPair=$true;serializerTest=$TestName;testResultSha256=(Get-FileHash -LiteralPath (Join-Path $test 'result.json')).Hash;binaries=$binaries;installedUnchanged=$installed;sourceHashes=$verified.sourceHashes;gpu=$false;gameExecuted=$false;stockRendererBuilt=$false}
$report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $output 'result.json') -Encoding UTF8
$report | ConvertTo-Json -Depth 3
