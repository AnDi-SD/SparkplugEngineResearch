#requires -Version 5.1
<#
.SYNOPSIS
Creates a new isolated Alfea28 game copy for the WinxCoop Stage P prototype.
.DESCRIPTION
Reads the source without changing it. Copies the pinned WinxClub.exe, two pinned
CRT71 DLLs, Media and Shaders, using independent ordinary files. Existing destinations,
reparse points, physical path overlap and unsafe destination paths are refused.
No resource is bundled with this script. No game, helper or registry operation
is started. A failed new tree is preserved for inspection; nothing is deleted.
#>
[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'Medium')]
param(
    [Parameter(Mandatory = $true)]
    [Alias('Source')]
    [ValidateNotNullOrEmpty()]
    [string]$SourceGameRoot,

    [Parameter(Mandatory = $true)]
    [Alias('GameRoot')]
    [ValidateNotNullOrEmpty()]
    [string]$Destination,

    [ValidateNotNullOrEmpty()]
    [string]$RuntimeDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$prepareExeSha = '3F022480BF55045DA4BF692E4BC8862ED38FC024E8A964A558FBDFDF646DFC4F'
$prepareOriginalConfigSha = '8528B4231A5D3FC302D90C19ABFCA87FE355011A221F13F3F723097FD1C41068'
$prepareRuntimeFiles = @(
    [pscustomobject]@{ name = 'msvcr71.dll'; bytes = [long]344064; sha256 = 'DCA0E5FAF6C94B6ADFF4D90D40795D5A91BA3A3059EA408E992A0F039A494D46' },
    [pscustomobject]@{ name = 'msvcp71.dll'; bytes = [long]503808; sha256 = '71D3CC0EDD1866C77DE6444A260512F54C40A8413F673ABF79FA24BE27495C23' }
)
$prepareBufferBytes = 65536
$prepareMaximumFiles = 200000
$prepareMaximumDepth = 64
$prepareMaximumRootLength = 180
$prepareMaximumPathLength = 239
$prepareAscii = New-Object System.Text.ASCIIEncoding
$prepareUtf8 = New-Object System.Text.UTF8Encoding($false)

# Filesystem identity only, for aliases such as SUBST drives. These are Win7
# path APIs, not game calls. CreateDirectoryW is used for atomic create-new
# refusal, rather than Directory.CreateDirectory's accept-existing behavior.
if (-not ('WinxCoop.Preparation.NativePathsV1' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
using Microsoft.Win32.SafeHandles;
namespace WinxCoop.Preparation {
    public static class NativePathsV1 {
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, ExactSpelling = true, SetLastError = true)]
        private static extern SafeFileHandle CreateFileW(string name, uint access, uint share, IntPtr security, uint creation, uint flags, IntPtr template);
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, ExactSpelling = true, SetLastError = true)]
        private static extern uint GetFinalPathNameByHandleW(SafeFileHandle handle, StringBuilder path, uint length, uint flags);
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, ExactSpelling = true, SetLastError = true)]
        private static extern bool CreateDirectoryW(string name, IntPtr security);
        public static string FinalDirectory(string name) {
            using (SafeFileHandle handle = CreateFileW(name, 0x80, 7, IntPtr.Zero, 3, 0x02000000, IntPtr.Zero)) {
                if (handle.IsInvalid) throw new Win32Exception(Marshal.GetLastWin32Error(), "Directory identity unavailable: " + name);
                StringBuilder path = new StringBuilder(32768);
                uint count = GetFinalPathNameByHandleW(handle, path, (uint)path.Capacity, 0);
                if (count == 0 || count >= path.Capacity) throw new Win32Exception(Marshal.GetLastWin32Error(), "Directory identity exceeds its bound");
                string result = path.ToString();
                if (result.StartsWith(@"\\?\UNC\", StringComparison.OrdinalIgnoreCase)) return @"\\" + result.Substring(8);
                if (result.StartsWith(@"\\?\", StringComparison.Ordinal)) return result.Substring(4);
                return result;
            }
        }
        public static void CreateNewDirectory(string name) {
            if (!CreateDirectoryW(name, IntPtr.Zero)) throw new Win32Exception(Marshal.GetLastWin32Error(), "Fresh directory required: " + name);
        }
    }
}
'@
}

function Get-PrepareFullPath([string]$Path) {
    $value = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Path)
    $value = [IO.Path]::GetFullPath($value)
    $root = [IO.Path]::GetPathRoot($value)
    if ($value.Length -gt $root.Length) { $value = $value.TrimEnd([char[]]'\/') }
    return $value
}

