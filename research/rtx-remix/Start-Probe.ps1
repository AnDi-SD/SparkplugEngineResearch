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
    [switch]$SkipLegacyProjectedShadows,
    [switch]$OpaqueAlphaTest,
    [switch]$NoDrawTrace,
    [switch]$DebugMenu,
    [switch]$LiveConfig,
    [switch]$ShaderAudit,
    [switch]$ShaderSemantics,
    [switch]$MaterialAudit,
    [switch]$MaterialChannels,
    [switch]$SceneAudit,
    [switch]$SceneLights,
    [switch]$SceneGeometry,
    [ValidateRange(0,1000)][float]$LightGain = 10,
    [switch]$Windowed,
    [switch]$SurfaceRoles,
    [switch]$AutoSurfaceRoles,
    [ValidateScript({ $_ -eq 0 -or ($_ -ge 1 -and $_ -le 37) -or ($_ -ge 41 -and $_ -le 49) })][int]$StartLevel = 0,
    [hashtable]$ConfigOverride = @{}
)
$ErrorActionPreference = 'Stop'
if ($MaterialChannels -and (-not $AutoSurfaceRoles -or -not $SceneLights)) { throw 'MaterialChannels requires AutoSurfaceRoles and SceneLights' }
if ($ShaderSemantics -and -not $DebugMenu) { throw 'Shader semantics require the hash-verified DebugMenu executable' }
if ($ShaderSemantics) { $ShaderAudit=$true; $MaterialAudit=$true }
if ($Windowed -and -not $StartLevel) { throw 'Windowed override requires an isolated StartLevel run' }
if ($SceneAudit -and -not $DebugMenu) { throw 'Scene audit requires the hash-verified DebugMenu executable' }
if ($SceneLights -and (-not $DebugMenu -or $Backend -ne 'remix' -or -not $Raytracing)) { throw 'Scene lights require RTX and the hash-verified DebugMenu executable' }
if ($SceneGeometry -and (-not $DebugMenu -or $Backend -ne 'remix' -or -not $Raytracing)) { throw 'Scene geometry requires RTX and the hash-verified DebugMenu executable' }
if ($SkipLegacyProjectedShadows -and ($Backend -ne 'remix' -or -not $Raytracing)) { throw 'Legacy shadow filtering requires the RTX mode' }
if ($OpaqueAlphaTest -and ($Backend -ne 'remix' -or -not $Raytracing)) { throw 'Opaque alpha normalization requires the RTX mode' }
if ($SurfaceRoles -and ($Backend -ne 'remix' -or -not $Raytracing)) { throw 'Surface roles require the RTX backend' }
if ($AutoSurfaceRoles -and ($Backend -ne 'remix' -or -not $Raytracing -or $SurfaceRoles -or -not $OpaqueAlphaTest)) { throw 'Automatic surface roles require RTX and OpaqueAlphaTest, without legacy SurfaceRoles' }
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
if ($AutoSurfaceRoles) { New-Item -ItemType Directory -Path (Join-Path $run 'surface-assets') | Out-Null }
$config = Join-Path $run 'rtx.conf'
$text = [IO.File]::ReadAllText((Join-Path $game 'rtx.conf'))
$text = $text.Replace('rtx.useVertexCapture - True','rtx.useVertexCapture = True')
$text += "`r`nrtx.enableRaytracing = $($Raytracing.IsPresent.ToString())`r`n"
if ($ImmediateTextureUpload) { $text += "d3d9.evictManagedOnUnlock = True`r`n" }
foreach ($key in ($ConfigOverride.Keys | Sort-Object)) {
    if ($key -notmatch '^[a-zA-Z0-9_.]+$' -or "$($ConfigOverride[$key])" -match '[\r\n]') { throw 'Invalid config override' }
    $text += "$key = $($ConfigOverride[$key])`r`n"
}
if ($AutoSurfaceRoles) {
    # The adapter owns these categories per draw. No persistent texture lists.
    $text += "`r`nrtx.decalTextures = `r`nrtx.dynamicDecalTextures = `r`nrtx.singleOffsetDecalTextures = `r`nrtx.nonOffsetDecalTextures = `r`nrtx.useObsoleteHashOnTextureUpload = False`r`n"
}
if ($SceneLights) {
    # This mode owns legacy/API routing. Begin on the legacy path until a
    # complete validated native registry has been submitted successfully.
    $text += "`r`nrtx.ignoreGameDirectionalLights = False`r`nrtx.ignoreGamePointLights = False`r`nrtx.ignoreGameSpotLights = False`r`n"
}
[IO.File]::WriteAllText($config,$text,[Text.UTF8Encoding]::new($false))
Copy-Item -LiteralPath (Join-Path $game 'd3d9.dll') -Destination (Join-Path $run 'd3d9.dll')
foreach ($file in @('user.conf','winx.ini')) {
    Copy-Item -LiteralPath (Join-Path $game $file) -Destination (Join-Path $run "$file.before")
}
Copy-Item -LiteralPath (Join-Path $game 'Media/Saved') -Destination (Join-Path $run 'saves-before') -Recurse
$workingDirectory = $game
if ($StartLevel) {
    # Own isolated test workspace; the game's existing startLevel configuration
    # route is also used by NativeLaunchWorkspace. Preserve the installed INI.
    $workingDirectory = Join-Path $run 'workdir'
    New-Item -ItemType Directory -Path $workingDirectory | Out-Null
    $ini = [IO.File]::ReadAllText((Join-Path $game 'winx.ini'))
    $ini = [regex]::Replace($ini, '(?im)^\s*(startLevel|showCinematics)\s*=.*$', '')
    $ini += "`r`nshowCinematics=false`r`nstartLevel=$StartLevel`r`n"
    if ($Windowed) {
        $ini = [regex]::Replace($ini, '(?im)^\s*fullScreen\s*=.*$', '')
        $ini += "fullScreen=false`r`n"
    }
    [IO.File]::WriteAllText((Join-Path $workingDirectory 'winx.ini'), $ini, [Text.UTF8Encoding]::new($false))
    Copy-Item -LiteralPath (Join-Path $game 'Shaders') -Destination (Join-Path $workingDirectory 'Shaders') -Recurse
    Copy-Item -LiteralPath (Join-Path $game 'user.conf') -Destination (Join-Path $workingDirectory 'user.conf')
}
if ($SurfaceRoles) {
    $surfaceRoleSource = Join-Path $PSScriptRoot 'surface-roles/mod.usda'
    $surfaceRoleDirectory = Join-Path $workingDirectory 'rtx-remix/mods/winx-surface-roles'
    New-Item -ItemType Directory -Force -Path $surfaceRoleDirectory | Out-Null
    Copy-Item -LiteralPath $surfaceRoleSource -Destination (Join-Path $surfaceRoleDirectory 'mod.usda')
}
if ($AutoSurfaceRoles) {
    $legacyRoleFile = Join-Path $workingDirectory 'rtx-remix/mods/winx-surface-roles/mod.usda'
    if (Test-Path -LiteralPath $legacyRoleFile) {
        $ownedRoleSource = Join-Path $PSScriptRoot 'surface-roles/mod.usda'
        if ((Get-FileHash -LiteralPath $legacyRoleFile).Hash -ne (Get-FileHash -LiteralPath $ownedRoleSource).Hash) {
            throw 'Legacy surface-role mod was modified; preserve it and disable it explicitly before using automatic roles'
        }
        Copy-Item -LiteralPath $legacyRoleFile -Destination (Join-Path $run 'legacy-surface-roles.usda.before')
        Remove-Item -LiteralPath $legacyRoleFile
    }
}
if ($ShaderAudit) {
    New-Item -ItemType Directory -Path (Join-Path $run 'shaders-client'),(Join-Path $run 'shaders-server') | Out-Null
}
if ($LiveConfig -or $AutoSurfaceRoles -or $SceneLights) {
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
# Record the effective bridge configuration as well as renderer settings. This
# is also needed for ordinary runs, without the diagnostic LiveConfig switch.
$bridgeConfigSha256 = $null
if ($Backend -eq 'remix') {
    $activeBridgeConfig = Join-Path $game '.trex/bridge.conf'
    if (Test-Path -LiteralPath $activeBridgeConfig) {
        Copy-Item -LiteralPath $activeBridgeConfig -Destination (Join-Path $run 'bridge.conf')
        $bridgeConfigSha256 = (Get-FileHash -LiteralPath (Join-Path $run 'bridge.conf')).Hash
    }
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
    WINX_REMIX_SKIP_LEGACY_PROJECTED_SHADOWS=$(if ($SkipLegacyProjectedShadows) { '1' } else { '0' })
    WINX_REMIX_OPAQUE_ALPHA_TEST=$(if ($OpaqueAlphaTest) { '1' } else { '0' })
    WINX_REMIX_AUTO_SURFACE_ROLES=$(if ($AutoSurfaceRoles) { '1' } else { '0' })
    WINX_REMIX_MATERIAL_CHANNELS=$(if ($MaterialChannels) { '1' } else { '0' })
    WINX_REMIX_SURFACE_AUDIT=$(if ($AutoSurfaceRoles) { Join-Path $run 'surface-roles.jsonl' } else { $null })
    WINX_REMIX_SURFACE_ASSETS=$(if ($AutoSurfaceRoles) { Join-Path $run 'surface-assets' } else { $null })
    WINX_REMIX_LIVE_CONFIG=$(if ($LiveConfig) { Join-Path $run 'live.conf' } else { $null })
    WINX_REMIX_SHADER_AUDIT=$(if ($ShaderAudit) { Join-Path $run 'shaders-client' } else { $null })
    WINX_REMIX_SHADER_SEMANTICS=$(if ($ShaderSemantics) { Join-Path $run 'shader-semantics.jsonl' } else { $null })
    WINX_REMIX_MATERIAL_AUDIT=$(if ($MaterialAudit) { Join-Path $run 'materials.jsonl' } else { $null })
    WINX_REMIX_SCENE_AUDIT=$(if ($SceneAudit) { Join-Path $run 'scene-audit.jsonl' } else { $null })
    WINX_REMIX_SCENE_LIGHTS=$(if ($SceneLights) { '1' } else { '0' })
    WINX_REMIX_SCENE_GEOMETRY=$(if ($SceneGeometry) { '1' } else { '0' })
    WINX_REMIX_GEOMETRY_AUDIT=$(if ($SceneGeometry) { Join-Path $run 'scene-geometry.jsonl' } else { $null })
    WINX_REMIX_LIGHT_GAIN=$LightGain.ToString([Globalization.CultureInfo]::InvariantCulture)
    WINX_REMIX_LIGHT_AUDIT=$(if ($SceneLights) { Join-Path $run 'scene-lights.jsonl' } else { $null })
    DXVK_SHADER_DUMP_PATH=$(if ($ShaderAudit -and $Backend -eq 'remix') { Join-Path $run 'shaders-server' } else { $null })
    DXVK_RTX_CONFIG_FILE=$config
}
try {
    foreach ($key in $values.Keys) {
        $savedEnvironment[$key]=[Environment]::GetEnvironmentVariable($key,'Process')
        [Environment]::SetEnvironmentVariable($key,$values[$key],'Process')
    }
    $gameProcess=Start-Process -FilePath $executable -WorkingDirectory $workingDirectory -WindowStyle Normal -PassThru
    if ($LiveConfig -or $AutoSurfaceRoles -or $SceneLights) {
        $restoreScript = Join-Path $PSScriptRoot 'Restore-LiveConfig.ps1'
        Start-Process -FilePath powershell -WindowStyle Hidden -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$restoreScript+'"'),'-Run',('"'+$run+'"'),'-GamePid',$gameProcess.Id) | Out-Null
    }
    @{
        pid=$gameProcess.Id; started=(Get-Date).ToString('o'); environment=$values
        proxySha256=(Get-FileHash -LiteralPath (Join-Path $run 'd3d9.dll')).Hash
        executable=$executable; executableSha256=(Get-FileHash -LiteralPath $executable).Hash
        workingDirectory=$workingDirectory; startLevel=$StartLevel
        windowedOverride=$Windowed.IsPresent
        surfaceRolesSha256=$(if ($SurfaceRoles) { (Get-FileHash -LiteralPath $surfaceRoleSource).Hash } else { $null })
        autoSurfaceRoles=$AutoSurfaceRoles.IsPresent
        sceneAudit=$SceneAudit.IsPresent
        materialAudit=$MaterialAudit.IsPresent
        materialChannels=$MaterialChannels.IsPresent
        shaderSemantics=$ShaderSemantics.IsPresent
        sceneLights=$SceneLights.IsPresent
        sceneGeometry=$SceneGeometry.IsPresent
        sceneLightGain=$LightGain
        bridgeConfigSha256=$bridgeConfigSha256
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $run 'launch.json') -Encoding UTF8
    Write-Output "PID=$($gameProcess.Id) Run=$run"
} finally {
    foreach ($key in $savedEnvironment.Keys) { [Environment]::SetEnvironmentVariable($key,$savedEnvironment[$key],'Process') }
}
