直播台 · Windows 便携版

适用：Windows 10（1903 或更新版本）/ Windows 11，64 位 x64。
无需安装 Docker、Node.js、数据库、编译器或 VC++ 运行库。

1. 将 ZIP 完整解压到有写入权限的目录。不要直接在压缩包内运行。
2. 双击 start.cmd（或 DouyinDanmu.exe），浏览器自动打开工作台。
3. 首次访问创建管理员账号，再添加直播间开始监控。

“暂停”保留房间卡片，方便随时启动；“删除”在确认后停止采集并移出列表。
支持单个或批量删除。历史记录和房间配置会保留，重新添加相同房间号可继续使用。

进入单个直播间，点击“本房间排行榜”查看弹幕榜和礼物榜。
可筛选今天、近 7 天或全部已采集记录；榜单和用户明细均限定在当前房间。
礼物明细显示“本次新增”和“上报数量”，同组累计 1～10 只计 10 个。

启动脚本：
  start.cmd                  普通模式，协议诊断隐藏
  start-debug.cmd            debug 模式，显示协议诊断
  stop.cmd                   停止当前目录的程序，保留所有数据
  start.cmd --port 3001      3000 端口已占用时使用其他端口
  start.cmd --no-browser     启动服务，但不自动打开浏览器

也可以在启动窗口按 Ctrl+C 停止。关闭启动窗口会结束本程序的子进程。
请不要单独运行 app 目录中的脚本，或移走 runtime、bin、web 目录。

账号、历史消息、直播间配置和采集缓冲：data 目录
运行日志：logs 目录
更新前先停止旧版；把整个 data 目录复制到新版目录后，再启动新版。
本包使用独立的新数据目录，不会自动读取或修改 Docker 卷内的数据。

旧版礼物连送已重复计数时，升级不会自动改写历史。停止程序后，
在本目录打开 PowerShell，先预览，再应用：
  $env:DATABASE_PATH = (Resolve-Path '.\data\pipeline.db').Path
  .\bin\douyin-pipeline.exe --repair-gift-groups
  .\bin\douyin-pipeline.exe --repair-gift-groups --apply
应用前会自动建立完整数据库备份，报告 backup 是备份路径。
缺少原始载荷或身份有歧义的组会跳过，请查看 skipped_groups / skip_reasons。
完整说明：https://github.com/LiukerSun/DouyinDanmu/blob/master/backend/pipeline/analytics-api.md

程序只监听本机，不需要管理员权限。首次启动无需联网下载依赖；
采集抖音直播仍需要网络。未开播时会正常等待开播。

每个直播间按 live_id 独立订阅 WebSocket channel，入口为 /ws。
先登录工作台，再订阅。行为消息用于列表，状态消息用于更新直播间状态。

便携版使用本地采集缓冲 → C++ 解析 → SQLite 入库 → WebSocket 推送。
服务管理中的“本地投递 / 本地统计”对应便携版，不需要 RabbitMQ 或 Redis。
收到数据库确认前会保留缓冲，异常停止后下次启动自动重试并按消息 ID 去重。

构建来源及第三方许可证见 licenses；文件校验清单见 manifest.json。
