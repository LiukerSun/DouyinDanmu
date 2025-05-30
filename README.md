# 抖音直播弹幕采集工具 (DouyinDanmu)

[![.NET](https://img.shields.io/badge/.NET-8.0-blue.svg)](https://dotnet.microsoft.com/download/dotnet/8.0)
[![Platform](https://img.shields.io/badge/platform-Windows-lightgrey.svg)](https://www.microsoft.com/windows)
[![License](https://img.shields.io/badge/license-Educational-green.svg)](#许可证)
[![Version](https://img.shields.io/badge/version-1.5.0-brightgreen.svg)](#版本历史)

一个基于 C# .NET 8.0 和 WinForms 开发的抖音直播弹幕实时采集工具，支持实时抓取、数据库存储、智能过滤和数据导出。

## ✨ 核心功能

### 🔄 实时采集
- **WebSocket连接** - 通过WebSocket实时接收抖音直播间消息
- **多类型消息** - 支持聊天、礼物、点赞、进场、关注等多种消息类型
- **自动重连** - 网络异常时自动重连，保证数据采集的连续性
- **JavaScript签名** - 集成JavaScript引擎生成请求签名

### 💾 数据管理
- **SQLite数据库** - 本地数据库持久化存储所有消息
- **批量操作** - 优化的批量插入和查询，提升性能
- **数据导出** - 支持TXT/CSV格式导出，包含完整统计信息
- **历史查询** - 内置数据库查询界面，支持条件筛选

### 🎯 智能过滤
- **用户关注** - 支持关注特定用户，高亮显示其消息
- **消息分类** - 按消息类型分类显示（聊天/礼物/进场/关注）
- **实时统计** - 显示消息数量、用户数量等实时统计信息

### 🖥️ 现代界面
- **分类显示** - 多个ListView分别显示不同类型的消息
- **右键菜单** - 支持用户关注、消息复制等快捷操作
- **自动滚动** - 可配置的自动滚动功能
- **双缓冲** - 减少界面闪烁，提升用户体验

## 🏗️ 技术架构

### 分层设计
```
DouyinDanmu/
├── Models/                 # 数据模型层
│   ├── LiveMessage.cs     # 消息模型定义
│   ├── AppConfig.cs       # 配置模型
│   ├── AppSettings.cs     # 应用设置
│   └── UserInfo.cs        # 用户信息模型
├── Services/              # 服务层
│   ├── DouyinLiveFetcher.cs    # 核心抓取服务
│   ├── DatabaseService.cs     # 数据库服务
│   ├── SignatureGenerator.cs  # 签名生成服务
│   ├── ProtobufParser.cs      # Protobuf解析服务
│   ├── ConnectionManager.cs   # 连接管理服务
│   ├── UIUpdateService.cs     # UI更新服务
│   ├── PerformanceMonitor.cs  # 性能监控服务
│   ├── LoggingService.cs      # 日志服务
│   └── SettingsManager.cs    # 设置管理服务
├── UI层/
│   ├── Form1.cs              # 主窗体
│   ├── SettingsForm.cs       # 设置窗体
│   └── DatabaseQueryForm.cs  # 数据库查询窗体
└── Tests/                    # 测试项目
```

### 核心技术栈
- **UI框架**: WinForms (.NET 8.0)
- **数据库**: SQLite + Microsoft.Data.Sqlite
- **网络通信**: WebSocket + HttpClient
- **JavaScript引擎**: Microsoft.ClearScript.V8
- **JSON处理**: System.Text.Json
- **测试框架**: xUnit + Moq + FluentAssertions

### 性能优化
- **内存池**: 使用ArrayPool减少内存分配
- **批量处理**: UI更新和数据库操作均采用批量处理
- **双缓冲**: ListView双缓冲减少界面闪烁
- **异步处理**: 全面使用async/await模式
- **连接池**: 数据库连接池优化

## 🚀 快速开始

### 系统要求
- **操作系统**: Windows 10/11 (x64)
- **运行时**: .NET 8.0 Runtime (自包含版本无需安装)

### 📦 下载运行

从 [Releases](../../releases) 页面下载最新版本，或使用构建脚本编译：

```bash
# 克隆项目
git clone git@github.com:LiukerSun/DouyinDanmu.git
cd DouyinDanmu

# 快速构建所有版本
cd build-scripts
.\quick-release.bat

# 或者直接运行开发版本
dotnet run
```

### 构建版本说明
构建脚本会生成四个不同的版本：

1. **最小化版本** - 需要.NET 8.0运行时，体积最小
2. **单文件版本** - 单个可执行文件，自包含
3. **精简版本** - 经过裁剪优化，自包含
4. **快速启动版本** - 启动速度最快，自包含

### 使用步骤
1. 运行 `DouyinDanmu.exe`
2. 输入抖音直播间ID（支持直播间URL或纯数字ID）
3. 点击"连接"开始采集
4. 在不同标签页查看分类消息：
   - **聊天** - 用户发送的文字消息
   - **礼物/关注** - 礼物和关注消息
   - **进场** - 用户进入直播间消息
   - **关注用户** - 已关注用户的消息
5. 右键用户名可添加关注
6. 使用"保存日志"导出数据

## 📋 支持的消息类型

| 消息类型 | 说明 | 包含信息 |
|---------|------|----------|
| 💬 **Chat** | 聊天消息 | 用户名、内容、时间戳、粉丝团等级 |
| 🎁 **Gift** | 礼物消息 | 用户名、礼物名称、数量、价值 |
| 👍 **Like** | 点赞消息 | 用户名、点赞数量 |
| 👋 **Member** | 进场消息 | 用户名、进场时间 |
| ❤️ **Social** | 关注消息 | 用户名、关注时间 |
| 📊 **RoomStats** | 直播间统计 | 在线人数、总观看数 |
| 🏆 **Fansclub** | 粉丝团消息 | 粉丝团相关活动 |
| 🎭 **EmojiChat** | 表情聊天 | 表情消息 |

## ⚙️ 配置选项

### 界面设置
- **最大显示消息数量**: 100-10000条
- **UI更新间隔**: 50-1000毫秒
- **自动滚动**: 开启/关闭
- **字体大小**: 8-20pt
- **主题模式**: 明亮/暗黑/自动

### 性能设置
- **批量处理大小**: 10-500条
- **性能监控**: 开启/关闭
- **监控间隔**: 1-60秒
- **内存清理阈值**: 50-1000MB
- **自动GC**: 开启/关闭

### 网络设置
- **连接超时**: 5-60秒
- **重连间隔**: 1-30秒
- **最大重连次数**: 0-10次
- **心跳间隔**: 10-120秒

### 数据库设置
- **批量插入大小**: 10-1000条
- **数据保留天数**: 1-365天
- **自动清理**: 开启/关闭
- **连接池大小**: 1-20个连接

## 🧪 测试

项目包含完整的单元测试框架：

```bash
# 运行所有测试
dotnet test

# 运行特定测试
dotnet test --filter "Category=Unit"

# 生成覆盖率报告
dotnet test --collect:"XPlat Code Coverage"
```

## 🔍 常见问题

### 无法连接直播间
1. 检查网络连接状态
2. 确认直播间ID正确且正在直播
3. 查看应用程序日志获取详细错误信息
4. 尝试重启应用程序

### 连接成功但收不到消息
1. 选择人气较高的直播间进行测试
2. 检查网络连接的稳定性
3. 在设置中启用详细日志记录
4. 确认JavaScript引擎正常工作

### 性能问题
1. 调整最大显示消息数量
2. 启用自动垃圾回收
3. 降低批量处理大小
4. 关闭不必要的性能监控

### 数据导出问题
1. 确保有足够的磁盘空间
2. 检查导出目录的写入权限
3. 尝试导出较小的时间范围

## 🔄 版本历史

### v1.5.0 - 当前版本
- ✅ 完整的分层架构重构
- ✅ 新增配置管理系统
- ✅ 实现结构化日志系统
- ✅ 优化连接管理和重连机制
- ✅ 添加UI更新服务
- ✅ 统一使用System.Text.Json
- ✅ 新增单元测试框架
- ✅ 完善错误处理和资源管理
- ✅ 性能优化和内存管理改进

## 🛠️ 开发说明

### 开发环境
- Visual Studio 2022 或 VS Code
- .NET 8.0 SDK
- Windows 10/11

### 项目结构说明
- **Models**: 数据模型和配置模型
- **Services**: 核心业务逻辑服务
- **UI**: WinForms界面层
- **Tests**: 单元测试项目
- **build-scripts**: 构建和发布脚本

### 贡献指南
1. Fork 项目
2. 创建功能分支
3. 提交更改
4. 推送到分支
5. 创建 Pull Request

## 📄 许可证

本项目仅用于教育和学习目的。请遵守相关法律法规，不得用于商业用途。

## 🙏 致谢

本项目受到以下项目的启发：
- [@saermart/DouyinLiveWebFetcher](https://github.com/saermart/DouyinLiveWebFetcher) - 原始Python版本
- JavaScript签名算法来源于上述项目

---

**⚠️ 免责声明**: 本项目仅用于技术学习和研究，请用户自行承担使用风险和责任。