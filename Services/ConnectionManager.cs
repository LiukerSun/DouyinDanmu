using DouyinDanmu.Models;
using System;
using System.Threading;
using System.Threading.Tasks;

namespace DouyinDanmu.Services
{
    /// <summary>
    /// 连接管理服务
    /// </summary>
    public class ConnectionManager : IDisposable
    {
        private readonly NetworkSettings _settings;
        private readonly LoggingService _logger;
        private DouyinLiveFetcher? _fetcher;
        private CancellationTokenSource? _cancellationTokenSource;
        private System.Threading.Timer? _reconnectTimer;
        private int _reconnectAttempts = 0;
        private bool _disposed = false;
        private bool _isConnecting = false;

        public event EventHandler<ConnectionStateChangedEventArgs>? ConnectionStateChanged;
        public event EventHandler<LiveMessage>? MessageReceived;
        public event EventHandler<string>? StatusChanged;

        public bool IsConnected { get; private set; }
        public string? CurrentLiveId { get; private set; }

        public ConnectionManager(NetworkSettings settings, LoggingService logger)
        {
            _settings = settings;
            _logger = logger;
        }

        /// <summary>
        /// 连接到直播间
        /// </summary>
        public async Task<bool> ConnectAsync(string liveId)
        {
            if (_isConnecting)
            {
                _logger.LogWarning("正在连接中，忽略重复连接请求", category: "ConnectionManager");
                return false;
            }

            if (IsConnected && CurrentLiveId == liveId)
            {
                _logger.LogInformation("已连接到相同直播间，无需重复连接", category: "ConnectionManager");
                return true;
            }

            _isConnecting = true;
            try
            {
                // 断开现有连接
                await DisconnectAsync().ConfigureAwait(false);

                CurrentLiveId = liveId;
                _reconnectAttempts = 0;
                _cancellationTokenSource = new CancellationTokenSource();

                return await ConnectInternalAsync().ConfigureAwait(false);
            }
            finally
            {
                _isConnecting = false;
            }
        }

        /// <summary>
        /// 断开连接
        /// </summary>
        public async Task DisconnectAsync()
        {
            _logger.LogInformation("开始断开连接", category: "ConnectionManager");

            // 停止重连定时器
            _reconnectTimer?.Dispose();
            _reconnectTimer = null;

            // 取消操作
            _cancellationTokenSource?.Cancel();

            // 释放fetcher
            if (_fetcher != null)
            {
                _fetcher.MessageReceived -= OnMessageReceived;
                _fetcher.StatusChanged -= OnStatusChanged;
                _fetcher.ErrorOccurred -= OnErrorOccurred;
                _fetcher.Dispose();
                _fetcher = null;
            }

            // 更新状态
            if (IsConnected)
            {
                IsConnected = false;
                OnConnectionStateChanged(ConnectionState.Disconnected, "用户主动断开");
            }

            _logger.LogInformation("连接已断开", category: "ConnectionManager");
            
            // 添加一个小延迟确保所有操作完成
            await Task.Delay(100).ConfigureAwait(false);
        }

        /// <summary>
        /// 内部连接逻辑
        /// </summary>
        private async Task<bool> ConnectInternalAsync()
        {
            if (string.IsNullOrEmpty(CurrentLiveId))
                return false;

            try
            {
                _logger.LogInformation($"尝试连接到直播间: {CurrentLiveId}", category: "ConnectionManager");
                OnConnectionStateChanged(ConnectionState.Connecting, "正在连接...");

                // 创建新的fetcher
                _fetcher = new DouyinLiveFetcher(CurrentLiveId);
                _fetcher.MessageReceived += OnMessageReceived;
                _fetcher.StatusChanged += OnStatusChanged;
                _fetcher.ErrorOccurred += OnErrorOccurred;

                // 设置超时
                using var timeoutCts = new CancellationTokenSource(TimeSpan.FromSeconds(_settings.ConnectionTimeoutSeconds));
                using var combinedCts = CancellationTokenSource.CreateLinkedTokenSource(
                    _cancellationTokenSource?.Token ?? CancellationToken.None,
                    timeoutCts.Token);

                // 尝试连接
                var success = await _fetcher.ConnectAsync(combinedCts.Token).ConfigureAwait(false);

                if (success)
                {
                    IsConnected = true;
                    _reconnectAttempts = 0;
                    OnConnectionStateChanged(ConnectionState.Connected, "连接成功");
                    _logger.LogInformation($"成功连接到直播间: {CurrentLiveId}", category: "ConnectionManager");
                    return true;
                }
                else
                {
                    _logger.LogWarning($"连接失败: {CurrentLiveId}", category: "ConnectionManager");
                    OnConnectionStateChanged(ConnectionState.Failed, "连接失败");
                    await ScheduleReconnectAsync().ConfigureAwait(false);
                    return false;
                }
            }
            catch (OperationCanceledException)
            {
                _logger.LogInformation("连接操作被取消", category: "ConnectionManager");
                OnConnectionStateChanged(ConnectionState.Cancelled, "连接被取消");
                return false;
            }
            catch (Exception ex)
            {
                _logger.LogError($"连接时发生异常: {ex.Message}", ex, "ConnectionManager");
                OnConnectionStateChanged(ConnectionState.Failed, $"连接异常: {ex.Message}");
                await ScheduleReconnectAsync().ConfigureAwait(false);
                return false;
            }
        }

