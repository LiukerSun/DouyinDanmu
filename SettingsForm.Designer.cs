namespace DouyinDanmu
{
    partial class SettingsForm
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

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
            this.textBoxNickname = new System.Windows.Forms.TextBox();
            this.labelNickname = new System.Windows.Forms.Label();
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
            this.groupBoxWatchedUsers.Location = new System.Drawing.Point(12, 88);
            this.groupBoxWatchedUsers.Name = "groupBoxWatchedUsers";
            this.groupBoxWatchedUsers.Size = new System.Drawing.Size(460, 280);
            this.groupBoxWatchedUsers.TabIndex = 0;
            this.groupBoxWatchedUsers.TabStop = false;
            this.groupBoxWatchedUsers.Text = "关注的用户列表";
            // 
            // listBoxUserIds
            // 
            this.listBoxUserIds.FormattingEnabled = true;
            this.listBoxUserIds.ItemHeight = 17;
            this.listBoxUserIds.Location = new System.Drawing.Point(15, 25);
            this.listBoxUserIds.Name = "listBoxUserIds";
            this.listBoxUserIds.Size = new System.Drawing.Size(430, 208);
            this.listBoxUserIds.TabIndex = 0;
            // 
            // groupBoxAddUser
            // 
            this.groupBoxAddUser.Controls.Add(this.buttonAdd);
            this.groupBoxAddUser.Controls.Add(this.textBoxNickname);
            this.groupBoxAddUser.Controls.Add(this.labelNickname);
            this.groupBoxAddUser.Controls.Add(this.textBoxUserId);
            this.groupBoxAddUser.Controls.Add(this.labelUserId);
            this.groupBoxAddUser.Location = new System.Drawing.Point(12, 12);
            this.groupBoxAddUser.Name = "groupBoxAddUser";
            this.groupBoxAddUser.Size = new System.Drawing.Size(460, 70);
            this.groupBoxAddUser.TabIndex = 1;
            this.groupBoxAddUser.TabStop = false;
            this.groupBoxAddUser.Text = "添加用户";
            // 
            // buttonAdd
            // 
            this.buttonAdd.Location = new System.Drawing.Point(370, 27);
            this.buttonAdd.Name = "buttonAdd";
            this.buttonAdd.Size = new System.Drawing.Size(75, 25);
            this.buttonAdd.TabIndex = 4;
            this.buttonAdd.Text = "添加";
            this.buttonAdd.UseVisualStyleBackColor = true;
            this.buttonAdd.Click += new System.EventHandler(this.buttonAdd_Click);
            // 
            // textBoxNickname
            // 
            this.textBoxNickname.Location = new System.Drawing.Point(80, 62);
            this.textBoxNickname.Name = "textBoxNickname";
            this.textBoxNickname.Size = new System.Drawing.Size(280, 23);
            this.textBoxNickname.TabIndex = 3;
            this.textBoxNickname.Visible = false;
            this.textBoxNickname.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.textBoxNickname_KeyPress);
            // 
            // labelNickname
            // 
            this.labelNickname.AutoSize = true;
            this.labelNickname.Location = new System.Drawing.Point(15, 65);
            this.labelNickname.Name = "labelNickname";
            this.labelNickname.Size = new System.Drawing.Size(59, 17);
            this.labelNickname.TabIndex = 2;
            this.labelNickname.Text = "用户昵称:";
            this.labelNickname.Visible = false;
            // 
            // textBoxUserId
            // 
            this.textBoxUserId.Location = new System.Drawing.Point(80, 27);
            this.textBoxUserId.Name = "textBoxUserId";
            this.textBoxUserId.Size = new System.Drawing.Size(280, 23);
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
            this.buttonRemove.Location = new System.Drawing.Point(15, 245);
            this.buttonRemove.Name = "buttonRemove";
            this.buttonRemove.Size = new System.Drawing.Size(85, 25);
            this.buttonRemove.TabIndex = 1;
            this.buttonRemove.Text = "删除选中";
            this.buttonRemove.UseVisualStyleBackColor = true;
            this.buttonRemove.Click += new System.EventHandler(this.buttonRemove_Click);
            // 
            // buttonClear
            // 
            this.buttonClear.Location = new System.Drawing.Point(110, 245);
            this.buttonClear.Name = "buttonClear";
            this.buttonClear.Size = new System.Drawing.Size(85, 25);
            this.buttonClear.TabIndex = 2;
            this.buttonClear.Text = "清空全部";
            this.buttonClear.UseVisualStyleBackColor = true;
            this.buttonClear.Click += new System.EventHandler(this.buttonClear_Click);
            // 
            // buttonOK
            // 
            this.buttonOK.Location = new System.Drawing.Point(312, 385);
            this.buttonOK.Name = "buttonOK";
            this.buttonOK.Size = new System.Drawing.Size(75, 30);
            this.buttonOK.TabIndex = 2;
            this.buttonOK.Text = "确定";
            this.buttonOK.UseVisualStyleBackColor = true;
            this.buttonOK.Click += new System.EventHandler(this.buttonOK_Click);
            // 
            // buttonCancel
            // 
            this.buttonCancel.Location = new System.Drawing.Point(397, 385);
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
            this.labelDescription.Location = new System.Drawing.Point(12, 390);
            this.labelDescription.Name = "labelDescription";
            this.labelDescription.Size = new System.Drawing.Size(240, 17);
            this.labelDescription.TabIndex = 4;
            this.labelDescription.Text = "提示：输入用户ID后会自动从数据库查找昵称";
            this.labelDescription.Click += new System.EventHandler(this.labelDescription_Click);
            // 
            // SettingsForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 17F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(484, 430);
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
        private System.Windows.Forms.TextBox textBoxNickname;
        private System.Windows.Forms.Label labelNickname;
        private System.Windows.Forms.TextBox textBoxUserId;
        private System.Windows.Forms.Label labelUserId;
        private System.Windows.Forms.Button buttonRemove;
        private System.Windows.Forms.Button buttonClear;
        private System.Windows.Forms.Button buttonOK;
        private System.Windows.Forms.Button buttonCancel;
        private System.Windows.Forms.Label labelDescription;
    }
} 