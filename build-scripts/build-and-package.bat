@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

echo ========================================
echo 🚀 抖音弹幕工具 - 一键编译打包脚本
echo ========================================
echo.

cd /d "%~dp0"

:: 获取当前日期和时间作为版本号 (使用符合.NET规范的格式)
for /f "tokens=*" %%i in ('powershell -Command "$d=Get-Date; '{0:yy}.{1:MM}.{2:dd}.{3:HHmm}' -f $d,$d,$d,$d"') do set "DOTNET_VERSION=%%i"
for /f "tokens=*" %%i in ('powershell -Command "Get-Date -Format 'yyMMdd-HHmm'"') do set "VERSION=%%i"

:: 如果PowerShell失败，使用备用方法
if "%VERSION%"=="" (
    echo 使用备用方法获取版本号...
    for /f "tokens=1-4 delims=/ " %%a in ('date /t') do (
        set "MM=%%a"
        set "DD=%%b"
        set "YY=%%c"
    )
    for /f "tokens=1-2 delims=: " %%a in ('time /t') do (
        set "HH=%%a"
        set "MIN=%%b"
    )
    :: 格式化为两位数
    if !MM! lss 10 set "MM=0!MM!"
    if !DD! lss 10 set "DD=0!DD!"
    if !HH! lss 10 set "HH=0!HH!"
    if !MIN! lss 10 set "MIN=0!MIN!"
    set "VERSION=!YY!!MM!!DD!-!HH!!MIN!"
    set "DOTNET_VERSION=!YY!.!MM!.!DD!.!HH!!MIN!"
)

echo 📅 版本号: %VERSION%
echo 📅 .NET版本号: %DOTNET_VERSION%
echo.

:: 创建发布目录
set "RELEASE_DIR=发布版本\Release-%VERSION%"
if exist "%RELEASE_DIR%" rmdir /s /q "%RELEASE_DIR%"
mkdir "%RELEASE_DIR%"

echo 🧹 清理旧的编译文件...
if exist "bin\Release" rmdir /s /q "bin\Release"
if exist "obj\Release" rmdir /s /q "obj\Release"

echo.
echo ========================================
echo 开始编译四个版本...
echo ========================================

:: 版本1: 最小化版本 (需要.NET 8)
echo.
echo [1/4] 📦 编译最小化版本 (需要.NET 8)...
dotnet publish -c Release -r win-x64 --self-contained false -p:PublishSingleFile=false -p:PublishReadyToRun=true -p:Version=%DOTNET_VERSION% -o "bin\Release\minimal"

if %errorlevel% neq 0 (
    echo ❌ 最小化版本编译失败！
    pause
    exit /b 1
)

:: 整理最小化版本
set "MINIMAL_DIR=%RELEASE_DIR%\DouyinDanmu-Minimal-%VERSION%"
mkdir "%MINIMAL_DIR%"
xcopy "bin\Release\minimal\*" "%MINIMAL_DIR%\" /E /I /Q
ren "%MINIMAL_DIR%\DouyinDanmu.exe" "DouyinDanmu-Minimal.exe"

echo 📝 创建说明文件...
(
echo 抖音直播弹幕采集工具 - 最小化版本
echo.
echo 版本: %VERSION%
echo 运行要求: 需要安装 .NET 8 运行时
echo 文件大小: 约 5-10 MB
echo 优点: 文件最小，启动快速
echo 缺点: 需要预装 .NET 8
echo.
echo 使用方法: 双击 DouyinDanmu-Minimal.exe 运行
) > "%MINIMAL_DIR%\使用说明.txt"

echo ✅ 最小化版本完成

:: 版本2: 单文件版本 (自包含)
echo.
echo [2/4] 📦 编译单文件版本 (自包含)...
dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -p:EnableCompressionInSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:Version=%DOTNET_VERSION% -o "bin\Release\singlefile"

