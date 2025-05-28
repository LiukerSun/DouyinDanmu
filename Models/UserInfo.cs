using System;

namespace DouyinDanmu.Models
{
    /// <summary>
    /// 用户信息类
    /// </summary>
    public class UserInfo
    {
        /// <summary>
        /// 用户ID
        /// </summary>
        public string UserId { get; set; } = "";

        /// <summary>
        /// 用户昵称
        /// </summary>
        public string Nickname { get; set; } = "";

        /// <summary>
        /// 最后更新时间
        /// </summary>
        public DateTime LastUpdated { get; set; } = DateTime.Now;

        public UserInfo()
        {
        }

        public UserInfo(string userId, string nickname = "")
        {
            UserId = userId;
            Nickname = nickname;
            LastUpdated = DateTime.Now;
        }

        /// <summary>
        /// 显示格式：昵称 (ID) 或 用户ID: XXXXX
        /// </summary>
        public override string ToString()
        {
            if (string.IsNullOrEmpty(Nickname))
            {
                return $"用户ID: {UserId}";
            }
            return $"{Nickname} (ID: {UserId})";
        }

        /// <summary>
        /// 获取显示文本，如果昵称为空则只显示ID
        /// </summary>
        public string GetDisplayText()
        {
            if (string.IsNullOrEmpty(Nickname))
            {
                return $"用户ID: {UserId}";
            }
            return $"{Nickname} (ID: {UserId})";
        }
    }
} 