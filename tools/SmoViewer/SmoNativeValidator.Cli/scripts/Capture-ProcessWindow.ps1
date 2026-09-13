param(
    [Parameter(Mandatory = $true)]
    [string]$ProcessName,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;

public static class NativeWindowCapture
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Rect
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr handle, out Rect rect);
}
"@

$targetProcess = Get-Process -Name $ProcessName -ErrorAction Stop |
    Where-Object { $_.MainWindowHandle -ne [IntPtr]::Zero } |
    Select-Object -First 1
if ($null -eq $targetProcess) {
    throw "No visible window was found for process '$ProcessName'."
}

$windowRect = New-Object NativeWindowCapture+Rect
if (-not [NativeWindowCapture]::GetWindowRect(
        $targetProcess.MainWindowHandle,
        [ref]$windowRect)) {
    throw "GetWindowRect failed for process $($targetProcess.Id)."
}

$width = $windowRect.Right - $windowRect.Left
$height = $windowRect.Bottom - $windowRect.Top
if ($width -le 0 -or $height -le 0) {
    throw "The target window has invalid dimensions ${width}x${height}."
}

$resolvedOutput = [System.IO.Path]::GetFullPath($OutputPath)
$outputDirectory = [System.IO.Path]::GetDirectoryName($resolvedOutput)
[System.IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
$bitmap = New-Object System.Drawing.Bitmap($width, $height)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
try {
    $graphics.CopyFromScreen(
        $windowRect.Left,
        $windowRect.Top,
        0,
        0,
        $bitmap.Size,
        [System.Drawing.CopyPixelOperation]::SourceCopy)
    $bitmap.Save($resolvedOutput, [System.Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $graphics.Dispose()
    $bitmap.Dispose()
}

Write-Output $resolvedOutput
