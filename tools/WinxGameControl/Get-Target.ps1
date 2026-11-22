param(
    [Parameter(Mandatory=$true)][ValidateRange(1,2147483647)][int]$GamePid,
    [Parameter(Mandatory=$true)][string]$GamePath,
    [Parameter(Mandatory=$true)][string]$OutFile
)
$ErrorActionPreference='Stop'
$controlExpected=[IO.Path]::GetFullPath($GamePath)
$controlProcess=Get-Process -Id $GamePid -ErrorAction Stop
if([IO.Path]::GetFileName($controlExpected) -ine 'WinxClub.exe' -or
   ![string]::Equals($controlProcess.Path,$controlExpected,[StringComparison]::OrdinalIgnoreCase)){
    throw 'The selected running image must match the explicit full WinxClub.exe path.'
}
$controlHwnd=$controlProcess.MainWindowHandle.ToInt64()
if(!$controlHwnd){throw 'The selected game has no main window yet.'}
$controlTarget=[ordered]@{
    Pid=$GamePid
    CreatedFileTimeUtc=$controlProcess.StartTime.ToUniversalTime().ToFileTimeUtc()
    Hwnd=$controlHwnd
    ImagePath=$controlExpected
}
$controlBytes=[Text.Encoding]::UTF8.GetBytes(($controlTarget | ConvertTo-Json))
$controlStream=[IO.File]::Open([IO.Path]::GetFullPath($OutFile),[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
try{$controlStream.Write($controlBytes,0,$controlBytes.Length);$controlStream.Flush($true)}finally{$controlStream.Dispose()}
Write-Output "Saved selected game identity to $OutFile"
