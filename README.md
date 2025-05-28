# 抖音直播弹幕抓取器 (C# WinForms版本)

这是一个使用C# .NET 6.0开发的抖音直播弹幕抓取器，具有图形用户界面。

## 功能特性

- 🎯 实时抓取抖音直播间弹幕、礼物、点赞、进场等消息
- 🔗 WebSocket连接直播间，支持心跳保活
- 🔐 使用原始JavaScript签名算法确保连接稳定
- 📊 实时统计消息数量
- 💾 支持导出聊天记录（TXT/CSV格式）
- 🎨 现代化的WinForms界面设计

## 系统要求

- Windows 10/11
- .NET 6.0 Runtime
- Node.js (用于执行JavaScript签名算法)

## 最新修复内容

### WebSocket连接问题修复

**问题描述：**
之前版本在WebSocket连接时出现错误，连接成功后立即被服务器关闭。

**修复内容：**

1. **JavaScript签名算法集成**
   - 使用Node.js执行原始的抖音JavaScript签名算法
   - 自动查找并使用 `sign.js` 文件

2. **心跳包协议修复**
   - 修正心跳包格式，使用正确的protobuf编码
   - 改为10秒间隔发送心跳包

3. **ACK消息处理**
   - 实现ACK消息的自动发送
   - 正确解析服务器的needAck标志

## 使用方法

1. 启动程序
2. 输入抖音直播间ID
3. 点击"检查状态"确认直播间正在直播
4. 点击"开始抓取"连接直播间
5. 实时查看弹幕和其他消息

## 支持的消息类型

- 💬 聊天消息
- 🎁 礼物消息  
- 👍 点赞消息
- 👋 进场消息
- ❤️ 关注消息
- 📊 直播间统计
- ⚠️ 控制消息

## 技术架构

### 核心组件

1. **DouyinLiveFetcher** - 主要的抓取器类
   - 负责连接抖音直播间 WebSocket
   - 处理消息接收和解析
   - 管理连接状态和心跳

2. **SignatureGenerator** - 签名生成器
   - 使用 JavaScript 引擎生成请求签名
   - 模拟浏览器环境

3. **ProtobufParser** - Protocol Buffer 解析器
   - 解析 WebSocket 接收到的二进制消息
   - 支持 Gzip 解压缩

4. **LiveMessage 模型** - 消息数据模型
   - 定义各种消息类型的数据结构
   - 支持聊天、礼物、点赞等多种消息

### 依赖包

- **Microsoft.ClearScript.V8** - JavaScript 引擎，用于签名生成
- **Newtonsoft.Json** - JSON 序列化和反序列化
- **System.Net.WebSockets.Client** - WebSocket 客户端

## 使用方法

1. **启动程序**
   - 运行 `DouyinLiveFetcher.WinForms.exe`

2. **输入直播间ID**
   - 在"直播间ID"文本框中输入抖音直播间的ID
   - 例如：`MS4wLjABAAAA...`

3. **检查直播状态**
   - 点击"检查状态"按钮验证直播间是否正在直播

4. **开始抓取**
   - 点击"开始抓取"按钮开始实时获取弹幕
   - 消息将显示在下方的列表中

5. **停止抓取**
   - 点击"停止"按钮停止抓取

6. **保存日志**
   - 点击"保存日志"按钮将消息保存到文件

## 编译和运行

### 环境要求

- .NET 6.0 SDK
- Windows 10/11
- Visual Studio 2022 或 Visual Studio Code

### 编译步骤

```bash
# 克隆项目
git clone <repository-url>

# 进入项目目录
cd DouyinLiveFetcher.WinForms

# 还原依赖包
dotnet restore

# 编译项目
dotnet build

# 运行项目
dotnet run
```

### 发布可执行文件

```bash
# 发布为单文件可执行程序
dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true
```

## 项目结构

```
DouyinLiveFetcher.WinForms/
├── Models/
│   └── LiveMessage.cs          # 消息数据模型
├── Services/
│   ├── DouyinLiveFetcher.cs    # 主抓取器类
│   ├── SignatureGenerator.cs   # 签名生成器
│   └── ProtobufParser.cs       # Protocol Buffer解析器
├── Form1.cs                    # 主窗体代码
├── Form1.Designer.cs           # 主窗体设计器
├── Program.cs                  # 程序入口点
└── DouyinLiveFetcher.WinForms.csproj  # 项目文件
```

## 注意事项

1. **签名算法**
   - 当前使用的是简化版签名算法
   - 实际使用时可能需要更新为完整的 sign.js 内容

2. **Protocol Buffer 解析**
   - 当前实现了基础的 protobuf 解析
   - 对于复杂消息可能需要完整的 protobuf 库支持

3. **网络连接**
   - 需要稳定的网络连接
   - 某些网络环境可能需要代理设置

4. **使用限制**
   - 请遵守抖音的使用条款
   - 仅用于学习和研究目的

## 故障排除

### 常见问题

1. **无法连接直播间**
   - 检查网络连接
   - 确认直播间ID正确
   - 检查直播间是否正在直播

2. **消息解析失败**
   - 可能是 protobuf 格式变化
   - 需要更新解析逻辑

3. **签名验证失败**
   - 可能需要更新签名算法
   - 检查 JavaScript 引擎是否正常工作

## 更新日志

### v1.0.0
- 初始版本发布
- 支持基本的弹幕抓取功能
- 实现图形界面
- 支持消息保存

## 许可证

本项目仅供学习和研究使用，请勿用于商业用途。

## 贡献

欢迎提交 Issue 和 Pull Request 来改进项目。

## 联系方式

如有问题或建议，请通过 GitHub Issues 联系。 