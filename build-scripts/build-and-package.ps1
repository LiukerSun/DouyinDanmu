# 抖音弹幕工具 - 一键编译打包脚本 (PowerShell版本)
param(
    [switch]$NoInteraction = $false
)

# 设置控制台编码
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "🚀 抖音弹幕工具 - 一键编译打包脚本" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 切换到脚本目录
Set-Location $PSScriptRoot

# 获取版本号
$now = Get-Date
$version = $now.ToString("yyMMdd-HHmm")
$dotnetVersion = $now.ToString("yy.MM.dd.HHmm")

Write-Host "📅 版本号: $version" -ForegroundColor Yellow
Write-Host ""

# 创建发布目录
$releaseDir = "发布版本\Release-$version"
if (Test-Path $releaseDir) {
    Remove-Item $releaseDir -Recurse -Force
}
New-Item -ItemType Directory -Path $releaseDir -Force | Out-Null

Write-Host "🧹 清理旧的编译文件..." -ForegroundColor Yellow
if (Test-Path "bin\Release") { Remove-Item "bin\Release" -Recurse -Force -ErrorAction SilentlyContinue }
if (Test-Path "obj\Release") { Remove-Item "obj\Release" -Recurse -Force -ErrorAction SilentlyContinue }

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "开始编译四个版本..." -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan

# 定义版本配置
$versions = @(
    @{
        Name = "Minimal"
        DisplayName = "最小化版本 (需要.NET 8)"
        Command = "dotnet publish -c Release -r win-x64 --self-contained false -p:PublishSingleFile=false -p:PublishReadyToRun=true -p:Version=$dotnetVersion -o `"bin\Release\minimal`""
        OutputDir = "bin\Release\minimal"
        Size = "约 5-10 MB"
        Requirements = "需要安装 .NET 8 运行时"
        Pros = "文件最小，启动快速"
        Cons = "需要预装 .NET 8"
    },
    @{
        Name = "SingleFile"
        DisplayName = "单文件版本 (自包含)"
        Command = "dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -p:EnableCompressionInSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:Version=$dotnetVersion -o `"bin\Release\singlefile`""
        OutputDir = "bin\Release\singlefile"
        Size = "约 80-120 MB"
        Requirements = "无需安装任何运行时"
        Pros = "完全自包含，单文件部署"
        Cons = "文件较大，首次启动稍慢"
    },
    @{
        Name = "Trimmed"
        DisplayName = "精简版本 (自包含，文件分离)"
        Command = "dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=false -p:Version=$dotnetVersion -o `"bin\Release\trimmed`""
        OutputDir = "bin\Release\trimmed"
        Size = "约 40-60 MB"
        Requirements = "无需安装任何运行时"
        Pros = "自包含且文件较小"
        Cons = "多个文件，可能启动稍慢"
    },
    @{
        Name = "FastStart"
        DisplayName = "快速启动版本 (推荐)"
        Command = "dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -p:PublishReadyToRun=true -p:EnableCompressionInSingleFile=false -p:Version=$dotnetVersion -o `"bin\Release\faststart`""
        OutputDir = "bin\Release\faststart"
        Size = "约 100-140 MB"
        Requirements = "无需安装任何运行时"
        Pros = "启动速度最快，性能最佳"
        Cons = "文件较大"
    }
)

# 编译各个版本
for ($i = 0; $i -lt $versions.Count; $i++) {
    $ver = $versions[$i]
    $num = $i + 1
    
    Write-Host ""
    Write-Host "[$num/4] 📦 编译$($ver.DisplayName)..." -ForegroundColor Green
    
    try {
        Invoke-Expression $ver.Command | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "编译失败"
        }
        
        # 整理文件
        $targetDir = "$releaseDir\DouyinDanmu-$($ver.Name)-$version"
        New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
        
        if ($ver.Name -eq "SingleFile") {
            # 单文件版本特殊处理
            Copy-Item "$($ver.OutputDir)\DouyinDanmu.exe" "$targetDir\DouyinDanmu-$($ver.Name).exe"
            if (Test-Path "$($ver.OutputDir)\*.dll") {
                New-Item -ItemType Directory -Path "$targetDir\libs" -Force | Out-Null
                Copy-Item "$($ver.OutputDir)\*.dll" "$targetDir\libs\"
            }
        } else {
            # 其他版本复制所有文件
            Copy-Item "$($ver.OutputDir)\*" $targetDir -Recurse
            if (Test-Path "$targetDir\DouyinDanmu.exe") {
                Rename-Item "$targetDir\DouyinDanmu.exe" "DouyinDanmu-$($ver.Name).exe"
            }
        }
        
        # 创建说明文件
        $readme = @"
抖音直播弹幕采集工具 - $($ver.DisplayName)

版本: $version
运行要求: $($ver.Requirements)
文件大小: $($ver.Size)
优点: $($ver.Pros)
缺点: $($ver.Cons)

使用方法: 双击 DouyinDanmu-$($ver.Name).exe 运行
"@
        
        if ($ver.Name -eq "FastStart") {
            $readme += "`n`n⚡ 这是推荐版本，启动速度和运行性能最佳！"
        }
        
        $readme | Out-File "$targetDir\使用说明.txt" -Encoding UTF8
        
        Write-Host "✅ $($ver.DisplayName)完成" -ForegroundColor Green
    }
    catch {
        Write-Host "❌ $($ver.DisplayName)编译失败！" -ForegroundColor Red
        Write-Host "错误: $_" -ForegroundColor Red
        if (-not $NoInteraction) {
            Read-Host "按回车键继续..."
        }
        exit 1
    }
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "📦 开始压缩打包..." -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan

