using System;
using System.Windows.Forms;
using DouyinDanmu.Models;
using DouyinDanmu.Services;

namespace DouyinDanmu
{
    public partial class WebSocketSettingsForm : Form
    {
        public AppSettings Settings { get; private set; }
        private readonly WebSocketService? _webSocketService;

        public WebSocketSettingsForm(
            AppSettings currentSettings,
            WebSocketService? webSocketService = null
        )
        {
            InitializeComponent();
            _webSocketService = webSocketService;
            Settings = new AppSettings
            {
                WebSocketEnabled = currentSettings.WebSocketEnabled,
                WebSocketPort = currentSettings.WebSocketPort,
                AutoStartWebSocket = currentSettings.AutoStartWebSocket,
            };

            LoadSettings();
            UpdateServiceStatus();
        }

        private void LoadSettings()
        {
            checkBoxEnableWebSocket.Checked = Settings.WebSocketEnabled;
            numericUpDownPort.Value = Settings.WebSocketPort;
            checkBoxAutoStart.Checked = Settings.AutoStartWebSocket;

            // 根据是否启用WebSocket来设置控件状态
            UpdateControlsState();
        }

        private void UpdateControlsState()
        {
            numericUpDownPort.Enabled = checkBoxEnableWebSocket.Checked;
            checkBoxAutoStart.Enabled = checkBoxEnableWebSocket.Checked;
            buttonStartStop.Enabled = checkBoxEnableWebSocket.Checked;
        }

        private void UpdateServiceStatus()
        {
            if (_webSocketService != null)
            {
                var isRunning = _webSocketService.IsRunning;
                var clientCount = _webSocketService.ConnectedClientsCount;
                var port = _webSocketService.Port;

                if (isRunning)
                {
                    labelStatus.Text = $"服务状态：运行中 (端口: {port}, 客户端: {clientCount})";
                    labelStatus.ForeColor = System.Drawing.Color.Green;
                    buttonStartStop.Text = "停止服务";
                }
                else
                {
                    labelStatus.Text = "服务状态：已停止";
                    labelStatus.ForeColor = System.Drawing.Color.Red;
                    buttonStartStop.Text = "启动服务";
                }
            }
            else
            {
                labelStatus.Text = "服务状态：不可用";
                labelStatus.ForeColor = System.Drawing.Color.Gray;
                buttonStartStop.Enabled = false;
            }
        }

        private void CheckBoxEnableWebSocket_CheckedChanged(object sender, EventArgs e)
        {
            Settings.WebSocketEnabled = checkBoxEnableWebSocket.Checked;
            UpdateControlsState();
        }

        private void NumericUpDownPort_ValueChanged(object sender, EventArgs e)
        {
            Settings.WebSocketPort = (int)numericUpDownPort.Value;
        }

        private void CheckBoxAutoStart_CheckedChanged(object sender, EventArgs e)
        {
            Settings.AutoStartWebSocket = checkBoxAutoStart.Checked;
        }

        private async void ButtonStartStop_Click(object sender, EventArgs e)
        {
            if (_webSocketService == null)
                return;

            try
            {
                if (_webSocketService.IsRunning)
                {
                    // 停止服务
                    await _webSocketService.StopAsync();
                    MessageBox.Show(
                        "WebSocket服务已停止",
                        "提示",
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Information
                    );
                }
                else
                {
                    // 启动服务
                    var port = (int)numericUpDownPort.Value;
                    await _webSocketService.StartAsync(port);
                    MessageBox.Show(
                        $"WebSocket服务已在端口 {port} 启动",
                        "提示",
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Information
                    );
                }

                UpdateServiceStatus();
            }
            catch (Exception ex)
            {
                MessageBox.Show(
                    $"操作失败: {ex.Message}",
                    "错误",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error
                );
            }
        }

        private void ButtonOK_Click(object sender, EventArgs e)
        {
            // 验证端口号
            if (
                Settings.WebSocketEnabled
                && (Settings.WebSocketPort < 1024 || Settings.WebSocketPort > 65535)
            )
            {
                MessageBox.Show(
                    "端口号必须在1024-65535范围内！",
                    "错误",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error
                );
                return;
            }

            DialogResult = DialogResult.OK;
            Close();
        }

        private void ButtonCancel_Click(object sender, EventArgs e)
        {
            DialogResult = DialogResult.Cancel;
            Close();
        }

        private void ButtonTest_Click(object sender, EventArgs e)
        {
            if (!Settings.WebSocketEnabled)
            {
                MessageBox.Show(
                    "请先启用WebSocket服务！",
                    "提示",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Information
                );
                return;
            }

            try
            {
                var port =
                    _webSocketService?.IsRunning == true
                        ? _webSocketService.Port
                        : Settings.WebSocketPort;
                var url = $"http://localhost:{port}";
                System.Diagnostics.Process.Start(
                    new System.Diagnostics.ProcessStartInfo
                    {
                        FileName = url,
                        UseShellExecute = true,
                    }
                );
                MessageBox.Show(
                    $"已打开WebSocket服务页面：{url}",
                    "提示",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Information
                );
            }
            catch (Exception ex)
            {
                MessageBox.Show(
                    $"无法打开页面：{ex.Message}",
                    "错误",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error
                );
            }
        }
    }
}
