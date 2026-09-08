@echo off
chcp 65001 >nul
"%~dp0runtime\node.exe" "%~dp0app\portable\launcher.js" --stop
if errorlevel 1 pause
