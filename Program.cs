using System;
using System.Windows.Forms;

namespace DouyinDanmu;

static class Program
{
    /// <summary>
    ///  The main entry point for the application.
    /// </summary>
    [STAThread]
    static void Main(string[] args)
    {
        // 检查是否是测试模式
        if (args.Length > 0 && args[0] == "--test-settings")
        {
            TestSettingsRecovery.RunTest();
            return;
        }
        
        // 检查是否是修复模式
        if (args.Length > 0 && args[0] == "--fix-settings")
        {
            SettingsFixTool.FixSettings();
            return;
        }
        
        try
        {
            // 在启动应用程序前测试 SQLite
            TestSqlite.TestConnection();
        }
        catch (Exception ex)
        {
            MessageBox.Show($"SQLite 初始化失败:\n{ex.Message}\n\n请确保已安装 Visual C++ Redistributable。", 
                          "数据库错误", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return;
        }

        // To customize application configuration such as set high DPI settings or default font,
        // see https://aka.ms/applicationconfiguration.
        ApplicationConfiguration.Initialize();
        Application.Run(new Form1());
    }    
}