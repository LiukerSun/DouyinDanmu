using System;
using System.IO;
using Microsoft.Data.Sqlite;
using DouyinDanmu.Models;
using System.Threading.Tasks;
using System.Collections.Generic;
using System.Linq;

namespace DouyinDanmu.Services
{
    /// <summary>
    /// 数据库服务类
    /// </summary>
    public class DatabaseService : IDisposable
    {
        private readonly string _connectionString;
        private SqliteConnection? _connection;

        public DatabaseService()
        {
            // 优先使用应用程序目录，如果无法写入则使用用户数据目录
            string dbPath;
            
            try
            {
                // 尝试使用应用程序目录
                var appDir = AppDomain.CurrentDomain.BaseDirectory;
                var testFile = Path.Combine(appDir, "test_write.tmp");
                
                // 测试是否可以在应用程序目录写入文件
                File.WriteAllText(testFile, "test");
                File.Delete(testFile);
                
                // 如果成功，使用应用程序目录
                dbPath = Path.Combine(appDir, "douyin_live_messages.db");
            }
            catch
            {
                // 如果应用程序目录无法写入，使用用户数据目录
                var appDataPath = Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
                    "DouyinLiveFetcher"
                );
                
                if (!Directory.Exists(appDataPath))
                {
                    Directory.CreateDirectory(appDataPath);
                }
                
                dbPath = Path.Combine(appDataPath, "douyin_live_messages.db");
            }

            _connectionString = $"Data Source={dbPath}";
        }

        /// <summary>
        /// 初始化数据库连接和表结构
        /// </summary>
        public async Task InitializeAsync()
        {
            try
            {
                _connection = new SqliteConnection(_connectionString);
                await _connection.OpenAsync();
                await CreateTablesAsync();
                
                // 验证数据库是否正确创建
                await VerifyDatabaseAsync();
            }
            catch (Exception ex)
            {
                throw new InvalidOperationException($"数据库初始化失败: {ex.Message}", ex);
            }
        }

        /// <summary>
        /// 验证数据库表是否正确创建
        /// </summary>
        private async Task VerifyDatabaseAsync()
        {
            if (_connection == null) return;

            var tableNames = new[] { "chat_messages", "member_messages", "interaction_messages" };
            
            foreach (var tableName in tableNames)
            {
                var sql = "SELECT name FROM sqlite_master WHERE type='table' AND name=@tableName";
                using var command = new SqliteCommand(sql, _connection);
                command.Parameters.AddWithValue("@tableName", tableName);
                
                var result = await command.ExecuteScalarAsync();
                if (result == null)
                {
                    throw new InvalidOperationException($"数据库表 {tableName} 创建失败");
                }
            }
        }

        /// <summary>
        /// 创建数据表
        /// </summary>
        private async Task CreateTablesAsync()
        {
            if (_connection == null) return;

            // 创建聊天消息表
            var createChatTableSql = @"
                CREATE TABLE IF NOT EXISTS chat_messages (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    live_id TEXT NOT NULL,
                    timestamp DATETIME NOT NULL,
                    user_id TEXT,
                    user_name TEXT,
                    fans_club_level INTEGER DEFAULT 0,
                    pay_grade_level INTEGER DEFAULT 0,
                    content TEXT,
                    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
                )";

            // 创建进场消息表
            var createMemberTableSql = @"
                CREATE TABLE IF NOT EXISTS member_messages (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    live_id TEXT NOT NULL,
                    timestamp DATETIME NOT NULL,
                    user_id TEXT,
                    user_name TEXT,
                    fans_club_level INTEGER DEFAULT 0,
                    pay_grade_level INTEGER DEFAULT 0,
                    action_type TEXT DEFAULT 'enter',
                    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
                )";

            // 创建礼物和互动消息表
            var createInteractionTableSql = @"
                CREATE TABLE IF NOT EXISTS interaction_messages (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    live_id TEXT NOT NULL,
                    timestamp DATETIME NOT NULL,
                    user_id TEXT,
                    user_name TEXT,
                    fans_club_level INTEGER DEFAULT 0,
                    pay_grade_level INTEGER DEFAULT 0,
                    message_type TEXT NOT NULL,
                    gift_name TEXT,
                    gift_count INTEGER DEFAULT 0,
                    like_count INTEGER DEFAULT 0,
                    content TEXT,
                    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
                )";

            // 创建表
            using var command1 = new SqliteCommand(createChatTableSql, _connection);
            await command1.ExecuteNonQueryAsync();

            using var command2 = new SqliteCommand(createMemberTableSql, _connection);
            await command2.ExecuteNonQueryAsync();

            using var command3 = new SqliteCommand(createInteractionTableSql, _connection);
            await command3.ExecuteNonQueryAsync();

            // 创建索引
            await CreateIndexesAsync();
        }

