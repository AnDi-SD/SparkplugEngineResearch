param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,
      [Parameter(Mandatory=$true)][string]$Catalog,[ValidateSet('x86','x64')][string]$Platform='x86')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$build=Join-Path $root "local-data/rtx-remix/skin-shader-tests/$Name"
if(Test-Path -LiteralPath $build){throw 'Use a fresh test name'}
$catalogPath=[IO.Path]::GetFullPath((Join-Path $root $Catalog))
if((Get-Item -LiteralPath $catalogPath).Length -gt 1MB){throw 'Catalog exceeds bound'}
New-Item -ItemType Directory -Path $build | Out-Null
Copy-Item -LiteralPath $catalogPath -Destination (Join-Path $build 'contracts.wsf')
Copy-Item -LiteralPath $PSCommandPath -Destination $build
foreach($fixtureFile in @('test_skin_shader_contract.cpp','winx_skin_shader_contract.h')){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $fixtureFile) -Destination $build}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $installation){throw 'MSVC tools not found'}
$environmentScript=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$commands=@"
@echo off
call "$environmentScript" $Platform
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MT /O2 /W4 test_skin_shader_contract.cpp /Fe:test_skin_shader_contract.exe
if errorlevel 1 exit /b %errorlevel%
test_skin_shader_contract.exe contracts.wsf > result.json 2> stderr.log
exit /b %errorlevel%
"@
$command=Join-Path $build 'build.cmd';[IO.File]::WriteAllText($command,$commands,[Text.Encoding]::ASCII)
Push-Location $build
try{
  & $env:ComSpec /d /c $command *> (Join-Path $build 'build.log')
  if($LASTEXITCODE -ne 0){throw "Skin shader contract check failed: $build"}
  $hashes=Get-ChildItem -LiteralPath $build -File | Where-Object {$_.Extension -in '.h','.cpp','.exe','.wsf'} | Get-FileHash
  @{platform=$Platform;inputs=$hashes;result=(Get-Content -LiteralPath (Join-Path $build 'result.json') -Raw | ConvertFrom-Json)} |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $build 'verification.json') -Encoding UTF8
  Get-Content -LiteralPath (Join-Path $build 'result.json')
}finally{Pop-Location}
