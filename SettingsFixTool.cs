using System;
using System.Collections.Generic;
using System.IO;
using DouyinDanmu.Models;
using DouyinDanmu.Services;

namespace DouyinDanmu
{
    /// <summary>
    /// 设置修复工具
    /// </summary>
    public static class SettingsFixTool
    {
        public static void FixSettings()
        {
            Console.WriteLine("=== 抖音直播设置修复工具 ===");
            
            try
            {
                // 获取设置文件路径
                var settingsPath = SettingsManager.GetSettingsFilePath();
                Console.WriteLine($"设置文件路径: {settingsPath}");
                
                // 检查文件是否存在
                if (!File.Exists(settingsPath))
                {
                    Console.WriteLine("设置文件不存在，无需修复");
                    return;
                }
                
                // 加载设置
                var settings = SettingsManager.LoadSettings();
                Console.WriteLine($"WatchedUserIds 数量: {settings.WatchedUserIds?.Count ?? 0}");
                Console.WriteLine($"UserInfos 数量: {settings.UserInfos?.Count ?? 0}");
                
                // 显示当前状态
                if (settings.UserInfos != null && settings.UserInfos.Count > 0)
                {
                    Console.WriteLine("\n当前 UserInfos 内容:");
                    foreach (var kvp in settings.UserInfos)
                    {
                        Console.WriteLine($"  - {kvp.Key}: {kvp.Value.Nickname}");
                    }
                }
                
                if (settings.WatchedUserIds != null && settings.WatchedUserIds.Count > 0)
                {
                    Console.WriteLine("\n当前 WatchedUserIds 内容:");
                    foreach (var userId in settings.WatchedUserIds)
                    {
                        Console.WriteLine($"  - {userId}");
                    }
                }
                
                // 检查是否需要修复
                if ((settings.WatchedUserIds == null || settings.WatchedUserIds.Count == 0) && 
                    settings.UserInfos != null && settings.UserInfos.Count > 0)
                {
                    Console.WriteLine("\n检测到需要修复：WatchedUserIds为空但UserInfos有数据");
                    Console.Write("是否要执行修复？(y/n): ");
                    
                    var input = Console.ReadLine();
                    if (input?.ToLower() == "y" || input?.ToLower() == "yes")
                    {
                        // 执行修复
                        settings.WatchedUserIds = new List<string>(settings.UserInfos.Keys);
                        Console.WriteLine($"修复后 WatchedUserIds 数量: {settings.WatchedUserIds.Count}");
                        
                        // 保存修复后的设置
                        bool success = SettingsManager.SaveSettings(settings);
                        Console.WriteLine($"保存修复后的设置: {(success ? "成功" : "失败")}");
                        
                        if (success)
                        {
                            Console.WriteLine("修复完成！现在重新启动程序应该能看到关注的用户列表了。");
                        }
                        else
                        {
                            Console.WriteLine("修复失败，请检查文件权限。");
                        }
                    }
                    else
                    {
                        Console.WriteLine("取消修复");
                    }
                }
                else
                {
                    Console.WriteLine("\n设置正常，无需修复");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"修复过程中出错: {ex.Message}");
            }
            
            Console.WriteLine("\n按任意键退出...");
            Console.ReadKey();
        }
    }
} 