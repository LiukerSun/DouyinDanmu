using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using DouyinDanmu.Models;
using DouyinDanmu.Services;

namespace DouyinDanmu
{
    public partial class Form1 : Form
    {
        // 多房间连接管理
        private ConnectionManager? _connectionManager;
        private LoggingService? _loggingService;
        private readonly ConcurrentDictionary<string, RoomTabPage> _roomTabs = new();
        private TabControl? _roomTabControl;

        // 向后兼容：单房间模式字段（保留用于过渡）
        private Services.DouyinLiveFetcher? _fetcher;
        private bool _isConnected = false;
        private List<string> _watchedUserIds = [];
        private AppSettings _appSettings = new();
        private DatabaseService? _databaseService;
        private Services.WebSocketService? _webSocketService;

        // 批量更新相关字段（保留用于向后兼容，新房间使用RoomTabPage内部队列）
        private readonly System.Windows.Forms.Timer _updateTimer;
        private readonly Queue<LiveMessage> _pendingMessages = new();
        private readonly object _pendingMessagesLock = new();
        private bool _statisticsNeedUpdate = false;
        private readonly Dictionary<string, (ListViewItem item, int count, DateTime ts)> _giftAggregates = [];
        private readonly Dictionary<string, (ListViewItem item, int count, DateTime ts)> _watchedGiftAggregates = [];
        private int _totalGiftCount = 0;

        public Form1()
        {
            InitializeComponent();

            // 启用双缓冲以减少闪烁
            SetStyle(
                ControlStyles.AllPaintingInWmPaint
                    | ControlStyles.UserPaint
                    | ControlStyles.DoubleBuffer,
                true
            );

            // 为所有ListView启用双缓冲
            EnableListViewDoubleBuffering(listViewChat);
            EnableListViewDoubleBuffering(listViewMember);
            EnableListViewDoubleBuffering(listViewGiftFollow);
            EnableListViewDoubleBuffering(listViewWatchedUsers);

            // 初始化批量更新定时器
            _updateTimer = new System.Windows.Forms.Timer
            {
                Interval = 100 // 100ms批量更新一次
            };
            _updateTimer.Tick += UpdateTimer_Tick;
            _updateTimer.Start();

            // 初始化TabControl（放在groupBoxMessages内部，与原有4个GroupBox并列）
            InitializeRoomTabControl();

            // 初始化ConnectionManager
            InitializeConnectionManager();

            // 加载设置
            LoadSettings();

            // 初始化数据库
            InitializeDatabaseAsync();

            UpdateStatus("就绪");
            UpdateStatistics();

            // 添加窗口大小变化事件处理
            this.Resize += Form1_Resize;

            // 初始化布局
            AdjustLayout();

            // 测试JavaScript引擎
            TestJavaScriptEngineOnStartup();

            // 初始化WebSocket服务
            InitializeWebSocketService();
        }

        /// <summary>
        /// 初始化多房间TabControl
        /// </summary>
        private void InitializeRoomTabControl()
        {
            _roomTabControl = new TabControl
            {
                Dock = DockStyle.None,
                Font = new Font("Microsoft YaHei UI", 9F),
                Visible = false // 初始隐藏，有多房间时才显示
            };
            _roomTabControl.SelectedIndexChanged += RoomTabControl_SelectedIndexChanged;
            groupBoxMessages.Controls.Add(_roomTabControl);
        }

        /// <summary>
        /// 初始化ConnectionManager
        /// </summary>
        private void InitializeConnectionManager()
        {
            var networkSettings = new NetworkSettings();
            var loggingSettings = new LoggingSettings();
            _loggingService = new LoggingService(loggingSettings);
            _connectionManager = new ConnectionManager(networkSettings, _loggingService);

            // 绑定多房间事件
            _connectionManager.RoomMessageReceived += OnRoomMessageReceived;
            _connectionManager.ConnectionStateChanged += OnConnectionStateChanged;
            _connectionManager.StatusChanged += OnConnectionManagerStatusChanged;
        }

        /// <summary>
        /// TabControl选中标签页变化事件
        /// </summary>
        private void RoomTabControl_SelectedIndexChanged(object? sender, EventArgs e)
        {
            // 可以在这里更新状态栏显示当前选中房间的信息
            if (_roomTabControl?.SelectedTab is RoomTabPage roomTab)
            {
                UpdateStatus($"当前查看房间: {roomTab.LiveId}");
            }
        }

        /// <summary>
        /// 为ListView启用双缓冲
        /// </summary>
        private static void EnableListViewDoubleBuffering(ListView listView)
        {
            typeof(ListView).InvokeMember(
                "DoubleBuffered",
                System.Reflection.BindingFlags.NonPublic
                    | System.Reflection.BindingFlags.Instance
                    | System.Reflection.BindingFlags.SetProperty,
                null,
                listView,
                [true]
            );
        }

        #region 多房间管理

        /// <summary>
        /// ConnectionManager消息接收事件 - 路由到对应的RoomTabPage
        /// </summary>
        private void OnRoomMessageReceived(object? sender, RoomMessageEventArgs e)
        {
            // 路由消息到对应的RoomTabPage
            if (_roomTabs.TryGetValue(e.RoomId, out var roomTab))
            {
                roomTab.EnqueueMessage(e.Message);
            }

            // 同时保存到数据库
            _databaseService?.QueueMessage(e.Message);

            // 广播到WebSocket
            if (_webSocketService != null && _webSocketService.IsRunning)
            {
                _ = Task.Run(async () =>
                    await _webSocketService.BroadcastLiveMessageAsync(e.Message)
                );
            }
        }

        /// <summary>
        /// ConnectionManager连接状态变化事件
        /// </summary>
        private void OnConnectionStateChanged(object? sender, ConnectionStateChangedEventArgs e)
        {
            if (InvokeRequired)
            {
                BeginInvoke(() => OnConnectionStateChanged(sender, e));
                return;
            }

            // 更新对应RoomTabPage的连接状态
            if (_roomTabs.TryGetValue(e.RoomId, out var roomTab))
            {
                bool connected = e.State == Services.ConnectionState.Connected;
                roomTab.UpdateConnectionState(e.Message, connected);
            }

            UpdateStatus($"[{e.RoomId}] {e.Message}");

            // 更新连接按钮状态
            UpdateConnectButtonState();
        }

        /// <summary>
        /// ConnectionManager状态变化事件
        /// </summary>
        private void OnConnectionManagerStatusChanged(object? sender, string status)
        {
            if (InvokeRequired)
            {
                BeginInvoke(() => OnConnectionManagerStatusChanged(sender, status));
                return;
            }
            UpdateStatus(status);
        }

        /// <summary>
        /// 添加新的直播间标签页并连接
        /// </summary>
        private async Task AddRoomAndConnectAsync(string liveId)
        {
            if (string.IsNullOrWhiteSpace(liveId))
            {
                UpdateStatus("请输入直播间ID");
                return;
            }

            var roomId = liveId; // 使用liveId作为roomId

            // 检查是否已存在
            if (_roomTabs.ContainsKey(roomId))
            {
                // 切换到已有标签页
                if (_roomTabControl != null)
                {
                    foreach (TabPage tab in _roomTabControl.TabPages)
                    {
                        if (tab is RoomTabPage existingTab && existingTab.RoomId == roomId)
                        {
                            _roomTabControl.SelectedTab = existingTab;
                            break;
                        }
                    }
                }
                UpdateStatus($"房间 {liveId} 已存在，已切换到该标签页");
                return;
            }

            // 创建RoomTabPage
            var roomTab = new RoomTabPage(
                roomId, liveId, _watchedUserIds, GetDisplayUserName);
            roomTab.DisconnectRequested += RoomTab_DisconnectRequested;
            roomTab.CloseRequested += RoomTab_CloseRequested;
            roomTab.SetAutoScroll(checkBoxAutoScroll.Checked);

            // 为RoomTabPage中的ListView绑定右键菜单
            foreach (var lv in roomTab.GetAllListViews())
            {
                lv.ContextMenuStrip = contextMenuStripMessage;
            }

            // 添加到字典和TabControl
            _roomTabs[roomId] = roomTab;
            _roomTabControl!.TabPages.Add(roomTab);
            _roomTabControl.SelectedTab = roomTab;

            // 显示TabControl（如果是第一个房间）
            if (_roomTabs.Count == 1)
            {
                ShowMultiRoomUI();
            }

            // 通过ConnectionManager连接
            UpdateStatus($"正在连接房间 {liveId}...");
            buttonConnect.Enabled = false;

            try
            {
                var success = await _connectionManager!.ConnectAsync(roomId, liveId);
                if (success)
                {
                    UpdateStatus($"房间 {liveId} 连接成功");
                }
                else
                {
                    UpdateStatus($"房间 {liveId} 连接失败");
                }
            }
            catch (Exception ex)
            {
                UpdateStatus($"房间 {liveId} 连接异常: {ex.Message}");
            }
            finally
            {
                buttonConnect.Enabled = true;
                UpdateConnectButtonState();
            }
        }

