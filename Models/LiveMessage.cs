using System;

namespace DouyinDanmu.Models
{
    /// <summary>
    /// 直播消息类型枚举
    /// </summary>
    public enum LiveMessageType
    {
        Chat,           // 聊天消息
        Gift,           // 礼物消息
        Like,           // 点赞消息
        Member,         // 进场消息
        Social,         // 关注消息
        RoomStats,      // 直播间统计
        Fansclub,       // 粉丝团消息
        Control,        // 控制消息
        EmojiChat,      // 表情聊天
        RoomInfo,       // 直播间信息
        Rank,           // 排行榜
        Unknown         // 未知消息
    }

    /// <summary>
    /// 直播消息基类
    /// </summary>
    public class LiveMessage
    {
        public LiveMessageType Type { get; set; }
        public DateTime Timestamp { get; set; } = DateTime.Now;
        public string RoomId { get; set; } = string.Empty;
        public string Content { get; set; } = string.Empty;
        public string UserId { get; set; } = string.Empty;
        public string UserName { get; set; } = string.Empty;
        public string Gender { get; set; } = string.Empty;
        public int FansClubLevel { get; set; } = 0;  // 粉丝团等级
        public int PayGradeLevel { get; set; } = 0;  // 财富等级
    }

    /// <summary>
    /// 聊天消息
    /// </summary>
    public class ChatMessage : LiveMessage
    {
        public ChatMessage()
        {
            Type = LiveMessageType.Chat;
        }
    }

    /// <summary>
    /// 礼物消息
    /// </summary>
    public class GiftMessage : LiveMessage
    {
        public string GiftName { get; set; } = string.Empty;
        public int GiftCount { get; set; }

        public GiftMessage()
        {
            Type = LiveMessageType.Gift;
        }
    }

    /// <summary>
    /// 点赞消息
    /// </summary>
    public class LikeMessage : LiveMessage
    {
        public int LikeCount { get; set; }

        public LikeMessage()
        {
            Type = LiveMessageType.Like;
        }
    }

    /// <summary>
    /// 进场消息
    /// </summary>
    public class MemberMessage : LiveMessage
    {
        public MemberMessage()
        {
            Type = LiveMessageType.Member;
        }
    }

    /// <summary>
    /// 关注消息
    /// </summary>
    public class SocialMessage : LiveMessage
    {
        public SocialMessage()
        {
            Type = LiveMessageType.Social;
        }
    }

    /// <summary>
    /// 直播间统计消息
    /// </summary>
    public class RoomStatsMessage : LiveMessage
    {
        public int CurrentViewers { get; set; }
        public string TotalViewers { get; set; } = string.Empty;

        public RoomStatsMessage()
        {
            Type = LiveMessageType.RoomStats;
        }
    }

    /// <summary>
    /// 粉丝团消息
    /// </summary>
    public class FansclubMessage : LiveMessage
    {
        public FansclubMessage()
        {
            Type = LiveMessageType.Fansclub;
        }
    }

    /// <summary>
    /// 控制消息
    /// </summary>
    public class ControlMessage : LiveMessage
    {
        public int Status { get; set; }

        public ControlMessage()
        {
            Type = LiveMessageType.Control;
        }
    }
}