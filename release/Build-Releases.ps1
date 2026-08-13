[CmdletBinding()]
param(
    [string[]]$Product,
    [string]$OutputDirectory,
    [switch]$NoArchive
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$manifestPath = Join-Path $PSScriptRoot 'release-manifest.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$runtimeIdentifier = [string]$manifest.runtimeIdentifier
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repositoryRoot 'artifacts\release\current'
}
if (-not [IO.Path]::IsPathRooted($OutputDirectory)) {
    $OutputDirectory = Join-Path $repositoryRoot $OutputDirectory
}
$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
$releaseArtifactsRoot = Join-Path $repositoryRoot 'artifacts\release'
$stagingRoot = Join-Path $releaseArtifactsRoot ('.staging\' + [Guid]::NewGuid().ToString('N'))

function Assert-PathUnderRoot([string]$Path, [string]$Root, [string]$Description) {
    $fullPath = [IO.Path]::GetFullPath($Path)
    $fullRoot = [IO.Path]::GetFullPath($Root).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    if (-not $fullPath.StartsWith($fullRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "$Description must stay under $fullRoot (got $fullPath)."
    }
    return $fullPath
}

$outputRoot = Assert-PathUnderRoot $outputRoot $releaseArtifactsRoot 'Output directory'
$stagingRoot = Assert-PathUnderRoot $stagingRoot $releaseArtifactsRoot 'Staging directory'

function Invoke-DotNet([string[]]$Arguments, [string]$WorkingDirectory = $repositoryRoot) {
    Write-Host "dotnet $($Arguments -join ' ')"
    & dotnet @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "dotnet exited with code $LASTEXITCODE."
    }
}

function Get-ProjectVersion([string]$ProjectPath) {
    $result = & dotnet msbuild $ProjectPath -nologo -getProperty:Version
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to read Version from $ProjectPath."
    }
    $version = ($result | Where-Object { $_ -match '^\d+\.\d+' } | Select-Object -Last 1).Trim()
    if ([string]::IsNullOrWhiteSpace($version)) {
        throw "Project $ProjectPath has no usable Version property."
    }
    return $version
}

function Build-RuntimeBootstrapper([string]$ProductName, [string]$Version) {
    $sourcePath = Join-Path $PSScriptRoot 'bootstrapper\RuntimeBootstrap.cs'
    $compilerPath = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
    if (-not (Test-Path -LiteralPath $compilerPath -PathType Leaf)) {
        throw "The Windows .NET Framework compiler is required to build the runtime bootstrapper: $compilerPath"
    }

    $bootstrapDirectory = Join-Path $stagingRoot '.bootstrapper'
    $bootstrapExecutable = Join-Path $bootstrapDirectory 'RuntimeBootstrap.exe'
    $assemblyInfoPath = Join-Path $bootstrapDirectory 'RuntimeBootstrap.Version.cs'
    New-Item -ItemType Directory -Path $bootstrapDirectory -Force | Out-Null

    if ($ProductName -notmatch '^[A-Za-z0-9._-]+$' -or $Version -notmatch '^[0-9A-Za-z.+-]+$') {
        throw "Unsafe bootstrapper metadata: product='$ProductName', version='$Version'."
    }
    $numericVersion = ($Version -split '[-+]', 2)[0]
    $versionParts = @($numericVersion.Split('.'))
    if ($versionParts.Count -lt 2 -or $versionParts.Count -gt 4 -or
        @($versionParts | Where-Object { $_ -notmatch '^\d+$' }).Count -gt 0) {
        throw "Version '$Version' cannot be converted to Windows file metadata."
    }
    while ($versionParts.Count -lt 4) { $versionParts += '0' }
    $assemblyVersion = $versionParts -join '.'
    $assemblyInfo = @"
using System.Reflection;
[assembly: AssemblyProduct("$ProductName")]
[assembly: AssemblyVersion("$assemblyVersion")]
[assembly: AssemblyFileVersion("$assemblyVersion")]
[assembly: AssemblyInformationalVersion("$Version")]
"@
    Set-Content -LiteralPath $assemblyInfoPath -Value $assemblyInfo -Encoding UTF8

    $arguments = @(
        '/nologo', '/target:winexe', '/platform:x64', '/optimize+', '/debug-',
        "/out:$bootstrapExecutable",
        '/reference:System.dll',
        '/reference:System.Core.dll',
        '/reference:System.Drawing.dll',
        '/reference:System.Windows.Forms.dll',
        '/reference:System.Web.Extensions.dll',
        $sourcePath,
        $assemblyInfoPath
    )
    Write-Host "csc $sourcePath"
    & $compilerPath @arguments
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $bootstrapExecutable -PathType Leaf)) {
        throw "Runtime bootstrapper compilation failed with code $LASTEXITCODE."
    }
    return $bootstrapExecutable
}

