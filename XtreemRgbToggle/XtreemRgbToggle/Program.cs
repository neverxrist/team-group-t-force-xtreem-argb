using System.Runtime.InteropServices;

namespace XtreemRgbToggle;

internal static class Program
{
    [STAThread]
    private static int Main(string[] args)
    {
        // Enable diagnostic logging if env var set or --diag flag
        if (Environment.GetEnvironmentVariable("XTREEM_DIAG") == "1" || 
            args.Any(a => a is "--diag" or "-d"))
        {
            DiagnosticTimer.Enabled = true;
        }
        
        using var appTimer = DiagnosticTimer.Start("Application startup");
        
        string baseDir = AppContext.BaseDirectory;
        SetDllDirectory(baseDir);

        // No arguments = GUI (double-click behavior)
        string cmd = args.Length > 0 ? args[0].Trim().ToLowerInvariant() : "gui";
        XtreemService.Trace = msg =>
        {
            try
            {
                string dir = Path.GetDirectoryName(LogPath)!;
                Directory.CreateDirectory(dir);
                File.AppendAllText(LogPath, msg + Environment.NewLine);
            }
            catch { }
        };

        bool wantConsole = cmd is not ("tray" or "gui");

        if (wantConsole)
            EnsureConsole();

        try
        {
            return cmd switch
            {
                "off" => RunOnce(wantOff: true),
                "on" => RunOnce(wantOff: false),
                "toggle" => RunToggle(),
                "status" => RunStatus(),
                "detect" => RunDetect(),
                "tray" => RunTray(),
                "gui" or "ui" => RunGui(),
                "debug" or "regs" or "dump" => RunDebug(),
                "install-autostart" or "autostart-install" => RunInstallAutostart(),
                "uninstall-autostart" or "autostart-uninstall" => RunUninstallAutostart(),
                "interactive" or "console" or "-i" => RunInteractive(),
                "help" or "-h" or "--help" => PrintHelp(),
                _ => PrintHelp($"Unknown command: {cmd}")
            };
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine("Error: " + ex.Message);
            if (args.Any(a => a is "-v" or "--verbose"))
                Console.Error.WriteLine(ex);
            if (wantConsole)
            {
                Console.WriteLine("Press Enter to exit...");
                try { Console.ReadLine(); } catch { }
            }
            return 1;
        }
    }

