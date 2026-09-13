param([switch]$Offline)
$ErrorActionPreference = 'Stop'
$arguments = @((Join-Path $PSScriptRoot 'prepare_dependencies.py'))
if ($Offline) { $arguments += '--offline' }
$result = & python @arguments
if ($LASTEXITCODE -ne 0) { throw 'Winx Remix dependency preparation failed' }
$result
