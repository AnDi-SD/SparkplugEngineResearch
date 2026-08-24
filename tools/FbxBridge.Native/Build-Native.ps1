param(
    [string]$FbxSdkRoot = 'C:\Program Files\Autodesk\FBX\FBX SDK\2020.3.10',
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$sourceRoot = $PSScriptRoot
$buildRoot = Join-Path $sourceRoot 'build'
$cmakeCandidates = @(
    (Get-Command cmake.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -ErrorAction SilentlyContinue),
    'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe',
    'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe',
    'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
) | Where-Object { $_ -and (Test-Path -LiteralPath $_) }

if (@($cmakeCandidates).Count -eq 0) {
    throw 'CMake was not found. Install the Desktop development with C++ workload.'
}
if (-not (Test-Path -LiteralPath (Join-Path $FbxSdkRoot 'include\fbxsdk.h'))) {
    throw "Autodesk FBX SDK was not found at '$FbxSdkRoot'."
}

$cmake = @($cmakeCandidates)[0]
& $cmake -S $sourceRoot -B $buildRoot -A x64 "-DFBX_SDK_ROOT=$FbxSdkRoot"
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with exit code $LASTEXITCODE." }
& $cmake --build $buildRoot --config $Configuration
if ($LASTEXITCODE -ne 0) { throw "Native FBX bridge build failed with exit code $LASTEXITCODE." }

$output = Join-Path $buildRoot "bin\$Configuration\SmoFbxBridge.exe"
Write-Output $output
