using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Web;

namespace DouyinDanmu.Services
{
    /// <summary>
    /// 签名生成器（使用Node.js执行原始JavaScript算法）
    /// </summary>
    public class SignatureGenerator : IDisposable
    {
        private bool _disposed = false;
        private readonly string _jsContent;

        public SignatureGenerator()
        {
            // 从嵌入资源中读取sign.js内容
            _jsContent = GetEmbeddedResource("DouyinDanmu.sign.js");
            
            // 如果嵌入资源读取失败，尝试从文件系统读取（向后兼容）
            if (string.IsNullOrEmpty(_jsContent))
            {
                var jsFilePath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "sign.js");
                if (File.Exists(jsFilePath))
                {
                    _jsContent = File.ReadAllText(jsFilePath, Encoding.UTF8);
                }
                else
                {
                    // 尝试从上级目录查找
                    var parentDir = Directory.GetParent(AppDomain.CurrentDomain.BaseDirectory)?.Parent?.Parent?.Parent?.FullName;
                    if (parentDir != null)
                    {
                        var originalPath = Path.Combine(parentDir, "DouyinLiveWebFetcher", "sign.js");
                        if (File.Exists(originalPath))
                        {
                            _jsContent = File.ReadAllText(originalPath, Encoding.UTF8);
                        }
                    }
                }
            }
        }

        /// <summary>
        /// 从嵌入资源中获取文件内容
        /// </summary>
        /// <param name="resourceName">资源名称</param>
        /// <returns>资源内容</returns>
        private string GetEmbeddedResource(string resourceName)
        {
            try
            {
                var assembly = Assembly.GetExecutingAssembly();
                using (var stream = assembly.GetManifestResourceStream(resourceName))
                {
                    if (stream == null)
                        return string.Empty;
                    
                    using (var reader = new StreamReader(stream, Encoding.UTF8))
                    {
                        return reader.ReadToEnd();
                    }
                }
            }
            catch
            {
                return string.Empty;
            }
        }

        /// <summary>
        /// 生成签名
        /// </summary>
        /// <param name="wssUrl">WebSocket URL</param>
        /// <returns>签名字符串</returns>
        public string GenerateSignature(string wssUrl)
        {
            try
            {
                // 如果找不到sign.js内容，使用简化版本
                if (string.IsNullOrEmpty(_jsContent))
                {
                    return GenerateSimpleSignature(wssUrl);
                }

                var uri = new Uri(wssUrl);
                var query = HttpUtility.ParseQueryString(uri.Query);
                
                var parameters = new[]
                {
                    "live_id", "aid", "version_code", "webcast_sdk_version",
                    "room_id", "sub_room_id", "sub_channel_id", "did_rule",
                    "user_unique_id", "device_platform", "device_type", "ac",
                    "identity"
                };

                var paramValues = new List<string>();
                foreach (var param in parameters)
                {
                    var value = query[param] ?? "";
                    paramValues.Add($"{param}={value}");
                }

                var paramString = string.Join(",", paramValues);
                var md5Hash = ComputeMD5Hash(paramString);

                // 使用Node.js执行JavaScript签名算法
                return ExecuteJavaScriptSignature(md5Hash);
            }
            catch (Exception)
            {
                // 如果JavaScript执行失败，回退到简化版本
                return GenerateSimpleSignature(wssUrl);
            }
        }

