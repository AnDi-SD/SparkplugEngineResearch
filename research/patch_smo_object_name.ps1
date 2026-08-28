[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $InputPath,

    [Parameter(Mandatory = $true)]
    [string] $OutputPath,

    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[0-9A-Fa-f]{64}$')]
    [string] $ExpectedInputSha256,

    [Parameter(Mandatory = $true)]
    [ValidateRange(0, 2147483647)]
    [int] $ObjectIndex,

    [Parameter(Mandatory = $true)]
    [string] $ExpectedName,

    [Parameter(Mandatory = $true)]
    [string] $NewName
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if (-not [BitConverter]::IsLittleEndian) {
    throw 'This helper expects a little-endian host.'
}

$resolvedInput = (Resolve-Path -LiteralPath $InputPath).Path
$fullOutput = [IO.Path]::GetFullPath($OutputPath)
if ([StringComparer]::OrdinalIgnoreCase.Equals($resolvedInput, $fullOutput)) {
    throw 'Input and output paths must be different.'
}
if ([IO.File]::Exists($fullOutput) -or [IO.Directory]::Exists($fullOutput)) {
    throw "Output already exists: $fullOutput"
}

$bytes = [IO.File]::ReadAllBytes($resolvedInput)
$actualHash = (Get-FileHash -LiteralPath $resolvedInput -Algorithm SHA256).Hash
if (-not [StringComparer]::OrdinalIgnoreCase.Equals(
        $actualHash, $ExpectedInputSha256)) {
    throw "Input SHA-256 mismatch: expected $ExpectedInputSha256, got $actualHash"
}
if ($bytes.Length -lt 0x20 -or
    -not [Text.Encoding]::ASCII.GetString($bytes, 0, 4).Equals('FFPS')) {
    throw 'Input is not an FFPS container.'
}

$objectCount = [BitConverter]::ToUInt32($bytes, 0x1C)
$dataStart = [BitConverter]::ToUInt32($bytes, 0x14)
if ($ObjectIndex -ge $objectCount) {
    throw "Object index $ObjectIndex is outside the declared count $objectCount."
}
if ($dataStart -gt $bytes.Length -or $dataStart -lt 0x24) {
    throw ('Invalid data-start value 0x{0:X8}.' -f $dataStart)
}

$offset = 0x20
$targetNameOffset = -1
$targetNameLength = -1
$storedName = $null
for ($index = 0; $index -lt $objectCount; $index++) {
    if ($offset + 6 -gt $dataStart - 4) {
        throw "Object table is truncated before entry $index."
    }
    $nameLength = [BitConverter]::ToUInt16($bytes, $offset + 4)
    $entryLength = 6 + $nameLength + 12
    if ($offset + $entryLength -gt $dataStart - 4) {
        throw "Object table entry $index crosses the table terminator."
    }
    if ($index -eq $ObjectIndex) {
        if ($nameLength -eq 0 -or $bytes[$offset + 6 + $nameLength - 1] -ne 0) {
            throw "Object $ObjectIndex does not have a NUL-terminated directory name."
        }
        $targetNameOffset = $offset + 6
        $targetNameLength = $nameLength - 1
        $storedName = [Text.Encoding]::ASCII.GetString(
            $bytes, $targetNameOffset, $targetNameLength)
    }
    $offset += $entryLength
}
if ($offset -ne $dataStart - 4 -or [BitConverter]::ToUInt32($bytes, $offset) -ne 0) {
    throw 'Object table does not end at the declared zero terminator.'
}
if (-not [StringComparer]::Ordinal.Equals($storedName, $ExpectedName)) {
    throw "Object $ObjectIndex name mismatch: expected '$ExpectedName', got '$storedName'."
}

$expectedBytes = [Text.Encoding]::ASCII.GetBytes($ExpectedName)
$newBytes = [Text.Encoding]::ASCII.GetBytes($NewName)
if ($expectedBytes.Length -ne $ExpectedName.Length -or
    $newBytes.Length -ne $NewName.Length) {
    throw 'Only 7-bit ASCII names are accepted by this controlled helper.'
}
if ($expectedBytes.Length -ne $targetNameLength -or
    $newBytes.Length -ne $targetNameLength) {
    throw 'The replacement must have exactly the same byte length as the stored name.'
}
if ([Convert]::ToBase64String($expectedBytes) -eq
    [Convert]::ToBase64String($newBytes)) {
    throw 'ExpectedName and NewName encode to identical bytes.'
}

$mutated = [byte[]] $bytes.Clone()
[Array]::Copy($newBytes, 0, $mutated, $targetNameOffset, $newBytes.Length)
$differences = [Collections.Generic.List[int]]::new()
for ($index = 0; $index -lt $bytes.Length; $index++) {
    if ($bytes[$index] -ne $mutated[$index]) {
        $differences.Add($index)
        if ($index -lt $targetNameOffset -or
            $index -ge $targetNameOffset + $targetNameLength) {
            throw ('Unexpected mutation outside the target name at 0x{0:X}.' -f $index)
        }
    }
}
if ($differences.Count -eq 0) {
    throw 'Mutation produced no byte differences.'
}

$outputDirectory = [IO.Path]::GetDirectoryName($fullOutput)
if ([String]::IsNullOrEmpty($outputDirectory)) {
    throw 'Output path must include a parent directory.'
}
[IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
[IO.File]::WriteAllBytes($fullOutput, $mutated)
$outputHash = (Get-FileHash -LiteralPath $fullOutput -Algorithm SHA256).Hash

[pscustomobject]@{
    InputPath = $resolvedInput
    OutputPath = $fullOutput
    InputSha256 = $actualHash
    OutputSha256 = $outputHash
    ObjectIndex = $ObjectIndex
    OldName = $ExpectedName
    NewName = $NewName
    NameOffset = ('0x{0:X}' -f $targetNameOffset)
    ChangedByteCount = $differences.Count
    ChangedOffsets = @($differences | ForEach-Object { '0x{0:X}' -f $_ })
}
