using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.WebSockets;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using DouyinDanmu.Models;

namespace DouyinDanmu.Services
{
    /// <summary>
    /// WebSocket客户端连接信息
    /// </summary>
    public class WebSocketClient
    {
        public WebSocket WebSocket { get; set; }
        public string Id { get; set; }
        public HashSet<string> Subscriptions { get; set; }
        public DateTime ConnectedAt { get; set; }

        public WebSocketClient(WebSocket webSocket)
        {
            WebSocket = webSocket;
            Id = Guid.NewGuid().ToString();
            Subscriptions = new HashSet<string>();
            ConnectedAt = DateTime.Now;
        }
    }

    /// <summary>
    /// WebSocket消息格式
    /// </summary>
    public class WebSocketMessage
    {
        public string Type { get; set; } = string.Empty;
        public DateTime Timestamp { get; set; }
        public object Data { get; set; } = new();
    }

    /// <summary>
    /// WebSocket服务器
    /// </summary>
    public class WebSocketService : IDisposable
    {
        private HttpListener? _httpListener;
        private readonly ConcurrentDictionary<string, WebSocketClient> _clients = new();
        private CancellationTokenSource? _cancellationTokenSource;
        private bool _isRunning = false;
        private readonly object _lockObject = new object();

        public int Port { get; private set; }
        public bool IsRunning => _isRunning;
        public int ConnectedClientsCount => _clients.Count;

        public event EventHandler<string>? StatusChanged;
        public event EventHandler<Exception>? ErrorOccurred;

        /// <summary>
        /// 启动WebSocket服务器
        /// </summary>
        public async Task StartAsync(int port)
        {
            if (_isRunning)
            {
                await Task.CompletedTask;
                return;
            }

            try
            {
                Port = port;
                _cancellationTokenSource = new CancellationTokenSource();

                _httpListener = new HttpListener();
                _httpListener.Prefixes.Add($"http://localhost:{port}/");
                _httpListener.Start();

                lock (_lockObject)
                {
                    _isRunning = true;
                }

                StatusChanged?.Invoke(this, $"WebSocket服务已启动，端口: {port}");

                // 开始监听连接
                _ = Task.Run(async () =>
                    await AcceptWebSocketRequestsAsync(_cancellationTokenSource.Token)
                );

                await Task.CompletedTask;
            }
            catch (Exception ex)
            {
                lock (_lockObject)
                {
                    _isRunning = false;
                }
                ErrorOccurred?.Invoke(this, ex);
                throw;
            }
        }

        /// <summary>
        /// 停止WebSocket服务器
        /// </summary>
        public async Task StopAsync()
        {
            if (!_isRunning)
                return;

            try
            {
                lock (_lockObject)
                {
                    _isRunning = false;
                }

                _cancellationTokenSource?.Cancel();

                // 关闭所有客户端连接
                var closeOptions = new List<Task>();
                foreach (var client in _clients.Values.ToList())
                {
                    if (client.WebSocket.State == WebSocketState.Open)
                    {
                        closeOptions.Add(CloseClientAsync(client));
                    }
                }

                if (closeOptions.Count > 0)
                {
                    await Task.WhenAll(closeOptions);
                }

                _clients.Clear();
                _httpListener?.Stop();
                _httpListener?.Close();

                StatusChanged?.Invoke(this, "WebSocket服务已停止");
            }
            catch (Exception ex)
            {
                ErrorOccurred?.Invoke(this, ex);
            }
        }

        /// <summary>
        /// 接受WebSocket连接请求
        /// </summary>
        private async Task AcceptWebSocketRequestsAsync(CancellationToken cancellationToken)
        {
            while (!cancellationToken.IsCancellationRequested && _httpListener != null)
            {
                try
                {
                    var context = await _httpListener.GetContextAsync();

                    if (context.Request.IsWebSocketRequest)
                    {
                        _ = Task.Run(async () => await ProcessWebSocketAsync(context));
                    }
                    else
                    {
                        // 返回WebSocket服务信息页面
                        await SendInfoPageAsync(context.Response);
                    }
                }
                catch (ObjectDisposedException)
                {
                    // HttpListener已被释放，正常退出
                    break;
                }
                catch (Exception ex)
                {
                    if (!cancellationToken.IsCancellationRequested)
                    {
                        ErrorOccurred?.Invoke(this, ex);
                    }
                }
            }
        }

