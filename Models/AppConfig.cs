using System;
using System.ComponentModel.DataAnnotations;
using System.Text.Json.Serialization;

namespace DouyinDanmu.Models
{
    /// <summary>
    /// 应用程序配置
    /// </summary>
    public class AppConfig
    {
        /// <summary>
        /// 界面设置
        /// </summary>
        public UISettings UI { get; set; } = new();

        /// <summary>
        /// 性能设置
        /// </summary>
        public PerformanceSettings Performance { get; set; } = new();

        /// <summary>
        /// 数据库设置
        /// </summary>
        public DatabaseSettings Database { get; set; } = new();

        /// <summary>
        /// 网络设置
        /// </summary>
        public NetworkSettings Network { get; set; } = new();

        /// <summary>
        /// 日志设置
        /// </summary>
        public LoggingSettings Logging { get; set; } = new();
    }

    /// <summary>
    /// 界面设置
    /// </summary>
    public class UISettings
    {
        /// <summary>
        /// 最大显示消息数量
        /// </summary>
        [Range(100, 10000)]
        public int MaxDisplayMessages { get; set; } = 1000;

        /// <summary>
        /// UI更新间隔(毫秒)
        /// </summary>
        [Range(50, 1000)]
        public int UpdateIntervalMs { get; set; } = 100;

        /// <summary>
        /// 是否启用自动滚动
        /// </summary>
        public bool AutoScroll { get; set; } = true;

        /// <summary>
        /// 是否显示时间戳
        /// </summary>
        public bool ShowTimestamp { get; set; } = true;

        /// <summary>
        /// 字体大小
        /// </summary>
        [Range(8, 20)]
        public float FontSize { get; set; } = 9.0f;

        /// <summary>
        /// 主题模式
        /// </summary>
        public ThemeMode Theme { get; set; } = ThemeMode.Light;
    }

    /// <summary>
    /// 性能设置
    /// </summary>
    public class PerformanceSettings
    {
        /// <summary>
        /// 批量处理大小
        /// </summary>
        [Range(10, 500)]
        public int BatchSize { get; set; } = 50;

        /// <summary>
        /// 是否启用性能监控
        /// </summary>
        public bool EnablePerformanceMonitoring { get; set; } = true;

        /// <summary>
        /// 性能监控间隔(秒)
        /// </summary>
        [Range(1, 60)]
        public int MonitoringIntervalSeconds { get; set; } = 5;

        /// <summary>
        /// 内存清理阈值(MB)
        /// </summary>
        [Range(50, 1000)]
        public int MemoryCleanupThresholdMB { get; set; } = 200;

        /// <summary>
        /// 是否启用自动GC
        /// </summary>
        public bool EnableAutoGC { get; set; } = true;
    }

    /// <summary>
    /// 数据库设置
    /// </summary>
    public class DatabaseSettings
    {
        /// <summary>
        /// 批量插入大小
        /// </summary>
        [Range(10, 1000)]
        public int BatchInsertSize { get; set; } = 100;

        /// <summary>
        /// 批量插入间隔(毫秒)
        /// </summary>
        [Range(100, 5000)]
        public int BatchInsertIntervalMs { get; set; } = 1000;

        /// <summary>
        /// 数据保留天数
        /// </summary>
        [Range(1, 365)]
        public int DataRetentionDays { get; set; } = 30;

        /// <summary>
        /// 是否启用自动清理
        /// </summary>
        public bool EnableAutoCleanup { get; set; } = true;

        /// <summary>
        /// 连接池大小
        /// </summary>
        [Range(1, 20)]
        public int ConnectionPoolSize { get; set; } = 5;
    }

    /// <summary>
    /// 网络设置
    /// </summary>
    public class NetworkSettings
    {
        /// <summary>
        /// 连接超时(秒)
        /// </summary>
        [Range(5, 60)]
        public int ConnectionTimeoutSeconds { get; set; } = 30;

        /// <summary>
        /// 重连间隔(秒)
        /// </summary>
        [Range(1, 30)]
        public int ReconnectIntervalSeconds { get; set; } = 5;

        /// <summary>
        /// 最大重连次数
        /// </summary>
        [Range(0, 10)]
        public int MaxReconnectAttempts { get; set; } = 3;

        /// <summary>
        /// 心跳间隔(秒)
        /// </summary>
        [Range(10, 120)]
        public int HeartbeatIntervalSeconds { get; set; } = 30;

        /// <summary>
        /// 是否启用代理
        /// </summary>
        public bool EnableProxy { get; set; } = false;

        /// <summary>
        /// 代理地址
        /// </summary>
        public string ProxyAddress { get; set; } = string.Empty;

        /// <summary>
        /// 代理端口
        /// </summary>
        [Range(1, 65535)]
        public int ProxyPort { get; set; } = 8080;
    }

    /// <summary>
    /// 日志设置
    /// </summary>
    public class LoggingSettings
    {
        /// <summary>
        /// 日志级别
        /// </summary>
        public LogLevel Level { get; set; } = LogLevel.Information;

        /// <summary>
        /// 是否启用文件日志
        /// </summary>
        public bool EnableFileLogging { get; set; } = true;

        /// <summary>
        /// 日志文件最大大小(MB)
        /// </summary>
        [Range(1, 100)]
        public int MaxFileSizeMB { get; set; } = 10;

        /// <summary>
        /// 日志文件保留数量
        /// </summary>
        [Range(1, 20)]
        public int MaxFileCount { get; set; } = 5;

        /// <summary>
        /// 是否启用控制台日志
        /// </summary>
        public bool EnableConsoleLogging { get; set; } = false;
    }

    /// <summary>
    /// 主题模式
    /// </summary>
    [JsonConverter(typeof(JsonStringEnumConverter))]
    public enum ThemeMode
    {
        Light,
        Dark,
        Auto
    }

    /// <summary>
    /// 日志级别
    /// </summary>
    [JsonConverter(typeof(JsonStringEnumConverter))]
    public enum LogLevel
    {
        Trace,
        Debug,
        Information,
        Warning,
        Error,
        Critical
    }
} 