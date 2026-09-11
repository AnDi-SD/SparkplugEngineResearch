param([ValidateSet('Debug','Release')][string]$Configuration = 'Release', [switch]$Fresh, [switch]$RunChecks,
    [string]$VisualStudioPath,
    [ValidateRange(1,4)][int]$BuildWorkers = 2,
    [ValidateSet('AnimationKey','AnimationRuntime','NodeWorld','TransformInput','SanReader','CollisionCore','MeshBVCore','FullLoader','TextureSerialization','PS2TextureInspection','DataBlockWriter','ReadReference','SaveReference','NodeSerialization','MeshReader','RenderNode','StaticRenderObject','MaterialSerialization','MaterialController','MaterialColor','SkinSerialization','SkinRender','UVFunction','ColorFunction','SpatialSerialization','SceneSerialization','SkyBox','NavigationSerialization','LensFlare','OcclusionTopology','ParticleSerialization','LightSerialization','SimpleBVSerialization','OBBScalar','ReferenceReadTrace','FontSerialization','TextInspection','BorrowedInput','BufferInspection','RendererScene','RendererSubmit','GeometryHelper','FunctionEval','Msvcr71Sort','OcclusionRuntime','ParticleRuntime','ShaderLighting','TextRuntime','LegacyTextureAdapter','RenderTopology')]
    [ValidateNotNullOrEmpty()]
    [string[]]$CheckSuites = @('AnimationKey','AnimationRuntime','NodeWorld','TransformInput','SanReader','CollisionCore','MeshBVCore','FullLoader'))
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false)
$taskRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$taskBuild = Join-Path $taskRepo "artifacts/native/viewer/$Configuration"
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio C++ tools are required.' }
$installation = if ($VisualStudioPath) { [IO.Path]::GetFullPath($VisualStudioPath) } else {
    & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}
if (-not $installation) { throw 'No complete C++ workload found. If VS is pending restart but its compiler is available, pass -VisualStudioPath explicitly.' }
$devCmd = Join-Path $installation 'Common7/Tools/VsDevCmd.bat'
if (-not (Test-Path -LiteralPath $devCmd -PathType Leaf)) { throw 'The selected Visual Studio toolchain has no VsDevCmd.bat.' }
# Only build-tool paths enter cmd; environment output is consumed, never logged.
$environmentLines = & $env:ComSpec /d /s /c "`"$devCmd`" -arch=x64 -host_arch=x64 >nul && set"
if ($LASTEXITCODE -ne 0) { throw 'Cannot initialize the x64 compiler environment.' }
foreach ($line in $environmentLines) {
    $separator = $line.IndexOf('=')
    if ($separator -gt 0) { [Environment]::SetEnvironmentVariable($line.Substring(0,$separator),$line.Substring($separator+1),'Process') }
}
$cmake = Join-Path $installation 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$ninja = Join-Path $installation 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
$env:VSLANG = '1033'
$configureArguments = @('-S', $PSScriptRoot, '-B', $taskBuild, '-G', 'Ninja', "-DCMAKE_MAKE_PROGRAM=$ninja", "-DCMAKE_BUILD_TYPE=$Configuration", '-DCMAKE_CXX_FLAGS=/utf-8 /Zc:__cplusplus')
if ($RunChecks) { $configureArguments += '-DSPV_BUILD_CHECKS=ON' }
if ($Fresh) { $configureArguments += '--fresh' }
& $cmake @configureArguments
if ($LASTEXITCODE -ne 0) { throw 'Sparkplug Viewer CMake configure failed.' }
# CMake can misdecode UTF-8 MSVC diagnostics with the host OEM code page when
# English compiler resources are absent. Without this repair Ninja records no
# header dependencies. Strict round-trip decoding leaves ordinary prefixes alone.
$rulesPath = Join-Path $taskBuild 'CMakeFiles/rules.ninja'
$prefixLine = Get-Content -LiteralPath $rulesPath -Encoding UTF8 | Where-Object { $_.StartsWith('msvc_deps_prefix = ') } | Select-Object -First 1
if ($prefixLine) {
    $prefix = $prefixLine.Substring('msvc_deps_prefix = '.Length)
    $oem = [Text.Encoding]::GetEncoding([Globalization.CultureInfo]::CurrentCulture.TextInfo.OEMCodePage)
    $strictUtf8 = New-Object Text.UTF8Encoding($false,$true)
    try { $repaired = $strictUtf8.GetString($oem.GetBytes($prefix)) } catch { $repaired = $prefix }
    if ($repaired -ne $prefix -and $repaired.IndexOf([char]0xFFFD) -lt 0) {
        & $cmake -S $PSScriptRoot -B $taskBuild "-DSPV_SHOWINCLUDES_PREFIX:STRING=$repaired"
        if ($LASTEXITCODE -ne 0) { throw 'MSVC dependency-prefix configuration failed.' }
    }
}
& $cmake --build $taskBuild --target SparkplugViewerNative --parallel $BuildWorkers
if ($LASTEXITCODE -ne 0) { throw 'Sparkplug Viewer native build failed.' }
if ($RunChecks) {
    $taskCheckTargets = @($CheckSuites | ForEach-Object { "Viewer${_}Checks" })
    & $cmake --build $taskBuild --target @taskCheckTargets --parallel $BuildWorkers
    if ($LASTEXITCODE -ne 0) { throw 'Viewing-core checks did not build.' }
    $taskCheckPattern = '^Viewer(' + ($CheckSuites -join '|') + ')Checks$'
    & (Join-Path (Split-Path $cmake) 'ctest.exe') --test-dir $taskBuild --output-on-failure -R $taskCheckPattern -j 1
    if ($LASTEXITCODE -ne 0) { throw 'Viewing-core checks failed.' }
}
Write-Output (Join-Path $taskBuild 'SparkplugViewerNative.dll')
