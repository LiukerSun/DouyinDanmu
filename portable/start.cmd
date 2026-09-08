@echo off
chcp 65001 >nul
"%~dp0DouyinDanmu.exe" %*
if errorlevel 1 pause
