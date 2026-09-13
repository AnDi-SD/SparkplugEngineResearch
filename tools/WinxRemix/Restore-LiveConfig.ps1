param([Parameter(Mandatory=$true)][string]$Run, [int]$GamePid)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$runs=[IO.Path]::GetFullPath((Join-Path $root 'local-data/rtx-remix/runs'))
$Run=[IO.Path]::GetFullPath($Run)
if (-not $Run.StartsWith($runs+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Only a local Remix diagnostic run may restore its config'
}
if ($GamePid) { Wait-Process -Id $GamePid -ErrorAction SilentlyContinue }
$target=Join-Path $root 'local-data/Winx Club/.trex/bridge.conf'
$state=Get-Content -LiteralPath (Join-Path $Run 'bridge-config-state.json') -Raw | ConvertFrom-Json
if (-not (Test-Path -LiteralPath $target) -or (Get-FileHash -LiteralPath $target).Hash -ne $state.temporarySha256) {
    throw 'Bridge config changed during the test; preserving it for review'
}
if ($state.existed) {
    Copy-Item -LiteralPath (Join-Path $Run 'bridge.conf.before') -Destination $target
} else {
    Remove-Item -LiteralPath $target
}
@{restored=(Get-Date).ToString('o'); originallyExisted=$state.existed} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Run 'bridge-config-restored.json') -Encoding UTF8
