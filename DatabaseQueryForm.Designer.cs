namespace DouyinDanmu
{
    partial class DatabaseQueryForm
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
            this.groupBoxFilters = new System.Windows.Forms.GroupBox();
            this.labelLiveId = new System.Windows.Forms.Label();
            this.comboBoxLiveId = new System.Windows.Forms.ComboBox();
            this.labelUserId = new System.Windows.Forms.Label();
            this.textBoxUserId = new System.Windows.Forms.TextBox();
            this.labelUserName = new System.Windows.Forms.Label();
            this.textBoxUserName = new System.Windows.Forms.TextBox();
            this.labelMessageTypes = new System.Windows.Forms.Label();
            this.checkBoxChat = new System.Windows.Forms.CheckBox();
            this.checkBoxMember = new System.Windows.Forms.CheckBox();
            this.checkBoxGift = new System.Windows.Forms.CheckBox();
            this.checkBoxLike = new System.Windows.Forms.CheckBox();
            this.checkBoxSocial = new System.Windows.Forms.CheckBox();
            this.labelDateRange = new System.Windows.Forms.Label();
            this.dateTimePickerStart = new System.Windows.Forms.DateTimePicker();
            this.labelTo = new System.Windows.Forms.Label();
            this.dateTimePickerEnd = new System.Windows.Forms.DateTimePicker();
            this.buttonQuery = new System.Windows.Forms.Button();
            this.buttonReset = new System.Windows.Forms.Button();
            this.buttonExport = new System.Windows.Forms.Button();
            this.groupBoxResults = new System.Windows.Forms.GroupBox();
            this.listViewResults = new System.Windows.Forms.ListView();
            this.columnHeaderId = new System.Windows.Forms.ColumnHeader();
            this.columnHeaderLiveId = new System.Windows.Forms.ColumnHeader();
            this.columnHeaderTimestamp = new System.Windows.Forms.ColumnHeader();
            this.columnHeaderType = new System.Windows.Forms.ColumnHeader();
            this.columnHeaderUserId = new System.Windows.Forms.ColumnHeader();
            this.columnHeaderUserName = new System.Windows.Forms.ColumnHeader();
            this.columnHeaderFansLevel = new System.Windows.Forms.ColumnHeader();
            this.columnHeaderPayLevel = new System.Windows.Forms.ColumnHeader();
            this.columnHeaderContent = new System.Windows.Forms.ColumnHeader();
            this.labelResultCount = new System.Windows.Forms.Label();
            this.groupBoxFilters.SuspendLayout();
            this.groupBoxResults.SuspendLayout();
            this.SuspendLayout();
            // 
            // groupBoxFilters
            // 
            this.groupBoxFilters.Controls.Add(this.buttonExport);
            this.groupBoxFilters.Controls.Add(this.buttonReset);
            this.groupBoxFilters.Controls.Add(this.buttonQuery);
            this.groupBoxFilters.Controls.Add(this.dateTimePickerEnd);
            this.groupBoxFilters.Controls.Add(this.labelTo);
            this.groupBoxFilters.Controls.Add(this.dateTimePickerStart);
            this.groupBoxFilters.Controls.Add(this.labelDateRange);
            this.groupBoxFilters.Controls.Add(this.checkBoxSocial);
            this.groupBoxFilters.Controls.Add(this.checkBoxLike);
            this.groupBoxFilters.Controls.Add(this.checkBoxGift);
            this.groupBoxFilters.Controls.Add(this.checkBoxMember);
            this.groupBoxFilters.Controls.Add(this.checkBoxChat);
            this.groupBoxFilters.Controls.Add(this.labelMessageTypes);
            this.groupBoxFilters.Controls.Add(this.textBoxUserName);
            this.groupBoxFilters.Controls.Add(this.labelUserName);
            this.groupBoxFilters.Controls.Add(this.textBoxUserId);
            this.groupBoxFilters.Controls.Add(this.labelUserId);
            this.groupBoxFilters.Controls.Add(this.comboBoxLiveId);
            this.groupBoxFilters.Controls.Add(this.labelLiveId);
            this.groupBoxFilters.Location = new System.Drawing.Point(12, 12);
            this.groupBoxFilters.Name = "groupBoxFilters";
            this.groupBoxFilters.Size = new System.Drawing.Size(1160, 120);
            this.groupBoxFilters.TabIndex = 0;
            this.groupBoxFilters.TabStop = false;
            this.groupBoxFilters.Text = "筛选条件";
            // 
            // labelLiveId
            // 
            this.labelLiveId.AutoSize = true;
            this.labelLiveId.Location = new System.Drawing.Point(15, 30);
            this.labelLiveId.Name = "labelLiveId";
            this.labelLiveId.Size = new System.Drawing.Size(59, 17);
            this.labelLiveId.TabIndex = 0;
            this.labelLiveId.Text = "直播间号:";
            // 
            // comboBoxLiveId
            // 
            this.comboBoxLiveId.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.comboBoxLiveId.FormattingEnabled = true;
            this.comboBoxLiveId.Location = new System.Drawing.Point(80, 27);
            this.comboBoxLiveId.Name = "comboBoxLiveId";
            this.comboBoxLiveId.Size = new System.Drawing.Size(200, 25);
            this.comboBoxLiveId.TabIndex = 1;
            // 
            // labelUserId
            // 
            this.labelUserId.AutoSize = true;
            this.labelUserId.Location = new System.Drawing.Point(300, 30);
            this.labelUserId.Name = "labelUserId";
            this.labelUserId.Size = new System.Drawing.Size(59, 17);
            this.labelUserId.TabIndex = 2;
            this.labelUserId.Text = "用户ID:";
            // 
            // textBoxUserId
            // 
            this.textBoxUserId.Location = new System.Drawing.Point(365, 27);
            this.textBoxUserId.Name = "textBoxUserId";
            this.textBoxUserId.Size = new System.Drawing.Size(150, 23);
            this.textBoxUserId.TabIndex = 3;
            // 
            // labelUserName
            // 
            this.labelUserName.AutoSize = true;
            this.labelUserName.Location = new System.Drawing.Point(535, 30);
            this.labelUserName.Name = "labelUserName";
            this.labelUserName.Size = new System.Drawing.Size(47, 17);
            this.labelUserName.TabIndex = 4;
            this.labelUserName.Text = "用户名:";
            // 
            // textBoxUserName
            // 
            this.textBoxUserName.Location = new System.Drawing.Point(588, 27);
            this.textBoxUserName.Name = "textBoxUserName";
            this.textBoxUserName.Size = new System.Drawing.Size(150, 23);
            this.textBoxUserName.TabIndex = 5;
            // 
            // labelMessageTypes
            // 
            this.labelMessageTypes.AutoSize = true;
            this.labelMessageTypes.Location = new System.Drawing.Point(15, 65);
            this.labelMessageTypes.Name = "labelMessageTypes";
            this.labelMessageTypes.Size = new System.Drawing.Size(59, 17);
            this.labelMessageTypes.TabIndex = 6;
            this.labelMessageTypes.Text = "消息类型:";
            // 
            // checkBoxChat
            // 
            this.checkBoxChat.AutoSize = true;
            this.checkBoxChat.Checked = true;
            this.checkBoxChat.CheckState = System.Windows.Forms.CheckState.Checked;
            this.checkBoxChat.Location = new System.Drawing.Point(80, 64);
            this.checkBoxChat.Name = "checkBoxChat";
            this.checkBoxChat.Size = new System.Drawing.Size(51, 21);
            this.checkBoxChat.TabIndex = 7;
            this.checkBoxChat.Text = "聊天";
            this.checkBoxChat.UseVisualStyleBackColor = true;
            // 
            // checkBoxMember
            // 
            this.checkBoxMember.AutoSize = true;
            this.checkBoxMember.Checked = true;
            this.checkBoxMember.CheckState = System.Windows.Forms.CheckState.Checked;
            this.checkBoxMember.Location = new System.Drawing.Point(140, 64);
            this.checkBoxMember.Name = "checkBoxMember";
            this.checkBoxMember.Size = new System.Drawing.Size(51, 21);
            this.checkBoxMember.TabIndex = 8;
            this.checkBoxMember.Text = "进场";
            this.checkBoxMember.UseVisualStyleBackColor = true;
            // 
            // checkBoxGift
            // 
            this.checkBoxGift.AutoSize = true;
            this.checkBoxGift.Checked = true;
            this.checkBoxGift.CheckState = System.Windows.Forms.CheckState.Checked;
            this.checkBoxGift.Location = new System.Drawing.Point(200, 64);
            this.checkBoxGift.Name = "checkBoxGift";
            this.checkBoxGift.Size = new System.Drawing.Size(51, 21);
            this.checkBoxGift.TabIndex = 9;
            this.checkBoxGift.Text = "礼物";
            this.checkBoxGift.UseVisualStyleBackColor = true;
            // 
            // checkBoxLike
            // 
            this.checkBoxLike.AutoSize = true;
            this.checkBoxLike.Checked = true;
            this.checkBoxLike.CheckState = System.Windows.Forms.CheckState.Checked;
            this.checkBoxLike.Location = new System.Drawing.Point(260, 64);
            this.checkBoxLike.Name = "checkBoxLike";
            this.checkBoxLike.Size = new System.Drawing.Size(51, 21);
            this.checkBoxLike.TabIndex = 10;
            this.checkBoxLike.Text = "点赞";
            this.checkBoxLike.UseVisualStyleBackColor = true;
            // 
            // checkBoxSocial
            // 
            this.checkBoxSocial.AutoSize = true;
            this.checkBoxSocial.Checked = true;
            this.checkBoxSocial.CheckState = System.Windows.Forms.CheckState.Checked;
            this.checkBoxSocial.Location = new System.Drawing.Point(320, 64);
            this.checkBoxSocial.Name = "checkBoxSocial";
            this.checkBoxSocial.Size = new System.Drawing.Size(51, 21);
            this.checkBoxSocial.TabIndex = 11;
            this.checkBoxSocial.Text = "关注";
            this.checkBoxSocial.UseVisualStyleBackColor = true;
            // 
            // labelDateRange
            // 
            this.labelDateRange.AutoSize = true;
            this.labelDateRange.Location = new System.Drawing.Point(400, 67);
            this.labelDateRange.Name = "labelDateRange";
            this.labelDateRange.Size = new System.Drawing.Size(59, 17);
            this.labelDateRange.TabIndex = 12;
            this.labelDateRange.Text = "时间范围:";
            // 
            // dateTimePickerStart
            // 
            this.dateTimePickerStart.CustomFormat = "yyyy-MM-dd HH:mm";
            this.dateTimePickerStart.Format = System.Windows.Forms.DateTimePickerFormat.Custom;
            this.dateTimePickerStart.Location = new System.Drawing.Point(465, 64);
            this.dateTimePickerStart.Name = "dateTimePickerStart";
            this.dateTimePickerStart.Size = new System.Drawing.Size(140, 23);
            this.dateTimePickerStart.TabIndex = 13;
            // 
            // labelTo
            // 
            this.labelTo.AutoSize = true;
            this.labelTo.Location = new System.Drawing.Point(615, 67);
            this.labelTo.Name = "labelTo";
            this.labelTo.Size = new System.Drawing.Size(20, 17);
            this.labelTo.TabIndex = 14;
            this.labelTo.Text = "至";
            // 
            // dateTimePickerEnd
            // 
            this.dateTimePickerEnd.CustomFormat = "yyyy-MM-dd HH:mm";
            this.dateTimePickerEnd.Format = System.Windows.Forms.DateTimePickerFormat.Custom;
            this.dateTimePickerEnd.Location = new System.Drawing.Point(645, 64);
            this.dateTimePickerEnd.Name = "dateTimePickerEnd";
            this.dateTimePickerEnd.Size = new System.Drawing.Size(140, 23);
            this.dateTimePickerEnd.TabIndex = 15;
            // 
            // buttonQuery
            // 
            this.buttonQuery.Location = new System.Drawing.Point(800, 25);
            this.buttonQuery.Name = "buttonQuery";
            this.buttonQuery.Size = new System.Drawing.Size(80, 30);
            this.buttonQuery.TabIndex = 16;
            this.buttonQuery.Text = "查询";
            this.buttonQuery.UseVisualStyleBackColor = true;
            this.buttonQuery.Click += new System.EventHandler(this.buttonQuery_Click);
            // 
            // buttonReset
            // 
            this.buttonReset.Location = new System.Drawing.Point(890, 25);
            this.buttonReset.Name = "buttonReset";
            this.buttonReset.Size = new System.Drawing.Size(80, 30);
            this.buttonReset.TabIndex = 17;
            this.buttonReset.Text = "重置";
            this.buttonReset.UseVisualStyleBackColor = true;
            this.buttonReset.Click += new System.EventHandler(this.buttonReset_Click);
            // 
            // buttonExport
            // 
            this.buttonExport.Location = new System.Drawing.Point(980, 25);
            this.buttonExport.Name = "buttonExport";
            this.buttonExport.Size = new System.Drawing.Size(80, 30);
            this.buttonExport.TabIndex = 18;
            this.buttonExport.Text = "导出";
            this.buttonExport.UseVisualStyleBackColor = true;
            this.buttonExport.Click += new System.EventHandler(this.buttonExport_Click);
            // 
            // groupBoxResults
            // 
            this.groupBoxResults.Controls.Add(this.labelResultCount);
            this.groupBoxResults.Controls.Add(this.listViewResults);
            this.groupBoxResults.Location = new System.Drawing.Point(12, 138);
            this.groupBoxResults.Name = "groupBoxResults";
            this.groupBoxResults.Size = new System.Drawing.Size(1160, 450);
            this.groupBoxResults.TabIndex = 1;
            this.groupBoxResults.TabStop = false;
            this.groupBoxResults.Text = "查询结果";
            // 
            // listViewResults
            // 
            this.listViewResults.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.columnHeaderId,
            this.columnHeaderLiveId,
            this.columnHeaderTimestamp,
            this.columnHeaderType,
            this.columnHeaderUserId,
            this.columnHeaderUserName,
            this.columnHeaderFansLevel,
            this.columnHeaderPayLevel,
            this.columnHeaderContent});
            this.listViewResults.FullRowSelect = true;
            this.listViewResults.GridLines = true;
            this.listViewResults.Location = new System.Drawing.Point(15, 22);
            this.listViewResults.Name = "listViewResults";
            this.listViewResults.Size = new System.Drawing.Size(1130, 390);
            this.listViewResults.TabIndex = 0;
            this.listViewResults.UseCompatibleStateImageBehavior = false;
            this.listViewResults.View = System.Windows.Forms.View.Details;
            // 
            // columnHeaderId
            // 
            this.columnHeaderId.Text = "ID";
            this.columnHeaderId.Width = 50;
            // 
            // columnHeaderLiveId
            // 
            this.columnHeaderLiveId.Text = "直播间号";
            this.columnHeaderLiveId.Width = 100;
            // 
            // columnHeaderTimestamp
            // 
            this.columnHeaderTimestamp.Text = "时间";
            this.columnHeaderTimestamp.Width = 130;
            // 
            // columnHeaderType
            // 
            this.columnHeaderType.Text = "类型";
            this.columnHeaderType.Width = 60;
            // 
            // columnHeaderUserId
            // 
            this.columnHeaderUserId.Text = "用户ID";
            this.columnHeaderUserId.Width = 100;
            // 
            // columnHeaderUserName
            // 
            this.columnHeaderUserName.Text = "用户名";
            this.columnHeaderUserName.Width = 120;
            // 
            // columnHeaderFansLevel
            // 
            this.columnHeaderFansLevel.Text = "粉丝等级";
            this.columnHeaderFansLevel.Width = 70;
            // 
            // columnHeaderPayLevel
            // 
            this.columnHeaderPayLevel.Text = "财富等级";
            this.columnHeaderPayLevel.Width = 70;
            // 
            // columnHeaderContent
            // 
            this.columnHeaderContent.Text = "内容";
            this.columnHeaderContent.Width = 400;
            // 
            // labelResultCount
            // 
            this.labelResultCount.AutoSize = true;
            this.labelResultCount.Location = new System.Drawing.Point(15, 420);
            this.labelResultCount.Name = "labelResultCount";
            this.labelResultCount.Size = new System.Drawing.Size(80, 17);
            this.labelResultCount.TabIndex = 1;
            this.labelResultCount.Text = "查询结果: 0条";
            // 
            // DatabaseQueryForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 17F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(1184, 600);
            this.Controls.Add(this.groupBoxResults);
            this.Controls.Add(this.groupBoxFilters);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "DatabaseQueryForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "数据库查询 - 抖音直播消息";
            this.Load += new System.EventHandler(this.DatabaseQueryForm_Load);
            this.groupBoxFilters.ResumeLayout(false);
            this.groupBoxFilters.PerformLayout();
            this.groupBoxResults.ResumeLayout(false);
            this.groupBoxResults.PerformLayout();
            this.ResumeLayout(false);
        }

        #endregion

        private System.Windows.Forms.GroupBox groupBoxFilters;
        private System.Windows.Forms.Label labelLiveId;
        private System.Windows.Forms.ComboBox comboBoxLiveId;
        private System.Windows.Forms.Label labelUserId;
        private System.Windows.Forms.TextBox textBoxUserId;
        private System.Windows.Forms.Label labelUserName;
        private System.Windows.Forms.TextBox textBoxUserName;
        private System.Windows.Forms.Label labelMessageTypes;
        private System.Windows.Forms.CheckBox checkBoxChat;
        private System.Windows.Forms.CheckBox checkBoxMember;
        private System.Windows.Forms.CheckBox checkBoxGift;
        private System.Windows.Forms.CheckBox checkBoxLike;
        private System.Windows.Forms.CheckBox checkBoxSocial;
        private System.Windows.Forms.Label labelDateRange;
        private System.Windows.Forms.DateTimePicker dateTimePickerStart;
        private System.Windows.Forms.Label labelTo;
        private System.Windows.Forms.DateTimePicker dateTimePickerEnd;
        private System.Windows.Forms.Button buttonQuery;
        private System.Windows.Forms.Button buttonReset;
        private System.Windows.Forms.Button buttonExport;
        private System.Windows.Forms.GroupBox groupBoxResults;
        private System.Windows.Forms.ListView listViewResults;
        private System.Windows.Forms.ColumnHeader columnHeaderId;
        private System.Windows.Forms.ColumnHeader columnHeaderLiveId;
        private System.Windows.Forms.ColumnHeader columnHeaderTimestamp;
        private System.Windows.Forms.ColumnHeader columnHeaderType;
        private System.Windows.Forms.ColumnHeader columnHeaderUserId;
        private System.Windows.Forms.ColumnHeader columnHeaderUserName;
        private System.Windows.Forms.ColumnHeader columnHeaderFansLevel;
        private System.Windows.Forms.ColumnHeader columnHeaderPayLevel;
        private System.Windows.Forms.ColumnHeader columnHeaderContent;
        private System.Windows.Forms.Label labelResultCount;
    }
} 