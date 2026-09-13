param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$game=Join-Path $root 'local-data/Winx Club'
$source=Join-Path $root 'local-data/rtx-remix/server-instance-audit/server-v2/NvRemixBridge.exe'
$installed=Join-Path $game '.trex/NvRemixBridge.exe'
$backup=Join-Path $root "local-data/rtx-remix/server-instance-audit/install/$Name"
$expected='D7FEE509F46B63F2202E563E30C519FA18D47373E044563213C7AAC4DB6437DC'
$previous='C9D3E807CA36BFF6D3437D4DA3B8BB68DEED0EBEDDECC2E32B6E9D5547FD1927'
if (Get-Process WinxClub,WinxClubDebug,NvRemixBridge -ErrorAction SilentlyContinue) { throw 'Game/bridge is running' }
if (Test-Path -LiteralPath $backup) { throw 'Fresh backup directory required' }
if ((Get-FileHash -LiteralPath $source).Hash -ne $expected) { throw 'Audit build changed' }
if ((Get-FileHash -LiteralPath $installed).Hash -ne $previous) { throw 'Unexpected installed server' }
$client=Join-Path $game 'd3d9.remix-original.dll'
$renderer=Join-Path $game '.trex/d3d9.dll'
if ((Get-FileHash -LiteralPath $client).Hash -ne '79E88A694D233112605E7AB0F4F258F1FF536AD8471F67623C412DDAD6564F11') { throw 'Unexpected camera client' }
if ((Get-FileHash -LiteralPath $renderer).Hash -ne 'F7C310821AA98BCDFDEC120330B0A89457B7C5EBA58D21464AF32639611C809F') { throw 'Unexpected renderer' }
New-Item -ItemType Directory -Path $backup | Out-Null
Copy-Item -LiteralPath $installed -Destination (Join-Path $backup 'NvRemixBridge-before.exe')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'manifest.json') -Destination (Join-Path $backup 'build-manifest.json')
Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $backup 'Install-ServerAudit.ps1')
Copy-Item -LiteralPath $source -Destination $installed -Force
if ((Get-FileHash -LiteralPath $installed).Hash -ne $expected) { throw 'Installed server mismatch' }
@{schema=1;installedAt=(Get-Date).ToString('o');previousSha256=$previous;installedSha256=$expected;
  source=$source;destination=$installed;clientSha256=(Get-FileHash -LiteralPath $client).Hash;
  rendererSha256=(Get-FileHash -LiteralPath $renderer).Hash;
  scope='Optional bounded x64 renderer API return/order audit; camera protocol and stock renderer unchanged; no GPU execution implied'} |
  ConvertTo-Json | Set-Content -LiteralPath (Join-Path $backup 'installation.json') -Encoding UTF8
Get-Content -LiteralPath (Join-Path $backup 'installation.json')
