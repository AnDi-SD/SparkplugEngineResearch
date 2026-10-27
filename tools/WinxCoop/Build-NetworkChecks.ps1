param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [ValidateSet('x64','x86')][string]$Architecture = 'x64',
    [ValidateRange(1,4)][int]$BuildWorkers = 1,
    [string]$BuildDirectory,
    [switch]$IncludePrototype
)
$ErrorActionPreference = 'Stop'
if ($IncludePrototype -and $Architecture -ne 'x86') { throw 'The original-game prototype requires -Architecture x86.' }
[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false)
$coopRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$coopBuild = if ($BuildDirectory) { [IO.Path]::GetFullPath($BuildDirectory) } else {
    Join-Path $coopRepo "artifacts/WinxCoop/network-checks/$Architecture/$Configuration"
}
$coopVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $coopVswhere)) { throw 'Visual Studio C++ tools are required.' }
$coopInstallation = & $coopVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $coopInstallation) { throw 'No complete Visual Studio C++ workload found.' }
$coopDevCmd = Join-Path $coopInstallation 'Common7/Tools/VsDevCmd.bat'
$coopEnvironment = & $env:ComSpec /d /s /c "`"$coopDevCmd`" -arch=$Architecture -host_arch=x64 >nul && set"
if ($LASTEXITCODE -ne 0) { throw 'Cannot initialize the compiler environment.' }
foreach ($coopLine in $coopEnvironment) {
    $coopSeparator = $coopLine.IndexOf('=')
    if ($coopSeparator -gt 0) { [Environment]::SetEnvironmentVariable($coopLine.Substring(0,$coopSeparator),$coopLine.Substring($coopSeparator+1),'Process') }
}
$coopCmake = Join-Path $coopInstallation 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$coopNinja = Join-Path $coopInstallation 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
$env:VSLANG = '1033'
& $coopCmake -S $PSScriptRoot -B $coopBuild -G Ninja "-DCMAKE_MAKE_PROGRAM=$coopNinja" "-DCMAKE_BUILD_TYPE=$Configuration" '-DBUILD_TESTING=ON' '-DCMAKE_CXX_FLAGS=/utf-8 /EHsc /GR /Zc:__cplusplus /D_WIN32_WINNT=0x0601 /DWINVER=0x0601'
if ($LASTEXITCODE -ne 0) { throw 'WinxCoop network checks configure failed.' }
# Same strict localized showIncludes repair as the canonical native builder.
$coopRules = Join-Path $coopBuild 'CMakeFiles/rules.ninja'
$coopPrefixLine = Get-Content -LiteralPath $coopRules -Encoding utf8 | Where-Object { $_.StartsWith('msvc_deps_prefix = ') } | Select-Object -First 1
if ($coopPrefixLine) {
    $coopPrefix = $coopPrefixLine.Substring('msvc_deps_prefix = '.Length)
    $coopOem = [Text.Encoding]::GetEncoding([Globalization.CultureInfo]::CurrentCulture.TextInfo.OEMCodePage)
    $coopUtf8 = New-Object Text.UTF8Encoding($false,$true)
    try { $coopRepaired = $coopUtf8.GetString($coopOem.GetBytes($coopPrefix)) } catch { $coopRepaired = $coopPrefix }
    if ($coopRepaired -ne $coopPrefix -and $coopRepaired.IndexOf([char]0xFFFD) -lt 0) {
        & $coopCmake -S $PSScriptRoot -B $coopBuild "-DWINXCOOP_SHOWINCLUDES_PREFIX:STRING=$coopRepaired"
        if ($LASTEXITCODE -ne 0) { throw 'MSVC dependency-prefix repair failed.' }
    }
}
$coopTargets = @('WinxCoopNetworkProbe','WinxCoopTcpFramerTests','WinxCoopSessionTests','WinxCoopGameplayTests','SparkplugNetworkManagerTests','SparkplugNetworkFlowTests')
if ($IncludePrototype) { $coopTargets += @('WinxCoopLauncher','WinxCoopNativeAdapter','WinxCoopAdapterProbe') }
& $coopCmake --build $coopBuild --target $coopTargets --parallel $BuildWorkers
if ($LASTEXITCODE -ne 0) { throw 'WinxCoop network checks build failed.' }
& (Join-Path (Split-Path $coopCmake) 'ctest.exe') --test-dir $coopBuild --output-on-failure -j 1 -R '^(WinxCoop(TcpFramer|Session|Gameplay)Tests|SparkplugNetwork(Manager|Flow)Tests)$'
if ($LASTEXITCODE -ne 0) { throw 'WinxCoop network checks failed.' }
Write-Output "WinxCoop network contracts PASS: $coopBuild"
