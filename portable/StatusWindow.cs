using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Net.Http;
using System.Threading;
using System.Web.Script.Serialization;
using System.Windows.Forms;

internal sealed class StatusWindow : Form
{
    private sealed class Instance
    {
        public int pid { get; set; }
        public int port { get; set; }
        public int? controlPort { get; set; }
        public string controlToken { get; set; }
    }

    private readonly Process owner;
    private readonly string statePath;
    private readonly Label status = new Label();
    private readonly Label address = new Label();
    private readonly Button open = new Button();
    private readonly NotifyIcon tray = new NotifyIcon();
    private readonly ContextMenuStrip menu = new ContextMenuStrip();
    private readonly System.Windows.Forms.Timer timer = new System.Windows.Forms.Timer { Interval = 400 };
    private readonly ToolStripItem openItem;
    private Instance instance;
    private bool serviceReady, stopping, terminating, restoring;

    [STAThread]
    private static int Main(string[] args)
    {
        int pid;
        if (args.Length < 2 || !int.TryParse(args[1], out pid)) return 1;
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        try {
            using (var owner = Process.GetProcessById(pid))
            using (var window = new StatusWindow(args[0], owner, args.Length == 3 && args[2] == "--managed"))
                Application.Run(window);
        } catch (ArgumentException) { return 1; } // The launcher already exited.
        return 0;
    }

    private StatusWindow(string root, Process process, bool managed)
    {
        owner = process;
        serviceReady = !managed;
        statePath = Path.Combine(root, "data", ".runtime", "instance.json");
        Text = "抖音直播台";
        Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath);
        Font = SystemFonts.MessageBoxFont;
        AutoScaleMode = AutoScaleMode.Dpi;
        ClientSize = new Size(344, 150);
        FormBorderStyle = FormBorderStyle.FixedSingle;
        MaximizeBox = false;
        StartPosition = FormStartPosition.CenterScreen;

        status.SetBounds(20, 18, 304, 25);
        status.Font = new Font(Font.FontFamily, 12, FontStyle.Bold);
        status.Text = "正在启动服务…";
        address.SetBounds(21, 52, 302, 25);
        open.SetBounds(19, 98, 124, 32);
        open.Text = "打开工作台";
        open.Enabled = false;
        open.Click += delegate { OpenWorkbench(); };
        var minimize = new Button { Text = "最小化到托盘" };
        minimize.SetBounds(155, 98, 145, 32);
        minimize.Click += delegate { WindowState = FormWindowState.Minimized; };
        Controls.AddRange(new Control[] { status, address, open, minimize });

        menu.Items.Add("显示运行窗口", null, delegate { RestoreWindow(); });
        openItem = menu.Items.Add("打开工作台", null, delegate { OpenWorkbench(); });
        openItem.Enabled = false;
        menu.Items.Add(new ToolStripSeparator());
        menu.Items.Add("退出直播台", null, delegate { StopService(); });
        tray.Icon = Icon;
        tray.Text = "抖音直播台 · 正在启动";
        tray.ContextMenuStrip = menu;
        tray.DoubleClick += delegate { RestoreWindow(); };
        Resize += delegate {
            if (restoring || WindowState != FormWindowState.Minimized) return;
            tray.Visible = true;
            Hide();
        };
        FormClosing += (sender, e) => {
            if (terminating || e.CloseReason == CloseReason.WindowsShutDown) return;
            e.Cancel = true;
            StopService();
        };
        Shown += delegate {
            RefreshStatus();
            timer.Start();
            if (managed) {
                var reader = new Thread(() => {
                    try {
                        string line;
                        while ((line = Console.In.ReadLine()) != null) {
                            if (line == "ready" && IsHandleCreated)
                                BeginInvoke((Action)(() => { serviceReady = true; RefreshStatus(); }));
                        }
                        // The launcher closes this pipe on normal shutdown and on update.
                        if (IsHandleCreated) BeginInvoke((Action)Finish);
                    } catch (IOException) { } catch (InvalidOperationException) { }
                });
                reader.IsBackground = true;
                reader.Start();
            }
        };
        timer.Tick += delegate { RefreshStatus(); };
    }

    private void RefreshStatus()
    {
        if (owner.HasExited) { Finish(); return; }
        if (open.Enabled || stopping) return;
        try {
            if (instance == null) {
                Instance state;
                using (var file = new FileStream(statePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete))
                using (var reader = new StreamReader(file))
                    state = new JavaScriptSerializer().Deserialize<Instance>(reader.ReadToEnd());
                if (state == null || state.pid != owner.Id || state.controlPort == null) return;
                instance = state;
            }
            if (!serviceReady) return;
            address.Text = "http://localhost:" + instance.port;
            status.Text = "服务运行中";
            tray.Text = "抖音直播台 · 服务运行中";
            open.Enabled = openItem.Enabled = true;
        } catch (IOException) { } catch (ArgumentException) { } catch (InvalidOperationException) { }
    }

    private void RestoreWindow()
    {
        restoring = true;
        try {
            Show();
            WindowState = FormWindowState.Normal;
            Activate();
            tray.Visible = false;
        } finally { restoring = false; }
    }

    private void OpenWorkbench()
    {
        if (!open.Enabled) return;
        try {
            Process.Start(new ProcessStartInfo(address.Text) { UseShellExecute = true });
        } catch (System.ComponentModel.Win32Exception error) {
            RestoreWindow();
            MessageBox.Show(this, error.Message, "打开工作台失败", MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }
    }

    private async void StopService()
    {
        if (stopping || terminating) return;
        RefreshStatus();
        if (instance == null || terminating) return;
        stopping = true;
        status.Text = "正在停止服务…";
        open.Enabled = openItem.Enabled = false;
        try {
            using (var client = new HttpClient(new HttpClientHandler { UseProxy = false, AllowAutoRedirect = false })) {
                client.Timeout = TimeSpan.FromSeconds(5);
                client.DefaultRequestHeaders.Add("X-Control-Token", instance.controlToken);
                using (var response = await client.PostAsync("http://127.0.0.1:" + instance.controlPort + "/stop", new ByteArrayContent(new byte[0])))
                    response.EnsureSuccessStatusCode();
            }
        } catch (Exception error) {
            if (terminating || IsDisposed || owner.HasExited) return;
            stopping = false;
            RestoreWindow();
            RefreshStatus();
            MessageBox.Show(this, error.Message, "停止服务失败", MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }
    }

    private void Finish()
    {
        if (terminating) return;
        terminating = true;
        tray.Visible = false;
        Close();
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing) { timer.Dispose(); tray.Dispose(); menu.Dispose(); }
        base.Dispose(disposing);
    }
}