if %errorlevel% neq 0 (
    echo ❌ 单文件版本编译失败！
    pause
    exit /b 1
)

:: 整理单文件版本
set "SINGLE_DIR=%RELEASE_DIR%\DouyinDanmu-SingleFile-%VERSION%"
mkdir "%SINGLE_DIR%"
copy "bin\Release\singlefile\DouyinDanmu.exe" "%SINGLE_DIR%\DouyinDanmu-SingleFile.exe"
if exist "bin\Release\singlefile\*.dll" (
    mkdir "%SINGLE_DIR%\libs"
    copy "bin\Release\singlefile\*.dll" "%SINGLE_DIR%\libs\"
)

(
echo 抖音直播弹幕采集工具 - 单文件版本
echo.
echo 版本: %VERSION%
echo 运行要求: 无需安装任何运行时
echo 文件大小: 约 80-120 MB
echo 优点: 完全自包含，单文件部署
echo 缺点: 文件较大，首次启动稍慢
echo.
echo 使用方法: 双击 DouyinDanmu-SingleFile.exe 运行
) > "%SINGLE_DIR%\使用说明.txt"

echo ✅ 单文件版本完成

:: 版本3: 精简版本 (自包含，文件分离)
echo.
echo [3/4] 📦 编译精简版本 (自包含，文件分离)...
dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=false -p:Version=%DOTNET_VERSION% -o "bin\Release\trimmed"

if %errorlevel% neq 0 (
    echo ❌ 精简版本编译失败！
    pause
    exit /b 1
)

:: 整理精简版本
set "TRIMMED_DIR=%RELEASE_DIR%\DouyinDanmu-Trimmed-%VERSION%"
mkdir "%TRIMMED_DIR%"
xcopy "bin\Release\trimmed\*" "%TRIMMED_DIR%\" /E /I /Q
ren "%TRIMMED_DIR%\DouyinDanmu.exe" "DouyinDanmu-Trimmed.exe"

(
echo 抖音直播弹幕采集工具 - 精简版本
echo.
echo 版本: %VERSION%
echo 运行要求: 无需安装任何运行时
echo 文件大小: 约 40-60 MB
echo 优点: 自包含且文件较小
echo 缺点: 多个文件，可能启动稍慢
echo.
echo 使用方法: 双击 DouyinDanmu-Trimmed.exe 运行
) > "%TRIMMED_DIR%\使用说明.txt"

echo ✅ 精简版本完成

:: 版本4: 快速启动版本 (推荐)
echo.
echo [4/4] 📦 编译快速启动版本 (推荐)...
dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -p:PublishReadyToRun=true -p:EnableCompressionInSingleFile=false -p:Version=%DOTNET_VERSION% -o "bin\Release\faststart"

if %errorlevel% neq 0 (
    echo ❌ 快速启动版本编译失败！
    pause
    exit /b 1
)

:: 整理快速启动版本
set "FAST_DIR=%RELEASE_DIR%\DouyinDanmu-FastStart-%VERSION%"
mkdir "%FAST_DIR%"
xcopy "bin\Release\faststart\*" "%FAST_DIR%\" /E /I /Q
ren "%FAST_DIR%\DouyinDanmu.exe" "DouyinDanmu-FastStart.exe"

(
echo 抖音直播弹幕采集工具 - 快速启动版本 ^(推荐^)
echo.
echo 版本: %VERSION%
echo 运行要求: 无需安装任何运行时
echo 文件大小: 约 100-140 MB
echo 优点: 启动速度最快，性能最佳
echo 缺点: 文件较大
echo.
echo 使用方法: 双击 DouyinDanmu-FastStart.exe 运行
echo.
echo ⚡ 这是推荐版本，启动速度和运行性能最佳！
) > "%FAST_DIR%\使用说明.txt"

echo ✅ 快速启动版本完成

echo.
echo ========================================
echo 📦 开始压缩打包...
echo ========================================