        /// <summary>
        /// 使用Node.js执行JavaScript签名算法
        /// </summary>
        /// <param name="md5Hash">MD5哈希值</param>
        /// <returns>签名字符串</returns>
        private string ExecuteJavaScriptSignature(string md5Hash)
        {
            try
            {
                // 创建临时的Node.js脚本
                var tempScript = Path.GetTempFileName() + ".js";
                var jsCode = $@"
// 嵌入的sign.js内容
{_jsContent}

// 调用get_sign函数
const signature = get_sign('{md5Hash}');
console.log(signature);
";

                File.WriteAllText(tempScript, jsCode, Encoding.UTF8);

                // 执行Node.js
                var processInfo = new ProcessStartInfo
                {
                    FileName = "node",
                    Arguments = $"\"{tempScript}\"",
                    UseShellExecute = false,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    CreateNoWindow = true,
                    StandardOutputEncoding = Encoding.UTF8
                };

                using (var process = Process.Start(processInfo))
                {
                    if (process != null)
                    {
                        var output = process.StandardOutput.ReadToEnd();
                        var error = process.StandardError.ReadToEnd();
                        
                        process.WaitForExit(10000); // 10秒超时

                        // 清理临时文件
                        try { File.Delete(tempScript); } catch { }

                        if (process.ExitCode == 0 && !string.IsNullOrWhiteSpace(output))
                        {
                            return output.Trim();
                        }
                        else
                        {
                            throw new InvalidOperationException($"Node.js execution failed: {error}");
                        }
                    }
                }

                throw new InvalidOperationException("Failed to start Node.js process");
            }
            catch (Exception ex)
            {
                throw new InvalidOperationException($"Failed to execute JavaScript signature: {ex.Message}", ex);
            }
        }

        /// <summary>
        /// 简化的签名生成（备用方案）
        /// </summary>
        /// <param name="wssUrl">WebSocket URL</param>
        /// <returns>签名字符串</returns>
        private string GenerateSimpleSignature(string wssUrl)
        {
            try
            {
                var uri = new Uri(wssUrl);
                var query = HttpUtility.ParseQueryString(uri.Query);
                
                var parameters = new[]
                {
                    "live_id", "aid", "version_code", "webcast_sdk_version",
                    "room_id", "sub_room_id", "sub_channel_id", "did_rule",
                    "user_unique_id", "device_platform", "device_type", "ac",
                    "identity"
                };

                var paramValues = new List<string>();
                foreach (var param in parameters)
                {
                    var value = query[param] ?? "";
                    paramValues.Add($"{param}={value}");
                }

                var paramString = string.Join(",", paramValues);
                var md5Hash = ComputeMD5Hash(paramString);

                // 简化的签名生成（实际项目中需要完整的算法）
                var timestamp = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();
                var random = GenerateRandomString(8);
                return $"{md5Hash}_{timestamp}_{random}";
            }
            catch (Exception ex)
            {
                throw new InvalidOperationException($"Failed to generate simple signature: {ex.Message}", ex);
            }
        }

        /// <summary>
        /// 计算MD5哈希
        /// </summary>
        /// <param name="input">输入字符串</param>
        /// <returns>MD5哈希值</returns>
        private static string ComputeMD5Hash(string input)
        {
            using var md5 = MD5.Create();
            var inputBytes = Encoding.UTF8.GetBytes(input);
            var hashBytes = md5.ComputeHash(inputBytes);
            return Convert.ToHexString(hashBytes).ToLower();
        }

        /// <summary>
        /// 生成随机字符串
        /// </summary>
        /// <param name="length">长度</param>
        /// <returns>随机字符串</returns>
        private static string GenerateRandomString(int length)
        {
            const string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
            var random = new Random();
            var result = new StringBuilder(length);

            for (int i = 0; i < length; i++)
            {
                result.Append(chars[random.Next(chars.Length)]);
            }

            return result.ToString();
        }

        /// <summary>
        /// 生成msToken
        /// </summary>
        /// <param name="length">长度，默认107</param>
        /// <returns>msToken字符串</returns>
        public static string GenerateMsToken(int length = 107)
        {
            const string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789=_";
            var random = new Random();
            var result = new StringBuilder(length);

            for (int i = 0; i < length; i++)
            {
                result.Append(chars[random.Next(chars.Length)]);
            }

            return result.ToString();
        }

        public void Dispose()
        {
            if (!_disposed)
            {
                _disposed = true;
            }
        }
    }
} 