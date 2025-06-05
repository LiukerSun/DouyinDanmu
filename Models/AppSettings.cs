using System.Collections.Generic;

namespace DouyinDanmu.Models
{
    /// <summary>
    /// 应用程序设置模型
    /// </summary>
    public class AppSettings
    {
        /// <summary>
        /// 直播间ID
        /// </summary>
        public string LiveId { get; set; } = "";

        /// <summary>
        /// 关注的用户ID列表
        /// </summary>
        public List<string> WatchedUserIds { get; set; } = new List<string>();

        /// <summary>
        /// 用户信息字典（用户ID -> 用户信息）
        /// </summary>
        public Dictionary<string, UserInfo> UserInfos { get; set; } = new Dictionary<string, UserInfo>();

        /// <summary>
        /// 窗口位置X
        /// </summary>
        public int WindowX { get; set; } = -1;

        /// <summary>
        /// 窗口位置Y
        /// </summary>
        public int WindowY { get; set; } = -1;

        /// <summary>
        /// 窗口宽度
        /// </summary>
        public int WindowWidth { get; set; } = 1600;

        /// <summary>
        /// 窗口高度
        /// </summary>
        public int WindowHeight { get; set; } = 596;

        /// <summary>
        /// 窗口状态（最大化、最小化等）
        /// </summary>
        public int WindowState { get; set; } = 0; // 0=Normal, 1=Minimized, 2=Maximized

        /// <summary>
        /// 是否自动滚动
        /// </summary>
        public bool AutoScroll { get; set; } = true;

        /// <summary>
        /// 是否启用WebSocket服务
        /// </summary>
        public bool WebSocketEnabled { get; set; } = false;

        /// <summary>
        /// WebSocket服务端口
        /// </summary>
        public int WebSocketPort { get; set; } = 8080;

        /// <summary>
        /// 是否在连接时自动启动WebSocket服务
        /// </summary>
        public bool AutoStartWebSocket { get; set; } = false;
    }
} 