        /// <summary>
        /// 创建数据库索引
        /// </summary>
        private async Task CreateIndexesAsync()
        {
            if (_connection == null) return;

            var indexes = new[]
            {
                // 聊天消息表索引
                "CREATE INDEX IF NOT EXISTS idx_chat_live_id ON chat_messages(live_id)",
                "CREATE INDEX IF NOT EXISTS idx_chat_timestamp ON chat_messages(timestamp)",
                "CREATE INDEX IF NOT EXISTS idx_chat_user_id ON chat_messages(user_id)",
                
                // 进场消息表索引
                "CREATE INDEX IF NOT EXISTS idx_member_live_id ON member_messages(live_id)",
                "CREATE INDEX IF NOT EXISTS idx_member_timestamp ON member_messages(timestamp)",
                "CREATE INDEX IF NOT EXISTS idx_member_user_id ON member_messages(user_id)",
                
                // 互动消息表索引
                "CREATE INDEX IF NOT EXISTS idx_interaction_live_id ON interaction_messages(live_id)",
                "CREATE INDEX IF NOT EXISTS idx_interaction_timestamp ON interaction_messages(timestamp)",
                "CREATE INDEX IF NOT EXISTS idx_interaction_user_id ON interaction_messages(user_id)",
                "CREATE INDEX IF NOT EXISTS idx_interaction_message_type ON interaction_messages(message_type)"
            };

            foreach (var indexSql in indexes)
            {
                using var command = new SqliteCommand(indexSql, _connection);
                await command.ExecuteNonQueryAsync();
            }
        }

        /// <summary>
        /// 保存聊天消息
        /// </summary>
        public async Task SaveChatMessageAsync(string liveId, LiveMessage message)
        {
            if (_connection == null) return;

            var sql = @"
                INSERT INTO chat_messages (live_id, timestamp, user_id, user_name, fans_club_level, pay_grade_level, content)
                VALUES (@liveId, @timestamp, @userId, @userName, @fansClubLevel, @payGradeLevel, @content)";

            using var command = new SqliteCommand(sql, _connection);
            command.Parameters.AddWithValue("@liveId", liveId);
            command.Parameters.AddWithValue("@timestamp", message.Timestamp);
            command.Parameters.AddWithValue("@userId", message.UserId ?? "");
            command.Parameters.AddWithValue("@userName", message.UserName ?? "");
            command.Parameters.AddWithValue("@fansClubLevel", message.FansClubLevel);
            command.Parameters.AddWithValue("@payGradeLevel", message.PayGradeLevel);
            command.Parameters.AddWithValue("@content", message.Content ?? "");

            await command.ExecuteNonQueryAsync();
        }

        /// <summary>
        /// 保存进场消息
        /// </summary>
        public async Task SaveMemberMessageAsync(string liveId, LiveMessage message)
        {
            if (_connection == null) return;

            var sql = @"
                INSERT INTO member_messages (live_id, timestamp, user_id, user_name, fans_club_level, pay_grade_level, action_type)
                VALUES (@liveId, @timestamp, @userId, @userName, @fansClubLevel, @payGradeLevel, @actionType)";

            using var command = new SqliteCommand(sql, _connection);
            command.Parameters.AddWithValue("@liveId", liveId);
            command.Parameters.AddWithValue("@timestamp", message.Timestamp);
            command.Parameters.AddWithValue("@userId", message.UserId ?? "");
            command.Parameters.AddWithValue("@userName", message.UserName ?? "");
            command.Parameters.AddWithValue("@fansClubLevel", message.FansClubLevel);
            command.Parameters.AddWithValue("@payGradeLevel", message.PayGradeLevel);
            command.Parameters.AddWithValue("@actionType", "enter");

            await command.ExecuteNonQueryAsync();
        }

