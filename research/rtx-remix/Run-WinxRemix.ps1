param([ValidateSet('RTX','Raster','Original')][string]$Mode='RTX')
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
if ($Mode -eq 'Original') {
    $options.Backend='system'
} else {
    $options.ExplicitMipLevels=$true
    $options.ResubmitTextures=$true
    if ($Mode -eq 'RTX') {
        $options.Raytracing=$true
        $options.OrthographicUi=$true
    }
}
& (Join-Path $PSScriptRoot 'Start-Probe.ps1') @options
