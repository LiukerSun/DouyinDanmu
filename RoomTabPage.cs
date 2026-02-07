using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;
using DouyinDanmu.Models;

namespace DouyinDanmu
{
    /// <summary>
    /// 单个直播间的TabPage，封装独立的消息展示和状态管理
    /// </summary>
    public class RoomTabPage : TabPage, IDisposable
    {
        public string RoomId { get; }
        public string LiveId { get; }

        // 4个独立的ListView
        private readonly ListView _listViewChat;
        private readonly ListView _listViewMember;
        private readonly ListView _listViewGiftFollow;
        private readonly ListView _listViewWatchedUsers;

        // 容器GroupBox
        private readonly GroupBox _groupBoxChat;
        private readonly GroupBox _groupBoxMember;
        private readonly GroupBox _groupBoxGiftFollow;
        private readonly GroupBox _groupBoxWatchedUsers;

        // 状态栏
        private readonly Label _statusLabel;
        private readonly Button _disconnectButton;
        private readonly Button _closeButton;
        private readonly Panel _topPanel;

        // 四列布局容器
        private readonly TableLayoutPanel _contentPanel;

        // 消息队列
        private readonly ConcurrentQueue<LiveMessage> _pendingMessages = new();
        private readonly System.Windows.Forms.Timer _uiUpdateTimer;
        private bool _autoScroll = true;

        // 礼物聚合
        private readonly Dictionary<string, (ListViewItem item, int count, DateTime ts)> _giftAggregates = new();
        private readonly Dictionary<string, (ListViewItem item, int count, DateTime ts)> _watchedGiftAggregates = new();

        // 关注用户列表引用（从Form1共享）
        private List<string> _watchedUserIds;
        private readonly Func<string?, string?, string> _getDisplayUserName;

        private bool _disposed;

        public event EventHandler? DisconnectRequested;
        public event EventHandler? CloseRequested;