        /// <summary>
        /// 保存互动消息（礼物、点赞、关注等）
        /// </summary>
        public async Task SaveInteractionMessageAsync(string liveId, LiveMessage message)
        {
            if (_connection == null) return;

            var sql = @"
                INSERT INTO interaction_messages (live_id, timestamp, user_id, user_name, fans_club_level, pay_grade_level, 
                                                message_type, gift_name, gift_count, like_count, content)
                VALUES (@liveId, @timestamp, @userId, @userName, @fansClubLevel, @payGradeLevel, 
                        @messageType, @giftName, @giftCount, @likeCount, @content)";

            using var command = new SqliteCommand(sql, _connection);
            command.Parameters.AddWithValue("@liveId", liveId);
            command.Parameters.AddWithValue("@timestamp", message.Timestamp);
            command.Parameters.AddWithValue("@userId", message.UserId ?? "");
            command.Parameters.AddWithValue("@userName", message.UserName ?? "");
            command.Parameters.AddWithValue("@fansClubLevel", message.FansClubLevel);
            command.Parameters.AddWithValue("@payGradeLevel", message.PayGradeLevel);
            command.Parameters.AddWithValue("@messageType", message.Type.ToString());

            // 根据消息类型设置特定字段
            if (message is GiftMessage gift)
            {
                command.Parameters.AddWithValue("@giftName", gift.GiftName ?? "");
                command.Parameters.AddWithValue("@giftCount", gift.GiftCount);
                command.Parameters.AddWithValue("@likeCount", 0);
                command.Parameters.AddWithValue("@content", $"{gift.GiftName} x{gift.GiftCount}");
            }
            else if (message is LikeMessage like)
            {
                command.Parameters.AddWithValue("@giftName", "");
                command.Parameters.AddWithValue("@giftCount", 0);
                command.Parameters.AddWithValue("@likeCount", like.LikeCount);
                command.Parameters.AddWithValue("@content", $"点赞 x{like.LikeCount}");
            }
            else
            {
                command.Parameters.AddWithValue("@giftName", "");
                command.Parameters.AddWithValue("@giftCount", 0);
                command.Parameters.AddWithValue("@likeCount", 0);
                command.Parameters.AddWithValue("@content", message.Content ?? "");
            }

            await command.ExecuteNonQueryAsync();
        }

        /// <summary>
        /// 获取数据库统计信息
        /// </summary>
        public async Task<DatabaseStats> GetStatsAsync(string liveId)
        {
            if (_connection == null) return new DatabaseStats();

            var stats = new DatabaseStats { LiveId = liveId };

            // 统计聊天消息数量
            var chatCountSql = "SELECT COUNT(*) FROM chat_messages WHERE live_id = @liveId";
            using (var command = new SqliteCommand(chatCountSql, _connection))
            {
                command.Parameters.AddWithValue("@liveId", liveId);
                var result = await command.ExecuteScalarAsync();
                stats.ChatMessageCount = Convert.ToInt32(result);
            }

            // 统计进场消息数量
            var memberCountSql = "SELECT COUNT(*) FROM member_messages WHERE live_id = @liveId";
            using (var command = new SqliteCommand(memberCountSql, _connection))
            {
                command.Parameters.AddWithValue("@liveId", liveId);
                var result = await command.ExecuteScalarAsync();
                stats.MemberMessageCount = Convert.ToInt32(result);
            }

            // 统计互动消息数量
            var interactionCountSql = "SELECT COUNT(*) FROM interaction_messages WHERE live_id = @liveId";
            using (var command = new SqliteCommand(interactionCountSql, _connection))
            {
                command.Parameters.AddWithValue("@liveId", liveId);
                var result = await command.ExecuteScalarAsync();
                stats.InteractionMessageCount = Convert.ToInt32(result);
            }

            // 统计独立用户数量
            var uniqueUsersSql = @"
                SELECT COUNT(DISTINCT user_id) FROM (
                    SELECT user_id FROM chat_messages WHERE live_id = @liveId AND user_id != ''
                    UNION
                    SELECT user_id FROM member_messages WHERE live_id = @liveId AND user_id != ''
                    UNION
                    SELECT user_id FROM interaction_messages WHERE live_id = @liveId AND user_id != ''
                )";
            using (var command = new SqliteCommand(uniqueUsersSql, _connection))
            {
                command.Parameters.AddWithValue("@liveId", liveId);
                var result = await command.ExecuteScalarAsync();
                stats.UniqueUserCount = Convert.ToInt32(result);
            }

            return stats;
        }

