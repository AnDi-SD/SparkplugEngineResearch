param(
    [ValidateSet('RTX','Raster','Original')][string]$Mode='RTX',
    [switch]$DebugMenu,
    [switch]$ShaderAudit,
    [switch]$ShaderSemantics,
    [switch]$MaterialAudit,
    [switch]$Windowed,
    [switch]$AutoSurfaceRoles,
    [switch]$SceneLights,
    [switch]$SceneGeometry,
    [ValidateScript({ $_ -eq 0 -or ($_ -ge 1 -and $_ -le 37) -or ($_ -ge 41 -and $_ -le 49) })][int]$StartLevel=0
)
$ErrorActionPreference='Stop'
if ($AutoSurfaceRoles -and $Mode -ne 'RTX') { throw 'AutoSurfaceRoles requires RTX mode' }
if ($SceneLights -and ($Mode -ne 'RTX' -or -not $DebugMenu)) { throw 'SceneLights requires RTX and the verified DebugMenu build' }
if ($SceneGeometry -and ($Mode -ne 'RTX' -or -not $DebugMenu)) { throw 'SceneGeometry requires RTX and the verified DebugMenu build' }
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$game=Join-Path $root 'local-data/Winx Club'
$build=Join-Path $root 'local-data/rtx-remix/build/d3d9.dll'
if (-not (Test-Path -LiteralPath $build)) { throw 'Build the adapter with Build-Probe.ps1 first' }
if ((Get-FileHash -LiteralPath (Join-Path $game 'd3d9.dll')).Hash -ne (Get-FileHash -LiteralPath $build).Hash) {
    throw 'The installed adapter differs from the current build. Install/review it before running this profile.'
}
$name="play-$($Mode.ToLowerInvariant())-$(Get-Date -Format 'yyyyMMdd-HHmmss-fff')"
$options=@{ Name=$name; NoDrawTrace=$true }
if ($DebugMenu) { $options.DebugMenu=$true }
if ($ShaderAudit) { $options.ShaderAudit=$true }
if ($ShaderSemantics) { $options.ShaderSemantics=$true }
if ($MaterialAudit) { $options.MaterialAudit=$true }
if ($SceneLights) { $options.SceneLights=$true }
if ($SceneGeometry) { $options.SceneGeometry=$true }
if ($StartLevel) { $options.StartLevel=$StartLevel }
if ($Windowed) { $options.Windowed=$true }
if ($Mode -eq 'Original') {
    $options.Backend='system'
} else {
    $options.ExplicitMipLevels=$true
    $options.ResubmitTextures=$true
    $options.FitWindow=$true
    $options.ViewportScale=$true
    if ($Mode -eq 'RTX') {
        $options.Raytracing=$true
        $options.OrthographicUi=$true
        $options.MenuBackground=$true
        $options.SkyLayers=$true
        $options.SkipLegacyProjectedShadows=$true
        $options.OpaqueAlphaTest=$true
        if ($AutoSurfaceRoles -or $SceneLights -or $SceneGeometry) { $options.AutoSurfaceRoles=$true }
        else { $options.SurfaceRoles=$true }
        # Visual tuning for original Winx vertex colors, not recovered game constants.
        $options.ConfigOverride=@{
            'rtx.vertexColorIsBakedLighting'='False'
            'rtx.lightConversionIntensityFactor'='10'
            'rtx.lightConversionDistantLightFixedIntensity'='10'
            'rtx.localtonemap.exposure'='1'
            'rtx.localtonemap.shadows'='5'
        }
        # Historical comparison profile until stock USD capture supports API materials.
        if (-not ($AutoSurfaceRoles -or $SceneLights -or $SceneGeometry)) { $options.ConfigOverride['rtx.decalTextures']='0xFAC245110A8BD959, 0x3323174FD6FAE171' }
    }
}
& (Join-Path $PSScriptRoot 'Start-Probe.ps1') @options
