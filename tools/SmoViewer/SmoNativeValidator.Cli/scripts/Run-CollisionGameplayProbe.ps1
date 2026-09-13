param(
    [Parameter(Mandatory = $true)]
    [string]$RepositoryRoot,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,

    [string]$ManifestPath,

    [ValidateRange(1, 30)]
    [int]$MovementSeconds = 6,

    [ValidateRange(0, 20)]
    [int]$CollisionHoldSeconds = 0
)

$ErrorActionPreference = 'Stop'
$repository = [System.IO.Path]::GetFullPath($RepositoryRoot)
$outputRoot = [System.IO.Path]::GetFullPath($OutputDirectory)
[System.IO.Directory]::CreateDirectory($outputRoot) | Out-Null
$validatorProject = Join-Path $repository `
    'tools/SmoViewer/SmoNativeValidator.Cli/SmoNativeValidator.Cli.csproj'
$manifest = if ([string]::IsNullOrWhiteSpace($ManifestPath)) {
    Join-Path $repository `
        'tools/SmoViewer/SmoNativeValidator.Cli/manifests/mvp-gate6-collision-gameplay.json'
}
else {
    [System.IO.Path]::GetFullPath($ManifestPath)
}
$executable = Join-Path $repository 'local-data/pc-pristine/WinxClub.exe'
$captureScript = Join-Path $repository `
    'tools/SmoViewer/SmoNativeValidator.Cli/scripts/Capture-ProcessWindow.ps1'
$stdoutPath = Join-Path $outputRoot 'validator.stdout.txt'
$stderrPath = Join-Path $outputRoot 'validator.stderr.txt'
$startedAt = Get-Date

Add-Type @"
using System;
using System.Runtime.InteropServices;

public static class NativeGameplayProbeInput
{
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr handle);

    [DllImport("user32.dll")]
    public static extern void keybd_event(
        byte virtualKey,
        byte scanCode,
        uint flags,
        UIntPtr extraInfo);

    [DllImport("user32.dll")]
    public static extern void mouse_event(
        uint flags,
        int dx,
        int dy,
        uint data,
        UIntPtr extraInfo);
}
"@

$arguments = @(
    'run', '--project', $validatorProject,
    '-c', 'Release', '--no-build', '--',
    '--exe', $executable,
    '--manifest', $manifest,
    '--output-dir', $outputRoot,
    '--timeout', '90'
)
$validator = Start-Process `
    -FilePath 'dotnet' `
    -ArgumentList $arguments `
    -WorkingDirectory $repository `
    -WindowStyle Hidden `
    -RedirectStandardOutput $stdoutPath `
    -RedirectStandardError $stderrPath `
    -PassThru

try {
    $sceneLog = $null
    $deadline = (Get-Date).AddSeconds(55)
    while ((Get-Date) -lt $deadline) {
        if ($validator.HasExited) {
            throw "Native validator exited before SCENE01 (code $($validator.ExitCode))."
        }
        $sceneLog = Get-ChildItem -LiteralPath $outputRoot -Recurse -Filter '*.jsonl' |
            Where-Object { $_.LastWriteTime -ge $startedAt } |
            Sort-Object LastWriteTime -Descending |
            Where-Object {
                Select-String -LiteralPath $_.FullName -Pattern '"kind":9' -Quiet
            } |
            Select-Object -First 1
        if ($null -ne $sceneLog) {
            break
        }
        Start-Sleep -Milliseconds 250
    }
    if ($null -eq $sceneLog) {
        throw 'SCENE01 was not observed before the gameplay-probe deadline.'
    }

    $game = Get-Process -Name 'WinxClub' -ErrorAction Stop |
        Where-Object {
            $_.StartTime -ge $startedAt -and
            $_.MainWindowHandle -ne [IntPtr]::Zero
        } |
        Sort-Object StartTime -Descending |
        Select-Object -First 1
    if ($null -eq $game) {
        throw 'The owned WinxClub window was not found after SCENE01.'
    }

    [NativeGameplayProbeInput]::SetForegroundWindow($game.MainWindowHandle) |
        Out-Null
    Start-Sleep -Milliseconds 400
    $before = Join-Path $outputRoot '01-scene-ready-before-input.png'
    & $captureScript -ProcessName 'WinxClub' -OutputPath $before | Out-Null

    # W is the documented PC control for movement away from the camera. The
    # 2006 DirectInput path polls hardware scan codes, so emit DIK_W (0x11)
    # instead of a synthetic virtual-key-only event.
    [NativeGameplayProbeInput]::keybd_event(0, 0x11, 0x0008, [UIntPtr]::Zero)
    Start-Sleep -Seconds $MovementSeconds
    [NativeGameplayProbeInput]::keybd_event(0, 0x11, 0x000A, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 700
    $afterMove = Join-Path $outputRoot '02-after-forward-movement.png'
    & $captureScript -ProcessName 'WinxClub' -OutputPath $afterMove | Out-Null

    $afterCollisionHold = $null
    if ($CollisionHoldSeconds -gt 0) {
        [NativeGameplayProbeInput]::keybd_event(
            0, 0x11, 0x0008, [UIntPtr]::Zero)
        Start-Sleep -Seconds $CollisionHoldSeconds
        [NativeGameplayProbeInput]::keybd_event(
            0, 0x11, 0x000A, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds 700
        $afterCollisionHold = Join-Path $outputRoot `
            '03-after-collision-hold.png'
        & $captureScript `
            -ProcessName 'WinxClub' `
            -OutputPath $afterCollisionHold | Out-Null
    }

    # Relative mouse input rotates the documented third-person camera.
    [NativeGameplayProbeInput]::mouse_event(0x0001, 140, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 900
    $cameraFileName = if ($CollisionHoldSeconds -gt 0) {
        '04-after-camera-rotation.png'
    }
    else {
        '03-after-camera-rotation.png'
    }
    $afterCamera = Join-Path $outputRoot $cameraFileName
    & $captureScript -ProcessName 'WinxClub' -OutputPath $afterCamera | Out-Null

    Write-Output "SCENE_LOG=$($sceneLog.FullName)"
    Write-Output "BEFORE=$before"
    Write-Output "AFTER_MOVE=$afterMove"
    if ($null -ne $afterCollisionHold) {
        Write-Output "AFTER_COLLISION_HOLD=$afterCollisionHold"
    }
    Write-Output "AFTER_CAMERA=$afterCamera"
}
finally {
    if (-not $validator.HasExited) {
        $validator.WaitForExit(65000) | Out-Null
    }
    if (-not $validator.HasExited) {
        Stop-Process -Id $validator.Id -Force
        $validator.WaitForExit()
    }
    $validator.Refresh()
    $summaryFile = Get-ChildItem `
        -LiteralPath $outputRoot `
        -Recurse `
        -Filter 'summary.json' |
        Where-Object { $_.LastWriteTime -ge $startedAt } |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if ($null -eq $summaryFile) {
        throw 'Native validator did not produce summary.json.'
    }
    $summary = Get-Content -LiteralPath $summaryFile.FullName -Raw |
        ConvertFrom-Json
    if ($summary.WasCancelled -or
        $summary.CompletedCaseCount -ne $summary.CaseCount -or
        $summary.StatusCounts.Passed -ne $summary.CaseCount) {
        throw "Native validator gameplay-probe did not pass: $($summaryFile.FullName)"
    }
    Write-Output "VALIDATOR_STATUS=Passed"
    Write-Output "SUMMARY=$($summaryFile.FullName)"
}
