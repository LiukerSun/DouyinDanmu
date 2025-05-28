using System;
using Microsoft.Data.Sqlite;
using SQLitePCL;

namespace DouyinDanmu
{
    public static class TestSqlite
    {
        public static void TestConnection()
        {
            try
            {
                // 初始化 SQLite
                SQLitePCL.Batteries.Init();
                Console.WriteLine("SQLite 初始化成功");

                // 创建内存数据库连接
                using var connection = new SqliteConnection("Data Source=:memory:");
                connection.Open();
                Console.WriteLine("SQLite 连接创建成功");

                // 创建测试表
                var createTableSql = @"
                    CREATE TABLE test_table (
                        id INTEGER PRIMARY KEY,
                        name TEXT NOT NULL
                    )";

                using var createCommand = new SqliteCommand(createTableSql, connection);
                createCommand.ExecuteNonQuery();
                Console.WriteLine("测试表创建成功");

                // 插入测试数据
                var insertSql = "INSERT INTO test_table (name) VALUES (@name)";
                using var insertCommand = new SqliteCommand(insertSql, connection);
                insertCommand.Parameters.AddWithValue("@name", "测试数据");
                insertCommand.ExecuteNonQuery();
                Console.WriteLine("测试数据插入成功");

                // 查询测试数据
                var selectSql = "SELECT COUNT(*) FROM test_table";
                using var selectCommand = new SqliteCommand(selectSql, connection);
                var count = Convert.ToInt32(selectCommand.ExecuteScalar());
                Console.WriteLine($"查询结果: {count} 条记录");

                Console.WriteLine("SQLite 测试完成，所有功能正常！");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"SQLite 测试失败: {ex.Message}");
                Console.WriteLine($"详细错误: {ex}");
                throw;
            }
        }
    }
} 