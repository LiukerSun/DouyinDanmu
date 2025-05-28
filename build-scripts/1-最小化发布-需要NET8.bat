@echo off
chcp 65001 >nul
echo ========================================
echo 最小化发布 - 需要目标设备安装.NET 8运行时
echo ========================================
echo.

cd /d "%~dp0.."

echo 正在清理旧文件...
if exist "bin\Release\net8.0-windows\win-x64\publish" rmdir /s /q "bin\Release\net8.0-windows\win-x64\publish"

echo 正在发布程序...
dotnet publish -c Release -f net8.0-windows --self-contained false -p:PublishSingleFile=false -p:PublishTrimmed=false

if %errorlevel% neq 0 (
    echo 发布失败！
    pause
    exit /b 1
)

echo.
echo 正在整理文件...
set "publish_dir=bin\Release\net8.0-windows\win-x64\publish"
set "output_dir=发布版本\最小化版本-需要NET8"

if exist "%output_dir%" rmdir /s /q "%output_dir%"
mkdir "%output_dir%"

:: 复制主要文件
copy "%publish_dir%\DouyinDanmu.exe" "%output_dir%\" >nul 2>&1
copy "%publish_dir%\DouyinDanmu.dll" "%output_dir%\" >nul 2>&1
copy "%publish_dir%\DouyinDanmu.runtimeconfig.json" "%output_dir%\" >nul 2>&1
copy "%publish_dir%\DouyinDanmu.deps.json" "%output_dir%\" >nul 2>&1

:: 复制第三方依赖库
mkdir "%output_dir%\libs"
copy "%publish_dir%\Newtonsoft.Json.dll" "%output_dir%\libs\" >nul 2>&1
copy "%publish_dir%\Microsoft.Data.Sqlite.dll" "%output_dir%\libs\" >nul 2>&1
copy "%publish_dir%\SQLitePCLRaw.*.dll" "%output_dir%\libs\" >nul 2>&1
copy "%publish_dir%\ClearScript*.dll" "%output_dir%\libs\" >nul 2>&1
copy "%publish_dir%\e_sqlite3.dll" "%output_dir%\libs\" >nul 2>&1

:: 复制所有dll文件到libs目录
for %%f in ("%publish_dir%\*.dll") do (
    if not "%%~nf"=="DouyinDanmu" (
        copy "%%f" "%output_dir%\libs\" >nul 2>&1
    )
)

:: 复制runtimes目录（如果存在）
if exist "%publish_dir%\runtimes" (
    xcopy "%publish_dir%\runtimes" "%output_dir%\libs\runtimes" /s /e /i >nul 2>&1
)

:: 创建说明文件
echo 抖音直播弹幕采集工具 - 最小化版本 > "%output_dir%\使用说明.txt"
echo. >> "%output_dir%\使用说明.txt"
echo 运行要求： >> "%output_dir%\使用说明.txt"
echo 1. 需要安装 .NET 8.0 Desktop Runtime >> "%output_dir%\使用说明.txt"
echo 2. 下载地址：https://dotnet.microsoft.com/download/dotnet/8.0 >> "%output_dir%\使用说明.txt"
echo. >> "%output_dir%\使用说明.txt"
echo 文件大小：约 5-10 MB >> "%output_dir%\使用说明.txt"
echo 优点：文件小，启动快 >> "%output_dir%\使用说明.txt"
echo 缺点：需要目标设备安装.NET运行时 >> "%output_dir%\使用说明.txt"

echo.
echo ✅ 发布完成！
echo 📁 输出目录：%output_dir%
echo.
echo 📄 主要文件：
dir "%output_dir%\*.exe" "%output_dir%\*.dll" "%output_dir%\*.json" 2>nul
echo.
echo 📚 依赖库：
dir "%output_dir%\libs" 2>nul
echo.
echo 💡 这个版本文件最小，但需要目标设备安装.NET 8运行时
pause 