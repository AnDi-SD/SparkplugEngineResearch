param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$build=Join-Path $root "local-data/rtx-remix/native-light-source-tests/$Name"
if(Test-Path -LiteralPath $build){throw 'Use a fresh evidence directory'}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $installation){throw 'MSVC x86 tools not found'}
$vcvars=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$snapshot=Join-Path $build 'source/research/rtx-remix'
New-Item -ItemType Directory -Path $snapshot -Force | Out-Null
Get-ChildItem -LiteralPath $PSScriptRoot -File | Where-Object { $_.Extension -in '.h','.cpp' } | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $snapshot }
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'third-party') -Destination $snapshot -Recurse
foreach($relative in @('Sparkplug/Analysis/PC/SparkplugAbi.h','Sparkplug/Analysis/PC/SparkBaseAbi.h','Sparkplug/Analysis/PC/spNodeTransformMath.h','Sparkplug/Analysis/PC/spRenderNodeMath.h','Sparkplug/Analysis/PC/spColorMath.h','Sparkplug/Code/SparkplugPC/spPCVertexDeclarationElements.h','Sparkplug/Code/SparkplugDX/spPCTextureStateMapping.h','Sparkplug/Code/SparkplugDX/spPCMaterialStateMapping.h','Sparkplug/Code/SparkplugDX/spPCLightPayload.h','Sparkplug/Code/SparkplugDX/spPCDXVertexBytes.h')) {
  $target=Join-Path (Join-Path $build 'source') $relative
  New-Item -ItemType Directory -Path (Split-Path $target) -Force | Out-Null
  Copy-Item -LiteralPath (Join-Path $root $relative) -Destination $target
}
Copy-Item -LiteralPath $PSCommandPath -Destination $build
$apiInclude=Join-Path $root 'local-data/rtx-remix/upstream/dxvk-remix/public/include'
$commands=@"
@echo off
call "$vcvars" x86
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MT /O2 /W4 /I"$apiInclude" "$snapshot\test_native_light_source.cpp" /Fe:test_native_light_source.exe /link user32.lib
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MT /O2 /W4 /I"$apiInclude" "$snapshot\test_scene_lights.cpp" /Fe:test_scene_lights.exe /link user32.lib
exit /b %errorlevel%
"@
[IO.File]::WriteAllText((Join-Path $build 'build.cmd'),$commands,[Text.Encoding]::ASCII)
Push-Location $build
try {
  & $env:ComSpec /d /c (Join-Path $build 'build.cmd') *> (Join-Path $build 'build.log')
  if($LASTEXITCODE -ne 0){throw "Build failed; preserved $build/build.log"}
  foreach($fixture in @(@{name='test_native_light_source';result='result.json'},@{name='test_scene_lights';result='legacy-result.txt'})) {
  $process=New-Object System.Diagnostics.Process
  $process.StartInfo.FileName=Join-Path $build ($fixture.name+'.exe')
  $process.StartInfo.WorkingDirectory=$build
  $process.StartInfo.UseShellExecute=$false
  $process.StartInfo.CreateNoWindow=$true
  $process.StartInfo.RedirectStandardOutput=$true
  $process.StartInfo.RedirectStandardError=$true
  if(-not $process.Start()){throw 'CPU fixture did not start'}
  $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
  if(-not $process.WaitForExit(30000)) { $process.Kill();$process.WaitForExit();throw 'CPU fixture exceeded 30 seconds' }
  $exitCode=$process.ExitCode
  [IO.File]::WriteAllText((Join-Path $build $fixture.result),$stdout.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
  [IO.File]::WriteAllText((Join-Path $build ($fixture.name+'-stderr.txt')),$stderr.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
  if($exitCode -ne 0){throw "CPU fixture failed: $exitCode; see $build/$($fixture.name)-stderr.txt"}
  }
  $files=Get-ChildItem -LiteralPath $build -File -Recurse | ForEach-Object {
    @{path=$_.FullName.Substring($build.Length+1).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
  }
  @{schema=1;cpuOnly=$true;gameLaunched=$false;files=@($files)} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $build 'evidence.json') -Encoding UTF8
  Get-Content -LiteralPath (Join-Path $build 'result.json')
  Get-Content -LiteralPath (Join-Path $build 'legacy-result.txt')
} finally { Pop-Location }
