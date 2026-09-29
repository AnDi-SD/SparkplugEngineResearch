param(
    [string]$FbxSdkRoot = 'C:\Program Files\Autodesk\FBX\FBX SDK\2020.3.10',
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$sourceRoot = $PSScriptRoot
$buildRoot = Join-Path $sourceRoot 'build'
$cmakeCandidates = @(
    (Get-Command cmake.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -ErrorAction SilentlyContinue)
)

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
    $installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.CMake.Project -property installationPath
    if ($installation) {
        $cmakeCandidates += (Join-Path $installation 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe')
    }
}
$cmakeCandidates = @($cmakeCandidates | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) })

if (@($cmakeCandidates).Count -eq 0) {
    throw 'CMake was not found. Install the Desktop development with C++ workload.'
}
if (-not (Test-Path -LiteralPath (Join-Path $FbxSdkRoot 'include\fbxsdk.h'))) {
    throw "Autodesk FBX SDK was not found at '$FbxSdkRoot'."
}

$cmake = @($cmakeCandidates)[0]
$configureArguments = @('-S', $sourceRoot, '-B', $buildRoot, '-A', 'x64', "-DFBX_SDK_ROOT=$FbxSdkRoot")
if ($installation) {
    $configureArguments += "-DCMAKE_GENERATOR_INSTANCE=$installation"
    $cachePath = Join-Path $buildRoot 'CMakeCache.txt'
    if (Test-Path -LiteralPath $cachePath -PathType Leaf) {
        $cacheLine = Get-Content -LiteralPath $cachePath | Where-Object {
            $_ -match '^CMAKE_GENERATOR_INSTANCE:(UNINITIALIZED|INTERNAL)=.+'
        } | Select-Object -First 1
        if ($cacheLine) {
            $cachedInstallation = $cacheLine.Substring($cacheLine.IndexOf('=') + 1)
            if ($cachedInstallation.Replace('\', '/') -ine $installation.Replace('\', '/')) {
                $configureArguments += '--fresh'
            }
        }
    }
}
& $cmake @configureArguments
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with exit code $LASTEXITCODE." }
& $cmake --build $buildRoot --config $Configuration --parallel 2
if ($LASTEXITCODE -ne 0) { throw "Native FBX bridge build failed with exit code $LASTEXITCODE." }

$output = Join-Path $buildRoot "bin\$Configuration\SmoFbxBridge.exe"
Write-Output $output
