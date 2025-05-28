@echo off
chcp 65001 >nul
echo ========================================
echo 快速启动版本发布 - 自包含版本 (推荐)
echo ========================================
echo.

cd /d "%~dp0.."

echo 正在清理旧文件...
if exist "bin\Release\net8.0-windows\win-x64\publish" rmdir /s /q "bin\Release\net8.0-windows\win-x64\publish"

echo 正在发布程序...
dotnet publish -c Release -f net8.0-windows -r win-x64 --self-contained true -p:PublishSingleFile=true -p:PublishReadyToRun=true -p:EnableCompressionInSingleFile=false

if %errorlevel% neq 0 (
    echo 发布失败！
    pause
    exit /b 1
)

echo.
echo 正在整理文件...
set "publish_dir=bin\Release\net8.0-windows\win-x64\publish"
set "output_dir=build-scripts\发布版本\快速启动版本-自包含"

if exist "%output_dir%" rmdir /s /q "%output_dir%"
mkdir "%output_dir%"

:: 复制所有文件
xcopy "%publish_dir%\*" "%output_dir%\" /E /I /Q >nul 2>&1

:: 重命名主程序
ren "%output_dir%\DouyinDanmu.exe" "DouyinDanmu-FastStart.exe" >nul 2>&1

:: 创建说明文件
echo 抖音直播弹幕采集工具 - 快速启动版本 (推荐) > "%output_dir%\使用说明.txt"
echo. >> "%output_dir%\使用说明.txt"
echo 运行要求：无需安装任何运行时 >> "%output_dir%\使用说明.txt"
echo 文件大小：约 100-140 MB >> "%output_dir%\使用说明.txt"
echo 优点：启动速度最快，性能最佳 >> "%output_dir%\使用说明.txt"
echo 缺点：文件较大 >> "%output_dir%\使用说明.txt"
echo. >> "%output_dir%\使用说明.txt"
echo 使用方法：双击 DouyinDanmu-FastStart.exe 运行 >> "%output_dir%\使用说明.txt"
echo. >> "%output_dir%\使用说明.txt"
echo ⚡ 这是推荐版本，启动速度和运行性能最佳！ >> "%output_dir%\使用说明.txt"

echo.
echo ✅ 发布完成！
echo 📁 输出目录：%output_dir%
echo.
echo 📄 生成的文件：
dir "%output_dir%" 2>nul
echo.
echo ⚡ 这是推荐版本，启动速度和运行性能最佳！
pause 