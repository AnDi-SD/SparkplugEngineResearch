param([string]$Name = ('cross-arch-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff')))
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
if ($Name -notmatch '^[A-Za-z0-9._-]+$') { throw 'Test name must be a single path segment' }
$output = Join-Path $root "local-data/rtx-remix/direct-camera-bridge-tests/$Name"
if (Test-Path -LiteralPath $output) { throw 'Use a fresh test name' }
New-Item -ItemType Directory -Path $output | Out-Null
$results = @()
foreach ($writer in @('x86','x64')) {
    $reader = if ($writer -eq 'x86') { 'x64' } else { 'x86' }
    $payload = Join-Path $output "$writer.bin"
    foreach ($step in @(@($writer,'write'),@($reader,'read'))) {
        $platform,$operation = $step
        $executable = Join-Path $root "local-data/rtx-remix/direct-camera-bridge-build/$platform/test/rtx/unit/test_remix_api_camera_$platform.exe"
        if (-not (Test-Path -LiteralPath $executable)) { throw "Missing test executable: $executable" }
        $prefix = "$writer-$platform-$operation"
        $stdout = Join-Path $output "$prefix.out.txt"
        $stderr = Join-Path $output "$prefix.err.txt"
        $process = Start-Process -FilePath $executable -ArgumentList @("--$operation",('"{0}"' -f $payload)) -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        # Hold the process handle before this very short helper exits; otherwise
        # Windows PowerShell can lose ExitCode and report null after Refresh.
        $null = $process.Handle
        if (-not $process.WaitForExit(30000)) {
            $process.Kill()
            throw "Camera test timed out: $prefix"
        }
        $result = [ordered]@{ operation=$operation; platform=$platform; inputPlatform=$writer; exitCode=$process.ExitCode; executableHash=(Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash; stdout=[IO.File]::ReadAllText($stdout); stderr=[IO.File]::ReadAllText($stderr) }
        $results += $result
        if ($process.ExitCode -ne 0) { throw "Camera test failed: $prefix; $($result.stderr)" }
    }
}
$x86 = Get-FileHash -LiteralPath (Join-Path $output 'x86.bin') -Algorithm SHA256
$x64 = Get-FileHash -LiteralPath (Join-Path $output 'x64.bin') -Algorithm SHA256
if ($x86.Hash -ne $x64.Hash) { throw 'Cross-architecture wire bytes differ' }
$report = [ordered]@{ scope='Production CameraInfo serializer/validator, x86 and x64; no game, GPU, or live IPC'; wireBytes=140; fileBytes=(Get-Item -LiteralPath $x86.Path).Length; identicalWire=$true; wireFileHash=$x86.Hash; results=$results }
$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $output 'result.json') -Encoding UTF8
$report | ConvertTo-Json -Depth 5
