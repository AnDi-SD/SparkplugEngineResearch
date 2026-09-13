param([switch]$InitializeDetours)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$reference = Join-Path $root 'local-data/rtx-remix/upstream/dxvk-remix'
$work = Join-Path $root 'local-data/rtx-remix/direct-camera-bridge-work'
$patch = Join-Path $PSScriptRoot 'camera-v1.patch'
$manifest = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if ((Get-FileHash -LiteralPath $patch -Algorithm SHA256).Hash -ne $manifest.patchSha256) { throw 'Patch hash mismatch' }
if (-not (Test-Path -LiteralPath $work)) {
    & git clone --shared --no-checkout $reference $work
    if ($LASTEXITCODE -ne 0) { throw 'Local clone failed' }
    & git -C $work checkout --detach $manifest.baseRevision
    if ($LASTEXITCODE -ne 0) { throw 'Base checkout failed' }
}
$base = & git -C $work rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $base -ne $manifest.baseRevision) { throw 'Worktree has an unexpected base revision' }
& git -C $work apply --reverse --check $patch 2>$null
if ($LASTEXITCODE -ne 0) {
    & git -C $work apply --check $patch
    if ($LASTEXITCODE -ne 0) { throw 'Patch does not apply; existing work was preserved' }
    & git -C $work apply $patch
    if ($LASTEXITCODE -ne 0) { throw 'Patch application failed' }
}
foreach ($entry in $manifest.patchedFiles) {
    $path = Join-Path $work $entry.path
    # Git's Windows checkout may use CRLF; compare the explicit canonical LF
    # source digest; detailed build-input provenance is stored privately.
    $bytes = [Text.Encoding]::UTF8.GetBytes([IO.File]::ReadAllText($path).Replace("`r`n","`n"))
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $hash = [BitConverter]::ToString($sha.ComputeHash($bytes)).Replace('-','') } finally { $sha.Dispose() }
    if ($hash -ne $entry.lfSha256) { throw "Patched file mismatch: $($entry.path)" }
}
if ($InitializeDetours) {
    & git -C $work submodule update --init --depth 1 -- submodules/Detours
    if ($LASTEXITCODE -ne 0) { throw 'Pinned Detours dependency preparation failed' }
}
Write-Output "Prepared camera bridge worktree: $work"
