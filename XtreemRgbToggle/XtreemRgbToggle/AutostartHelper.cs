using System.Diagnostics;
using System.Text;

namespace XtreemRgbToggle;

internal enum TaskQueryResult
{
    Present,
    Absent,
    Unknown
}

internal enum AutostartInstallState
{
    Unknown,
    Installed,
    NotInstalled
}

/// <summary>
/// Pure combine/cache rules for scheduled-task query results.
/// Timeout and launch failures are Unknown and must not be cached as NotInstalled.
/// </summary>
internal static class AutostartStatusLogic
{
    public static AutostartInstallState Combine(TaskQueryResult current, TaskQueryResult legacy)
    {
        if (current == TaskQueryResult.Present || legacy == TaskQueryResult.Present)
            return AutostartInstallState.Installed;
        if (current == TaskQueryResult.Absent && legacy == TaskQueryResult.Absent)
            return AutostartInstallState.NotInstalled;
        return AutostartInstallState.Unknown;
    }

    public static bool CanCache(AutostartInstallState state) =>
        state is AutostartInstallState.Installed or AutostartInstallState.NotInstalled;
}

internal static class AutostartHelper
{
    public const string TaskName = "XtreemRgbToggle-Off";
    private const string LegacyTaskName = "XtreemRgbToggle_AutoOff";
    private const int QueryTimeoutMs = 1000;

    private static AutostartInstallState? _cachedStatus;
    private static readonly object _cacheLock = new();

    public static string ExePath => Environment.ProcessPath ?? throw new InvalidOperationException("Cannot determine exe path");

    public static AutostartInstallState QueryInstallState()
    {
        lock (_cacheLock)
        {
            if (_cachedStatus.HasValue)
                return _cachedStatus.Value;
        }

        using var timer = DiagnosticTimer.Start("AutostartHelper.QueryInstallState");
        var state = AutostartStatusLogic.Combine(QueryTask(TaskName), QueryTask(LegacyTaskName));
        TryCache(state);
        return state;
    }

    public static async Task<AutostartInstallState> QueryInstallStateAsync()
    {
        lock (_cacheLock)
        {
            if (_cachedStatus.HasValue)
                return _cachedStatus.Value;
        }

        return await Task.Run(QueryInstallState).ConfigureAwait(false);
    }

    /// <summary>
    /// True only when the task is confirmed present. Unknown (timeout/error) is not treated as installed.
    /// </summary>
    public static bool IsInstalled() => QueryInstallState() == AutostartInstallState.Installed;

    public static async Task<bool> IsInstalledAsync() =>
        await QueryInstallStateAsync().ConfigureAwait(false) == AutostartInstallState.Installed;

    public static void InvalidateCache()
    {
        lock (_cacheLock)
        {
            _cachedStatus = null;
        }
    }

    private static void TryCache(AutostartInstallState state)
    {
        if (!AutostartStatusLogic.CanCache(state))
            return;

        lock (_cacheLock)
        {
            _cachedStatus = state;
        }
    }

    private static TaskQueryResult QueryTask(string taskName)
    {
        using var timer = DiagnosticTimer.Start($"QueryTask({taskName})");
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
            if (proc == null)
            {
                DiagnosticTimer.Log($"QueryTask({taskName}) process start returned null");
                return TaskQueryResult.Unknown;
            }

            // QueryTimeoutMs is a maximum wait, not a measured typical duration.
            if (!proc.WaitForExit(QueryTimeoutMs))
            {
                try { proc.Kill(); } catch { }
                DiagnosticTimer.Log($"QueryTask({taskName}) timed out after {QueryTimeoutMs}ms cap");
                return TaskQueryResult.Unknown;
            }

            return proc.ExitCode == 0 ? TaskQueryResult.Present : TaskQueryResult.Absent;
        }
        catch (Exception ex)
        {
            DiagnosticTimer.Log($"QueryTask({taskName}) exception: {ex.Message}");
            return TaskQueryResult.Unknown;
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
            InvalidateCache();
            var msg = new StringBuilder();
            msg.AppendLine($"Task creation failed (exit code {proc.ExitCode})");
            if (!string.IsNullOrWhiteSpace(stderr))
                msg.AppendLine($"Error: {stderr.Trim()}");
            if (!string.IsNullOrWhiteSpace(stdout))
                msg.AppendLine($"Output: {stdout.Trim()}");
            throw new InvalidOperationException(msg.ToString().TrimEnd());
        }

        System.Threading.Thread.Sleep(500);

        InvalidateCache();
        var verify = QueryTask(TaskName);
        if (verify == TaskQueryResult.Present)
        {
            TryCache(AutostartInstallState.Installed);
            return;
        }

        var fail = new StringBuilder();
        fail.AppendLine(verify == TaskQueryResult.Unknown
            ? "Task creation completed, but verification timed out or failed to query."
            : "Task creation completed, but verification did not find the task.");
        fail.AppendLine($"Check Task Scheduler manually: {TaskName}");
        if (!string.IsNullOrWhiteSpace(stdout))
            fail.AppendLine($"Creation output: {stdout.Trim()}");
        throw new InvalidOperationException(fail.ToString().TrimEnd());
    }

    public static void Uninstall()
    {
        using var timer = DiagnosticTimer.Start("AutostartHelper.Uninstall");

        InvalidateCache();

        var current = QueryTask(TaskName);
        var legacy = QueryTask(LegacyTaskName);

        if (current == TaskQueryResult.Present)
            DeleteTask(TaskName);
        if (legacy == TaskQueryResult.Present)
            DeleteTask(LegacyTaskName);

        System.Threading.Thread.Sleep(500);

        InvalidateCache();
        var after = AutostartStatusLogic.Combine(QueryTask(TaskName), QueryTask(LegacyTaskName));
        if (after == AutostartInstallState.NotInstalled)
        {
            TryCache(AutostartInstallState.NotInstalled);
            return;
        }

        if (after == AutostartInstallState.Unknown)
        {
            throw new InvalidOperationException(
                $"Task deletion ran, but verification timed out or failed to query.\n" +
                $"Check Task Scheduler manually: {TaskName}");
        }

        throw new InvalidOperationException(
            $"Task deletion completed, but the task still exists.\n" +
            $"Check Task Scheduler manually: {TaskName}");
    }

    private static void CleanupLegacyTask()
    {
        if (QueryTask(LegacyTaskName) == TaskQueryResult.Present)
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
            proc.WaitForExit(3000);
    }
}
