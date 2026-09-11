param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,
    [ValidateSet('system','remix')][string]$Backend = 'remix',
    [switch]$Raytracing,
    [switch]$NormalizeFVF
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$game = Join-Path $root 'local-data/Winx Club'
if (Get-Process WinxClub,NvRemixBridge -ErrorAction SilentlyContinue) { throw 'Close the previous game/bridge before starting another run' }
$run = Join-Path $root "local-data/rtx-remix/runs/$Name"
if (Test-Path -LiteralPath $run) { throw 'Choose a fresh run name to preserve evidence' }
New-Item -ItemType Directory -Path $run | Out-Null
$config = Join-Path $run 'rtx.conf'
$text = [IO.File]::ReadAllText((Join-Path $game 'rtx.conf'))
$text = $text.Replace('rtx.useVertexCapture - True','rtx.useVertexCapture = True')
$text += "`r`nrtx.enableRaytracing = $($Raytracing.IsPresent.ToString())`r`n"
[IO.File]::WriteAllText($config,$text,[Text.UTF8Encoding]::new($false))
Copy-Item -LiteralPath (Join-Path $game 'd3d9.dll') -Destination (Join-Path $run 'd3d9.dll')
foreach ($file in @('user.conf','winx.ini')) {
    Copy-Item -LiteralPath (Join-Path $game $file) -Destination (Join-Path $run "$file.before")
}
$savedEnvironment = @{}
$values = @{
    WINX_REMIX_BACKEND=$Backend
    WINX_REMIX_TRACE=(Join-Path $run 'draws.jsonl')
    WINX_REMIX_NORMALIZE_FVF=$(if ($NormalizeFVF) { '1' } else { '0' })
    DXVK_RTX_CONFIG_FILE=$config
}
try {
    foreach ($key in $values.Keys) {
        $savedEnvironment[$key]=[Environment]::GetEnvironmentVariable($key,'Process')
        [Environment]::SetEnvironmentVariable($key,$values[$key],'Process')
    }
    $gameProcess=Start-Process -FilePath (Join-Path $game 'WinxClub.exe') -WorkingDirectory $game -WindowStyle Normal -PassThru
    @{
        pid=$gameProcess.Id; started=(Get-Date).ToString('o'); environment=$values
        proxySha256=(Get-FileHash -LiteralPath (Join-Path $run 'd3d9.dll')).Hash
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'launch.json') -Encoding UTF8
    Write-Output "PID=$($gameProcess.Id) Run=$run"
} finally {
    foreach ($key in $savedEnvironment.Keys) { [Environment]::SetEnvironmentVariable($key,$savedEnvironment[$key],'Process') }
}
