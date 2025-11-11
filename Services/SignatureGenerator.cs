using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Web;
using Microsoft.ClearScript.V8;

namespace DouyinDanmu.Services
{
    /// <summary>
    /// 签名生成器（使用内置V8 JavaScript引擎）
    /// </summary>
    public class SignatureGenerator : IDisposable
    {
        private bool _disposed = false;
        private readonly string? _jsContent;
        private readonly string? _abogusJsContent;
        private readonly string? _webMsSdkJsContent;
        private readonly V8ScriptEngine? _jsEngine;
        private readonly object _engineLock = new object();

        public SignatureGenerator()
        {
            try
            {
                Console.WriteLine("🔧 正在初始化JavaScript签名引擎...");
                
                // 从嵌入资源中读取sign.js内容
                _jsContent = GetEmbeddedResource("DouyinDanmu.sign.js");
                Console.WriteLine($"📄 嵌入资源读取: {(_jsContent?.Length > 0 ? $"成功 ({_jsContent.Length} 字符)" : "失败")}");
                
                // 如果嵌入资源读取失败，尝试从文件系统读取（向后兼容）
                if (string.IsNullOrEmpty(_jsContent))
                {
                    Console.WriteLine("🔍 尝试从文件系统读取sign.js...");
                    var jsFilePath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "sign.js");
                    Console.WriteLine($"📁 检查路径: {jsFilePath}");
                    
                    if (File.Exists(jsFilePath))
                    {
                        _jsContent = File.ReadAllText(jsFilePath, Encoding.UTF8);
                        Console.WriteLine($"📄 文件读取成功: {_jsContent.Length} 字符");
                    }
                    else
                    {
                        Console.WriteLine("❌ 当前目录未找到sign.js，尝试上级目录...");
                        // 尝试从上级目录查找
                        var parentDir = Directory.GetParent(AppDomain.CurrentDomain.BaseDirectory)?.Parent?.Parent?.Parent?.FullName;
                        if (parentDir != null)
                        {
                            var originalPath = Path.Combine(parentDir, "DouyinLiveWebFetcher", "sign.js");
                            Console.WriteLine($"📁 检查上级路径: {originalPath}");
                            if (File.Exists(originalPath))
                            {
                                _jsContent = File.ReadAllText(originalPath, Encoding.UTF8);
                                Console.WriteLine($"📄 上级目录读取成功: {_jsContent.Length} 字符");
                            }
                        }
                    }
                }

                Console.WriteLine("🚀 正在初始化V8 JavaScript引擎...");
                _jsEngine = new V8ScriptEngine();
                _jsEngine.Execute("var window = window || {}; var globalThis = this;");
                if (!string.IsNullOrEmpty(_jsContent))
                {
                    Console.WriteLine("📜 正在执行sign.js...");
                    _jsEngine.Execute(_jsContent);
                    var testResult = _jsEngine.Evaluate("typeof get_sign");
                    Console.WriteLine($"🔍 get_sign函数类型: {testResult}");
                    if (testResult?.ToString() != "function")
                    {
                        Console.WriteLine("⚠️ get_sign函数未找到，可能JavaScript代码有问题");
                    }
                }
                else
                {
                    Console.WriteLine("⚠️ 未找到sign.js文件，将使用简化签名算法");
                }

                try
                {
                    var abPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "demo", "a_bogus.js");
                    if (File.Exists(abPath))
                    {
                        _abogusJsContent = File.ReadAllText(abPath, Encoding.UTF8);
                        Console.WriteLine($"📄 加载a_bogus.js: {_abogusJsContent.Length} 字符");
                        _jsEngine?.Execute(_abogusJsContent);
                    }
                    var msPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "demo", "webmssdk.js");
                    if (File.Exists(msPath))
                    {
                        _webMsSdkJsContent = File.ReadAllText(msPath, Encoding.UTF8);
                        Console.WriteLine($"📄 加载webmssdk.js: {_webMsSdkJsContent.Length} 字符");
                        _jsEngine?.Execute(_webMsSdkJsContent);
                    }
                }
                catch (Exception e2)
                {
                    Console.WriteLine($"⚠️ 执行外部JS失败: {e2.Message}");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"❌ JavaScript引擎初始化失败: {ex.Message}");
                Console.WriteLine($"🔍 异常类型: {ex.GetType().Name}");
                if (ex.InnerException != null)
                {
                    Console.WriteLine($"🔍 内部异常: {ex.InnerException.Message}");
                }
                Console.WriteLine("🔄 将使用简化签名算法作为备用方案");
                
                _jsEngine?.Dispose();
                _jsEngine = null;
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
                // 如果V8引擎可用，使用完整的JavaScript算法
                if (_jsEngine != null)
                {
                    return ExecuteJavaScriptSignature(wssUrl);
                }
                
