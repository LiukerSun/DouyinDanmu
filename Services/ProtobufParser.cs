using DouyinDanmu.Models;
using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Text;
using System.Collections.Concurrent;

namespace DouyinDanmu.Services
{
    /// <summary>
    /// Protocol Buffer消息解析器
    /// </summary>
    public static class ProtobufParser
    {
        // 记录遇到的未知消息类型
        private static readonly ConcurrentDictionary<string, int> _unknownMessageTypes = new();
        
        /// <summary>
        /// 获取遇到的未知消息类型统计
        /// </summary>
        public static Dictionary<string, int> GetUnknownMessageTypes()
        {
            return new Dictionary<string, int>(_unknownMessageTypes);
        }

        /// <summary>
        /// 解析WebSocket消息
        /// </summary>
        /// <param name="data">原始数据</param>
        /// <returns>元组：(是否需要ACK, ACK数据, 解析出的消息列表)</returns>
        public static (bool needAck, byte[]? ackData, List<LiveMessage> messages) ParseWebSocketMessage(byte[] data)
        {
            var messages = new List<LiveMessage>();
            bool needAck = false;
            byte[]? ackData = null;

            try
            {
                // 解析PushFrame
                var (logId, payload) = ParsePushFrame(data);
                
                if (payload != null && payload.Length > 0)
                {
                    // 尝试解压缩
                    byte[] decompressedData;
                    try
                    {
                        decompressedData = DecompressGzip(payload);
                    }
                    catch
                    {
                        decompressedData = payload;
                    }

                    // 解析Response
                    var (responseNeedAck, internalExt, messagesList) = ParseResponse(decompressedData);
                    
                    if (responseNeedAck && !string.IsNullOrEmpty(internalExt))
                    {
                        needAck = true;
                        ackData = CreateAckFrame(logId, internalExt);
                    }

                    // 解析消息列表
                    foreach (var msgData in messagesList)
                    {
                        var message = ParseMessage(msgData);
                        if (message != null)
                        {
                            messages.Add(message);
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                // 解析失败时返回空结果
                Console.WriteLine($"解析消息失败: {ex.Message}");
            }

            return (needAck, ackData, messages);
        }

        /// <summary>
        /// 解析PushFrame
        /// </summary>
        private static (ulong logId, byte[]? payload) ParsePushFrame(byte[] data)
        {
            ulong logId = 0;
            byte[]? payload = null;

            try
            {
                using var stream = new MemoryStream(data);
                using var reader = new BinaryReader(stream);

                while (stream.Position < stream.Length)
                {
                    var tag = ReadVarint(reader);
                    var fieldNumber = tag >> 3;
                    var wireType = tag & 0x7;

                    switch (fieldNumber)
                    {
                        case 2: // logId
                            if (wireType == 0) // varint
                            {
                                logId = ReadVarint(reader);
                            }
                            break;
                        case 8: // payload
                            if (wireType == 2) // length-delimited
                            {
                                var length = (int)ReadVarint(reader);
                                payload = reader.ReadBytes(length);
                            }
                            break;
                        default:
                            SkipField(reader, (int)wireType);
                            break;
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"解析PushFrame失败: {ex.Message}");
            }

            return (logId, payload);
        }

        /// <summary>
        /// 解析Response
        /// </summary>
        private static (bool needAck, string? internalExt, List<byte[]> messagesList) ParseResponse(byte[] data)
        {
            bool needAck = false;
            string? internalExt = null;
            var messagesList = new List<byte[]>();

            try
            {
                using var stream = new MemoryStream(data);
                using var reader = new BinaryReader(stream);

                while (stream.Position < stream.Length)
                {
                    var tag = ReadVarint(reader);
                    var fieldNumber = tag >> 3;
                    var wireType = tag & 0x7;

                    switch (fieldNumber)
                    {
                        case 1: // messagesList
                            if (wireType == 2) // length-delimited
                            {
                                var length = (int)ReadVarint(reader);
                                var messageData = reader.ReadBytes(length);
                                messagesList.Add(messageData);
                            }
                            break;
                        case 5: // internalExt
                            if (wireType == 2) // length-delimited
                            {
                                var length = (int)ReadVarint(reader);
                                var bytes = reader.ReadBytes(length);
                                internalExt = Encoding.UTF8.GetString(bytes);
                            }
                            break;
                        case 9: // needAck
                            if (wireType == 0) // varint
                            {
                                needAck = ReadVarint(reader) != 0;
                            }
                            break;
                        default:
                            SkipField(reader, (int)wireType);
                            break;
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"解析Response失败: {ex.Message}");
            }

            return (needAck, internalExt, messagesList);
        }

        /// <summary>
        /// 创建ACK帧
        /// </summary>
        private static byte[] CreateAckFrame(ulong logId, string internalExt)
        {
            var result = new List<byte>
            {
                // logId (field 2)
                0x10 // field 2, wire type 0 (varint)
            };
            result.AddRange(EncodeVarint(logId));

            // payloadType (field 7)
            var ackBytes = Encoding.UTF8.GetBytes("ack");
            result.Add(0x3A); // field 7, wire type 2 (length-delimited)
            result.Add((byte)ackBytes.Length);
            result.AddRange(ackBytes);

            // payload (field 8)
            var payloadBytes = Encoding.UTF8.GetBytes(internalExt);
            result.Add(0x42); // field 8, wire type 2 (length-delimited)
            result.AddRange(EncodeVarint((ulong)payloadBytes.Length));
            result.AddRange(payloadBytes);

            return [.. result];
        }

        /// <summary>
        /// 解析单个消息
        /// </summary>
        private static LiveMessage? ParseMessage(byte[] data)
        {
            try
            {
                using var stream = new MemoryStream(data);
                using var reader = new BinaryReader(stream);

                string? method = null;
                byte[]? payload = null;

                while (stream.Position < stream.Length)
                {
                    var tag = ReadVarint(reader);
                    var fieldNumber = tag >> 3;
                    var wireType = tag & 0x7;

                    switch (fieldNumber)
                    {
                        case 1: // method
                            if (wireType == 2) // length-delimited
                            {
                                var length = (int)ReadVarint(reader);
                                var bytes = reader.ReadBytes(length);
                                method = Encoding.UTF8.GetString(bytes);
                            }
                            break;
                        case 2: // payload
                            if (wireType == 2) // length-delimited
                            {
                                var length = (int)ReadVarint(reader);
                                payload = reader.ReadBytes(length);
                            }
                            break;
                        default:
                            SkipField(reader, (int)wireType);
                            break;
                    }
                }

                if (!string.IsNullOrEmpty(method) && payload != null)
                {
                    return ParseSpecificMessage(method, payload);
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"解析消息失败: {ex.Message}");
            }

            return null;
        }

        /// <summary>
        /// 解析特定类型的消息
        /// </summary>
        private static LiveMessage? ParseSpecificMessage(string method, byte[] payload)
        {
            try
            {
                switch (method)
                {
                    // 重要的用户交互消息 - 显示
                    case "WebcastChatMessage":
                        return ParseChatMessage(payload);
                    case "WebcastGiftMessage":
                        return ParseGiftMessage(payload);
                    case "WebcastLikeMessage":
                        return ParseLikeMessage(payload);
                    case "WebcastMemberMessage":
                        return ParseMemberMessage(payload);
                    case "WebcastSocialMessage":
                        return ParseSocialMessage(payload);
                    case "WebcastEmojiChatMessage":
                        return ParseEmojiChatMessage(payload);
                    case "WebcastFansclubMessage":
                        return ParseFansclubMessage(payload);
                    
                    // 统计信息消息 - 不显示（根据用户要求过滤）
                    case "WebcastRoomUserSeqMessage":
                    case "WebcastRoomStatsMessage":
                        return null; // 过滤掉统计类型消息
                    
                    // 控制消息 - 显示（重要）
                    case "WebcastControlMessage":
                        return ParseControlMessage(payload);
                    
                    // 系统消息 - 可选显示
                    case "WebcastRoomMessage":
                        return ParseRoomMessage(payload);
                    case "WebcastRoomRankMessage":
                        return ParseRoomRankMessage(payload);
                    
                    // 技术性消息 - 不显示（过滤掉）
                    case "WebcastRoomStreamAdaptationMessage":
                    case "WebcastRoomDataSyncMessage":
                    case "WebcastProfitGameStatusMessage":
                    case "WebcastInRoomBannerMessage":
                    case "WebcastGiftSortMessage":
                    case "WebcastLiveShoppingMessage":
                    case "WebcastProductChangeMessage":
                    case "WebcastUpdateFanTicketMessage":
                    case "WebcastCommonTextMessage":
                    case "WebcastMatchAgainstScoreMessage":
                    case "WebcastEpisodeChatMessage":
                    case "WebcastPreMessage":
                    case "WebcastLandscapeAreaCommon":
                    case "WebcastPublicAreaCommon":
                    case "WebcastTextEffect":
                    case "WebcastEffectConfig":
                    case "WebcastImageContent":
                    case "WebcastNinePatchSetting":
                    case "WebcastHeadersList":
                    case "WebcastExtList":
                    case "WebcastSendMessageBody":
                    case "WebcastRsp":
                    case "WebcastKk":
                    case "WebcastPushFrame":
                    case "WebcastRanklistHourEntranceMessage":  // 排行榜小时入口消息
                    // 广告和推广相关
                    case "WebcastAdMessage":
                    case "WebcastPromotionMessage":
                    case "WebcastBannerMessage":
                    case "WebcastShoppingMessage":
                    // 游戏和互动相关
                    case "WebcastGameMessage":
                    case "WebcastInteractMessage":
                    case "WebcastActivityMessage":
                    // 直播间配置相关
                    case "WebcastConfigMessage":
                    case "WebcastSettingMessage":
                    case "WebcastLayoutMessage":
                    // 数据同步相关
                    case "WebcastSyncMessage":
                    case "WebcastUpdateMessage":
                    case "WebcastRefreshMessage":
                    // 其他技术性消息
                    case "WebcastHeartbeatMessage":
                    case "WebcastPingMessage":
                    case "WebcastPongMessage":
                    case "WebcastAckMessage":
                    case "WebcastErrorMessage":
                    case "WebcastWarningMessage":
                    case "WebcastDebugMessage":
                    case "WebcastLogMessage":
                    case "WebcastMetricsMessage":
                    case "WebcastAnalyticsMessage":
                    case "WebcastTrackingMessage":
                    case "WebcastMonitorMessage":
                    case "WebcastStatusMessage":
                    case "WebcastHealthMessage":
                    case "WebcastDiagnosticMessage":
                        return null; // 不显示这些消息
                    
                    // 未知消息类型 - 根据用户要求不显示
                    default:
                        // 记录未知消息类型
                        _unknownMessageTypes.AddOrUpdate(method, 1, (key, value) => value + 1);
                        
                        // 如果是明显的技术性或系统消息，不显示
                        if (method.Contains("Sync") || method.Contains("Update") || method.Contains("Config") ||
                            method.Contains("Setting") || method.Contains("Layout") || method.Contains("Refresh") ||
                            method.Contains("Heartbeat") || method.Contains("Ping") || method.Contains("Pong") ||
                            method.Contains("Ack") || method.Contains("Error") || method.Contains("Warning") ||
                            method.Contains("Debug") || method.Contains("Log") || method.Contains("Metrics") ||
                            method.Contains("Analytics") || method.Contains("Tracking") || method.Contains("Monitor") ||
                            method.Contains("Status") || method.Contains("Health") || method.Contains("Diagnostic") ||
                            method.Contains("Banner") || method.Contains("Ad") || method.Contains("Promotion") ||
                            method.Contains("Shopping") || method.Contains("Game") || method.Contains("Activity") ||
                            method.Contains("Interact") || method.Contains("Stream") || method.Contains("Adaptation"))
                        {
                            // 输出调试信息但不显示消息
                            Console.WriteLine($"[过滤] 技术性消息: {method}");
                            return null;
                        }
                        
                        // 输出调试信息但不显示未知消息（根据用户要求）
                        Console.WriteLine($"[过滤] 未知消息类型: {method} (出现次数: {_unknownMessageTypes[method]})");
                        return null; // 不显示未知消息
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"解析{method}消息失败: {ex.Message}");
                return null;
            }
        }

        /// <summary>
        /// 解析聊天消息
        /// </summary>
        private static ChatMessage? ParseChatMessage(byte[] payload)
        {
            try
            {
                using var stream = new MemoryStream(payload);
                using var reader = new BinaryReader(stream);

                string? content = null;
                string? userName = null;
                string? userId = null;
                int fansClubLevel = 0;
                int payGradeLevel = 0;

                while (stream.Position < stream.Length)
                {
                    var tag = ReadVarint(reader);
                    var fieldNumber = tag >> 3;
                    var wireType = tag & 0x7;

                    switch (fieldNumber)
                    {
                        case 2: // user
                            if (wireType == 2) // length-delimited
                            {
                                var length = (int)ReadVarint(reader);
                                var userData = reader.ReadBytes(length);
                                var user = ParseUser(userData);
                                if (user != null)
                                {
                                    userName = user.NickName;
                                    userId = user.IdStr;
                                    fansClubLevel = user.FansClubLevel;
                                    payGradeLevel = user.PayGradeLevel;
                                }
                            }
                            break;
                        case 3: // content
                            if (wireType == 2) // length-delimited
                            {
                                var length = (int)ReadVarint(reader);
                                var bytes = reader.ReadBytes(length);
                                content = Encoding.UTF8.GetString(bytes);
                            }
                            break;
                        default:
                            SkipField(reader, (int)wireType);
                            break;
                    }
                }

                return new ChatMessage
                {
                    Content = content ?? "聊天消息内容",
                    UserName = userName ?? "用户名",
                    UserId = userId ?? "123456",
                    FansClubLevel = fansClubLevel,
                    PayGradeLevel = payGradeLevel
                };
            }
            catch (Exception ex)
            {
                Console.WriteLine($"解析聊天消息失败: {ex.Message}");
                return new ChatMessage
                {
                    Content = "聊天消息内容",
                    UserName = "用户名",
                    UserId = "123456",
                    FansClubLevel = 0,
                    PayGradeLevel = 0
                };
            }
        }

        /// <summary>
        /// 解析礼物消息
        /// </summary>
        private static GiftMessage? ParseGiftMessage(byte[] payload)
        {
            try
            {
                using var stream = new MemoryStream(payload);
                using var reader = new BinaryReader(stream);

                string? userName = null;
                string? userId = null;
                string? giftName = null;
                int giftCount = 1;
                int fansClubLevel = 0;
                int payGradeLevel = 0;

                while (stream.Position < stream.Length)
                {
                    var tag = ReadVarint(reader);
                    var fieldNumber = tag >> 3;
                    var wireType = tag & 0x7;

                    switch (fieldNumber)
                    {
                        case 6: // combo_count
                            if (wireType == 0) // varint
                            {
                                giftCount = (int)ReadVarint(reader);
                            }
                            break;
                        case 7: // user
                            if (wireType == 2) // length-delimited
                            {
                                var length = (int)ReadVarint(reader);
                                var userData = reader.ReadBytes(length);
                                var user = ParseUser(userData);
                                if (user != null)
                                {
                                    userName = user.NickName;
                                    userId = user.IdStr;
                                    fansClubLevel = user.FansClubLevel;
                                    payGradeLevel = user.PayGradeLevel;
                                }
                            }
                            break;
                        case 15: // gift
                            if (wireType == 2) // length-delimited
                            {
                                var length = (int)ReadVarint(reader);
                                var giftData = reader.ReadBytes(length);
                                giftName = ParseGiftStruct(giftData);
                            }
                            break;
                        default:
                            SkipField(reader, (int)wireType);
                            break;
                    }
                }

                return new GiftMessage
                {
                    UserName = userName ?? "用户名",
                    UserId = userId ?? "",
                    GiftName = giftName ?? "礼物名称",
                    GiftCount = giftCount,
                    FansClubLevel = fansClubLevel,
                    PayGradeLevel = payGradeLevel
                };
            }
            catch (Exception ex)
            {
                Console.WriteLine($"解析礼物消息失败: {ex.Message}");
                return new GiftMessage
                {
                    UserName = "用户名",
                    UserId = "",
                    GiftName = "礼物名称",
                    GiftCount = 1,
                    FansClubLevel = 0,
                    PayGradeLevel = 0
                };
            }
        }

        /// <summary>
        /// 解析点赞消息
        /// </summary>
        private static LikeMessage? ParseLikeMessage(byte[] payload)
        {
            try
            {
                using var stream = new MemoryStream(payload);
                using var reader = new BinaryReader(stream);

                string? userName = null;
                string? userId = null;
                int likeCount = 1;
                int fansClubLevel = 0;
                int payGradeLevel = 0;

                while (stream.Position < stream.Length)
                {
                    var tag = ReadVarint(reader);
                    var fieldNumber = tag >> 3;
                    var wireType = tag & 0x7;

                    switch (fieldNumber)
                    {
                        case 2: // count
                            if (wireType == 0) // varint
                            {
                                likeCount = (int)ReadVarint(reader);
                            }
                            break;
                        case 5: // user
                            if (wireType == 2) // length-delimited
                            {
                                var length = (int)ReadVarint(reader);
                                var userData = reader.ReadBytes(length);
                                var user = ParseUser(userData);
                                if (user != null)
                                {
                                    userName = user.NickName;
                                    userId = user.IdStr;
                                    fansClubLevel = user.FansClubLevel;
                                    payGradeLevel = user.PayGradeLevel;
                                }
                            }
                            break;
                        default:
                            SkipField(reader, (int)wireType);
                            break;
                    }
                }

                return new LikeMessage
                {
                    UserName = userName ?? "用户名",
                    UserId = userId ?? "",
                    LikeCount = likeCount,
                    FansClubLevel = fansClubLevel,
                    PayGradeLevel = payGradeLevel
                };
            }
            catch (Exception ex)
            {
                Console.WriteLine($"解析点赞消息失败: {ex.Message}");
                return new LikeMessage
                {
                    UserName = "用户名",
                    UserId = "",
                    LikeCount = 1,
                    FansClubLevel = 0,
                    PayGradeLevel = 0
                };
            }
        }

        /// <summary>
        /// 解析进场消息
        /// </summary>
        private static MemberMessage? ParseMemberMessage(byte[] payload)
        {
            try
            {
                using var stream = new MemoryStream(payload);
                using var reader = new BinaryReader(stream);

                string? userName = null;
                string? userId = null;
                int fansClubLevel = 0;
                int payGradeLevel = 0;

                while (stream.Position < stream.Length)
                {
                    var tag = ReadVarint(reader);
                    var fieldNumber = tag >> 3;
                    var wireType = tag & 0x7;

                    switch (fieldNumber)
                    {
                        case 2: // user
                            if (wireType == 2) // length-delimited
                            {
                                var length = (int)ReadVarint(reader);
                                var userData = reader.ReadBytes(length);
                                var user = ParseUser(userData);
                                if (user != null)
                                {
                                    userName = user.NickName;
                                    userId = user.IdStr;
                                    fansClubLevel = user.FansClubLevel;
                                    payGradeLevel = user.PayGradeLevel;
                                }
                            }
                            break;
                        default:
                            SkipField(reader, (int)wireType);
                            break;
                    }
                }

                return new MemberMessage
                {
                    UserName = userName ?? "用户名",
                    UserId = userId ?? "",
                    Content = "进入了直播间",
                    FansClubLevel = fansClubLevel,
                    PayGradeLevel = payGradeLevel
                };
            }
            catch (Exception ex)
            {
                Console.WriteLine($"解析进场消息失败: {ex.Message}");
                return new MemberMessage
                {
                    UserName = "用户名",
                    UserId = "",
                    Content = "进入了直播间",
                    FansClubLevel = 0,
                    PayGradeLevel = 0
                };
            }
        }

        /// <summary>
        /// 解析关注消息
        /// </summary>
        private static SocialMessage? ParseSocialMessage(byte[] payload)
        {
            try
            {
                using var stream = new MemoryStream(payload);
                using var reader = new BinaryReader(stream);

                string? userName = null;
                string? userId = null;
                int fansClubLevel = 0;
                int payGradeLevel = 0;

                while (stream.Position < stream.Length)
                {
                    var tag = ReadVarint(reader);
                    var fieldNumber = tag >> 3;
                    var wireType = tag & 0x7;

                    switch (fieldNumber)
                    {
                        case 2: // user
                            if (wireType == 2) // length-delimited
                            {
                                var length = (int)ReadVarint(reader);
                                var userData = reader.ReadBytes(length);
                                var user = ParseUser(userData);
                                if (user != null)
                                {
                                    userName = user.NickName;
                                    userId = user.IdStr;
                                    fansClubLevel = user.FansClubLevel;
                                    payGradeLevel = user.PayGradeLevel;
                                }
                            }
                            break;
                        default:
                            SkipField(reader, (int)wireType);
                            break;
                    }
                }

                return new SocialMessage
                {
                    UserName = userName ?? "用户名",
                    UserId = userId ?? "",
                    Content = "关注了主播",
                    FansClubLevel = fansClubLevel,
                    PayGradeLevel = payGradeLevel
                };
            }
            catch (Exception ex)
            {
                Console.WriteLine($"解析关注消息失败: {ex.Message}");
                return new SocialMessage
                {
                    UserName = "用户名",
                    UserId = "",
                    Content = "关注了主播",
                    FansClubLevel = 0,
                    PayGradeLevel = 0
                };
            }
        }

        /// <summary>
        /// 解析直播间统计消息
        /// </summary>
        private static RoomStatsMessage? ParseRoomUserSeqMessage(byte[] payload)
        {
            try
            {
                using var stream = new MemoryStream(payload);
                using var reader = new BinaryReader(stream);

                int currentViewers = 0;
                string totalViewers = "";

                while (stream.Position < stream.Length)
                {
                    var tag = ReadVarint(reader);
                    var fieldNumber = tag >> 3;
                    var wireType = tag & 0x7;

                    switch (fieldNumber)
                    {
                        case 3: // total
                            if (wireType == 0) // varint
                            {
                                currentViewers = (int)ReadVarint(reader);
                            }
                            break;
                        case 11: // total_pv_for_anchor
                            if (wireType == 2) // length-delimited
                            {
                                var length = (int)ReadVarint(reader);
                                var bytes = reader.ReadBytes(length);
                                totalViewers = Encoding.UTF8.GetString(bytes);
                            }
                            break;
                        default:
                            SkipField(reader, (int)wireType);
                            break;
                    }
                }

                return new RoomStatsMessage
                {
                    CurrentViewers = currentViewers,
                    TotalViewers = totalViewers
                };
            }
            catch (Exception ex)
            {
                Console.WriteLine($"解析统计消息失败: {ex.Message}");
                return new RoomStatsMessage
                {
                    CurrentViewers = 1000,
                    TotalViewers = "10万"
                };
            }
        }

        /// <summary>
        /// 解析控制消息
        /// </summary>
        private static ControlMessage? ParseControlMessage(byte[] payload)
        {
            return new ControlMessage
            {
                Status = 0,
                Content = "直播间状态变化"
            };
        }

        /// <summary>
        /// 解析用户信息
        /// </summary>
        private static UserInfo? ParseUser(byte[] userData)
        {
            try
            {
                using var stream = new MemoryStream(userData);
                using var reader = new BinaryReader(stream);

                string? nickName = null;
                string? idStr = null;
                int fansClubLevel = 0;
                int payGradeLevel = 0;

                Console.WriteLine($"[调试] 开始解析用户信息，数据长度: {userData.Length}");

                while (stream.Position < stream.Length)
                {
                    var tag = ReadVarint(reader);
                    var fieldNumber = tag >> 3;
                    var wireType = tag & 0x7;

                    Console.WriteLine($"[调试] 解析字段 {fieldNumber}, wireType: {wireType}");

                    switch (fieldNumber)
                    {
                        case 1: // id (uint64)
                            if (wireType == 0) // varint
                            {
                                var id = ReadVarint(reader);
                                Console.WriteLine($"[调试] 解析到用户ID (field 1): {id}");
                                if (string.IsNullOrEmpty(idStr))
                                {
                                    idStr = id.ToString();
                                }
                            }
                            break;
                        case 2: // shortId (uint64)
                            if (wireType == 0) // varint
                            {
                                var shortId = ReadVarint(reader);
                                Console.WriteLine($"[调试] 解析到短ID (field 2): {shortId}");
                            }
                            break;
                        case 3: // nick_name
                            if (wireType == 2) // length-delimited
                            {
                                var length = (int)ReadVarint(reader);
                                var bytes = reader.ReadBytes(length);
                                nickName = Encoding.UTF8.GetString(bytes);
                                Console.WriteLine($"[调试] 解析到用户名 (field 3): {nickName}");
                            }
                            break;
                        case 1028: // id_str
                            if (wireType == 2) // length-delimited
                            {
                                var length = (int)ReadVarint(reader);
                                var bytes = reader.ReadBytes(length);
                                idStr = Encoding.UTF8.GetString(bytes);
                                Console.WriteLine($"[调试] 解析到用户ID字符串 (field 1028): {idStr}");
                            }
                            break;
                        case 23: // pay_grade (修正字段编号)
                            if (wireType == 2) // length-delimited
                            {
                                var length = (int)ReadVarint(reader);
                                var payGradeData = reader.ReadBytes(length);
                                payGradeLevel = ParsePayGradeLevel(payGradeData);
                                Console.WriteLine($"[调试] 解析到财富等级: {payGradeLevel} (用户: {nickName})");
                            }
                            break;
                        case 24: // fans_club (修正字段编号)
                            if (wireType == 2) // length-delimited
                            {
                                var length = (int)ReadVarint(reader);
                                var fansClubData = reader.ReadBytes(length);
                                fansClubLevel = ParseFansClubLevel(fansClubData);
                                Console.WriteLine($"[调试] 解析到粉丝团等级: {fansClubLevel} (用户: {nickName})");
                            }
                            break;
                        default:
                            SkipField(reader, (int)wireType);
                            break;
                    }
                }

                var result = new UserInfo
                {
                    NickName = nickName ?? "",
                    IdStr = idStr ?? "",
                    FansClubLevel = fansClubLevel,
                    PayGradeLevel = payGradeLevel
                };

                Console.WriteLine($"[调试] 用户解析完成 - 用户名: {result.NickName}, ID: {result.IdStr}, 粉丝团: {result.FansClubLevel}, 财富: {result.PayGradeLevel}");

                return result;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"解析用户信息失败: {ex.Message}");
                return null;
            }
        }

        /// <summary>
        /// 解析礼物结构
        /// </summary>
        private static string ParseGiftStruct(byte[] giftData)
        {
            try
            {
                using var stream = new MemoryStream(giftData);
                using var reader = new BinaryReader(stream);

                while (stream.Position < stream.Length)
                {
                    var tag = ReadVarint(reader);
                    var fieldNumber = tag >> 3;
                    var wireType = tag & 0x7;

                    switch (fieldNumber)
                    {
                        case 16: // name
                            if (wireType == 2) // length-delimited
                            {
                                var length = (int)ReadVarint(reader);
                                var bytes = reader.ReadBytes(length);
                                return Encoding.UTF8.GetString(bytes);
                            }
                            break;
                        default:
                            SkipField(reader, (int)wireType);
                            break;
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"解析礼物结构失败: {ex.Message}");
            }

            return "礼物";
        }

        /// <summary>
        /// 用户信息类
        /// </summary>
        private class UserInfo
        {
            public string NickName { get; set; } = "";
            public string IdStr { get; set; } = "";
            public int FansClubLevel { get; set; } = 0;  // 粉丝团等级
            public int PayGradeLevel { get; set; } = 0;  // 财富等级
        }

        /// <summary>
        /// 编码Varint
        /// </summary>
        private static byte[] EncodeVarint(ulong value)
        {
            var result = new List<byte>();
            
            while (value >= 0x80)
            {
                result.Add((byte)(value | 0x80));
                value >>= 7;
            }
            result.Add((byte)value);
            
            return [.. result];
        }

        /// <summary>
        /// 跳过字段
        /// </summary>
        private static void SkipField(BinaryReader reader, int wireType)
        {
            switch (wireType)
            {
                case 0: // varint
                    ReadVarint(reader);
                    break;
                case 1: // fixed64
                    reader.ReadBytes(8);
                    break;
                case 2: // length-delimited
                    var length = (int)ReadVarint(reader);
                    reader.ReadBytes(length);
                    break;
                case 3: // start group (deprecated)
                case 4: // end group (deprecated)
                    throw new NotSupportedException("Group wire types are not supported");
                case 5: // fixed32
                    reader.ReadBytes(4);
                    break;
                default:
                    throw new ArgumentException($"Unknown wire type: {wireType}");
            }
        }

        /// <summary>
        /// 解压缩Gzip数据
        /// </summary>
        private static byte[] DecompressGzip(byte[] data)
        {
            try
            {
                using var input = new MemoryStream(data);
                using var gzip = new GZipStream(input, CompressionMode.Decompress);
                using var output = new MemoryStream();
                gzip.CopyTo(output);
                return output.ToArray();
            }
            catch
            {
                // 如果解压失败，返回原始数据
                return data;
            }
        }

        /// <summary>
        /// 读取Varint
        /// </summary>
        private static ulong ReadVarint(BinaryReader reader)
        {
            ulong result = 0;
            int shift = 0;
            
            while (true)
            {
                var b = reader.ReadByte();
                result |= (ulong)(b & 0x7F) << shift;
                
                if ((b & 0x80) == 0)
                    break;
                    
                shift += 7;
                if (shift >= 64)
                    throw new InvalidDataException("Varint too long");
            }
            
            return result;
        }

        /// <summary>
        /// 读取字符串
        /// </summary>
        private static string ReadString(BinaryReader reader)
        {
            var length = (int)ReadVarint(reader);
            var bytes = reader.ReadBytes(length);
            return Encoding.UTF8.GetString(bytes);
        }

        /// <summary>
        /// 读取字节数组
        /// </summary>
        private static byte[] ReadBytes(BinaryReader reader)
        {
            var length = (int)ReadVarint(reader);
            return reader.ReadBytes(length);
        }

        /// <summary>
        /// 解析直播间统计消息 (WebcastRoomStatsMessage)
        /// </summary>
        private static RoomStatsMessage? ParseRoomStatsMessage(byte[] payload)
        {
            try
            {
                using var stream = new MemoryStream(payload);
                using var reader = new BinaryReader(stream);

                string displayLong = "";

                while (stream.Position < stream.Length)
                {
                    var tag = ReadVarint(reader);
                    var fieldNumber = tag >> 3;
                    var wireType = tag & 0x7;

                    switch (fieldNumber)
                    {
                        case 4: // display_long
                            if (wireType == 2) // length-delimited
                            {
                                var length = (int)ReadVarint(reader);
                                var bytes = reader.ReadBytes(length);
                                displayLong = Encoding.UTF8.GetString(bytes);
                            }
                            break;
                        default:
                            SkipField(reader, (int)wireType);
                            break;
                    }
                }

                return new RoomStatsMessage
                {
                    CurrentViewers = 0,
                    TotalViewers = displayLong
                };
            }
            catch (Exception ex)
            {
                Console.WriteLine($"解析房间统计消息失败: {ex.Message}");
                return null;
            }
        }

        /// <summary>
        /// 解析排行榜消息
        /// </summary>
        private static LiveMessage? ParseRoomRankMessage(byte[] payload)
        {
            return new LiveMessage
            {
                Type = LiveMessageType.Unknown,
                Content = "排行榜更新",
                Timestamp = DateTime.Now
            };
        }

        /// <summary>
        /// 解析直播间流适配消息
        /// </summary>
        private static LiveMessage? ParseRoomStreamAdaptationMessage(byte[] payload)
        {
            return null; // 不显示这类消息
        }

        /// <summary>
        /// 解析粉丝团消息
        /// </summary>
        private static LiveMessage? ParseFansclubMessage(byte[] payload)
        {
            try
            {
                using var stream = new MemoryStream(payload);
                using var reader = new BinaryReader(stream);

                string content = "";
                string? userName = null;

                while (stream.Position < stream.Length)
                {
                    var tag = ReadVarint(reader);
                    var fieldNumber = tag >> 3;
                    var wireType = tag & 0x7;

                    switch (fieldNumber)
                    {
                        case 3: // content
                            if (wireType == 2) // length-delimited
                            {
                                var length = (int)ReadVarint(reader);
                                var bytes = reader.ReadBytes(length);
                                content = Encoding.UTF8.GetString(bytes);
                            }
                            break;
                        case 4: // user
                            if (wireType == 2) // length-delimited
                            {
                                var length = (int)ReadVarint(reader);
                                var userData = reader.ReadBytes(length);
                                var user = ParseUser(userData);
                                if (user != null)
                                {
                                    userName = user.NickName;
                                }
                            }
                            break;
                        default:
                            SkipField(reader, (int)wireType);
                            break;
                    }
                }

                return new LiveMessage
                {
                    Type = LiveMessageType.Social,
                    UserName = userName ?? "用户",
                    Content = string.IsNullOrEmpty(content) ? "粉丝团消息" : content,
                    Timestamp = DateTime.Now
                };
            }
            catch (Exception ex)
            {
                Console.WriteLine($"解析粉丝团消息失败: {ex.Message}");
                return null;
            }
        }

        /// <summary>
        /// 解析房间消息
        /// </summary>
        private static LiveMessage? ParseRoomMessage(byte[] payload)
        {
            try
            {
                using var stream = new MemoryStream(payload);
                using var reader = new BinaryReader(stream);

                string content = "";

                while (stream.Position < stream.Length)
                {
                    var tag = ReadVarint(reader);
                    var fieldNumber = tag >> 3;
                    var wireType = tag & 0x7;

                    switch (fieldNumber)
                    {
                        case 2: // content
                            if (wireType == 2) // length-delimited
                            {
                                var length = (int)ReadVarint(reader);
                                var bytes = reader.ReadBytes(length);
                                content = Encoding.UTF8.GetString(bytes);
                            }
                            break;
                        default:
                            SkipField(reader, (int)wireType);
                            break;
                    }
                }

                if (!string.IsNullOrEmpty(content))
                {
                    return new LiveMessage
                    {
                        Type = LiveMessageType.Unknown,
                        Content = $"房间消息: {content}",
                        Timestamp = DateTime.Now
                    };
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"解析房间消息失败: {ex.Message}");
            }

            return null;
        }

        /// <summary>
        /// 解析表情聊天消息
        /// </summary>
        private static ChatMessage? ParseEmojiChatMessage(byte[] payload)
        {
            try
            {
                using var stream = new MemoryStream(payload);
                using var reader = new BinaryReader(stream);

                string? userName = null;
                string? userId = null;
                string defaultContent = "";
                int fansClubLevel = 0;
                int payGradeLevel = 0;

                while (stream.Position < stream.Length)
                {
                    var tag = ReadVarint(reader);
                    var fieldNumber = tag >> 3;
                    var wireType = tag & 0x7;

                    switch (fieldNumber)
                    {
                        case 2: // user
                            if (wireType == 2) // length-delimited
                            {
                                var length = (int)ReadVarint(reader);
                                var userData = reader.ReadBytes(length);
                                var user = ParseUser(userData);
                                if (user != null)
                                {
                                    userName = user.NickName;
                                    userId = user.IdStr;
                                    fansClubLevel = user.FansClubLevel;
                                    payGradeLevel = user.PayGradeLevel;
                                }
                            }
                            break;
                        case 5: // default_content
                            if (wireType == 2) // length-delimited
                            {
                                var length = (int)ReadVarint(reader);
                                var bytes = reader.ReadBytes(length);
                                defaultContent = Encoding.UTF8.GetString(bytes);
                            }
                            break;
                        default:
                            SkipField(reader, (int)wireType);
                            break;
                    }
                }

                return new ChatMessage
                {
                    Content = string.IsNullOrEmpty(defaultContent) ? "[表情]" : defaultContent,
                    UserName = userName ?? "用户名",
                    UserId = userId ?? "",
                    FansClubLevel = fansClubLevel,
                    PayGradeLevel = payGradeLevel
                };
            }
            catch (Exception ex)
            {
                Console.WriteLine($"解析表情聊天消息失败: {ex.Message}");
                return new ChatMessage
                {
                    Content = "[表情]",
                    UserName = "用户名",
                    UserId = "",
                    FansClubLevel = 0,
                    PayGradeLevel = 0
                };
            }
        }

        /// <summary>
        /// 解析粉丝团等级
        /// </summary>
        private static int ParseFansClubLevel(byte[] fansClubData)
        {
            try
            {
                using var stream = new MemoryStream(fansClubData);
                using var reader = new BinaryReader(stream);

                while (stream.Position < stream.Length)
                {
                    var tag = ReadVarint(reader);
                    var fieldNumber = tag >> 3;
                    var wireType = tag & 0x7;

                    switch (fieldNumber)
                    {
                        case 1: // data
                            if (wireType == 2) // length-delimited
                            {
                                var length = (int)ReadVarint(reader);
                                var dataBytes = reader.ReadBytes(length);
                                return ParseFansClubData(dataBytes);
                            }
                            break;
                        default:
                            SkipField(reader, (int)wireType);
                            break;
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"解析粉丝团信息失败: {ex.Message}");
            }

            return 0;
        }

        /// <summary>
        /// 解析粉丝团数据
        /// </summary>
        private static int ParseFansClubData(byte[] dataBytes)
        {
            try
            {
                using var stream = new MemoryStream(dataBytes);
                using var reader = new BinaryReader(stream);

                while (stream.Position < stream.Length)
                {
                    var tag = ReadVarint(reader);
                    var fieldNumber = tag >> 3;
                    var wireType = tag & 0x7;

                    switch (fieldNumber)
                    {
                        case 2: // level (修正字段编号，根据protobuf定义)
                            if (wireType == 0) // varint
                            {
                                return (int)ReadVarint(reader);
                            }
                            break;
                        default:
                            SkipField(reader, (int)wireType);
                            break;
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"解析粉丝团数据失败: {ex.Message}");
            }

            return 0;
        }

        /// <summary>
        /// 解析财富等级
        /// </summary>
        private static int ParsePayGradeLevel(byte[] payGradeData)
        {
            try
            {
                using var stream = new MemoryStream(payGradeData);
                using var reader = new BinaryReader(stream);

                while (stream.Position < stream.Length)
                {
                    var tag = ReadVarint(reader);
                    var fieldNumber = tag >> 3;
                    var wireType = tag & 0x7;

                    switch (fieldNumber)
                    {
                        case 6: // level (修正字段编号，根据protobuf定义)
                            if (wireType == 0) // varint
                            {
                                return (int)ReadVarint(reader);
                            }
                            break;
                        default:
                            SkipField(reader, (int)wireType);
                            break;
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"解析财富等级失败: {ex.Message}");
            }

            return 0;
        }
    }
} 