:: 检查是否有PowerShell可用于压缩
where powershell >nul 2>&1
if %errorlevel% equ 0 (
    echo 使用 PowerShell 进行压缩...
    
    echo [1/4] 压缩最小化版本...
    powershell -Command "Compress-Archive -Path '%MINIMAL_DIR%' -DestinationPath '%RELEASE_DIR%\DouyinDanmu-Minimal-%VERSION%.zip' -Force"
    
    echo [2/4] 压缩单文件版本...
    powershell -Command "Compress-Archive -Path '%SINGLE_DIR%' -DestinationPath '%RELEASE_DIR%\DouyinDanmu-SingleFile-%VERSION%.zip' -Force"
    
    echo [3/4] 压缩精简版本...
    powershell -Command "Compress-Archive -Path '%TRIMMED_DIR%' -DestinationPath '%RELEASE_DIR%\DouyinDanmu-Trimmed-%VERSION%.zip' -Force"
    
    echo [4/4] 压缩快速启动版本...
    powershell -Command "Compress-Archive -Path '%FAST_DIR%' -DestinationPath '%RELEASE_DIR%\DouyinDanmu-FastStart-%VERSION%.zip' -Force"
    
    echo ✅ 所有版本已压缩完成！
) else (
    echo ⚠️  PowerShell 不可用，跳过压缩步骤
    echo 💡 您可以手动压缩各个版本文件夹
)

:: 创建发布说明
echo.
echo 📝 创建发布说明...
(
echo 抖音直播弹幕采集工具 - Release %VERSION%
echo ========================================
echo.
echo 📦 本次发布包含四个版本:
echo.
echo 1. DouyinDanmu-Minimal-%VERSION%.zip
echo    - 最小化版本 ^(需要.NET 8^)
echo    - 文件大小: 约 5-10 MB
echo    - 适合: 已安装.NET 8的用户
echo.
echo 2. DouyinDanmu-SingleFile-%VERSION%.zip
echo    - 单文件版本 ^(自包含^)
echo    - 文件大小: 约 80-120 MB
echo    - 适合: 希望单文件部署的用户
echo.
echo 3. DouyinDanmu-Trimmed-%VERSION%.zip
echo    - 精简版本 ^(自包含^)
echo    - 文件大小: 约 40-60 MB
echo    - 适合: 希望文件较小的用户
echo.
echo 4. DouyinDanmu-FastStart-%VERSION%.zip ⚡ 推荐
echo    - 快速启动版本 ^(自包含^)
echo    - 文件大小: 约 100-140 MB
echo    - 适合: 大多数用户 ^(启动最快^)
echo.
echo 💡 推荐使用 FastStart 版本，启动速度和性能最佳！
echo.
echo 🔧 编译信息:
echo - 编译时间: %date% %time%
echo - 目标框架: .NET 8.0
echo - 目标平台: Windows x64
echo - 编译配置: Release
echo.
echo 📁 文件结构:
) > "%RELEASE_DIR%\Release-Notes-%VERSION%.txt"

:: 显示目录结构
dir "%RELEASE_DIR%" >> "%RELEASE_DIR%\Release-Notes-%VERSION%.txt"

echo.
echo ========================================
echo 🎉 编译打包完成！
echo ========================================
echo.
echo 📁 发布目录: %RELEASE_DIR%
echo.
echo 📦 生成的文件:
dir "%RELEASE_DIR%" /b
echo.
echo 💡 提示:
echo - 推荐使用 DouyinDanmu-FastStart-%VERSION%.zip
echo - 所有版本都已准备好发布到 GitHub Release
echo - 查看 Release-Notes-%VERSION%.txt 了解详细信息
echo.

:: 询问是否打开发布目录
set /p open_dir=是否打开发布目录? (y/n): 
if /i "%open_dir%"=="y" (
    explorer "%RELEASE_DIR%"
)

echo.
echo 🚀 发布准备完成！可以上传到 GitHub Release 了！
pause 