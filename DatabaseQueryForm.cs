using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using DouyinDanmu.Services;

namespace DouyinDanmu
{
    public partial class DatabaseQueryForm : Form
    {
        private DatabaseService _databaseService;
        private List<QueryResult> _currentResults = new List<QueryResult>();

        public DatabaseQueryForm(DatabaseService databaseService)
        {
            InitializeComponent();
            _databaseService = databaseService;
        }

        /// <summary>
        /// 窗体加载事件
        /// </summary>
        private async void DatabaseQueryForm_Load(object sender, EventArgs e)
        {
            try
            {
                // 初始化时间范围（默认最近7天）
                dateTimePickerEnd.Value = DateTime.Now;
                dateTimePickerStart.Value = DateTime.Now.AddDays(-7);

                // 添加时间选择器事件处理
                dateTimePickerStart.ValueChanged += DateTimePicker_ValueChanged;
                dateTimePickerEnd.ValueChanged += DateTimePicker_ValueChanged;

                // 加载直播间号列表
                await LoadLiveIdsAsync();
            }
            catch (Exception ex)
            {
                MessageBox.Show($"初始化失败: {ex.Message}", "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        /// <summary>
        /// 时间选择器值改变事件处理
        /// </summary>
        private void DateTimePicker_ValueChanged(object? sender, EventArgs e)
        {
            ValidateTimeRange(sender as DateTimePicker);
        }

        /// <summary>
        /// 验证时间范围
        /// </summary>
        private void ValidateTimeRange(DateTimePicker? changedPicker)
        {
            if (dateTimePickerStart.Value > dateTimePickerEnd.Value)
            {
                // 如果起始时间大于结束时间，自动调整
                if (changedPicker == dateTimePickerStart)
                {
                    // 如果是修改了起始时间，将结束时间设置为起始时间
                    dateTimePickerEnd.Value = dateTimePickerStart.Value;
                    MessageBox.Show("起始时间不能大于结束时间，已自动调整结束时间。", "时间范围提示", 
                        MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
                else if (changedPicker == dateTimePickerEnd)
                {
                    // 如果是修改了结束时间，将起始时间设置为结束时间
                    dateTimePickerStart.Value = dateTimePickerEnd.Value;
                    MessageBox.Show("结束时间不能小于起始时间，已自动调整起始时间。", "时间范围提示", 
                        MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
            }
        }

        /// <summary>
        /// 加载直播间号列表
        /// </summary>
        private async Task LoadLiveIdsAsync()
        {
            try
            {
                var liveIds = await _databaseService.GetAllLiveIdsAsync();
                
                comboBoxLiveId.Items.Clear();
                comboBoxLiveId.Items.Add("全部");
                
                foreach (var liveId in liveIds)
                {
                    comboBoxLiveId.Items.Add(liveId);
                }
                
                if (comboBoxLiveId.Items.Count > 0)
                {
                    comboBoxLiveId.SelectedIndex = 0;
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"加载直播间列表失败: {ex.Message}", "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        /// <summary>
        /// 查询按钮点击事件
        /// </summary>
        private async void buttonQuery_Click(object sender, EventArgs e)
        {
            try
            {
                // 验证时间范围
                if (dateTimePickerStart.Value > dateTimePickerEnd.Value)
                {
                    MessageBox.Show("起始时间不能大于结束时间，请重新选择时间范围。", "时间范围错误", 
                        MessageBoxButtons.OK, MessageBoxIcon.Warning);
                    return;
                }

                // 验证时间范围是否过大（超过30天提醒）
                var timeSpan = dateTimePickerEnd.Value - dateTimePickerStart.Value;
                if (timeSpan.TotalDays > 30)
                {
                    var result = MessageBox.Show(
                        $"查询时间范围较大（{timeSpan.TotalDays:F0}天），可能需要较长时间。是否继续？", 
                        "时间范围提醒", 
                        MessageBoxButtons.YesNo, 
                        MessageBoxIcon.Question);
                    
                    if (result == DialogResult.No)
                        return;
                }

                buttonQuery.Enabled = false;
                buttonQuery.Text = "查询中...";
                listViewResults.Items.Clear();
                
                var filter = BuildQueryFilter();
                _currentResults = await _databaseService.QueryMessagesAsync(filter);
                
                DisplayResults(_currentResults);
                
                labelResultCount.Text = $"查询结果: {_currentResults.Count}条";
            }
            catch (Exception ex)
            {
                MessageBox.Show($"查询失败: {ex.Message}", "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            finally
            {
                buttonQuery.Enabled = true;
                buttonQuery.Text = "查询";
            }
        }

        /// <summary>
        /// 重置按钮点击事件
        /// </summary>
        private void buttonReset_Click(object sender, EventArgs e)
        {
            // 重置所有筛选条件
            comboBoxLiveId.SelectedIndex = 0;
            textBoxUserId.Clear();
            textBoxUserName.Clear();
            
            checkBoxChat.Checked = true;
            checkBoxMember.Checked = true;
            checkBoxGift.Checked = true;
            checkBoxLike.Checked = true;
            checkBoxSocial.Checked = true;
            
            dateTimePickerEnd.Value = DateTime.Now;
            dateTimePickerStart.Value = DateTime.Now.AddDays(-7);
            
            listViewResults.Items.Clear();
            labelResultCount.Text = "查询结果: 0条";
            _currentResults.Clear();
        }

        /// <summary>
        /// 导出按钮点击事件
        /// </summary>
        private void buttonExport_Click(object sender, EventArgs e)
        {
            if (_currentResults.Count == 0)
            {
                MessageBox.Show("没有可导出的数据", "提示", MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }

            try
            {
                using var dialog = new SaveFileDialog();
                dialog.Filter = "CSV文件 (*.csv)|*.csv|文本文件 (*.txt)|*.txt";
                dialog.DefaultExt = "csv";
                dialog.FileName = $"抖音直播消息查询结果_{DateTime.Now:yyyyMMdd_HHmmss}";

                if (dialog.ShowDialog() == DialogResult.OK)
                {
                    ExportResults(dialog.FileName);
                    MessageBox.Show($"导出成功！\n文件位置: {dialog.FileName}", "导出完成", 
                        MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"导出失败: {ex.Message}", "错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        /// <summary>
        /// 构建查询筛选条件
        /// </summary>
        private QueryFilter BuildQueryFilter()
        {
            var filter = new QueryFilter
            {
                StartTime = dateTimePickerStart.Value,
                EndTime = dateTimePickerEnd.Value.AddDays(1).AddSeconds(-1), // 包含结束日期的整天
                UserId = string.IsNullOrWhiteSpace(textBoxUserId.Text) ? null : textBoxUserId.Text.Trim(),
                UserName = string.IsNullOrWhiteSpace(textBoxUserName.Text) ? null : textBoxUserName.Text.Trim(),
                MessageTypes = new List<string>()
            };

            // 设置直播间ID
            if (comboBoxLiveId.SelectedItem?.ToString() != "全部")
            {
                filter.LiveId = comboBoxLiveId.SelectedItem?.ToString();
            }

            // 设置消息类型
            if (checkBoxChat.Checked) filter.MessageTypes.Add("Chat");
            if (checkBoxMember.Checked) filter.MessageTypes.Add("Member");
            if (checkBoxGift.Checked) filter.MessageTypes.Add("Gift");
            if (checkBoxLike.Checked) filter.MessageTypes.Add("Like");
            if (checkBoxSocial.Checked) filter.MessageTypes.Add("Social");

            return filter;
        }

        /// <summary>
        /// 显示查询结果
        /// </summary>
        private void DisplayResults(List<QueryResult> results)
        {
            listViewResults.Items.Clear();
            
            foreach (var result in results)
            {
                var item = new ListViewItem(result.Id.ToString());
                item.SubItems.Add(result.LiveId);
                item.SubItems.Add(result.Timestamp.ToString("yyyy-MM-dd HH:mm:ss"));
                item.SubItems.Add(GetMessageTypeText(result.MessageType));
                item.SubItems.Add(result.UserId ?? "");
                item.SubItems.Add(result.UserName ?? "");
                item.SubItems.Add(result.FansClubLevel > 0 ? result.FansClubLevel.ToString() : "-");
                item.SubItems.Add(result.PayGradeLevel > 0 ? result.PayGradeLevel.ToString() : "-");
                item.SubItems.Add(result.Content ?? "");
                
                listViewResults.Items.Add(item);
            }
        }

        /// <summary>
        /// 获取消息类型显示文本
        /// </summary>
        private string GetMessageTypeText(string messageType)
        {
            return messageType switch
            {
                "Chat" => "聊天",
                "Member" => "进场",
                "Gift" => "礼物",
                "Like" => "点赞",
                "Social" => "关注",
                _ => messageType
            };
        }

        /// <summary>
        /// 导出查询结果
        /// </summary>
        private void ExportResults(string filePath)
        {
            var isCSV = filePath.EndsWith(".csv", StringComparison.OrdinalIgnoreCase);
            var separator = isCSV ? "," : "\t";
            var lines = new List<string>();

            // 添加表头
            var headers = new[] { "ID", "直播间号", "时间", "类型", "用户ID", "用户名", "粉丝等级", "财富等级", "内容" };
            lines.Add(string.Join(separator, headers));

            // 添加数据行
            foreach (var result in _currentResults)
            {
                var values = new[]
                {
                    result.Id.ToString(),
                    result.LiveId,
                    result.Timestamp.ToString("yyyy-MM-dd HH:mm:ss"),
                    GetMessageTypeText(result.MessageType),
                    result.UserId ?? "",
                    result.UserName ?? "",
                    result.FansClubLevel > 0 ? result.FansClubLevel.ToString() : "-",
                    result.PayGradeLevel > 0 ? result.PayGradeLevel.ToString() : "-",
                    result.Content ?? ""
                };

                // CSV格式需要处理包含逗号的字段
                if (isCSV)
                {
                    for (int i = 0; i < values.Length; i++)
                    {
                        if (values[i].Contains(",") || values[i].Contains("\"") || values[i].Contains("\n"))
                        {
                            values[i] = "\"" + values[i].Replace("\"", "\"\"") + "\"";
                        }
                    }
                }

                lines.Add(string.Join(separator, values));
            }

            // 添加统计信息
            lines.Add("");
            lines.Add($"导出时间: {DateTime.Now:yyyy-MM-dd HH:mm:ss}");
            lines.Add($"总记录数: {_currentResults.Count}");

            File.WriteAllLines(filePath, lines, Encoding.UTF8);
        }
    }
} 