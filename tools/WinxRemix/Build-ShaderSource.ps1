param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$build=Join-Path $root "local-data/rtx-remix/shader-source-build/$Name"
if(Test-Path -LiteralPath $build){throw 'Use a fresh source build name'}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $installation){throw 'MSVC tools not found'}
$environmentScript=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$sources=@('tools/WinxRemix/shader_source.cpp','Sparkplug/Code/SparkplugPC/spPCShaderSource.h')
$hashes=@()
foreach($relative in $sources){
  $target=Join-Path (Join-Path $build 'source') $relative
  New-Item -ItemType Directory -Path (Split-Path $target) -Force | Out-Null
  Copy-Item -LiteralPath (Join-Path $root $relative) -Destination $target
  $hashes+=@{path=$relative;sha256=(Get-FileHash -LiteralPath $target).Hash}
}
$source=Join-Path $build 'source/tools/WinxRemix/shader_source.cpp'
$commands=@"
@echo off
call "$environmentScript" x64
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MT /O2 /W4 "$source" /Fe:shader_source.exe
exit /b %errorlevel%
"@
$command=Join-Path $build 'build.cmd'
[IO.File]::WriteAllText($command,$commands,[Text.Encoding]::ASCII)
Copy-Item -LiteralPath $PSCommandPath -Destination $build
Push-Location $build
try{
  & $env:ComSpec /d /c $command *> (Join-Path $build 'build.log')
  if($LASTEXITCODE -ne 0){throw "Shader source build failed: $build/build.log"}
  @{sources=$hashes;exeSha256=(Get-FileHash -LiteralPath (Join-Path $build 'shader_source.exe')).Hash} |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $build 'build.json') -Encoding UTF8
  Write-Output (Join-Path $build 'shader_source.exe')
}finally{Pop-Location}
