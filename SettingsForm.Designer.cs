namespace DouyinDanmu
{
    partial class SettingsForm
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
            this.groupBoxWatchedUsers = new System.Windows.Forms.GroupBox();
            this.listBoxUserIds = new System.Windows.Forms.ListBox();
            this.groupBoxAddUser = new System.Windows.Forms.GroupBox();
            this.buttonAdd = new System.Windows.Forms.Button();
            this.textBoxUserId = new System.Windows.Forms.TextBox();
            this.labelUserId = new System.Windows.Forms.Label();
            this.buttonRemove = new System.Windows.Forms.Button();
            this.buttonClear = new System.Windows.Forms.Button();
            this.buttonOK = new System.Windows.Forms.Button();
            this.buttonCancel = new System.Windows.Forms.Button();
            this.labelDescription = new System.Windows.Forms.Label();
            this.groupBoxWatchedUsers.SuspendLayout();
            this.groupBoxAddUser.SuspendLayout();
            this.SuspendLayout();
            // 
            // groupBoxWatchedUsers
            // 
            this.groupBoxWatchedUsers.Controls.Add(this.buttonClear);
            this.groupBoxWatchedUsers.Controls.Add(this.buttonRemove);
            this.groupBoxWatchedUsers.Controls.Add(this.listBoxUserIds);
            this.groupBoxWatchedUsers.Location = new System.Drawing.Point(12, 80);
            this.groupBoxWatchedUsers.Name = "groupBoxWatchedUsers";
            this.groupBoxWatchedUsers.Size = new System.Drawing.Size(360, 250);
            this.groupBoxWatchedUsers.TabIndex = 0;
            this.groupBoxWatchedUsers.TabStop = false;
            this.groupBoxWatchedUsers.Text = "关注的用户ID列表";
            // 
            // listBoxUserIds
            // 
            this.listBoxUserIds.FormattingEnabled = true;
            this.listBoxUserIds.ItemHeight = 17;
            this.listBoxUserIds.Location = new System.Drawing.Point(15, 25);
            this.listBoxUserIds.Name = "listBoxUserIds";
            this.listBoxUserIds.Size = new System.Drawing.Size(330, 174);
            this.listBoxUserIds.TabIndex = 0;
            // 
            // groupBoxAddUser
            // 
            this.groupBoxAddUser.Controls.Add(this.buttonAdd);
            this.groupBoxAddUser.Controls.Add(this.textBoxUserId);
            this.groupBoxAddUser.Controls.Add(this.labelUserId);
            this.groupBoxAddUser.Location = new System.Drawing.Point(12, 12);
            this.groupBoxAddUser.Name = "groupBoxAddUser";
            this.groupBoxAddUser.Size = new System.Drawing.Size(360, 62);
            this.groupBoxAddUser.TabIndex = 1;
            this.groupBoxAddUser.TabStop = false;
            this.groupBoxAddUser.Text = "添加用户ID";
            // 
            // buttonAdd
            // 
            this.buttonAdd.Location = new System.Drawing.Point(270, 25);
            this.buttonAdd.Name = "buttonAdd";
            this.buttonAdd.Size = new System.Drawing.Size(75, 25);
            this.buttonAdd.TabIndex = 2;
            this.buttonAdd.Text = "添加";
            this.buttonAdd.UseVisualStyleBackColor = true;
            this.buttonAdd.Click += new System.EventHandler(this.buttonAdd_Click);
            // 
            // textBoxUserId
            // 
            this.textBoxUserId.Location = new System.Drawing.Point(80, 27);
            this.textBoxUserId.Name = "textBoxUserId";
            this.textBoxUserId.Size = new System.Drawing.Size(180, 23);
            this.textBoxUserId.TabIndex = 1;
            this.textBoxUserId.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.textBoxUserId_KeyPress);
            // 
            // labelUserId
            // 
            this.labelUserId.AutoSize = true;
            this.labelUserId.Location = new System.Drawing.Point(15, 30);
            this.labelUserId.Name = "labelUserId";
            this.labelUserId.Size = new System.Drawing.Size(59, 17);
            this.labelUserId.TabIndex = 0;
            this.labelUserId.Text = "用户ID:";
            // 
            // buttonRemove
            // 
            this.buttonRemove.Location = new System.Drawing.Point(15, 210);
            this.buttonRemove.Name = "buttonRemove";
            this.buttonRemove.Size = new System.Drawing.Size(75, 25);
            this.buttonRemove.TabIndex = 1;
            this.buttonRemove.Text = "删除选中";
            this.buttonRemove.UseVisualStyleBackColor = true;
            this.buttonRemove.Click += new System.EventHandler(this.buttonRemove_Click);
            // 
            // buttonClear
            // 
            this.buttonClear.Location = new System.Drawing.Point(100, 210);
            this.buttonClear.Name = "buttonClear";
            this.buttonClear.Size = new System.Drawing.Size(75, 25);
            this.buttonClear.TabIndex = 2;
            this.buttonClear.Text = "清空全部";
            this.buttonClear.UseVisualStyleBackColor = true;
            this.buttonClear.Click += new System.EventHandler(this.buttonClear_Click);
            // 
            // buttonOK
            // 
            this.buttonOK.Location = new System.Drawing.Point(212, 345);
            this.buttonOK.Name = "buttonOK";
            this.buttonOK.Size = new System.Drawing.Size(75, 30);
            this.buttonOK.TabIndex = 2;
            this.buttonOK.Text = "确定";
            this.buttonOK.UseVisualStyleBackColor = true;
            this.buttonOK.Click += new System.EventHandler(this.buttonOK_Click);
            // 
            // buttonCancel
            // 
            this.buttonCancel.Location = new System.Drawing.Point(297, 345);
            this.buttonCancel.Name = "buttonCancel";
            this.buttonCancel.Size = new System.Drawing.Size(75, 30);
            this.buttonCancel.TabIndex = 3;
            this.buttonCancel.Text = "取消";
            this.buttonCancel.UseVisualStyleBackColor = true;
            this.buttonCancel.Click += new System.EventHandler(this.buttonCancel_Click);
            // 
            // labelDescription
            // 
            this.labelDescription.AutoSize = true;
            this.labelDescription.ForeColor = System.Drawing.SystemColors.GrayText;
            this.labelDescription.Location = new System.Drawing.Point(12, 350);
            this.labelDescription.Name = "labelDescription";
            this.labelDescription.Size = new System.Drawing.Size(180, 17);
            this.labelDescription.TabIndex = 4;
            this.labelDescription.Text = "提示：添加的用户消息将单独显示";
            this.labelDescription.Click += new System.EventHandler(this.labelDescription_Click);
            // 
            // SettingsForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 17F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(384, 387);
            this.Controls.Add(this.labelDescription);
            this.Controls.Add(this.buttonCancel);
            this.Controls.Add(this.buttonOK);
            this.Controls.Add(this.groupBoxAddUser);
            this.Controls.Add(this.groupBoxWatchedUsers);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "SettingsForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "设置 - 关注用户";
            this.groupBoxWatchedUsers.ResumeLayout(false);
            this.groupBoxAddUser.ResumeLayout(false);
            this.groupBoxAddUser.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();
        }

        #endregion

        private System.Windows.Forms.GroupBox groupBoxWatchedUsers;
        private System.Windows.Forms.ListBox listBoxUserIds;
        private System.Windows.Forms.GroupBox groupBoxAddUser;
        private System.Windows.Forms.Button buttonAdd;
        private System.Windows.Forms.TextBox textBoxUserId;
        private System.Windows.Forms.Label labelUserId;
        private System.Windows.Forms.Button buttonRemove;
        private System.Windows.Forms.Button buttonClear;
        private System.Windows.Forms.Button buttonOK;
        private System.Windows.Forms.Button buttonCancel;
        private System.Windows.Forms.Label labelDescription;
    }
} 