#requires -Version 5.1
<#
.SYNOPSIS
Builds a fresh, local E7 runnable folder from held sources and qualified native inputs.
.DESCRIPTION
Publishes Host and Actor serially with the same RID/runtime mode, verifies every
overlap before merging, and writes the manifest last. Never runs a game or native
build, installs a runtime, edits the registry, or removes failed/previous output.
#>
[CmdletBinding()]
param(
    [ValidateSet('x64','x86')][string]$Architecture = 'x64',
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [Parameter(Mandatory = $true)][string]$NativeBuildDirectory,
    [Parameter(Mandatory = $true)][string]$XpProbeBuildDirectory,
    [ValidateSet('Release','Debug')][string]$Configuration = 'Release',
    [Nullable[bool]]$SelfContained
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = New-Object Text.UTF8Encoding($false)
$bundleUtf8 = New-Object Text.UTF8Encoding($false)
$bundleRepo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\','/')
$bundleSelfContained = if ($null -eq $SelfContained) { $Architecture -eq 'x86' } else { [bool]$SelfContained }
$bundleFramework = if ($Architecture -eq 'x86') { 'net6.0' } else { 'net8.0' }
$bundleRid = 'win-' + $Architecture
$bundlePins = New-Object 'Collections.Generic.List[IDisposable]'
$bundlePinned = New-Object 'Collections.Generic.Dictionary[string,IO.FileStream]' ([StringComparer]::OrdinalIgnoreCase)
$bundleManifestName = 'WinxCoop.SessionBundle.json'

function Assert-BundleOrdinary([string]$Path, [bool]$Directory) {
    $item = Get-Item -LiteralPath $Path -Force
    if ($item.PSIsContainer -ne $Directory -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Expected an ordinary local $(if ($Directory) {'directory'} else {'file'}): $Path"
    }
    $parent = if ($Directory) { $item.Parent } else { $item.Directory }
    while ($null -ne $parent) {
        if ($parent.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Reparse-point ancestor: $($parent.FullName)" }
        $parent = $parent.Parent
    }
}
function Resolve-BundleDirectory([string]$Path) {
    $full = [IO.Path]::GetFullPath($Path).TrimEnd('\','/')
    if ($full -notmatch '^[A-Za-z]:\\' -or $full.Length -le 3) { throw "An absolute local directory is required: $Path" }
    Assert-BundleOrdinary $full $true
    return $full
}
function Get-BundleHash([string]$Path, [switch]$Pin) {
    if ($bundlePinned.ContainsKey($Path)) { $stream = $bundlePinned[$Path]; $owned = $false }
    else {
        Assert-BundleOrdinary $Path $false
        $stream = New-Object IO.FileStream($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
        $owned = -not $Pin
        if ($Pin) { $bundlePins.Add($stream); $bundlePinned.Add($Path,$stream) }
    }
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $stream.Position = 0; return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','') }
    finally { $stream.Position = 0; $sha.Dispose(); if ($owned) { $stream.Dispose() } }
}
function Write-BundleJson([string]$Path, $Value) {
    if (Test-Path -LiteralPath $Path) { throw "Refusing to replace evidence: $Path" }
    [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 12) + "`n", $bundleUtf8)
}
function Get-BundleFiles([string]$Root, [string[]]$Extensions) {
    $pending = New-Object 'Collections.Generic.Stack[string]'; $pending.Push($Root)
    while ($pending.Count) {
        $directory = $pending.Pop(); Assert-BundleOrdinary $directory $true
        foreach ($item in Get-ChildItem -LiteralPath $directory -Force) {
            if ($item.PSIsContainer) {
                if ($item.Name -in @('bin','obj','.git','.private','.agents','.codex','node_modules','artifacts','build','resources')) { continue }
                if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Source/output directory is a reparse point: $($item.FullName)" }
                $pending.Push($item.FullName)
            } elseif ($item.Extension.ToLowerInvariant() -in $Extensions) { $item.FullName }
        }
    }
}
function Get-BundleSourceProfile([switch]$Pin) {
    $files = New-Object 'Collections.Generic.Dictionary[string,string]' ([StringComparer]::OrdinalIgnoreCase)
    $queue = New-Object 'Collections.Generic.Queue[string]'
    $add = {
        param([string]$Path)
        $full = [IO.Path]::GetFullPath($Path)
        if (-not $full.StartsWith($bundleRepo + '\',[StringComparison]::OrdinalIgnoreCase)) { throw "Source escaped repository: $full" }
        Assert-BundleOrdinary $full $false
        $relative = $full.Substring($bundleRepo.Length + 1).Replace('\','/')
        if ($relative -match '(^|/)(AGENTS\.md|\.private|\.agents|\.git|bin|obj)(/|$)') { throw "Private/generated source in profile: $relative" }
        if (-not $files.ContainsKey($relative)) { $files.Add($relative,$full); $queue.Enqueue($full) }
    }
    foreach ($relative in @('tools/WinxCoop/src/SessionHost','tools/WinxCoop/src/SessionRuntime',
        'tools/WinxCoop/src/RuntimeProbe','tools/WinxCoop/src/Compatibility',
        'tools/WinxCoop/src/DiagnosticInput','tools/WinxCoop/src/Gameplay','tools/WinxCoop/src/Networking',
        'tools/WinxCoop/src/Progression','tools/WinxCoop/src/WorldState','tools/WinxCoop/src/Adapter',
        'tools/SmoViewer/SmoNativeValidator.Core')) {
        foreach ($file in Get-BundleFiles (Join-Path $bundleRepo $relative) @('.cs','.cpp','.h','.c','.def','.csproj','.props','.targets')) { & $add $file }
    }
    foreach ($relative in @('tools/WinxCoop/WinxCoop.SessionHost.csproj','tools/WinxCoop/WinxCoop.SessionRuntime.csproj',
        'tools/WinxCoop/WinxCoop.ActorProbe.csproj','tools/WinxCoop/WinxCoop.ActorProbe.Win7.csproj',
        'tools/WinxCoop/WinxCoop.CompatCore.csproj','tools/WinxCoop/CMakeLists.txt',
        'tools/WinxCoop/Build-NetworkChecks.ps1','tools/WinxCoop/Build-SessionBundle.ps1',
        'tools/WinxCoop/src/Launcher/LobbyDiscovery.cpp','tools/WinxCoop/src/Launcher/LobbyDiscovery.h',
        'tools/WinxCoop/tests/XpGameProbe.c','Sparkplug/CMakeLists.txt')) { & $add (Join-Path $bundleRepo $relative) }
    foreach ($relative in @('global.json','NuGet.Config','Directory.Build.props','Directory.Build.targets','Directory.Packages.props',
        'tools/Directory.Build.props','tools/Directory.Build.targets','tools/Directory.Packages.props',
        'tools/WinxCoop/Directory.Build.props','tools/WinxCoop/Directory.Build.targets','tools/WinxCoop/Directory.Packages.props',
        'tools/SmoViewer/Directory.Build.props','tools/SmoViewer/Directory.Build.targets','tools/SmoViewer/Directory.Packages.props')) {
        $file = Join-Path $bundleRepo $relative; if (Test-Path -LiteralPath $file) { & $add $file }
    }
    # Include all Windows SparkBase/Network target inputs, then resolve local
    # quoted includes and companion cpp conservatively. This is a source profile,
    # not an attempted second native linker or a claim of semantic equivalence.
    $common = Get-Content -LiteralPath (Join-Path $bundleRepo 'Sparkplug/CMakeLists.txt') -Raw -Encoding utf8
    foreach ($block in [regex]::Matches($common,'(?s)(?:add_library|target_sources)\(\s*(?:SparkBase|SparkNetwork)\b.*?\)')) {
        foreach ($match in [regex]::Matches($block.Value,'(?:Code|Analysis)/[A-Za-z0-9_./-]+\.(?:cpp|h)')) {
            & $add (Join-Path (Join-Path $bundleRepo 'Sparkplug') $match.Value)
        }
    }
    while ($queue.Count) {
        $file = $queue.Dequeue(); if ([IO.Path]::GetExtension($file) -notin @('.cpp','.h','.c')) { continue }
        $text = Get-Content -LiteralPath $file -Raw -Encoding utf8
        foreach ($include in [regex]::Matches($text,'(?m)^\s*#\s*include\s*"([^"]+)"')) {
            $found = $null
            foreach ($root in @([IO.Path]::GetDirectoryName($file),(Join-Path $bundleRepo 'Sparkplug'),
                (Join-Path $bundleRepo 'Winx'),(Join-Path $bundleRepo 'tools/WinxCoop/src'))) {
                $candidate = [IO.Path]::GetFullPath((Join-Path $root $include.Groups[1].Value))
                if (Test-Path -LiteralPath $candidate -PathType Leaf) { $found = $candidate; break }
            }
            if ($null -eq $found) { throw "Unresolved quoted include in source profile: $file -> $($include.Groups[1].Value)" }
            & $add $found
            if ([IO.Path]::GetExtension($found) -eq '.h') {
                $companion = [IO.Path]::ChangeExtension($found,'.cpp')
                if (Test-Path -LiteralPath $companion) { & $add $companion }
            }
        }
    }
    $names = [string[]]@($files.Keys); [Array]::Sort($names,[StringComparer]::Ordinal)
    $rows = @(); $canonical = New-Object Text.StringBuilder
    [void]$canonical.Append("WinxCoop.SourceProfile/v1`n")
    foreach ($name in $names) {
        $hash = Get-BundleHash $files[$name] -Pin:$Pin
        $rows += [ordered]@{ Path=$name; Sha256=$hash }
        [void]$canonical.Append($name).Append("`t").Append($hash).Append("`n")
    }
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $digest = ([BitConverter]::ToString($sha.ComputeHash($bundleUtf8.GetBytes($canonical.ToString())))).Replace('-','') }
    finally { $sha.Dispose() }
    return [ordered]@{ Version=1; Algorithm='WinxCoop.SourceProfile/v1: UTF8 ordinal path TAB uppercase SHA256 LF'; SourceProfileSha256=$digest; Files=$rows }
}
function Assert-BundleCmake([string]$Directory) {
    $cachePath = Join-Path $Directory 'CMakeCache.txt'; Assert-BundleOrdinary $cachePath $false
    $cache = Get-Content -LiteralPath $cachePath -Raw -Encoding utf8
    $bundleCmakeHome = [regex]::Match($cache,'(?m)^CMAKE_HOME_DIRECTORY:INTERNAL=([^\r\n]+)')
    if (-not $bundleCmakeHome.Success -or [IO.Path]::GetFullPath($bundleCmakeHome.Groups[1].Value).TrimEnd('\','/') -ine $PSScriptRoot) {
        throw "Native input is not a WinxCoop CMake build: $Directory"
    }
}
function Assert-BundleNativeTests([string]$Directory) {
    $logPath = Join-Path $Directory 'Testing/Temporary/LastTest.log'; Assert-BundleOrdinary $logPath $false
    $log = Get-Content -LiteralPath $logPath -Raw -Encoding utf8
    $names = @('WinxCoopProgressionTests','WinxCoopProgressionNativeTests','WinxCoopSessionBootstrapTests','WinxCoopSessionDiscoveryTests',
        'WinxCoopWindowsPumpWaitTests','WinxCoopWorldStateTests','WinxCoopTcpFramerTests','WinxCoopSessionTests',
        'WinxCoopGameplayTests','WinxCoopPoseInterpolationTests','WinxCoopPoseInterpolationNativeTests','WinxCoopDiagnosticInputTests',
        'SparkplugNetworkManagerTests','SparkplugNetworkFlowTests')
    if ([regex]::Matches($log,'(?m)^\s*Test Passed\.\s*$').Count -ne $names.Count -or $log -match 'Test Failed\.') { throw 'Native14 suites must pass before packaging.' }
    foreach ($name in $names) { if ($log -notmatch ('(?m)^\d+/\d+ Test: ' + [regex]::Escape($name) + '\s*$')) { throw "Missing qualified native suite: $name" } }
}
function Get-BundlePe([string]$Path) {
    $stream = New-Object IO.FileStream($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    $reader = New-Object IO.BinaryReader($stream)
    try {
        if ($reader.ReadUInt16() -ne 0x5a4d -or $stream.Length -lt 256) { throw "Invalid PE: $Path" }
        $stream.Position=0x3c; $offset=$reader.ReadUInt32()
        if ($offset -gt ($stream.Length - 256)) { throw "Invalid PE header bounds: $Path" }
        $stream.Position=$offset; if ($reader.ReadUInt32() -ne 0x4550) { throw "Invalid PE signature: $Path" }
        $machine=$reader.ReadUInt16(); $stream.Position=$offset+24; $magic=$reader.ReadUInt16()
        $directories = if ($magic -eq 0x10b) { $offset+24+96 } elseif ($magic -eq 0x20b) { $offset+24+112 } else { throw "Unknown PE optional header: $Path" }
        $stream.Position=$directories+14*8; $managed=$reader.ReadUInt32() -ne 0
        return @{ Machine=$machine; Managed=$managed }
    } finally { $reader.Dispose(); $stream.Dispose() }
}
function Copy-BundleFile([string]$Source, [string]$Name) {
    if ($Name -ne [IO.Path]::GetFileName($Name) -or $Name.Length -ge 128 -or $Name -match '^(AGENTS\.md|\.private|\.agents)$') { throw "Invalid bundle name: $Name" }
    $hash = Get-BundleHash $Source -Pin
    $target = Join-Path $bundleOutput $Name
    if (Test-Path -LiteralPath $target) {
        if ((Get-BundleHash $target) -ne $hash) { throw "Different shared publish files cannot be merged: $Name" }
    } else { [IO.File]::Copy($Source,$target,$false) }
    if ((Get-BundleHash $target) -ne $hash) { throw "Copied file changed: $Name" }
}
function Publish-BundleProject([string]$Project, [string]$Directory, [string]$LogName) {
    $arguments = @('publish',$Project,'-c',$Configuration,'-r',$bundleRid,'--self-contained',$bundleSelfContained.ToString().ToLowerInvariant(),
        '-o',$Directory,'-m:1','-nr:false','-p:BuildInParallel=false','-p:UseSharedCompilation=false',
        '-p:UseAppHost=true','-p:PublishSingleFile=false','-p:PublishReadyToRun=false','-p:DebugType=none','-p:DebugSymbols=false',
        '-p:WinxCoopBundleDesktopRuntime=true',
        '-p:SatelliteResourceLanguages=en','-p:NuGetAudit=false','-p:SkipSparkplugNativeBuild=true',
        ('-p:WinxCoopTargetFramework='+$bundleFramework),('-p:WinxCoopPlatform='+$Architecture),('-p:PlatformTarget='+$Architecture),
        ('-p:DirectoryBuildPropsPath='+$bundleProps))
    if ($Architecture -eq 'x86') { $arguments += '-p:RuntimeFrameworkVersion=6.0.36' }
    & $bundleDotnet @arguments *> (Join-Path $bundleWork $LogName)
    if ($LASTEXITCODE -ne 0) { throw "dotnet publish failed; retained log: $(Join-Path $bundleWork $LogName)" }
    Assert-BundleOrdinary $Directory $true
    $children = @(Get-ChildItem -LiteralPath $Directory -Force)
    if (@($children | Where-Object { $_.PSIsContainer }).Count) { throw "Publish requires subdirectories; flat manifest cannot omit them: $Directory" }
    foreach ($file in $children) { Copy-BundleFile $file.FullName $file.Name }
}
function Assert-BundleRuntimeConfig([string]$Name) {
    $path=Join-Path $bundleOutput ($Name+'.runtimeconfig.json'); Assert-BundleOrdinary $path $false
    $json=Get-Content -LiteralPath $path -Raw -Encoding utf8 | ConvertFrom-Json
    $options=$json.runtimeOptions; $properties=@($options.PSObject.Properties.Name)
    $included=$properties -contains 'includedFrameworks'; $framework=($properties -contains 'framework') -or ($properties -contains 'frameworks')
    if ($bundleSelfContained) {
        if (-not $included -or $framework) { throw "Executable runtimeconfig is not self-contained: $Name" }
    } elseif (-not $framework -or $included) { throw "Executable runtimeconfig is not framework-dependent: $Name" }
    if ($options.tfm -notlike ($bundleFramework+'*')) { throw "Executable runtime framework differs: $Name" }
    $runtimeRows = if ($included) { @($options.includedFrameworks) } elseif ($properties -contains 'frameworks') { @($options.frameworks) } else { @($options.framework) }
    $runtimeNames = @($runtimeRows | ForEach-Object { $_.name })
    if ($runtimeRows.Count -ne 2 -or $runtimeNames -cnotcontains 'Microsoft.NETCore.App' -or $runtimeNames -cnotcontains 'Microsoft.WindowsDesktop.App') {
        throw "Both executables require the same explicit Core and WindowsDesktop runtime set: $Name"
    }
    $runtimeKeys = [string[]]@($runtimeRows | ForEach-Object {
        if ($_.version -notmatch '^\d+\.\d+\.\d+$') { throw "Invalid executable runtime version: $Name" }
        [string]$_.name + '=' + [string]$_.version
    })
    [Array]::Sort($runtimeKeys,[StringComparer]::Ordinal)
    return $runtimeKeys -join "`n"
}

try {
    $bundleOutput=[IO.Path]::GetFullPath($OutputDirectory).TrimEnd('\','/')
    if ($bundleOutput -notmatch '^[A-Za-z]:\\' -or $bundleOutput.Length -gt 140 -or $bundleOutput.Length -le 3 -or $bundleOutput -match '[^\x20-\x7e]|"') {
        throw 'Use a fresh absolute ASCII output directory with at most140 characters.'
    }
    if (Test-Path -LiteralPath $bundleOutput) { throw "Output must not exist: $bundleOutput" }
    $bundleParent=Resolve-BundleDirectory ([IO.Path]::GetDirectoryName($bundleOutput))
    $bundleNative=Resolve-BundleDirectory $NativeBuildDirectory; $bundleXp=Resolve-BundleDirectory $XpProbeBuildDirectory
    Assert-BundleCmake $bundleNative; Assert-BundleCmake $bundleXp; Assert-BundleNativeTests $bundleNative
    $bundleDotnet=(Get-Command dotnet -CommandType Application -ErrorAction Stop).Source
    $profile=Get-BundleSourceProfile -Pin
    $bundleWork=$bundleOutput+'.build-'+[Guid]::NewGuid().ToString('N').Substring(0,8)
    if (Test-Path -LiteralPath $bundleWork) { throw "Work directory must not exist: $bundleWork" }
    [void][IO.Directory]::CreateDirectory($bundleWork); [void][IO.Directory]::CreateDirectory($bundleOutput)
    $bundleProps=Join-Path $bundleWork 'SessionBundle.Build.props'
    $props=@'
<Project>
  <PropertyGroup>
    <WinxCoopInheritedBuildProps>$([MSBuild]::GetPathOfFileAbove('Directory.Build.props', '$(MSBuildProjectDirectory)/'))</WinxCoopInheritedBuildProps>
  </PropertyGroup>
  <Import Project="$(WinxCoopInheritedBuildProps)" Condition="'$(WinxCoopInheritedBuildProps)' != ''" />
  <PropertyGroup>
    <BaseIntermediateOutputPath>__WORK__/obj/$(MSBuildProjectName)/</BaseIntermediateOutputPath>
    <MSBuildProjectExtensionsPath>$(BaseIntermediateOutputPath)</MSBuildProjectExtensionsPath>
    <BaseOutputPath>__WORK__/bin/$(MSBuildProjectName)/</BaseOutputPath>
    <DefaultItemExcludes>$(DefaultItemExcludes);$(MSBuildProjectDirectory)/obj/**;$(MSBuildProjectDirectory)/bin/**</DefaultItemExcludes>
  </PropertyGroup>
</Project>
'@
    [IO.File]::WriteAllText($bundleProps,$props.Replace('__WORK__',[Security.SecurityElement]::Escape($bundleWork.Replace('\','/'))),$bundleUtf8)
    $requiredNative=@('WinxCoopInputBridge.exe','WinxCoopWorldBridge.exe','WinxCoop.ProgressionNative.dll',
        'WinxCoop.PoseInterpolationNative.dll','WinxCoop.SessionBootstrapNative.dll','WinxCoop.SessionDiscoveryNative.dll')
    foreach ($name in $requiredNative) { Copy-BundleFile (Join-Path $bundleNative $name) $name }
    Copy-BundleFile (Join-Path $bundleXp 'XpGameProbe.exe') 'XpGameProbe.exe'
    Publish-BundleProject (Join-Path $PSScriptRoot 'WinxCoop.SessionHost.csproj') (Join-Path $bundleWork 'host-publish') 'host-publish.log'
    $actorProject=if ($Architecture -eq 'x86') { 'WinxCoop.ActorProbe.Win7.csproj' } else { 'WinxCoop.ActorProbe.csproj' }
    Publish-BundleProject (Join-Path $PSScriptRoot $actorProject) (Join-Path $bundleWork 'actor-publish') 'actor-publish.log'
    $bundleHostRuntime = Assert-BundleRuntimeConfig 'WinxCoop.SessionHost'
    $bundleActorRuntime = Assert-BundleRuntimeConfig 'WinxCoop.ActorProbe'
    if ($bundleHostRuntime -cne $bundleActorRuntime) { throw 'Host and Actor runtime names/versions differ; no bundle manifest is issued.' }
    if ($bundleSelfContained) {
        foreach ($name in @('coreclr.dll','hostfxr.dll','hostpolicy.dll','System.Private.CoreLib.dll','System.Windows.Forms.dll')) {
            Assert-BundleOrdinary (Join-Path $bundleOutput $name) $false
        }
    }
    Copy-BundleFile (Join-Path $bundleRepo 'LICENSE.md') 'SparkplugEngineResearch.LICENSE.md'
    Copy-BundleFile (Join-Path $bundleRepo 'ACKNOWLEDGEMENTS.md') 'SparkplugEngineResearch.ACKNOWLEDGEMENTS.md'
    $after=Get-BundleSourceProfile
    if ($profile.SourceProfileSha256 -ne $after.SourceProfileSha256) { throw 'Source closure changed during publish; no bundle manifest is issued.' }
    Write-BundleJson (Join-Path $bundleOutput 'WinxCoop.SourceProfile.json') $profile
    $required=@('WinxCoop.SessionHost.exe','WinxCoop.SessionHost.dll','WinxCoop.ActorProbe.exe','WinxCoop.ActorProbe.dll',
        'SmoNativeValidator.Core.dll','WinxCoop.SessionRuntime.dll','XpGameProbe.exe')+$requiredNative
    foreach ($name in $required) { Assert-BundleOrdinary (Join-Path $bundleOutput $name) $false }
    $files=@(Get-ChildItem -LiteralPath $bundleOutput -File -Force | Sort-Object Name)
    if ($files.Count -gt 512) { throw 'Flat runtime manifest exceeds512 files; no files were dropped.' }
    $rows=@(); $expected=if ($Architecture -eq 'x64') { 0x8664 } else { 0x14c }
    foreach ($file in $files) {
        $kind='data'
        if ($file.Extension -in @('.dll','.exe')) {
            $pe=Get-BundlePe $file.FullName
            $kind=if ($file.Extension -eq '.exe') { 'executable' } elseif ($pe.Managed) { 'managed' } else { 'native' }
            if ($file.Name -eq 'XpGameProbe.exe') {
                if ($pe.Machine -ne 0x14c -or $pe.Managed) { throw 'XpGameProbe must be the native x86 helper.' }
            } elseif ($kind -in @('native','executable') -and $pe.Machine -ne $expected) { throw "Wrong native PE architecture: $($file.Name)" }
        }
        $rows += [ordered]@{ Name=$file.Name; Sha256=(Get-BundleHash $file.FullName); Kind=$kind }
    }
    $manifest=[ordered]@{ Version=1; Architecture=$Architecture; SourceProfileSha256=$profile.SourceProfileSha256; Files=$rows }
    Write-BundleJson (Join-Path $bundleOutput $bundleManifestName) $manifest
    Write-Output "Local E7 bundle prepared: $bundleOutput"
    Write-Output "Architecture=$Architecture framework=$bundleFramework selfContained=$bundleSelfContained files=$($rows.Count) sourceProfile=$($profile.SourceProfileSha256)"
    Write-Output "Retained serial publish work/logs: $bundleWork"
} catch {
    Write-Warning 'Bundle preparation failed. New partial output/work is preserved; no previous folder was replaced.'
    throw
} finally { foreach ($pin in $bundlePins) { $pin.Dispose() } }
