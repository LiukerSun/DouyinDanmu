# 观众统计接口

统计接口用于礼物榜、弹幕榜及用户详情。走现有 `/api/` 登录网关，前端无需加载全部消息；桌面版与容器版使用同一组 C++ 接口。仅统计本系统保存的抖音记录，不代表平台全量数据或实际付款金额。

前端入口位于单个直播间内的“本房间排行榜”，提供弹幕榜、礼物榜及今天／近 7 天／全部已采集记录筛选。所有榜单请求明确携带当前 `room`，不展示跨房间排名；切换房间重置榜单状态。点“本房间记录”查询该用户在相同房间、时间范围和快照内的全部行为，返回后保留榜单位置。刷新榜单才获取新快照，翻页期间不自动重排。

## 路由

| GET 路径 | 用途 |
| --- | --- |
| `/api/analytics/chat-ranking` | 按发言条数降序排列的用户榜 |
| `/api/analytics/gift-ranking` | 按已知礼物价值或数量降序排列的用户榜 |
| `/api/analytics/users/{uid}/summary` | 一个用户在筛选范围内的统计摘要 |
| `/api/analytics/users/{uid}/rooms` | 一个用户在各直播间的统计，按最近出现时间倒序分页 |
| `/api/messages/search` | 既有消息查询，增加时间及统计快照筛选、礼物统计增量 |

### 通用统计参数

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `room` | 全部抖音直播间 | 数字直播间号；已停止监控的房间仍可查询 |
| `from_ms` | 无下界 | UTC Unix 毫秒时间戳，包含此时刻 |
| `to_ms` | 无上界 | UTC Unix 毫秒时间戳，不包含此时刻；必须大于 `from_ms` |
| `as_of_seq` | 请求时最新入库序号 | 非负十进制字符串；后续翻页及明细沿用响应中的值，排除快照之后入库的消息 |
| `limit` | 50 | 1–100，榜单和用户房间列表分页大小 |
| `offset` | 0 | 0–1000000，榜单和用户房间列表分页偏移 |
| `sort` | `value` | 礼物榜支持 `value`、`quantity`；其余接口排序固定 |

省略时间上下界查询全部已采集历史。“今天”“近 7 天”的边界由前端根据用户时区计算。接口不将一次采集连接解释为一场直播。非法数字、逆序时间范围及未知统计参数返回 HTTP 400。用户无记录返回零统计／空列表。

### 礼物榜响应示例

```json
{
  "items": [{
    "rank": 1,
    "user_id": "9007199254740993",
    "user_name": "示例用户",
    "user_level": 32,
    "chat_count": "3",
    "gift_quantity": "9",
    "known_gift_value": "70",
    "unknown_price_quantity": "2",
    "value_complete": false,
    "last_seen_at_ms": 1788885600000
  }],
  "total": "1",
  "next_offset": null,
  "sort": "value",
  "meta": {
    "as_of_seq": "12345",
    "room": null,
    "from_ms": null,
    "to_ms": null,
    "time_basis": "event_timestamp",
    "time_range": "[from_ms,to_ms)",
    "coverage": "collected_events_only",
    "value_unit": "diamond",
    "value_basis": "reported_gift_unit_price",
    "chat_types": ["chat", "emoji"],
    "unattributed_event_count": "0"
  }
}
```

数量、金额及 `total` 使用十进制字符串，UID 和序号也使用字符串，防止 JavaScript 整数精度丢失。金额乘积及累加使用任意精度整数。`rank`、`next_offset`、等级和毫秒时间戳使用 JSON 数字。`rank` 是连续名次；弹幕并列按 UID 升序，礼物价值并列先按数量降序，再按 UID 升序。

弹幕榜使用相同响应结构，`sort` 为 `chat_count`。两个榜单都返回当前范围内该用户的弹幕及礼物指标。无有效 UID 的记录不参与用户排名，`unattributed_event_count` 披露相应类别中无法归属用户的记录数。

用户摘要额外包含 `event_count`、`first_seen_at_ms`、`last_seen_at_ms`、`room_count`；空历史的时间为 null。`event_count` 是可展示的投递记录数，包含礼物连送过程，不能当作逻辑送礼次数。昵称及财富等级是筛选范围内按事件时间最近的一次观测值，缺失等级为 null。用户房间列表的每项包含上述指标、`live_id`、该房间最近观测的昵称和 `fans_club`；粉丝团信息不会合并成一个全局等级。

