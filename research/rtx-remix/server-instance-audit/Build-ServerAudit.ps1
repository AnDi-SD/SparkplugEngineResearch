param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$source=Join-Path $root 'local-data/rtx-remix/direct-camera-bridge-work'
$build=Join-Path $root 'local-data/rtx-remix/direct-camera-bridge-build/x64'
$output=Join-Path $root "local-data/rtx-remix/server-instance-audit/$Name"
$tools=Join-Path $root 'local-data/rtx-remix/direct-camera-build-tools'
$oldExe=Join-Path $build 'src/server/NvRemixBridge.exe'
$client=Join-Path $root 'local-data/rtx-remix/direct-camera-bridge-build/x86/src/client/d3d9.dll'
if(Test-Path -LiteralPath $output){throw 'Use a fresh build evidence directory'}
if((& git -C $source rev-parse HEAD) -ne 'b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4'){throw 'Unexpected source base'}
if(-not (Test-Path -LiteralPath (Join-Path $build 'build.ninja'))){throw 'Expected configured x64 bridge build is missing'}
New-Item -ItemType Directory -Path (Join-Path $output 'before'),(Join-Path $output 'source/bridge/src/server') -Force | Out-Null
Copy-Item -LiteralPath $oldExe -Destination (Join-Path $output 'before/NvRemixBridge.exe')
Copy-Item -LiteralPath $PSCommandPath -Destination $output
$clientBefore=(Get-FileHash -LiteralPath $client).Hash
$relativeFiles=@(& git -C $source diff --name-only)+@('bridge/src/server/instance_audit.h')
$sourceHashes=@()
foreach($relative in $relativeFiles | Sort-Object -Unique) {
  $file=Join-Path $source $relative;$destination=Join-Path (Join-Path $output 'source') $relative
  New-Item -ItemType Directory -Path (Split-Path $destination) -Force | Out-Null
  Copy-Item -LiteralPath $file -Destination $destination
  $sourceHashes+=@{path=$relative;sha256=(Get-FileHash -LiteralPath $file).Hash}
}
Copy-Item -LiteralPath (Join-Path $build 'version.h') -Destination $output
$ninjaBefore=(Get-FileHash -LiteralPath (Join-Path $build 'build.ninja')).Hash
$compileCommands=Get-Content -LiteralPath (Join-Path $build 'compile_commands.json') -Raw | ConvertFrom-Json
@($compileCommands | Where-Object {$_.file -match 'server[/\\]main\.cpp$'}) | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $output 'main-compile-command.json') -Encoding UTF8
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $installation){throw 'MSVC tools not found'}
$vcvars=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$commands=@"
@echo off
call "$vcvars" x64
if errorlevel 1 exit /b %errorlevel%
set "PYTHONPATH=$tools"
set "PATH=$tools\bin;%PATH%"
"$tools\bin\ninja.exe" -C "$build" -j 1 src/server/NvRemixBridge.exe
exit /b %errorlevel%
"@
[IO.File]::WriteAllText((Join-Path $output 'build.cmd'),$commands,[Text.Encoding]::ASCII)
& $env:ComSpec /d /c (Join-Path $output 'build.cmd') *> (Join-Path $output 'build.log')
if($LASTEXITCODE -ne 0){throw "x64 server build failed; preserved $output/build.log"}
if((Get-FileHash -LiteralPath $client).Hash -ne $clientBefore){throw 'Unexpected x86 client change'}
foreach($entry in $sourceHashes){if((Get-FileHash -LiteralPath (Join-Path $source $entry.path)).Hash -ne $entry.sha256){throw "Source changed during build: $($entry.path)"}}
Copy-Item -LiteralPath $oldExe -Destination (Join-Path $output 'NvRemixBridge.exe')
$result=@{status='BUILT_NOT_INSTALLED_OR_RUN';architecture='x64';target='src/server/NvRemixBridge.exe';workers=1;
  baseRevision='b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4';serverSha256=(Get-FileHash -LiteralPath (Join-Path $output 'NvRemixBridge.exe')).Hash;
  previousServerSha256=(Get-FileHash -LiteralPath (Join-Path $output 'before/NvRemixBridge.exe')).Hash;x86ClientUnchangedSha256=$clientBefore;
  buildNinjaSha256=$ninjaBefore;sourceHashes=$sourceHashes;gpu=$false;gameExecuted=$false;stockRendererBuilt=$false}
$result | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath (Join-Path $output 'result.json') -Encoding UTF8
$result | ConvertTo-Json -Depth 2