        /// <summary>
        /// RoomTabPage断开请求事件
        /// </summary>
        private async void RoomTab_DisconnectRequested(object? sender, EventArgs e)
        {
            if (sender is RoomTabPage roomTab)
            {
                await DisconnectRoomAsync(roomTab.RoomId);
            }
        }

        /// <summary>
        /// RoomTabPage关闭请求事件
        /// </summary>
        private async void RoomTab_CloseRequested(object? sender, EventArgs e)
        {
            if (sender is RoomTabPage roomTab)
            {
                var result = MessageBox.Show(
                    $"确定要关闭房间 {roomTab.LiveId} 吗？",
                    "确认关闭",
                    MessageBoxButtons.YesNo,
                    MessageBoxIcon.Question
                );
                if (result == DialogResult.Yes)
                {
                    await RemoveRoomAsync(roomTab.RoomId);
                }
            }
        }

        /// <summary>
        /// 断开指定房间
        /// </summary>
        private async Task DisconnectRoomAsync(string roomId)
        {
            if (_connectionManager != null)
            {
                await _connectionManager.DisconnectAsync(roomId);
            }
            UpdateConnectButtonState();
        }

        /// <summary>
        /// 移除房间标签页
        /// </summary>
        private async Task RemoveRoomAsync(string roomId)
        {
            // 先断开连接
            if (_connectionManager != null)
            {
                await _connectionManager.RemoveRoomAsync(roomId);
            }

            // 移除TabPage
            if (_roomTabs.TryRemove(roomId, out var roomTab))
            {
                roomTab.DisconnectRequested -= RoomTab_DisconnectRequested;
                roomTab.CloseRequested -= RoomTab_CloseRequested;
                _roomTabControl?.TabPages.Remove(roomTab);
                roomTab.Dispose();
            }

            // 如果没有房间了，隐藏TabControl
            if (_roomTabs.IsEmpty)
            {
                HideMultiRoomUI();
            }

            UpdateConnectButtonState();
        }

        /// <summary>
        /// 显示多房间UI（TabControl），隐藏原有的4个GroupBox
        /// </summary>
        private void ShowMultiRoomUI()
        {
            groupBoxChat.Visible = false;
            groupBoxMember.Visible = false;
            groupBoxGiftFollow.Visible = false;
            groupBoxWatchedUsers.Visible = false;

            if (_roomTabControl != null)
            {
                _roomTabControl.Visible = true;
            }

            AdjustLayout();
        }

        /// <summary>
        /// 隐藏多房间UI，恢复原有的4个GroupBox
        /// </summary>
        private void HideMultiRoomUI()
        {
            if (_roomTabControl != null)
            {
                _roomTabControl.Visible = false;
            }

            groupBoxChat.Visible = true;
            groupBoxMember.Visible = true;
            groupBoxGiftFollow.Visible = true;
            groupBoxWatchedUsers.Visible = true;

            AdjustLayout();
        }

        /// <summary>
        /// 更新连接按钮状态
        /// </summary>
        private void UpdateConnectButtonState()
        {
            if (InvokeRequired)
            {
                BeginInvoke(UpdateConnectButtonState);
                return;
            }

            bool hasAnyConnection = _connectionManager?.HasAnyConnection == true || _isConnected;
            buttonConnect.Text = hasAnyConnection ? "添加房间" : "连接";
            buttonConnect.Enabled = true;
        }

        #endregion

        /// <summary>
        /// 批量更新定时器事件
        /// </summary>
        private void UpdateTimer_Tick(object? sender, EventArgs e)
        {
            ProcessPendingMessages();

            if (_statisticsNeedUpdate)
            {
                UpdateStatistics();
                _statisticsNeedUpdate = false;
            }
        }

        /// <summary>
        /// 处理待处理的消息队列
        /// </summary>
        private void ProcessPendingMessages()
        {
            List<LiveMessage> messagesToProcess = [];

            lock (_pendingMessagesLock)
            {
                // 一次性处理所有待处理的消息，但限制数量避免UI卡顿
                int maxProcessCount = Math.Min(_pendingMessages.Count, 50);
                for (int i = 0; i < maxProcessCount; i++)
                {
                    if (_pendingMessages.Count > 0)
                    {
                        messagesToProcess.Add(_pendingMessages.Dequeue());
                    }
                }
            }

            if (messagesToProcess.Count == 0)
                return;

            // 暂停ListView的重绘
            listViewChat.BeginUpdate();
            listViewMember.BeginUpdate();
            listViewGiftFollow.BeginUpdate();
            listViewWatchedUsers.BeginUpdate();

            try
            {
                foreach (var message in messagesToProcess)
                {
                    ProcessSingleMessage(message);
                }

                _statisticsNeedUpdate = true;
            }
            finally
            {
                // 恢复ListView的重绘
                listViewChat.EndUpdate();
                listViewMember.EndUpdate();
                listViewGiftFollow.EndUpdate();
                listViewWatchedUsers.EndUpdate();

                // 批量处理自动滚动
                if (checkBoxAutoScroll.Checked)
                {
                    PerformAutoScroll();
                }
            }
        }

        /// <summary>
        /// 处理单个消息（不触发重绘）
        /// </summary>
        private void ProcessSingleMessage(LiveMessage message)
        {
            // 检查是否为关注的用户
            bool isWatchedUser =
                !string.IsNullOrEmpty(message.UserId) && _watchedUserIds.Contains(message.UserId);

            // 如果是关注的用户，添加到关注用户列表
            if (isWatchedUser)
            {
                if (message.Type == LiveMessageType.Gift && message is GiftMessage giftMsg)
                {
                    AddOrUpdateWatchedGift(giftMsg);
                }
                else
                {
                    AddWatchedUserMessageInternal(message);
                }
            }

            // 根据消息类型添加到对应的ListView
            switch (message.Type)
            {
                case LiveMessageType.Chat:
                    AddChatMessageInternal(message);
                    break;
                case LiveMessageType.Member:
                    AddMemberMessageInternal(message);
                    break;
                case LiveMessageType.Gift:
                    if (message is GiftMessage gift)
                    {
                        AddOrUpdateGift(gift);
                    }
                    break;
                case LiveMessageType.Like:
                case LiveMessageType.Social:
                    AddGiftFollowMessageInternal(message);
                    break;
            }
        }

        private void AddOrUpdateGift(GiftMessage gift)
        {
            var key = (gift.UserId ?? "") + "|" + (gift.GiftName ?? "");
            var now = DateTime.Now;
            if (_giftAggregates.TryGetValue(key, out var agg))
            {
                if (gift.GiftCount >= agg.count)
                {
                    agg.item.SubItems[6].Text = $"{gift.GiftName} x{gift.GiftCount}";
                    _totalGiftCount += Math.Max(0, gift.GiftCount - agg.count);
                    _giftAggregates[key] = (agg.item, gift.GiftCount, now);
                    return;
                }
            }

            var item = new ListViewItem(gift.Timestamp.ToString("HH:mm:ss"));
            var typeText = "礼物";
            item.SubItems.Add(typeText);
            item.SubItems.Add(gift.UserName ?? "");
            item.SubItems.Add(FormatUserId(gift.UserId));
            item.SubItems.Add(gift.FansClubLevel > 0 ? gift.FansClubLevel.ToString() : "-");
            item.SubItems.Add(gift.PayGradeLevel > 0 ? gift.PayGradeLevel.ToString() : "-");
            item.SubItems.Add($"{gift.GiftName} x{gift.GiftCount}");
            listViewGiftFollow.Items.Add(item);
            _giftAggregates[key] = (item, gift.GiftCount, now);
        }

        private void AddOrUpdateWatchedGift(GiftMessage gift)
        {
            var key = (gift.UserId ?? "") + "|" + (gift.GiftName ?? "");
            var now = DateTime.Now;
            if (_watchedGiftAggregates.TryGetValue(key, out var agg))
            {
                if (gift.GiftCount >= agg.count)
                {
                    agg.item.SubItems[6].Text = $"{gift.GiftName} x{gift.GiftCount}";
                    _watchedGiftAggregates[key] = (agg.item, gift.GiftCount, now);
                    return;
                }
            }

            var item = new ListViewItem(gift.Timestamp.ToString("HH:mm:ss"));
            var typeText = "礼物";
            item.SubItems.Add(typeText);
            string displayUserName = GetDisplayUserName(gift.UserId, gift.UserName);
            item.SubItems.Add(displayUserName);
            item.SubItems.Add(FormatUserId(gift.UserId));
            item.SubItems.Add(gift.FansClubLevel > 0 ? gift.FansClubLevel.ToString() : "-");
            item.SubItems.Add(gift.PayGradeLevel > 0 ? gift.PayGradeLevel.ToString() : "-");
            item.SubItems.Add($"{gift.GiftName} x{gift.GiftCount}");
            listViewWatchedUsers.Items.Add(item);
            _watchedGiftAggregates[key] = (item, gift.GiftCount, now);
        }