function Publish-Application($Application, [string]$Destination) {
    $projectPath = Join-Path $repositoryRoot ([string]$Application.project)
    if (-not (Test-Path -LiteralPath $projectPath -PathType Leaf)) {
        throw "Project not found: $projectPath"
    }

    $publishDirectory = Join-Path $stagingRoot ([Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $publishDirectory -Force | Out-Null
    try {
        $commonProperties = @(
            '-p:Configuration=Release',
            "-p:RuntimeIdentifier=$runtimeIdentifier",
            '-p:SelfContained=false',
            '-p:PublishSingleFile=true',
            '-p:IncludeNativeLibrariesForSelfExtract=true',
            '-p:PublishTrimmed=false',
            '-p:DebugType=embedded',
            '-p:DebugSymbols=false',
            '-p:SatelliteResourceLanguages=ru',
            '-p:IncludeSourceRevisionInInformationalVersion=false'
        )

        Invoke-DotNet (@('restore', $projectPath, '-r', $runtimeIdentifier, '--ignore-failed-sources') + $commonProperties)
        Invoke-DotNet (@('clean', $projectPath, '-c', 'Release', '-r', $runtimeIdentifier) + $commonProperties)
        $publishArguments = @('publish', $projectPath, '-c', 'Release', '-r', $runtimeIdentifier,
            '--no-restore', '-o', $publishDirectory) + $commonProperties
        Invoke-DotNet $publishArguments

        # Some native NuGet packages copy vendor debugging symbols even when
        # DebugSymbols=false. They are not runtime dependencies and belong in
        # a separate symbols package, not in the end-user release.
        Get-ChildItem -LiteralPath $publishDirectory -Filter '*.pdb' -File -Recurse |
            Remove-Item -Force

        $files = @(Get-ChildItem -LiteralPath $publishDirectory -File -Recurse)
        if ($files.Count -ne 1 -or $files[0].Name -ne [string]$Application.executable) {
            $names = ($files | ForEach-Object { $_.FullName.Substring($publishDirectory.Length + 1) }) -join ', '
            throw "Publish contract violation for $($Application.id): expected only $($Application.executable), got [$names]. Run from a clean tree and keep framework-dependent single-file enabled."
        }
        New-Item -ItemType Directory -Path $Destination -Force | Out-Null
        Copy-Item -LiteralPath $files[0].FullName -Destination (Join-Path $Destination $files[0].Name)
    }
    finally {
        if (Test-Path -LiteralPath $publishDirectory) {
            Remove-Item -LiteralPath $publishDirectory -Recurse -Force
        }
    }
}

function Copy-Documents($Application, [string]$Destination) {
    if ($null -eq $Application.PSObject.Properties['documents'] -or @($Application.documents).Count -eq 0) { return }
    $docsDirectory = Join-Path $Destination 'docs'
    New-Item -ItemType Directory -Path $docsDirectory -Force | Out-Null
    $usedNames = @{}
    foreach ($relativePath in $Application.documents) {
        $source = Join-Path $repositoryRoot ([string]$relativePath)
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
            throw "Release document not found: $source"
        }
        $name = Split-Path $source -Leaf
        if ($usedNames.ContainsKey($name)) {
            throw "Duplicate document name '$name' in $($Application.id)."
        }
        $usedNames[$name] = $true
        Copy-Item -LiteralPath $source -Destination (Join-Path $docsDirectory $name)
    }
}

function Assert-ReleaseLayout([string]$ReleaseDirectory, [string]$ExecutableName) {
    $rootFiles = @(Get-ChildItem -LiteralPath $ReleaseDirectory -File)
    $unexpected = @($rootFiles | Where-Object { $_.Name -ne $ExecutableName -and $_.Extension -notin @('.json', '.toml', '.ini', '.config') })
    if ($unexpected.Count -gt 0) {
        throw "Unexpected root files in ${ReleaseDirectory}: $($unexpected.Name -join ', ')"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $ReleaseDirectory $ExecutableName) -PathType Leaf)) {
        throw "Missing root executable $ExecutableName in $ReleaseDirectory."
    }

    $duplicateGroups = @(Get-ChildItem -LiteralPath $ReleaseDirectory -File -Recurse |
        Get-FileHash -Algorithm SHA256 | Group-Object Hash | Where-Object Count -gt 1)
    if ($duplicateGroups.Count -gt 0) {
        $duplicates = $duplicateGroups | ForEach-Object { ($_.Group.Path -join ' = ') }
        throw "Duplicate file contents found in ${ReleaseDirectory}:`n$($duplicates -join "`n")"
    }
}

$selectedProducts = @($manifest.products)
if ($null -ne $Product -and $Product.Count -gt 0) {
    $requested = @($Product | ForEach-Object { $_.ToLowerInvariant() })
    $selectedProducts = @($selectedProducts | Where-Object { $requested -contains ([string]$_.id).ToLowerInvariant() })
    $missing = @($Product | Where-Object { $name = $_; -not ($selectedProducts | Where-Object { $_.id -ieq $name }) })
    if ($missing.Count -gt 0) {
        throw "Unknown products: $($missing -join ', '). Available: $(($manifest.products.id) -join ', ')."
    }
}

New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
New-Item -ItemType Directory -Path $stagingRoot -Force | Out-Null
$originalLocalAppData = [Environment]::GetEnvironmentVariable('LOCALAPPDATA', 'Process')
$buildLocalAppData = Join-Path $stagingRoot '.localappdata'
New-Item -ItemType Directory -Path $buildLocalAppData -Force | Out-Null
$env:LOCALAPPDATA = $buildLocalAppData
$builtPackages = @()
try {
    foreach ($entry in $selectedProducts) {
        $projectPath = Join-Path $repositoryRoot ([string]$entry.project)
        $version = Get-ProjectVersion $projectPath
        $isSuite = $null -ne $entry.PSObject.Properties['suite'] -and [bool]$entry.suite
        $suiteSuffix = if ($isSuite) { '-suite' } else { '' }
        $packageName = "$($entry.id)-$version$suiteSuffix-$runtimeIdentifier"
        $bootstrapExecutable = Build-RuntimeBootstrapper ([string]$entry.id) $version
        $packageDirectory = Join-Path $outputRoot $packageName
        $packageDirectory = Assert-PathUnderRoot $packageDirectory $outputRoot 'Package directory'
        if (Test-Path -LiteralPath $packageDirectory) {
            Remove-Item -LiteralPath $packageDirectory -Recurse -Force
        }
        New-Item -ItemType Directory -Path $packageDirectory | Out-Null

        Write-Host "`nBuilding $packageName"
        $applicationDirectory = Join-Path $packageDirectory 'app'
        Publish-Application $entry $applicationDirectory
        Copy-Documents $entry $packageDirectory

        if ($null -ne $entry.PSObject.Properties['tools']) {
            foreach ($tool in @($entry.tools)) {
                $toolDirectory = Join-Path $packageDirectory ("tools\" + [string]$tool.id)
                Publish-Application $tool $toolDirectory
                Copy-Documents $tool $toolDirectory
            }
        }

        $manifestData = [ordered]@{
            schemaVersion = 1
            product = [string]$entry.id
            version = $version
            runtimeIdentifier = $runtimeIdentifier
            deployment = 'framework-dependent-single-file-with-runtime-bootstrapper'
            entryPoint = "app\$([string]$entry.executable)"
            runtime = [ordered]@{
                framework = 'Microsoft.WindowsDesktop.App'
                version = '8.0'
                architecture = 'x64'
                bootstrap = 'download-and-install-if-missing'
                officialInstaller = 'https://aka.ms/dotnet/8.0/windowsdesktop-runtime-win-x64.exe'
            }
        }
        $manifestData | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $packageDirectory 'release.json') -Encoding utf8
        Copy-Item -LiteralPath $bootstrapExecutable -Destination (Join-Path $packageDirectory ([string]$entry.executable))
        Assert-ReleaseLayout $packageDirectory ([string]$entry.executable)

        if (-not $NoArchive) {
            $archivePath = Join-Path $outputRoot ($packageName + '.zip')
            if (Test-Path -LiteralPath $archivePath) { Remove-Item -LiteralPath $archivePath -Force }
            Compress-Archive -Path $packageDirectory -DestinationPath $archivePath -CompressionLevel Optimal
        }
        $builtPackages += $packageDirectory
    }
}
finally {
    if ($null -eq $originalLocalAppData) {
        Remove-Item Env:LOCALAPPDATA -ErrorAction SilentlyContinue
    }
    else {
        $env:LOCALAPPDATA = $originalLocalAppData
    }
    if (Test-Path -LiteralPath $stagingRoot) {
        Remove-Item -LiteralPath $stagingRoot -Recurse -Force
    }
    $stagingParent = Split-Path $stagingRoot -Parent
    if ((Test-Path -LiteralPath $stagingParent -PathType Container) -and
        @(Get-ChildItem -LiteralPath $stagingParent -Force).Count -eq 0) {
        Remove-Item -LiteralPath $stagingParent -Force
    }
}

Write-Host "`nRelease packages:"
$builtPackages | ForEach-Object { Write-Host "  $_" }