## 统计口径

- 弹幕只计 `chat` 和 `emoji`。点赞、进场、礼物、通知及暂未确认的特殊聊天类型不计入；相同内容但不同消息 ID 的真实发言各计一次。
- 礼物沿用 Store 已确认的连送身份，保留接收者区分。统计表按每组累计数量的历史最大值计算增量，再按时间筛选。累计 `1 → 2 → 5 → 5` 对应 `1、1、3、0`；乱序回退及结束帧不会额外增加数量。
- 时间使用记录的 `timestamp`，缺失时退回接收时间或入库时间。跨边界累计增量归到观测到该增量的消息时间；监控缺口中每件礼物的真实发生时刻无法恢复。
- 价值为本次数量增量乘以消息中的 `GiftStruct.diamondCount`，单位 `diamond`。它是协议报告的礼物单位价值，不直接推算人民币、付款金额或主播收入。
- 未携带价格为未知，明确携带 0 才是已知零价值。未知价格数量进入 `unknown_price_quantity`，`known_gift_value` 只累加有价格的部分。价格缺失不会通过当天价格表或另一个礼物组推断；后续消息补出价格也不会反向改写已冻结的统计事实。
- 同一连送过程中不同消息报告的价格各自保留。仅已确认组可以合并；无法确认的组仍保留独立记录，不能承诺未确认组也已完全去重。
- 重复事件和重复帧不增加统计；历史重放已有事件不重复累计。停止监控、重启进程不会清空统计。

## 与历史明细联动

从榜单拿到 UID 和 `meta.as_of_seq`，使用相同的 `room/from_ms/to_ms` 查询，例如：

```text
GET /api/messages/search?user_id=9007199254740993&type=gift&view=events&from_ms=1788796800000&to_ms=1788883200000&as_of_seq=12345
```

每条礼物记录增加：

```json
{"gift_statistics":{"quantity_delta":"3","unit_price":10,"value_delta":"30"}}
```

逐页累加 `quantity_delta` 和非 null 的 `value_delta` 可以核对榜单。数量增量为 0 的结束帧仍保留在历史里。未知价格的 `unit_price`、`value_delta` 为 null。沿用原有 `before` 游标分页。

带 `as_of_seq` 的明细必须使用 `view=events`，否则返回 400；合并展示表只保留连送的最新状态，无法还原任意旧快照。查询弹幕榜明细时，既有 `type=chat` 还会包含特殊聊天类型，客户端须按统计返回的 `meta.chat_types` 筛选，或分别使用 `method=WebcastChatMessage` 和对应表情方法查询。

## 数据维护与性能

`analytics_events` 是独立的统计事实表，`analytics_gift_groups` 保存连送计数高水位。新消息与统计事实在同一事务内提交；时间、房间、UID 均有查询索引。

首次启动新后端会在一个事务中按入库顺序回填历史，成功后记录 `analytics_v1` 迁移标记。历史价格优先读取已有事件字段，其次读取保存详情中的 `decoded.gift.diamondCount`；没有依据时保持未知。不会改写原始消息、详情、房间计数或消费游标。迁移失败整笔回滚，重复启动不重复回填。

当前实现按条件聚合事实表，没有缓存完整排行榜或额外的日汇总表；查询期间使用现有 Store 锁。数据量很大时，首次回填及全历史排行会变慢，应根据实际规模再引入独立读取连接和统计缓存。协议重解析若改变事件类型或用户身份，需要后续版本显式升级统计投影；普通通知文案修复不影响金额和弹幕口径。

排行榜先完成用户汇总和分页，再按用户／时间索引查询当页资料。不要将覆盖整个时间范围的用户资料窗口与汇总结果直接关联：带时间筛选时 SQLite 可能反复执行汇总，阻塞同一 Store 的其他请求。Windows CTest 的 `analytics_responsiveness` 使用 6,000 条记录、2,000 位用户检查真实 HTTP 排行榜与并发健康请求，包含时间范围和翻页场景。
