using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Diagnostics;
using System.IO;
using DouyinDanmu.Services;
using DouyinDanmu.Models;
using System.Net.Http;
using Newtonsoft.Json.Linq;

namespace DouyinDanmu
{
    public partial class SettingsForm : Form
    {
        public List<string> WatchedUserIds { get; private set; }
        private Dictionary<string, UserInfo> _userInfoCache;
        private readonly HttpClient _httpClient;

        public SettingsForm(List<string> currentUserIds)
        {
            InitializeComponent();
            WatchedUserIds = new List<string>(currentUserIds);
            _userInfoCache = new Dictionary<string, UserInfo>();
            _httpClient = new HttpClient();
            
            LoadUserIds();
            
            // 更新描述标签，显示设置文件位置
            var settingsPath = SettingsManager.GetSettingsFilePath();
            labelDescription.Text = $"提示：设置自动保存到 {Path.GetFileName(settingsPath)} (点击查看路径)";
        }

        private void LoadUserIds()
        {
            listBoxUserIds.Items.Clear();
            foreach (var userId in WatchedUserIds)
            {
                var userInfo = GetUserInfo(userId);
                listBoxUserIds.Items.Add(userInfo);
            }
        }

        private UserInfo GetUserInfo(string userId)
        {
            if (_userInfoCache.ContainsKey(userId))
            {
                return _userInfoCache[userId];
            }

            var userInfo = new UserInfo(userId);
            _userInfoCache[userId] = userInfo;
            
            // 异步获取用户昵称
            _ = Task.Run(async () => await FetchUserNicknameAsync(userId));
            
            return userInfo;
        }

        private async Task FetchUserNicknameAsync(string userId)
        {
            try
            {
                // 这里可以调用抖音API获取用户信息
                // 由于抖音API需要认证，这里先使用简化的方式
                // 实际项目中可以集成真实的API调用
                
                await Task.Delay(100); // 模拟网络延迟
                
                // 更新UI需要在主线程执行
                this.Invoke(new Action(() =>
                {
                    if (_userInfoCache.ContainsKey(userId))
                    {
                        // 这里可以设置真实的昵称
                        // _userInfoCache[userId].Nickname = "获取到的昵称";
                        // RefreshUserList();
                    }
                }));
            }
            catch (Exception ex)
            {
                // 忽略获取昵称失败的错误
                Debug.WriteLine($"获取用户昵称失败: {ex.Message}");
            }
        }

        private void RefreshUserList()
        {
            var selectedIndex = listBoxUserIds.SelectedIndex;
            listBoxUserIds.Items.Clear();
            
            foreach (var userId in WatchedUserIds)
            {
                var userInfo = _userInfoCache.ContainsKey(userId) ? _userInfoCache[userId] : new UserInfo(userId);
                listBoxUserIds.Items.Add(userInfo);
            }
            
            if (selectedIndex >= 0 && selectedIndex < listBoxUserIds.Items.Count)
            {
                listBoxUserIds.SelectedIndex = selectedIndex;
            }
        }

        private async void buttonAdd_Click(object sender, EventArgs e)
        {
            var userId = textBoxUserId.Text.Trim();
            if (string.IsNullOrEmpty(userId))
            {
                MessageBox.Show("请输入用户ID", "提示", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            if (WatchedUserIds.Contains(userId))
            {
                MessageBox.Show("该用户ID已存在", "提示", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            // 禁用添加按钮，显示加载状态
            buttonAdd.Enabled = false;
            buttonAdd.Text = "添加中...";

            try
            {
                WatchedUserIds.Add(userId);
                var userInfo = GetUserInfo(userId);
                listBoxUserIds.Items.Add(userInfo);
                textBoxUserId.Clear();
                
                // 尝试获取用户昵称
                await FetchUserNicknameAsync(userId);
            }
            finally
            {
                buttonAdd.Enabled = true;
                buttonAdd.Text = "添加";
            }
        }

        private void buttonRemove_Click(object sender, EventArgs e)
        {
            if (listBoxUserIds.SelectedItem == null)
            {
                MessageBox.Show("请选择要删除的用户", "提示", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            var selectedUserInfo = (UserInfo)listBoxUserIds.SelectedItem;
            var userId = selectedUserInfo.UserId;
            
            WatchedUserIds.Remove(userId);
            _userInfoCache.Remove(userId);
            listBoxUserIds.Items.Remove(selectedUserInfo);
        }

        private void buttonClear_Click(object sender, EventArgs e)
        {
            if (MessageBox.Show("确定要清空所有用户吗？", "确认", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes)
            {
                WatchedUserIds.Clear();
                _userInfoCache.Clear();
                listBoxUserIds.Items.Clear();
            }
        }

        private void buttonOK_Click(object sender, EventArgs e)
        {
            this.DialogResult = DialogResult.OK;
            this.Close();
        }

        private void buttonCancel_Click(object sender, EventArgs e)
        {
            this.DialogResult = DialogResult.Cancel;
            this.Close();
        }

        private void textBoxUserId_KeyPress(object sender, KeyPressEventArgs e)
        {
            if (e.KeyChar == (char)Keys.Enter)
            {
                buttonAdd_Click(sender, e);
                e.Handled = true;
            }
        }

        /// <summary>
        /// 描述标签点击事件 - 显示设置文件位置
        /// </summary>
        private void labelDescription_Click(object sender, EventArgs e)
        {
            try
            {
                var settingsPath = SettingsManager.GetSettingsFilePath();
                var directory = Path.GetDirectoryName(settingsPath);
                
                var message = $"设置文件位置：\n{settingsPath}\n\n是否要打开设置文件所在的文件夹？";
                var result = MessageBox.Show(message, "设置文件位置", MessageBoxButtons.YesNo, MessageBoxIcon.Information);
                
                if (result == DialogResult.Yes && !string.IsNullOrEmpty(directory))
                {
                    Process.Start("explorer.exe", directory);
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"无法打开文件夹: {ex.Message}", "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                _httpClient?.Dispose();
                if (components != null)
                {
                    components.Dispose();
                }
            }
            base.Dispose(disposing);
        }
    }
} 