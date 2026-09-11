param([string]$OutputDirectory = 'local-data/rtx-remix/build', [ValidateSet('x86','x64')][string]$Platform = 'x86')
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$build = [IO.Path]::GetFullPath((Join-Path $root $OutputDirectory))
if (-not $build.StartsWith($root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Build output must be inside the repository'
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'MSVC x86 tools not found' }
$environmentScript = Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
New-Item -ItemType Directory -Path $build -Force | Out-Null
$source = Join-Path $PSScriptRoot 'winx_d3d9_probe.cpp'
$exports = Join-Path $PSScriptRoot 'winx_d3d9_probe.def'
if ($Platform -eq 'x64') {
    # x64 uses undecorated C exports; Regex replacement must preserve function names.
    $exportText = [IO.File]::ReadAllText($exports) -replace '=_([A-Za-z0-9]+)@[0-9]+','=$1'
    $exports = Join-Path $build 'probe-x64.def'
    [IO.File]::WriteAllText($exports,$exportText,[Text.Encoding]::ASCII)
}
$commands = @"
@echo off
call "$environmentScript" $Platform
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MT /O2 /W4 /LD "$source" /link /DEF:"$exports" /OUT:d3d9.dll /MACHINE:$Platform user32.lib
exit /b %errorlevel%
"@
$commandFile = Join-Path $build 'build-probe.cmd'
[IO.File]::WriteAllText($commandFile, $commands, [Text.Encoding]::ASCII)
Push-Location $build
try {
    & $env:ComSpec /d /c $commandFile
    if ($LASTEXITCODE -ne 0) { throw "Probe compilation failed: $LASTEXITCODE" }
} finally { Pop-Location }
Get-FileHash -LiteralPath (Join-Path $build 'd3d9.dll') -Algorithm SHA256
