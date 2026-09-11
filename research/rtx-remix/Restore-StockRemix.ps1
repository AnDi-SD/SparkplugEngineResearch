$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$game=Join-Path $root 'local-data/Winx Club'
if (Get-Process WinxClub,NvRemixBridge -ErrorAction SilentlyContinue) { throw 'Close the game and bridge first' }
$original=Join-Path $game 'd3d9.remix-original.dll'
if ((Get-FileHash -LiteralPath $original).Hash -ne 'A9D0846720E90D36D19AFB67E76A4D894EB349ECF13B847DE0CEDA4861669965') {
    throw 'The saved original differs from the verified Remix 1.5.2 client; inspect it before restoring'
}
$backup=Join-Path $root "local-data/rtx-remix/rollback/$(Get-Date -Format 'yyyyMMdd-HHmmss-fff')"
New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Item -LiteralPath (Join-Path $game 'd3d9.dll') -Destination (Join-Path $backup 'd3d9.dll')
Copy-Item -LiteralPath $original -Destination (Join-Path $game 'd3d9.dll')
Write-Output "Stock Remix client restored. Previous client saved in $backup"
