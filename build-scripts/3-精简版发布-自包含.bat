@echo off
chcp 65001 >nul
echo ========================================
echo 精简版发布 - 自包含版本
echo ========================================
echo.

cd /d "%~dp0.."

echo 正在清理旧文件...
if exist "bin\Release\net8.0-windows\win-x64\publish" rmdir /s /q "bin\Release\net8.0-windows\win-x64\publish"

echo 正在发布程序...
dotnet publish -c Release -f net8.0-windows -r win-x64 --self-contained true -p:PublishSingleFile=false

if %errorlevel% neq 0 (
    echo 发布失败！
    pause
    exit /b 1
)

echo.
echo 正在整理文件...
set "publish_dir=bin\Release\net8.0-windows\win-x64\publish"
set "output_dir=build-scripts\发布版本\精简版本-自包含"

if exist "%output_dir%" rmdir /s /q "%output_dir%"
mkdir "%output_dir%"

:: 复制所有文件
xcopy "%publish_dir%\*" "%output_dir%\" /E /I /Q >nul 2>&1

:: 重命名主程序
ren "%output_dir%\DouyinDanmu.exe" "DouyinDanmu-Trimmed.exe" >nul 2>&1

:: 创建说明文件
echo 抖音直播弹幕采集工具 - 精简版本 > "%output_dir%\使用说明.txt"
echo. >> "%output_dir%\使用说明.txt"
echo 运行要求：无需安装任何运行时 >> "%output_dir%\使用说明.txt"
echo 文件大小：约 40-60 MB >> "%output_dir%\使用说明.txt"
echo 优点：自包含且文件较小 >> "%output_dir%\使用说明.txt"
echo 缺点：多个文件，可能启动稍慢 >> "%output_dir%\使用说明.txt"
echo. >> "%output_dir%\使用说明.txt"
echo 使用方法：双击 DouyinDanmu-Trimmed.exe 运行 >> "%output_dir%\使用说明.txt"

echo.
echo ✅ 发布完成！
echo 📁 输出目录：%output_dir%
echo.
echo 📄 生成的文件：
dir "%output_dir%" 2>nul
echo.
echo 💡 这个版本自包含且文件较小，适合希望文件较小的用户
pause 