@echo off
rem Applies the Polish translation again, e.g. after the game files were verified or updated.
chcp 65001 >nul
rem The game folder is often not writable without administrator rights
net session >nul 2>&1
if errorlevel 1 (
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)
cd /d "%~dp0"
"%~dp0PolishPatcher.exe" patch "%~dp0..\bigfile.x64.dat" "%~dp0polish.pak"
echo.
pause