        /// <summary>
        /// 发送服务信息页面
        /// </summary>
        private async Task SendInfoPageAsync(HttpListenerResponse response)
        {
            try
            {
                var html =
                    $@"
<!DOCTYPE html>
<html>
<head>
    <meta charset='utf-8'>
    <title>抖音弹幕WebSocket服务</title>
    <style>
        body {{ font-family: Arial, sans-serif; margin: 40px; }}
        .info {{ background: #f5f5f5; padding: 20px; border-radius: 8px; }}
        .endpoint {{ background: #e8f4fd; padding: 10px; border-radius: 4px; font-family: monospace; }}
        .example {{ background: #fff; border: 1px solid #ddd; padding: 15px; border-radius: 4px; margin: 10px 0; }}
    </style>
</head>
<body>
    <h1>抖音弹幕WebSocket服务</h1>
    <div class='info'>
        <h2>服务状态</h2>
        <p>端口: {Port}</p>
        <p>在线客户端: {ConnectedClientsCount}</p>
        <p>服务时间: {DateTime.Now:yyyy-MM-dd HH:mm:ss}</p>
    </div>
    
    <h2>连接方式</h2>
    <div class='endpoint'>ws://localhost:{Port}/?type=chat,gift</div>
    
    <h2>支持的订阅类型</h2>
    <ul>
        <li><code>chat</code> - 弹幕消息</li>
        <li><code>gift</code> - 礼物消息</li>
        <li><code>like</code> - 点赞消息</li>
        <li><code>member</code> - 进场消息</li>
        <li><code>social</code> - 关注消息</li>
        <li><code>all</code> - 所有消息</li>
    </ul>
    
    <h2>使用示例</h2>
    <div class='example'>
        <h3>JavaScript客户端</h3>
        <pre><code>const ws = new WebSocket('ws://localhost:{Port}/?type=chat,gift');
ws.onmessage = function(event) {{
    const message = JSON.parse(event.data);
    console.log('收到消息:', message);
}};
ws.onopen = function() {{
    console.log('WebSocket连接已建立');
}};
ws.onclose = function() {{
    console.log('WebSocket连接已关闭');
}};</code></pre>
    </div>
</body>
</html>";

                var buffer = Encoding.UTF8.GetBytes(html);
                response.ContentType = "text/html; charset=utf-8";
                response.ContentLength64 = buffer.Length;
                await response.OutputStream.WriteAsync(buffer, 0, buffer.Length);
                response.Close();
            }
            catch (Exception ex)
            {
                ErrorOccurred?.Invoke(this, ex);
            }
        }

        /// <summary>
        /// 处理WebSocket连接
        /// </summary>
        private async Task ProcessWebSocketAsync(HttpListenerContext context)
        {
            WebSocketClient? client = null;
            try
            {
                StatusChanged?.Invoke(
                    this,
                    $"新的WebSocket连接请求，来自: {context.Request.RemoteEndPoint}"
                );
                
                var webSocketContext = await context.AcceptWebSocketAsync(null);
                var webSocket = webSocketContext.WebSocket;
                
                client = new WebSocketClient(webSocket);
                
                // 解析订阅类型
                var query = context.Request.Url?.Query;
                if (!string.IsNullOrEmpty(query))
                {
                    // 手动解析查询字符串，避免依赖System.Web
                    var typeParam = ParseQueryParameter(query, "type");
                    if (!string.IsNullOrEmpty(typeParam))
                    {
                        var types = typeParam.Split(',', StringSplitOptions.RemoveEmptyEntries);
                        foreach (var type in types)
                        {
                            client.Subscriptions.Add(type.Trim().ToLower());
                        }
                    }
                }
                
                // 如果没有指定订阅类型，默认订阅所有消息
                if (client.Subscriptions.Count == 0)
                {
                    client.Subscriptions.Add("all");
                }

                // 先添加到客户端列表，确保在发送消息前已注册
                _clients.TryAdd(client.Id, client);
                StatusChanged?.Invoke(
                    this,
                    $"新客户端连接成功: {client.Id}, 订阅: {string.Join(",", client.Subscriptions)}, 总客户端数: {_clients.Count}"
                );

                // 发送欢迎消息
                StatusChanged?.Invoke(this, $"准备发送欢迎消息给客户端 {client.Id}");
                await SendWelcomeMessageAsync(client);
                StatusChanged?.Invoke(this, $"欢迎消息发送完成，客户端 {client.Id}");

                // 开始监听客户端消息 - 这里是阻塞调用，直到连接关闭
                StatusChanged?.Invoke(this, $"开始监听客户端 {client.Id} 的消息");
                await ListenToClientAsync(client);
                StatusChanged?.Invoke(this, $"客户端 {client.Id} 监听正常结束");
            }
            catch (Exception ex)
            {
                StatusChanged?.Invoke(this, $"处理WebSocket连接时发生错误: {ex.Message}");
                ErrorOccurred?.Invoke(this, ex);
            }
            finally
            {
                // 清理工作
                if (client != null)
                {
                    // 确保客户端从列表中移除
                    _clients.TryRemove(client.Id, out _);
                    StatusChanged?.Invoke(
                        this,
                        $"客户端断开连接: {client.Id}, 剩余客户端数: {_clients.Count}"
                    );
                    
                    // 尝试优雅关闭连接（只有在连接还开着的情况下）
                    try
                    {
                        if (client.WebSocket.State == WebSocketState.Open)
                        {
                            StatusChanged?.Invoke(this, $"正在关闭客户端 {client.Id} 的连接");
                            await client.WebSocket.CloseAsync(
                                WebSocketCloseStatus.NormalClosure,
                                "服务器关闭连接",
                                CancellationToken.None
                            );
                            StatusChanged?.Invoke(this, $"客户端 {client.Id} 连接已关闭");
                        }
                        else
                        {
                            StatusChanged?.Invoke(this, $"客户端 {client.Id} 连接状态: {client.WebSocket.State}，无需关闭");
                        }
                    }
                    catch (Exception closeEx)
                    {
                        StatusChanged?.Invoke(
                            this,
                            $"关闭客户端 {client.Id} 连接时发生错误: {closeEx.Message}"
                        );
                    }
                }
            }
        }

        /// <summary>
        /// 发送欢迎消息
        /// </summary>
        private async Task SendWelcomeMessageAsync(WebSocketClient client)
        {
            try
            {
                // 发送欢迎消息
                var welcomeMessage = new WebSocketMessage
                {
                    Type = "welcome",
                    Timestamp = DateTime.Now,
                    Data = new
                    {
                        clientId = client.Id,
                        subscriptions = client.Subscriptions.ToArray(),
                        message = "WebSocket连接成功",
                        serverInfo = new
                        {
                            version = "1.0",
                            supportsPing = true,
                            heartbeatInterval = 300000 // 5分钟心跳间隔
                        }
                    },
                };

                await SendMessageToClientAsync(client, welcomeMessage);
                
                // 稍等一下再发送member消息，避免消息发送过快
                await Task.Delay(100);
                
                // 发送member消息（模拟客户端期望的消息格式）
                var memberMessage = new WebSocketMessage
                {
                    Type = "member",
                    Timestamp = DateTime.Now,
                    Data = new
                    {
                        message = "连接已建立",
                        status = "connected"
                    }
                };

                await SendMessageToClientAsync(client, memberMessage);
                
                StatusChanged?.Invoke(this, $"欢迎消息和状态消息已发送给客户端 {client.Id}");
            }
            catch (Exception ex)
            {
                StatusChanged?.Invoke(this, $"发送欢迎消息给客户端 {client.Id} 失败: {ex.Message}");
                throw;
            }
        }

        /// <summary>
        /// 监听客户端消息
        /// </summary>
        private async Task ListenToClientAsync(WebSocketClient client)
        {
            var buffer = new byte[1024 * 4];
            
            try
            {
                StatusChanged?.Invoke(this, $"开始监听客户端 {client.Id} 的消息");
                
                while (client.WebSocket.State == WebSocketState.Open && _isRunning)
                {
                    try
                    {
                        // 减少状态日志输出，避免日志过多
                        // StatusChanged?.Invoke(this, $"客户端 {client.Id} 准备接收消息，当前状态: {client.WebSocket.State}");
                        
                        // 使用较长的超时时间，减少心跳包频率
                        using var cts = new CancellationTokenSource(TimeSpan.FromMinutes(5)); // 5分钟超时
                        var result = await client.WebSocket.ReceiveAsync(
                            new ArraySegment<byte>(buffer),
                            cts.Token
                        );
                        
                        // 减少消息接收日志，只在收到特殊消息时记录
                        if (result.MessageType == WebSocketMessageType.Close)
                        {
                            StatusChanged?.Invoke(
                                this,
                                $"客户端 {client.Id} 发送关闭消息: {result.CloseStatus} - {result.CloseStatusDescription}"
                            );
                            
                            // 客户端主动关闭，我们也响应关闭
                            if (client.WebSocket.State == WebSocketState.Open || client.WebSocket.State == WebSocketState.CloseReceived)
                            {
                                await client.WebSocket.CloseAsync(
                                    WebSocketCloseStatus.NormalClosure,
                                    "响应客户端关闭",
                                    CancellationToken.None
                                );
                            }
                            break; // 退出监听循环
                        }
                        else if (result.MessageType == WebSocketMessageType.Text)
                        {
                            var message = Encoding.UTF8.GetString(buffer, 0, result.Count);
                            
                            // 处理ping消息
                            if (message.Contains("\"type\":\"ping\""))
                            {
                                StatusChanged?.Invoke(this, $"收到客户端 {client.Id} 心跳包，回复pong");
                                try
                                {
                                    var pongMessage = JsonSerializer.Serialize(new { 
                                        type = "pong", 
                                        timestamp = DateTime.Now,
                                        message = "服务器心跳回复"
                                    });
                                    var pongBuffer = Encoding.UTF8.GetBytes(pongMessage);
                                    await client.WebSocket.SendAsync(
                                        new ArraySegment<byte>(pongBuffer),
                                        WebSocketMessageType.Text,
                                        true,
                                        CancellationToken.None
                                    );
                                    StatusChanged?.Invoke(this, $"已向客户端 {client.Id} 回复pong");
                                }
                                catch (Exception pongEx)
                                {
                                    StatusChanged?.Invoke(this, $"向客户端 {client.Id} 发送pong失败: {pongEx.Message}");
                                }
                            }
                            else
                            {
                                StatusChanged?.Invoke(this, $"收到客户端 {client.Id} 消息: {message}");
                                await HandleClientMessageAsync(client, message);
                            }
                        }
                        else if (result.MessageType == WebSocketMessageType.Binary)
                        {
                            StatusChanged?.Invoke(this, $"客户端 {client.Id} 发送二进制数据，长度: {result.Count}");
                        }
                        else
                        {
                            StatusChanged?.Invoke(this, $"客户端 {client.Id} 发送未知消息类型: {result.MessageType}");
                        }
                    }
                    catch (WebSocketException wsEx)
                    {
                        StatusChanged?.Invoke(
                            this,
                            $"客户端 {client.Id} WebSocket异常: {wsEx.Message} (错误代码: {wsEx.WebSocketErrorCode})"
                        );
                        break; // 退出监听循环
                    }
                    catch (OperationCanceledException)
                    {
                        // 超时处理 - 发送心跳包到客户端
                        if (client.WebSocket.State == WebSocketState.Open)
                        {
                            StatusChanged?.Invoke(this, $"客户端 {client.Id} 接收超时，发送心跳包");
                            try
                            {
                                var pingMessage = JsonSerializer.Serialize(new { 
                                    type = "ping", 
                                    timestamp = DateTime.Now,
                                    message = "服务器心跳包"
                                });
                                var pingBuffer = Encoding.UTF8.GetBytes(pingMessage);
                                await client.WebSocket.SendAsync(
                                    new ArraySegment<byte>(pingBuffer),
                                    WebSocketMessageType.Text,
                                    true,
                                    CancellationToken.None
                                );
                                StatusChanged?.Invoke(this, $"已向客户端 {client.Id} 发送心跳包");
                            }
                            catch (Exception pingEx)
                            {
                                StatusChanged?.Invoke(this, $"向客户端 {client.Id} 发送心跳包失败: {pingEx.Message}");
                                break; // 发送失败，退出循环
                            }
                        }
                        else
                        {
                            StatusChanged?.Invoke(this, $"客户端 {client.Id} 连接已关闭，停止监听");
                            break;
                        }
                        // 不退出循环，继续监听
                    }
                    catch (Exception ex)
                    {
                        StatusChanged?.Invoke(
                            this,
                            $"客户端 {client.Id} 处理消息时发生未知错误: {ex.Message} (类型: {ex.GetType().Name})"
                        );
                        ErrorOccurred?.Invoke(this, ex);
                        break; // 发生未知错误，退出监听循环
                    }
                }
                
                StatusChanged?.Invoke(
                    this,
                    $"客户端 {client.Id} 监听循环结束，连接状态: {client.WebSocket.State}"
                );
            }
            catch (Exception ex)
            {
                StatusChanged?.Invoke(this, $"监听客户端 {client.Id} 时发生严重错误: {ex.Message}");
                ErrorOccurred?.Invoke(this, ex);
            }
        }

        /// <summary>
        /// 处理客户端消息
        /// </summary>
        private async Task HandleClientMessageAsync(WebSocketClient client, string message)
        {
            try
            {
                var clientMessage = JsonSerializer.Deserialize<dynamic>(message);
                // 这里可以实现更多的客户端交互功能
                // 比如动态修改订阅类型等
                await Task.CompletedTask;
            }
            catch (Exception ex)
            {
                ErrorOccurred?.Invoke(this, ex);
            }
        }

        /// <summary>
        /// 广播直播消息给订阅的客户端
        /// </summary>
        public async Task BroadcastLiveMessageAsync(LiveMessage liveMessage)
        {
            if (!_isRunning || _clients.IsEmpty)
                return;

            var messageType = GetMessageTypeString(liveMessage.Type);
            var wsMessage = new WebSocketMessage
            {
                Type = messageType,
                Timestamp = liveMessage.Timestamp,
                Data = CreateMessageData(liveMessage),
            };

            var tasks = new List<Task>();

            foreach (var client in _clients.Values.ToList())
            {
                if (ShouldSendMessage(client, messageType))
                {
                    tasks.Add(SendMessageToClientAsync(client, wsMessage));
                }
            }

            if (tasks.Count > 0)
            {
                await Task.WhenAll(tasks);
            }
        }

        /// <summary>
        /// 判断是否应该发送消息给客户端
        /// </summary>
        private bool ShouldSendMessage(WebSocketClient client, string messageType)
        {
            return client.WebSocket.State == WebSocketState.Open
                && (
                    client.Subscriptions.Contains("all")
                    || client.Subscriptions.Contains(messageType)
                );
        }

        /// <summary>
        /// 获取消息类型字符串
        /// </summary>
        private string GetMessageTypeString(LiveMessageType type)
        {
            return type switch
            {
                LiveMessageType.Chat => "chat",
                LiveMessageType.Gift => "gift",
                LiveMessageType.Like => "like",
                LiveMessageType.Member => "member",
                LiveMessageType.Social => "social",
                LiveMessageType.RoomStats => "roomstats",
                LiveMessageType.Fansclub => "fansclub",
                LiveMessageType.Control => "control",
                LiveMessageType.EmojiChat => "emojichat",
                LiveMessageType.RoomInfo => "roominfo",
                LiveMessageType.Rank => "rank",
                _ => "unknown",
            };
        }

        /// <summary>
        /// 创建消息数据对象
        /// </summary>
        private object CreateMessageData(LiveMessage message)
        {
            var baseData = new
            {
                content = message.Content,
                userId = message.UserId,
                userName = message.UserName,
                gender = message.Gender,
                fansClubLevel = message.FansClubLevel,
                payGradeLevel = message.PayGradeLevel,
            };

            return message switch
            {
                GiftMessage gift => new
                {
                    content = baseData.content,
                    userId = baseData.userId,
                    userName = baseData.userName,
                    gender = baseData.gender,
                    fansClubLevel = baseData.fansClubLevel,
                    payGradeLevel = baseData.payGradeLevel,
                    giftName = gift.GiftName,
                    giftCount = gift.GiftCount,
                },
                LikeMessage like => new
                {
                    content = baseData.content,
                    userId = baseData.userId,
                    userName = baseData.userName,
                    gender = baseData.gender,
                    fansClubLevel = baseData.fansClubLevel,
                    payGradeLevel = baseData.payGradeLevel,
                    likeCount = like.LikeCount,
                },
                RoomStatsMessage stats => new
                {
                    content = baseData.content,
                    userId = baseData.userId,
                    userName = baseData.userName,
                    gender = baseData.gender,
                    fansClubLevel = baseData.fansClubLevel,
                    payGradeLevel = baseData.payGradeLevel,
                    currentViewers = stats.CurrentViewers,
                    totalViewers = stats.TotalViewers,
                },
                ControlMessage control => new
                {
                    content = baseData.content,
                    userId = baseData.userId,
                    userName = baseData.userName,
                    gender = baseData.gender,
                    fansClubLevel = baseData.fansClubLevel,
                    payGradeLevel = baseData.payGradeLevel,
                    status = control.Status,
                },
                _ => baseData,
            };
        }

        /// <summary>
        /// 发送消息给指定客户端
        /// </summary>
        private async Task SendMessageToClientAsync(
            WebSocketClient client,
            WebSocketMessage message
        )
        {
            if (client.WebSocket.State != WebSocketState.Open)
            {
                StatusChanged?.Invoke(
                    this,
                    $"跳过发送消息给客户端 {client.Id}，连接状态: {client.WebSocket.State}"
                );
                return;
            }

            const int maxRetries = 3;
            int retryCount = 0;

            while (retryCount < maxRetries)
            {
                try
                {
                    var json = JsonSerializer.Serialize(
                        message,
                        new JsonSerializerOptions { PropertyNamingPolicy = JsonNamingPolicy.CamelCase }
                    );
                    var buffer = Encoding.UTF8.GetBytes(json);

                    // 使用较短的超时时间发送消息
                    using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(10));
                    await client.WebSocket.SendAsync(
                        new ArraySegment<byte>(buffer),
                        WebSocketMessageType.Text,
                        true,
                        cts.Token
                    );

                    // 只在发送特殊消息时记录日志，避免过多日志
                    if (message.Type == "welcome" || message.Type == "member" || message.Type == "ping" || message.Type == "pong")
                    {
                        StatusChanged?.Invoke(this, $"已发送{message.Type}消息给客户端 {client.Id}");
                    }
                    
                    return; // 发送成功，退出重试循环
                }
                catch (WebSocketException wsEx)
                {
                    retryCount++;
                    StatusChanged?.Invoke(
                        this,
                        $"发送消息给客户端 {client.Id} 失败 (WebSocket) - 尝试 {retryCount}/{maxRetries}: {wsEx.Message} (错误代码: {wsEx.WebSocketErrorCode})"
                    );
                    
                    if (retryCount >= maxRetries)
                    {
                        // 重试次数用完，移除客户端
                        _clients.TryRemove(client.Id, out _);
                        break;
                    }
                    
                    // 等待一小段时间再重试
                    await Task.Delay(100);
                }
                catch (OperationCanceledException)
                {
                    retryCount++;
                    StatusChanged?.Invoke(this, $"发送消息给客户端 {client.Id} 超时 - 尝试 {retryCount}/{maxRetries}");
                    
                    if (retryCount >= maxRetries)
                    {
                        _clients.TryRemove(client.Id, out _);
                        break;
                    }
                    
                    await Task.Delay(100);
                }
                catch (Exception ex)
                {
                    StatusChanged?.Invoke(this, $"发送消息给客户端 {client.Id} 失败: {ex.Message}");
                    // 对于其他异常，直接移除客户端，不重试
                    _clients.TryRemove(client.Id, out _);
                    break;
                }
            }
        }

        /// <summary>
        /// 关闭客户端连接
        /// </summary>
        private async Task CloseClientAsync(WebSocketClient client)
        {
            try
            {
                if (client.WebSocket.State == WebSocketState.Open)
                {
                    await client.WebSocket.CloseAsync(
                        WebSocketCloseStatus.NormalClosure,
                        "服务器关闭",
                        CancellationToken.None
                    );
                }
            }
            catch (Exception)
            {
                // 忽略关闭时的异常
            }
        }

        /// <summary>
        /// 获取服务状态信息
        /// </summary>
        public object GetServiceInfo()
        {
            return new
            {
                isRunning = _isRunning,
                port = Port,
                connectedClients = ConnectedClientsCount,
                clients = _clients
                    .Values.Select(c => new
                    {
                        id = c.Id,
                        subscriptions = c.Subscriptions.ToArray(),
                        connectedAt = c.ConnectedAt,
                        state = c.WebSocket.State.ToString(),
                    })
                    .ToArray(),
            };
        }

        public void Dispose()
        {
            _ = Task.Run(async () => await StopAsync());
            _cancellationTokenSource?.Dispose();
            _httpListener?.Close();
        }

        /// <summary>
        /// 手动解析查询参数
        /// </summary>
        private string? ParseQueryParameter(string query, string paramName)
        {
            if (string.IsNullOrEmpty(query) || string.IsNullOrEmpty(paramName))
                return null;

            // 移除开头的 '?' 如果存在
            if (query.StartsWith("?"))
                query = query.Substring(1);

            var pairs = query.Split('&', StringSplitOptions.RemoveEmptyEntries);
            foreach (var pair in pairs)
            {
                var keyValue = pair.Split('=', 2);
                if (
                    keyValue.Length == 2
                    && string.Equals(keyValue[0], paramName, StringComparison.OrdinalIgnoreCase)
                )
                {
                    return Uri.UnescapeDataString(keyValue[1]);
                }
            }

            return null;
        }
    }
}
