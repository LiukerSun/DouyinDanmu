using DouyinDanmu.Models;
using System;
using System.Collections.Concurrent;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace DouyinDanmu.Services
{
    /// <summary>
    /// 日志服务
    /// </summary>
    public class LoggingService : IDisposable
    {
        private readonly LoggingSettings _settings;
        private readonly ConcurrentQueue<LogEntry> _logQueue = new();
        private readonly System.Threading.Timer _flushTimer;
        private readonly SemaphoreSlim _flushSemaphore = new(1, 1);
        private readonly string _logDirectory;
        private readonly string _logFilePrefix;
        private bool _disposed = false;

        public LoggingService(LoggingSettings settings)
        {
            _settings = settings;

            // 设置日志目录
            _logDirectory = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "DouyinDanmu", "Logs");

            if (!Directory.Exists(_logDirectory))
            {
                Directory.CreateDirectory(_logDirectory);
            }

            _logFilePrefix = "douyin-danmu";

            // 启动定时刷新
            _flushTimer = new System.Threading.Timer(FlushLogs, null, TimeSpan.FromSeconds(1), TimeSpan.FromSeconds(1));
        }

        /// <summary>
        /// 记录日志
        /// </summary>
        public void Log(LogLevel level, string message, Exception? exception = null, string? category = null)
        {
            if (level < _settings.Level)
                return;

            var entry = new LogEntry
            {
                Timestamp = DateTime.Now,
                Level = level,
                Message = message,
                Exception = exception,
                Category = category ?? "General",
                ThreadId = Environment.CurrentManagedThreadId
            };

            _logQueue.Enqueue(entry);
        }

        /// <summary>
        /// 记录跟踪日志
        /// </summary>
        public void LogTrace(string message, string? category = null)
            => Log(LogLevel.Trace, message, null, category);

        /// <summary>
        /// 记录调试日志
        /// </summary>
        public void LogDebug(string message, string? category = null)
            => Log(LogLevel.Debug, message, null, category);

        /// <summary>
        /// 记录信息日志
        /// </summary>
        public void LogInformation(string message, string? category = null)
            => Log(LogLevel.Information, message, null, category);

        /// <summary>
        /// 记录警告日志
        /// </summary>
        public void LogWarning(string message, Exception? exception = null, string? category = null)
            => Log(LogLevel.Warning, message, exception, category);

        /// <summary>
        /// 记录错误日志
        /// </summary>
        public void LogError(string message, Exception? exception = null, string? category = null)
            => Log(LogLevel.Error, message, exception, category);

        /// <summary>
        /// 记录严重错误日志
        /// </summary>
        public void LogCritical(string message, Exception? exception = null, string? category = null)
            => Log(LogLevel.Critical, message, exception, category);

        /// <summary>
        /// 刷新日志到文件
        /// </summary>
        private async void FlushLogs(object? state)
        {
            if (_logQueue.IsEmpty)
                return;

            await _flushSemaphore.WaitAsync().ConfigureAwait(false);
            try
            {
                var entries = new List<LogEntry>();

                // 取出所有待写入的日志
                while (_logQueue.TryDequeue(out var entry))
                {
                    entries.Add(entry);
                }

                if (entries.Count == 0)
                    return;

                // 写入文件日志
                if (_settings.EnableFileLogging)
                {
                    await WriteToFileAsync(entries).ConfigureAwait(false);
                }

                // 写入控制台日志
                if (_settings.EnableConsoleLogging)
                {
                    WriteToConsole(entries);
                }
            }
            catch (Exception ex)
            {
                // 日志系统本身的错误不应该影响主程序
                Console.WriteLine($"[LoggingService] 写入日志失败: {ex.Message}");
            }
            finally
            {
                _flushSemaphore.Release();
            }
        }

        /// <summary>
        /// 写入文件日志
        /// </summary>
        private async Task WriteToFileAsync(List<LogEntry> entries)
        {
            var logFileName = GetCurrentLogFileName();
            var logFilePath = Path.Combine(_logDirectory, logFileName);

            var sb = new StringBuilder();
            foreach (var entry in entries)
            {
                sb.AppendLine(FormatLogEntry(entry));
            }

            await File.AppendAllTextAsync(logFilePath, sb.ToString(), Encoding.UTF8).ConfigureAwait(false);

            // 检查文件大小并轮转
            await RotateLogFileIfNeeded(logFilePath).ConfigureAwait(false);
        }

        /// <summary>
        /// 写入控制台日志
        /// </summary>
        private static void WriteToConsole(List<LogEntry> entries)
        {
            foreach (var entry in entries)
            {
                var color = GetConsoleColor(entry.Level);
                var originalColor = Console.ForegroundColor;

                try
                {
                    Console.ForegroundColor = color;
                    Console.WriteLine(FormatLogEntry(entry));
                }
                finally
                {
                    Console.ForegroundColor = originalColor;
                }
            }
        }

        /// <summary>
        /// 格式化日志条目
        /// </summary>
        private static string FormatLogEntry(LogEntry entry)
        {
            var sb = new StringBuilder();
            sb.Append($"[{entry.Timestamp:yyyy-MM-dd HH:mm:ss.fff}] ");
            sb.Append($"[{entry.Level.ToString().ToUpper()}] ");
            sb.Append($"[{entry.Category}] ");
            sb.Append($"[T{entry.ThreadId:D2}] ");
            sb.Append(entry.Message);

            if (entry.Exception != null)
            {
                sb.AppendLine();
                sb.Append($"异常: {entry.Exception}");
            }

            return sb.ToString();
        }

        /// <summary>
        /// 获取当前日志文件名
        /// </summary>
        private string GetCurrentLogFileName()
        {
            return $"{_logFilePrefix}-{DateTime.Now:yyyyMMdd}.log";
        }

        /// <summary>
        /// 轮转日志文件
        /// </summary>
        private async Task RotateLogFileIfNeeded(string logFilePath)
        {
            if (!File.Exists(logFilePath))
                return;

            var fileInfo = new FileInfo(logFilePath);
            if (fileInfo.Length < _settings.MaxFileSizeMB * 1024 * 1024)
                return;

            // 重命名当前文件
            var timestamp = DateTime.Now.ToString("yyyyMMdd-HHmmss");
            var rotatedFileName = $"{_logFilePrefix}-{timestamp}.log";
            var rotatedFilePath = Path.Combine(_logDirectory, rotatedFileName);

            File.Move(logFilePath, rotatedFilePath);

            // 清理旧文件
            await CleanupOldLogFiles().ConfigureAwait(false);
        }

        /// <summary>
        /// 清理旧日志文件
        /// </summary>
        private async Task CleanupOldLogFiles()
        {
            await Task.Run(() =>
            {
                try
                {
                    var logFiles = Directory.GetFiles(_logDirectory, $"{_logFilePrefix}-*.log");
                    if (logFiles.Length <= _settings.MaxFileCount)
                        return;

                    // 按修改时间排序，删除最旧的文件
                    Array.Sort(logFiles, (x, y) => File.GetLastWriteTime(x).CompareTo(File.GetLastWriteTime(y)));

                    var filesToDelete = logFiles.Length - _settings.MaxFileCount;
                    for (int i = 0; i < filesToDelete; i++)
                    {
                        File.Delete(logFiles[i]);
                    }
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"[LoggingService] 清理旧日志文件失败: {ex.Message}");
                }
            }).ConfigureAwait(false);
        }

        /// <summary>
        /// 获取控制台颜色
        /// </summary>
        private static ConsoleColor GetConsoleColor(LogLevel level)
        {
            return level switch
            {
                LogLevel.Trace => ConsoleColor.Gray,
                LogLevel.Debug => ConsoleColor.White,
                LogLevel.Information => ConsoleColor.Green,
                LogLevel.Warning => ConsoleColor.Yellow,
                LogLevel.Error => ConsoleColor.Red,
                LogLevel.Critical => ConsoleColor.Magenta,
                _ => ConsoleColor.White
            };
        }

        public void Dispose()
        {
            if (!_disposed)
            {
                _flushTimer?.Dispose();

                // 最后一次刷新
                FlushLogs(null);

                _flushSemaphore?.Dispose();
                _disposed = true;
            }
        }
    }

    /// <summary>
    /// 日志条目
    /// </summary>
    internal class LogEntry
    {
        public DateTime Timestamp { get; set; }
        public LogLevel Level { get; set; }
        public string Message { get; set; } = string.Empty;
        public Exception? Exception { get; set; }
        public string Category { get; set; } = string.Empty;
        public int ThreadId { get; set; }
    }
}