                // 否则使用简化版本
                return GenerateSimpleSignature(wssUrl);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"签名生成失败，使用简化算法: {ex.Message}");
                // 如果JavaScript执行失败，回退到简化版本
                return GenerateSimpleSignature(wssUrl);
            }
        }

        public string GenerateABogus(string dpf, string ua)
        {
            if (_jsEngine == null)
                return string.Empty;
            lock (_engineLock)
            {
                try
                {
                    var dpfEsc = EscapeJsString(dpf);
                    var uaEsc = EscapeJsString(ua);
                    var result = _jsEngine.Evaluate($"get_ab('{dpfEsc}','{uaEsc}')");
                    return result?.ToString() ?? string.Empty;
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"生成a_bogus失败: {ex.Message}");
                    return string.Empty;
                }
            }
        }

        public string GenerateAcSignature(string siteWithoutScheme, string nonce, string ua, long? timestamp = null)
        {
            var ts = timestamp ?? DateTimeOffset.UtcNow.ToUnixTimeSeconds();

            static uint CalOneStr(string s, uint init)
            {
                uint k = init;
                foreach (var ch in s)
                {
                    k = (uint)(((k ^ ch) * 65599) & 0xFFFFFFFF);
                }
                return k;
            }

            static uint CalOneStr3(string s, uint init)
            {
                uint k = init;
                foreach (var ch in s)
                {
                    k = (uint)((k * 65599 + ch) & 0xFFFFFFFF);
                }
                return k;
            }

            static char GetOneChr(int enc)
            {
                if (enc < 26) return (char)(enc + 65);
                if (enc < 52) return (char)(enc + 71);
                if (enc < 62) return (char)(enc - 4);
                return (char)(enc - 17);
            }

            static string EncNumToStr(uint val)
            {
                var sb = new StringBuilder(4);
                for (int i = 24; i >= 0; i -= 6)
                {
                    int bits = (int)((val >> i) & 63);
                    sb.Append(GetOneChr(bits));
                }
                return sb.ToString();
            }

            var signHead = "_02B4Z6wo00f01";
            var timeStampStr = ts.ToString();

            var a = CalOneStr(siteWithoutScheme, CalOneStr(timeStampStr, 0)) % 65521;

            var binStr = Convert.ToString((int)(ts ^ (a * 65521)), 2).PadLeft(32, '0');
            var b = Convert.ToUInt64("10000000110000" + binStr, 2);
            var b_s = b.ToString();

            var c = CalOneStr(b_s, 0);

            var d = EncNumToStr((uint)(b >> 2));
            var e = (uint)((b / 4294967296) & 0xFFFFFFFF);
            var f = EncNumToStr((uint)((b << 28) | (e >> 4)));
            var g = (uint)(582085784 ^ b);
            var h = EncNumToStr((uint)((e << 26) | (g >> 6)));
            var i = GetOneChr((int)(g & 63));

            var j = (uint)(((CalOneStr(ua, c) % 65521) << 16) | (CalOneStr(nonce, c) % 65521));
            var k = EncNumToStr(j >> 2);
            var l = EncNumToStr((uint)((j << 28) | ((524576 ^ b) >> 4)));
            var m = EncNumToStr((uint)a);

            var n = signHead + d + f + h + i + k + l + m;
            var oHex = CalOneStr3(n, 0).ToString("x");
            var o = oHex.Length >= 2 ? oHex.Substring(oHex.Length - 2) : oHex.PadLeft(2, '0');

            return n + o;
        }

        /// <summary>
        /// 使用内置V8引擎执行JavaScript签名算法
        /// </summary>
        /// <param name="wssUrl">WebSocket URL</param>
        /// <returns>签名字符串</returns>
        private string ExecuteJavaScriptSignature(string wssUrl)
        {
            if (_jsEngine == null)
                throw new InvalidOperationException("JavaScript引擎未初始化");

            lock (_engineLock)
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

                    // 调用JavaScript的get_sign函数
                    var result = _jsEngine.Evaluate($"get_sign('{md5Hash}')");
                    
                    if (result != null)
                    {
                        return result.ToString() ?? "";
                    }
                    
                    throw new InvalidOperationException("JavaScript函数返回空值");
                }
                catch (Exception ex)
                {
                    throw new InvalidOperationException($"JavaScript执行失败: {ex.Message}", ex);
                }
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
                    paramValues.Add(value);
                }

                var combinedParams = string.Join("", paramValues);
                var hash = ComputeMD5Hash(combinedParams);
                
                // 简化的签名算法
                var timestamp = DateTimeOffset.UtcNow.ToUnixTimeSeconds();
                var randomStr = GenerateRandomString(16);
                var signatureBase = $"{hash}_{timestamp}_{randomStr}";
                
                return ComputeMD5Hash(signatureBase);
            }
            catch (Exception)
            {
                // 最后的备用方案
                return GenerateRandomString(32);
            }
        }

        private static string EscapeJsString(string s)
        {
            return s.Replace("\\", "\\\\").Replace("'", "\\'");
        }

        /// <summary>
        /// 计算MD5哈希
        /// </summary>
        /// <param name="input">输入字符串</param>
        /// <returns>MD5哈希值</returns>
        private static string ComputeMD5Hash(string input)
        {
            using (var md5 = MD5.Create())
            {
                var inputBytes = Encoding.UTF8.GetBytes(input);
                var hashBytes = md5.ComputeHash(inputBytes);
                return Convert.ToHexString(hashBytes).ToLower();
            }
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
        /// 生成MsToken
        /// </summary>
        /// <param name="length">长度</param>
        /// <returns>MsToken字符串</returns>
        public static string GenerateMsToken(int length = 107)
        {
            const string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
            var random = new Random();
            var result = new StringBuilder(length);
            
            for (int i = 0; i < length; i++)
            {
                result.Append(chars[random.Next(chars.Length)]);
            }
            
            return result.ToString();
        }

        /// <summary>
        /// 检查JavaScript引擎是否可用
        /// </summary>
        /// <returns>是否可用</returns>
        public bool IsJavaScriptEngineAvailable()
        {
            return _jsEngine != null;
        }

        /// <summary>
        /// 获取引擎状态信息
        /// </summary>
        /// <returns>状态信息</returns>
        public string GetEngineStatus()
        {
            if (_jsEngine != null)
            {
                return "内置V8 JavaScript引擎";
            }
            else
            {
                return "简化签名算法（备用方案）";
            }
        }

        public void Dispose()
        {
            if (!_disposed)
            {
                _jsEngine?.Dispose();
                _disposed = true;
            }
        }
    }
} 
