param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$source=Join-Path $root 'local-data/rtx-remix/direct-camera-bridge-work'
$buildRoot=Join-Path $root 'local-data/rtx-remix/direct-camera-bridge-build'
$output=Join-Path $root "local-data/rtx-remix/skinning-bridge/$Name"
$buildTools=Join-Path $root 'local-data/rtx-remix/direct-camera-build-tools'
if(Test-Path -LiteralPath $output){throw 'Use a fresh evidence directory'}
if((& git -C $source rev-parse HEAD) -ne 'b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4'){throw 'Unexpected base revision'}
New-Item -ItemType Directory -Path $output | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'test_skinning.cpp') -Destination (Join-Path $source 'bridge/test/rtx/unit/test_remix_api_skinning.cpp')
$relativeFiles=@(& git -C $source diff HEAD --name-only)+@('bridge/src/server/instance_audit.h','bridge/test/rtx/unit/test_remix_api_skinning.cpp')
$hashes=@()
foreach($relative in $relativeFiles | Sort-Object -Unique){
  $file=Join-Path $source $relative;$destination=Join-Path (Join-Path $output 'source') $relative
  New-Item -ItemType Directory -Path (Split-Path $destination) -Force | Out-Null
  Copy-Item -LiteralPath $file -Destination $destination
  $hashes+=@{path=$relative;sha256=(Get-FileHash -LiteralPath $file).Hash}
}
Copy-Item -LiteralPath $PSCommandPath -Destination $output
$hashes | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $output 'source-hashes.json') -Encoding UTF8
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $installation){throw 'MSVC tools not found'}
$vcvars=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$results=@()
foreach($platform in @('x86','x64')){
  $build=Join-Path $buildRoot $platform
  $commands=@"
@echo off
call "$vcvars" $platform
if errorlevel 1 exit /b %errorlevel%
set "PYTHONPATH=$buildTools"
set "PATH=$buildTools\bin;%PATH%"
python -m mesonbuild.mesonmain setup --reconfigure --backend ninja --buildtype release -Denable_tests=true "$build" "$source\bridge"
if errorlevel 1 exit /b %errorlevel%
python -m mesonbuild.mesonmain compile -C "$build" -j 1 test_remix_api_skinning_$platform test_remix_api_camera_$platform
exit /b %errorlevel%
"@
  $cmdFile=Join-Path $output "build-$platform.cmd"
  [IO.File]::WriteAllText($cmdFile,$commands,[Text.Encoding]::ASCII)
  & $env:ComSpec /d /c $cmdFile *> (Join-Path $output "build-$platform.log")
  if($LASTEXITCODE -ne 0){throw "CPU build failed: $platform; evidence preserved"}
  Copy-Item -LiteralPath (Join-Path $build "test/rtx/unit/test_remix_api_skinning_$platform.exe") -Destination $output
  Copy-Item -LiteralPath (Join-Path $build 'version.h') -Destination (Join-Path $output "version-$platform.h")
}
foreach($writer in @('x86','x64')){
  $reader=if($writer -eq 'x86'){'x64'}else{'x86'}
  $payload=Join-Path $output "$writer.bin"
  foreach($step in @(@($writer,'write'),@($reader,'read'))){
    $platform,$operation=$step
    $exe=Join-Path $output "test_remix_api_skinning_$platform.exe"
    $prefix="$writer-$platform-$operation"
    $stdout=Join-Path $output "$prefix.out.txt";$stderr=Join-Path $output "$prefix.err.txt"
    $p=Start-Process -FilePath $exe -ArgumentList @("--$operation",('"{0}"' -f $payload)) -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    $null=$p.Handle
    if(-not $p.WaitForExit(30000)){$p.Kill();throw "Owned CPU helper timed out: $prefix"}
    $entry=@{platform=$platform;operation=$operation;writer=$writer;exitCode=$p.ExitCode;exeSha256=(Get-FileHash -LiteralPath $exe).Hash;stdout=[IO.File]::ReadAllText($stdout);stderr=[IO.File]::ReadAllText($stderr)}
    $results+=$entry
    if($p.ExitCode -ne 0){throw "CPU helper failed: $prefix; preserved exit/stdout/stderr"}
  }
}
$x86=(Get-FileHash -LiteralPath (Join-Path $output 'x86.bin')).Hash
$x64=(Get-FileHash -LiteralPath (Join-Path $output 'x64.bin')).Hash
if($x86 -ne $x64){throw 'Cross architecture wire differs'}
foreach($entry in $hashes){if((Get-FileHash -LiteralPath (Join-Path $source $entry.path)).Hash -ne $entry.sha256){throw "Source changed during CPU test: $($entry.path)"}}
$report=@{status='PASS';scope='Real util_remixapi.cpp Mesh/Bone serializer; CPU only, no D3D device or IPC';wireVersion='remix-main-skinwire-v1';identicalWire=$true;wireSha256=$x86;wireFileBytes=(Get-Item -LiteralPath (Join-Path $output 'x86.bin')).Length;results=$results;sourceHashes=$hashes;gameExecuted=$false;gpu=$false}
$report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $output 'result.json') -Encoding UTF8
$report | ConvertTo-Json -Depth 3
