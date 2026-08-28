[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath,

    [Parameter(Mandatory = $true)]
    [ValidateSet(4, 8, 16)]
    [int]$Offset,

    [Parameter(Mandatory = $true)]
    [uint32]$NewValue,

    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[0-9A-Fa-f]{64}$')]
    [string]$ExpectedInputSha256
)

$ErrorActionPreference = 'Stop'

$inputFullPath = [System.IO.Path]::GetFullPath($InputPath)
$outputFullPath = [System.IO.Path]::GetFullPath($OutputPath)

if ([string]::Equals(
        $inputFullPath,
        $outputFullPath,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'InputPath and OutputPath must be different.'
}

if (-not [System.IO.File]::Exists($inputFullPath)) {
    throw "Input SMO does not exist: $inputFullPath"
}

if ([System.IO.File]::Exists($outputFullPath)) {
    throw "Output already exists; refusing to overwrite it: $outputFullPath"
}

$actualInputSha256 = (Get-FileHash -LiteralPath $inputFullPath -Algorithm SHA256).Hash
if (-not [string]::Equals(
        $actualInputSha256,
        $ExpectedInputSha256,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Input SHA-256 mismatch: expected $ExpectedInputSha256, got $actualInputSha256"
}

$inputBytes = [System.IO.File]::ReadAllBytes($inputFullPath)
if ($inputBytes.Length -lt 0x20) {
    throw "Input is shorter than the fixed 0x20-byte SMO header: $($inputBytes.Length) bytes"
}

if ($inputBytes[0] -ne 0x46 -or
    $inputBytes[1] -ne 0x46 -or
    $inputBytes[2] -ne 0x50 -or
    $inputBytes[3] -ne 0x53) {
    throw 'Input does not begin with the FFPS signature.'
}

$oldValue = [System.BitConverter]::ToUInt32($inputBytes, $Offset)
if ($oldValue -eq $NewValue) {
    throw ('Requested value is already present at 0x{0:X2}: 0x{1:X8}' -f $Offset, $NewValue)
}

$outputBytes = [byte[]]$inputBytes.Clone()
$replacement = [System.BitConverter]::GetBytes($NewValue)
[System.Array]::Copy($replacement, 0, $outputBytes, $Offset, 4)

$changedOffsets = [System.Collections.Generic.List[int]]::new()
for ($index = 0; $index -lt $inputBytes.Length; $index++) {
    if ($inputBytes[$index] -ne $outputBytes[$index]) {
        $changedOffsets.Add($index)
    }
}

if ($changedOffsets.Count -lt 1 -or $changedOffsets.Count -gt 4) {
    throw "Expected 1..4 changed bytes, got $($changedOffsets.Count)."
}

foreach ($changedOffset in $changedOffsets) {
    if ($changedOffset -lt $Offset -or $changedOffset -ge ($Offset + 4)) {
        throw ('Unexpected byte change outside 0x{0:X2}..0x{1:X2}: 0x{2:X}' -f
            $Offset, ($Offset + 3), $changedOffset)
    }
}

$outputDirectory = [System.IO.Path]::GetDirectoryName($outputFullPath)
if (-not [System.IO.Directory]::Exists($outputDirectory)) {
    [System.IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
}

[System.IO.File]::WriteAllBytes($outputFullPath, $outputBytes)
$actualOutputSha256 = (Get-FileHash -LiteralPath $outputFullPath -Algorithm SHA256).Hash

[pscustomobject]@{
    inputPath = $inputFullPath
    outputPath = $outputFullPath
    inputSha256 = $actualInputSha256
    outputSha256 = $actualOutputSha256
    fileSize = $outputBytes.Length
    offset = ('0x{0:X2}' -f $Offset)
    oldValue = ('0x{0:X8}' -f $oldValue)
    newValue = ('0x{0:X8}' -f $NewValue)
    changedByteOffsets = @($changedOffsets | ForEach-Object { '0x{0:X}' -f $_ })
} | ConvertTo-Json -Depth 3