function Assert-PrepareOrdinary([string]$Path, [bool]$Directory) {
    $attributes = [IO.File]::GetAttributes($Path)
    if (($attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Reparse points are not allowed: $Path" }
    if ((($attributes -band [IO.FileAttributes]::Directory) -ne 0) -ne $Directory) { throw "Unexpected file/directory type: $Path" }
}

function Assert-PrepareAncestors([string]$Directory) {
    $current = $Directory
    while ($current) {
        Assert-PrepareOrdinary $current $true
        $parent = [IO.Directory]::GetParent($current)
        if ($null -eq $parent) { break }
        $current = $parent.FullName
    }
}

function Assert-PrepareDestinationPath([string]$Path) {
    if ($Path.Length -gt $prepareMaximumPathLength -or $Path -notmatch '^[A-Za-z]:\\') { throw "Short absolute ASCII drive path required: $Path" }
    foreach ($character in $Path.ToCharArray()) {
        if ([int]$character -lt 32 -or [int]$character -gt 126 -or $character -eq '"') { throw "Destination paths must be plain ASCII: $Path" }
    }
}

function Test-PrepareExists([string]$Path) {
    try { $null = [IO.File]::GetAttributes($Path); return $true }
    catch [IO.FileNotFoundException] { return $false }
    catch [IO.DirectoryNotFoundException] { return $false }
}

function Test-PrepareOverlap([string]$Left, [string]$Right) {
    $comparison = [StringComparison]::OrdinalIgnoreCase
    $leftPrefix = $Left.TrimEnd([char[]]'\/') + '\'
    $rightPrefix = $Right.TrimEnd([char[]]'\/') + '\'
    return $Left.Equals($Right, $comparison) -or $Left.StartsWith($rightPrefix, $comparison) -or $Right.StartsWith($leftPrefix, $comparison)
}

function Get-PrepareDigest([string]$Path) {
    Assert-PrepareOrdinary $Path $false
    $stream = $null
    $hash = [Security.Cryptography.SHA256]::Create()
    try {
        $stream = New-Object IO.FileStream($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read, $prepareBufferBytes, [IO.FileOptions]::SequentialScan)
        return ([BitConverter]::ToString($hash.ComputeHash($stream))).Replace('-', '')
    }
    finally { if ($null -ne $stream) { $stream.Dispose() }; $hash.Dispose() }
}

function Copy-PrepareVerified([string]$SourcePath, [string]$TargetPath, [string]$RelativePath, [string]$ExpectedSha = '') {
    Assert-PrepareOrdinary $SourcePath $false
    Assert-PrepareDestinationPath $TargetPath
    $inputFile = $null
    $outputFile = $null
    $hash = [Security.Cryptography.SHA256]::Create()
    $copied = [long]0
    $sourceSha = ''
    try {
        $inputFile = New-Object IO.FileStream($SourcePath, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read, $prepareBufferBytes, [IO.FileOptions]::SequentialScan)
        $originalBytes = $inputFile.Length
        $outputFile = New-Object IO.FileStream($TargetPath, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None, $prepareBufferBytes, [IO.FileOptions]::SequentialScan)
        $buffer = New-Object byte[] $prepareBufferBytes
        while (($count = $inputFile.Read($buffer, 0, $buffer.Length)) -ne 0) {
            $outputFile.Write($buffer, 0, $count)
            $null = $hash.TransformBlock($buffer, 0, $count, $buffer, 0)
            $copied += $count
        }
        $null = $hash.TransformFinalBlock([byte[]]@(), 0, 0)
        $sourceSha = ([BitConverter]::ToString($hash.Hash)).Replace('-', '')
        if ($copied -ne $originalBytes -or $inputFile.Length -ne $originalBytes) { throw "Source length changed while copying: $SourcePath" }
        if ($ExpectedSha -and $sourceSha -ne $ExpectedSha) { throw "Source identity changed or is unsupported: $SourcePath" }
        $outputFile.Flush($true)
    }
    finally {
        if ($null -ne $outputFile) { $outputFile.Dispose() }
        if ($null -ne $inputFile) { $inputFile.Dispose() }
        $hash.Dispose()
    }
    $destinationSha = Get-PrepareDigest $TargetPath
    $destinationBytes = (Get-Item -LiteralPath $TargetPath -Force).Length
    if ($sourceSha -ne $destinationSha -or $copied -ne $destinationBytes) { throw "Destination bytes differ from the source stream: $TargetPath" }
    return [pscustomobject][ordered]@{
        path = $RelativePath.Replace('\', '/')
        bytes = $copied
        sourceSha256 = $sourceSha
        destinationSha256 = $destinationSha
        verified = $true
        copyMethod = 'FileStream-CreateNew-physical'
    }
}

function Write-PrepareFreshText([string]$Path, [string]$Text, [Text.Encoding]$Encoding) {
    $bytes = $Encoding.GetBytes($Text)
    $stream = New-Object IO.FileStream($Path, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::Read)
    try { $stream.Write($bytes, 0, $bytes.Length); $stream.Flush($true) }
    finally { $stream.Dispose() }
}

$prepareSource = Get-PrepareFullPath $SourceGameRoot
$prepareDestination = Get-PrepareFullPath $Destination
Assert-PrepareOrdinary $prepareSource $true
Assert-PrepareAncestors $prepareSource
Assert-PrepareDestinationPath $prepareDestination
if ($prepareDestination.Length -gt $prepareMaximumRootLength) { throw "Destination root must be at most $prepareMaximumRootLength characters." }
if (Test-PrepareExists $prepareDestination) { throw "Destination must not exist, including an empty or failed partial tree: $prepareDestination" }
$prepareParent = [IO.Directory]::GetParent($prepareDestination)
if ($null -eq $prepareParent) { throw 'Destination must be a new child directory under an existing parent.' }
Assert-PrepareAncestors $prepareParent.FullName
$preparePhysicalSource = [WinxCoop.Preparation.NativePathsV1]::FinalDirectory($prepareSource).TrimEnd([char[]]'\/')
$preparePhysicalParent = [WinxCoop.Preparation.NativePathsV1]::FinalDirectory($prepareParent.FullName)
$preparePhysicalDestination = [IO.Path]::Combine($preparePhysicalParent, [IO.Path]::GetFileName($prepareDestination))
if ((Test-PrepareOverlap $prepareSource $prepareDestination) -or (Test-PrepareOverlap $preparePhysicalSource $preparePhysicalDestination)) { throw 'Source and destination must not overlap, including physical drive aliases.' }

$prepareSourceExe = Join-Path $prepareSource 'WinxClub.exe'
$prepareSourceMedia = Join-Path $prepareSource 'Media'
$prepareSourceShaders = Join-Path $prepareSource 'Shaders'
Assert-PrepareOrdinary $prepareSourceExe $false
Assert-PrepareOrdinary $prepareSourceMedia $true
Assert-PrepareOrdinary $prepareSourceShaders $true
Assert-PrepareOrdinary (Join-Path $prepareSourceShaders 'Fixed.rfx') $false
if ((Get-PrepareDigest $prepareSourceExe) -ne $prepareExeSha) { throw "Unsupported original WinxClub.exe SHA256; expected $prepareExeSha" }

$prepareRuntimeRoot = if ($RuntimeDirectory) { Get-PrepareFullPath $RuntimeDirectory } else { $prepareSource }
Assert-PrepareOrdinary $prepareRuntimeRoot $true
Assert-PrepareAncestors $prepareRuntimeRoot
$preparePhysicalRuntime = [WinxCoop.Preparation.NativePathsV1]::FinalDirectory($prepareRuntimeRoot).TrimEnd([char[]]'\/')
if ((Test-PrepareOverlap $prepareRuntimeRoot $prepareDestination) -or (Test-PrepareOverlap $preparePhysicalRuntime $preparePhysicalDestination)) { throw 'Runtime source and destination must not overlap.' }
foreach ($runtime in $prepareRuntimeFiles) {
    $runtimeSource = Join-Path $prepareRuntimeRoot $runtime.name
    Assert-PrepareOrdinary $runtimeSource $false
    if ((Get-Item -LiteralPath $runtimeSource -Force).Length -ne $runtime.bytes -or (Get-PrepareDigest $runtimeSource) -ne $runtime.sha256) { throw "Pinned CRT71 identity mismatch: $runtimeSource" }
    Assert-PrepareDestinationPath (Join-Path $prepareDestination $runtime.name)
}

# Inventory and all mandatory root dependency checks complete before any
# destination exists. Media and Shaders remain verbatim input data, including
# Saved and original developer utilities. Nothing from either tree is executed.
$prepareDirectories = New-Object 'System.Collections.Generic.List[string]'
$prepareFiles = New-Object 'System.Collections.Generic.List[object]'
$preparePending = New-Object 'System.Collections.Generic.Stack[object]'
$preparePending.Push([pscustomobject]@{ path = $prepareSourceMedia; relative = 'Media'; depth = 0 })
$preparePending.Push([pscustomobject]@{ path = $prepareSourceShaders; relative = 'Shaders'; depth = 0 })
$prepareTreeCounts = @{ Media = 0; Shaders = 0 }
while ($preparePending.Count -ne 0) {
    $folder = $preparePending.Pop()
    if ($folder.depth -gt $prepareMaximumDepth) { throw 'Resource tree exceeds the depth bound.' }
    Assert-PrepareOrdinary $folder.path $true
    $prepareDirectories.Add($folder.relative)
    foreach ($entry in Get-ChildItem -LiteralPath $folder.path -Force -ErrorAction Stop) {
        if (($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Reparse resource input refused: $($entry.FullName)" }
        $relative = $folder.relative + '\' + $entry.Name
        Assert-PrepareDestinationPath (Join-Path $prepareDestination $relative)
        if ($entry.PSIsContainer) {
            $preparePending.Push([pscustomobject]@{ path = $entry.FullName; relative = $relative; depth = $folder.depth + 1 })
        }
        else {
            if ($prepareFiles.Count -ge $prepareMaximumFiles) { throw 'Resource inventory exceeds 200000 files.' }
            $prepareFiles.Add([pscustomobject]@{ source = $entry.FullName; relative = $relative; bytes = [long]$entry.Length })
            $tree = $relative.Split('\')[0]
            $prepareTreeCounts[$tree]++
        }
    }
}
if ($prepareTreeCounts.Media -eq 0 -or $prepareTreeCounts.Shaders -eq 0) { throw 'Both Media and Shaders must contain files.' }
$prepareSourceSaved = Join-Path $prepareSourceMedia 'Saved'
$prepareSavedPresent = Test-PrepareExists $prepareSourceSaved
if ($prepareSavedPresent) { Assert-PrepareOrdinary $prepareSourceSaved $true }
$prepareSourceConfig = Join-Path $prepareSource 'config.ini'
$prepareConfigSourceSha = $null
$prepareConfigCopy = $false
if (Test-PrepareExists $prepareSourceConfig) {
    $prepareConfigSourceSha = Get-PrepareDigest $prepareSourceConfig
    $prepareConfigCopy = $prepareConfigSourceSha -eq $prepareOriginalConfigSha
}
$prepareEstimatedBytes = [long]0
foreach ($entry in $prepareFiles) { $prepareEstimatedBytes += $entry.bytes }
Write-Verbose "Read-only source: $prepareSource; Media+Shaders files=$($prepareFiles.Count), bytes=$prepareEstimatedBytes; CRT source=$prepareRuntimeRoot."
if (-not $PSCmdlet.ShouldProcess($prepareDestination, 'Create a NEW independent WinxCoop Alfea28 game and verify copied bytes')) { return }

$prepareCreated = $false
$prepareVerified = New-Object 'System.Collections.Generic.List[object]'
$prepareStarted = [DateTime]::UtcNow.ToString('o')
$prepareSettings = [ordered]@{
    showCinematics = 'false'; useGamePad = 'false'; fullScreen = 'false'
    enableSound = 'false'; enableShadows = 'false'; language = 'en'
    startLevel = '28'; testCinematic = 'false'; cinematicToTest = '0'
}
$prepareManifest = [ordered]@{
    kind = 'WinxCoop-isolated-prototype-preparation'; version = 2; completed = $false
    startedUtc = $prepareStarted; finishedUtc = $null; sourceGameRoot = $prepareSource; destinationGameRoot = $prepareDestination
    physicalSourceGameRoot = $preparePhysicalSource; physicalDestinationGameRoot = $preparePhysicalDestination
    sourceReadOnly = $true; sourceMutationPerformed = $false; sourceExecutableSha256 = $prepareExeSha
    runtimeDirectory = $prepareRuntimeRoot; physicalRuntimeDirectory = $preparePhysicalRuntime; pinnedRuntimeFiles = $prepareRuntimeFiles
    copiedTreeRoots = @('Media', 'Shaders'); resourceFileCounts = $prepareTreeCounts
    sourceMediaAssetIdentity = 'User-supplied Media; per-file copied bytes verified, no pristine whole-corpus claim'
    independentSaved = $false; savedMode = $(if ($prepareSavedPresent) { 'PhysicalCopyFromSource' } else { 'FreshEmptyDirectory' })
    linksCreated = 0; rootProxyDllsOrOtherExecutablesCopied = 0; winxIniSettings = $prepareSettings
    configMode = $(if ($prepareConfigCopy) { 'KnownOriginalByteCopy' } else { 'GeneratedInputDefault' })
    sourceConfigSha256 = $prepareConfigSourceSha; expectedOriginalConfigSha256 = $prepareOriginalConfigSha
    streamBufferBytes = $prepareBufferBytes; verifiedFiles = 0; verifiedBytes = [long]0
    generatedFiles = @(); files = @(); failure = $null
}
try {
    # Recheck immediately before the atomic create; no accept-existing or Force.
    Assert-PrepareAncestors $prepareParent.FullName
    [WinxCoop.Preparation.NativePathsV1]::CreateNewDirectory($prepareDestination)
    $prepareCreated = $true
    foreach ($relative in ($prepareDirectories | Sort-Object { $_.Length }, { $_ })) {
        [WinxCoop.Preparation.NativePathsV1]::CreateNewDirectory((Join-Path $prepareDestination $relative))
    }
    $prepareVerified.Add((Copy-PrepareVerified $prepareSourceExe (Join-Path $prepareDestination 'WinxClub.exe') 'WinxClub.exe' $prepareExeSha))
    foreach ($runtime in $prepareRuntimeFiles) {
        $prepareVerified.Add((Copy-PrepareVerified (Join-Path $prepareRuntimeRoot $runtime.name) (Join-Path $prepareDestination $runtime.name) $runtime.name $runtime.sha256))
    }
    $prepareFileNumber = 0
    foreach ($entry in ($prepareFiles | Sort-Object relative)) {
        $prepareFileNumber++
        Write-Progress -Activity 'Preparing isolated WinxCoop game' -Status "$prepareFileNumber / $($prepareFiles.Count): $($entry.relative)" -PercentComplete ([int](100.0 * $prepareFileNumber / $prepareFiles.Count))
        $prepareVerified.Add((Copy-PrepareVerified $entry.source (Join-Path $prepareDestination $entry.relative) $entry.relative))
    }
    Write-Progress -Activity 'Preparing isolated WinxCoop game' -Completed
    if (-not $prepareSavedPresent) { [WinxCoop.Preparation.NativePathsV1]::CreateNewDirectory((Join-Path $prepareDestination 'Media\Saved')) }
    $prepareIniLines = @($prepareSettings.GetEnumerator() | ForEach-Object { $_.Key + '=' + $_.Value })
    $prepareWinxIni = Join-Path $prepareDestination 'winx.ini'
    Write-PrepareFreshText $prepareWinxIni (($prepareIniLines -join "`r`n") + "`r`n") $prepareAscii
    $prepareGenerated = New-Object 'System.Collections.Generic.List[object]'
    $prepareGenerated.Add([pscustomobject]@{ path = 'winx.ini'; sha256 = (Get-PrepareDigest $prepareWinxIni); encoding = 'ASCII'; source = 'FixedStagePSettings' })
    $prepareManifest.generatedFiles = @($prepareGenerated.ToArray())
    $prepareConfig = Join-Path $prepareDestination 'config.ini'
    if ($prepareConfigCopy) { $prepareVerified.Add((Copy-PrepareVerified $prepareSourceConfig $prepareConfig 'config.ini' $prepareOriginalConfigSha)) }
    else {
        Write-PrepareFreshText $prepareConfig "[Input]`r`nmouse_exclusive=false`r`n" $prepareAscii
        $prepareGenerated.Add([pscustomobject]@{ path = 'config.ini'; sha256 = (Get-PrepareDigest $prepareConfig); encoding = 'ASCII'; source = 'DefaultInputSettings' })
    }
    $prepareManifest.independentSaved = $true
    $prepareManifest.verifiedFiles = $prepareVerified.Count
    foreach ($entry in $prepareVerified) { $prepareManifest.verifiedBytes += $entry.bytes }
    $prepareManifest.generatedFiles = @($prepareGenerated.ToArray())
    $prepareManifest.files = @($prepareVerified.ToArray())
    $prepareManifest.completed = $true
    $prepareManifest.finishedUtc = [DateTime]::UtcNow.ToString('o')
    $prepareManifestPath = Join-Path $prepareDestination 'WinxCoop-preparation.json'
    Write-PrepareFreshText $prepareManifestPath (($prepareManifest | ConvertTo-Json -Depth 8) + "`n") $prepareUtf8
    Write-Output "Prepared isolated Alfea28 game: $prepareDestination"
    Write-Output "Verified physical copies: $($prepareManifest.verifiedFiles) files, $($prepareManifest.verifiedBytes) bytes. Manifest: $prepareManifestPath"
}
catch {
    $prepareError = $_
    Write-Progress -Activity 'Preparing isolated WinxCoop game' -Completed
    if ($prepareCreated) {
        $prepareManifest.completed = $false
        $prepareManifest.failure = $prepareError.Exception.Message
        $prepareManifest.finishedUtc = [DateTime]::UtcNow.ToString('o')
        $prepareManifest.verifiedFiles = $prepareVerified.Count
        $prepareManifest.verifiedBytes = [long]0
        foreach ($entry in $prepareVerified) { $prepareManifest.verifiedBytes += $entry.bytes }
        $prepareManifest.files = @($prepareVerified.ToArray())
        try { Write-PrepareFreshText (Join-Path $prepareDestination 'WinxCoop-preparation.failed.json') (($prepareManifest | ConvertTo-Json -Depth 8) + "`n") $prepareUtf8 }
        catch { Write-Warning 'The failed preparation manifest could not be written.' }
        Write-Warning "The failed NEW tree is preserved: $prepareDestination. Choose a different, nonexistent destination for another attempt."
    }
    throw $prepareError
}
