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

namespace DouyinDanmu
{
    public partial class SettingsForm : Form
    {
        public List<string> WatchedUserIds { get; private set; }

        public SettingsForm(List<string> currentUserIds)
        {
            InitializeComponent();
            WatchedUserIds = new List<string>(currentUserIds);
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
                listBoxUserIds.Items.Add(userId);
            }
        }

        private void buttonAdd_Click(object sender, EventArgs e)
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

            WatchedUserIds.Add(userId);
            listBoxUserIds.Items.Add(userId);
            textBoxUserId.Clear();
        }

        private void buttonRemove_Click(object sender, EventArgs e)
        {
            if (listBoxUserIds.SelectedItem == null)
            {
                MessageBox.Show("请选择要删除的用户ID", "提示", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            var selectedUserId = listBoxUserIds.SelectedItem.ToString();
            WatchedUserIds.Remove(selectedUserId);
            listBoxUserIds.Items.Remove(selectedUserId);
        }

        private void buttonClear_Click(object sender, EventArgs e)
        {
            if (MessageBox.Show("确定要清空所有用户ID吗？", "确认", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes)
            {
                WatchedUserIds.Clear();
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
    }
} 