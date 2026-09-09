# Protobuf 增量解析

2026-09-09：参考 [Protobuf 逆向解析两种方法](https://www.cnblogs.com/wyc-1009/p/17547994.html)，使用本机构建的 `protoc --decode_raw` 核对保存的消息载荷，再更新 C++ 解析。运行时继续使用已有 Protobuf 库，不增加 Python 或 blackboxprotobuf 依赖。

**解码过程零失败不等于完整解析。** 第一轮仅增加结构候选与少量消息头；当时 939 条样本全部仍为 partial。本轮进一步补全高频的有名字段和嵌套类型，以剩余未知字段、实际 decoded/partial 数量衡量效果。

## 本轮实际验证（v3）

对同一批 944 条真实载荷做旧版/新版对照：939 条原基准样本，加上从 469 条旧隔离帧中提取的 4 条 RoomIndicator 与 1 条 ChatLike。样本覆盖 23 种方法；在独立临时数据库运行，没有修改原归档。

| 指标 | 修改前（v2） | 修改后（v3） |
| --- | ---: | ---: |
| 当前 schema 无未知字段且未被保守标记的 decoded 消息 | 0 | 934 |
| partial 消息 | 939 | 10 |
| unmapped 消息 | 5 | 0 |
| 未知/不透明字段分析条目 | 17,582 | 22 |
| 重解析失败 | 0 | 0 |

主要结果：进场 295/295、榜单 257/257、在线人数 221/221、点赞 38/38、关注/分享 9/9、粉丝团 3/3 都达到当前 schema 完整解码；弹幕 67/68、礼物 9/11。礼物排序 10 条、常驻访客 8 条、低在线引导 6 条、CommonDot 1 条，以及新增归档消息 5 条也完整解码。额外榜单用户条目为 1,025 条，全部保留。

验证不只检查返回码：逐条比较原始 Base64、方法、消息/帧 ID、WRDS 版本、wire 预览及既有业务字段，均保持一致；只读预演前后数据库 SHA-256 相同；第二次 apply 的 scanned/updated 为 0。详情 JSON 的同口径序列化大小由 71,734,688 字节降至 15,149,387 字节，主要因为正式字段替代了冗长候选树，不是裁剪原始数据。

Windows 后端构建成功，`room_metadata`、`gift_store`、`event_parser` 三组 CTest 全部通过。回归覆盖重复榜单/勋章、嵌套 map、packed ID、64 位精度、指导消息与行为统计隔离、未知新字段回退，以及原有礼物统计、游标和历史修复保留。私有完整报告在 `.codex/reference/semantic-validation.json`，未纳入源码。

## 有名字段与嵌套结构扩展（v3）

| 范围 | 本轮实际补全 |
| --- | --- |
| User | 第二版勋章图片列表、用户属性（禁言/管理员/权限列表）、授权码、脱敏昵称、webcast_uid、认证信息、公屏勋章、头像边框、粉丝群链接、订阅标记、隐私状态、连麦状态、新图标和城市 |
| PayGrade.buffInfo | 等级增益、状态码、结束时间、64 位键值统计和增益勋章；等级增益不覆盖财富等级 |
| PublicAreaCommon | 修正消费量、送礼数、公屏优先级等 5 个错误的 string 类型为 int64；这些消息详情中的原始计数不合并进平台业务统计 |
| LikeMessage / Common / Image / 入场特效 | 点赞消息的公屏信息、公共头的 room_id_str、图片的 flex/text 设置，以及入场特效中的重复勋章列表 |
| GiftMessage / GiftStruct | 礼物托盘、热度信息、托盘整数类型、触发词、连送选项、提示文字、WebP 图片、特效映射、资源列表等；不改变礼物累计方式 |
| RoomRankMessage 字段 3 | 按 CDN 定义解析重复的 audience_ranks，展开 1,025 条用户资料及 score/rank/delta 等列；不将分数换算成礼物金额，不把榜单用户作为消息发送者 |
| GiftSort / LowPcuGuide / LowPcuGuideChat | 礼物排序的 ID 列表、时间、策略、事件属性；低在线引导文字、状态、延迟和重复表情配置。原先的 bytes 已改为对应消息类型 |
| ResidentGuest / CommonDot | 两类访客允许配置；面板提示映射、项目提示映射和具体提示 ID/样式 |
| ChatLike / RoomIndicator | 从旧隔离归档补出的 5 条消息：弹幕 ID 到点赞计数/版本的映射，以及直播间指标的业务类型、状态、图文内容。两者不直接累加普通点赞/在线人数统计 |

字段编号及名字参考以下固定版本，并逐条检查本机样本的实际 wire 类型。优先使用抖音自身 CDN 的 decoder 表；代码仅作为静态文本读取，没有执行下载的脚本。

- [抖音 CDN 的 live-schema-im.19dd06d0.js](https://lf-webcast-platform.bytetos.com/obj/webcast-platform-cdn/webcast/douyin_live/chunks/live-schema-im.19dd06d0.js)：榜单、礼物排序、引导、公屏、入场特效、提示映射及新增方法的直接证据。下载文件 SHA-256：`76d044322e73fe50410a45f9bd4c97e35c387b734cb7d1c34271e7c7fce1e3f8`。
- [脚本及协议整理来源](https://github.com/Remember-the-past/douyin_proto/tree/1f8804c7bb8ccf32717d63796c89a081581a3561)。整理后的 `.proto` 在 audience_ranks 和 badge_list 上漏写 repeated；直接 JS decoder 对应模式为 3，本机样本也有多条，所以保留 repeated。未整份照搬其定义。

- [scx User](https://github.com/scx567888/live-room-watcher/blob/aa62f538c7b186a17ff5fc6c33ed1d289823355e/src/main/proto/douyin_hack/webcast/data/User.proto)、[PublicAreaCommon](https://github.com/scx567888/live-room-watcher/blob/aa62f538c7b186a17ff5fc6c33ed1d289823355e/src/main/proto/douyin_hack/webcast/im/PublicAreaCommon.proto)、[Common](https://github.com/scx567888/live-room-watcher/blob/aa62f538c7b186a17ff5fc6c33ed1d289823355e/src/main/proto/douyin_hack/webcast/im/Common.proto)。
- [f2 的序列化协议描述符](https://github.com/Johnserf-Seed/f2/blob/7dab3e2ffffaa2535834d28fca99dbc2e89fa9d3/f2/apps/douyin/proto/douyin_webcast_pb2.py)：仅用 AST 提取静态 descriptor 字节，不执行参考 Python 文件。描述符 SHA-256：`2f7dc8cde136f3f059a50e47437e7a177a754ccaa7936436cf93ace5287e9990`。

f2 的若干 64 位整数被定义为 string；CDN decoder 的 `int64String` 是“按 varint 读取，再表示为字符串”，不代表 wire string。本轮用 int64 接收，JSON 继续保留精确十进制字符串。

这些字段名是参考协议支持的名称，未解释的枚举值继续保存原始码；`webcast_uid`、脱敏昵称和增益等级不替换现有 UID、昵称或财富等级。

### 当前仍未补全的范围

基准样本中仍保留以下未知字段，不以解析成功或编号占位代替语义确认：

| 方法 | 剩余缺口 |
| --- | --- |
| ChatMessage | 字段 41；相关 PublicAreaCommon 的字段 16 |
| GiftMessage | 字段 37；GiftTrayInfo 的字段 24 |
| RoomCommentTopicMessage | 字段 3；话题条目字段 1/2/4 |
| ProfileViewMessage | 字段 4/5 |
| ProfitGameStatusMessage | 字段 19 |
| RoomDataSyncMessage | payload 的具体业务结构 |
| BattleStatus / PrizeNotice | 仍有只按编号命名的字段，保持 partial |
| VisibilityRangeChange | 只有公共头样本，保持 partial |

`decoded` 仅表示该条载荷的字段都被当前 schema 接收，不表示枚举的所有取值、平台业务规则或未来新增字段均已掌握。

## 第一轮补入的消息结构

| 消息 | 更新 | 证据范围 |
| --- | --- | --- |
| `BattleStatusMessage` | 公共头、状态码、2/3/6/7/8/9 字段 | 保存样本与参考结构一致；编号字段不命名为用户、时间或时长，不推断单位 |
| `PrizeNoticeMessage` | 公共头、2/6/8 数值字段 | 保存样本可展开；不据此推断中奖人或奖励行为 |
| `VisibilityRangeChangeMessage` | 公共头 | 样本只包含公共头；其余字段继续未知，始终保持 partial |
| `RoomDataSyncMessage` | 修正 `roomID` 和 `version` 的 wire 类型 | 实际字段 2/4 都是 varint，使用 uint64；JSON 仍输出十进制字符串 |

第一轮这三种新方法的样本每类仅一条，不能据此认为完整协议已还原。原始样本留在本机，不进入源码或测试夹具。测试使用独立手写 wire 字节和虚构值。

参考结构来自本机 2026-09-08 保存的 [opedium 协议](https://github.com/opedium/douyin-live-proto/blob/master/douyin_live.proto)，快照 SHA-256 为 `1a2d059c243480ef4119cddb06fbd1615cb124bbce83e7b66f32591367cff268`。这是社区逆向资料；其中 RoomDataSync 字段 4 被定义为 string，与本机 varint 样本冲突，因此采用实际字节证据。

## 无 schema 的候选解释

消息详情中的 `field_analysis.version` 当前为 3，事件 `parser_version` 为 7。版本 6 补上直播通知文本模板的展开：当正文为空白或通用占位文案时，使用 `common.display_text` 中的模板和文本片段，必要时回退到 `common.describe`。原始解码字段不变，模板中提到的用户不作为发送者。版本 2 引入的字段路径、嵌套 Protobuf、UTF-8、JSON 和原始 Base64 保留，包括：

- `numeric_candidates`：varint 的 int64 / ZigZag sint64，fixed32 的 sfixed32 / float，fixed64 的 sfixed64 / double。
- `packed_candidates`：对非文本 bytes 尝试无 tag 的 varint、fixed32、fixed64 数组；每种编码必须完整消费数据，拒绝截断或超过 64 位的 varint。合法的多个解释同时保留。
- `wire_valid`：区分合法空消息和无法解析的 wire。已知 schema 解析或 JSON 转换失败时，也保留无 schema 分析。

这些解释均标记 `candidate_only`，原始整数仍作为精确十进制字符串保留；非有限浮点值用 `NaN` / `Infinity` / `-Infinity` 字符串表示。候选解释不会提升 `parse_status` 或生成关注、送礼等业务事件。可读文本不尝试 packed，以免普通文字产生大量无意义数值。

限制：字段树 512 个条目，schema 遍历 8192 次/32 层，候选嵌套 5 层，单块 bytes 最多检查 16 KiB。packed 每个解释最多预览 128 个数值，整条消息共享 512 个数值预算；超限标记 `truncated`。以上均只限制诊断预览，完整 `payload_base64` 保留。

[Protobuf 官方 wire 格式说明](https://protobuf.dev/programming-guides/encoding/)明确说明：wire 类型 2 同时用于字符串、bytes、嵌套消息和 packed；字段名及精确类型需要 schema。无 schema 解码成功本身不是业务语义的证据。

## 复核原始文件

便携版源码构建后，protoc 位于 `backend/build_portable/_deps/protobuf-build/protoc.exe`。对从完整 `payload_base64` 还原的单条消息文件，可在仓库根目录执行以下 Python 示例；二进制直接通过 stdin 传给 protoc，避免 PowerShell 文本管道转换字节：

```python
from pathlib import Path
import subprocess

result = subprocess.run(
    ['backend/build_portable/_deps/protobuf-build/protoc.exe', '--decode_raw'],
    input=Path('message.bin').read_bytes(), capture_output=True, check=True,
)
Path('message.decode.txt').write_bytes(result.stdout)
```

需要输入单条 `Message.payload`；采集的外层 PushFrame、gzip 内容须先按项目流程解包，不能直接当作业务消息。

## 历史详情升级

构建新后端后，设置 `DATABASE_PATH` 指向要检查的数据库，运行 `douyin-pipeline --redecode-details` 可只读预演。加 `--apply` 才写入。详情分析版本过旧的记录会重新处理；解析器版本 6/7 的文案升级仅重处理四类通知，不重写无关的聊天或礼物历史。已处理记录再次执行时跳过。空白或通用占位文案的 RoomMessage/CommonTextMessage/NotifyMessage/FansclubMessage 会补全文本，并同步历史展示记录；已有具体正文、用户资料、原始载荷和业务计数保留。

维护命令只更新解析详情及元数据；原 unknown 消息可更新类型与摘要。不会重新投递事件、推进游标、重算统计或覆盖已有业务修复。生产数据升级前请按项目备份流程备份。Schema 扩展先在临时样本数据库验证；通知文案修复另在本地 debug 实例备份后应用，并核对接口和页面展示。

## 第一轮结构试验结果（v2 对照基线）

- Windows 原生后端构建成功，CTest 的 `room_metadata`、`gift_store`、`event_parser` 三组全部通过。测试覆盖 wire 独立编码、新增 schema、数值边界、packed 溢出/截断、预算上限和版本 1 详情升级。
- 本机 939 条保存样本覆盖 21 种方法。先用旧解析器生成版本 1，再用新解析器只读预演、应用升级；两次均扫描 939 条，失败 0。新增识别 3 条原 unmapped 消息，分别为 BattleStatus、PrizeNotice、VisibilityRangeChange，均保持 partial。
- 升级后生成 17,564 个顶层分析条目；递归结构中 31,536 处数值字段提供候选解释，15,296 处 bytes 提供 packed 候选。这些是结构候选数量，不是确认的业务字段数量。
- 样本数据库的只读预演前后 SHA-256 相同；逐条核对原始 Base64、method、消息/帧 ID、WRDS 版本、wire 预览及既有业务字段无变化。第二次 apply 的 scanned/updated 均为 0。
- 临时样本数据库只用于详情迁移验证；真实 Store 的统计及游标保持不变由 `gift_store` 回归覆盖。私有验证报告在本机 `.codex/reference/protobuf-v2-validation.json`，不纳入源码。

### 粉丝团展示文案（解析器版本 7）

粉丝团消息优先保留平台正文，其次展开 `commonInfo.display_text` 或读取 `describe`。动作 1/2 的升级、加入含义有本机正文样本佐证；缺少正文时展示动作及平台报告的团名、等级。动作 6/7 及其他未确认动作只展示当前团名和等级，并注明具体动作未确认，不推断点亮、退团、购买或等级变化。原始动作码保留在协议详情中。历史重解析只补全空白或“粉丝团事件”占位文案，已有正文和业务计数不变。

### 已知待办：粉丝团动作 6 / 7

- 尚未确认这两个动作码对应的具体行为，暂时保留“具体动作未确认”的展示，不把当前状态直接解释为一次加入、升级或点亮行为。
- 本地验证集的 186 条粉丝团消息中，动作 1/2 各 15 条，动作 6 有 154 条、动作 7 有 2 条；后两类均未携带正文或显示模板。相关原始载荷继续保存在本地，不纳入源码。
- 后续线索：抖音 [client-entry.57a2f398.js](https://lf-webcast-platform.bytetos.com/obj/webcast-platform-cdn/webcast/douyin_live/client-entry.57a2f398.js) 包含 `NOT_FANS=0`、`FANS=1`、`DARK_FANS=2` 的状态枚举；[RoomInfoBar.16bde485.js](https://lf-webcast-platform.bytetos.com/obj/webcast-platform-cdn/webcast/douyin_live/chunks/RoomInfoBar.16bde485.js) 收到当前用户的粉丝团消息后刷新资料。这些证据说明状态和刷新流程，尚不足以确认动作 6/7 的语义。
- 后续验收：找到明确的动作枚举或消费分支，并与原始样本交叉验证，再补回归测试、升级展示文案和历史记录。不要仅根据灯牌图片颜色或当前等级推断动作。
