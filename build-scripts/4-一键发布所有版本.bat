@echo off
chcp 65001 >nul
echo ========================================
echo 一键发布所有版本
echo ========================================
echo.

cd /d "%~dp0.."

echo 选择要发布的版本：
echo.
echo 1. 最小化版本 (需要.NET 8) - 约5-10MB
echo 2. 单文件版本 (标准) - 约80-120MB  
echo 3. 精简版本 (文件分离) - 约40-60MB
echo 4. 快速启动版本 (推荐) - 约100-140MB ⚡
echo 5. 发布所有版本
echo 6. 退出
echo.
set /p choice=请输入选择 (1-6): 

if "%choice%"=="1" goto build_minimal
if "%choice%"=="2" goto build_single
if "%choice%"=="3" goto build_trimmed
if "%choice%"=="4" goto build_fast
if "%choice%"=="5" goto build_all
if "%choice%"=="6" goto exit
goto invalid_choice

:build_minimal
echo.
echo 正在发布最小化版本...
call "build-scripts\1-最小化发布-需要NET8.bat"
goto end

:build_single
echo.
echo 正在发布单文件版本...
call "build-scripts\2-单文件发布-自包含.bat"
goto end

:build_trimmed
echo.
echo 正在发布精简版本...
call "build-scripts\3-精简版发布-自包含.bat"
goto end

:build_fast
echo.
echo 正在发布快速启动版本...
call "build-scripts\5-快速启动版本-自包含.bat"
goto end

:build_all
echo.
echo 正在发布所有版本，这可能需要几分钟...
echo.

echo [1/4] 发布最小化版本...
call "build-scripts\1-最小化发布-需要NET8.bat" >nul
if %errorlevel% neq 0 (
    echo ❌ 最小化版本发布失败
) else (
    echo ✅ 最小化版本发布成功
)

echo.
echo [2/4] 发布单文件版本...
call "build-scripts\2-单文件发布-自包含.bat" >nul
if %errorlevel% neq 0 (
    echo ❌ 单文件版本发布失败
) else (
    echo ✅ 单文件版本发布成功
)

echo.
echo [3/4] 发布精简版本...
call "build-scripts\3-精简版发布-自包含.bat" >nul
if %errorlevel% neq 0 (
    echo ❌ 精简版本发布失败
) else (
    echo ✅ 精简版本发布成功
)

echo.
echo [4/4] 发布快速启动版本...
call "build-scripts\5-快速启动版本-自包含.bat" >nul
if %errorlevel% neq 0 (
    echo ❌ 快速启动版本发布失败
) else (
    echo ✅ 快速启动版本发布成功
)

echo.
echo ========================================
echo 🎉 所有版本发布完成！
echo ========================================
echo.
echo 📁 发布目录结构：
echo 发布版本\
echo ├── 最小化版本-需要NET8\       (约5-10MB)
echo ├── 单文件版本-自包含\         (约80-120MB)
echo ├── 精简版本-自包含\           (约40-60MB)
echo └── 快速启动版本-自包含\       (约100-140MB) ⚡
echo.
echo 💡 推荐使用"快速启动版本-自包含"，启动速度最快
echo.
dir "发布版本" /s
goto end

:invalid_choice
echo.
echo ❌ 无效选择，请重新运行脚本
goto end

:exit
echo.
echo 👋 已取消发布
goto end

:end
echo. 