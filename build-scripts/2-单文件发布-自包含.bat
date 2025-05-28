@echo off
chcp 65001 >nul
echo ========================================
echo 单文件发布 - 自包含版本
echo ========================================
echo.

cd /d "%~dp0.."

echo 正在清理旧文件...
if exist "bin\Release\net8.0-windows\win-x64\publish" rmdir /s /q "bin\Release\net8.0-windows\win-x64\publish"

echo 正在发布程序...
dotnet publish -c Release -f net8.0-windows -r win-x64 --self-contained true -p:PublishSingleFile=true -p:EnableCompressionInSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true

if %errorlevel% neq 0 (
    echo 发布失败！
    pause
    exit /b 1
)

echo.
echo 正在整理文件...
set "publish_dir=bin\Release\net8.0-windows\win-x64\publish"
set "output_dir=build-scripts\发布版本\单文件版本-自包含"

if exist "%output_dir%" rmdir /s /q "%output_dir%"
mkdir "%output_dir%"

:: 复制单文件
copy "%publish_dir%\DouyinDanmu.exe" "%output_dir%\DouyinDanmu-SingleFile.exe" >nul 2>&1

:: 复制可能的额外文件
if exist "%publish_dir%\*.dll" (
    mkdir "%output_dir%\libs"
    copy "%publish_dir%\*.dll" "%output_dir%\libs\" >nul 2>&1
)

:: 创建说明文件
echo 抖音直播弹幕采集工具 - 单文件版本 > "%output_dir%\使用说明.txt"
echo. >> "%output_dir%\使用说明.txt"
echo 运行要求：无需安装任何运行时 >> "%output_dir%\使用说明.txt"
echo 文件大小：约 80-120 MB >> "%output_dir%\使用说明.txt"
echo 优点：完全自包含，单文件部署 >> "%output_dir%\使用说明.txt"
echo 缺点：文件较大，首次启动稍慢 >> "%output_dir%\使用说明.txt"
echo. >> "%output_dir%\使用说明.txt"
echo 使用方法：双击 DouyinDanmu-SingleFile.exe 运行 >> "%output_dir%\使用说明.txt"

echo.
echo ✅ 发布完成！
echo 📁 输出目录：%output_dir%
echo.
echo 📄 生成的文件：
dir "%output_dir%" 2>nul
echo.
echo 💡 这个版本是单个可执行文件，无需安装任何运行时
pause 