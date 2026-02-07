using DouyinDanmu.Models;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace DouyinDanmu.Services
{
    /// <summary>
    /// 单个房间的连接封装
    /// </summary>
    internal class RoomConnection : IDisposable
    {
        public string RoomId { get; }
        public string LiveId { get; }
        public DouyinLiveFetcher? Fetcher { get; set; }
        public CancellationTokenSource? CancellationTokenSource { get; set; }
        public int ReconnectAttempts { get; set; }
        public bool IsConnected { get; set; }
        public bool IsConnecting { get; set; }
        public ConnectionState State { get; set; } = ConnectionState.Disconnected;
        private readonly SemaphoreSlim _operationLock = new(1, 1);
        private bool _disposed;

        public RoomConnection(string roomId, string liveId)
        {
            RoomId = roomId;
            LiveId = liveId;
        }

        public async Task<IDisposable> LockAsync()
        {
            await _operationLock.WaitAsync().ConfigureAwait(false);
            return new LockReleaser(_operationLock);
        }

        public void Dispose()
        {
            if (!_disposed)
            {
                CancellationTokenSource?.Cancel();
                CancellationTokenSource?.Dispose();
                Fetcher?.Dispose();
                _operationLock.Dispose();
                _disposed = true;
            }
        }

        private sealed class LockReleaser(SemaphoreSlim semaphore) : IDisposable
        {
            private readonly SemaphoreSlim _semaphore = semaphore;
            public void Dispose() => _semaphore.Release();
        }
    }

    /// <summary>
    /// 多房间连接管理服务
    /// </summary>
    public class ConnectionManager : IDisposable
    {
        private readonly NetworkSettings _settings;
        private readonly LoggingService _logger;
        private readonly SignatureGenerator _sharedSignatureGenerator;
        private readonly ConcurrentDictionary<string, RoomConnection> _rooms = new();
        private bool _disposed;

        public event EventHandler<ConnectionStateChangedEventArgs>? ConnectionStateChanged;
        public event EventHandler<RoomMessageEventArgs>? RoomMessageReceived;
        public event EventHandler<string>? StatusChanged;

        // Backward compatibility: single-room message event
        public event EventHandler<LiveMessage>? MessageReceived;

        public ConnectionManager(NetworkSettings settings, LoggingService logger)
        {
            _settings = settings;
            _logger = logger;
            _sharedSignatureGenerator = new SignatureGenerator();
        }

        /// <summary>
        /// 获取所有房间ID
        /// </summary>
        public IReadOnlyCollection<string> GetAllRoomIds()
        {
            return _rooms.Keys.ToList().AsReadOnly();
        }

        /// <summary>
        /// 获取房间连接状态
        /// </summary>
        public ConnectionState GetRoomState(string roomId)
        {
            if (_rooms.TryGetValue(roomId, out var room))
                return room.State;
            return ConnectionState.Disconnected;
        }

        /// <summary>
        /// 房间是否已连接
        /// </summary>
        public bool IsRoomConnected(string roomId)
        {
            if (_rooms.TryGetValue(roomId, out var room))
                return room.IsConnected;
            return false;
        }

        /// <summary>
        /// 是否有任何房间已连接
        /// </summary>
        public bool HasAnyConnection => _rooms.Values.Any(r => r.IsConnected);

        /// <summary>
        /// 添加房间并连接
        /// </summary>
        public async Task<bool> ConnectAsync(string roomId, string liveId)
        {
            if (_rooms.Count >= _settings.MaxConcurrentRooms)
            {
                _logger.LogWarning($"已达到最大房间数 ({_settings.MaxConcurrentRooms})，无法添加新房间", category: "ConnectionManager");
                return false;
            }

            var room = _rooms.GetOrAdd(roomId, _ => new RoomConnection(roomId, liveId));

            using (await room.LockAsync().ConfigureAwait(false))
            {
                if (room.IsConnecting)
                {
                    _logger.LogWarning($"房间 {liveId} 正在连接中，忽略重复请求", category: "ConnectionManager");
                    return false;
                }

                if (room.IsConnected)
                {
                    _logger.LogInformation($"房间 {liveId} 已连接", category: "ConnectionManager");
                    return true;
                }

                room.IsConnecting = true;
                try
                {
                    room.ReconnectAttempts = 0;
                    room.CancellationTokenSource = new CancellationTokenSource();
                    return await ConnectRoomInternalAsync(room).ConfigureAwait(false);
                }
                finally
                {
                    room.IsConnecting = false;
                }
            }
        }

        /// <summary>
        /// 向后兼容：单房间连接（使用liveId作为roomId）
        /// </summary>
        public Task<bool> ConnectAsync(string liveId)
        {
            return ConnectAsync(liveId, liveId);
        }

        /// <summary>
        /// 断开指定房间
        /// </summary>
        public async Task DisconnectAsync(string roomId)
        {
            if (!_rooms.TryGetValue(roomId, out var room))
                return;

            using (await room.LockAsync().ConfigureAwait(false))
            {
                await DisconnectRoomInternalAsync(room, "用户主动断开").ConfigureAwait(false);
            }
        }

        /// <summary>
        /// 向后兼容：断开所有连接
        /// </summary>
        public async Task DisconnectAsync()
        {
            await DisconnectAllAsync().ConfigureAwait(false);
        }

        /// <summary>
        /// 断开所有房间
        /// </summary>
        public async Task DisconnectAllAsync()
        {
            var tasks = _rooms.Keys.Select(roomId => DisconnectAsync(roomId));
            await Task.WhenAll(tasks).ConfigureAwait(false);
        }

        /// <summary>
        /// 移除房间（断开并从列表中删除）
        /// </summary>
        public async Task RemoveRoomAsync(string roomId)
        {
            if (_rooms.TryRemove(roomId, out var room))
            {
                using (await room.LockAsync().ConfigureAwait(false))
                {
                    await DisconnectRoomInternalAsync(room, "房间已移除").ConfigureAwait(false);
                }
                room.Dispose();
            }
        }

        /// <summary>
        /// 连接所有已添加的房间
        /// </summary>
        public async Task ConnectAllAsync()
        {
            var tasks = _rooms.Values.Select(room => ConnectAsync(room.RoomId, room.LiveId));
            await Task.WhenAll(tasks).ConfigureAwait(false);
        }

        /// <summary>
        /// 内部连接逻辑
        /// </summary>
        private async Task<bool> ConnectRoomInternalAsync(RoomConnection room)
        {
            try
            {
                _logger.LogInformation($"尝试连接到直播间: {room.LiveId} (roomId: {room.RoomId})", category: "ConnectionManager");
                room.State = ConnectionState.Connecting;
                OnConnectionStateChanged(room.RoomId, ConnectionState.Connecting, "正在连接...");

                // 清理旧的fetcher
                if (room.Fetcher != null)
                {
                    DetachFetcherEvents(room);
                    room.Fetcher.Dispose();
                }

                // 创建新的fetcher（使用共享的SignatureGenerator）
                room.Fetcher = new DouyinLiveFetcher(room.LiveId, _sharedSignatureGenerator);
                AttachFetcherEvents(room);

                // 设置超时
                using var timeoutCts = new CancellationTokenSource(TimeSpan.FromSeconds(_settings.ConnectionTimeoutSeconds));
                using var combinedCts = CancellationTokenSource.CreateLinkedTokenSource(
                    room.CancellationTokenSource?.Token ?? CancellationToken.None,
                    timeoutCts.Token);

                var success = await room.Fetcher.ConnectAsync(combinedCts.Token).ConfigureAwait(false);

                if (success)
                {
                    room.IsConnected = true;
                    room.ReconnectAttempts = 0;
                    room.State = ConnectionState.Connected;
                    OnConnectionStateChanged(room.RoomId, ConnectionState.Connected, "连接成功");
                    _logger.LogInformation($"成功连接到直播间: {room.LiveId}", category: "ConnectionManager");
                    return true;
                }
                else
                {
                    _logger.LogWarning($"连接失败: {room.LiveId}", category: "ConnectionManager");
                    room.State = ConnectionState.Failed;
                    OnConnectionStateChanged(room.RoomId, ConnectionState.Failed, "连接失败");
                    _ = ScheduleReconnectAsync(room);
                    return false;
                }
            }
            catch (OperationCanceledException)
            {
                _logger.LogInformation($"房间 {room.LiveId} 连接操作被取消", category: "ConnectionManager");
                room.State = ConnectionState.Cancelled;
                OnConnectionStateChanged(room.RoomId, ConnectionState.Cancelled, "连接被取消");
                return false;
            }
            catch (Exception ex)
            {
                _logger.LogError($"房间 {room.LiveId} 连接异常: {ex.Message}", ex, "ConnectionManager");
                room.State = ConnectionState.Failed;
                OnConnectionStateChanged(room.RoomId, ConnectionState.Failed, $"连接异常: {ex.Message}");
                _ = ScheduleReconnectAsync(room);
                return false;
            }
        }

        /// <summary>
        /// 内部断开逻辑
        /// </summary>
        private async Task DisconnectRoomInternalAsync(RoomConnection room, string reason)
        {
            _logger.LogInformation($"断开房间 {room.LiveId}: {reason}", category: "ConnectionManager");

            room.CancellationTokenSource?.Cancel();

            if (room.Fetcher != null)
            {
                DetachFetcherEvents(room);
                try
                {
                    await room.Fetcher.StopAsync().ConfigureAwait(false);
                }
                catch { /* ignore stop errors */ }
                room.Fetcher.Dispose();
                room.Fetcher = null;
            }

            if (room.IsConnected)
            {
                room.IsConnected = false;
                room.State = ConnectionState.Disconnected;
                OnConnectionStateChanged(room.RoomId, ConnectionState.Disconnected, reason);
            }

            _logger.LogInformation($"房间 {room.LiveId} 已断开", category: "ConnectionManager");
        }

        /// <summary>
        /// 安排重连
        /// </summary>
        private async Task ScheduleReconnectAsync(RoomConnection room)
        {
            if (room.ReconnectAttempts >= _settings.MaxReconnectAttempts)
            {
                _logger.LogWarning($"房间 {room.LiveId} 达到最大重连次数 ({_settings.MaxReconnectAttempts})", category: "ConnectionManager");
                room.State = ConnectionState.Failed;
                OnConnectionStateChanged(room.RoomId, ConnectionState.Failed, "达到最大重连次数");
                return;
            }

            room.ReconnectAttempts++;
            var delay = TimeSpan.FromSeconds(_settings.ReconnectIntervalSeconds * room.ReconnectAttempts);

            _logger.LogInformation($"房间 {room.LiveId} 将在 {delay.TotalSeconds}s 后第 {room.ReconnectAttempts} 次重连", category: "ConnectionManager");
            room.State = ConnectionState.Reconnecting;
            OnConnectionStateChanged(room.RoomId, ConnectionState.Reconnecting, $"第 {room.ReconnectAttempts} 次重连 ({delay.TotalSeconds}s)");

            try
            {
                await Task.Delay(delay, room.CancellationTokenSource?.Token ?? CancellationToken.None).ConfigureAwait(false);
            }
            catch (OperationCanceledException)
            {
                return; // reconnect cancelled
            }

            if (!_disposed && _rooms.ContainsKey(room.RoomId))
            {
                await ConnectRoomInternalAsync(room).ConfigureAwait(false);
            }
        }

        /// <summary>
        /// 绑定fetcher事件
        /// </summary>
        private void AttachFetcherEvents(RoomConnection room)
        {
            if (room.Fetcher == null) return;
            room.Fetcher.MessageReceived += (s, msg) => OnFetcherMessageReceived(room, msg);
            room.Fetcher.StatusChanged += (s, status) => OnFetcherStatusChanged(room, status);
            room.Fetcher.ErrorOccurred += (s, ex) => OnFetcherErrorOccurred(room, ex);
        }

        /// <summary>
        /// 解绑fetcher事件
        /// </summary>
        private void DetachFetcherEvents(RoomConnection room)
        {
            // Since we use lambdas, we dispose the fetcher to stop events
            // The fetcher will be disposed after detach
        }

        private void OnFetcherMessageReceived(RoomConnection room, LiveMessage message)
        {
            message.RoomId = room.RoomId;
            RoomMessageReceived?.Invoke(this, new RoomMessageEventArgs
            {
                RoomId = room.RoomId,
                Message = message
            });
            // Backward compatibility
            MessageReceived?.Invoke(this, message);
        }

        private void OnFetcherStatusChanged(RoomConnection room, string status)
        {
            StatusChanged?.Invoke(this, $"[{room.LiveId}] {status}");
        }

        private async void OnFetcherErrorOccurred(RoomConnection room, Exception exception)
        {
            _logger.LogError($"房间 {room.LiveId} 错误: {exception.Message}", exception, "ConnectionManager");

            if (room.IsConnected)
            {
                room.IsConnected = false;
                room.State = ConnectionState.Failed;
                OnConnectionStateChanged(room.RoomId, ConnectionState.Failed, $"连接错误: {exception.Message}");
                await ScheduleReconnectAsync(room).ConfigureAwait(false);
            }
        }

        private void OnConnectionStateChanged(string roomId, ConnectionState state, string message)
        {
            ConnectionStateChanged?.Invoke(this, new ConnectionStateChangedEventArgs(roomId, state, message));
        }

        public void Dispose()
        {
            if (!_disposed)
            {
                _disposed = true;
                foreach (var room in _rooms.Values)
                {
                    room.Dispose();
                }
                _rooms.Clear();
                _sharedSignatureGenerator.Dispose();
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
        public string RoomId { get; }
        public ConnectionState State { get; }
        public string Message { get; }
        public DateTime Timestamp { get; }

        public ConnectionStateChangedEventArgs(ConnectionState state, string message)
            : this(string.Empty, state, message) { }

        public ConnectionStateChangedEventArgs(string roomId, ConnectionState state, string message)
        {
            RoomId = roomId;
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
