using System;
using System.IO;
using Newtonsoft.Json;
using DouyinDanmu.Models;
using System.Diagnostics;

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
                Debug.WriteLine($"[SettingsManager] 尝试加载设置文件: {SettingsFilePath}");
                
                if (File.Exists(SettingsFilePath))
                {
                    var json = File.ReadAllText(SettingsFilePath);
                    var settings = JsonConvert.DeserializeObject<AppSettings>(json);
                    Debug.WriteLine($"[SettingsManager] 成功加载设置，关注用户数: {settings?.WatchedUserIds?.Count ?? 0}");
                    return settings ?? new AppSettings();
                }
                else
                {
                    Debug.WriteLine($"[SettingsManager] 设置文件不存在，返回默认设置");
                }
            }
            catch (Exception ex)
            {
                // 如果加载失败，记录错误但不抛出异常
                Debug.WriteLine($"[SettingsManager] 加载设置失败: {ex.Message}");
                Console.WriteLine($"加载设置失败: {ex.Message}");
            }

            return new AppSettings();
        }

        /// <summary>
        /// 保存设置
        /// </summary>
        /// <param name="settings">要保存的设置</param>
        /// <returns>是否保存成功</returns>
        public static bool SaveSettings(AppSettings settings)
        {
            try
            {
                Debug.WriteLine($"[SettingsManager] 尝试保存设置到: {SettingsFilePath}");
                Debug.WriteLine($"[SettingsManager] 关注用户数: {settings?.WatchedUserIds?.Count ?? 0}");
                
                // 确保目录存在
                var directory = Path.GetDirectoryName(SettingsFilePath);
                if (!string.IsNullOrEmpty(directory))
                {
                    if (!Directory.Exists(directory))
                    {
                        Debug.WriteLine($"[SettingsManager] 创建目录: {directory}");
                        Directory.CreateDirectory(directory);
                    }
                    else
                    {
                        Debug.WriteLine($"[SettingsManager] 目录已存在: {directory}");
                    }
                }

                // 序列化并保存
                var json = JsonConvert.SerializeObject(settings, Formatting.Indented);
                File.WriteAllText(SettingsFilePath, json);
                
                // 验证文件是否成功创建
                if (File.Exists(SettingsFilePath))
                {
                    var fileInfo = new FileInfo(SettingsFilePath);
                    Debug.WriteLine($"[SettingsManager] 设置保存成功，文件大小: {fileInfo.Length} 字节");
                    return true;
                }
                else
                {
                    Debug.WriteLine($"[SettingsManager] 设置保存失败，文件未创建");
                    return false;
                }
            }
            catch (Exception ex)
            {
                // 如果保存失败，记录错误但不抛出异常
                Debug.WriteLine($"[SettingsManager] 保存设置失败: {ex.Message}");
                Console.WriteLine($"保存设置失败: {ex.Message}");
                return false;
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

        /// <summary>
        /// 检查设置文件是否存在
        /// </summary>
        /// <returns>设置文件是否存在</returns>
        public static bool SettingsFileExists()
        {
            return File.Exists(SettingsFilePath);
        }

        /// <summary>
        /// 获取设置文件信息
        /// </summary>
        /// <returns>设置文件信息字符串</returns>
        public static string GetSettingsFileInfo()
        {
            try
            {
                if (File.Exists(SettingsFilePath))
                {
                    var fileInfo = new FileInfo(SettingsFilePath);
                    return $"文件存在，大小: {fileInfo.Length} 字节，最后修改: {fileInfo.LastWriteTime}";
                }
                else
                {
                    return "文件不存在";
                }
            }
            catch (Exception ex)
            {
                return $"获取文件信息失败: {ex.Message}";
            }
        }
    }
} 