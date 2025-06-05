namespace DouyinDanmu
{
    partial class WebSocketSettingsForm
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.groupBoxWebSocket = new System.Windows.Forms.GroupBox();
            this.buttonStartStop = new System.Windows.Forms.Button();
            this.buttonTest = new System.Windows.Forms.Button();
            this.checkBoxAutoStart = new System.Windows.Forms.CheckBox();
            this.numericUpDownPort = new System.Windows.Forms.NumericUpDown();
            this.labelPort = new System.Windows.Forms.Label();
            this.checkBoxEnableWebSocket = new System.Windows.Forms.CheckBox();
            this.labelStatus = new System.Windows.Forms.Label();
            this.labelInfo = new System.Windows.Forms.Label();
            this.buttonOK = new System.Windows.Forms.Button();
            this.buttonCancel = new System.Windows.Forms.Button();
            this.groupBoxWebSocket.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.numericUpDownPort)).BeginInit();
            this.SuspendLayout();
            // 
            // groupBoxWebSocket
            // 
            this.groupBoxWebSocket.Controls.Add(this.buttonStartStop);
            this.groupBoxWebSocket.Controls.Add(this.buttonTest);
            this.groupBoxWebSocket.Controls.Add(this.checkBoxAutoStart);
            this.groupBoxWebSocket.Controls.Add(this.numericUpDownPort);
            this.groupBoxWebSocket.Controls.Add(this.labelPort);
            this.groupBoxWebSocket.Controls.Add(this.checkBoxEnableWebSocket);
            this.groupBoxWebSocket.Location = new System.Drawing.Point(12, 12);
            this.groupBoxWebSocket.Name = "groupBoxWebSocket";
            this.groupBoxWebSocket.Size = new System.Drawing.Size(460, 120);
            this.groupBoxWebSocket.TabIndex = 0;
            this.groupBoxWebSocket.TabStop = false;
            this.groupBoxWebSocket.Text = "WebSocket设置";
            // 
            // buttonStartStop
            // 
            this.buttonStartStop.Location = new System.Drawing.Point(270, 50);
            this.buttonStartStop.Name = "buttonStartStop";
            this.buttonStartStop.Size = new System.Drawing.Size(75, 23);
            this.buttonStartStop.TabIndex = 5;
            this.buttonStartStop.Text = "启动服务";
            this.buttonStartStop.UseVisualStyleBackColor = true;
            this.buttonStartStop.Click += new System.EventHandler(this.buttonStartStop_Click);
            // 
            // buttonTest
            // 
            this.buttonTest.Location = new System.Drawing.Point(350, 50);
            this.buttonTest.Name = "buttonTest";
            this.buttonTest.Size = new System.Drawing.Size(75, 23);
            this.buttonTest.TabIndex = 4;
            this.buttonTest.Text = "测试连接";
            this.buttonTest.UseVisualStyleBackColor = true;
            this.buttonTest.Click += new System.EventHandler(this.buttonTest_Click);
            // 
            // checkBoxAutoStart
            // 
            this.checkBoxAutoStart.AutoSize = true;
            this.checkBoxAutoStart.Location = new System.Drawing.Point(20, 85);
            this.checkBoxAutoStart.Name = "checkBoxAutoStart";
            this.checkBoxAutoStart.Size = new System.Drawing.Size(192, 19);
            this.checkBoxAutoStart.TabIndex = 3;
            this.checkBoxAutoStart.Text = "连接直播间时自动启动WebSocket";
            this.checkBoxAutoStart.UseVisualStyleBackColor = true;
            this.checkBoxAutoStart.CheckedChanged += new System.EventHandler(this.checkBoxAutoStart_CheckedChanged);
            // 
            // numericUpDownPort
            // 
            this.numericUpDownPort.Location = new System.Drawing.Point(70, 52);
            this.numericUpDownPort.Maximum = new decimal(new int[] {
            65535,
            0,
            0,
            0});
            this.numericUpDownPort.Minimum = new decimal(new int[] {
            1024,
            0,
            0,
            0});
            this.numericUpDownPort.Name = "numericUpDownPort";
            this.numericUpDownPort.Size = new System.Drawing.Size(80, 23);
            this.numericUpDownPort.TabIndex = 2;
            this.numericUpDownPort.Value = new decimal(new int[] {
            8080,
            0,
            0,
            0});
            this.numericUpDownPort.ValueChanged += new System.EventHandler(this.numericUpDownPort_ValueChanged);
            // 
            // labelPort
            // 
            this.labelPort.AutoSize = true;
            this.labelPort.Location = new System.Drawing.Point(20, 54);
            this.labelPort.Name = "labelPort";
            this.labelPort.Size = new System.Drawing.Size(44, 15);
            this.labelPort.TabIndex = 1;
            this.labelPort.Text = "端口：";
            // 
            // checkBoxEnableWebSocket
            // 
            this.checkBoxEnableWebSocket.AutoSize = true;
            this.checkBoxEnableWebSocket.Location = new System.Drawing.Point(20, 25);
            this.checkBoxEnableWebSocket.Name = "checkBoxEnableWebSocket";
            this.checkBoxEnableWebSocket.Size = new System.Drawing.Size(122, 19);
            this.checkBoxEnableWebSocket.TabIndex = 0;
            this.checkBoxEnableWebSocket.Text = "启用WebSocket服务";
            this.checkBoxEnableWebSocket.UseVisualStyleBackColor = true;
            this.checkBoxEnableWebSocket.CheckedChanged += new System.EventHandler(this.checkBoxEnableWebSocket_CheckedChanged);
            // 
            // labelStatus
            // 
            this.labelStatus.AutoSize = true;
            this.labelStatus.Font = new System.Drawing.Font("Microsoft YaHei UI", 9F, System.Drawing.FontStyle.Bold);
            this.labelStatus.Location = new System.Drawing.Point(12, 145);
            this.labelStatus.Name = "labelStatus";
            this.labelStatus.Size = new System.Drawing.Size(92, 17);
            this.labelStatus.TabIndex = 4;
            this.labelStatus.Text = "服务状态：未知";
            // 
            // labelInfo
            // 
            this.labelInfo.Location = new System.Drawing.Point(12, 175);
            this.labelInfo.Name = "labelInfo";
            this.labelInfo.Size = new System.Drawing.Size(460, 60);
            this.labelInfo.TabIndex = 1;
            this.labelInfo.Text = "WebSocket服务允许其他应用程序订阅直播间的实时消息。\r\n连接地址格式：ws://localhost:端口/?type=chat,gift\r\n支持的消息类型：chat(弹幕)、gift(礼物)、like(点赞)、member(进场)、social(关注)、all(所有)";
            // 
            // buttonOK
            // 
            this.buttonOK.Location = new System.Drawing.Point(316, 250);
            this.buttonOK.Name = "buttonOK";
            this.buttonOK.Size = new System.Drawing.Size(75, 30);
            this.buttonOK.TabIndex = 2;
            this.buttonOK.Text = "确定";
            this.buttonOK.UseVisualStyleBackColor = true;
            this.buttonOK.Click += new System.EventHandler(this.buttonOK_Click);
            // 
            // buttonCancel
            // 
            this.buttonCancel.Location = new System.Drawing.Point(397, 250);
            this.buttonCancel.Name = "buttonCancel";
            this.buttonCancel.Size = new System.Drawing.Size(75, 30);
            this.buttonCancel.TabIndex = 3;
            this.buttonCancel.Text = "取消";
            this.buttonCancel.UseVisualStyleBackColor = true;
            this.buttonCancel.Click += new System.EventHandler(this.buttonCancel_Click);
            // 
            // WebSocketSettingsForm
            // 
            this.AcceptButton = this.buttonOK;
            this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 15F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.CancelButton = this.buttonCancel;
            this.ClientSize = new System.Drawing.Size(484, 292);
            this.Controls.Add(this.labelStatus);
            this.Controls.Add(this.buttonCancel);
            this.Controls.Add(this.buttonOK);
            this.Controls.Add(this.labelInfo);
            this.Controls.Add(this.groupBoxWebSocket);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "WebSocketSettingsForm";
            this.ShowInTaskbar = false;
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "WebSocket设置";
            this.groupBoxWebSocket.ResumeLayout(false);
            this.groupBoxWebSocket.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.numericUpDownPort)).EndInit();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.GroupBox groupBoxWebSocket;
        private System.Windows.Forms.CheckBox checkBoxEnableWebSocket;
        private System.Windows.Forms.Label labelPort;
        private System.Windows.Forms.NumericUpDown numericUpDownPort;
        private System.Windows.Forms.CheckBox checkBoxAutoStart;
        private System.Windows.Forms.Button buttonTest;
        private System.Windows.Forms.Label labelInfo;
        private System.Windows.Forms.Button buttonOK;
        private System.Windows.Forms.Button buttonCancel;
        private System.Windows.Forms.Button buttonStartStop;
        private System.Windows.Forms.Label labelStatus;
    }
} 