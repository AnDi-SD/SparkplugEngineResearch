param(
    [Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,
    [ValidateSet('system','remix')][string]$Backend = 'remix',
    [switch]$Raytracing,
    [switch]$NormalizeFVF,
    [switch]$ExplicitMipLevels,
    [switch]$ImmediateTextureUpload,
    [switch]$TextureReadback,
    [switch]$ResubmitTextures,
    [switch]$OrthographicUi,
    [switch]$FitWindow,
    [switch]$ViewportScale,
    [switch]$MenuBackground,
    [switch]$SkyLayers,
    [switch]$NoDrawTrace,
    [switch]$DebugMenu,
    [switch]$LiveConfig,
    [hashtable]$ConfigOverride = @{}
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$game = Join-Path $root 'local-data/Winx Club'
if (Get-Process WinxClub,WinxClubDebug,NvRemixBridge -ErrorAction SilentlyContinue) { throw 'Close the previous game/bridge before starting another run' }
$executable = Join-Path $game $(if ($DebugMenu) { 'WinxClubDebug.exe' } else { 'WinxClub.exe' })
if ($DebugMenu -and (Get-FileHash -LiteralPath $executable).Hash -ne 'C27EA9DB4228781A12A90AE808807D4AF1397A7E40DD8F5FFF28F3C87CC62CDB') {
    throw 'The existing debug executable differs from the documented F1-menu build'
}
$run = Join-Path $root "local-data/rtx-remix/runs/$Name"
if (Test-Path -LiteralPath $run) { throw 'Choose a fresh run name to preserve evidence' }
New-Item -ItemType Directory -Path $run | Out-Null
$config = Join-Path $run 'rtx.conf'
$text = [IO.File]::ReadAllText((Join-Path $game 'rtx.conf'))
$text = $text.Replace('rtx.useVertexCapture - True','rtx.useVertexCapture = True')
$text += "`r`nrtx.enableRaytracing = $($Raytracing.IsPresent.ToString())`r`n"
if ($ImmediateTextureUpload) { $text += "d3d9.evictManagedOnUnlock = True`r`n" }
foreach ($key in ($ConfigOverride.Keys | Sort-Object)) {
    if ($key -notmatch '^[a-zA-Z0-9_.]+$' -or "$($ConfigOverride[$key])" -match '[\r\n]') { throw 'Invalid config override' }
    $text += "$key = $($ConfigOverride[$key])`r`n"
}
[IO.File]::WriteAllText($config,$text,[Text.UTF8Encoding]::new($false))
Copy-Item -LiteralPath (Join-Path $game 'd3d9.dll') -Destination (Join-Path $run 'd3d9.dll')
foreach ($file in @('user.conf','winx.ini')) {
    Copy-Item -LiteralPath (Join-Path $game $file) -Destination (Join-Path $run "$file.before")
}
Copy-Item -LiteralPath (Join-Path $game 'Media/Saved') -Destination (Join-Path $run 'saves-before') -Recurse
if ($LiveConfig) {
    if ($Backend -ne 'remix') { throw 'Live config requires the Remix backend' }
    $bridgeConfig = Join-Path $game '.trex/bridge.conf'
    $bridgeExisted = Test-Path -LiteralPath $bridgeConfig
    $bridgeText = ''
    if ($bridgeExisted) {
        Copy-Item -LiteralPath $bridgeConfig -Destination (Join-Path $run 'bridge.conf.before')
        $bridgeText = [IO.File]::ReadAllText($bridgeConfig)
    }
    $bridgeText += "`r`nexposeRemixApi = True`r`n"
    [IO.File]::WriteAllText($bridgeConfig,$bridgeText,[Text.UTF8Encoding]::new($false))
    @{ existed=$bridgeExisted; temporarySha256=(Get-FileHash -LiteralPath $bridgeConfig).Hash } |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'bridge-config-state.json') -Encoding UTF8
    [IO.File]::WriteAllText((Join-Path $run 'live.conf'),'# Diagnostic settings only; restart to clear them.',[Text.UTF8Encoding]::new($false))
}
$savedEnvironment = @{}
$values = @{
    WINX_REMIX_BACKEND=$Backend
    WINX_REMIX_TRACE=$(if ($NoDrawTrace) { $null } else { Join-Path $run 'draws.jsonl' })
    WINX_REMIX_NORMALIZE_FVF=$(if ($NormalizeFVF) { '1' } else { '0' })
    WINX_REMIX_EXPLICIT_MIPS=$(if ($ExplicitMipLevels) { '1' } else { '0' })
    WINX_REMIX_TEXTURE_READBACK=$(if ($TextureReadback) { '1' } else { '0' })
    WINX_REMIX_RESUBMIT_TEXTURES=$(if ($ResubmitTextures) { '1' } else { '0' })
    WINX_REMIX_ORTHOGRAPHIC_UI=$(if ($OrthographicUi) { '1' } else { '0' })
    WINX_REMIX_FIT_WINDOW=$(if ($FitWindow) { '1' } else { '0' })
    WINX_REMIX_VIEWPORT_SCALE=$(if ($ViewportScale) { '1' } else { '0' })
    WINX_REMIX_MENU_BACKGROUND=$(if ($MenuBackground) { '1' } else { '0' })
    WINX_REMIX_SKY_LAYERS=$(if ($SkyLayers) { '1' } else { '0' })
    WINX_REMIX_LIVE_CONFIG=$(if ($LiveConfig) { Join-Path $run 'live.conf' } else { $null })
    DXVK_RTX_CONFIG_FILE=$config
}
try {
    foreach ($key in $values.Keys) {
        $savedEnvironment[$key]=[Environment]::GetEnvironmentVariable($key,'Process')
        [Environment]::SetEnvironmentVariable($key,$values[$key],'Process')
    }
    $gameProcess=Start-Process -FilePath $executable -WorkingDirectory $game -WindowStyle Normal -PassThru
    if ($LiveConfig) {
        $restoreScript = Join-Path $PSScriptRoot 'Restore-LiveConfig.ps1'
        Start-Process -FilePath powershell -WindowStyle Hidden -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$restoreScript+'"'),'-Run',('"'+$run+'"'),'-GamePid',$gameProcess.Id) | Out-Null
    }
    @{
        pid=$gameProcess.Id; started=(Get-Date).ToString('o'); environment=$values
        proxySha256=(Get-FileHash -LiteralPath (Join-Path $run 'd3d9.dll')).Hash
        executable=$executable; executableSha256=(Get-FileHash -LiteralPath $executable).Hash
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'launch.json') -Encoding UTF8
    Write-Output "PID=$($gameProcess.Id) Run=$run"
} finally {
    foreach ($key in $savedEnvironment.Keys) { [Environment]::SetEnvironmentVariable($key,$savedEnvironment[$key],'Process') }
}
