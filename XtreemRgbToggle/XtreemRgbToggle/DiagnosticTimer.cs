using System.Diagnostics;

namespace XtreemRgbToggle;

/// <summary>
/// Lightweight diagnostic timing for opt-in performance logging.
/// No secrets, no PII - only operation names and elapsed milliseconds.
/// </summary>
internal sealed class DiagnosticTimer : IDisposable
{
    private static readonly Stopwatch _appStart = Stopwatch.StartNew();
    private static bool _enabled;
    private static readonly object _logLock = new();
    
    private readonly string _operation;
    private readonly Stopwatch _sw;
    
    private DiagnosticTimer(string operation)
    {
        _operation = operation;
        _sw = Stopwatch.StartNew();
    }
    
    public static bool Enabled
    {
        get => _enabled;
        set
        {
            _enabled = value;
            if (value)
            {
                LogDirect("DIAG", $"Diagnostic timing enabled (app started {_appStart.ElapsedMilliseconds}ms ago)");
            }
        }
    }
    
    public static DiagnosticTimer Start(string operation)
    {
        if (_enabled)
        {
            LogDirect("START", $"{operation} (app+{_appStart.ElapsedMilliseconds}ms)");
        }
        return new DiagnosticTimer(operation);
    }
    
    public void Dispose()
    {
        if (_enabled)
        {
            _sw.Stop();
            LogDirect("DONE", $"{_operation} took {_sw.ElapsedMilliseconds}ms (app+{_appStart.ElapsedMilliseconds}ms)");
        }
    }
    
    public static void Log(string message)
    {
        if (_enabled)
        {
            LogDirect("INFO", $"{message} (app+{_appStart.ElapsedMilliseconds}ms)");
        }
    }
    
    private static void LogDirect(string level, string message)
    {
        lock (_logLock)
        {
            try
            {
                string logPath = Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                    "XtreemRgbToggle", "diagnostic.log");
                
                Directory.CreateDirectory(Path.GetDirectoryName(logPath)!);
                
                string line = $"[{DateTime.Now:yyyy-MM-dd HH:mm:ss.fff}] [{level}] {message}";
                File.AppendAllText(logPath, line + Environment.NewLine);
                Console.WriteLine(line);
            }
            catch
            {
                // Diagnostic logging must never crash the app
            }
        }
    }
}
