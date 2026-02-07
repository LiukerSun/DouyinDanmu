using System;

namespace DouyinDanmu.Models
{
    /// <summary>
    /// 房间连接状态
    /// </summary>
    public enum RoomState
    {
        Disconnected,
        Connecting,
        Connected,
        Reconnecting,
        Error
    }

    /// <summary>
    /// 直播间配置
    /// </summary>
    public class RoomConfig
    {
        /// <summary>
        /// 房间唯一标识（内部使用）
        /// </summary>
        public string RoomId { get; set; } = string.Empty;

        /// <summary>
        /// 直播间ID（用户输入的抖音直播间号）
        /// </summary>
        public string LiveId { get; set; } = string.Empty;

        /// <summary>
        /// 显示名称（Tab标题，为空时使用LiveId）
        /// </summary>
        public string? DisplayName { get; set; }

        /// <summary>
        /// 是否自动连接
        /// </summary>
        public bool AutoConnect { get; set; } = false;

        /// <summary>
        /// 是否启用
        /// </summary>
        public bool Enabled { get; set; } = true;
    }

    /// <summary>
    /// 房间消息事件参数
    /// </summary>
    public class RoomMessageEventArgs : EventArgs
    {
        public string RoomId { get; init; } = string.Empty;
        public LiveMessage Message { get; init; } = null!;
    }

    /// <summary>
    /// 房间状态变更事件参数
    /// </summary>
    public class RoomStateEventArgs : EventArgs
    {
        public string RoomId { get; init; } = string.Empty;
        public RoomState State { get; init; }
        public string? ErrorMessage { get; init; }
    }
}