        /// <summary>
        /// 批量执行自动滚动
        /// </summary>
        private void PerformAutoScroll()
        {
            if (listViewChat.Items.Count > 0)
                listViewChat.EnsureVisible(listViewChat.Items.Count - 1);
            if (listViewMember.Items.Count > 0)
                listViewMember.EnsureVisible(listViewMember.Items.Count - 1);
            if (listViewGiftFollow.Items.Count > 0)
                listViewGiftFollow.EnsureVisible(listViewGiftFollow.Items.Count - 1);
            if (listViewWatchedUsers.Items.Count > 0)
                listViewWatchedUsers.EnsureVisible(listViewWatchedUsers.Items.Count - 1);
        }

        /// <summary>
        /// 加载应用程序设置
        /// </summary>
        private void LoadSettings()
        {
            try
            {
                _appSettings = SettingsManager.LoadSettings();

                // 应用设置到界面
                textBoxLiveId.Text = _appSettings.LiveId;
                _watchedUserIds = [.. _appSettings.WatchedUserIds];
                checkBoxAutoScroll.Checked = _appSettings.AutoScroll;

                // 修复：如果WatchedUserIds为空但UserInfos有数据，从UserInfos恢复WatchedUserIds
                if (
                    _watchedUserIds.Count == 0
                    && _appSettings.UserInfos != null
                    && _appSettings.UserInfos.Count > 0
                )
                {
                    _watchedUserIds = [.. _appSettings.UserInfos.Keys];
                    UpdateStatus($"从用户信息中恢复了 {_watchedUserIds.Count} 个关注用户");

                    // 立即保存修复后的设置
                    SaveSettings();
                }

                // 应用窗口设置
                if (_appSettings.WindowX >= 0 && _appSettings.WindowY >= 0)
                {
                    this.StartPosition = FormStartPosition.Manual;
                    this.Location = new Point(_appSettings.WindowX, _appSettings.WindowY);
                }

                this.Size = new Size(_appSettings.WindowWidth, _appSettings.WindowHeight);

                if (_appSettings.WindowState >= 0 && _appSettings.WindowState <= 2)
                {
                    this.WindowState = (FormWindowState)_appSettings.WindowState;
                }

                var settingsPath = SettingsManager.GetSettingsFilePath();
                UpdateStatus(
                    $"已加载设置，关注用户: {_watchedUserIds.Count}个 | 设置文件: {settingsPath}"
                );
            }
            catch (Exception ex)
            {
                UpdateStatus($"加载设置失败: {ex.Message}");
            }
        }

        /// <summary>
        /// 保存应用程序设置
        /// </summary>
        private void SaveSettings()
        {
            try
            {
                // 更新设置
                _appSettings.LiveId = textBoxLiveId.Text.Trim();
                _appSettings.WatchedUserIds = [.. _watchedUserIds];
                _appSettings.AutoScroll = checkBoxAutoScroll.Checked;

                // 保存多房间配置
                _appSettings.LiveRooms = _roomTabs.Values.Select(tab => new RoomConfig
                {
                    RoomId = tab.RoomId,
                    LiveId = tab.LiveId,
                    Enabled = true
                }).ToList();

                // 保存窗口状态（只在正常状态下保存位置和大小）
                if (this.WindowState == FormWindowState.Normal)
                {
                    _appSettings.WindowX = this.Location.X;
                    _appSettings.WindowY = this.Location.Y;
                    _appSettings.WindowWidth = this.Size.Width;
                    _appSettings.WindowHeight = this.Size.Height;
                }
                _appSettings.WindowState = (int)this.WindowState;

                // 使用改进的SaveSettings方法
                bool success = SettingsManager.SaveSettings(_appSettings);
                if (!success)
                {
                    UpdateStatus($"保存设置失败，请检查文件权限");
                }
            }
            catch (Exception ex)
            {
                UpdateStatus($"保存设置失败: {ex.Message}");
            }
        }

        /// <summary>
        /// 窗口大小变化事件处理
        /// </summary>
        private void Form1_Resize(object? sender, EventArgs e)
        {
            AdjustLayout();
        }

        /// <summary>
        /// 调整布局以适应窗口大小
        /// </summary>
        private void AdjustLayout()
        {
            if (groupBoxMessages == null || panelConnection == null)
                return;

            const int connectionHeight = 60;
            const int statusHeight = 196;
            const int margin = 12;
            const int pad = 15;
            const int gap = 10;
            const int btnH = 28;
            const int btnRowH = 36;

            int clientW = this.ClientSize.Width;
            int clientH = this.ClientSize.Height;

            // 从下往上分配固定区域，剩余给消息区
            // 1. 状态栏固定在底部
            int statusTop = clientH - margin - statusHeight;
            groupBoxStatus.SetBounds(margin, statusTop, clientW - margin * 2, statusHeight);
            textBoxStatus.SetBounds(pad, 22, groupBoxStatus.Width - pad * 2, statusHeight - 32);

            // 2. 按钮行固定在状态栏上方
            int btnY = statusTop - btnRowH - 4;
            checkBoxAutoScroll.Location = new Point(margin + pad, btnY + 5);
            buttonSaveLog.SetBounds(clientW - margin - 85, btnY + 2, 85, btnH);
            buttonClear.SetBounds(buttonSaveLog.Left - 95, btnY + 2, 85, btnH);

            // 3. 消息区填充剩余空间
            int msgTop = connectionHeight + margin;
            int msgBottom = btnY - 4;
            int msgHeight = msgBottom - msgTop;
            if (msgHeight < 150) msgHeight = 150;
            groupBoxMessages.SetBounds(margin, msgTop, clientW - margin * 2, msgHeight);

            int innerW = groupBoxMessages.Width - pad * 2;
            int contentTop = 25;
            int contentH = groupBoxMessages.Height - contentTop - 8;
            if (contentH < 80) contentH = 80;

            bool isMultiRoom = _roomTabControl != null && _roomTabControl.Visible;

            if (isMultiRoom)
            {
                _roomTabControl!.Dock = DockStyle.None;
                _roomTabControl.SetBounds(pad, contentTop, innerW, contentH);
            }
            else
            {
                int topH = (int)(contentH * 0.60);
                int bottomH = contentH - topH - gap;
                int colW = (innerW - gap * 2) / 3;

                groupBoxChat.SetBounds(pad, contentTop, colW, topH);
                listViewChat.SetBounds(6, 20, colW - 12, topH - 26);

                groupBoxMember.SetBounds(pad + colW + gap, contentTop, colW, topH);
                listViewMember.SetBounds(6, 20, colW - 12, topH - 26);

                int thirdW = innerW - (colW + gap) * 2;
                groupBoxGiftFollow.SetBounds(pad + (colW + gap) * 2, contentTop, thirdW, topH);
                listViewGiftFollow.SetBounds(6, 20, thirdW - 12, topH - 26);

                groupBoxWatchedUsers.SetBounds(pad, contentTop + topH + gap, innerW, bottomH);
                listViewWatchedUsers.SetBounds(6, 20, innerW - 12, bottomH - 26);
            }
        }

        /// <summary>
        /// 清空消息列表
        /// </summary>
        private void ButtonClear_Click(object sender, EventArgs e)
        {
            var result = MessageBox.Show(
                "选择清空范围：\n\n是(Y) - 仅清空界面显示\n否(N) - 同时清空数据库数据\n取消 - 不执行清空操作",
                "清空确认",
                MessageBoxButtons.YesNoCancel,
                MessageBoxIcon.Question
            );

            if (result == DialogResult.Cancel)
                return;

            // 清空多房间TabPage
            if (_roomTabControl != null && _roomTabControl.Visible)
            {
                if (_roomTabControl.SelectedTab is RoomTabPage activeRoomTab)
                {
                    activeRoomTab.ClearMessages();
                }
            }
            else
            {
                // 清空原有单房间界面显示
                listViewChat.Items.Clear();
                listViewMember.Items.Clear();
                listViewGiftFollow.Items.Clear();
                listViewWatchedUsers.Items.Clear();
            }

            if (result == DialogResult.No)
            {
                // 同时清空数据库数据
                _ = ClearDatabaseDataAsync();
            }

            UpdateStatistics();
            UpdateStatus(
                result == DialogResult.Yes ? "已清空界面消息列表" : "已清空界面和数据库消息"
            );
        }

