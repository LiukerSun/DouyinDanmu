# 更新日志

## v1.1.1 - 2024-12-19

### 🔧 消息过滤优化

#### 新增功能
- ✅ **智能消息过滤**
  - 大幅扩展过滤规则，过滤掉60+种技术性和系统消息类型
  - 智能关键词过滤：自动过滤包含"Sync"、"Update"、"Config"等技术性关键词的消息
  - 只显示真正有意义的用户交互消息

- ✅ **未知消息类型统计**
  - 新增"未知类型统计"按钮，可查看所有遇到的未知消息类型及其出现频率
  - 调试模式：在控制台输出过滤和未知消息的详细信息
  - 帮助开发者了解还需要处理的消息类型

- ✅ **日志保存功能**
  - 支持TXT和CSV格式导出
  - 包含完整的消息统计信息
  - 自动生成带时间戳的文件名

#### 过滤的消息类型
**技术性消息（不显示）：**
- 数据同步：WebcastRoomDataSyncMessage、WebcastSyncMessage等
- 配置更新：WebcastConfigMessage、WebcastSettingMessage等
- 流媒体：WebcastRoomStreamAdaptationMessage等
- 广告推广：WebcastAdMessage、WebcastPromotionMessage等
- 游戏互动：WebcastGameMessage、WebcastActivityMessage等
- 系统监控：WebcastMonitorMessage、WebcastHealthMessage等

**保留显示的消息类型：**
- 用户交互：聊天、礼物、点赞、进场、关注、表情
- 统计信息：直播间人数、房间统计
- 重要控制：直播间状态变化

#### 用户体验改进
- 显著减少界面中的"未知消息类型"提示
- 提高消息列表的可读性
- 便于开发者调试和优化

---

## v1.1.0 - 2024-12-19

### 🎉 重大修复：WebSocket连接和消息解析

#### 问题描述
- WebSocket连接后立即被服务器关闭
- 无法正确解析用户名和聊天内容
- 大量未知消息类型显示

#### 修复内容

##### 1. WebSocket连接稳定性修复
- ✅ **JavaScript签名算法集成**
  - 使用Node.js执行原始的抖音JavaScript签名算法
  - 自动查找并使用 `sign.js` 文件
  - 支持回退到简化签名算法

- ✅ **心跳包协议修复**
  - 修正心跳包格式，使用正确的protobuf编码
  - 改为10秒间隔发送心跳包
  - 心跳包类型设置为 "hb"

- ✅ **ACK消息处理**
  - 实现ACK消息的自动发送
  - 正确解析服务器的needAck标志
  - 使用正确的protobuf格式构造ACK帧

##### 2. Protobuf消息解析重构
- ✅ **用户信息解析**
  - 正确解析用户昵称 (nick_name, field 3)
  - 正确解析用户ID (id_str, field 1028)
  - 支持嵌套User结构解析

- ✅ **聊天消息解析**
  - 正确解析聊天内容 (content, field 3)
  - 正确解析发送用户信息 (user, field 2)
  - 支持表情聊天消息 (WebcastEmojiChatMessage)

- ✅ **礼物消息解析**
  - 正确解析礼物数量 (combo_count, field 6)
  - 正确解析礼物名称 (gift.name, field 15.16)
  - 正确解析送礼用户信息 (user, field 7)

- ✅ **其他消息类型支持**
  - 点赞消息 (WebcastLikeMessage)
  - 进场消息 (WebcastMemberMessage)
  - 关注消息 (WebcastSocialMessage)
  - 直播间统计 (WebcastRoomUserSeqMessage)
  - 房间统计 (WebcastRoomStatsMessage)
  - 粉丝团消息 (WebcastFansclubMessage)

##### 3. 消息过滤优化
- ✅ **噪音消息过滤**
  - 过滤 WebcastRoomDataSyncMessage
  - 过滤 WebcastProfitGameStatusMessage
  - 过滤 WebcastInRoomBannerMessage
  - 过滤 WebcastGiftSortMessage
  - 过滤 WebcastRoomStreamAdaptationMessage

#### 技术细节

##### Protobuf字段映射
根据Python项目中的protobuf定义，正确映射了以下字段：

**ChatMessage:**
- field 2: User (用户信息)
- field 3: content (聊天内容)

**User:**
- field 3: nick_name (用户昵称)
- field 1028: id_str (用户ID字符串)

**GiftMessage:**
- field 6: combo_count (礼物数量)
- field 7: User (送礼用户)
- field 15: GiftStruct (礼物信息)

**GiftStruct:**
- field 16: name (礼物名称)

**LikeMessage:**
- field 2: count (点赞数量)
- field 5: User (点赞用户)

##### 解析流程优化
1. **PushFrame解析** → 提取logId和payload
2. **Gzip解压缩** → 解压缩payload数据
3. **Response解析** → 提取needAck、internalExt和消息列表
4. **ACK处理** → 自动发送ACK确认
5. **消息解析** → 根据method类型解析具体消息
6. **用户信息提取** → 递归解析嵌套的User结构

#### 测试结果
- ✅ WebSocket连接稳定，不再被服务器关闭
- ✅ 正确显示真实的用户昵称和聊天内容
- ✅ 正确解析礼物、点赞、进场等消息
- ✅ 大幅减少未知消息类型的显示
- ✅ 自动发送ACK确认，保持连接活跃

#### 依赖要求
- Windows 10/11
- .NET 6.0 Runtime
- Node.js (用于JavaScript签名算法)

#### 使用说明
1. 确保Node.js已安装
2. 确保sign.js文件在程序目录中
3. 启动程序，输入直播间ID
4. 点击"检查状态"确认直播状态
5. 点击"开始抓取"开始实时获取弹幕

---

## v1.0.0 - 2024-12-18

### 初始版本
- 基本的WebSocket连接功能
- 简化的protobuf解析
- WinForms图形界面
- 消息保存功能 