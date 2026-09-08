param([string]$OutputDirectory = 'local-data/results/viewer-sparkplug-core-20260908/release-contract')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$outputRoot = [IO.Path]::GetFullPath((Join-Path $repositoryRoot $OutputDirectory))
$allowed = [IO.Path]::GetFullPath((Join-Path $repositoryRoot 'local-data/results')) + '\'
if (-not $outputRoot.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)) { throw 'Fixture output must stay in local-data/results.' }
if (Test-Path -LiteralPath $outputRoot) { throw 'Use a fresh fixture output directory.' }
$stagingRoot = Join-Path $outputRoot 'staging'
New-Item -ItemType Directory -Path $stagingRoot -Force | Out-Null
$tokens=$null; $parseErrors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $repositoryRoot 'release/Build-Releases.ps1'),[ref]$tokens,[ref]$parseErrors)
if ($parseErrors.Count) { throw 'Packaging script syntax errors.' }
foreach ($name in @('Assert-PathUnderRoot','Publish-Application')) {
    $function=$ast.Find({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name},$true)
    if (-not $function) { throw "Missing actual packaging function: $name" }
    Invoke-Expression $function.Extent.Text
}
$nativeFbxFiles=@('SmoFbxBridge.exe','libfbxsdk.dll','FBX_SDK_License.rtf')
$runtimeIdentifier='win-x64'; $PackageSource=''; $publishedApplications=@{}
$script:fakeOutputs=@(); $script:publishes=0; $script:checks=0
function Check([bool]$condition,[string]$message) { $script:checks++; if (-not $condition) { throw $message } }
# Only dotnet compilation is substituted. The actual copy, allowlist and cache
# code executes over small synthetic files; no game/executable is started.
function Invoke-DotNet($arguments) {
    if ($arguments[0] -ne 'publish') { return }
    $script:publishes++
    $destination=$arguments[[Array]::IndexOf($arguments,'-o')+1]
    foreach ($name in $script:fakeOutputs) {
        $path=Join-Path $destination $name
        New-Item -ItemType Directory -Path (Split-Path $path) -Force | Out-Null
        [IO.File]::WriteAllText($path,"fixture:$name")
    }
}
function App([string]$executable,$companions) {
    return [pscustomobject]@{id='fixture';project='tools/SmoViewer/SmoViewer/SmoViewer.csproj';executable=$executable;companionFiles=@($companions)}
}
$script:fakeOutputs=@('Legacy.exe')
Publish-Application (App 'Legacy.exe' @()) (Join-Path $outputRoot 'legacy')
Check (Test-Path -LiteralPath (Join-Path $outputRoot 'legacy/Legacy.exe')) 'Existing exe-only package'
$application=App 'Viewer.exe' @('SparkplugViewerNative.dll')
$script:fakeOutputs=@('Viewer.exe','SparkplugViewerNative.dll')
Publish-Application $application (Join-Path $outputRoot 'first')
$before=$script:publishes
Publish-Application $application (Join-Path $outputRoot 'cached')
Check ($script:publishes -eq $before) 'Reuse avoids a second compilation'
foreach ($name in $script:fakeOutputs) {
    Check ((Get-FileHash -LiteralPath (Join-Path $outputRoot "first/$name")).Hash -eq (Get-FileHash -LiteralPath (Join-Path $outputRoot "cached/$name")).Hash) "Exact cached $name"
}
$cache=$publishedApplications.Values | Where-Object { $_.Files.Count -eq 2 } | Select-Object -First 1
$dll=$cache.Files | Where-Object { $_.Name -eq 'SparkplugViewerNative.dll' }
[IO.File]::AppendAllText($dll.Path,'tampered')
$rejected=$false
try { Publish-Application $application (Join-Path $outputRoot 'tampered') } catch { $rejected=$_.Exception.Message -like 'Published application cache changed:*' }
Check $rejected 'Tampered companion cache rejected before copy'
$cases=@(
    @{Names=@('../escape.dll');Output=@('Viewer.exe')},
    @{Names=@('core.dll','CORE.dll');Output=@('Viewer.exe')},
    @{Names=@('libfbxsdk.dll');Output=@('Viewer.exe')},
    @{Names=@('core.dll');Output=@('Viewer.exe')},
    @{Names=@('core.dll');Output=@('Viewer.exe','core.dll','extra.dll')},
    @{Names=@('core.dll');Output=@('Viewer.exe','nested/core.dll')}
)
$index=0
foreach ($case in $cases) {
    $publishedApplications.Clear(); $script:fakeOutputs=$case.Output; $rejected=$false
    try { Publish-Application (App 'Viewer.exe' $case.Names) (Join-Path $outputRoot "invalid-$index") }
    catch { $rejected=$_.Exception.Message -match 'Invalid/duplicate companion|Publish contract violation' }
    Check $rejected "Companion guard $index"
    $index++
}
@{status='passed';checks=$script:checks;syntheticPublishes=$script:publishes;scope='Actual package copy/cache/allowlist with synthetic files; dotnet compilation substituted.'} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $outputRoot 'report.json') -Encoding UTF8
Write-Output "PASS companion packaging: $script:checks checks"