        /// <summary>
        /// 获取数据库文件路径
        /// </summary>
        public string GetDatabasePath()
        {
            var connection = new SqliteConnection(_connectionString);
            return connection.DataSource;
        }

        /// <summary>
        /// 清理指定直播间的数据
        /// </summary>
        public async Task ClearLiveDataAsync(string liveId)
        {
            if (_connection == null) return;

            var deleteChatSql = "DELETE FROM chat_messages WHERE live_id = @liveId";
            var deleteMemberSql = "DELETE FROM member_messages WHERE live_id = @liveId";
            var deleteInteractionSql = "DELETE FROM interaction_messages WHERE live_id = @liveId";

            using var transaction = _connection.BeginTransaction();
            try
            {
                using (var command = new SqliteCommand(deleteChatSql, _connection, transaction))
                {
                    command.Parameters.AddWithValue("@liveId", liveId);
                    await command.ExecuteNonQueryAsync();
                }

                using (var command = new SqliteCommand(deleteMemberSql, _connection, transaction))
                {
                    command.Parameters.AddWithValue("@liveId", liveId);
                    await command.ExecuteNonQueryAsync();
                }

                using (var command = new SqliteCommand(deleteInteractionSql, _connection, transaction))
                {
                    command.Parameters.AddWithValue("@liveId", liveId);
                    await command.ExecuteNonQueryAsync();
                }

                await transaction.CommitAsync();
            }
            catch
            {
                await transaction.RollbackAsync();
                throw;
            }
        }

        /// <summary>
        /// 获取所有直播间ID
        /// </summary>
        public async Task<List<string>> GetAllLiveIdsAsync()
        {
            if (_connection == null) return new List<string>();

            var liveIds = new HashSet<string>();

            // 从三个表中获取所有直播间ID
            var queries = new[]
            {
                "SELECT DISTINCT live_id FROM chat_messages WHERE live_id != ''",
                "SELECT DISTINCT live_id FROM member_messages WHERE live_id != ''",
                "SELECT DISTINCT live_id FROM interaction_messages WHERE live_id != ''"
            };

            foreach (var query in queries)
            {
                using var command = new SqliteCommand(query, _connection);
                using var reader = await command.ExecuteReaderAsync();
                
                while (await reader.ReadAsync())
                {
                    var liveId = reader.GetString(0);
                    if (!string.IsNullOrEmpty(liveId))
                    {
                        liveIds.Add(liveId);
                    }
                }
            }

            return liveIds.OrderBy(x => x).ToList();
        }

