using DouyinDanmu.Models;
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;

namespace DouyinDanmu.Services
{
    /// <summary>
    /// UI更新服务 - 负责处理界面更新逻辑
    /// </summary>
    public class UIUpdateService
    {
        private readonly ListView _chatListView;
        private readonly ListView _memberListView;
        private readonly ListView _giftFollowListView;
        private readonly ListView _watchedUsersListView;
        private readonly Label _statusLabel;
        private readonly Label _statisticsLabel;

        // 统计数据
        private int _totalMessages = 0;
        private int _chatMessages = 0;
        private int _giftMessages = 0;
        private int _likeMessages = 0;
        private int _memberMessages = 0;
        private int _followMessages = 0;

        public UIUpdateService(
            ListView chatListView,
            ListView memberListView, 
            ListView giftFollowListView,
            ListView watchedUsersListView,
            Label statusLabel,
            Label statisticsLabel)
        {
            _chatListView = chatListView;
            _memberListView = memberListView;
            _giftFollowListView = giftFollowListView;
            _watchedUsersListView = watchedUsersListView;
            _statusLabel = statusLabel;
            _statisticsLabel = statisticsLabel;
        }

        /// <summary>
        /// 批量更新消息到UI
        /// </summary>
        public void UpdateMessages(List<LiveMessage> messages)
        {
            if (messages.Count == 0) return;

            foreach (var message in messages)
            {
                UpdateMessageToUI(message);
                UpdateStatistics(message);
            }

            UpdateStatisticsDisplay();
        }

        /// <summary>
        /// 更新单条消息到UI
        /// </summary>
        private void UpdateMessageToUI(LiveMessage message)
        {
            var item = CreateListViewItem(message);
            
            switch (message.Type)
            {
                case LiveMessageType.Chat:
                case LiveMessageType.EmojiChat:
                    AddItemToListView(_chatListView, item);
                    break;
                case LiveMessageType.Member:
                    AddItemToListView(_memberListView, item);
                    break;
                case LiveMessageType.Gift:
                case LiveMessageType.Social:
                case LiveMessageType.Like:
                case LiveMessageType.Fansclub:
                    AddItemToListView(_giftFollowListView, item);
                    break;
            }
        }

        /// <summary>
        /// 创建ListView项
        /// </summary>
        private ListViewItem CreateListViewItem(LiveMessage message)
        {
            var item = new ListViewItem(message.Timestamp.ToString("HH:mm:ss"));
            item.SubItems.Add(message.UserName);
            item.SubItems.Add(message.Content);
            item.Tag = message;

            // 设置颜色
            switch (message.Type)
            {
                case LiveMessageType.Gift:
                    item.ForeColor = Color.Red;
                    break;
                case LiveMessageType.Social:
                    item.ForeColor = Color.Blue;
                    break;
                case LiveMessageType.Like:
                    item.ForeColor = Color.Orange;
                    break;
                case LiveMessageType.Member:
                    item.ForeColor = Color.Green;
                    break;
                case LiveMessageType.Fansclub:
                    item.ForeColor = Color.Purple;
                    break;
                case LiveMessageType.EmojiChat:
                    item.ForeColor = Color.DarkCyan;
                    break;
            }

            return item;
        }

        /// <summary>
        /// 添加项到ListView并保持最大数量
        /// </summary>
        private void AddItemToListView(ListView listView, ListViewItem item)
        {
            const int maxItems = 1000;
            
            listView.Items.Insert(0, item);
            
            // 保持最大数量，移除旧项
            while (listView.Items.Count > maxItems)
            {
                listView.Items.RemoveAt(listView.Items.Count - 1);
            }

            // 自动滚动到顶部
            if (listView.Items.Count > 0)
            {
                listView.EnsureVisible(0);
            }
        }

        /// <summary>
        /// 更新统计数据
        /// </summary>
        private void UpdateStatistics(LiveMessage message)
        {
            _totalMessages++;
            
            switch (message.Type)
            {
                case LiveMessageType.Chat:
                case LiveMessageType.EmojiChat:
                    _chatMessages++;
                    break;
                case LiveMessageType.Gift:
                    _giftMessages++;
                    break;
                case LiveMessageType.Like:
                    _likeMessages++;
                    break;
                case LiveMessageType.Member:
                    _memberMessages++;
                    break;
                case LiveMessageType.Social:
                    _followMessages++;
                    break;
            }
        }

        /// <summary>
        /// 更新统计显示
        /// </summary>
        private void UpdateStatisticsDisplay()
        {
            var stats = $"总计: {_totalMessages} | " +
                       $"聊天: {_chatMessages} | " +
                       $"礼物: {_giftMessages} | " +
                       $"点赞: {_likeMessages} | " +
                       $"进场: {_memberMessages} | " +
                       $"关注: {_followMessages}";
            
            _statisticsLabel.Text = stats;
        }

        /// <summary>
        /// 更新状态
        /// </summary>
        public void UpdateStatus(string status)
        {
            _statusLabel.Text = $"状态: {status} | {DateTime.Now:HH:mm:ss}";
        }

        /// <summary>
        /// 清空所有列表
        /// </summary>
        public void ClearAllLists()
        {
            _chatListView.Items.Clear();
            _memberListView.Items.Clear();
            _giftFollowListView.Items.Clear();
            _watchedUsersListView.Items.Clear();
            
            ResetStatistics();
        }

        /// <summary>
        /// 重置统计数据
        /// </summary>
        private void ResetStatistics()
        {
            _totalMessages = 0;
            _chatMessages = 0;
            _giftMessages = 0;
            _likeMessages = 0;
            _memberMessages = 0;
            _followMessages = 0;
            
            UpdateStatisticsDisplay();
        }
    }
} 