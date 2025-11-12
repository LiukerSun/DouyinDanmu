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
        public Dictionary<string, UserInfo> UserInfos { get; private set; }
        private readonly Dictionary<string, UserInfo> _userInfoCache;
        private readonly HttpClient _httpClient;
        private readonly DatabaseService? _databaseService;

        public SettingsForm(List<string> currentUserIds, Dictionary<string, UserInfo>? userInfos = null, DatabaseService? databaseService = null)
        {
            InitializeComponent();
            WatchedUserIds = [.. currentUserIds];
            UserInfos = userInfos != null ? new Dictionary<string, UserInfo>(userInfos) : [];
            _userInfoCache = new Dictionary<string, UserInfo>(UserInfos);
            _httpClient = new HttpClient();
            _databaseService = databaseService;

            LoadUserIds();

            // 添加双击事件处理
            listBoxUserIds.DoubleClick += ListBoxUserIds_DoubleClick;

            // 更新描述标签，显示设置文件位置
            var settingsPath = SettingsManager.GetSettingsFilePath();
            labelDescription.Text = $"打开配置文件文件夹";
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
            if (_userInfoCache.TryGetValue(userId, out UserInfo? value))
            {
                return value;
            }

            // 首先检查是否有已保存的用户信息
            UserInfo userInfo;
            if (UserInfos.TryGetValue(userId, out UserInfo? cachedInfo))
            {
                // 使用已保存的用户信息
                userInfo = new UserInfo(userId, cachedInfo.Nickname);
            }
            else
            {
                // 创建新的用户信息
                userInfo = new UserInfo(userId);
            }

            _userInfoCache[userId] = userInfo;

            // 异步获取用户昵称（如果当前没有昵称）
            if (string.IsNullOrEmpty(userInfo.Nickname))
            {
                _ = Task.Run(async () => await FetchUserNicknameAsync(userId));
            }

            return userInfo;
        }

        private async Task FetchUserNicknameAsync(string userId)
        {
            try
            {
                string? nickname = null;

                // 首先尝试从数据库获取用户昵称
                if (_databaseService != null)
                {
                    nickname = await _databaseService.GetUserNicknameAsync(userId);
                }

                // 更新UI需要在主线程执行
                this.Invoke(new Action(() =>
                {
                    if (_userInfoCache.TryGetValue(userId, out UserInfo? value) && !string.IsNullOrEmpty(nickname))
                    {
                        value.Nickname = nickname;
                        value.LastUpdated = DateTime.Now;
                        RefreshUserList();
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
                var userInfo = _userInfoCache.TryGetValue(userId, out UserInfo? value) ? value : new UserInfo(userId);
                listBoxUserIds.Items.Add(userInfo);
            }

            if (selectedIndex >= 0 && selectedIndex < listBoxUserIds.Items.Count)
            {
                listBoxUserIds.SelectedIndex = selectedIndex;
            }
        }

        private async void ButtonAdd_Click(object sender, EventArgs e)
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
            buttonAdd.Text = "查询中...";

            try
            {
                string? nickname = null;

                // 首先尝试从数据库获取用户昵称
                if (_databaseService != null)
                {
                    nickname = await _databaseService.GetUserNicknameAsync(userId);
                }

                // 如果数据库中没有找到昵称，询问用户是否要手动输入
                if (string.IsNullOrEmpty(nickname))
                {
                    var result = MessageBox.Show(
                        $"数据库中未找到用户ID '{userId}' 的昵称信息。\n\n是否要手动输入昵称？\n\n点击'是'手动输入昵称\n点击'否'仅使用用户ID",
                        "未找到昵称",
                        MessageBoxButtons.YesNoCancel,
                        MessageBoxIcon.Question);

                    if (result == DialogResult.Cancel)
                    {
                        return; // 用户取消操作
                    }
                    else if (result == DialogResult.Yes)
                    {
                        // 弹出输入框让用户手动输入昵称
                        nickname = ShowNicknameInputDialog(userId);
                        if (nickname == null) // 用户取消了输入
                        {
                            return;
                        }
                    }
                    // 如果选择'否'，nickname保持为null，只使用用户ID
                }

                // 添加用户
                WatchedUserIds.Add(userId);

                // 创建用户信息
                var userInfo = new UserInfo(userId, nickname ?? "");
                _userInfoCache[userId] = userInfo;

                listBoxUserIds.Items.Add(userInfo);
                textBoxUserId.Clear();
                textBoxNickname.Clear();

                // 显示结果
                if (!string.IsNullOrEmpty(nickname))
                {
                    UpdateStatus($"已添加用户: {nickname} (ID: {userId})");
                }
                else
                {
                    UpdateStatus($"已添加用户: {userId}");
                }
            }
            finally
            {
                buttonAdd.Enabled = true;
                buttonAdd.Text = "添加";
            }
        }

        /// <summary>
        /// 显示昵称输入对话框
        /// </summary>
        private static string? ShowNicknameInputDialog(string userId)
        {
            using var inputForm = new Form();
            inputForm.Text = "输入用户昵称";
            inputForm.Size = new Size(400, 150);
            inputForm.StartPosition = FormStartPosition.CenterParent;
            inputForm.FormBorderStyle = FormBorderStyle.FixedDialog;
            inputForm.MaximizeBox = false;
            inputForm.MinimizeBox = false;

            var label = new Label()
            {
                Text = $"用户ID: {userId}",
                Location = new Point(15, 15),
                Size = new Size(350, 20)
            };

            var textBox = new TextBox()
            {
                Location = new Point(15, 45),
                Size = new Size(350, 23),
                PlaceholderText = "请输入用户昵称"
            };

            var buttonOK = new Button()
            {
                Text = "确定",
                Location = new Point(220, 80),
                Size = new Size(75, 25),
                DialogResult = DialogResult.OK
            };

            var buttonCancel = new Button()
            {
                Text = "取消",
                Location = new Point(305, 80),
                Size = new Size(75, 25),
                DialogResult = DialogResult.Cancel
            };

            inputForm.Controls.AddRange([label, textBox, buttonOK, buttonCancel]);
            inputForm.AcceptButton = buttonOK;
            inputForm.CancelButton = buttonCancel;

            textBox.Focus();

            if (inputForm.ShowDialog() == DialogResult.OK)
            {
                return textBox.Text.Trim();
            }

            return null; // 用户取消了输入
        }

        /// <summary>
        /// 更新状态信息（如果父窗体有UpdateStatus方法）
        /// </summary>
        private static void UpdateStatus(string message)
        {
            // 这里可以添加状态更新逻辑，比如在窗体底部显示状态
            // 暂时使用Debug输出
            Debug.WriteLine($"[SettingsForm] {message}");
        }

        private void ButtonRemove_Click(object sender, EventArgs e)
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

        private void ButtonClear_Click(object sender, EventArgs e)
        {
            if (MessageBox.Show("确定要清空所有用户吗？", "确认", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes)
            {
                WatchedUserIds.Clear();
                _userInfoCache.Clear();
                listBoxUserIds.Items.Clear();
            }
        }

        private void ButtonOK_Click(object sender, EventArgs e)
        {
            // 保存用户信息到UserInfos
            UserInfos.Clear();
            foreach (var kvp in _userInfoCache)
            {
                UserInfos[kvp.Key] = kvp.Value;
            }

            this.DialogResult = DialogResult.OK;
            this.Close();
        }

        private void ButtonCancel_Click(object sender, EventArgs e)
        {
            this.DialogResult = DialogResult.Cancel;
            this.Close();
        }

        private void TextBoxUserId_KeyPress(object sender, KeyPressEventArgs e)
        {
            if (e.KeyChar == (char)Keys.Enter)
            {
                ButtonAdd_Click(sender, e);
                e.Handled = true;
            }
        }

        private void TextBoxNickname_KeyPress(object sender, KeyPressEventArgs e)
        {
            if (e.KeyChar == (char)Keys.Enter)
            {
                ButtonAdd_Click(sender, e);
                e.Handled = true;
            }
        }

        /// <summary>
        /// 双击列表项编辑昵称
        /// </summary>
        private void ListBoxUserIds_DoubleClick(object? sender, EventArgs e)
        {
            if (listBoxUserIds.SelectedItem == null)
                return;

            var selectedUserInfo = (UserInfo)listBoxUserIds.SelectedItem;
            var userId = selectedUserInfo.UserId;
            var currentNickname = selectedUserInfo.Nickname;

            // 创建简单的输入对话框
            using var inputForm = new Form();
            inputForm.Text = "编辑用户昵称";
            inputForm.Size = new Size(400, 150);
            inputForm.StartPosition = FormStartPosition.CenterParent;
            inputForm.FormBorderStyle = FormBorderStyle.FixedDialog;
            inputForm.MaximizeBox = false;
            inputForm.MinimizeBox = false;

            var label = new Label()
            {
                Text = $"用户ID: {userId}",
                Location = new Point(15, 15),
                Size = new Size(350, 20)
            };

            var textBox = new TextBox()
            {
                Text = currentNickname,
                Location = new Point(15, 45),
                Size = new Size(350, 23),
                PlaceholderText = "请输入用户昵称（可为空）"
            };

            var buttonOK = new Button()
            {
                Text = "确定",
                Location = new Point(220, 80),
                Size = new Size(75, 25),
                DialogResult = DialogResult.OK
            };

            var buttonCancel = new Button()
            {
                Text = "取消",
                Location = new Point(305, 80),
                Size = new Size(75, 25),
                DialogResult = DialogResult.Cancel
            };

            inputForm.Controls.AddRange([label, textBox, buttonOK, buttonCancel]);
            inputForm.AcceptButton = buttonOK;
            inputForm.CancelButton = buttonCancel;

            // 选中文本框内容
            textBox.SelectAll();
            textBox.Focus();

            if (inputForm.ShowDialog() == DialogResult.OK)
            {
                var newNickname = textBox.Text.Trim();

                // 更新用户信息
                selectedUserInfo.Nickname = newNickname;
                selectedUserInfo.LastUpdated = DateTime.Now;

                // 刷新列表显示
                RefreshUserList();
            }
        }

        /// <summary>
        /// 描述标签点击事件 - 显示设置文件位置
        /// </summary>
        private void LabelDescription_Click(object sender, EventArgs e)
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
                components?.Dispose();
            }
            base.Dispose(disposing);
        }
    }
}