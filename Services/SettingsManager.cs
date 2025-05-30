using System;
using System.IO;
using System.Text.Json;
using DouyinDanmu.Models;
using System.Diagnostics;

namespace DouyinDanmu.Services
{
    /// <summary>
    /// 设置管理器
    /// </summary>
    public static class SettingsManager
    {
        private static readonly string SettingsDirectory = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "Temp", ".net", "DouyinDanmu"
        );
        
        private static readonly string SettingsFilePath = Path.Combine(SettingsDirectory, "settings.json");

        /// <summary>
        /// 加载应用程序设置
        /// </summary>
        public static AppSettings LoadSettings()
        {
            try
            {
                if (!File.Exists(SettingsFilePath))
                {
                    return new AppSettings();
                }

                var json = File.ReadAllText(SettingsFilePath);
                var settings = JsonSerializer.Deserialize<AppSettings>(json);
                
                return settings ?? new AppSettings();
            }
            catch (Exception ex)
            {
                // 记录错误但返回默认设置，不影响程序运行
                Console.WriteLine($"[SettingsManager] 加载设置失败: {ex.Message}");
                return new AppSettings();
            }
        }

        /// <summary>
        /// 保存应用程序设置
        /// </summary>
        public static bool SaveSettings(AppSettings settings)
        {
            try
            {
                // 确保目录存在
                if (!Directory.Exists(SettingsDirectory))
                {
                    Directory.CreateDirectory(SettingsDirectory);
                }

                var options = new JsonSerializerOptions
                {
                    WriteIndented = true,
                    Encoder = System.Text.Encodings.Web.JavaScriptEncoder.UnsafeRelaxedJsonEscaping
                };

                var json = JsonSerializer.Serialize(settings, options);
                File.WriteAllText(SettingsFilePath, json);

                // 验证文件是否成功创建
                return File.Exists(SettingsFilePath);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[SettingsManager] 保存设置失败: {ex.Message}");
                return false;
            }
        }

        /// <summary>
        /// 获取设置文件路径
        /// </summary>
        public static string GetSettingsFilePath()
        {
            return SettingsFilePath;
        }

        /// <summary>
        /// 检查设置文件是否存在
        /// </summary>
        public static bool SettingsFileExists()
        {
            return File.Exists(SettingsFilePath);
        }

        /// <summary>
        /// 删除设置文件
        /// </summary>
        public static bool DeleteSettings()
        {
            try
            {
                if (File.Exists(SettingsFilePath))
                {
                    File.Delete(SettingsFilePath);
                }
                return true;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[SettingsManager] 删除设置文件失败: {ex.Message}");
                return false;
            }
        }
    }
} 