# 在线观众榜扩展开发设计

日期：2026-10-05。需求：[DouyinDanmu #13](https://github.com/LiukerSun/DouyinDanmu/issues/13)。

## 目标和验收

在线观众榜默认展示前 10 位，提供 10 / 20 / 50 / 100 位快捷选项及 1–100 的自定义人数。偏好按账号保存在当前浏览器，刷新和切换房间后继续生效。人数设置是展示上限：平台只下发 3 位时，明确显示实际收到 3 位，不补造名次，也不保证配置 Cookie 就能获取完整榜单。

接入 `WebcastRoomRankMessage.audience_ranks`，继续支持 `WebcastRoomUserSeqMessage.ranksList`。榜单消息独立更新，在线人数仍由在线人数消息更新。贡献值缺失显示“未提供”，明确提供的 0 仍显示 0，64 位数值保留精度；沿用平台的贡献描述，不擅自把未确认单位的 score 标成钻石。匿名用户保持匿名。

榜单与实时行为列表分开，扩展榜单不能挤占消息列表全部空间。榜单限制高度、内部滚动并支持收起；桌面和 390px 窄屏无横向溢出。房间切换、断线重连、过期数据和空榜均有确定行为。

## 改动前的实现和证据

- `backend/pipeline/event_parser.inc` 只从在线人数消息的 ranksList 输出观众榜，最多 20 条。RoomRankMessage 虽有重复的 audience_ranks 定义，目前未进入顶部状态投影。
- `main.cpp` 的快照和增量批次只保留最后一条 online_count 状态；前端 messages.ts 也只接受这一种状态。
- Pipeline.tsx 直接展示数字 score；改动前 proto 的普通整数无法区分字段缺失和明确的 0。独立 wire 标签编码的回归夹具已复现这一行为。
- [上游 #169](https://github.com/saermart/DouyinLiveWebFetcher/issues/169) 提到 audience_ranks 能包含多于 3 位；[上游协议更新 #165](https://github.com/saermart/DouyinLiveWebFetcher/pull/165) 提供结构线索。本项目已有固定版本 CDN decoder 和 RoomRankMessage 重复字段测试，见 `backend/proto/README.md`。

这些证据只支持读取平台已下发的字段，不能证明当前所有直播间都下发完整榜单或提供钻石值。

## 数据流及接口

1. 解析器统一输出榜单条目：rank、user_id、user_name、score（精确十进制字符串或 null）、hidden，以及平台提供的 score_description / exactly_score。
2. RoomUserSeqMessage 保持 type=online_count，并附带来源 room_user_seq、实际条目数和榜单。RoomRankMessage 输出 type=audience_rank，来源 room_rank；优先使用字段 3 的 audience_ranks，兼容只有字段 2 ranksList 的旧格式。
3. 新榜单保存平台实际顺序和名次，解析事件保留条目；对前端投影最多发送 100 位，完整载荷仍由现有协议详情保存。显式空数组代表平台本次下发空榜。
4. 快照和增量批次分别保存最新的 online_count / audience_rank，按 seq 防止旧消息覆盖新消息。through_seq 继续随所有事件推进，榜单不进入行为列表、用户发言数、礼物数量和礼物金额统计。
   快照查询使用按房间、状态类型、序号排列的局部索引；首次启动自动建立索引，避免没有扩展榜单的房间每次遍历全部历史。
5. 前端保留两类房间状态及原 method。优先展示 RoomRankMessage；当它比在线消息落后超过 60 秒时，使用在线消息的榜单；若没有新数据则保留上次观测并注明，空榜不能复活旧条目。

兼容：前端能读取旧后端的数字 score；新后端输出字符串。旧后端无法提供扩展榜单，页面按实际人数解释。无需增加采集 HTTP 请求、修改 Cookie 保存逻辑或迁移业务数据。已有榜单记录随新的平台消息刷新，无需重算历史统计。

## 前端模块

`pipeline/audience-ranking.ts` 封装人数校验、来源选择和数值显示；`components/AudienceRanking.tsx` 封装账号偏好、快捷/自定义控件、空态、上次观测和可滚动榜单。Pipeline 只传当前房间状态、账号和连接状态。保持现有 HeroUI 控件及视觉样式。

尚未收到任何榜单时不增加面板，保留消息区域空间；收到显式空榜后显示“平台本次未提供观众榜名单”。100 位名单最多占用 100px 滚动区域，小窗口桌面缩至 74px，也可收起。桌面、手机端的切换人数和收起操作均可使用。

自定义输入只接受 1–100 的整数，提交后生效；无效输入提示范围，保留原设置。隐藏条目不展示其真实昵称或贡献描述。平台 score_description 优先用于显示，exactly_score 作为平台补充文本；有数值时用 BigInt 格式化，缺失不使用默认 0。

## 开发步骤

1. 建立 score 缺失变为 0 的 wire 回归，复现后确认原因。
2. 增加分数 presence，归一化两个榜单来源；投影和批次保留两类状态。
3. 开发人数偏好和榜单模块，替换 Pipeline 内嵌渲染。
4. 完成 wire、存储/快照/批次、状态合并和浏览器验收；记录结果。

## 验证计划

- wire：字段缺失 / 明确 0 / 大于 JS 安全整数的分数、平台描述、扩展多用户、旧榜单、空榜、隐藏用户。
- Store：多房间隔离，快照与实时批次一致，100 位投影上限，两类状态均不丢失、游标推进及统计不变。
- 前端状态：更旧 seq 不回退，RoomRank 更新不改变在线人数，来源过期回退，空榜及切换房间。
- 浏览器：默认 10、快捷人数、自定义和无效值、刷新持久化、仅下发 3 位、缺失值/明确 0/大整数/匿名、空态、断线、滚动/收起和桌面/窄屏消息空间。
- 运行 CTest、前端 build、test:multiroom 和相关浏览器回归。合成测试与本地隔离预览使用虚构数据；真实直播房间下发量仍需实播确认。

## 交付状态

实现和验证已完成。结果：

- 缺失分数的 wire 夹具在改动前失败（`An omitted audience score must not appear as zero diamonds`），增加 presence 后通过。显式 0、大整数、平台文字、新旧榜单和空榜均有回归。
- Store 测试覆盖两类状态的快照/实时投影、跨房间隔离、130 位原记录投影为 100 位、匿名信息过滤、空榜清除和统计不变。Windows 原生构建成功，最终版本 7 组 CTest 全部通过（13.86 秒）。
- `npm run build`、`test:audience-ranking`、`test:multiroom`、`test:messages`、`test:rankings` 通过；新增多房间测试确认两类榜单状态在断线和重连后保留，并使用精确游标恢复。
- `test:audience-ranking-ui` 通过：默认 10、快捷 20/100、自定义 13、无效 0、刷新和账号隔离、平台仅下发 3 位、精确/缺失/匿名值、实时更新、空榜、旧消息重放、过期来源回退、手机端操作及内部滚动。
- 原有 `check-monitor-layout.cjs` 通过，在无榜单时保留桌面至少 4 条完整消息和手机端多条消息。100 位展开时，1260×620 的消息区仍有 137px，390×844 的消息区为 464px；四个验收视口均无横向溢出。
- SQLite 查询计划确认新快照查询使用 `message_views_room_state` 索引。
- v2.4.0 Windows 发布构建通过 7 组 CTest（14.84 秒），账号和采集相关 30 项测试通过。实际便携包通过版本号、中文和空格解压路径、独立运行环境、登录、采集、WebSocket、重启、历史持久化、暂停/删除/重新添加、缓冲恢复及进程清理验收；打包后的网页通过观众榜交互和桌面/手机布局验收。
- 发布包包含 735 个文件，逐项校验清单哈希及 ZIP 内容一致，排除运行数据和凭据，并生成 ZIP 的 SHA256 校验文件。

截图保存在本机 `.codex/reference/audience-ranking-ui/audience-1260.png` 和 `audience-390.png`；不纳入源码。其他既有本机文档继续忽略，只将此开发设计纳入仓库。

未验证范围：报告者的真实直播间没有提供可复核载荷，本轮使用虚构 wire / API / WebSocket 数据验证软件行为，没有验证该房间的实际下发人数或贡献值。若平台不提供字段，Cookie 与展示人数设置均不能补出数据。
