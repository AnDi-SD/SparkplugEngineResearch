param(
  [Parameter(Mandatory=$true)][string]$Python,
  [Parameter(Mandatory=$true)][string]$UsdDirectory,
  [Parameter(Mandatory=$true)][string]$Output
)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $Output){throw 'Preserve previous result; choose a new output'}
if(-not (Test-Path -LiteralPath $Python -PathType Leaf)){throw 'Compatible USD Python executable required'}
if(-not (Test-Path -LiteralPath (Join-Path $UsdDirectory 'lib/python/pxr') -PathType Container)){throw 'USD distribution root required'}
$prior=@{}
foreach($key in @('PYTHONDONTWRITEBYTECODE','PYTHONNOUSERSITE','PYTHONPATH','PXR_WORK_THREAD_LIMIT')){
  $prior[$key]=[Environment]::GetEnvironmentVariable($key,'Process')
}
try{
  $env:PYTHONDONTWRITEBYTECODE='1';$env:PYTHONNOUSERSITE='1';$env:PYTHONPATH=$null;$env:PXR_WORK_THREAD_LIMIT='2'
  & $Python (Join-Path $PSScriptRoot 'test_usd_weights.py') --usd-directory $UsdDirectory --output $Output
  if($LASTEXITCODE -ne 0){throw 'CPU USD weight check failed; preserve the report'}
}finally{
  foreach($key in $prior.Keys){[Environment]::SetEnvironmentVariable($key,$prior[$key],'Process')}
}
