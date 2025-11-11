using DouyinDanmu.Models;
using Newtonsoft.Json;
using System;
using System.Buffers;
using System.Collections.Generic;
using System.Net.Http;
using System.Net.WebSockets;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;

namespace DouyinDanmu.Services
{
    /// <summary>
    /// 抖音直播抓取器
    /// </summary>
    public class DouyinLiveFetcher : IDisposable
    {
        private readonly string _liveId;
        private readonly HttpClient _httpClient;
        private readonly SignatureGenerator _signatureGenerator;
        private ClientWebSocket? _webSocket;
        private CancellationTokenSource? _cancellationTokenSource;
        private System.Threading.Timer? _heartbeatTimer;
        private bool _disposed = false;

        // 内存池优化
        private static readonly ArrayPool<byte> _arrayPool = ArrayPool<byte>.Shared;
        private const int BufferSize = 8192;

        private string? _ttwid;
        private string? _roomId;
        private string? _acNonce;

        public event EventHandler<LiveMessage>? MessageReceived;
        public event EventHandler<string>? StatusChanged;
        public event EventHandler<Exception>? ErrorOccurred;

        public DouyinLiveFetcher(string liveId)
        {
            _liveId = liveId;
            _httpClient = new HttpClient();
            _signatureGenerator = new SignatureGenerator();
            
            // 设置User-Agent
            _httpClient.DefaultRequestHeaders.Add("User-Agent", 
                "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
        }

        /// <summary>
        /// 获取ttwid
        /// </summary>
        private async Task<string?> GetTtwidAsync()
        {
            if (!string.IsNullOrEmpty(_ttwid))
                return _ttwid;

            try
            {
                var response = await _httpClient.GetAsync("https://live.douyin.com/").ConfigureAwait(false);
                response.EnsureSuccessStatusCode();

                if (response.Headers.TryGetValues("Set-Cookie", out var cookies))
                {
                    foreach (var cookie in cookies)
                    {
                        var match = Regex.Match(cookie, @"ttwid=([^;]+)");
                        if (match.Success)
                        {
                            _ttwid = match.Groups[1].Value;
                            return _ttwid;
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                ErrorOccurred?.Invoke(this, ex);
            }

            return null;
        }

        private async Task<string?> GetAcNonceAsync()
        {
            if (!string.IsNullOrEmpty(_acNonce))
                return _acNonce;

            try
            {
                var response = await _httpClient.GetAsync("https://www.douyin.com/").ConfigureAwait(false);
                response.EnsureSuccessStatusCode();

                if (response.Headers.TryGetValues("Set-Cookie", out var cookies))
                {
                    foreach (var cookie in cookies)
                    {
                        var match = Regex.Match(cookie, "__ac_nonce=([^;]+)");
                        if (match.Success)
                        {
                            _acNonce = match.Groups[1].Value;
                            return _acNonce;
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                ErrorOccurred?.Invoke(this, ex);
            }
            return null;
        }

        /// <summary>
        /// 获取房间ID
        /// </summary>
        private async Task<string?> GetRoomIdAsync()
        {
            if (!string.IsNullOrEmpty(_roomId))
                return _roomId;

            try
            {
                var ttwid = await GetTtwidAsync().ConfigureAwait(false);
                if (string.IsNullOrEmpty(ttwid))
                    return null;

                var msToken = SignatureGenerator.GenerateMsToken();
                var url = $"https://live.douyin.com/{_liveId}";
                
                var request = new HttpRequestMessage(HttpMethod.Get, url);
                request.Headers.Add("Cookie", $"ttwid={ttwid}; msToken={msToken}; __ac_nonce=0123407cc00a9e438deb4");

                var response = await _httpClient.SendAsync(request).ConfigureAwait(false);
                response.EnsureSuccessStatusCode();

                var content = await response.Content.ReadAsStringAsync().ConfigureAwait(false);
                var match = Regex.Match(content, @"roomId\\"":\\""(\d+)\\""");
                
                if (!match.Success)
                {
                    match = Regex.Match(content, "\"roomId\"\\s*:\\s*\"(\\d+)\"");
                }

                if (match.Success)
                {
                    _roomId = match.Groups[1].Value;
                    return _roomId;
                }
            }
            catch (Exception ex)
            {
                ErrorOccurred?.Invoke(this, ex);
            }

            return null;
        }

        /// <summary>
        /// 获取直播间状态
        /// </summary>
        public async Task<bool> GetRoomStatusAsync()
        {
            try
            {
                var roomId = await GetRoomIdAsync().ConfigureAwait(false);
                var ttwid = await GetTtwidAsync().ConfigureAwait(false);
                var acNonce = await GetAcNonceAsync().ConfigureAwait(false);
                
                if (string.IsNullOrEmpty(roomId) || string.IsNullOrEmpty(ttwid) || string.IsNullOrEmpty(acNonce))
                    return false;

                var msToken2 = SignatureGenerator.GenerateMsToken();
                var ua = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";
                var url = $"https://live.douyin.com/webcast/room/web/enter/?aid=6383" +
                          $"&app_name=douyin_web&live_id=1&device_platform=web&language=zh-CN&enter_from=web_live" +
                          $"&cookie_enabled=true&screen_width=1536&screen_height=864&browser_language=zh-CN&browser_platform=Win32" +
                          $"&browser_name=Edge&browser_version=133.0.0.0" +
                          $"&web_rid={_liveId}" +
                          $"&room_id_str={roomId}" +
                          $"&enter_source=&is_need_double_stream=false&insert_task_id=&live_reason=" +
                         $"&msToken={msToken2}";
                var query = new Uri(url).Query.TrimStart('?');
                var aBogus = _signatureGenerator.GenerateABogus(query, ua);
                url += $"&a_bogus={aBogus}";

                var request = new HttpRequestMessage(HttpMethod.Get, url);
                var acSignature = _signatureGenerator.GenerateAcSignature("www.douyin.com", acNonce!, ua);
                request.Headers.Add("Cookie", $"ttwid={ttwid}; __ac_nonce={acNonce}; __ac_signature={acSignature}");
                request.Headers.Add("Referer", $"https://live.douyin.com/{_liveId}");

                var response = await _httpClient.SendAsync(request).ConfigureAwait(false);
                response.EnsureSuccessStatusCode();

                var content = await response.Content.ReadAsStringAsync().ConfigureAwait(false);
                var jsonResponse = JsonConvert.DeserializeObject<dynamic>(content);

                if (jsonResponse?.data != null)
                {
                    var roomStatus = (int)(jsonResponse.data.room_status ?? 2);
                    var user = jsonResponse.data.user;
                    var userId = user?.id_str?.ToString() ?? "";
                    var nickname = user?.nickname?.ToString() ?? "";

                    var statusText = roomStatus == 0 ? "正在直播" : "已结束";
                    StatusChanged?.Invoke(this, $"【{nickname}】[{userId}]直播间：{statusText}");

                    return roomStatus == 0;
                }
            }
            catch (Exception ex)
            {
                ErrorOccurred?.Invoke(this, ex);
            }

            return false;
        }

        /// <summary>
        /// 开始抓取
        /// </summary>
        public async Task StartAsync()
        {
            try
            {
                _cancellationTokenSource = new CancellationTokenSource();
                
                var roomId = await GetRoomIdAsync().ConfigureAwait(false);
                var ttwid = await GetTtwidAsync().ConfigureAwait(false);
                
                if (string.IsNullOrEmpty(roomId) || string.IsNullOrEmpty(ttwid))
                {
                    throw new InvalidOperationException("无法获取房间信息");
                }

                await ConnectWebSocketAsync(roomId, ttwid).ConfigureAwait(false);
            }
            catch (Exception ex)
            {
                ErrorOccurred?.Invoke(this, ex);
            }
        }

        /// <summary>
        /// 连接WebSocket - 使用内存池优化
        /// </summary>
        private async Task ConnectWebSocketAsync(string roomId, string ttwid)
        {
            try
            {
                var wssUrl = "wss://webcast5-ws-web-hl.douyin.com/webcast/im/push/v2/?" +
                           "app_name=douyin_web&version_code=180800&webcast_sdk_version=1.0.14-beta.0" +
                           "&update_version_code=1.0.14-beta.0&compress=gzip&device_platform=web&cookie_enabled=true" +
                           "&screen_width=1536&screen_height=864&browser_language=zh-CN&browser_platform=Win32" +
                           "&browser_name=Mozilla" +
                           "&browser_version=5.0%20(Windows%20NT%2010.0;%20Win64;%20x64)%20AppleWebKit/537.36%20(KHTML," +
                           "%20like%20Gecko)%20Chrome/126.0.0.0%20Safari/537.36" +
                           "&browser_online=true&tz_name=Asia/Shanghai" +
                           "&cursor=d-1_u-1_fh-7392091211001140287_t-1721106114633_r-1" +
                           $"&internal_ext=internal_src:dim|wss_push_room_id:{roomId}|wss_push_did:7319483754668557238" +
                           $"|first_req_ms:1721106114541|fetch_time:1721106114633|seq:1|wss_info:0-1721106114633-0-0|" +
                           $"wrds_v:7392094459690748497" +
                           $"&host=https://live.douyin.com&aid=6383&live_id=1&did_rule=3&endpoint=live_pc&support_wrds=1" +
                           $"&user_unique_id=7319483754668557238&im_path=/webcast/im/fetch/&identity=audience" +
                           $"&need_persist_msg_count=15&insert_task_id=&live_reason=&room_id={roomId}&heartbeatDuration=0";

                var signature = _signatureGenerator.GenerateSignature(wssUrl);
                wssUrl += $"&signature={signature}";

                _webSocket = new ClientWebSocket();
                _webSocket.Options.SetRequestHeader("Cookie", $"ttwid={ttwid}");
                _webSocket.Options.SetRequestHeader("User-Agent", 
                    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");

                if (_cancellationTokenSource != null)
                {
                    await _webSocket.ConnectAsync(new Uri(wssUrl), _cancellationTokenSource.Token);
                }
                else
                {
                    await _webSocket.ConnectAsync(new Uri(wssUrl), CancellationToken.None);
                }
                
                StatusChanged?.Invoke(this, "WebSocket连接成功");

                // 启动心跳
                StartHeartbeat();

                // 开始接收消息
                _ = Task.Run(ReceiveMessagesAsync);
            }
            catch (Exception ex)
            {
                ErrorOccurred?.Invoke(this, ex);
            }
        }

        /// <summary>
        /// 启动心跳
        /// </summary>
        private void StartHeartbeat()
        {
            _heartbeatTimer = new System.Threading.Timer(async _ =>
            {
                try
                {
                    if (_webSocket?.State == WebSocketState.Open)
                    {
                        var heartbeatData = CreateHeartbeatFrame();
                        await _webSocket.SendAsync(heartbeatData, WebSocketMessageType.Binary, true, CancellationToken.None);
                        StatusChanged?.Invoke(this, "发送心跳包");
                    }
                }
                catch (Exception ex)
                {
                    ErrorOccurred?.Invoke(this, ex);
                }
            }, null, TimeSpan.FromSeconds(1), TimeSpan.FromSeconds(10));
        }

        /// <summary>
        /// 创建心跳帧
        /// </summary>
        private byte[] CreateHeartbeatFrame()
        {
            // 创建符合抖音协议的心跳帧
            // PushFrame with payload_type='hb'
            var frame = new
            {
                seqId = 1UL,
                logId = 1UL,
                service = 1UL,
                method = 1UL,
                payloadType = "hb",
                payload = new byte[0]
            };

            // 简化的protobuf编码
            // 这里使用简化的方式，实际应该使用正确的protobuf编码
            var payloadTypeBytes = Encoding.UTF8.GetBytes("hb");
            var result = new List<byte>();
            
            // 添加字段7 (payloadType) - string
            result.Add(0x3A); // field 7, wire type 2 (length-delimited)
            result.Add((byte)payloadTypeBytes.Length);
            result.AddRange(payloadTypeBytes);
            
            return result.ToArray();
        }

        /// <summary>
        /// 接收消息 - 优化内存使用
        /// </summary>
        private async Task ReceiveMessagesAsync()
        {
            if (_webSocket == null || _cancellationTokenSource == null) return;

            var buffer = _arrayPool.Rent(BufferSize);
            try
            {
                while (_webSocket.State == WebSocketState.Open && !_cancellationTokenSource.Token.IsCancellationRequested)
                {
                    try
                    {
                        var result = await _webSocket.ReceiveAsync(
                            new ArraySegment<byte>(buffer), 
                            _cancellationTokenSource.Token).ConfigureAwait(false);

                        if (result.MessageType == WebSocketMessageType.Binary)
                        {
                            var messageData = new byte[result.Count];
                            Array.Copy(buffer, 0, messageData, 0, result.Count);
                            await ProcessWebSocketMessage(messageData).ConfigureAwait(false);
                        }
                        else if (result.MessageType == WebSocketMessageType.Close)
                        {
                            break;
                        }
                    }
                    catch (OperationCanceledException)
                    {
                        break;
                    }
                    catch (Exception ex)
                    {
                        ErrorOccurred?.Invoke(this, ex);
                        break;
                    }
                }
            }
            finally
            {
                _arrayPool.Return(buffer);
            }
        }

        /// <summary>
        /// 处理WebSocket消息
        /// </summary>
        private async Task ProcessWebSocketMessage(byte[] messageData)
        {
            try
            {
                // 尝试解析为PushFrame
                var (needAck, ackData, messages) = ProtobufParser.ParseWebSocketMessage(messageData);
                
                // 如果需要发送ACK
                if (needAck && ackData != null)
                {
                    await SendAckMessage(ackData);
                }
                
                // 处理解析出的消息
                foreach (var message in messages)
                {
                    MessageReceived?.Invoke(this, message);
                }
            }
            catch (Exception ex)
            {
                StatusChanged?.Invoke(this, $"处理消息失败: {ex.Message}");
            }
        }

        /// <summary>
        /// 发送ACK消息
        /// </summary>
        private async Task SendAckMessage(byte[] ackData)
        {
            try
            {
                if (_webSocket?.State == WebSocketState.Open)
                {
                    await _webSocket.SendAsync(ackData, WebSocketMessageType.Binary, true, CancellationToken.None);
                    StatusChanged?.Invoke(this, "发送ACK确认");
                }
            }
            catch (Exception ex)
            {
                StatusChanged?.Invoke(this, $"发送ACK失败: {ex.Message}");
            }
        }

        /// <summary>
        /// 停止抓取
        /// </summary>
        public async Task StopAsync()
        {
            try
            {
                _cancellationTokenSource?.Cancel();
                _heartbeatTimer?.Dispose();
                
                if (_webSocket?.State == WebSocketState.Open)
                {
                    await _webSocket.CloseAsync(WebSocketCloseStatus.NormalClosure, "停止抓取", CancellationToken.None);
                }
                
                StatusChanged?.Invoke(this, "已停止抓取");
            }
            catch (Exception ex)
            {
                ErrorOccurred?.Invoke(this, ex);
            }
        }

        public void Dispose()
        {
            if (!_disposed)
            {
                _cancellationTokenSource?.Cancel();
                _heartbeatTimer?.Dispose();
                _webSocket?.Dispose();
                _httpClient?.Dispose();
                _signatureGenerator?.Dispose();
                _disposed = true;
            }
        }
    }
} 
