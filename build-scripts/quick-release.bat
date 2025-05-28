@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

echo ========================================
echo 🚀 快速发布脚本 - 无交互模式
echo ========================================

:: 切换到项目根目录 (脚本所在目录的上级目录)
cd /d "%~dp0\.."

:: 获取版本号 (使用符合.NET规范的格式)
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

echo 📅 版本: %VERSION%
echo 🧹 清理旧文件...

if exist "bin\Release" rmdir /s /q "bin\Release" >nul 2>&1
if exist "obj\Release" rmdir /s /q "obj\Release" >nul 2>&1

set "RELEASE_DIR=build-scripts\发布版本\Release-%VERSION%"
if exist "%RELEASE_DIR%" rmdir /s /q "%RELEASE_DIR%" >nul 2>&1
mkdir "%RELEASE_DIR%" >nul 2>&1

echo.
echo 📦 [1/4] 编译最小化版本...
dotnet publish -c Release -r win-x64 --self-contained false -p:PublishSingleFile=false -p:PublishReadyToRun=true -p:Version=%DOTNET_VERSION% -o "bin\Release\minimal"
if !errorlevel! neq 0 (
    echo ❌ 最小化版本编译失败
    goto error
)
mkdir "%RELEASE_DIR%\DouyinDanmu-Minimal-%VERSION%" >nul 2>&1
xcopy "bin\Release\minimal\*" "%RELEASE_DIR%\DouyinDanmu-Minimal-%VERSION%\" /E /I /Q >nul 2>&1
ren "%RELEASE_DIR%\DouyinDanmu-Minimal-%VERSION%\DouyinDanmu.exe" "DouyinDanmu-Minimal.exe" >nul 2>&1
echo ✅ 完成

echo 📦 [2/4] 编译单文件版本...
dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -p:EnableCompressionInSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:Version=%DOTNET_VERSION% -o "bin\Release\singlefile"
if !errorlevel! neq 0 (
    echo ❌ 单文件版本编译失败
    goto error
)
mkdir "%RELEASE_DIR%\DouyinDanmu-SingleFile-%VERSION%" >nul 2>&1
copy "bin\Release\singlefile\DouyinDanmu.exe" "%RELEASE_DIR%\DouyinDanmu-SingleFile-%VERSION%\DouyinDanmu-SingleFile.exe" >nul 2>&1
echo ✅ 完成

echo 📦 [3/4] 编译精简版本...
dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=false -p:Version=%DOTNET_VERSION% -o "bin\Release\trimmed"
if !errorlevel! neq 0 (
    echo ❌ 精简版本编译失败
    goto error
)
mkdir "%RELEASE_DIR%\DouyinDanmu-Trimmed-%VERSION%" >nul 2>&1
xcopy "bin\Release\trimmed\*" "%RELEASE_DIR%\DouyinDanmu-Trimmed-%VERSION%\" /E /I /Q >nul 2>&1
ren "%RELEASE_DIR%\DouyinDanmu-Trimmed-%VERSION%\DouyinDanmu.exe" "DouyinDanmu-Trimmed.exe" >nul 2>&1
echo ✅ 完成

echo 📦 [4/4] 编译快速启动版本...
dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -p:PublishReadyToRun=true -p:EnableCompressionInSingleFile=false -p:Version=%DOTNET_VERSION% -o "bin\Release\faststart"
if !errorlevel! neq 0 (
    echo ❌ 快速启动版本编译失败
    goto error
)
mkdir "%RELEASE_DIR%\DouyinDanmu-FastStart-%VERSION%" >nul 2>&1
xcopy "bin\Release\faststart\*" "%RELEASE_DIR%\DouyinDanmu-FastStart-%VERSION%\" /E /I /Q >nul 2>&1
ren "%RELEASE_DIR%\DouyinDanmu-FastStart-%VERSION%\DouyinDanmu.exe" "DouyinDanmu-FastStart.exe" >nul 2>&1
echo ✅ 完成

echo.
echo 🗜️ 开始压缩...
where powershell >nul 2>&1
if %errorlevel% equ 0 (
    powershell -Command "Compress-Archive -Path '%RELEASE_DIR%\DouyinDanmu-Minimal-%VERSION%' -DestinationPath '%RELEASE_DIR%\DouyinDanmu-Minimal-%VERSION%.zip' -Force" >nul 2>&1
    powershell -Command "Compress-Archive -Path '%RELEASE_DIR%\DouyinDanmu-SingleFile-%VERSION%' -DestinationPath '%RELEASE_DIR%\DouyinDanmu-SingleFile-%VERSION%.zip' -Force" >nul 2>&1
    powershell -Command "Compress-Archive -Path '%RELEASE_DIR%\DouyinDanmu-Trimmed-%VERSION%' -DestinationPath '%RELEASE_DIR%\DouyinDanmu-Trimmed-%VERSION%.zip' -Force" >nul 2>&1
    powershell -Command "Compress-Archive -Path '%RELEASE_DIR%\DouyinDanmu-FastStart-%VERSION%' -DestinationPath '%RELEASE_DIR%\DouyinDanmu-FastStart-%VERSION%.zip' -Force" >nul 2>&1
    echo ✅ 压缩完成
) else (
    echo ⚠️ PowerShell不可用，跳过压缩
)

echo.
echo 🎉 发布完成！
echo 📁 输出目录: %RELEASE_DIR%
echo.
echo 📦 生成的文件:
echo - DouyinDanmu-Minimal-%VERSION%.zip      (约5-10MB, 需要.NET 8)
echo - DouyinDanmu-SingleFile-%VERSION%.zip   (约80-120MB, 单文件)
echo - DouyinDanmu-Trimmed-%VERSION%.zip      (约40-60MB, 精简版)
echo - DouyinDanmu-FastStart-%VERSION%.zip    (约100-140MB, 推荐)
echo.
echo 🚀 可以直接上传到 GitHub Release！
goto end

:error
echo.
echo ❌ 编译失败！请检查错误信息。
pause
exit /b 1

:end 