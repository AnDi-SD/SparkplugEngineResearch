param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,
      [Parameter(Mandatory=$true)][string]$Manifest, [switch]$SignedWeights)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$tool=Join-Path $root 'tools/WinxRemix'
$bridge=Join-Path $root 'local-data/rtx-remix/direct-camera-bridge-work'
$output=Join-Path $root "local-data/rtx-remix/skinning-bridge/$Name"
$manifestPath=[IO.Path]::GetFullPath($Manifest)
if (Test-Path -LiteralPath $output) { throw 'Fresh result directory required' }
if (-not (Test-Path -LiteralPath $manifestPath) -or (Get-Item -LiteralPath $manifestPath).Length -gt 128KB) { throw 'Bounded manifest required' }
if ((& git -C $bridge rev-parse HEAD) -ne 'b81a7b566b1eeb9edb4dc2b3c9d3972e0f253ad4') { throw 'Unexpected bridge base revision' }
$dependencyInclude=& (Join-Path $tool 'Prepare-Dependencies.ps1') -Offline
$snapshot=Join-Path $output 'source'
$snapshotTools=Join-Path $snapshot 'tools/WinxRemix'
New-Item -ItemType Directory -Path $snapshotTools -Force | Out-Null
Get-ChildItem -LiteralPath $tool -File | Where-Object {$_.Extension -in '.h','.cpp'} | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination $snapshotTools}
Copy-Item -LiteralPath (Join-Path $dependencyInclude 'third-party') -Destination $snapshotTools -Recurse
foreach($relative in @('Sparkplug/Analysis/PC/SparkplugAbi.h','Sparkplug/Analysis/PC/SparkBaseAbi.h','Sparkplug/Analysis/PC/spNodeTransformMath.h','Sparkplug/Analysis/PC/spFixedShaderSkinning.h','Sparkplug/Analysis/PC/spRenderNodeMath.h','Sparkplug/Analysis/PC/spColorMath.h','Sparkplug/Code/SparkplugPC/spPCVertexDeclarationElements.h','Sparkplug/Code/SparkplugDX/spPCTextureStateMapping.h','Sparkplug/Code/SparkplugDX/spPCMaterialStateMapping.h','Sparkplug/Code/SparkplugDX/spPCLightPayload.h','Sparkplug/Code/SparkplugDX/spPCDXVertexBytes.h')) {
 $target=Join-Path $snapshot $relative
 New-Item -ItemType Directory -Path (Split-Path $target) -Force | Out-Null
 Copy-Item -LiteralPath (Join-Path $root $relative) -Destination $target
}
$utility=Join-Path $snapshot 'bridge-util'
New-Item -ItemType Directory -Path $utility | Out-Null
foreach($file in @('util_remixapi.cpp','util_remixapi.h','util_common.h','util_serializable.h')) {
 Copy-Item -LiteralPath (Join-Path $bridge "bridge/src/util/$file") -Destination $utility
}
Copy-Item -LiteralPath (Join-Path $bridge 'public/include') -Destination (Join-Path $snapshot 'include') -Recurse
$hashes=Get-ChildItem -LiteralPath $snapshot -File -Recurse | ForEach-Object {@{path=$_.FullName.Substring($snapshot.Length+1);sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}}
$hashes | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $output 'sources.json') -Encoding UTF8
Copy-Item -LiteralPath $manifestPath -Destination (Join-Path $output 'packets.txt')
Copy-Item -LiteralPath $PSCommandPath -Destination $output
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $installation){throw 'MSVC unavailable'}
$vcvars=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$source=Join-Path $snapshotTools 'test_skin_packet_submit.cpp'
$utilSource=Join-Path $utility 'util_remixapi.cpp'
$include=Join-Path $snapshot 'include'
foreach($platform in @('x86','x64')) {
 $build=Join-Path $output $platform
 New-Item -ItemType Directory -Path $build | Out-Null
 $commands=@"
@echo off
call "$vcvars" $platform
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MT /O2 /W3 /DNOMINMAX /DWINX_SKIN_PACKET_WIRE_TEST /I"$include" /I"$utility" "$source" "$utilSource" /Fe:test_skin_packet_wire.exe /link user32.lib
exit /b %errorlevel%
"@
 $command=Join-Path $build 'build.cmd'
 [IO.File]::WriteAllText($command,$commands,[Text.Encoding]::ASCII)
 Push-Location $build
 try {& $env:ComSpec /d /c $command *> (Join-Path $build 'build.log')
  if($LASTEXITCODE -ne 0){throw "Wire fixture build failed: $build/build.log"}
 } finally {Pop-Location}
}
$results=@()
foreach($writer in @('x86','x64')) {
 $reader=if($writer -eq 'x86'){'x64'}else{'x86'}
 $wire=Join-Path $output "$writer.bin"
 foreach($step in @(@($writer,'write'),@($reader,'read'))) {
  $platform,$operation=$step
  $process=New-Object Diagnostics.Process
  $process.StartInfo.FileName=Join-Path $output "$platform/test_skin_packet_wire.exe"
  $process.StartInfo.WorkingDirectory=$root
  $mode=if($SignedWeights){"signed-$operation"}else{$operation}
  $process.StartInfo.Arguments='--wire-{0} "{1}" "{2}"' -f $mode,(Join-Path $output 'packets.txt'),$wire
  $process.StartInfo.UseShellExecute=$false;$process.StartInfo.CreateNoWindow=$true
  $process.StartInfo.RedirectStandardOutput=$true;$process.StartInfo.RedirectStandardError=$true
  if(-not $process.Start()){throw 'Owned wire fixture did not start'}
  $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
  $peak=0L;$limited=$false;$timer=[Diagnostics.Stopwatch]::StartNew()
  while(-not $process.WaitForExit(25)) {
   $process.Refresh();$own=Get-Process -Id $PID
   $peak=[Math]::Max($peak,[Math]::Max($own.WorkingSet64+$process.WorkingSet64,$own.PrivateMemorySize64+$process.PrivateMemorySize64))
   if($peak -gt 900MB -or $timer.Elapsed.TotalSeconds -gt 35){$limited=$true;Stop-Process -InputObject $process -Force;$process.WaitForExit();break}
  }
  $record=@{writer=$writer;platform=$platform;operation=$operation;exitCode=$process.ExitCode;limited=$limited;peakParentAndChildBytes=$peak;seconds=$timer.Elapsed.TotalSeconds;
    executableSha256=(Get-FileHash -LiteralPath $process.StartInfo.FileName).Hash;stdout=$stdout.GetAwaiter().GetResult();stderr=$stderr.GetAwaiter().GetResult()}
  $process.Dispose();$results+=$record
  $record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output "$writer-$platform-$operation.json") -Encoding UTF8
  if($record.exitCode -ne 0 -or $limited){throw 'Wire fixture failed; preserved execution record'}
 }
}
$x86=(Get-FileHash -LiteralPath (Join-Path $output 'x86.bin')).Hash
$x64=(Get-FileHash -LiteralPath (Join-Path $output 'x64.bin')).Hash
if($x86 -ne $x64){throw 'Cross-architecture bytes differ'}
$report=@{status='PASS';signedWeights=[bool]$SignedWeights;scope='Shared API consumer and real Mesh/Instance/Blend serializers and owning decoders; signed mode also covers two palettes through BoneTransforms. File exchange, no IPC, renderer, GPU or native game execution.';
 identicalWire=$true;wireSha256=$x86;wireBytes=(Get-Item -LiteralPath (Join-Path $output 'x86.bin')).Length;results=$results}
$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $output 'report.json') -Encoding UTF8
[pscustomobject]$report | Select-Object status,identicalWire,wireSha256,wireBytes | ConvertTo-Json
