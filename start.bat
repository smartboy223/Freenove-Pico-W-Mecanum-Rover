@echo off
setlocal
cd /d "%~dp0"
where python >nul 2>nul
if errorlevel 1 (
    echo Python is missing. Install Python and select Add Python to PATH.
    pause
    exit /b 1
)
python "%~dp0launch_dashboard.py" %*
if errorlevel 1 pause
exit /b %errorlevel%
