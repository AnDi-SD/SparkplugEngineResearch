@echo off
rem Run Python from this BAT's folder. The script uses its own input and output.
where py >nul 2>nul
if errorlevel 1 (
    python "%~dp0san_to_vmd.py" %*
) else (
    py -3 "%~dp0san_to_vmd.py" %*
)
set "san_result=%errorlevel%"
echo.
rem Keep the result or error visible after a double-click.
pause
exit /b %san_result%
