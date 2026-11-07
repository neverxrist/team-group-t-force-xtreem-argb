using System.Diagnostics;
using System.Text;

namespace XtreemRgbToggle;

internal static class AutostartHelper
{
    public const string TaskName = "XtreemRgbToggle-Off";
    private const string LegacyTaskName = "XtreemRgbToggle_AutoOff";

    private static bool? _cachedStatus;
    private static readonly object _cacheLock = new();

    public static string ExePath => Environment.ProcessPath ?? throw new InvalidOperationException("Cannot determine exe path");

    /// <summary>
    /// Check if autostart task is installed (cached result, non-blocking after first call).
    /// Call InvalidateCache() after Install/Uninstall to refresh.
    /// </summary>
    public static bool IsInstalled()
    {
        lock (_cacheLock)
        {
            if (_cachedStatus.HasValue)
                return _cachedStatus.Value;
            
            // First call: perform actual check
            using var timer = DiagnosticTimer.Start("AutostartHelper.IsInstalled (first check)");
            bool result = TaskExists(TaskName) || TaskExists(LegacyTaskName);
            _cachedStatus = result;
            return result;
        }
    }

    /// <summary>
    /// Check if autostart task is installed asynchronously (never blocks UI thread).
    /// </summary>
    public static async Task<bool> IsInstalledAsync()
    {
        lock (_cacheLock)
        {
            if (_cachedStatus.HasValue)
                return _cachedStatus.Value;
        }

        return await Task.Run(() =>
        {
            using var timer = DiagnosticTimer.Start("AutostartHelper.IsInstalledAsync (background check)");
            bool result = TaskExists(TaskName) || TaskExists(LegacyTaskName);
            lock (_cacheLock)
            {
                _cachedStatus = result;
            }
            return result;
        });
    }

    /// <summary>
    /// Invalidate cached status (call after Install/Uninstall).
    /// </summary>
    public static void InvalidateCache()
    {
        lock (_cacheLock)
        {
            _cachedStatus = null;
        }
    }

    private static bool TaskExists(string taskName)
    {
        using var timer = DiagnosticTimer.Start($"TaskExists({taskName})");
        try
        {
            var psi = new ProcessStartInfo
            {
                FileName = "schtasks.exe",
                Arguments = $"/Query /TN \"{taskName}\" /FO LIST",
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                CreateNoWindow = true
            };

            using var proc = Process.Start(psi);
            if (proc == null) return false;
            
            // Reduced timeout: schtasks should respond quickly
            bool exited = proc.WaitForExit(1000);
            if (!exited)
            {
                try { proc.Kill(); } catch { }
                DiagnosticTimer.Log($"TaskExists({taskName}) timeout after 1000ms");
                return false;
            }
            
            return proc.ExitCode == 0;
        }
        catch (Exception ex)
        {
            DiagnosticTimer.Log($"TaskExists({taskName}) exception: {ex.Message}");
            return false;
        }
    }

    public static void Install()
    {
        using var timer = DiagnosticTimer.Start("AutostartHelper.Install");
        
        CleanupLegacyTask();

        string exePath = ExePath;
        string taskAction = $"\"\\\"{exePath}\\\" off\"";
        string args = $"/Create /F /TN \"{TaskName}\" /TR {taskAction} /SC ONLOGON /RL HIGHEST /DELAY 0000:00";

        var psi = new ProcessStartInfo
        {
            FileName = "schtasks.exe",
            Arguments = args,
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true
        };

        using var proc = Process.Start(psi) ?? throw new InvalidOperationException("Failed to start schtasks.exe");
        proc.WaitForExit();

        var stdout = proc.StandardOutput.ReadToEnd();
        var stderr = proc.StandardError.ReadToEnd();

        if (proc.ExitCode != 0)
        {
            var msg = new StringBuilder();
            msg.AppendLine($"Task creation failed (exit code {proc.ExitCode})");
            if (!string.IsNullOrWhiteSpace(stderr))
                msg.AppendLine($"Error: {stderr.Trim()}");
            if (!string.IsNullOrWhiteSpace(stdout))
                msg.AppendLine($"Output: {stdout.Trim()}");
            throw new InvalidOperationException(msg.ToString().TrimEnd());
        }

        System.Threading.Thread.Sleep(500);

        // Verification: use quick in-memory check first
        InvalidateCache();
        if (!TaskExists(TaskName))
        {
            var msg = new StringBuilder();
            msg.AppendLine($"Task creation completed, but verification failed.");
            msg.AppendLine($"Check Task Scheduler manually: {TaskName}");
            if (!string.IsNullOrWhiteSpace(stdout))
                msg.AppendLine($"Creation output: {stdout.Trim()}");
            throw new InvalidOperationException(msg.ToString().TrimEnd());
        }
        
        // Update cache
        lock (_cacheLock)
        {
            _cachedStatus = true;
        }
    }

    public static void Uninstall()
    {
        using var timer = DiagnosticTimer.Start("AutostartHelper.Uninstall");
        
        InvalidateCache();
        
        bool hadCurrent = TaskExists(TaskName);
        bool hadLegacy = TaskExists(LegacyTaskName);

        if (hadCurrent)
            DeleteTask(TaskName);
        
        if (hadLegacy)
            DeleteTask(LegacyTaskName);

        System.Threading.Thread.Sleep(500);

        InvalidateCache();
        if (TaskExists(TaskName) || TaskExists(LegacyTaskName))
        {
            throw new InvalidOperationException(
                $"Task deletion completed, but verification failed.\n" +
                $"Check Task Scheduler manually: {TaskName}");
        }
        
        // Update cache
        lock (_cacheLock)
        {
            _cachedStatus = false;
        }
    }

    private static void CleanupLegacyTask()
    {
        if (TaskExists(LegacyTaskName))
            DeleteTask(LegacyTaskName);
    }

    private static void DeleteTask(string taskName)
    {
        var psi = new ProcessStartInfo
        {
            FileName = "schtasks.exe",
            Arguments = $"/Delete /F /TN \"{taskName}\"",
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true
        };

        using var proc = Process.Start(psi);
        if (proc != null)
        {
            proc.WaitForExit(3000);
        }
    }
}
