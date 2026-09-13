param(
    [Parameter(Mandatory=$true)][string]$Geometry,
    [Parameter(Mandatory=$true)][string]$Origins,
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$geometryPath=(Resolve-Path -LiteralPath $Geometry).Path
$originPath=(Resolve-Path -LiteralPath $Origins).Path
$build=Join-Path $root "local-data/rtx-remix/light-geometry-tests/$Name"
if (Test-Path -LiteralPath $build) { throw 'Use a fresh evidence directory' }
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'MSVC tools not found' }
$environmentScript=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
New-Item -ItemType Directory -Path $build | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'analyze_light_geometry.cpp') -Destination $build
Copy-Item -LiteralPath $originPath -Destination (Join-Path $build 'origins.txt')
$commands=@"
@echo off
call "$environmentScript" x86
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MT /O2 /W4 analyze_light_geometry.cpp /Fe:analyze_light_geometry.exe
exit /b %errorlevel%
"@
$commandFile=Join-Path $build 'build.cmd'
[IO.File]::WriteAllText($commandFile,$commands,[Text.Encoding]::ASCII)
Push-Location $build
try {
    & $env:ComSpec /d /c $commandFile
    if ($LASTEXITCODE -ne 0) { throw 'Geometry analyzer compilation failed' }
    $result=& (Join-Path $build 'analyze_light_geometry.exe') $geometryPath (Join-Path $build 'origins.txt')
    if ($LASTEXITCODE -ne 0) { throw 'Incomplete or invalid geometry/queries; see stderr' }
    $parsed=$result | ConvertFrom-Json
    [IO.File]::WriteAllText((Join-Path $build 'result.json'),($result -join "`n"),[Text.Encoding]::UTF8)
    @{geometry=$geometryPath;geometrySHA256=(Get-FileHash -LiteralPath $geometryPath).Hash;originsSHA256=(Get-FileHash -LiteralPath $originPath).Hash;sourceSHA256=(Get-FileHash -LiteralPath 'analyze_light_geometry.cpp').Hash;frame=$parsed.frame;draws=$parsed.draws;triangles=$parsed.triangles;origins=$parsed.origins.Count} |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $build 'manifest.json') -Encoding UTF8
    Get-Content -LiteralPath (Join-Path $build 'manifest.json')
} finally { Pop-Location }
