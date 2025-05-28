@echo off
chcp 65001 >nul
echo ========================================
echo 单文件发布 - 完全自包含（推荐）
echo ========================================
echo.

cd /d "%~dp0.."

echo 正在清理旧文件...
if exist "bin\Release\net8.0-windows\win-x64\publish" rmdir /s /q "bin\Release\net8.0-windows\win-x64\publish"

echo 正在发布程序...
dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -p:EnableCompressionInSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true

if %errorlevel% neq 0 (
    echo 发布失败！
    pause
    exit /b 1
)

echo.
echo 正在整理文件...
set "publish_dir=bin\Release\net8.0-windows\win-x64\publish"
set "output_dir=发布版本\单文件版本-自包含"

if exist "%output_dir%" rmdir /s /q "%output_dir%"
mkdir "%output_dir%"

:: 复制主要文件
copy "%publish_dir%\DouyinDanmu.exe" "%output_dir%\"
copy "%publish_dir%\DouyinDanmu.pdb" "%output_dir%\" >nul 2>&1

:: 检查是否有额外的dll文件（V8引擎可能无法打包进单文件）
if exist "%publish_dir%\*.dll" (
    echo 发现额外的DLL文件，正在复制...
    mkdir "%output_dir%\libs"
    copy "%publish_dir%\*.dll" "%output_dir%\libs\"
)

:: 创建说明文件
echo 抖音直播弹幕采集工具 - 单文件版本 > "%output_dir%\使用说明.txt"
echo. >> "%output_dir%\使用说明.txt"
echo 运行要求： >> "%output_dir%\使用说明.txt"
echo 1. 无需安装任何运行时 >> "%output_dir%\使用说明.txt"
echo 2. 直接双击 DouyinDanmu.exe 运行 >> "%output_dir%\使用说明.txt"
echo. >> "%output_dir%\使用说明.txt"
echo 文件大小：约 80-120 MB >> "%output_dir%\使用说明.txt"
echo 优点：完全自包含，无需安装依赖 >> "%output_dir%\使用说明.txt"
echo 缺点：文件较大 >> "%output_dir%\使用说明.txt"
echo. >> "%output_dir%\使用说明.txt"
echo 注意：首次运行可能需要几秒钟解压时间 >> "%output_dir%\使用说明.txt"

echo.
echo ✅ 发布完成！
echo 📁 输出目录：%output_dir%
dir "%output_dir%" /s
echo.
echo 💡 这个版本是单个exe文件，完全自包含，推荐使用
pause 