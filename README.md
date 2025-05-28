# 抖音直播弹幕采集工具 (DouyinDanmu)

[![.NET](https://img.shields.io/badge/.NET-8.0-blue.svg)](https://dotnet.microsoft.com/download/dotnet/8.0)
[![Platform](https://img.shields.io/badge/platform-Windows-lightgrey.svg)](https://www.microsoft.com/windows)
[![License](https://img.shields.io/badge/license-Educational-green.svg)](#许可证)

一个功能完整的抖音直播弹幕实时采集工具，使用 C# .NET 8.0 和 WinForms 开发，支持实时抓取、数据库存储、智能过滤和数据导出。

## ✨ 核心功能

- **实时采集** - 抖音直播弹幕、礼物、点赞等消息实时采集
- **数据存储** - SQLite 本地数据库持久化存储，支持历史查询
- **消息过滤** - 支持关注特定用户，消息分类显示
- **数据导出** - 支持 TXT/CSV 格式导出，包含完整统计信息
- **现代界面** - 直观的图形界面，右键菜单，自动滚动等功能

## 🚀 快速开始

### 系统要求
- **操作系统**: Windows 10/11 (x64)
- **运行时**: .NET 8.0 Runtime (可选，自包含版本无需安装)

### 📦 下载运行

从 [Releases](../../releases) 页面下载最新版本：

1. **快速启动版本** (推荐) - 约 100-140MB ⚡
   - 启动速度最快，运行时优化，自包含
   
2. **精简版本** - 约 40-60MB 📦
   - 文件结构清晰，大小适中，自包含
   
3. **最小化版本** - 约 5-10MB 🪶
   - 体积最小，需要 .NET 8.0 运行时

4. **单文件版本** - 约 80-120MB 📄
   - 单个可执行文件，便于分发，自包含

### 🛠️ 从源码编译

```bash
# 克隆项目
git clone git@github.com:LiukerSun/DouyinDanmu.git
cd DouyinDanmu

# 编译运行
dotnet run

# 或使用一键构建脚本
cd build-scripts
.\quick-release.bat          # 快速编译四个版本
```

### 使用步骤
1. 运行 `DouyinDanmu.exe`
2. 输入抖音直播间ID
3. 点击"连接"开始采集
4. 在分类面板中查看不同类型消息
5. 右键用户可添加关注
6. 使用"保存日志"导出数据

## 📋 支持的消息类型

- 💬 **聊天消息** - 用户发送的文字消息
- 🎁 **礼物消息** - 用户送礼信息  
- 👍 **点赞消息** - 用户点赞行为
- 👋 **进场消息** - 用户进入直播间
- ❤️ **关注消息** - 用户关注主播
- 📊 **统计信息** - 直播间统计数据

## 🔍 常见问题

#### 无法连接直播间
- 检查网络连接和直播间ID
- 确认直播间正在直播
- 重启程序重新连接

#### 连接成功但收不到消息  
- 选择人气较高的直播间测试
- 检查网络连接稳定性

## 📦 一键构建脚本

项目提供完整的构建脚本，位于 `build-scripts/` 目录：

### 🚀 推荐脚本

- **`quick-release.bat`** - 快速版本，无交互模式
- **`build-and-package.bat`** - 完整版本，详细进度显示  
- **`build-and-package.ps1`** - PowerShell版本，彩色输出

### 📁 构建输出

运行脚本后在 `发布版本\Release-YYMMDD-HHMM\` 目录生成四个版本的ZIP文件：

1. **DouyinDanmu-Minimal-YYMMDD-HHMM.zip** - 最小化版本
2. **DouyinDanmu-SingleFile-YYMMDD-HHMM.zip** - 单文件版本
3. **DouyinDanmu-Trimmed-YYMMDD-HHMM.zip** - 精简版本  
4. **DouyinDanmu-FastStart-YYMMDD-HHMM.zip** - 快速启动版本 ⚡

### 🔧 构建要求

- Windows 10/11 (x64)
- .NET 8 SDK
- PowerShell 5.0+ (用于压缩)

## 🔄 更新日志

### v1.3.0 - 项目清理版本
- ✅ 项目清理，移除测试和调试文件
- ✅ 代码优化，提升代码质量  
- ✅ 构建修复，确保所有版本正常构建
- ✅ 文档更新，精简README
- ✅ 性能提升，优化启动速度

## 🙏 致谢

本项目受到 [@saermart/DouyinLiveWebFetcher](https://github.com/saermart/DouyinLiveWebFetcher) 的启发而开发。

特别感谢：
- **[@saermart](https://github.com/saermart)** - 原始Python版本的作者
- **JavaScript签名算法** - 本项目使用的 `sign.js` 文件来源于该项目

---

**⚠️ 免责声明**: 本项目仅用于技术学习和研究，不承担任何使用风险。请用户自行承担使用责任。