param([string]$OutputDirectory = 'local-data/rtx-remix/build')
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
$commands = @"
@echo off
call "$environmentScript" x86
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MT /O2 /W4 /LD "$source" /link /DEF:"$exports" /OUT:d3d9.dll /MACHINE:X86 user32.lib
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
