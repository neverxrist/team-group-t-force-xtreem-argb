using System.Diagnostics;
using System.Text;

namespace XtreemRgbToggle;

internal static class AutostartHelper
{
    public const string TaskName = "XtreemRgbToggle-Off";
    private const string LegacyTaskName = "XtreemRgbToggle_AutoOff";

    public static string ExePath => Environment.ProcessPath ?? throw new InvalidOperationException("Cannot determine exe path");

    public static bool IsInstalled()
    {
        return TaskExists(TaskName) || TaskExists(LegacyTaskName);
    }

    private static bool TaskExists(string taskName)
    {
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
            proc.WaitForExit(3000);
            return proc.ExitCode == 0;
        }
        catch
        {
            return false;
        }
    }

    public static void Install()
    {
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

        if (!TaskExists(TaskName))
        {
            var msg = new StringBuilder();
            msg.AppendLine($"Task creation completed, but verification failed.");
            msg.AppendLine($"Check Task Scheduler manually: {TaskName}");
            if (!string.IsNullOrWhiteSpace(stdout))
                msg.AppendLine($"Creation output: {stdout.Trim()}");
            throw new InvalidOperationException(msg.ToString().TrimEnd());
        }
    }

    public static void Uninstall()
    {
        bool hadCurrent = TaskExists(TaskName);
        bool hadLegacy = TaskExists(LegacyTaskName);

        if (hadCurrent)
            DeleteTask(TaskName);
        
        if (hadLegacy)
            DeleteTask(LegacyTaskName);

        System.Threading.Thread.Sleep(500);

        if (TaskExists(TaskName) || TaskExists(LegacyTaskName))
        {
            throw new InvalidOperationException(
                $"Task deletion completed, but verification failed.\n" +
                $"Check Task Scheduler manually: {TaskName}");
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
