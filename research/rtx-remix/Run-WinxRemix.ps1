param([ValidateSet('RTX','Raster','Original')][string]$Mode='RTX', [switch]$DebugMenu, [switch]$ShaderAudit)
$ErrorActionPreference='Stop'
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
        # Visual tuning for original Winx vertex colors, not recovered game constants.
        $options.ConfigOverride=@{
            # Gardenia terrain overlays are coplanar with their opaque base.
            'rtx.decalTextures'='0xFAC245110A8BD959, 0x3323174FD6FAE171'
            'rtx.vertexColorIsBakedLighting'='False'
            'rtx.lightConversionIntensityFactor'='10'
            'rtx.lightConversionDistantLightFixedIntensity'='10'
            'rtx.localtonemap.exposure'='1'
            'rtx.localtonemap.shadows'='5'
        }
    }
}
& (Join-Path $PSScriptRoot 'Start-Probe.ps1') @options