# 压缩各个版本
foreach ($ver in $versions) {
    $sourceDir = "$releaseDir\DouyinDanmu-$($ver.Name)-$version"
    $zipFile = "$releaseDir\DouyinDanmu-$($ver.Name)-$version.zip"
    
    Write-Host "🗜️ 压缩$($ver.DisplayName)..." -ForegroundColor Yellow
    try {
        Compress-Archive -Path $sourceDir -DestinationPath $zipFile -Force
        Write-Host "✅ 压缩完成" -ForegroundColor Green
    }
    catch {
        Write-Host "❌ 压缩失败: $_" -ForegroundColor Red
    }
}

# 创建发布说明
Write-Host ""
Write-Host "📝 创建发布说明..." -ForegroundColor Yellow

$releaseNotes = @"
抖音直播弹幕采集工具 - Release $version
========================================

📦 本次发布包含四个版本:

1. DouyinDanmu-Minimal-$version.zip
   - 最小化版本 (需要.NET 8)
   - 文件大小: 约 5-10 MB
   - 适合: 已安装.NET 8的用户

2. DouyinDanmu-SingleFile-$version.zip
   - 单文件版本 (自包含)
   - 文件大小: 约 80-120 MB
   - 适合: 希望单文件部署的用户

3. DouyinDanmu-Trimmed-$version.zip
   - 精简版本 (自包含)
   - 文件大小: 约 40-60 MB
   - 适合: 希望文件较小的用户

4. DouyinDanmu-FastStart-$version.zip ⚡ 推荐
   - 快速启动版本 (自包含)
   - 文件大小: 约 100-140 MB
   - 适合: 大多数用户 (启动最快)

💡 推荐使用 FastStart 版本，启动速度和性能最佳！

🔧 编译信息:
- 编译时间: $(Get-Date)
- 目标框架: .NET 8.0
- 目标平台: Windows x64
- 编译配置: Release

📁 文件结构:
"@

$releaseNotes | Out-File "$releaseDir\Release-Notes-$version.txt" -Encoding UTF8
Get-ChildItem $releaseDir | Out-File "$releaseDir\Release-Notes-$version.txt" -Append -Encoding UTF8

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "🎉 编译打包完成！" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "📁 发布目录: $releaseDir" -ForegroundColor Yellow
Write-Host ""
Write-Host "📦 生成的文件:" -ForegroundColor Yellow
Get-ChildItem $releaseDir -Name | ForEach-Object { Write-Host "   $_" -ForegroundColor White }
Write-Host ""
Write-Host "💡 提示:" -ForegroundColor Cyan
Write-Host "- 推荐使用 DouyinDanmu-FastStart-$version.zip" -ForegroundColor White
Write-Host "- 所有版本都已准备好发布到 GitHub Release" -ForegroundColor White
Write-Host "- 查看 Release-Notes-$version.txt 了解详细信息" -ForegroundColor White
Write-Host ""

if (-not $NoInteraction) {
    $openDir = Read-Host "是否打开发布目录? (y/n)"
    if ($openDir -eq "y" -or $openDir -eq "Y") {
        Invoke-Item $releaseDir
    }
}

Write-Host ""
Write-Host "🚀 发布准备完成！可以上传到 GitHub Release 了！" -ForegroundColor Green

if (-not $NoInteraction) {
    Read-Host "按回车键退出..."
} 