        /// <summary>
        /// 安排重连
        /// </summary>
        private async Task ScheduleReconnectAsync()
        {
            if (_reconnectAttempts >= _settings.MaxReconnectAttempts)
            {
                _logger.LogWarning($"达到最大重连次数 ({_settings.MaxReconnectAttempts})，停止重连", category: "ConnectionManager");
                OnConnectionStateChanged(ConnectionState.Failed, "达到最大重连次数");
                return;
            }

            _reconnectAttempts++;
            var delay = TimeSpan.FromSeconds(_settings.ReconnectIntervalSeconds * _reconnectAttempts); // 指数退避
            
            _logger.LogInformation($"将在 {delay.TotalSeconds} 秒后进行第 {_reconnectAttempts} 次重连", category: "ConnectionManager");
            OnConnectionStateChanged(ConnectionState.Reconnecting, $"第 {_reconnectAttempts} 次重连 ({delay.TotalSeconds}s)");

            // 使用Task.Delay代替Timer来实现异步延迟
            await Task.Delay(delay).ConfigureAwait(false);
            
            if (!_disposed && !string.IsNullOrEmpty(CurrentLiveId))
            {
                await ConnectInternalAsync().ConfigureAwait(false);
            }
        }

        /// <summary>
        /// 处理消息接收
        /// </summary>
        private void OnMessageReceived(object? sender, LiveMessage message)
        {
            MessageReceived?.Invoke(this, message);
        }

        /// <summary>
        /// 处理状态变化
        /// </summary>
        private void OnStatusChanged(object? sender, string status)
        {
            StatusChanged?.Invoke(this, status);
        }

        /// <summary>
        /// 处理错误
        /// </summary>
        private async void OnErrorOccurred(object? sender, Exception exception)
        {
            _logger.LogError($"连接发生错误: {exception.Message}", exception, "ConnectionManager");
            
            if (IsConnected)
            {
                IsConnected = false;
                OnConnectionStateChanged(ConnectionState.Failed, $"连接错误: {exception.Message}");
                await ScheduleReconnectAsync().ConfigureAwait(false);
            }
        }

        /// <summary>
        /// 触发连接状态变化事件
        /// </summary>
        private void OnConnectionStateChanged(ConnectionState state, string message)
        {
            ConnectionStateChanged?.Invoke(this, new ConnectionStateChangedEventArgs(state, message));
        }

        public void Dispose()
        {
            if (!_disposed)
            {
                DisconnectAsync().Wait(5000); // 最多等待5秒
                _cancellationTokenSource?.Dispose();
                _disposed = true;
            }
        }
    }

    /// <summary>
    /// 连接状态
    /// </summary>
    public enum ConnectionState
    {
        Disconnected,
        Connecting,
        Connected,
        Reconnecting,
        Failed,
        Cancelled
    }

    /// <summary>
    /// 连接状态变化事件参数
    /// </summary>
    public class ConnectionStateChangedEventArgs : EventArgs
    {
        public ConnectionState State { get; }
        public string Message { get; }
        public DateTime Timestamp { get; }

        public ConnectionStateChangedEventArgs(ConnectionState state, string message)
        {
            State = state;
            Message = message;
            Timestamp = DateTime.Now;
        }
    }

    /// <summary>
    /// DouyinLiveFetcher扩展方法
    /// </summary>
    public static class DouyinLiveFetcherExtensions
    {
        public static async Task<bool> ConnectAsync(this DouyinLiveFetcher fetcher, CancellationToken cancellationToken = default)
        {
            try
            {
                await fetcher.StartAsync().ConfigureAwait(false);
                return true;
            }
            catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
            {
                return false;
            }
            catch
            {
                return false;
            }
        }
    }
} 