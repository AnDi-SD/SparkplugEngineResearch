@echo off
where py >nul 2>nul
if errorlevel 1 (
    python "%~dp0san_to_vmd.py" %*
) else (
    py -3 "%~dp0san_to_vmd.py" %*
)
set "san_result=%errorlevel%"
echo.
pause
exit /b %san_result%
