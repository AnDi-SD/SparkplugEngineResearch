param([Parameter(Mandatory=$true)][ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name,
      [ValidateSet('x86','x64')][string]$Platform='x86')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../..'))
$memory=& (Join-Path $PSScriptRoot '../../scripts/runtime/Get-SystemMemory.ps1')
if($memory.availablePhysicalBytes -lt 512MB){throw 'The CPU fixture build requires 512 MiB of available physical memory'}
$dependencyInclude = & (Join-Path $PSScriptRoot '../../Prepare-Dependencies.ps1') -Offline
$build=Join-Path $root "artifacts/WinxRemix/resource-identity-tests/$Name"
if(Test-Path -LiteralPath $build){throw 'Use a fresh evidence directory'}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $installation){throw 'MSVC tools not found'}
$environmentScript=Join-Path $installation 'VC/Auxiliary/Build/vcvarsall.bat'
$snapshot=Join-Path $build 'source'
$relativeInputs=@(
    'tools/WinxRemix/src/runtime/winx_resource_identity.h',
    'tools/WinxRemix/tests/runtime/test_resource_identity.cpp',
    'tools/WinxRemix/tests/runtime/Test-ResourceIdentity.ps1',
    'tools/WinxRemix/dependencies.json',
    'tools/WinxRemix/scripts/runtime/Get-SystemMemory.ps1'
)
foreach($relative in $relativeInputs){
    $destination=Join-Path $snapshot $relative
    New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($destination)) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $root $relative) -Destination $destination
}
$xxhashSource=Join-Path $dependencyInclude 'third-party/xxhash.h'
$xxhashTarget=Join-Path $snapshot 'tools/WinxRemix/third-party/xxhash.h'
$dependencyLock=Get-Content -LiteralPath (Join-Path $root 'tools/WinxRemix/dependencies.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if((Get-FileHash -LiteralPath $xxhashSource).Hash -ne $dependencyLock.xxhash.sha256){throw 'Pinned XXH3 header hash mismatch'}
New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($xxhashTarget)) -Force | Out-Null
Copy-Item -LiteralPath $xxhashSource -Destination $xxhashTarget
if((Get-FileHash -LiteralPath $xxhashTarget).Hash -ne $dependencyLock.xxhash.sha256){throw 'Frozen XXH3 header hash mismatch'}
$source=Join-Path $snapshot 'tools/WinxRemix/tests/runtime/test_resource_identity.cpp'
$commands=@"
@echo off
call "$environmentScript" $Platform
if errorlevel 1 exit /b %errorlevel%
cl /I"$snapshot/tools/WinxRemix" /nologo /std:c++17 /EHsc /MT /O2 /W4 /WX "$source" /Fe:test_resource_identity.exe
exit /b %errorlevel%
"@
$commandFile=Join-Path $build 'build.cmd'
[IO.File]::WriteAllText($commandFile,$commands,[Text.Encoding]::ASCII)
Push-Location $build
try{
    & $env:ComSpec /d /c $commandFile *> (Join-Path $build 'build.log')
    if($LASTEXITCODE -ne 0){throw "Resource identity build failed; see $build/build.log"}
    $exe=Join-Path $build 'test_resource_identity.exe'
    $process=New-Object System.Diagnostics.Process
    $process.StartInfo.FileName=$exe
    $process.StartInfo.WorkingDirectory=$build
    $process.StartInfo.UseShellExecute=$false
    $process.StartInfo.CreateNoWindow=$true
    $process.StartInfo.RedirectStandardOutput=$true
    $process.StartInfo.RedirectStandardError=$true
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
        if($timedOut){throw 'Owned CPU fixture exceeded 10 seconds'}
    }finally{$process.Dispose()}
    if($exitCode -ne 0){throw "Resource identity fixture failed ($exitCode); see $build/stderr.log"}
    $result=Get-Content -LiteralPath (Join-Path $build 'result.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if($result.status -ne 'PASS' -or $result.checks -lt 1){
        throw 'Resource identity fixture did not report its expected PASS result'
    }
    $inputs=@(Get-ChildItem -LiteralPath $snapshot -File -Recurse | Sort-Object FullName | ForEach-Object {
        @{file=$_.FullName.Substring($snapshot.Length+1).Replace('\','/');sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
    })
    @{schema=1;platform=$Platform;gpu=$false;gameExecuted=$false;
      scope='owned versioned material and geometry content identities only';sourceInputs=$inputs;
      exeSha256=(Get-FileHash -LiteralPath $exe).Hash;result=$result} |
        ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $build 'verification.json') -Encoding UTF8
    Get-Content -LiteralPath (Join-Path $build 'result.json') -Encoding UTF8
}finally{Pop-Location}
