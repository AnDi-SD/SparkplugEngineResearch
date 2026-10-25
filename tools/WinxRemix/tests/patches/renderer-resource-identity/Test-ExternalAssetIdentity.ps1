param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,
      [Parameter(Mandatory=$true)][string]$RendererSource,
      [ValidateSet('x86','x64')][string]$Platform='x86',
      [ValidateSet('Identity','Capture','Draw')][string]$Suite='Identity')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../../..'))
$RendererSource=(Resolve-Path -LiteralPath $RendererSource).Path
$rendererHeader=Join-Path $RendererSource 'src/dxvk/rtx_render/rtx_external_asset_identity.h'
if(-not (Test-Path -LiteralPath $rendererHeader -PathType Leaf)){throw 'Applied production renderer identity header is required'}
$memory=& (Join-Path $PSScriptRoot '../../../scripts/runtime/Get-SystemMemory.ps1')
if($memory.availablePhysicalBytes -lt 512MB){throw 'The CPU fixture build requires 512 MiB of available physical memory'}
$build=Join-Path $root "artifacts/WinxRemix/external-asset-identity-tests/$Name"
if(Test-Path -LiteralPath $build){throw 'Use a fresh evidence directory'}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $installation){throw 'MSVC tools not found'}
$environmentScript=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$snapshot=Join-Path $build 'source'
$relativeInputs=@(
    'tools/WinxRemix/tests/patches/renderer-resource-identity/test_external_asset_identity.cpp',
    'tools/WinxRemix/tests/patches/renderer-resource-identity/Test-ExternalAssetIdentity.ps1',
    'tools/WinxRemix/scripts/runtime/Get-SystemMemory.ps1'
)
if($Suite -eq 'Capture'){
    $relativeInputs+='tools/WinxRemix/tests/patches/renderer-resource-identity/test_capture_identity.cpp'
    $relativeInputs+='tools/WinxRemix/tests/patches/renderer-resource-identity/prepare_capture_fixture.py'
}elseif($Suite -eq 'Draw'){
    $relativeInputs+='tools/WinxRemix/tests/patches/renderer-resource-identity/test_external_draw.cpp'
    $relativeInputs+='tools/WinxRemix/tests/patches/renderer-resource-identity/prepare_draw_fixture.py'
}
foreach($relative in $relativeInputs){
    $destination=Join-Path $snapshot $relative
    New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($destination)) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $root $relative) -Destination $destination
}
$headerCopy=Join-Path $snapshot 'renderer/src/dxvk/rtx_render/rtx_external_asset_identity.h'
New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($headerCopy)) -Force | Out-Null
Copy-Item -LiteralPath $rendererHeader -Destination $headerCopy
$productionHeaderSha256=(Get-FileHash -LiteralPath $rendererHeader).Hash
if((Get-FileHash -LiteralPath $headerCopy).Hash -ne $productionHeaderSha256){throw 'Production header changed during snapshot'}
$source=Join-Path $snapshot 'tools/WinxRemix/tests/patches/renderer-resource-identity/test_external_asset_identity.cpp'
$include=Split-Path -Parent $headerCopy
$generated=Join-Path $build 'generated'
if($Suite -ne 'Identity'){
    $rendererInputs=@('rtx_types.h','rtx_instance_manager.h','rtx_instance_manager.cpp','rtx_game_capturer.h','rtx_game_capturer.cpp','rtx_scene_manager.cpp')
    foreach($file in $rendererInputs){
        $original=Join-Path $RendererSource "src/dxvk/rtx_render/$file"
        $copy=Join-Path $include $file
        Copy-Item -LiteralPath $original -Destination $copy
        if((Get-FileHash -LiteralPath $copy).Hash -ne (Get-FileHash -LiteralPath $original).Hash){throw 'Renderer input changed during snapshot'}
    }
    if($Suite -eq 'Capture'){
        $source=Join-Path $snapshot 'tools/WinxRemix/tests/patches/renderer-resource-identity/test_capture_identity.cpp'
        $extractor=Join-Path $snapshot 'tools/WinxRemix/tests/patches/renderer-resource-identity/prepare_capture_fixture.py'
        & python $extractor --source-directory (Join-Path $snapshot 'renderer') --output-directory $generated *> (Join-Path $build 'extraction.log')
    }else{
        $source=Join-Path $snapshot 'tools/WinxRemix/tests/patches/renderer-resource-identity/test_external_draw.cpp'
        $extractor=Join-Path $snapshot 'tools/WinxRemix/tests/patches/renderer-resource-identity/prepare_draw_fixture.py'
        & python $extractor --source (Join-Path $include 'rtx_scene_manager.cpp') --output $generated *> (Join-Path $build 'extraction.log')
    }
    if($LASTEXITCODE -ne 0){throw 'Source extraction failed; see extraction.log'}
}
$commands=@"
@echo off
call "$environmentScript" $Platform
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MT /O2 /W4 /WX /I"$include" /I"$generated" "$source" /Fe:test_external_asset_identity.exe
exit /b %errorlevel%
"@
$commandFile=Join-Path $build 'build.cmd'
[IO.File]::WriteAllText($commandFile,$commands,[Text.Encoding]::ASCII)
Push-Location $build
try{
    & $env:ComSpec /d /c $commandFile *> (Join-Path $build 'build.log')
    if($LASTEXITCODE -ne 0){throw "External asset identity build failed; see $build/build.log"}
    $exe=Join-Path $build 'test_external_asset_identity.exe'
    $process=New-Object System.Diagnostics.Process
    $process.StartInfo.FileName=$exe
    $process.StartInfo.WorkingDirectory=$build
    $process.StartInfo.UseShellExecute=$false
    $process.StartInfo.CreateNoWindow=$true
    $process.StartInfo.RedirectStandardOutput=$true
    $process.StartInfo.RedirectStandardError=$true
    $timedOut=$false;$exitCode=$null
    try{
        if(-not $process.Start()){throw 'Failed to start owned CPU fixture'}
        $stdout=$process.StandardOutput.ReadToEndAsync()
        $stderr=$process.StandardError.ReadToEndAsync()
        $timedOut=-not $process.WaitForExit(10000)
        if($timedOut){
            Stop-Process -InputObject $process -Force
            $process.WaitForExit()
        }
        $exitCode=$process.ExitCode
        [IO.File]::WriteAllText((Join-Path $build 'result.json'),$stdout.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        [IO.File]::WriteAllText((Join-Path $build 'stderr.log'),$stderr.GetAwaiter().GetResult(),[Text.UTF8Encoding]::new($false))
        @{exitCode=$exitCode;timedOut=$timedOut;maximumSeconds=10;ownedProcessClosed=$process.HasExited} |
            ConvertTo-Json | Set-Content -LiteralPath (Join-Path $build 'execution.json') -Encoding UTF8
        if($timedOut){throw 'Owned CPU fixture exceeded 10 seconds'}
    }finally{$process.Dispose()}
    if($exitCode -ne 0){throw "External asset identity fixture failed ($exitCode); see $build/stderr.log"}
    $result=Get-Content -LiteralPath (Join-Path $build 'result.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if($result.status -ne 'PASS' -or $result.checks -lt 1 -or $result.rendererExecuted -ne $false){
        throw 'External asset identity fixture did not report its expected CPU-only PASS result'
    }
    $inputs=@(Get-ChildItem -LiteralPath $snapshot -File -Recurse | Sort-Object FullName | ForEach-Object {
        @{file=$_.FullName.Substring($snapshot.Length+1).Replace('\','/');sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
    })
    $generatedInputs=@()
    if(Test-Path -LiteralPath $generated){
        $generatedInputs=@(Get-ChildItem -LiteralPath $generated -File | Sort-Object Name | ForEach-Object {
            @{file=$_.Name;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
        })
    }
    @{schema=1;platform=$Platform;suite=$Suite;gpu=$false;gameExecuted=$false;rendererExecuted=$false;
      scope='applied production identity header or extracted production functions with recording CPU boundaries';sourceInputs=$inputs;generatedInputs=$generatedInputs;
      initialMemory=$memory;minimumStartingPhysicalBytes=512MB;maximumFixtureSeconds=10;
      rendererSource=$RendererSource;productionHeaderSha256=$productionHeaderSha256;
      exeSha256=(Get-FileHash -LiteralPath $exe).Hash;result=$result} |
        ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $build 'verification.json') -Encoding UTF8
    Get-Content -LiteralPath (Join-Path $build 'result.json') -Encoding UTF8
}finally{Pop-Location}
