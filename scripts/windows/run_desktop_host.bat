@echo off
REM LAN Screen Share Host Application - Run Script

cd /d "%~dp0"

echo ========================================
echo LAN Screen Share Host Application
echo ========================================
echo.
echo Starting application...
echo.

set APP_PATH=..\..\out\desktop_host\x64\Debug\LanScreenShareHostApp.exe

if not exist "%APP_PATH%" (
    echo Error: Executable not found at "%APP_PATH%"
    echo Please build the project first:
    echo   .\scripts\windows\build.ps1 -Target desktop_host
    pause
    exit /b 1
)

"%APP_PATH%"

if errorlevel 1 (
    echo.
    echo Application exited with error code: %errorlevel%
) else (
    echo.
    echo Application closed successfully.
)

pause
