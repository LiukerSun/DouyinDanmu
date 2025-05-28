using System;
using System.IO;
using Newtonsoft.Json;
using DouyinDanmu.Models;

namespace DouyinDanmu.Services
{
    /// <summary>
    /// 设置管理器
    /// </summary>
    public static class SettingsManager
    {
        private static readonly string SettingsFilePath = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "DouyinLiveFetcher",
            "settings.json"
        );

        /// <summary>
        /// 加载设置
        /// </summary>
        /// <returns>应用程序设置</returns>
        public static AppSettings LoadSettings()
        {
            try
            {
                if (File.Exists(SettingsFilePath))
                {
                    var json = File.ReadAllText(SettingsFilePath);
                    var settings = JsonConvert.DeserializeObject<AppSettings>(json);
                    return settings ?? new AppSettings();
                }
            }
            catch (Exception ex)
            {
                // 如果加载失败，记录错误但不抛出异常
                Console.WriteLine($"加载设置失败: {ex.Message}");
            }

            return new AppSettings();
        }

        /// <summary>
        /// 保存设置
        /// </summary>
        /// <param name="settings">要保存的设置</param>
        public static void SaveSettings(AppSettings settings)
        {
            try
            {
                // 确保目录存在
                var directory = Path.GetDirectoryName(SettingsFilePath);
                if (!string.IsNullOrEmpty(directory) && !Directory.Exists(directory))
                {
                    Directory.CreateDirectory(directory);
                }

                // 序列化并保存
                var json = JsonConvert.SerializeObject(settings, Formatting.Indented);
                File.WriteAllText(SettingsFilePath, json);
            }
            catch (Exception ex)
            {
                // 如果保存失败，记录错误但不抛出异常
                Console.WriteLine($"保存设置失败: {ex.Message}");
            }
        }

        /// <summary>
        /// 获取设置文件路径
        /// </summary>
        /// <returns>设置文件的完整路径</returns>
        public static string GetSettingsFilePath()
        {
            return SettingsFilePath;
        }
    }
} 