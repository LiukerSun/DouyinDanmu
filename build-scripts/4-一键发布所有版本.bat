@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

echo ========================================
echo 🚀 一键发布所有版本
echo ========================================
echo.
echo 本脚本将依次执行以下发布：
echo 1️⃣ 最小化版本 (需要.NET 8)
echo 2️⃣ 单文件版本 (自包含)
echo 3️⃣ 精简版本 (自包含)
echo 4️⃣ 快速启动版本 (推荐)
echo.

set /p confirm=确认开始发布所有版本? (y/n): 
if /i not "%confirm%"=="y" (
    echo 取消发布
    pause
    exit /b 0
)

echo.
echo ========================================
echo 开始发布流程...
echo ========================================

:: 切换到脚本目录
cd /d "%~dp0"

:: 记录开始时间
echo 开始时间: %date% %time%
echo.

:: 执行各个发布脚本
echo [1/4] 🔄 执行最小化版本发布...
call "1-最小化发布-需要NET8.bat"
if %errorlevel% neq 0 (
    echo ❌ 最小化版本发布失败！
    goto error
)
echo ✅ 最小化版本发布完成
echo.

echo [2/4] 🔄 执行单文件版本发布...
call "2-单文件发布-自包含.bat"
if %errorlevel% neq 0 (
    echo ❌ 单文件版本发布失败！
    goto error
)
echo ✅ 单文件版本发布完成
echo.

echo [3/4] 🔄 执行精简版本发布...
call "3-精简版发布-自包含.bat"
if %errorlevel% neq 0 (
    echo ❌ 精简版本发布失败！
    goto error
)
echo ✅ 精简版本发布完成
echo.

echo [4/4] 🔄 执行快速启动版本发布...
call "5-快速启动版本-自包含.bat"
if %errorlevel% neq 0 (
    echo ❌ 快速启动版本发布失败！
    goto error
)
echo ✅ 快速启动版本发布完成
echo.

:: 记录结束时间
echo 结束时间: %date% %time%
echo.

echo ========================================
echo 🎉 所有版本发布完成！
echo ========================================
echo.
echo 📁 发布目录: build-scripts\发布版本\
echo.
echo 📦 生成的版本:
echo 1️⃣ 最小化版本-需要NET8\
echo 2️⃣ 单文件版本-自包含\
echo 3️⃣ 精简版本-自包含\
echo 4️⃣ 快速启动版本-自包含\
echo.

:: 询问是否打开发布目录
set /p open_dir=是否打开发布目录查看结果? (y/n): 
if /i "%open_dir%"=="y" (
    explorer "发布版本"
)

echo.
echo 💡 提示:
echo - 推荐使用快速启动版本，性能最佳
echo - 所有版本都可以直接分发使用
echo - 可以使用压缩工具打包成ZIP文件
echo.
echo 🚀 发布完成！
goto end

:error
echo.
echo ❌ 发布过程中出现错误！
echo 请检查错误信息并重试。
pause
exit /b 1

:end
pause 