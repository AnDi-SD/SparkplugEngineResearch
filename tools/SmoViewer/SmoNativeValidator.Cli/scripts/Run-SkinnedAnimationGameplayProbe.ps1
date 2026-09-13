param(
    [Parameter(Mandatory = $true)]
    [string]$RepositoryRoot,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
$repository = [System.IO.Path]::GetFullPath($RepositoryRoot)
$outputRoot = [System.IO.Path]::GetFullPath($OutputDirectory)
[System.IO.Directory]::CreateDirectory($outputRoot) | Out-Null
$validatorProject = Join-Path $repository `
    'tools/SmoViewer/SmoNativeValidator.Cli/SmoNativeValidator.Cli.csproj'
$executable = Join-Path $repository 'local-data/pc-pristine/WinxClub.exe'
$captureScript = Join-Path $repository `
    'tools/SmoViewer/SmoNativeValidator.Cli/scripts/Capture-ProcessWindow.ps1'

Add-Type @"
using System;
using System.Runtime.InteropServices;

public static class SkinnedAnimationProbeInput
{
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr handle);

    [DllImport("user32.dll")]
    public static extern void keybd_event(
        byte virtualKey,
        byte scanCode,
        uint flags,
        UIntPtr extraInfo);
}
"@

function Invoke-AnimationCase {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,

        [Parameter(Mandatory = $true)]
        [string]$ManifestName,

        [Parameter(Mandatory = $true)]
        [bool]$HoldForward
    )

    $caseRoot = Join-Path $outputRoot $Name
    [System.IO.Directory]::CreateDirectory($caseRoot) | Out-Null
    $manifest = Join-Path $repository `
        "tools/SmoViewer/SmoNativeValidator.Cli/manifests/$ManifestName"
    $stdoutPath = Join-Path $caseRoot 'validator.stdout.txt'
    $stderrPath = Join-Path $caseRoot 'validator.stderr.txt'
    $startedAt = Get-Date
    $arguments = @(
        'run', '--project', $validatorProject,
        '-c', 'Release', '--no-build', '--',
        '--exe', $executable,
        '--manifest', $manifest,
        '--output-dir', $caseRoot,
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
    $keyIsDown = $false

    try {
        $sceneLog = $null
        $deadline = (Get-Date).AddSeconds(60)
        while ((Get-Date) -lt $deadline) {
            if ($validator.HasExited) {
                throw "$Name validator exited before SCENE01 (code $($validator.ExitCode))."
            }
            $sceneLog = Get-ChildItem -LiteralPath $caseRoot -Recurse -Filter '*.jsonl' |
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
            throw "$Name did not reach SCENE01 before the gameplay deadline."
        }

        $game = Get-Process -Name 'WinxClub' -ErrorAction Stop |
            Where-Object {
                $_.StartTime -ge $startedAt -and
                $_.MainWindowHandle -ne [IntPtr]::Zero
            } |
            Sort-Object StartTime -Descending |
            Select-Object -First 1
        if ($null -eq $game) {
            throw "$Name owned WinxClub window was not found after SCENE01."
        }

        [SkinnedAnimationProbeInput]::SetForegroundWindow($game.MainWindowHandle) |
            Out-Null
        Start-Sleep -Milliseconds 350
        $frame1 = Join-Path $caseRoot '01-animation-frame.png'
        & $captureScript -ProcessName 'WinxClub' -OutputPath $frame1 | Out-Null

        if ($HoldForward) {
            # DirectInput polls the documented DIK_W hardware scan code (0x11).
            [SkinnedAnimationProbeInput]::keybd_event(
                0, 0x11, 0x0008, [UIntPtr]::Zero)
            $keyIsDown = $true
        }
        Start-Sleep -Milliseconds 900
        $frame2 = Join-Path $caseRoot '02-animation-frame.png'
        & $captureScript -ProcessName 'WinxClub' -OutputPath $frame2 | Out-Null
        Start-Sleep -Milliseconds 900
        $frame3 = Join-Path $caseRoot '03-animation-frame.png'
        & $captureScript -ProcessName 'WinxClub' -OutputPath $frame3 | Out-Null
        if ($keyIsDown) {
            [SkinnedAnimationProbeInput]::keybd_event(
                0, 0x11, 0x000A, [UIntPtr]::Zero)
            $keyIsDown = $false
        }

        $hashes = Get-FileHash -Algorithm SHA256 -LiteralPath $frame1,$frame2,$frame3
        if (($hashes.Hash | Select-Object -Unique).Count -lt 2) {
            throw "$Name produced three byte-identical gameplay frames."
        }
        Write-Output "$Name SCENE_LOG=$($sceneLog.FullName)"
        foreach ($hash in $hashes) {
            Write-Output "$Name FRAME=$($hash.Path) SHA256=$($hash.Hash)"
        }
    }
    finally {
        if ($keyIsDown) {
            [SkinnedAnimationProbeInput]::keybd_event(
                0, 0x11, 0x000A, [UIntPtr]::Zero)
        }
        if (-not $validator.HasExited) {
            $validator.WaitForExit(65000) | Out-Null
        }
        if (-not $validator.HasExited) {
            Stop-Process -Id $validator.Id -Force
            $validator.WaitForExit()
        }
        $validator.Refresh()
        $summaryFile = Get-ChildItem `
            -LiteralPath $caseRoot `
            -Recurse `
            -Filter 'summary.json' |
            Where-Object { $_.LastWriteTime -ge $startedAt } |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1
        if ($null -eq $summaryFile) {
            throw "$Name validator did not produce summary.json."
        }
        $summary = Get-Content -LiteralPath $summaryFile.FullName -Raw |
            ConvertFrom-Json
        if ($summary.WasCancelled -or
            $summary.CompletedCaseCount -ne $summary.CaseCount -or
            $summary.StatusCounts.Passed -ne $summary.CaseCount) {
            throw "$Name native animation probe did not pass: $($summaryFile.FullName)"
        }
        Write-Output "$Name VALIDATOR_STATUS=Passed SUMMARY=$($summaryFile.FullName)"
    }
}

Invoke-AnimationCase `
    -Name 'bloom' `
    -ManifestName 'mvp-gate4-bloom-animation-gameplay.json' `
    -HoldForward $true
Invoke-AnimationCase `
    -Name 'flora' `
    -ManifestName 'mvp-gate4-flora-animation-gameplay.json' `
    -HoldForward $false
