param([string]$OutputDirectory='local-data/results/pc-shader-oracle-build-20260912')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$build=[IO.Path]::GetFullPath((Join-Path $root $OutputDirectory))
if (-not $build.StartsWith((Join-Path $root 'local-data')+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Oracle artifacts must remain in local-data' }
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'MSVC tools not found' }
$devCmd=Join-Path $installation 'Common7/Tools/VsDevCmd.bat'
$environmentLines=& $env:ComSpec /d /s /c "`"$devCmd`" -arch=x64 -host_arch=x64 >nul && set"
if ($LASTEXITCODE -ne 0) { throw 'Compiler environment failed' }
foreach ($line in $environmentLines) {
    $separator=$line.IndexOf('=')
    if ($separator -gt 0) { [Environment]::SetEnvironmentVariable($line.Substring(0,$separator),$line.Substring($separator+1),'Process') }
}
New-Item -ItemType Directory -Path $build -Force | Out-Null
$source=Join-Path $PSScriptRoot 'pc_shader_vertex_oracle.cpp'
Copy-Item -LiteralPath $source -Destination (Join-Path $build 'pc_shader_vertex_oracle.cpp')
Push-Location $build
try {
    & cl /nologo /std:c++17 /EHsc /MT /O2 /W4 pc_shader_vertex_oracle.cpp /Fe:pc_shader_vertex_oracle.exe /link user32.lib
    if ($LASTEXITCODE -ne 0) { throw 'Shader oracle compilation failed' }
} finally { Pop-Location }
Get-FileHash -LiteralPath (Join-Path $build 'pc_shader_vertex_oracle.exe') -Algorithm SHA256