        /// <summary>
        /// 按条件查询消息
        /// </summary>
        public async Task<List<QueryResult>> QueryMessagesAsync(QueryFilter filter)
        {
            if (_connection == null) return new List<QueryResult>();

            var results = new List<QueryResult>();

            // 构建查询条件
            var whereConditions = new List<string>();
            var parameters = new List<SqliteParameter>();

            // 时间范围条件
            whereConditions.Add("timestamp >= @startTime AND timestamp <= @endTime");
            parameters.Add(new SqliteParameter("@startTime", filter.StartTime));
            parameters.Add(new SqliteParameter("@endTime", filter.EndTime));

            // 直播间ID条件
            if (!string.IsNullOrEmpty(filter.LiveId))
            {
                whereConditions.Add("live_id = @liveId");
                parameters.Add(new SqliteParameter("@liveId", filter.LiveId));
            }

            // 用户ID条件
            if (!string.IsNullOrEmpty(filter.UserId))
            {
                whereConditions.Add("user_id LIKE @userId");
                parameters.Add(new SqliteParameter("@userId", $"%{filter.UserId}%"));
            }

            // 用户名条件
            if (!string.IsNullOrEmpty(filter.UserName))
            {
                whereConditions.Add("user_name LIKE @userName");
                parameters.Add(new SqliteParameter("@userName", $"%{filter.UserName}%"));
            }

            var whereClause = whereConditions.Count > 0 ? "WHERE " + string.Join(" AND ", whereConditions) : "";

            // 查询聊天消息
            if (filter.MessageTypes.Contains("Chat"))
            {
                var sql = $@"
                    SELECT id, live_id, timestamp, 'Chat' as message_type, user_id, user_name, 
                           fans_club_level, pay_grade_level, content
                    FROM chat_messages 
                    {whereClause}";

                await ExecuteQueryAsync(sql, parameters, results);
            }

            // 查询进场消息
            if (filter.MessageTypes.Contains("Member"))
            {
                var sql = $@"
                    SELECT id, live_id, timestamp, 'Member' as message_type, user_id, user_name, 
                           fans_club_level, pay_grade_level, '进入直播间' as content
                    FROM member_messages 
                    {whereClause}";

                await ExecuteQueryAsync(sql, parameters, results);
            }

            // 查询互动消息
            var interactionTypes = filter.MessageTypes.Where(t => t != "Chat" && t != "Member").ToList();
            if (interactionTypes.Count > 0)
            {
                var typeCondition = "message_type IN (" + string.Join(",", interactionTypes.Select(t => $"'{t}'")) + ")";
                var interactionWhereClause = whereConditions.Count > 0 
                    ? $"{whereClause} AND {typeCondition}" 
                    : $"WHERE {typeCondition}";

                var sql = $@"
                    SELECT id, live_id, timestamp, message_type, user_id, user_name, 
                           fans_club_level, pay_grade_level, content
                    FROM interaction_messages 
                    {interactionWhereClause}";

                await ExecuteQueryAsync(sql, parameters, results);
            }

            // 按时间排序
            return results.OrderBy(r => r.Timestamp).ToList();
        }

        /// <summary>
        /// 执行查询并添加结果
        /// </summary>
        private async Task ExecuteQueryAsync(string sql, List<SqliteParameter> parameters, List<QueryResult> results)
        {
            if (_connection == null) return;

            using var command = new SqliteCommand(sql, _connection);
            
            // 添加参数
            foreach (var param in parameters)
            {
                command.Parameters.Add(new SqliteParameter(param.ParameterName, param.Value));
            }

            using var reader = await command.ExecuteReaderAsync();
            
            while (await reader.ReadAsync())
            {
                var result = new QueryResult
                {
                    Id = reader.GetInt32(0), // id
                    LiveId = reader.GetString(1), // live_id
                    Timestamp = reader.GetDateTime(2), // timestamp
                    MessageType = reader.GetString(3), // message_type
                    UserId = reader.IsDBNull(4) ? null : reader.GetString(4), // user_id
                    UserName = reader.IsDBNull(5) ? null : reader.GetString(5), // user_name
                    FansClubLevel = reader.IsDBNull(6) ? 0 : reader.GetInt32(6), // fans_club_level
                    PayGradeLevel = reader.IsDBNull(7) ? 0 : reader.GetInt32(7), // pay_grade_level
                    Content = reader.IsDBNull(8) ? null : reader.GetString(8) // content
                };
                
                results.Add(result);
            }
        }

        public void Dispose()
        {
            _connection?.Dispose();
        }
    }

    /// <summary>
    /// 数据库统计信息
    /// </summary>
    public class DatabaseStats
    {
        public string LiveId { get; set; } = "";
        public int ChatMessageCount { get; set; }
        public int MemberMessageCount { get; set; }
        public int InteractionMessageCount { get; set; }
        public int UniqueUserCount { get; set; }
        public int TotalMessageCount => ChatMessageCount + MemberMessageCount + InteractionMessageCount;
    }

    /// <summary>
    /// 查询筛选条件
    /// </summary>
    public class QueryFilter
    {
        public string? LiveId { get; set; }
        public string? UserId { get; set; }
        public string? UserName { get; set; }
        public List<string> MessageTypes { get; set; } = new List<string>();
        public DateTime StartTime { get; set; }
        public DateTime EndTime { get; set; }
    }

    /// <summary>
    /// 查询结果
    /// </summary>
    public class QueryResult
    {
        public int Id { get; set; }
        public string LiveId { get; set; } = "";
        public DateTime Timestamp { get; set; }
        public string MessageType { get; set; } = "";
        public string? UserId { get; set; }
        public string? UserName { get; set; }
        public int FansClubLevel { get; set; }
        public int PayGradeLevel { get; set; }
        public string? Content { get; set; }
    }
} 