    private static readonly string LogPath =
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "XtreemRgbToggle", "last-run.log");

    private static void ResetLog()
    {
        try
        {
            string dir = Path.GetDirectoryName(LogPath)!;
            Directory.CreateDirectory(dir);
            File.WriteAllText(LogPath, $"=== {DateTime.Now:yyyy-MM-dd HH:mm:ss} ==={Environment.NewLine}");
        }
        catch { }
    }

    private static void Log(string msg)
    {
        Console.WriteLine(msg);
        try
        {
            string dir = Path.GetDirectoryName(LogPath)!;
            Directory.CreateDirectory(dir);
            File.AppendAllText(LogPath, msg + Environment.NewLine);
        }
        catch { }
    }

    private static void EnsureConsole()
    {
        if (GetConsoleWindow() == IntPtr.Zero)
        {
            AllocConsole();
            Console.SetOut(new StreamWriter(Console.OpenStandardOutput()) { AutoFlush = true });
            Console.SetError(new StreamWriter(Console.OpenStandardError()) { AutoFlush = true });
            Console.SetIn(new StreamReader(Console.OpenStandardInput()));
        }
        try { Console.OutputEncoding = System.Text.Encoding.UTF8; } catch { }
    }

    private static int PrintHelp(string? extra = null)
    {
        if (extra != null) Console.WriteLine(extra);
        Console.WriteLine("""
            XtreemRgbToggle — TEAMGROUP T-Force XTREEM ARGB DDR4 RGB Control
            (PawnIO / AMD SMBus; no Armoury Crate, no OpenRGB GUI required)

            Usage (run as Administrator):
              XtreemRgbToggle.exe              RGB control GUI (double-click)
              XtreemRgbToggle.exe off          turn lights OFF
              XtreemRgbToggle.exe on           turn lights ON (T-Force Default 0x19)
              XtreemRgbToggle.exe toggle       one toggle then exit
              XtreemRgbToggle.exe tray         tray icon (LMB = toggle)
              XtreemRgbToggle.exe status       show saved state
              XtreemRgbToggle.exe detect       find modules only
              XtreemRgbToggle.exe debug        read back Direct/Mode registers
              XtreemRgbToggle.exe interactive  interactive console (Enter = toggle)
              XtreemRgbToggle.exe install-autostart    logon task: run "off" (highest)
              XtreemRgbToggle.exe uninstall-autostart  remove that logon task

            GUI features (verified only):
              - OFF / ON buttons (modes 0x00, 0x19 - verified on hardware)
              - Static color picker (uniform color - verified)
              - Autostart management (lights OFF at login)

            Before first run:
              1) Install PawnIO: https://pawnio.eu
              2) Keep PawnIOLib.dll and SmbusPIIX4.bin next to the exe
              3) Stop Armoury Crate / Aura Service (they hold SMBus)

            Note: Only verified features exposed. Color order R/B/G (verified from code).
            """);
        return extra == null ? 0 : 2;
    }

    private static int RunDetect()
    {
        var svc = new XtreemService();
        var devices = svc.DetectAndRemap(verbose: true);
        Console.WriteLine($"Modules found: {devices.Count}");
        foreach (var d in devices) Console.WriteLine("  " + d);
        return devices.Count > 0 ? 0 : 3;
    }

    private static int RunStatus()
    {
        var state = StateStore.Load();
        Console.WriteLine($"Saved state: {state}");
        Console.WriteLine(@"(file: %LocalAppData%\XtreemRgbToggle\state.txt)");
        Console.WriteLine($"Autostart task: {AutostartHelper.QueryInstallState()}");
        return 0;
    }

    private static int RunOnce(bool wantOff)
    {
        ResetLog();
        var svc = new XtreemService();
        var devices = svc.DetectAndRemap(verbose: true);
        if (devices.Count == 0)
        {
            Log("No XTREEM modules found. See README (PawnIO, admin, Aura off).");
            return 3;
        }

        if (wantOff)
        {
            Log("Turning lights OFF (mode 0x00 + Direct black)...");
            svc.ApplyOff(devices);
            StateStore.Save(LightState.Off);
            Log("OK: OFF");
        }
        else
        {
            Log("Turning lights ON (T-Force Default 0x19)...");
            svc.ApplyOn(devices);
            StateStore.Save(LightState.On);
            Log("OK: ON");
        }
        Log("Register read-back after command:");
        svc.DumpRegisters(devices);
        Log("Log file: " + LogPath);
        return 0;
    }

    private static int RunToggle()
    {
        var current = StateStore.Load();
        bool goOff = current != LightState.Off;
        Console.WriteLine($"Current state: {current} -> {(goOff ? "OFF" : "ON")}");
        return RunOnce(wantOff: goOff);
    }

    private static int RunInteractive()
    {
        Console.WriteLine("XtreemRgbToggle — interactive mode");
        Console.WriteLine("Enter = toggle, O = off, N = on, D = detect, G = debug, U = GUI, Q = quit");
        Console.WriteLine();

        RunDetect();
        Console.WriteLine($"State: {StateStore.Load()}");
        Console.WriteLine();

        while (true)
        {
            Console.Write("> ");
            string? line = Console.ReadLine();
            if (line is null) break;
            string key = line.Trim().ToLowerInvariant();
            if (key is "q" or "quit" or "exit") break;
            if (string.IsNullOrEmpty(key) || key is "t" or "toggle")
            {
                RunToggle();
                continue;
            }
            if (key is "o" or "off") { RunOnce(true); continue; }
            if (key is "n" or "on") { RunOnce(false); continue; }
            if (key is "d" or "detect") { RunDetect(); continue; }
            if (key is "g" or "debug" or "regs") { RunDebug(); continue; }
            if (key is "s" or "status") { RunStatus(); continue; }
            if (key is "u" or "ui" or "gui") { return RunGui(); }
            Console.WriteLine("Commands: Enter/t, o, n, d, g, s, u, q");
        }
        return 0;
    }

    private static int RunTray()
    {
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new TrayApplicationContext());
        return 0;
    }

    private static int RunGui()
    {
        using var timer = DiagnosticTimer.Start("RunGui (Application.Run)");
        DiagnosticTimer.Log("Enabling visual styles");
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        DiagnosticTimer.Log("Creating RgbControlForm");
        Application.Run(new RgbControlForm());
        return 0;
    }

    private static int RunDebug()
    {
        ResetLog();
        var svc = new XtreemService();
        var devices = svc.DetectAndRemap(verbose: true);
        if (devices.Count == 0)
        {
            Log("No XTREEM modules found.");
            return 3;
        }
        Log("Register read-back:");
        svc.DumpRegisters(devices);
        Log("Log file: " + LogPath);
        return 0;
    }

    private static int RunInstallAutostart()
    {
        AutostartHelper.Install();
        Console.WriteLine($"OK: installed scheduled task '{AutostartHelper.TaskName}'");
        Console.WriteLine($"  runs at logon: \"{AutostartHelper.ExePath}\" off");
        Console.WriteLine("  privileges: highest (required for PawnIO)");
        return 0;
    }

    private static int RunUninstallAutostart()
    {
        AutostartHelper.Uninstall();
        Console.WriteLine($"OK: removed scheduled task '{AutostartHelper.TaskName}' (if it existed)");
        return 0;
    }

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool SetDllDirectory(string lpPathName);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool AllocConsole();

    [DllImport("kernel32.dll")]
    private static extern IntPtr GetConsoleWindow();
}
