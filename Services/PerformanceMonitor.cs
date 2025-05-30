using System;
using System.Diagnostics;
using System.Threading;
using System.Threading.Tasks;

namespace DouyinDanmu.Services
{
    /// <summary>
    /// 性能监控服务
    /// </summary>
    public class PerformanceMonitor : IDisposable
    {
        private readonly System.Threading.Timer _monitorTimer;
        private readonly Process _currentProcess;
        private long _lastGcCollectionCount0;
        private long _lastGcCollectionCount1;
        private long _lastGcCollectionCount2;
        private DateTime _lastUpdateTime;
        private bool _disposed = false;

        public event EventHandler<PerformanceMetrics>? MetricsUpdated;

        public PerformanceMonitor()
        {
            _currentProcess = Process.GetCurrentProcess();
            _lastUpdateTime = DateTime.Now;
            _lastGcCollectionCount0 = GC.CollectionCount(0);
            _lastGcCollectionCount1 = GC.CollectionCount(1);
            _lastGcCollectionCount2 = GC.CollectionCount(2);

            // 每5秒更新一次性能指标
            _monitorTimer = new System.Threading.Timer(UpdateMetrics, null, TimeSpan.FromSeconds(5), TimeSpan.FromSeconds(5));
        }

        private void UpdateMetrics(object? state)
        {
            try
            {
                var now = DateTime.Now;
                var timeDiff = (now - _lastUpdateTime).TotalSeconds;

                var metrics = new PerformanceMetrics
                {
                    Timestamp = now,
                    
                    // 内存使用情况
                    WorkingSetMemoryMB = _currentProcess.WorkingSet64 / 1024.0 / 1024.0,
                    PrivateMemoryMB = _currentProcess.PrivateMemorySize64 / 1024.0 / 1024.0,
                    ManagedMemoryMB = GC.GetTotalMemory(false) / 1024.0 / 1024.0,
                    
                    // GC统计
                    Gen0Collections = GC.CollectionCount(0),
                    Gen1Collections = GC.CollectionCount(1),
                    Gen2Collections = GC.CollectionCount(2),
                    
                    // GC频率（每秒）
                    Gen0CollectionsPerSecond = (GC.CollectionCount(0) - _lastGcCollectionCount0) / timeDiff,
                    Gen1CollectionsPerSecond = (GC.CollectionCount(1) - _lastGcCollectionCount1) / timeDiff,
                    Gen2CollectionsPerSecond = (GC.CollectionCount(2) - _lastGcCollectionCount2) / timeDiff,
                    
                    // CPU使用率
                    CpuUsagePercent = _currentProcess.TotalProcessorTime.TotalMilliseconds / Environment.ProcessorCount / timeDiff / 10.0,
                    
                    // 线程数
                    ThreadCount = _currentProcess.Threads.Count,
                    
                    // 句柄数
                    HandleCount = _currentProcess.HandleCount
                };

                // 更新上次记录的值
                _lastGcCollectionCount0 = metrics.Gen0Collections;
                _lastGcCollectionCount1 = metrics.Gen1Collections;
                _lastGcCollectionCount2 = metrics.Gen2Collections;
                _lastUpdateTime = now;

                MetricsUpdated?.Invoke(this, metrics);
            }
            catch (Exception ex)
            {
                // 忽略性能监控错误，不影响主程序
                Console.WriteLine($"[PerformanceMonitor] 更新性能指标失败: {ex.Message}");
            }
        }

        /// <summary>
        /// 手动触发GC并获取内存使用情况
        /// </summary>
        public MemoryInfo GetMemoryInfo()
        {
            var beforeGC = GC.GetTotalMemory(false);
            GC.Collect();
            GC.WaitForPendingFinalizers();
            GC.Collect();
            var afterGC = GC.GetTotalMemory(false);

            return new MemoryInfo
            {
                BeforeGCBytes = beforeGC,
                AfterGCBytes = afterGC,
                ReleasedBytes = beforeGC - afterGC,
                WorkingSetBytes = _currentProcess.WorkingSet64,
                PrivateMemoryBytes = _currentProcess.PrivateMemorySize64
            };
        }

        /// <summary>
        /// 获取当前性能快照
        /// </summary>
        public PerformanceSnapshot GetSnapshot()
        {
            return new PerformanceSnapshot
            {
                Timestamp = DateTime.Now,
                WorkingSetMB = _currentProcess.WorkingSet64 / 1024.0 / 1024.0,
                ManagedMemoryMB = GC.GetTotalMemory(false) / 1024.0 / 1024.0,
                ThreadCount = _currentProcess.Threads.Count,
                HandleCount = _currentProcess.HandleCount,
                Gen0Collections = GC.CollectionCount(0),
                Gen1Collections = GC.CollectionCount(1),
                Gen2Collections = GC.CollectionCount(2)
            };
        }

        public void Dispose()
        {
            if (!_disposed)
            {
                _monitorTimer?.Dispose();
                _currentProcess?.Dispose();
                _disposed = true;
            }
        }
    }

    /// <summary>
    /// 性能指标数据
    /// </summary>
    public class PerformanceMetrics
    {
        public DateTime Timestamp { get; set; }
        
        // 内存指标
        public double WorkingSetMemoryMB { get; set; }
        public double PrivateMemoryMB { get; set; }
        public double ManagedMemoryMB { get; set; }
        
        // GC指标
        public long Gen0Collections { get; set; }
        public long Gen1Collections { get; set; }
        public long Gen2Collections { get; set; }
        public double Gen0CollectionsPerSecond { get; set; }
        public double Gen1CollectionsPerSecond { get; set; }
        public double Gen2CollectionsPerSecond { get; set; }
        
        // 系统指标
        public double CpuUsagePercent { get; set; }
        public int ThreadCount { get; set; }
        public int HandleCount { get; set; }
    }

    /// <summary>
    /// 内存信息
    /// </summary>
    public class MemoryInfo
    {
        public long BeforeGCBytes { get; set; }
        public long AfterGCBytes { get; set; }
        public long ReleasedBytes { get; set; }
        public long WorkingSetBytes { get; set; }
        public long PrivateMemoryBytes { get; set; }

        public double BeforeGCMB => BeforeGCBytes / 1024.0 / 1024.0;
        public double AfterGCMB => AfterGCBytes / 1024.0 / 1024.0;
        public double ReleasedMB => ReleasedBytes / 1024.0 / 1024.0;
        public double WorkingSetMB => WorkingSetBytes / 1024.0 / 1024.0;
        public double PrivateMemoryMB => PrivateMemoryBytes / 1024.0 / 1024.0;
    }

    /// <summary>
    /// 性能快照
    /// </summary>
    public class PerformanceSnapshot
    {
        public DateTime Timestamp { get; set; }
        public double WorkingSetMB { get; set; }
        public double ManagedMemoryMB { get; set; }
        public int ThreadCount { get; set; }
        public int HandleCount { get; set; }
        public long Gen0Collections { get; set; }
        public long Gen1Collections { get; set; }
        public long Gen2Collections { get; set; }

        public override string ToString()
        {
            return $"内存: {WorkingSetMB:F1}MB (托管: {ManagedMemoryMB:F1}MB), " +
                   $"线程: {ThreadCount}, 句柄: {HandleCount}, " +
                   $"GC: G0={Gen0Collections} G1={Gen1Collections} G2={Gen2Collections}";
        }
    }
} 