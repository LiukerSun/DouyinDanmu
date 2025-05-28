using System;
using System.Collections.Generic;
using DouyinDanmu.Models;
using DouyinDanmu.Services;

namespace DouyinDanmu
{
    /// <summary>
    /// 测试设置恢复功能
    /// </summary>
    public static class TestSettingsRecovery
    {
        public static void RunTest()
        {
            Console.WriteLine("=== 测试设置恢复功能 ===");
            
            // 加载当前设置
            var settings = SettingsManager.LoadSettings();
            
            Console.WriteLine($"设置文件路径: {SettingsManager.GetSettingsFilePath()}");
            Console.WriteLine($"设置文件信息: {SettingsManager.GetSettingsFileInfo()}");
            Console.WriteLine($"WatchedUserIds 数量: {settings.WatchedUserIds?.Count ?? 0}");
            Console.WriteLine($"UserInfos 数量: {settings.UserInfos?.Count ?? 0}");
            
            if (settings.WatchedUserIds != null && settings.WatchedUserIds.Count > 0)
            {
                Console.WriteLine("WatchedUserIds 内容:");
                foreach (var userId in settings.WatchedUserIds)
                {
                    Console.WriteLine($"  - {userId}");
                }
            }
            
            if (settings.UserInfos != null && settings.UserInfos.Count > 0)
            {
                Console.WriteLine("UserInfos 内容:");
                foreach (var kvp in settings.UserInfos)
                {
                    Console.WriteLine($"  - {kvp.Key}: {kvp.Value.Nickname}");
                }
            }
            
            // 如果需要修复
            if ((settings.WatchedUserIds == null || settings.WatchedUserIds.Count == 0) && 
                settings.UserInfos != null && settings.UserInfos.Count > 0)
            {
                Console.WriteLine("\n检测到需要修复的情况：WatchedUserIds为空但UserInfos有数据");
                
                // 执行修复
                settings.WatchedUserIds = new List<string>(settings.UserInfos.Keys);
                
                Console.WriteLine($"修复后 WatchedUserIds 数量: {settings.WatchedUserIds.Count}");
                
                // 保存修复后的设置
                bool success = SettingsManager.SaveSettings(settings);
                Console.WriteLine($"保存修复后的设置: {(success ? "成功" : "失败")}");
                
                if (success)
                {
                    Console.WriteLine("修复完成！");
                }
            }
            else
            {
                Console.WriteLine("\n设置正常，无需修复");
            }
            
            Console.WriteLine("=== 测试完成 ===");
        }
    }
} 