        public RoomTabPage(
            string roomId,
            string liveId,
            List<string> watchedUserIds,
            Func<string?, string?, string> getDisplayUserName)
        {
            RoomId = roomId;
            LiveId = liveId;
            _watchedUserIds = watchedUserIds;
            _getDisplayUserName = getDisplayUserName;

            Text = string.IsNullOrEmpty(liveId) ? $"房间 {roomId}" : $"房间 {liveId}";
            Tag = roomId;

            // 消除 TabPage 默认的 {3,3,3,3} Padding
            Padding = new Padding(0);

            // === 顶部状态栏 ===
            _statusLabel = new Label
            {
                Text = "未连接",
                AutoSize = false,
                ForeColor = Color.Gray,
                TextAlign = ContentAlignment.MiddleLeft,
                Margin = Padding.Empty
            };

            _disconnectButton = new Button
            {
                Text = "断开",
                Size = new Size(60, 25),
                Enabled = false,
                Margin = Padding.Empty
            };
            _disconnectButton.Click += (s, e) => DisconnectRequested?.Invoke(this, EventArgs.Empty);

            _closeButton = new Button
            {
                Text = "关闭",
                Size = new Size(50, 25),
                Margin = Padding.Empty
            };
            _closeButton.Click += (s, e) => CloseRequested?.Invoke(this, EventArgs.Empty);

            _topPanel = new Panel
            {
                BackColor = SystemColors.Control,
                Margin = Padding.Empty
            };
            _topPanel.Controls.Add(_statusLabel);
            _topPanel.Controls.Add(_disconnectButton);
            _topPanel.Controls.Add(_closeButton);

            // topPanel 内部控件的布局由 topPanel 自己的 Resize 驱动
            _topPanel.Resize += (s, e) =>
            {
                int pw = _topPanel.ClientSize.Width;
                int ph = _topPanel.ClientSize.Height;
                if (pw <= 0 || ph <= 0) return;

                int btnY = (ph - _closeButton.Height) / 2;
                _closeButton.SetBounds(pw - _closeButton.Width - 8, btnY, _closeButton.Width, _closeButton.Height);
                _disconnectButton.SetBounds(_closeButton.Left - _disconnectButton.Width - 5, btnY, _disconnectButton.Width, _disconnectButton.Height);
                _statusLabel.SetBounds(8, 0, _disconnectButton.Left - 16, ph);
            };

            // === 创建4个GroupBox + ListView ===
            _groupBoxChat = CreateGroupBox("聊天消息");
            _listViewChat = CreateListView(
                ("时间", 65), ("用户", 80), ("用户ID", 80),
                ("粉丝团", 45), ("财富", 45), ("内容", 200));
            _groupBoxChat.Controls.Add(_listViewChat);

            _groupBoxMember = CreateGroupBox("进场消息");
            _listViewMember = CreateListView(
                ("时间", 65), ("用户", 80), ("用户ID", 80),
                ("粉丝团", 45), ("财富", 45), ("内容", 100));
            _groupBoxMember.Controls.Add(_listViewMember);

            _groupBoxGiftFollow = CreateGroupBox("礼物&关注");
            _listViewGiftFollow = CreateListView(
                ("时间", 65), ("类型", 45), ("用户", 80), ("用户ID", 80),
                ("粉丝团", 45), ("财富", 45), ("内容", 120));
            _groupBoxGiftFollow.Controls.Add(_listViewGiftFollow);

            _groupBoxWatchedUsers = CreateGroupBox("关注用户");
            _listViewWatchedUsers = CreateListView(
                ("时间", 65), ("类型", 45), ("用户", 80), ("用户ID", 80),
                ("粉丝团", 45), ("财富", 45), ("内容", 120));
            _groupBoxWatchedUsers.Controls.Add(_listViewWatchedUsers);

            // === TableLayoutPanel: 1行4列，均分宽度 ===
            _contentPanel = new TableLayoutPanel
            {
                ColumnCount = 4,
                RowCount = 1
            };
            _contentPanel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 25F));
            _contentPanel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 25F));
            _contentPanel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 25F));
            _contentPanel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 25F));
            _contentPanel.RowStyles.Add(new RowStyle(SizeType.Percent, 100F));

            _contentPanel.Controls.Add(_groupBoxChat, 0, 0);
            _contentPanel.Controls.Add(_groupBoxMember, 1, 0);
            _contentPanel.Controls.Add(_groupBoxGiftFollow, 2, 0);
            _contentPanel.Controls.Add(_groupBoxWatchedUsers, 3, 0);

            // 添加到 TabPage（先添加的在底层，后添加的在顶层）
            // contentPanel 先添加，topPanel 后添加确保在最上层
            Controls.Add(_contentPanel);
            Controls.Add(_topPanel);
            _topPanel.BringToFront();

            // 启用双缓冲
            EnableListViewDoubleBuffering(_listViewChat);
            EnableListViewDoubleBuffering(_listViewMember);
            EnableListViewDoubleBuffering(_listViewGiftFollow);
            EnableListViewDoubleBuffering(_listViewWatchedUsers);

            // 启动UI更新定时器
            _uiUpdateTimer = new System.Windows.Forms.Timer { Interval = 100 };
            _uiUpdateTimer.Tick += UiUpdateTimer_Tick;
            _uiUpdateTimer.Start();

            // 布局 - 使用手动定位，不依赖 Dock（TabPage 内 Dock 行为不可靠）
            Resize += (s, e) => PerformLayout_Manual();
            // 首次显示时也要布局
            VisibleChanged += (s, e) => { if (Visible) PerformLayout_Manual(); };
        }

        private void PerformLayout_Manual()
        {
            if (ClientSize.Width <= 0 || ClientSize.Height <= 0) return;

            const int topPanelHeight = 38;
            int w = ClientSize.Width;
            int h = ClientSize.Height;

            // 顶部面板（内部控件由 _topPanel.Resize 事件自动布局）
            _topPanel.SetBounds(0, 0, w, topPanelHeight);

            // 内容面板填充剩余空间
            _contentPanel.SetBounds(0, topPanelHeight, w, h - topPanelHeight);
        }

        public void UpdateWatchedUserIds(List<string> watchedUserIds)
        {
            _watchedUserIds = watchedUserIds;
        }

        public void SetAutoScroll(bool autoScroll)
        {
            _autoScroll = autoScroll;
        }

        /// <summary>
        /// 入队消息（线程安全，可从任意线程调用）
        /// </summary>
        public void EnqueueMessage(LiveMessage message)
        {
            _pendingMessages.Enqueue(message);
        }

        /// <summary>
        /// 更新连接状态显示
        /// </summary>
        public void UpdateConnectionState(string state, bool connected)
        {
            if (InvokeRequired)
            {
                BeginInvoke(() => UpdateConnectionState(state, connected));
                return;
            }
            _statusLabel.Text = state;
            _statusLabel.ForeColor = connected ? Color.Green : Color.Gray;
            _disconnectButton.Enabled = connected;
            Text = connected ? $"[*] {LiveId}" : $"房间 {LiveId}";
        }

        /// <summary>
        /// 清空所有消息
        /// </summary>
        public void ClearMessages()
        {
            _listViewChat.Items.Clear();
            _listViewMember.Items.Clear();
            _listViewGiftFollow.Items.Clear();
            _listViewWatchedUsers.Items.Clear();
            _giftAggregates.Clear();
            _watchedGiftAggregates.Clear();
        }

        /// <summary>
        /// 获取所有ListView（用于右键菜单等）
        /// </summary>
        public IEnumerable<ListView> GetAllListViews()
        {
            yield return _listViewChat;
            yield return _listViewMember;
            yield return _listViewGiftFollow;
            yield return _listViewWatchedUsers;
        }

        /// <summary>
        /// 获取消息统计
        /// </summary>
        public (int chat, int member, int giftFollow, int watched) GetMessageCounts()
        {
            return (
                _listViewChat.Items.Count,
                _listViewMember.Items.Count,
                _listViewGiftFollow.Items.Count,
                _listViewWatchedUsers.Items.Count
            );
        }

        #region Private Methods

        private void UiUpdateTimer_Tick(object? sender, EventArgs e)
        {
            ProcessPendingMessages();
        }

        private void ProcessPendingMessages()
        {
            var batch = new List<LiveMessage>();
            while (_pendingMessages.TryDequeue(out var msg) && batch.Count < 50)
            {
                batch.Add(msg);
            }
            if (batch.Count == 0) return;

            _listViewChat.BeginUpdate();
            _listViewMember.BeginUpdate();
            _listViewGiftFollow.BeginUpdate();
            _listViewWatchedUsers.BeginUpdate();

            try
            {
                foreach (var message in batch)
                {
                    ProcessSingleMessage(message);
                }
            }
            finally
            {
                _listViewChat.EndUpdate();
                _listViewMember.EndUpdate();
                _listViewGiftFollow.EndUpdate();
                _listViewWatchedUsers.EndUpdate();

                if (_autoScroll)
                {
                    PerformAutoScroll();
                }
            }
        }

        private void ProcessSingleMessage(LiveMessage message)
        {
            bool isWatchedUser = !string.IsNullOrEmpty(message.UserId)
                && _watchedUserIds.Contains(message.UserId);

            if (isWatchedUser)
            {
                if (message.Type == LiveMessageType.Gift && message is GiftMessage giftMsg)
                    AddOrUpdateWatchedGift(giftMsg);
                else
                    AddWatchedUserMessage(message);
            }

            switch (message.Type)
            {
                case LiveMessageType.Chat:
                    AddChatMessage(message);
                    break;
                case LiveMessageType.Member:
                    AddMemberMessage(message);
                    break;
                case LiveMessageType.Gift:
                    if (message is GiftMessage gift)
                        AddOrUpdateGift(gift);
                    break;
                case LiveMessageType.Like:
                case LiveMessageType.Social:
                    AddGiftFollowMessage(message);
                    break;
            }
        }

        private void AddChatMessage(LiveMessage message)
        {
            var item = new ListViewItem(message.Timestamp.ToString("HH:mm:ss"));
            item.SubItems.Add(message.UserName ?? "");
            item.SubItems.Add(FormatUserId(message.UserId));
            item.SubItems.Add(message.FansClubLevel > 0 ? message.FansClubLevel.ToString() : "-");
            item.SubItems.Add(message.PayGradeLevel > 0 ? message.PayGradeLevel.ToString() : "-");
            item.SubItems.Add(message.Content ?? "");
            _listViewChat.Items.Add(item);
        }

        private void AddMemberMessage(LiveMessage message)
        {
            var item = new ListViewItem(message.Timestamp.ToString("HH:mm:ss"));
            item.SubItems.Add(message.UserName ?? "");
            item.SubItems.Add(FormatUserId(message.UserId));
            item.SubItems.Add(message.FansClubLevel > 0 ? message.FansClubLevel.ToString() : "-");
            item.SubItems.Add(message.PayGradeLevel > 0 ? message.PayGradeLevel.ToString() : "-");
            item.SubItems.Add(message.Content ?? "");
            _listViewMember.Items.Add(item);
        }

        private void AddGiftFollowMessage(LiveMessage message)
        {
            var item = new ListViewItem(message.Timestamp.ToString("HH:mm:ss"));
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
            string content = message.Type switch
            {
                LiveMessageType.Gift when message is GiftMessage gift => $"{gift.GiftName} x{gift.GiftCount}",
                LiveMessageType.Like when message is LikeMessage like => $"点赞 x{like.LikeCount}",
                _ => message.Content ?? "",
            };
            item.SubItems.Add(content);
            _listViewGiftFollow.Items.Add(item);
        }

        private void AddOrUpdateGift(GiftMessage gift)
        {
            var key = (gift.UserId ?? "") + "|" + (gift.GiftName ?? "");
            if (_giftAggregates.TryGetValue(key, out var agg) && gift.GiftCount >= agg.count)
            {
                agg.item.SubItems[6].Text = $"{gift.GiftName} x{gift.GiftCount}";
                _giftAggregates[key] = (agg.item, gift.GiftCount, DateTime.Now);
                return;
            }

            var item = new ListViewItem(gift.Timestamp.ToString("HH:mm:ss"));
            item.SubItems.Add("礼物");
            item.SubItems.Add(gift.UserName ?? "");
            item.SubItems.Add(FormatUserId(gift.UserId));
            item.SubItems.Add(gift.FansClubLevel > 0 ? gift.FansClubLevel.ToString() : "-");
            item.SubItems.Add(gift.PayGradeLevel > 0 ? gift.PayGradeLevel.ToString() : "-");
            item.SubItems.Add($"{gift.GiftName} x{gift.GiftCount}");
            _listViewGiftFollow.Items.Add(item);
            _giftAggregates[key] = (item, gift.GiftCount, DateTime.Now);
        }

        private void AddOrUpdateWatchedGift(GiftMessage gift)
        {
            var key = (gift.UserId ?? "") + "|" + (gift.GiftName ?? "");
            if (_watchedGiftAggregates.TryGetValue(key, out var agg) && gift.GiftCount >= agg.count)
            {
                agg.item.SubItems[6].Text = $"{gift.GiftName} x{gift.GiftCount}";
                _watchedGiftAggregates[key] = (agg.item, gift.GiftCount, DateTime.Now);
                return;
            }

            var item = new ListViewItem(gift.Timestamp.ToString("HH:mm:ss"));
            item.SubItems.Add("礼物");
            item.SubItems.Add(_getDisplayUserName(gift.UserId, gift.UserName));
            item.SubItems.Add(FormatUserId(gift.UserId));
            item.SubItems.Add(gift.FansClubLevel > 0 ? gift.FansClubLevel.ToString() : "-");
            item.SubItems.Add(gift.PayGradeLevel > 0 ? gift.PayGradeLevel.ToString() : "-");
            item.SubItems.Add($"{gift.GiftName} x{gift.GiftCount}");
            _listViewWatchedUsers.Items.Add(item);
            _watchedGiftAggregates[key] = (item, gift.GiftCount, DateTime.Now);
        }

        private void AddWatchedUserMessage(LiveMessage message)
        {
            var item = new ListViewItem(message.Timestamp.ToString("HH:mm:ss"));
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
            item.SubItems.Add(_getDisplayUserName(message.UserId, message.UserName));
            item.SubItems.Add(FormatUserId(message.UserId));
            item.SubItems.Add(message.FansClubLevel > 0 ? message.FansClubLevel.ToString() : "-");
            item.SubItems.Add(message.PayGradeLevel > 0 ? message.PayGradeLevel.ToString() : "-");
            string content = message.Type switch
            {
                LiveMessageType.Chat => message.Content ?? "",
                LiveMessageType.Gift when message is GiftMessage gift => $"{gift.GiftName} x{gift.GiftCount}",
                LiveMessageType.Like when message is LikeMessage like => $"点赞 x{like.LikeCount}",
                LiveMessageType.Member => "进入直播间",
                LiveMessageType.Social => "关注了主播",
                _ => message.Content ?? "",
            };
            item.SubItems.Add(content);
            _listViewWatchedUsers.Items.Add(item);
        }

        private void PerformAutoScroll()
        {
            if (_listViewChat.Items.Count > 0)
                _listViewChat.EnsureVisible(_listViewChat.Items.Count - 1);
            if (_listViewMember.Items.Count > 0)
                _listViewMember.EnsureVisible(_listViewMember.Items.Count - 1);
            if (_listViewGiftFollow.Items.Count > 0)
                _listViewGiftFollow.EnsureVisible(_listViewGiftFollow.Items.Count - 1);
            if (_listViewWatchedUsers.Items.Count > 0)
                _listViewWatchedUsers.EnsureVisible(_listViewWatchedUsers.Items.Count - 1);
        }

        private static GroupBox CreateGroupBox(string title)
        {
            return new GroupBox
            {
                Text = title,
                Font = new Font("Microsoft YaHei UI", 9F),
                Dock = DockStyle.Fill,
                Margin = new Padding(2)
            };
        }

        private static ListView CreateListView(params (string text, int width)[] columns)
        {
            var lv = new ListView
            {
                View = View.Details,
                FullRowSelect = true,
                GridLines = true,
                Dock = DockStyle.Fill,
                Font = new Font("Microsoft YaHei UI", 9F),
                HeaderStyle = ColumnHeaderStyle.Nonclickable
            };

            foreach (var (text, width) in columns)
            {
                lv.Columns.Add(text, width);
            }

            return lv;
        }

        private static void EnableListViewDoubleBuffering(ListView listView)
        {
            typeof(ListView).InvokeMember(
                "DoubleBuffered",
                System.Reflection.BindingFlags.NonPublic
                    | System.Reflection.BindingFlags.Instance
                    | System.Reflection.BindingFlags.SetProperty,
                null,
                listView,
                new object[] { true }
            );
        }

        private static string FormatUserId(string? userId)
        {
            if (string.IsNullOrEmpty(userId)) return "-";
            if (userId == "111111") return "匿名用户";
            return userId;
        }

        #endregion

        public new void Dispose()
        {
            if (!_disposed)
            {
                _uiUpdateTimer?.Stop();
                _uiUpdateTimer?.Dispose();
                _disposed = true;
            }
            base.Dispose();
        }
    }
}
