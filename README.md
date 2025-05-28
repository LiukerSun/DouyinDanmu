# 抖音直播弹幕抓取器 (DouyinDanmu)

[![.NET](https://img.shields.io/badge/.NET-6.0-blue.svg)](https://dotnet.microsoft.com/download/dotnet/6.0)
[![Platform](https://img.shields.io/badge/platform-Windows-lightgrey.svg)](https://www.microsoft.com/windows)
[![License](https://img.shields.io/badge/license-Educational-green.svg)](#许可证)

一个功能完整的抖音直播弹幕抓取器，使用C# .NET 6.0和WinForms开发，支持实时抓取、数据库存储、智能过滤和数据导出。

## ✨ 功能特性

### 🎯 核心功能
- **实时抓取** - 支持聊天、礼物、点赞、进场、关注等多种消息类型
- **稳定连接** - WebSocket长连接，自动心跳保活和ACK确认
- **智能过滤** - 过滤60+种技术性消息，只显示有意义的用户交互
- **数据持久化** - SQLite数据库存储，支持历史数据查询

### 📊 数据管理
- **实时统计** - 消息数量、用户数量等实时统计
- **数据查询** - 按时间、用户、消息类型等条件查询历史数据
- **数据导出** - 支持TXT/CSV格式导出，包含完整统计信息
- **用户关注** - 支持关注特定用户，单独显示其消息

### 🎨 用户界面
- **现代化界面** - 基于WinForms的直观图形界面
- **多列显示** - 聊天、进场、礼物、关注用户分类显示
- **右键菜单** - 快速添加/移除关注用户，复制用户信息
- **自动滚动** - 可选的消息自动滚动功能

### 🔧 技术特性
- **JavaScript签名** - 集成原始抖音签名算法，确保连接稳定
- **Protobuf解析** - 完整的Protocol Buffer消息解析
- **异步处理** - 全异步架构，界面响应流畅
- **错误处理** - 完善的异常处理和错误恢复机制

## 🚀 快速开始

### 系统要求
- **操作系统**: Windows 10/11
- **运行时**: .NET 6.0 Runtime
- **必须**: Node.js (用于JavaScript签名算法)

### 安装运行

#### 方式一：下载可执行文件
1. 从[Releases](../../releases)页面下载最新版本
2. 解压到任意目录
3. 双击运行 `DouyinDanmu.exe`

#### 方式二：从源码编译
```bash
# 克隆项目
git clone <repository-url>
cd DouyinDanmu

# 还原依赖
dotnet restore

# 编译运行
dotnet run

# 或发布为单文件可执行程序
dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true
```

### 使用步骤
1. **启动程序** - 运行DouyinDanmu.exe
2. **输入直播间ID** - 在文本框中输入抖音直播间ID
3. **检查状态** - 点击"检查状态"确认直播间正在直播
4. **开始抓取** - 点击"连接"按钮开始实时抓取
5. **查看消息** - 在四个分类面板中查看不同类型的消息
6. **管理关注** - 右键点击用户可添加到关注列表
7. **查询历史** - 点击"数据库"按钮查询历史数据
8. **导出数据** - 使用"保存日志"或数据库查询界面导出数据

## 📁 项目结构

```
DouyinDanmu/
├── Models/                     # 数据模型
│   ├── LiveMessage.cs         # 消息基类和各种消息类型
│   ├── AppSettings.cs         # 应用程序设置
│   └── UserInfo.cs           # 用户信息模型
├── Services/                   # 核心服务
│   ├── DouyinLiveFetcher.cs   # 主抓取器类
│   ├── DatabaseService.cs     # 数据库服务
│   ├── SignatureGenerator.cs  # 签名生成器
│   ├── ProtobufParser.cs      # Protobuf解析器
│   └── SettingsManager.cs     # 设置管理器
├── Forms/                      # 窗体文件
│   ├── Form1.cs              # 主窗体
│   ├── DatabaseQueryForm.cs  # 数据库查询窗体
│   └── SettingsForm.cs       # 设置窗体
├── sign.js                    # JavaScript签名算法
└── DouyinDanmu.csproj        # 项目文件
```

## 🔧 技术架构

### 核心组件

#### DouyinLiveFetcher
- **职责**: WebSocket连接管理、消息接收
- **特性**: 自动重连、心跳保活、ACK确认
- **协议**: 完整的抖音WebSocket协议实现

#### DatabaseService
- **职责**: 数据持久化、查询统计
- **数据库**: SQLite，支持事务处理
- **表结构**: chat_messages、member_messages、interaction_messages

#### ProtobufParser
- **职责**: Protocol Buffer消息解析
- **支持**: Gzip解压、嵌套结构解析
- **消息类型**: 60+种消息类型的完整解析

#### SignatureGenerator
- **职责**: 生成请求签名
- **算法**: 集成原始JavaScript签名算法
- **回退**: 支持简化签名算法作为备用

### 数据流程

```
抖音服务器 → WebSocket → Protobuf解析 → 消息分类 → 界面显示
                                    ↓
                              数据库存储 → 查询统计 → 数据导出
```

## 📋 支持的消息类型

### 用户交互消息
- 💬 **聊天消息** (WebcastChatMessage) - 用户发送的文字消息
- 🎁 **礼物消息** (WebcastGiftMessage) - 用户送礼信息
- 👍 **点赞消息** (WebcastLikeMessage) - 用户点赞行为
- 👋 **进场消息** (WebcastMemberMessage) - 用户进入直播间
- ❤️ **关注消息** (WebcastSocialMessage) - 用户关注主播
- 😊 **表情消息** (WebcastEmojiChatMessage) - 表情聊天

### 统计信息
- 📊 **房间统计** (WebcastRoomStatsMessage) - 直播间统计数据
- 👥 **用户序列** (WebcastRoomUserSeqMessage) - 在线用户信息
- 🎭 **粉丝团** (WebcastFansclubMessage) - 粉丝团相关消息

### 智能过滤
程序会自动过滤以下技术性消息，确保界面清洁：
- 数据同步消息 (WebcastRoomDataSyncMessage等)
- 配置更新消息 (WebcastConfigMessage等)
- 广告推广消息 (WebcastAdMessage等)
- 游戏互动消息 (WebcastGameMessage等)
- 系统监控消息 (WebcastMonitorMessage等)

## 🗄️ 数据库设计

### 表结构

#### chat_messages (聊天消息)
```sql
CREATE TABLE chat_messages (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    live_id TEXT NOT NULL,
    timestamp DATETIME NOT NULL,
    user_id TEXT,
    user_name TEXT,
    fans_club_level INTEGER DEFAULT 0,
    pay_grade_level INTEGER DEFAULT 0,
    content TEXT
);
```

#### member_messages (进场消息)
```sql
CREATE TABLE member_messages (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    live_id TEXT NOT NULL,
    timestamp DATETIME NOT NULL,
    user_id TEXT,
    user_name TEXT,
    fans_club_level INTEGER DEFAULT 0,
    pay_grade_level INTEGER DEFAULT 0,
    member_type TEXT
);
```

#### interaction_messages (互动消息)
```sql
CREATE TABLE interaction_messages (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    live_id TEXT NOT NULL,
    timestamp DATETIME NOT NULL,
    message_type TEXT NOT NULL,
    user_id TEXT,
    user_name TEXT,
    fans_club_level INTEGER DEFAULT 0,
    pay_grade_level INTEGER DEFAULT 0,
    content TEXT,
    gift_name TEXT,
    gift_count INTEGER DEFAULT 0,
    like_count INTEGER DEFAULT 0
);
```

### 数据库特性
- **自动索引**: 为查询优化创建索引
- **事务支持**: 确保数据一致性
- **统计查询**: 支持复杂的统计查询
- **数据导出**: 支持CSV/TXT格式导出

## ⚙️ 配置说明

### 应用程序设置
程序会自动保存以下设置到 `%APPDATA%/DouyinLiveFetcher/settings.json`:

```json
{
  "LiveId": "直播间ID",
  "WatchedUserIds": ["关注的用户ID列表"],
  "AutoScroll": true,
  "WindowX": 100,
  "WindowY": 100,
  "WindowWidth": 1200,
  "WindowHeight": 800,
  "WindowState": 0
}
```

### 数据库位置
- **优先位置**: 程序所在目录/douyin_live_messages.db
- **备用位置**: %APPDATA%/DouyinLiveFetcher/douyin_live_messages.db

## 🔍 故障排除

### 常见问题

#### 1. 无法连接直播间
**可能原因**:
- 网络连接问题
- 直播间ID错误
- 直播间未开播

**解决方案**:
- 检查网络连接
- 确认直播间ID正确
- 使用"检查状态"功能验证直播状态

#### 2. 消息解析失败
**可能原因**:
- Protobuf格式变化
- 签名算法失效

**解决方案**:
- 查看状态栏错误信息
- 更新sign.js文件
- 检查程序版本是否最新

#### 3. 数据库初始化失败
**可能原因**:
- 权限不足
- 磁盘空间不足
- 文件被占用

**解决方案**:
- 以管理员身份运行
- 检查磁盘空间
- 关闭其他可能占用数据库的程序

#### 4. Node.js相关错误
**可能原因**:
- Node.js未安装
- sign.js文件缺失

**解决方案**:
- 安装Node.js
- 确保sign.js文件在程序目录
- 程序会自动回退到简化签名算法

### 调试模式
程序在控制台输出详细的调试信息，包括：
- 过滤的消息类型
- 未知消息类型统计
- 数据库操作日志
- 网络连接状态

## 📊 性能优化

### 内存管理
- 自动清理过期消息
- 限制界面显示数量
- 及时释放资源

### 数据库优化
- 使用索引加速查询
- 批量插入提高性能
- 定期清理过期数据

### 网络优化
- 连接池复用
- 自动重连机制
- 心跳包优化

## 🔄 更新日志

### v1.1.1 (2024-12-19)
- ✅ 智能消息过滤，过滤60+种技术性消息
- ✅ 新增未知消息类型统计功能
- ✅ 优化日志保存功能，支持TXT/CSV格式
- ✅ 提高消息列表可读性

### v1.1.0 (2024-12-19)
- ✅ 重大修复：WebSocket连接稳定性
- ✅ JavaScript签名算法集成
- ✅ Protobuf消息解析重构
- ✅ 正确解析用户信息和消息内容
- ✅ 实现ACK消息自动处理

### v1.0.0 (2024-12-18)
- 🎉 初始版本发布
- ✅ 基本的WebSocket连接功能
- ✅ WinForms图形界面
- ✅ 消息保存功能

详细更新日志请查看 [CHANGELOG.md](CHANGELOG.md)

## 🤝 贡献指南

### 开发环境
- Visual Studio 2022 或 Visual Studio Code
- .NET 6.0 SDK
- Git

### 贡献流程
1. Fork 项目
2. 创建功能分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 创建 Pull Request

### 代码规范
- 遵循C#编码规范
- 添加必要的注释
- 编写单元测试
- 更新相关文档

## 📄 许可证

本项目仅供**学习和研究**使用，请勿用于商业用途。

使用本项目时请遵守：
- 抖音平台的使用条款
- 相关法律法规
- 不得用于恶意用途

## 📞 联系方式

- **Issues**: [GitHub Issues](../../issues)
- **讨论**: [GitHub Discussions](../../discussions)

## 🙏 致谢

### 项目启发
本项目受到 [@saermart/DouyinLiveWebFetcher](https://github.com/saermart/DouyinLiveWebFetcher) 的启发而开发。该项目是一个优秀的Python版本抖音直播弹幕抓取器，为我们提供了宝贵的技术参考和实现思路。

特别感谢：
- **[@saermart](https://github.com/saermart)** - 原始Python版本的作者，提供了完整的Protobuf解析和WebSocket连接实现
- **JavaScript签名算法** - 本项目使用的 `sign.js` 文件来源于该项目
- **协议分析** - 抖音WebSocket协议的分析和实现参考

### 项目差异
虽然受到启发，但本项目有以下特色：
- **C# .NET实现** - 使用C#重新实现，提供Windows原生体验
- **数据库存储** - 集成SQLite数据库，支持历史数据查询
- **图形界面** - 基于WinForms的现代化图形界面
- **智能过滤** - 更完善的消息过滤和分类显示
- **用户管理** - 支持关注特定用户功能

感谢所有为项目做出贡献的开发者和用户！

---

**⚠️ 免责声明**: 本项目仅用于技术学习和研究，不承担任何使用风险。请用户自行承担使用责任。 