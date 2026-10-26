param([Parameter(Mandatory=$true)][string]$Run, [int]$GamePid)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../..'))
$runs=[IO.Path]::GetFullPath((Join-Path $root 'local-data/rtx-remix/runs'))
$Run=[IO.Path]::GetFullPath($Run)
if (-not $Run.StartsWith($runs+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Only a local Remix diagnostic run may restore its config'
}
if ($GamePid) { Wait-Process -Id $GamePid -ErrorAction SilentlyContinue }
$state=Get-Content -LiteralPath (Join-Path $Run 'bridge-config-state.json') -Raw | ConvertFrom-Json
$target=Join-Path $root 'local-data/Winx Club/.trex/bridge.conf'
if ($state.targetPath) {
    $target=[IO.Path]::GetFullPath([string]$state.targetPath)
    if (-not $target.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or
        -not $target.Replace('\','/').EndsWith('/.trex/bridge.conf',[StringComparison]::OrdinalIgnoreCase)) {
        throw 'Recorded bridge config must be inside the selected workspace game directory'
    }
}
$backup=Join-Path $Run 'bridge.conf.before'
if ($state.existed) {
    # A legacy record without a pinned backup cannot establish safe undo bytes.
    if (-not $state.beforeSha256 -or -not (Test-Path -LiteralPath $backup)) {
        throw 'Bridge config backup is missing, changed or unverified; preserving the target'
    }
    $backupBytes=[IO.File]::ReadAllBytes($backup)
    $sha256=[Security.Cryptography.SHA256]::Create()
    try { $backupSha256=[BitConverter]::ToString($sha256.ComputeHash($backupBytes)).Replace('-','') } finally { $sha256.Dispose() }
    if ($backupSha256 -ne $state.beforeSha256) { throw 'Bridge config backup changed; preserving the target' }
}
$exists=Test-Path -LiteralPath $target
$currentSha256=$(if ($exists) { (Get-FileHash -LiteralPath $target).Hash } else { $null })
$alreadyOriginal=($state.existed -and $exists -and $currentSha256 -eq $state.beforeSha256) -or (-not $state.existed -and -not $exists)
if (-not $alreadyOriginal) {
    if (-not $exists -or $currentSha256 -ne $state.temporarySha256) {
        throw 'Bridge config changed during the test; preserving it for review'
    }
    if ($state.existed) {
        [IO.File]::WriteAllBytes($target,$backupBytes)
        if ((Get-FileHash -LiteralPath $target).Hash -ne $state.beforeSha256) { throw 'Restored bridge config does not match its backup' }
    } else {
        Remove-Item -LiteralPath $target
    }
}
@{restored=(Get-Date).ToString('o'); originallyExisted=$state.existed; targetPath=$target; alreadyOriginal=$alreadyOriginal;
  beforeSha256=$state.beforeSha256; temporarySha256=$state.temporarySha256} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Run 'bridge-config-restored.json') -Encoding UTF8
