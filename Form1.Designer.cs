namespace DouyinDanmu;

partial class Form1
{
    /// <summary>
    ///  Required designer variable.
    /// </summary>
    private System.ComponentModel.IContainer components = null;

    /// <summary>
    ///  Clean up any resources being used.
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
    ///  Required method for Designer support - do not modify
    ///  the contents of this method with the code editor.
    /// </summary>
    private void InitializeComponent()
    {
        this.groupBoxConnection = new System.Windows.Forms.GroupBox();
        this.buttonDatabase = new System.Windows.Forms.Button();
        this.buttonSettings = new System.Windows.Forms.Button();
        this.buttonWebSocket = new System.Windows.Forms.Button();
        this.buttonConnect = new System.Windows.Forms.Button();
        this.labelLiveId = new System.Windows.Forms.Label();
        this.textBoxLiveId = new System.Windows.Forms.TextBox();
        this.groupBoxMessages = new System.Windows.Forms.GroupBox();
        this.buttonSaveLog = new System.Windows.Forms.Button();
        this.buttonClear = new System.Windows.Forms.Button();
        this.buttonShowUnknownTypes = new System.Windows.Forms.Button();
        this.checkBoxAutoScroll = new System.Windows.Forms.CheckBox();
        this.groupBoxChat = new System.Windows.Forms.GroupBox();
        this.listViewChat = new System.Windows.Forms.ListView();
        this.groupBoxMember = new System.Windows.Forms.GroupBox();
        this.listViewMember = new System.Windows.Forms.ListView();
        this.groupBoxGiftFollow = new System.Windows.Forms.GroupBox();
        this.listViewGiftFollow = new System.Windows.Forms.ListView();
        this.groupBoxWatchedUsers = new System.Windows.Forms.GroupBox();
        this.listViewWatchedUsers = new System.Windows.Forms.ListView();
        this.groupBoxStatus = new System.Windows.Forms.GroupBox();
        this.textBoxStatus = new System.Windows.Forms.TextBox();
        this.columnHeaderChatTime = new System.Windows.Forms.ColumnHeader();
        this.columnHeaderChatUser = new System.Windows.Forms.ColumnHeader();
        this.columnHeaderChatUserId = new System.Windows.Forms.ColumnHeader();
        this.columnHeaderChatFansLevel = new System.Windows.Forms.ColumnHeader();
        this.columnHeaderChatPayLevel = new System.Windows.Forms.ColumnHeader();
        this.columnHeaderChatContent = new System.Windows.Forms.ColumnHeader();
        this.columnHeaderMemberTime = new System.Windows.Forms.ColumnHeader();
        this.columnHeaderMemberUser = new System.Windows.Forms.ColumnHeader();
        this.columnHeaderMemberUserId = new System.Windows.Forms.ColumnHeader();
        this.columnHeaderMemberFansLevel = new System.Windows.Forms.ColumnHeader();
        this.columnHeaderMemberPayLevel = new System.Windows.Forms.ColumnHeader();
        this.columnHeaderMemberContent = new System.Windows.Forms.ColumnHeader();
        this.columnHeaderGiftTime = new System.Windows.Forms.ColumnHeader();
        this.columnHeaderGiftType = new System.Windows.Forms.ColumnHeader();
        this.columnHeaderGiftUser = new System.Windows.Forms.ColumnHeader();
        this.columnHeaderGiftUserId = new System.Windows.Forms.ColumnHeader();
        this.columnHeaderGiftFansLevel = new System.Windows.Forms.ColumnHeader();
        this.columnHeaderGiftPayLevel = new System.Windows.Forms.ColumnHeader();
        this.columnHeaderGiftContent = new System.Windows.Forms.ColumnHeader();
        this.columnHeaderWatchedTime = new System.Windows.Forms.ColumnHeader();
        this.columnHeaderWatchedType = new System.Windows.Forms.ColumnHeader();
        this.columnHeaderWatchedUser = new System.Windows.Forms.ColumnHeader();
        this.columnHeaderWatchedUserId = new System.Windows.Forms.ColumnHeader();
        this.columnHeaderWatchedFansLevel = new System.Windows.Forms.ColumnHeader();
        this.columnHeaderWatchedPayLevel = new System.Windows.Forms.ColumnHeader();
        this.columnHeaderWatchedContent = new System.Windows.Forms.ColumnHeader();
        this.contextMenuStripMessage = new System.Windows.Forms.ContextMenuStrip();
        this.toolStripMenuItemAddToWatch = new System.Windows.Forms.ToolStripMenuItem();
        this.toolStripMenuItemRemoveFromWatch = new System.Windows.Forms.ToolStripMenuItem();
        this.toolStripSeparator1 = new System.Windows.Forms.ToolStripSeparator();
        this.toolStripMenuItemCopyUserId = new System.Windows.Forms.ToolStripMenuItem();
        this.toolStripMenuItemCopyUserName = new System.Windows.Forms.ToolStripMenuItem();
        this.groupBoxConnection.SuspendLayout();
        this.groupBoxMessages.SuspendLayout();
        this.groupBoxStatus.SuspendLayout();
        this.contextMenuStripMessage.SuspendLayout();
        this.SuspendLayout();
        // 
        // groupBoxConnection
        // 
        this.groupBoxConnection.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left)
        | System.Windows.Forms.AnchorStyles.Right)));
        this.groupBoxConnection.Controls.Add(this.buttonDatabase);
        this.groupBoxConnection.Controls.Add(this.buttonSettings);
        this.groupBoxConnection.Controls.Add(this.buttonWebSocket);
        this.groupBoxConnection.Controls.Add(this.buttonConnect);
        this.groupBoxConnection.Controls.Add(this.labelLiveId);
        this.groupBoxConnection.Controls.Add(this.textBoxLiveId);
        this.groupBoxConnection.Location = new System.Drawing.Point(12, 12);
        this.groupBoxConnection.Name = "groupBoxConnection";
        this.groupBoxConnection.Size = new System.Drawing.Size(1576, 80);
        this.groupBoxConnection.TabIndex = 0;
        this.groupBoxConnection.TabStop = false;
        this.groupBoxConnection.Text = "连接设置";
        // 
        // buttonDatabase
        // 
        this.buttonDatabase.Location = new System.Drawing.Point(640, 45);
        this.buttonDatabase.Name = "buttonDatabase";
        this.buttonDatabase.Size = new System.Drawing.Size(80, 25);
        this.buttonDatabase.TabIndex = 4;
        this.buttonDatabase.Text = "数据库";
        this.buttonDatabase.UseVisualStyleBackColor = true;
        this.buttonDatabase.Click += new System.EventHandler(this.ButtonDatabase_Click);
        // 
        // buttonSettings
        // 
        this.buttonSettings.Location = new System.Drawing.Point(550, 45);
        this.buttonSettings.Name = "buttonSettings";
        this.buttonSettings.Size = new System.Drawing.Size(80, 25);
        this.buttonSettings.TabIndex = 3;
        this.buttonSettings.Text = "设置";
        this.buttonSettings.UseVisualStyleBackColor = true;
        this.buttonSettings.Click += new System.EventHandler(this.ButtonSettings_Click);
        // 
        // buttonWebSocket
        // 
        this.buttonWebSocket.Location = new System.Drawing.Point(460, 45);
        this.buttonWebSocket.Name = "buttonWebSocket";
        this.buttonWebSocket.Size = new System.Drawing.Size(80, 25);
        this.buttonWebSocket.TabIndex = 2;
        this.buttonWebSocket.Text = "WebSocket";
        this.buttonWebSocket.UseVisualStyleBackColor = true;
        this.buttonWebSocket.Click += new System.EventHandler(this.ButtonWebSocket_Click);
        // 
        // buttonConnect
        // 
        this.buttonConnect.Location = new System.Drawing.Point(370, 45);
        this.buttonConnect.Name = "buttonConnect";
        this.buttonConnect.Size = new System.Drawing.Size(80, 25);
        this.buttonConnect.TabIndex = 2;
        this.buttonConnect.Text = "连接";
        this.buttonConnect.UseVisualStyleBackColor = true;
        this.buttonConnect.Click += new System.EventHandler(this.ButtonConnect_Click);
        // 
        // labelLiveId
        // 
        this.labelLiveId.AutoSize = true;
        this.labelLiveId.Location = new System.Drawing.Point(15, 50);
        this.labelLiveId.Name = "labelLiveId";
        this.labelLiveId.Size = new System.Drawing.Size(59, 17);
        this.labelLiveId.TabIndex = 0;
        this.labelLiveId.Text = "直播间ID:";
        // 
        // textBoxLiveId
        // 
        this.textBoxLiveId.Location = new System.Drawing.Point(80, 47);
        this.textBoxLiveId.Name = "textBoxLiveId";
        this.textBoxLiveId.Size = new System.Drawing.Size(280, 23);
        this.textBoxLiveId.TabIndex = 1;
        this.textBoxLiveId.Text = "MS4wLjABAAAA";
        this.textBoxLiveId.TextChanged += new System.EventHandler(this.TextBoxLiveId_TextChanged);
        // 
        // groupBoxMessages
        // 
        this.groupBoxMessages.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom)
        | System.Windows.Forms.AnchorStyles.Left)
        | System.Windows.Forms.AnchorStyles.Right)));
        this.groupBoxMessages.Controls.Add(this.buttonSaveLog);
        this.groupBoxMessages.Controls.Add(this.buttonClear);
        this.groupBoxMessages.Controls.Add(this.buttonShowUnknownTypes);
        this.groupBoxMessages.Controls.Add(this.checkBoxAutoScroll);
        this.groupBoxMessages.Controls.Add(this.groupBoxChat);
        this.groupBoxMessages.Controls.Add(this.groupBoxMember);
        this.groupBoxMessages.Controls.Add(this.groupBoxGiftFollow);
        this.groupBoxMessages.Controls.Add(this.groupBoxWatchedUsers);
        this.groupBoxMessages.Location = new System.Drawing.Point(12, 98);
        this.groupBoxMessages.Name = "groupBoxMessages";
        this.groupBoxMessages.Size = new System.Drawing.Size(1576, 400);
        this.groupBoxMessages.TabIndex = 1;
        this.groupBoxMessages.TabStop = false;
        this.groupBoxMessages.Text = "消息列表";
        // 
        // buttonSaveLog
        // 
        this.buttonSaveLog.Location = new System.Drawing.Point(1110, 368);
        this.buttonSaveLog.Name = "buttonSaveLog";
        this.buttonSaveLog.Size = new System.Drawing.Size(75, 25);
        this.buttonSaveLog.TabIndex = 3;
        this.buttonSaveLog.Text = "保存日志";
        this.buttonSaveLog.UseVisualStyleBackColor = true;
        this.buttonSaveLog.Click += new System.EventHandler(this.ButtonSaveLog_Click);
        // 
        // buttonClear
        // 
        this.buttonClear.Location = new System.Drawing.Point(1020, 368);
        this.buttonClear.Name = "buttonClear";
        this.buttonClear.Size = new System.Drawing.Size(75, 25);
        this.buttonClear.TabIndex = 2;
        this.buttonClear.Text = "清空";
        this.buttonClear.UseVisualStyleBackColor = true;
        this.buttonClear.Click += new System.EventHandler(this.ButtonClear_Click);
        // 
        // buttonShowUnknownTypes
        // 
        this.buttonShowUnknownTypes.Location = new System.Drawing.Point(920, 368);
        this.buttonShowUnknownTypes.Name = "buttonShowUnknownTypes";
        this.buttonShowUnknownTypes.Size = new System.Drawing.Size(90, 25);
        this.buttonShowUnknownTypes.TabIndex = 4;
        this.buttonShowUnknownTypes.Text = "未知类型统计";
        this.buttonShowUnknownTypes.UseVisualStyleBackColor = true;
        this.buttonShowUnknownTypes.Click += new System.EventHandler(this.BtnShowUnknownTypes_Click);
        // 
        // checkBoxAutoScroll
        // 
        this.checkBoxAutoScroll.AutoSize = true;
        this.checkBoxAutoScroll.Checked = true;
        this.checkBoxAutoScroll.CheckState = System.Windows.Forms.CheckState.Checked;
        this.checkBoxAutoScroll.Location = new System.Drawing.Point(15, 370);
        this.checkBoxAutoScroll.Name = "checkBoxAutoScroll";
        this.checkBoxAutoScroll.Size = new System.Drawing.Size(75, 21);
        this.checkBoxAutoScroll.TabIndex = 1;
        this.checkBoxAutoScroll.Text = "自动滚动";
        this.checkBoxAutoScroll.UseVisualStyleBackColor = true;
        this.checkBoxAutoScroll.CheckedChanged += new System.EventHandler(this.CheckBoxAutoScroll_CheckedChanged);
        // 
        // groupBoxChat
        // 
        this.groupBoxChat.Controls.Add(this.listViewChat);
        this.groupBoxChat.Location = new System.Drawing.Point(15, 22);
        this.groupBoxChat.Name = "groupBoxChat";
        this.groupBoxChat.Size = new System.Drawing.Size(380, 310);
        this.groupBoxChat.TabIndex = 5;
        this.groupBoxChat.TabStop = false;
        this.groupBoxChat.Text = "聊天消息";
        // 
        // listViewChat
        // 
        this.listViewChat.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
        this.columnHeaderChatTime,
        this.columnHeaderChatUser,
        this.columnHeaderChatUserId,
        this.columnHeaderChatFansLevel,
        this.columnHeaderChatPayLevel,
        this.columnHeaderChatContent});
        this.listViewChat.FullRowSelect = true;
        this.listViewChat.GridLines = true;
        this.listViewChat.Location = new System.Drawing.Point(6, 18);
        this.listViewChat.Name = "listViewChat";
        this.listViewChat.Size = new System.Drawing.Size(368, 286);
        this.listViewChat.TabIndex = 0;
        this.listViewChat.UseCompatibleStateImageBehavior = false;
        this.listViewChat.View = System.Windows.Forms.View.Details;
        this.listViewChat.ContextMenuStrip = this.contextMenuStripMessage;
        // 
        // groupBoxMember
        // 
        this.groupBoxMember.Controls.Add(this.listViewMember);
        this.groupBoxMember.Location = new System.Drawing.Point(405, 22);
        this.groupBoxMember.Name = "groupBoxMember";
        this.groupBoxMember.Size = new System.Drawing.Size(380, 310);
        this.groupBoxMember.TabIndex = 6;
        this.groupBoxMember.TabStop = false;
        this.groupBoxMember.Text = "进场消息";
        // 
        // listViewMember
        // 
        this.listViewMember.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
        this.columnHeaderMemberTime,
        this.columnHeaderMemberUser,
        this.columnHeaderMemberUserId,
        this.columnHeaderMemberFansLevel,
        this.columnHeaderMemberPayLevel,
        this.columnHeaderMemberContent});
        this.listViewMember.FullRowSelect = true;
        this.listViewMember.GridLines = true;
        this.listViewMember.Location = new System.Drawing.Point(6, 18);
        this.listViewMember.Name = "listViewMember";
        this.listViewMember.Size = new System.Drawing.Size(368, 286);
        this.listViewMember.TabIndex = 0;
        this.listViewMember.UseCompatibleStateImageBehavior = false;
        this.listViewMember.View = System.Windows.Forms.View.Details;
        this.listViewMember.ContextMenuStrip = this.contextMenuStripMessage;
        // 
        // groupBoxGiftFollow
        // 
        this.groupBoxGiftFollow.Controls.Add(this.listViewGiftFollow);
        this.groupBoxGiftFollow.Location = new System.Drawing.Point(795, 22);
        this.groupBoxGiftFollow.Name = "groupBoxGiftFollow";
        this.groupBoxGiftFollow.Size = new System.Drawing.Size(390, 310);
        this.groupBoxGiftFollow.TabIndex = 7;
        this.groupBoxGiftFollow.TabStop = false;
        this.groupBoxGiftFollow.Text = "礼物&关注";
        // 
        // listViewGiftFollow
        // 
        this.listViewGiftFollow.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
        this.columnHeaderGiftTime,
        this.columnHeaderGiftType,
        this.columnHeaderGiftUser,
        this.columnHeaderGiftUserId,
        this.columnHeaderGiftFansLevel,
        this.columnHeaderGiftPayLevel,
        this.columnHeaderGiftContent});
        this.listViewGiftFollow.FullRowSelect = true;
        this.listViewGiftFollow.GridLines = true;
        this.listViewGiftFollow.Location = new System.Drawing.Point(6, 18);
        this.listViewGiftFollow.Name = "listViewGiftFollow";
        this.listViewGiftFollow.Size = new System.Drawing.Size(378, 286);
        this.listViewGiftFollow.TabIndex = 0;
        this.listViewGiftFollow.UseCompatibleStateImageBehavior = false;
        this.listViewGiftFollow.View = System.Windows.Forms.View.Details;
        this.listViewGiftFollow.ContextMenuStrip = this.contextMenuStripMessage;
        // 
        // groupBoxWatchedUsers
        // 
        this.groupBoxWatchedUsers.Controls.Add(this.listViewWatchedUsers);
        this.groupBoxWatchedUsers.Location = new System.Drawing.Point(795, 338);
        this.groupBoxWatchedUsers.Name = "groupBoxWatchedUsers";
        this.groupBoxWatchedUsers.Size = new System.Drawing.Size(390, 310);
        this.groupBoxWatchedUsers.TabIndex = 8;
        this.groupBoxWatchedUsers.TabStop = false;
        this.groupBoxWatchedUsers.Text = "关注用户";
        // 
        // listViewWatchedUsers
        // 
        this.listViewWatchedUsers.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
        this.columnHeaderWatchedTime,
        this.columnHeaderWatchedType,
        this.columnHeaderWatchedUser,
        this.columnHeaderWatchedUserId,
        this.columnHeaderWatchedFansLevel,
        this.columnHeaderWatchedPayLevel,
        this.columnHeaderWatchedContent});
        this.listViewWatchedUsers.FullRowSelect = true;
        this.listViewWatchedUsers.GridLines = true;
        this.listViewWatchedUsers.Location = new System.Drawing.Point(6, 18);
        this.listViewWatchedUsers.Name = "listViewWatchedUsers";
        this.listViewWatchedUsers.Size = new System.Drawing.Size(378, 286);
        this.listViewWatchedUsers.TabIndex = 0;
        this.listViewWatchedUsers.UseCompatibleStateImageBehavior = false;
        this.listViewWatchedUsers.View = System.Windows.Forms.View.Details;
        this.listViewWatchedUsers.ContextMenuStrip = this.contextMenuStripMessage;
        // 
        // columnHeaderChatTime
        // 
        this.columnHeaderChatTime.Text = "时间";
        this.columnHeaderChatTime.Width = 60;
        // 
        // columnHeaderChatUser
        // 
        this.columnHeaderChatUser.Text = "用户";
        this.columnHeaderChatUser.Width = 80;
        // 
        // columnHeaderChatUserId
        // 
        this.columnHeaderChatUserId.Text = "用户ID";
        this.columnHeaderChatUserId.Width = 70;
        // 
        // columnHeaderChatFansLevel
        // 
        this.columnHeaderChatFansLevel.Text = "粉丝等级";
        this.columnHeaderChatFansLevel.Width = 60;
        // 
        // columnHeaderChatPayLevel
        // 
        this.columnHeaderChatPayLevel.Text = "付费等级";
        this.columnHeaderChatPayLevel.Width = 60;
        // 
        // columnHeaderChatContent
        // 
        this.columnHeaderChatContent.Text = "聊天内容";
        this.columnHeaderChatContent.Width = 120;
        // 
        // columnHeaderMemberTime
        // 
        this.columnHeaderMemberTime.Text = "时间";
        this.columnHeaderMemberTime.Width = 60;
        // 
        // columnHeaderMemberUser
        // 
        this.columnHeaderMemberUser.Text = "用户";
        this.columnHeaderMemberUser.Width = 80;
        // 
        // columnHeaderMemberUserId
        // 
        this.columnHeaderMemberUserId.Text = "用户ID";
        this.columnHeaderMemberUserId.Width = 70;
        // 
        // columnHeaderMemberFansLevel
        // 
        this.columnHeaderMemberFansLevel.Text = "粉丝等级";
        this.columnHeaderMemberFansLevel.Width = 60;
        // 
        // columnHeaderMemberPayLevel
        // 
        this.columnHeaderMemberPayLevel.Text = "付费等级";
        this.columnHeaderMemberPayLevel.Width = 60;
        // 
        // columnHeaderMemberContent
        // 
        this.columnHeaderMemberContent.Text = "内容";
        this.columnHeaderMemberContent.Width = 120;
        // 
        // columnHeaderGiftTime
        // 
        this.columnHeaderGiftTime.Text = "时间";
        this.columnHeaderGiftTime.Width = 50;
        // 
        // columnHeaderGiftType
        // 
        this.columnHeaderGiftType.Text = "类型";
        this.columnHeaderGiftType.Width = 50;
        // 
        // columnHeaderGiftUser
        // 
        this.columnHeaderGiftUser.Text = "用户";
        this.columnHeaderGiftUser.Width = 70;
        // 
        // columnHeaderGiftUserId
        // 
        this.columnHeaderGiftUserId.Text = "用户ID";
        this.columnHeaderGiftUserId.Width = 60;
        // 
        // columnHeaderGiftFansLevel
        // 
        this.columnHeaderGiftFansLevel.Text = "粉丝等级";
        this.columnHeaderGiftFansLevel.Width = 60;
        // 
        // columnHeaderGiftPayLevel
        // 
        this.columnHeaderGiftPayLevel.Text = "付费等级";
        this.columnHeaderGiftPayLevel.Width = 60;
        // 
        // columnHeaderGiftContent
        // 
        this.columnHeaderGiftContent.Text = "内容";
        this.columnHeaderGiftContent.Width = 100;
        // 
        // columnHeaderWatchedTime
        // 
        this.columnHeaderWatchedTime.Text = "时间";
        this.columnHeaderWatchedTime.Width = 50;
        // 
        // columnHeaderWatchedType
        // 
        this.columnHeaderWatchedType.Text = "类型";
        this.columnHeaderWatchedType.Width = 50;
        // 
        // columnHeaderWatchedUser
        // 
        this.columnHeaderWatchedUser.Text = "用户";
        this.columnHeaderWatchedUser.Width = 70;
        // 
        // columnHeaderWatchedUserId
        // 
        this.columnHeaderWatchedUserId.Text = "用户ID";
        this.columnHeaderWatchedUserId.Width = 60;
        // 
        // columnHeaderWatchedFansLevel
        // 
        this.columnHeaderWatchedFansLevel.Text = "粉丝等级";
        this.columnHeaderWatchedFansLevel.Width = 60;
        // 
        // columnHeaderWatchedPayLevel
        // 
        this.columnHeaderWatchedPayLevel.Text = "付费等级";
        this.columnHeaderWatchedPayLevel.Width = 60;
        // 
        // columnHeaderWatchedContent
        // 
        this.columnHeaderWatchedContent.Text = "内容";
        this.columnHeaderWatchedContent.Width = 100;
        // 
        // groupBoxStatus
        // 
        this.groupBoxStatus.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)
        | System.Windows.Forms.AnchorStyles.Right)));
        this.groupBoxStatus.Controls.Add(this.textBoxStatus);
        this.groupBoxStatus.Location = new System.Drawing.Point(12, 504);
        this.groupBoxStatus.Name = "groupBoxStatus";
        this.groupBoxStatus.Size = new System.Drawing.Size(800, 80);
        this.groupBoxStatus.TabIndex = 2;
        this.groupBoxStatus.TabStop = false;
        this.groupBoxStatus.Text = "状态信息";
        // 
        // textBoxStatus
        // 
        this.textBoxStatus.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom)
        | System.Windows.Forms.AnchorStyles.Left)
        | System.Windows.Forms.AnchorStyles.Right)));
        this.textBoxStatus.Location = new System.Drawing.Point(15, 22);
        this.textBoxStatus.Multiline = true;
        this.textBoxStatus.Name = "textBoxStatus";
        this.textBoxStatus.ReadOnly = true;
        this.textBoxStatus.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
        this.textBoxStatus.Size = new System.Drawing.Size(770, 50);
        this.textBoxStatus.TabIndex = 0;
        // 
        // 
        // contextMenuStripMessage
        // 
        this.contextMenuStripMessage.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
        this.toolStripMenuItemAddToWatch,
        this.toolStripMenuItemRemoveFromWatch,
        this.toolStripSeparator1,
        this.toolStripMenuItemCopyUserId,
        this.toolStripMenuItemCopyUserName});
        this.contextMenuStripMessage.Name = "contextMenuStripMessage";
        this.contextMenuStripMessage.Size = new System.Drawing.Size(181, 114);
        this.contextMenuStripMessage.Opening += new System.ComponentModel.CancelEventHandler(this.ContextMenuStripMessage_Opening);
        // 
        // toolStripMenuItemAddToWatch
        // 
        this.toolStripMenuItemAddToWatch.Name = "toolStripMenuItemAddToWatch";
        this.toolStripMenuItemAddToWatch.Size = new System.Drawing.Size(180, 22);
        this.toolStripMenuItemAddToWatch.Text = "添加到关注列表";
        this.toolStripMenuItemAddToWatch.Click += new System.EventHandler(this.ToolStripMenuItemAddToWatch_Click);
        // 
        // toolStripMenuItemRemoveFromWatch
        // 
        this.toolStripMenuItemRemoveFromWatch.Name = "toolStripMenuItemRemoveFromWatch";
        this.toolStripMenuItemRemoveFromWatch.Size = new System.Drawing.Size(180, 22);
        this.toolStripMenuItemRemoveFromWatch.Text = "从关注列表移除";
        this.toolStripMenuItemRemoveFromWatch.Click += new System.EventHandler(this.ToolStripMenuItemRemoveFromWatch_Click);
        // 
        // toolStripSeparator1
        // 
        this.toolStripSeparator1.Name = "toolStripSeparator1";
        this.toolStripSeparator1.Size = new System.Drawing.Size(177, 6);
        // 
        // toolStripMenuItemCopyUserId
        // 
        this.toolStripMenuItemCopyUserId.Name = "toolStripMenuItemCopyUserId";
        this.toolStripMenuItemCopyUserId.Size = new System.Drawing.Size(180, 22);
        this.toolStripMenuItemCopyUserId.Text = "复制用户ID";
        this.toolStripMenuItemCopyUserId.Click += new System.EventHandler(this.ToolStripMenuItemCopyUserId_Click);
        // 
        // toolStripMenuItemCopyUserName
        // 
        this.toolStripMenuItemCopyUserName.Name = "toolStripMenuItemCopyUserName";
        this.toolStripMenuItemCopyUserName.Size = new System.Drawing.Size(180, 22);
        this.toolStripMenuItemCopyUserName.Text = "复制用户名";
        this.toolStripMenuItemCopyUserName.Click += new System.EventHandler(this.ToolStripMenuItemCopyUserName_Click);
        // 
        // Form1
        // 
        this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 17F);
        this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
        this.ClientSize = new System.Drawing.Size(1600, 596);
        this.Controls.Add(this.groupBoxStatus);
        this.Controls.Add(this.groupBoxMessages);
        this.Controls.Add(this.groupBoxConnection);
        this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.Sizable;
        this.MaximizeBox = true;
        this.MinimumSize = new System.Drawing.Size(1616, 635);
        this.Name = "Form1";
        this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
        this.Text = "抖音直播弹幕抓取器 v1.3.0";
        this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.Form1_FormClosing);
        this.groupBoxConnection.ResumeLayout(false);
        this.groupBoxConnection.PerformLayout();
        this.groupBoxMessages.ResumeLayout(false);
        this.groupBoxMessages.PerformLayout();
        this.groupBoxStatus.ResumeLayout(false);
        this.groupBoxStatus.PerformLayout();
        this.contextMenuStripMessage.ResumeLayout(false);
        this.ResumeLayout(false);
    }

    #endregion

    private System.Windows.Forms.GroupBox groupBoxConnection;
    private System.Windows.Forms.Button buttonDatabase;
    private System.Windows.Forms.Button buttonSettings;
    private System.Windows.Forms.Button buttonWebSocket;
    private System.Windows.Forms.Button buttonConnect;
    private System.Windows.Forms.Label labelLiveId;
    private System.Windows.Forms.TextBox textBoxLiveId;
    private System.Windows.Forms.GroupBox groupBoxMessages;
    private System.Windows.Forms.Button buttonSaveLog;
    private System.Windows.Forms.Button buttonClear;
    private System.Windows.Forms.Button buttonShowUnknownTypes;
    private System.Windows.Forms.CheckBox checkBoxAutoScroll;
    private System.Windows.Forms.GroupBox groupBoxChat;
    private System.Windows.Forms.ListView listViewChat;
    private System.Windows.Forms.GroupBox groupBoxMember;
    private System.Windows.Forms.ListView listViewMember;
    private System.Windows.Forms.GroupBox groupBoxGiftFollow;
    private System.Windows.Forms.ListView listViewGiftFollow;
    private System.Windows.Forms.GroupBox groupBoxWatchedUsers;
    private System.Windows.Forms.ListView listViewWatchedUsers;
    private System.Windows.Forms.GroupBox groupBoxStatus;
    private System.Windows.Forms.TextBox textBoxStatus;

    private System.Windows.Forms.ColumnHeader columnHeaderChatTime;
    private System.Windows.Forms.ColumnHeader columnHeaderChatUser;
    private System.Windows.Forms.ColumnHeader columnHeaderChatUserId;
    private System.Windows.Forms.ColumnHeader columnHeaderChatFansLevel;
    private System.Windows.Forms.ColumnHeader columnHeaderChatPayLevel;
    private System.Windows.Forms.ColumnHeader columnHeaderChatContent;
    private System.Windows.Forms.ColumnHeader columnHeaderMemberTime;
    private System.Windows.Forms.ColumnHeader columnHeaderMemberUser;
    private System.Windows.Forms.ColumnHeader columnHeaderMemberUserId;
    private System.Windows.Forms.ColumnHeader columnHeaderMemberFansLevel;
    private System.Windows.Forms.ColumnHeader columnHeaderMemberPayLevel;
    private System.Windows.Forms.ColumnHeader columnHeaderMemberContent;
    private System.Windows.Forms.ColumnHeader columnHeaderGiftTime;
    private System.Windows.Forms.ColumnHeader columnHeaderGiftType;
    private System.Windows.Forms.ColumnHeader columnHeaderGiftUser;
    private System.Windows.Forms.ColumnHeader columnHeaderGiftUserId;
    private System.Windows.Forms.ColumnHeader columnHeaderGiftFansLevel;
    private System.Windows.Forms.ColumnHeader columnHeaderGiftPayLevel;
    private System.Windows.Forms.ColumnHeader columnHeaderGiftContent;
    private System.Windows.Forms.ColumnHeader columnHeaderWatchedTime;
    private System.Windows.Forms.ColumnHeader columnHeaderWatchedType;
    private System.Windows.Forms.ColumnHeader columnHeaderWatchedUser;
    private System.Windows.Forms.ColumnHeader columnHeaderWatchedUserId;
    private System.Windows.Forms.ColumnHeader columnHeaderWatchedFansLevel;
    private System.Windows.Forms.ColumnHeader columnHeaderWatchedPayLevel;
    private System.Windows.Forms.ColumnHeader columnHeaderWatchedContent;
    private System.Windows.Forms.ContextMenuStrip contextMenuStripMessage;
    private System.Windows.Forms.ToolStripMenuItem toolStripMenuItemAddToWatch;
    private System.Windows.Forms.ToolStripMenuItem toolStripMenuItemRemoveFromWatch;
    private System.Windows.Forms.ToolStripSeparator toolStripSeparator1;
    private System.Windows.Forms.ToolStripMenuItem toolStripMenuItemCopyUserId;
    private System.Windows.Forms.ToolStripMenuItem toolStripMenuItemCopyUserName;
}
