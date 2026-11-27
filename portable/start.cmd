@echo off
chcp 65001 >nul
start "" "%~dp0DouyinDanmu.exe" %*
if errorlevel 1 pause