        /// <summary>
        /// 异步清空数据库数据
        /// </summary>
        private async Task ClearDatabaseDataAsync()
        {
            if (_databaseService == null)
                return;

            try
            {
                var liveId = textBoxLiveId.Text.Trim();
                if (!string.IsNullOrEmpty(liveId))
                {
                    await _databaseService.ClearLiveDataAsync(liveId);
                }
            }
            catch (Exception ex)
            {
                UpdateStatus($"清空数据库数据失败: {ex.Message}");
            }
        }

        /// <summary>
        /// 保存日志
        /// </summary>
        private void ButtonSaveLog_Click(object sender, EventArgs e)
        {
            try
            {
                // 确定要保存的ListViews来源
                ListView chatLv, memberLv, giftFollowLv, watchedLv;
                string roomLabel;

                if (_roomTabControl != null && _roomTabControl.Visible
                    && _roomTabControl.SelectedTab is RoomTabPage activeTab)
                {
                    var listViews = activeTab.GetAllListViews().ToList();
                    chatLv = listViews[0];
                    memberLv = listViews[1];
                    giftFollowLv = listViews[2];
                    watchedLv = listViews[3];
                    roomLabel = activeTab.LiveId;
                }
                else
                {
                    chatLv = listViewChat;
                    memberLv = listViewMember;
                    giftFollowLv = listViewGiftFollow;
                    watchedLv = listViewWatchedUsers;
                    roomLabel = textBoxLiveId.Text.Trim();
                }

                using var dialog = new SaveFileDialog();
                dialog.Filter = "文本文件 (*.txt)|*.txt|CSV文件 (*.csv)|*.csv";
                dialog.DefaultExt = "txt";
                dialog.FileName = $"抖音直播消息_{roomLabel}_{DateTime.Now:yyyyMMdd_HHmmss}";

                if (dialog.ShowDialog() == DialogResult.OK)
                {
                    var isCSV = dialog.FileName.EndsWith(
                        ".csv",
                        StringComparison.OrdinalIgnoreCase
                    );
                    var separator = isCSV ? "," : "\t";
                    var lines = new List<string>();

                    // 添加聊天消息
                    lines.Add($"=== 聊天消息 ({chatLv.Items.Count}条) ===");
                    lines.Add(isCSV ? "时间,用户,用户ID,粉丝团等级,财富等级,聊天内容"
                                    : "时间\t用户\t用户ID\t粉丝团等级\t财富等级\t聊天内容");
                    foreach (ListViewItem item in chatLv.Items)
                    {
                        var values = new string[item.SubItems.Count];
                        for (int i = 0; i < item.SubItems.Count; i++)
                            values[i] = item.SubItems[i].Text;
                        lines.Add(string.Join(separator, values));
                    }
                    lines.Add("");

                    // 添加进场消息
                    lines.Add($"=== 进场消息 ({memberLv.Items.Count}条) ===");
                    lines.Add(isCSV ? "时间,用户,用户ID,粉丝团等级,财富等级,内容"
                                    : "时间\t用户\t用户ID\t粉丝团等级\t财富等级\t内容");
                    foreach (ListViewItem item in memberLv.Items)
                    {
                        var values = new string[item.SubItems.Count];
                        for (int i = 0; i < item.SubItems.Count; i++)
                            values[i] = item.SubItems[i].Text;
                        lines.Add(string.Join(separator, values));
                    }
                    lines.Add("");

                    // 添加礼物和关注消息
                    lines.Add($"=== 礼物&关注消息 ({giftFollowLv.Items.Count}条) ===");
                    lines.Add(isCSV ? "时间,类型,用户,用户ID,粉丝团等级,财富等级,内容"
                                    : "时间\t类型\t用户\t用户ID\t粉丝团等级\t财富等级\t内容");
                    foreach (ListViewItem item in giftFollowLv.Items)
                    {
                        var values = new string[item.SubItems.Count];
                        for (int i = 0; i < item.SubItems.Count; i++)
                            values[i] = item.SubItems[i].Text;
                        lines.Add(string.Join(separator, values));
                    }

                    // 添加关注用户消息
                    lines.Add($"=== 关注用户消息 ({watchedLv.Items.Count}条) ===");
                    lines.Add(isCSV ? "时间,类型,用户,用户ID,粉丝团等级,财富等级,内容"
                                    : "时间\t类型\t用户\t用户ID\t粉丝团等级\t财富等级\t内容");
                    foreach (ListViewItem item in watchedLv.Items)
                    {
                        var values = new string[item.SubItems.Count];
                        for (int i = 0; i < item.SubItems.Count; i++)
                            values[i] = item.SubItems[i].Text;
                        lines.Add(string.Join(separator, values));
                    }

                    // 添加统计信息
                    lines.Add("");
                    lines.Add("=== 统计信息 ===");
                    lines.Add($"房间: {roomLabel}");
                    lines.Add($"聊天消息: {chatLv.Items.Count}条");
                    lines.Add($"进场消息: {memberLv.Items.Count}条");
                    lines.Add($"礼物&关注消息: {giftFollowLv.Items.Count}条");
                    lines.Add($"关注用户消息: {watchedLv.Items.Count}条");
                    lines.Add(
                        $"总计: {chatLv.Items.Count + memberLv.Items.Count + giftFollowLv.Items.Count + watchedLv.Items.Count}条"
                    );
                    lines.Add($"保存时间: {DateTime.Now:yyyy-MM-dd HH:mm:ss}");

                    File.WriteAllLines(dialog.FileName, lines, Encoding.UTF8);
                    MessageBox.Show(
                        $"日志已保存到: {dialog.FileName}",
                        "保存成功",
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Information
                    );
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show(
                    $"保存日志失败: {ex.Message}",
                    "错误",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error
                );
            }
        }

        /// <summary>
        /// 消息接收事件处理
        /// </summary>
        private void OnMessageReceived(object? sender, LiveMessage message)
        {
            if (InvokeRequired)
            {
                BeginInvoke(new Action(() => OnMessageReceived(sender, message)));
                return;
            }

            // 使用批量队列而不是立即保存到数据库
            _databaseService?.QueueMessage(message);

            // 广播消息到WebSocket客户端
            if (_webSocketService != null && _webSocketService.IsRunning)
            {
                _ = Task.Run(async () =>
                    await _webSocketService.BroadcastLiveMessageAsync(message)
                );
            }

            // 添加到UI队列进行批量更新
            lock (_pendingMessagesLock)
            {
                _pendingMessages.Enqueue(message);
            }
        }

        /// <summary>
        /// 异步保存消息到数据库（已废弃，使用批量队列）
        /// </summary>
        [Obsolete("使用 DatabaseService.QueueMessage 代替")]
        private async Task SaveMessageToDatabaseAsync(LiveMessage message)
        {
            // 此方法已被批量处理替代
            await Task.CompletedTask.ConfigureAwait(false);
        }

        /// <summary>
        /// 添加聊天消息
        /// </summary>
        private void AddChatMessage(LiveMessage message)
        {
            var item = new ListViewItem(message.Timestamp.ToString("HH:mm:ss"));
            item.SubItems.Add(message.UserName ?? "");
            item.SubItems.Add(FormatUserId(message.UserId));
            item.SubItems.Add(message.FansClubLevel > 0 ? message.FansClubLevel.ToString() : "-");
            item.SubItems.Add(message.PayGradeLevel > 0 ? message.PayGradeLevel.ToString() : "-");
            item.SubItems.Add(message.Content ?? "");

            listViewChat.Items.Add(item);

            // 自动滚动到底部
            if (checkBoxAutoScroll.Checked && listViewChat.Items.Count > 0)
            {
                listViewChat.EnsureVisible(listViewChat.Items.Count - 1);
            }
        }

        /// <summary>
        /// 添加聊天消息（内部方法，不触发重绘）
        /// </summary>
        private void AddChatMessageInternal(LiveMessage message)
        {
            var item = new ListViewItem(message.Timestamp.ToString("HH:mm:ss"));
            item.SubItems.Add(message.UserName ?? "");
            item.SubItems.Add(FormatUserId(message.UserId));
            item.SubItems.Add(message.FansClubLevel > 0 ? message.FansClubLevel.ToString() : "-");
            item.SubItems.Add(message.PayGradeLevel > 0 ? message.PayGradeLevel.ToString() : "-");
            item.SubItems.Add(message.Content ?? "");

            listViewChat.Items.Add(item);
        }

        /// <summary>
        /// 添加进场消息
        /// </summary>
        private void AddMemberMessage(LiveMessage message)
        {
            var item = new ListViewItem(message.Timestamp.ToString("HH:mm:ss"));
            item.SubItems.Add(message.UserName ?? "");
            item.SubItems.Add(FormatUserId(message.UserId));
            item.SubItems.Add(message.FansClubLevel > 0 ? message.FansClubLevel.ToString() : "-");
            item.SubItems.Add(message.PayGradeLevel > 0 ? message.PayGradeLevel.ToString() : "-");
            item.SubItems.Add(message.Content ?? "");

            listViewMember.Items.Add(item);

            // 自动滚动到底部
            if (checkBoxAutoScroll.Checked && listViewMember.Items.Count > 0)
            {
                listViewMember.EnsureVisible(listViewMember.Items.Count - 1);
            }
        }

        /// <summary>
        /// 添加进场消息（内部方法，不触发重绘）
        /// </summary>
        private void AddMemberMessageInternal(LiveMessage message)
        {
            var item = new ListViewItem(message.Timestamp.ToString("HH:mm:ss"));
            item.SubItems.Add(message.UserName ?? "");
            item.SubItems.Add(FormatUserId(message.UserId));
            item.SubItems.Add(message.FansClubLevel > 0 ? message.FansClubLevel.ToString() : "-");
            item.SubItems.Add(message.PayGradeLevel > 0 ? message.PayGradeLevel.ToString() : "-");
            item.SubItems.Add(message.Content ?? "");

            listViewMember.Items.Add(item);
        }

        /// <summary>
        /// 添加礼物和关注消息
        /// </summary>
        private void AddGiftFollowMessage(LiveMessage message)
        {
            var item = new ListViewItem(message.Timestamp.ToString("HH:mm:ss"));

            // 类型列
            string messageType = message.Type switch
            {
                LiveMessageType.Gift => "礼物",
                LiveMessageType.Like => "点赞",
                LiveMessageType.Social => "关注",
                _ => "其他",
            };
            item.SubItems.Add(messageType);

            item.SubItems.Add(message.UserName ?? "");
            item.SubItems.Add(FormatUserId(message.UserId));
            item.SubItems.Add(message.FansClubLevel > 0 ? message.FansClubLevel.ToString() : "-");
            item.SubItems.Add(message.PayGradeLevel > 0 ? message.PayGradeLevel.ToString() : "-");

            // 内容列
            string content = message.Type switch
            {
                LiveMessageType.Gift when message is GiftMessage gift =>
                    $"{gift.GiftName} x{gift.GiftCount}",
                LiveMessageType.Like when message is LikeMessage like => $"点赞 x{like.LikeCount}",
                _ => message.Content ?? "",
            };
            item.SubItems.Add(content);

            listViewGiftFollow.Items.Add(item);

            // 自动滚动到底部
            if (checkBoxAutoScroll.Checked && listViewGiftFollow.Items.Count > 0)
            {
                listViewGiftFollow.EnsureVisible(listViewGiftFollow.Items.Count - 1);
            }
        }

        /// <summary>
        /// 添加礼物和关注消息（内部方法，不触发重绘）
        /// </summary>
        private void AddGiftFollowMessageInternal(LiveMessage message)
        {
            var item = new ListViewItem(message.Timestamp.ToString("HH:mm:ss"));

            // 类型列
            string messageType = message.Type switch
            {
                LiveMessageType.Gift => "礼物",
                LiveMessageType.Like => "点赞",
                LiveMessageType.Social => "关注",
                _ => "其他",
            };
            item.SubItems.Add(messageType);

            item.SubItems.Add(message.UserName ?? "");
            item.SubItems.Add(FormatUserId(message.UserId));
            item.SubItems.Add(message.FansClubLevel > 0 ? message.FansClubLevel.ToString() : "-");
            item.SubItems.Add(message.PayGradeLevel > 0 ? message.PayGradeLevel.ToString() : "-");

            // 内容列
            string content = message.Type switch
            {
                LiveMessageType.Gift when message is GiftMessage gift =>
                    $"{gift.GiftName} x{gift.GiftCount}",
                LiveMessageType.Like when message is LikeMessage like => $"点赞 x{like.LikeCount}",
                _ => message.Content ?? "",
            };
            item.SubItems.Add(content);

            listViewGiftFollow.Items.Add(item);
        }

        /// <summary>
        /// 添加关注用户消息
        /// </summary>
        private void AddWatchedUserMessage(LiveMessage message)
        {
            var item = new ListViewItem(message.Timestamp.ToString("HH:mm:ss"));

            // 类型列
            string messageType = message.Type switch
            {
                LiveMessageType.Chat => "聊天",
                LiveMessageType.Gift => "礼物",
                LiveMessageType.Like => "点赞",
                LiveMessageType.Member => "进场",
                LiveMessageType.Social => "关注",
                _ => "其他",
            };
            item.SubItems.Add(messageType);

            // 用户名处理 - 优先使用保存的昵称信息
            string displayUserName = GetDisplayUserName(message.UserId, message.UserName);
            item.SubItems.Add(displayUserName);

            item.SubItems.Add(FormatUserId(message.UserId));
            item.SubItems.Add(message.FansClubLevel > 0 ? message.FansClubLevel.ToString() : "-");
            item.SubItems.Add(message.PayGradeLevel > 0 ? message.PayGradeLevel.ToString() : "-");

            // 内容列
            string content = message.Type switch
            {
                LiveMessageType.Chat => message.Content ?? "",
                LiveMessageType.Gift when message is GiftMessage gift =>
                    $"{gift.GiftName} x{gift.GiftCount}",
                LiveMessageType.Like when message is LikeMessage like => $"点赞 x{like.LikeCount}",
                LiveMessageType.Member => "进入直播间",
                LiveMessageType.Social => "关注了主播",
                _ => message.Content ?? "",
            };
            item.SubItems.Add(content);

            listViewWatchedUsers.Items.Add(item);

            // 自动滚动到底部
            if (checkBoxAutoScroll.Checked && listViewWatchedUsers.Items.Count > 0)
            {
                listViewWatchedUsers.EnsureVisible(listViewWatchedUsers.Items.Count - 1);
            }
        }

        /// <summary>
        /// 添加关注用户消息（内部方法，不触发重绘）
        /// </summary>
        private void AddWatchedUserMessageInternal(LiveMessage message)
        {
            var item = new ListViewItem(message.Timestamp.ToString("HH:mm:ss"));

            // 类型列
            string messageType = message.Type switch
            {
                LiveMessageType.Chat => "聊天",
                LiveMessageType.Gift => "礼物",
                LiveMessageType.Like => "点赞",
                LiveMessageType.Member => "进场",
                LiveMessageType.Social => "关注",
                _ => "其他",
            };
            item.SubItems.Add(messageType);

            // 用户名处理 - 优先使用保存的昵称信息
            string displayUserName = GetDisplayUserName(message.UserId, message.UserName);
            item.SubItems.Add(displayUserName);

            item.SubItems.Add(FormatUserId(message.UserId));
            item.SubItems.Add(message.FansClubLevel > 0 ? message.FansClubLevel.ToString() : "-");
            item.SubItems.Add(message.PayGradeLevel > 0 ? message.PayGradeLevel.ToString() : "-");

            // 内容列
            string content = message.Type switch
            {
                LiveMessageType.Chat => message.Content ?? "",
                LiveMessageType.Gift when message is GiftMessage gift =>
                    $"{gift.GiftName} x{gift.GiftCount}",
                LiveMessageType.Like when message is LikeMessage like => $"点赞 x{like.LikeCount}",
                LiveMessageType.Member => "进入直播间",
                LiveMessageType.Social => "关注了主播",
                _ => message.Content ?? "",
            };
            item.SubItems.Add(content);

            listViewWatchedUsers.Items.Add(item);

            // 添加调试信息
            Console.WriteLine(
                $"关注用户消息: 用户名='{message.UserName}' 显示名='{displayUserName}' 用户ID='{message.UserId}' 类型={message.Type}"
            );
        }

        /// <summary>
        /// 获取用户显示名称，优先使用保存的昵称
        /// </summary>
        private string GetDisplayUserName(string? userId, string? originalUserName)
        {
            // 如果用户ID为空或无效，使用原始用户名或默认值
            if (string.IsNullOrEmpty(userId) || userId == "111111")
            {
                return string.IsNullOrEmpty(originalUserName) ? "匿名用户" : originalUserName;
            }

            // 检查是否有保存的用户信息
            if (_appSettings.UserInfos != null && _appSettings.UserInfos.TryGetValue(userId, out UserInfo? userInfo))
            {
                if (!string.IsNullOrEmpty(userInfo.Nickname))
                {
                    // 如果有保存的昵称，使用格式：昵称 (原始用户名)
                    if (
                        !string.IsNullOrEmpty(originalUserName)
                        && originalUserName != userInfo.Nickname
                    )
                    {
                        return $"{userInfo.Nickname} ({originalUserName})";
                    }
                    else
                    {
                        return userInfo.Nickname;
                    }
                }
            }

            // 如果没有保存的昵称，使用原始用户名
            if (!string.IsNullOrEmpty(originalUserName))
            {
                return originalUserName;
            }

            // 最后的备选方案
            return $"用户{userId}";
        }

        /// <summary>
        /// 格式化用户ID显示
        /// </summary>
        private static string FormatUserId(string? userId)
        {
            if (string.IsNullOrEmpty(userId))
                return "-";
            if (userId == "111111")
                return "匿名用户";
            return userId;
        }

        /// <summary>
        /// 更新统计信息
        /// </summary>
        private void UpdateStatistics()
        {
            _ = UpdateDatabaseStatsAsync();
        }

        /// <summary>
        /// 异步更新数据库统计信息
        /// </summary>
        private async Task UpdateDatabaseStatsAsync()
        {
            if (_databaseService == null)
                return;

            try
            {
                var liveId = textBoxLiveId.Text.Trim();
                if (string.IsNullOrEmpty(liveId))
                    return;

                var dbStats = await _databaseService.GetStatsAsync(liveId);

                // 在UI线程中更新显示
                if (InvokeRequired)
                {
                    Invoke(new Action(() => UpdateDatabaseStatsDisplay(dbStats)));
                }
                else
                {
                    UpdateDatabaseStatsDisplay(dbStats);
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"获取数据库统计信息失败: {ex.Message}");
            }
        }

        /// <summary>
        /// 更新数据库统计信息显示
        /// </summary>
        private void UpdateDatabaseStatsDisplay(DatabaseStats dbStats)
        {
            // 更新状态栏显示数据库统计信息
            var dbStatsText =
                $"数据库统计 - 聊天:{dbStats.ChatMessageCount} 进场:{dbStats.MemberMessageCount} 礼物:{dbStats.GiftTotalCount} 点赞:{dbStats.LikeTotalCount} 互动行:{dbStats.InteractionMessageCount} 独立用户:{dbStats.UniqueUserCount}";

            // 可以考虑添加一个专门的标签来显示数据库统计，或者在状态信息中显示
            // 这里我们在状态信息中显示
            if (_isConnected)
            {
                var currentLiveId = textBoxLiveId.Text.Trim();
                UpdateStatus($"直播间 {currentLiveId} - {dbStatsText}");
            }
        }

        private void OnStatusChanged(object? sender, string status)
        {
            if (InvokeRequired)
            {
                Invoke(new Action<object?, string>(OnStatusChanged), sender, status);
                return;
            }

            UpdateStatus(status);
        }

        /// <summary>
        /// 更新状态信息
        /// </summary>
        private void UpdateStatus(string status)
        {
            textBoxStatus.AppendText($"[{DateTime.Now:HH:mm:ss}] {status}{Environment.NewLine}");
            textBoxStatus.ScrollToCaret();
        }

        /// <summary>
        /// 窗体关闭事件
        /// </summary>
        private async void Form1_FormClosing(object sender, FormClosingEventArgs e)
        {
            try
            {
                // 保存设置
                SaveSettings();

                // 断开所有多房间连接
                if (_connectionManager != null && _connectionManager.HasAnyConnection)
                {
                    e.Cancel = true;
                    UpdateStatus("正在断开所有房间连接...");
                    await _connectionManager.DisconnectAllAsync().ConfigureAwait(false);
                    await Task.Delay(1000).ConfigureAwait(false);

                    // 清理RoomTabPages
                    foreach (var roomTab in _roomTabs.Values)
                    {
                        roomTab.Dispose();
                    }
                    _roomTabs.Clear();

                    _connectionManager.Dispose();
                    _loggingService?.Dispose();

                    if (InvokeRequired)
                    {
                        Invoke(new Action(() => { e.Cancel = false; Close(); }));
                    }
                    else
                    {
                        e.Cancel = false;
                        Close();
                    }
                    return;
                }

                // 向后兼容：断开单房间连接
                if (_isConnected && _fetcher != null)
                {
                    e.Cancel = true;

                    UpdateStatus("正在断开连接...");
                    await DisconnectAsync().ConfigureAwait(false);
                    await Task.Delay(2000).ConfigureAwait(false);

                    if (InvokeRequired)
                    {
                        Invoke(new Action(() => { e.Cancel = false; Close(); }));
                    }
                    else
                    {
                        e.Cancel = false;
                        Close();
                    }
                    return;
                }

                // 释放ConnectionManager
                _connectionManager?.Dispose();
                _loggingService?.Dispose();

                // 释放数据库连接
                _databaseService?.Dispose();

                // 释放WebSocket服务
                if (_webSocketService != null)
                {
                    try
                    {
                        if (_webSocketService.IsRunning)
                        {
                            await _webSocketService.StopAsync();
                        }
                        _webSocketService.Dispose();
                    }
                    catch (Exception wsEx)
                    {
                        System.Diagnostics.Debug.WriteLine(
                            $"释放WebSocket服务时发生错误: {wsEx.Message}"
                        );
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"关闭窗体时发生错误: {ex.Message}");
            }
        }

        /// <summary>
        /// 连接按钮点击事件 - 支持多房间
        /// </summary>
        private async void ButtonConnect_Click(object sender, EventArgs e)
        {
            try
            {
                var liveId = textBoxLiveId.Text.Trim();

                // 如果已有多房间连接，添加新房间
                if (_roomTabs.Count > 0 || (_connectionManager?.HasAnyConnection == true))
                {
                    await AddRoomAndConnectAsync(liveId);
                }
                else if (_isConnected)
                {
                    // 向后兼容：单房间断开
                    await DisconnectAsync().ConfigureAwait(false);
                }
                else
                {
                    // 使用新的多房间模式连接
                    await AddRoomAndConnectAsync(liveId);
                }
            }
            catch (Exception ex)
            {
                OnErrorOccurred(this, ex);
            }
        }

        /// <summary>
        /// 连接到直播间
        /// </summary>
        private async Task ConnectAsync()
        {
            try
            {
                UpdateStatus("正在连接...");
                buttonConnect.Enabled = false;

                _fetcher = new Services.DouyinLiveFetcher(textBoxLiveId.Text.Trim());
                _fetcher.MessageReceived += OnMessageReceived;
                _fetcher.StatusChanged += OnStatusChanged;
                _fetcher.ErrorOccurred += OnErrorOccurred;

                // 检查直播间状态，但不再拦截连接流程
                var isLive = await _fetcher.GetRoomStatusAsync().ConfigureAwait(false);
                if (!isLive)
                {
                    UpdateStatus("开播状态接口不可用或未开播，尝试直接连接");
                }

                // 开始抓取
                await _fetcher.StartAsync().ConfigureAwait(false);
                _isConnected = true;

                // 如果启用了自动启动WebSocket，启动WebSocket服务
                if (_appSettings.AutoStartWebSocket && _appSettings.WebSocketEnabled)
                {
                    try
                    {
                        await _webSocketService!.StartAsync(_appSettings.WebSocketPort);
                    }
                    catch (Exception wsEx)
                    {
                        UpdateStatus($"WebSocket服务启动失败: {wsEx.Message}");
                    }
                }

                // 更新UI状态
                if (InvokeRequired)
                {
                    Invoke(
                        new Action(() =>
                        {
                            buttonConnect.Text = "断开连接";
                            buttonConnect.Enabled = true;
                            UpdateStatus("已连接");
                        })
                    );
                }
                else
                {
                    buttonConnect.Text = "断开连接";
                    buttonConnect.Enabled = true;
                    UpdateStatus("已连接");
                }
            }
            catch (Exception ex)
            {
                // 确保在异常情况下也重新启用按钮
                if (InvokeRequired)
                {
                    Invoke(
                        new Action(() =>
                        {
                            buttonConnect.Enabled = true;
                            buttonConnect.Text = "连接";
                        })
                    );
                }
                else
                {
                    buttonConnect.Enabled = true;
                    buttonConnect.Text = "连接";
                }

                OnErrorOccurred(this, ex);
            }
        }

        /// <summary>
        /// 断开连接
        /// </summary>
        private async Task DisconnectAsync()
        {
            try
            {
                if (_fetcher != null)
                {
                    _fetcher.MessageReceived -= OnMessageReceived;
                    _fetcher.StatusChanged -= OnStatusChanged;
                    _fetcher.ErrorOccurred -= OnErrorOccurred;

                    await _fetcher.StopAsync().ConfigureAwait(false);
                    _fetcher.Dispose();
                    _fetcher = null;
                }

                // 注意：WebSocket服务保持运行，不在这里停止
                // 用户可以独立控制WebSocket服务的启停

                _isConnected = false;

                // 更新UI状态
                if (InvokeRequired)
                {
                    Invoke(
                        new Action(() =>
                        {
                            buttonConnect.Text = "连接";
                            buttonConnect.Enabled = true;
                            UpdateStatus("已断开连接 - WebSocket服务保持运行");
                        })
                    );
                }
                else
                {
                    buttonConnect.Text = "连接";
                    buttonConnect.Enabled = true;
                    UpdateStatus("已断开连接 - WebSocket服务保持运行");
                }
            }
            catch (Exception ex)
            {
                OnErrorOccurred(this, ex);
            }
            finally
            {
                if (InvokeRequired)
                {
                    Invoke(
                        new Action(() =>
                        {
                            buttonConnect.Enabled = true;
                        })
                    );
                }
                else
                {
                    buttonConnect.Enabled = true;
                }
            }
        }

        /// <summary>
        /// 错误处理事件
        /// </summary>
        private void OnErrorOccurred(object? sender, Exception ex)
        {
            if (InvokeRequired)
            {
                Invoke(new Action<object?, Exception>(OnErrorOccurred), sender, ex);
                return;
            }

            UpdateStatus($"发生错误: {ex.Message}");

            // 如果是WebSocket连接错误，提供更详细的信息
            if (
                ex.Message.Contains("WebSocket")
                || ex.Message.Contains("连接")
                || ex.Message.Contains("网络")
            )
            {
                UpdateStatus($"详细错误信息: {ex}");
            }

            // 重置连接状态和按钮状态
            if (_isConnected)
            {
                _isConnected = false;
            }

            // 确保按钮状态正确
            buttonConnect.Text = "连接";
            buttonConnect.Enabled = true;
            textBoxLiveId.Enabled = true;
        }

        /// <summary>
        /// 直播间ID文本变更事件
        /// </summary>
        private void TextBoxLiveId_TextChanged(object sender, EventArgs e)
        {
            // 延迟保存，避免频繁保存
            if (!_isConnected) // 只在未连接状态下自动保存
            {
                SaveSettings();
            }
        }

        /// <summary>
        /// 自动滚动复选框变更事件
        /// </summary>
        private void CheckBoxAutoScroll_CheckedChanged(object sender, EventArgs e)
        {
            // 同步到所有RoomTabPage
            foreach (var roomTab in _roomTabs.Values)
            {
                roomTab.SetAutoScroll(checkBoxAutoScroll.Checked);
            }
            SaveSettings();
        }


        /// <summary>
        /// 设置按钮点击事件
        /// </summary>
        private void ButtonSettings_Click(object sender, EventArgs e)
        {
            using var settingsForm = new SettingsForm(
                _watchedUserIds,
                _appSettings.UserInfos,
                _databaseService
            );
            if (settingsForm.ShowDialog() == DialogResult.OK)
            {
                _watchedUserIds = settingsForm.WatchedUserIds;
                _appSettings.UserInfos = settingsForm.UserInfos;

                // 同步到所有RoomTabPage
                foreach (var roomTab in _roomTabs.Values)
                {
                    roomTab.UpdateWatchedUserIds(_watchedUserIds);
                }

                UpdateStatus($"已更新关注用户列表，共{_watchedUserIds.Count}个用户");

                // 自动保存设置
                SaveSettings();
            }
        }

        /// <summary>
        /// 数据库按钮点击事件
        /// </summary>
        private void ButtonDatabase_Click(object sender, EventArgs e)
        {
            if (_databaseService == null)
            {
                MessageBox.Show(
                    "数据库服务未初始化",
                    "错误",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error
                );
                return;
            }

            try
            {
                // 直接打开数据库查询界面
                using var queryForm = new DatabaseQueryForm(_databaseService);
                queryForm.ShowDialog();
            }
            catch (Exception ex)
            {
                MessageBox.Show(
                    $"打开数据库查询界面失败: {ex.Message}",
                    "错误",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error
                );
            }
        }

        /// <summary>
        /// 初始化数据库服务
        /// </summary>
        private async void InitializeDatabaseAsync()
        {
            try
            {
                UpdateStatus("正在初始化数据库...");
                _databaseService = new DatabaseService();
                await _databaseService.InitializeAsync();

                var dbPath = _databaseService.GetDatabasePath();
                UpdateStatus($"数据库已初始化: {dbPath}");

                // 验证数据库文件是否存在
                if (File.Exists(dbPath))
                {
                    var fileInfo = new FileInfo(dbPath);
                    UpdateStatus($"数据库文件大小: {fileInfo.Length} 字节");
                }
                else
                {
                    UpdateStatus("警告: 数据库文件不存在");
                }
            }
            catch (Exception ex)
            {
                var errorMsg = $"数据库初始化失败: {ex.Message}";
                if (ex.InnerException != null)
                {
                    errorMsg += $" 内部错误: {ex.InnerException.Message}";
                }

                UpdateStatus(errorMsg);

                // 显示详细错误信息
                MessageBox.Show(
                    $"数据库初始化失败，可能影响消息保存功能。\n\n错误详情:\n{errorMsg}\n\n程序将继续运行，但不会保存消息到数据库。",
                    "数据库初始化错误",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Warning
                );
            }
        }

        /// <summary>
        /// 在程序启动时测试JavaScript引擎
        /// </summary>
        private void TestJavaScriptEngineOnStartup()
        {
            try
            {
                UpdateStatus("正在测试JavaScript引擎...");

                // 简单测试SignatureGenerator是否可用
                using var signatureGenerator = new Services.SignatureGenerator();
                var testUrl = "wss://webcast3-ws-web-lq.douyin.com/webcast/im/push/v2/?test=1";
                var signature = signatureGenerator.GenerateSignature(testUrl);

                if (!string.IsNullOrEmpty(signature))
                {
                    UpdateStatus("JavaScript引擎测试成功");
                }
                else
                {
                    UpdateStatus("JavaScript引擎测试失败：签名为空");
                }
            }
            catch (Exception ex)
            {
                UpdateStatus($"JavaScript引擎测试失败: {ex.Message}");
            }
        }

        /// <summary>
        /// 初始化WebSocket服务
        /// </summary>
        private void InitializeWebSocketService()
        {
            try
            {
                _webSocketService = new Services.WebSocketService();
                _webSocketService.StatusChanged += OnWebSocketStatusChanged;
                _webSocketService.ErrorOccurred += OnWebSocketErrorOccurred;

                UpdateStatus("WebSocket服务已初始化");
            }
            catch (Exception ex)
            {
                UpdateStatus($"WebSocket服务初始化失败: {ex.Message}");
            }
        }

        /// <summary>
        /// WebSocket状态变化事件处理
        /// </summary>
        private void OnWebSocketStatusChanged(object? sender, string status)
        {
            if (InvokeRequired)
            {
                Invoke(new Action<object?, string>(OnWebSocketStatusChanged), sender, status);
                return;
            }

            UpdateStatus($"WebSocket: {status}");
        }

        /// <summary>
        /// WebSocket错误事件处理
        /// </summary>
        private void OnWebSocketErrorOccurred(object? sender, Exception ex)
        {
            if (InvokeRequired)
            {
                Invoke(new Action<object?, Exception>(OnWebSocketErrorOccurred), sender, ex);
                return;
            }

            UpdateStatus($"WebSocket错误: {ex.Message}");
        }

        /// <summary>
        /// WebSocket按钮点击事件
        /// </summary>
        private void ButtonWebSocket_Click(object sender, EventArgs e)
        {
            try
            {
                using var settingsForm = new WebSocketSettingsForm(_appSettings, _webSocketService);
                if (settingsForm.ShowDialog() == DialogResult.OK)
                {
                    // 更新设置
                    _appSettings.WebSocketEnabled = settingsForm.Settings.WebSocketEnabled;
                    _appSettings.WebSocketPort = settingsForm.Settings.WebSocketPort;
                    _appSettings.AutoStartWebSocket = settingsForm.Settings.AutoStartWebSocket;

                    // 保存设置
                    SaveSettings();

                    var statusText = _webSocketService?.IsRunning == true
                        ? $"WebSocket设置已更新 - 启用: {_appSettings.WebSocketEnabled}, 端口: {_appSettings.WebSocketPort}, 自动启动: {_appSettings.AutoStartWebSocket} (当前运行中)"
                        : $"WebSocket设置已更新 - 启用: {_appSettings.WebSocketEnabled}, 端口: {_appSettings.WebSocketPort}, 自动启动: {_appSettings.AutoStartWebSocket}";

                    UpdateStatus(statusText);
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"打开WebSocket设置失败: {ex.Message}", "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        /// <summary>
        /// 重启WebSocket服务
        /// </summary>
        private async Task RestartWebSocketServiceAsync()
        {
            try
            {
                if (_webSocketService != null && _webSocketService.IsRunning)
                {
                    await _webSocketService.StopAsync();
                }

                if (_appSettings.WebSocketEnabled)
                {
                    await _webSocketService!.StartAsync(_appSettings.WebSocketPort);
                }
            }
            catch (Exception ex)
            {
                UpdateStatus($"重启WebSocket服务失败: {ex.Message}");
            }
        }

        #region 右键菜单事件处理

        /// <summary>
        /// 右键菜单打开前的事件处理
        /// </summary>
        private void ContextMenuStripMessage_Opening(
            object sender,
            System.ComponentModel.CancelEventArgs e
        )
        {
            var contextMenu = sender as ContextMenuStrip;
            if (contextMenu?.SourceControl is ListView listView)
            {
                // 检查是否有选中的项
                if (listView.SelectedItems.Count == 0)
                {
                    e.Cancel = true;
                    return;
                }

                var selectedItem = listView.SelectedItems[0];
                var userId = GetUserIdFromListViewItem(selectedItem);
                var userName = GetUserNameFromListViewItem(selectedItem);

                // 根据用户ID状态设置菜单项的可见性和文本
                if (string.IsNullOrEmpty(userId) || userId == "-" || userId == "匿名用户")
                {
                    // 无效用户ID，禁用相关菜单项
                    toolStripMenuItemAddToWatch.Enabled = false;
                    toolStripMenuItemRemoveFromWatch.Enabled = false;
                    toolStripMenuItemCopyUserId.Enabled = false;
                    toolStripMenuItemAddToWatch.Text = "添加到关注列表 (无效用户ID)";
                }
                else
                {
                    toolStripMenuItemCopyUserId.Enabled = true;

                    // 检查用户是否已在关注列表中
                    bool isWatched = _watchedUserIds.Contains(userId);

                    toolStripMenuItemAddToWatch.Enabled = !isWatched;
                    toolStripMenuItemRemoveFromWatch.Enabled = isWatched;

                    if (isWatched)
                    {
                        toolStripMenuItemAddToWatch.Text = "添加到关注列表 (已关注)";
                        toolStripMenuItemRemoveFromWatch.Text = $"从关注列表移除 ({userName})";
                    }
                    else
                    {
                        toolStripMenuItemAddToWatch.Text = $"添加到关注列表 ({userName})";
                        toolStripMenuItemRemoveFromWatch.Text = "从关注列表移除";
                    }
                }

                // 设置复制用户名菜单项
                if (string.IsNullOrEmpty(userName) || userName == "-")
                {
                    toolStripMenuItemCopyUserName.Enabled = false;
                    toolStripMenuItemCopyUserName.Text = "复制用户名 (无用户名)";
                }
                else
                {
                    toolStripMenuItemCopyUserName.Enabled = true;
                    toolStripMenuItemCopyUserName.Text = $"复制用户名 ({userName})";
                }
            }
        }

        /// <summary>
        /// 添加到关注列表菜单项点击事件
        /// </summary>
        private void ToolStripMenuItemAddToWatch_Click(object sender, EventArgs e)
        {
            var listView = GetActiveListView();
            if (listView?.SelectedItems.Count > 0)
            {
                var selectedItem = listView.SelectedItems[0];
                var userId = GetUserIdFromListViewItem(selectedItem);
                var userName = GetUserNameFromListViewItem(selectedItem);

                if (!string.IsNullOrEmpty(userId) && userId != "-" && userId != "匿名用户")
                {
                    if (!_watchedUserIds.Contains(userId))
                    {
                        _watchedUserIds.Add(userId);

                        // 同时保存用户信息到UserInfos
                        if (!string.IsNullOrEmpty(userName) && userName != "-")
                        {
                            _appSettings.UserInfos ??= [];

                            // 如果用户信息不存在，创建新的用户信息
                            if (!_appSettings.UserInfos.TryGetValue(userId, out UserInfo? existingUserInfo))
                            {
                                _appSettings.UserInfos[userId] = new UserInfo(userId, userName);
                            }
                            else
                            {
                                if (string.IsNullOrEmpty(existingUserInfo.Nickname))
                                {
                                    existingUserInfo.Nickname = userName;
                                    existingUserInfo.LastUpdated = DateTime.Now;
                                }
                            }
                        }

                        SaveSettings();
                        UpdateStatus($"已将用户 {userName} (ID: {userId}) 添加到关注列表");
                    }
                    else
                    {
                        UpdateStatus($"用户 {userName} (ID: {userId}) 已在关注列表中");
                    }
                }
            }
        }

        /// <summary>
        /// 从关注列表移除菜单项点击事件
        /// </summary>
        private void ToolStripMenuItemRemoveFromWatch_Click(object sender, EventArgs e)
        {
            var listView = GetActiveListView();
            if (listView?.SelectedItems.Count > 0)
            {
                var selectedItem = listView.SelectedItems[0];
                var userId = GetUserIdFromListViewItem(selectedItem);
                var userName = GetUserNameFromListViewItem(selectedItem);

                if (!string.IsNullOrEmpty(userId) && _watchedUserIds.Contains(userId))
                {
                    _watchedUserIds.Remove(userId);
                    SaveSettings();
                    UpdateStatus($"已将用户 {userName} (ID: {userId}) 从关注列表移除");
                }
            }
        }

        /// <summary>
        /// 复制用户ID菜单项点击事件
        /// </summary>
        private void ToolStripMenuItemCopyUserId_Click(object sender, EventArgs e)
        {
            var listView = GetActiveListView();
            if (listView?.SelectedItems.Count > 0)
            {
                var selectedItem = listView.SelectedItems[0];
                var userId = GetUserIdFromListViewItem(selectedItem);

                if (!string.IsNullOrEmpty(userId) && userId != "-")
                {
                    try
                    {
                        Clipboard.SetText(userId);
                        UpdateStatus($"已复制用户ID: {userId}");
                    }
                    catch (Exception ex)
                    {
                        UpdateStatus($"复制用户ID失败: {ex.Message}");
                    }
                }
            }
        }

        /// <summary>
        /// 复制用户名菜单项点击事件
        /// </summary>
        private void ToolStripMenuItemCopyUserName_Click(object sender, EventArgs e)
        {
            var listView = GetActiveListView();
            if (listView?.SelectedItems.Count > 0)
            {
                var selectedItem = listView.SelectedItems[0];
                var userName = GetUserNameFromListViewItem(selectedItem);

                if (!string.IsNullOrEmpty(userName) && userName != "-")
                {
                    try
                    {
                        Clipboard.SetText(userName);
                        UpdateStatus($"已复制用户名: {userName}");
                    }
                    catch (Exception ex)
                    {
                        UpdateStatus($"复制用户名失败: {ex.Message}");
                    }
                }
            }
        }

        /// <summary>
        /// 获取当前活动的ListView（右键菜单的源控件）
        /// </summary>
        private ListView? GetActiveListView()
        {
            // 通过ContextMenuStrip的SourceControl属性获取
            return contextMenuStripMessage.SourceControl as ListView;
        }

        /// <summary>
        /// 从ListViewItem中获取用户ID
        /// </summary>
        private string GetUserIdFromListViewItem(ListViewItem item)
        {
            // 用户ID在不同ListView中的列索引
            // 聊天消息: 列2, 进场消息: 列2 (6列: 时间,用户,用户ID,粉丝团,财富,内容)
            // 礼物&关注: 列3, 关注用户: 列3 (7列: 时间,类型,用户,用户ID,粉丝团,财富,内容)
            var listView = item.ListView;

            // 原有单房间ListViews
            if (listView == listViewChat || listView == listViewMember)
            {
                return item.SubItems.Count > 2 ? item.SubItems[2].Text : "";
            }
            else if (listView == listViewGiftFollow || listView == listViewWatchedUsers)
            {
                return item.SubItems.Count > 3 ? item.SubItems[3].Text : "";
            }

            // 多房间模式：根据列数判断（6列=Chat/Member，7列=GiftFollow/Watched）
            if (listView != null && listView.Columns.Count == 6)
            {
                return item.SubItems.Count > 2 ? item.SubItems[2].Text : "";
            }
            else if (listView != null && listView.Columns.Count == 7)
            {
                return item.SubItems.Count > 3 ? item.SubItems[3].Text : "";
            }

            return "";
        }

        /// <summary>
        /// 从ListViewItem中获取用户名
        /// </summary>
        private string GetUserNameFromListViewItem(ListViewItem item)
        {
            // 用户名在不同ListView中的列索引
            // 聊天消息: 列1, 进场消息: 列1 (6列)
            // 礼物&关注: 列2, 关注用户: 列2 (7列)
            var listView = item.ListView;

            // 原有单房间ListViews
            if (listView == listViewChat || listView == listViewMember)
            {
                return item.SubItems.Count > 1 ? item.SubItems[1].Text : "";
            }
            else if (listView == listViewGiftFollow || listView == listViewWatchedUsers)
            {
                return item.SubItems.Count > 2 ? item.SubItems[2].Text : "";
            }

            // 多房间模式：根据列数判断
            if (listView != null && listView.Columns.Count == 6)
            {
                return item.SubItems.Count > 1 ? item.SubItems[1].Text : "";
            }
            else if (listView != null && listView.Columns.Count == 7)
            {
                return item.SubItems.Count > 2 ? item.SubItems[2].Text : "";
            }

            return "";
        }

        #endregion
    }
}
