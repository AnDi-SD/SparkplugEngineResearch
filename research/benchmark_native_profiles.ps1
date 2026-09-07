param(
    [ValidateSet('pc-texture-native-writer','pc-texture-native-source','pc-texture-missing-mips','pc-scene-file-profile')]
    [string]$Profile = 'pc-texture-missing-mips',
    [string]$DeadlineUtc = ''
)
$ErrorActionPreference = 'Stop'
$repositoryPath = Split-Path -Parent $PSScriptRoot
$benchmarkDirectory = Join-Path $repositoryPath 'local-data/results/profile-benchmarks'
New-Item -ItemType Directory -Path $benchmarkDirectory -Force | Out-Null
$benchmarkStamp = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffffffZ')
$pythonCommand = (Get-Command python -ErrorAction Stop).Source
$deadlineArguments = @()
if ($DeadlineUtc) {
    $deadlineValue = [DateTimeOffset]::Parse($DeadlineUtc).ToUniversalTime()
    $deadlineArguments = @('--deadline-utc', $deadlineValue.ToString("yyyy-MM-dd'T'HH:mm:ss'Z'"))
}
$benchmarkRows = @()
foreach ($workerCount in @(1,4)) {
    $benchmarkOutput = Join-Path $benchmarkDirectory "$benchmarkStamp-$workerCount-stdout.txt"
    $benchmarkError = Join-Path $benchmarkDirectory "$benchmarkStamp-$workerCount-stderr.txt"
    $arguments = @('research/native_workbench.py','run',$Profile,'--workers',$workerCount) + $deadlineArguments
    $benchmarkProcess = Start-Process -FilePath $pythonCommand -ArgumentList $arguments -WorkingDirectory $repositoryPath -WindowStyle Hidden -PassThru -RedirectStandardOutput $benchmarkOutput -RedirectStandardError $benchmarkError
    $benchmarkWatch = [Diagnostics.Stopwatch]::StartNew()
    $peakSampledBytes = [int64]0
    $peakProcessCount = 0
    while (-not $benchmarkProcess.HasExited) {
        # Includes all matching processes, even unrelated ones: a conservative
        # sampled observation, not exclusive process ownership or an OS limit.
        $sampledProcesses = @(Get-Process -Name python,Sparkplug*Tests -ErrorAction SilentlyContinue)
        $sampledBytes = [int64](($sampledProcesses | Measure-Object WorkingSet64 -Sum).Sum)
        $peakSampledBytes = [Math]::Max($peakSampledBytes,$sampledBytes)
        $peakProcessCount = [Math]::Max($peakProcessCount,$sampledProcesses.Count)
        Start-Sleep -Milliseconds 200
        $benchmarkProcess.Refresh()
    }
    $benchmarkProcess.WaitForExit()
    $benchmarkWatch.Stop()
    $reportLine = Get-Content -LiteralPath $benchmarkOutput -Encoding UTF8 | Where-Object { $_.StartsWith('RUN REPORT ') } | Select-Object -First 1
    if (-not $reportLine) { throw "No profile report; inspect $benchmarkError" }
    $reportPath = Join-Path $repositoryPath $reportLine.Substring(11)
    $profileReport = Get-Content -LiteralPath $reportPath -Encoding UTF8 -Raw | ConvertFrom-Json
    $benchmarkRows += [pscustomobject]@{
        workers=$workerCount
        seconds=$benchmarkWatch.Elapsed.TotalSeconds
        processExitCode=$benchmarkProcess.ExitCode
        profileExitCode=$profileReport.exitCode
        profileStatus=$profileReport.status
        completedChildren=$profileReport.completedChildren
        peakSampledWorkingSetBytes=$peakSampledBytes
        peakProcessCount=$peakProcessCount
        sampleIntervalMilliseconds=200
        profileReport=$reportPath
        profileReportSha256=(Get-FileHash -LiteralPath $reportPath -Algorithm SHA256).Hash
    }
    $benchmarkRows[-1] | ConvertTo-Json -Compress
    if ($profileReport.status -ne 'passed' -or $profileReport.exitCode -ne 0) { break }
}
$benchmarkResult = [ordered]@{kind='sampled-profile-throughput';profile=$Profile;cases=$benchmarkRows}
if ($benchmarkRows.Count -eq 2 -and $benchmarkRows[1].profileStatus -eq 'passed') {
    $benchmarkResult.speedup=$benchmarkRows[0].seconds/$benchmarkRows[1].seconds
}
$benchmarkPath = Join-Path $benchmarkDirectory "$benchmarkStamp-summary.json"
$benchmarkResult | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $benchmarkPath -Encoding UTF8
Write-Output "BENCHMARK $benchmarkPath"
if ($benchmarkRows[-1].profileStatus -ne 'passed') { exit 1 }
