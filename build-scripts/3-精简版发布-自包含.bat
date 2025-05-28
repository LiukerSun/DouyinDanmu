@echo off
chcp 65001 >nul
echo ========================================
echo 精简版发布 - 自包含但文件分离
echo ========================================
echo.

cd /d "%~dp0.."

echo 正在清理旧文件...
if exist "bin\Release\net8.0-windows\win-x64\publish" rmdir /s /q "bin\Release\net8.0-windows\win-x64\publish"

echo 正在发布程序...
dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=false

if %errorlevel% neq 0 (
    echo 发布失败！
    pause
    exit /b 1
)

echo.
echo 正在整理文件...
set "publish_dir=bin\Release\net8.0-windows\win-x64\publish"
set "output_dir=发布版本\精简版本-自包含"

if exist "%output_dir%" rmdir /s /q "%output_dir%"
mkdir "%output_dir%"

:: 创建子目录结构
mkdir "%output_dir%\runtime"
mkdir "%output_dir%\libs"

:: 复制主程序文件
copy "%publish_dir%\DouyinDanmu.exe" "%output_dir%\"
copy "%publish_dir%\DouyinDanmu.dll" "%output_dir%\"
copy "%publish_dir%\DouyinDanmu.runtimeconfig.json" "%output_dir%\"
copy "%publish_dir%\DouyinDanmu.deps.json" "%output_dir%\"

:: 复制核心运行时文件
copy "%publish_dir%\Microsoft.NETCore.App.dll" "%output_dir%\runtime\" >nul 2>&1
copy "%publish_dir%\Microsoft.WindowsDesktop.App.dll" "%output_dir%\runtime\" >nul 2>&1
copy "%publish_dir%\System.*.dll" "%output_dir%\runtime\" >nul 2>&1
copy "%publish_dir%\Microsoft.Win32.*.dll" "%output_dir%\runtime\" >nul 2>&1

:: 复制第三方库
copy "%publish_dir%\Newtonsoft.Json.dll" "%output_dir%\libs\"
copy "%publish_dir%\Microsoft.Data.Sqlite.dll" "%output_dir%\libs\"
copy "%publish_dir%\SQLitePCLRaw.*.dll" "%output_dir%\libs\"
copy "%publish_dir%\ClearScript*.dll" "%output_dir%\libs\"

:: 复制本机库
copy "%publish_dir%\*.so" "%output_dir%\libs\" >nul 2>&1
copy "%publish_dir%\runtimes" "%output_dir%\libs\" /s >nul 2>&1

:: 创建启动脚本
echo @echo off > "%output_dir%\启动程序.bat"
echo cd /d "%%~dp0" >> "%output_dir%\启动程序.bat"
echo DouyinDanmu.exe >> "%output_dir%\启动程序.bat"
echo pause >> "%output_dir%\启动程序.bat"

:: 创建说明文件
echo 抖音直播弹幕采集工具 - 精简版本 > "%output_dir%\使用说明.txt"
echo. >> "%output_dir%\使用说明.txt"
echo 运行要求： >> "%output_dir%\使用说明.txt"
echo 1. 无需安装任何运行时 >> "%output_dir%\使用说明.txt"
echo 2. 双击"启动程序.bat"或直接运行DouyinDanmu.exe >> "%output_dir%\使用说明.txt"
echo. >> "%output_dir%\使用说明.txt"
echo 文件结构： >> "%output_dir%\使用说明.txt"
echo ├── DouyinDanmu.exe     (主程序) >> "%output_dir%\使用说明.txt"
echo ├── runtime\            (.NET运行时) >> "%output_dir%\使用说明.txt"
echo ├── libs\               (第三方库) >> "%output_dir%\使用说明.txt"
echo └── 启动程序.bat        (启动脚本) >> "%output_dir%\使用说明.txt"
echo. >> "%output_dir%\使用说明.txt"
echo 文件大小：约 40-60 MB >> "%output_dir%\使用说明.txt"
echo 优点：文件结构清晰，大小适中 >> "%output_dir%\使用说明.txt"
echo 缺点：需要保持目录结构完整 >> "%output_dir%\使用说明.txt"

echo.
echo ✅ 发布完成！
echo 📁 输出目录：%output_dir%
dir "%output_dir%" /s
echo.
echo 💡 这个版本文件结构清晰，大小适中
pause 