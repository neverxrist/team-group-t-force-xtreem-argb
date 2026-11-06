namespace XtreemRgbToggle;

internal enum LightState
{
    Unknown = 0,
    Off = 1,
    On = 2
}

internal static class StateStore
{
    private static string StatePath =>
        Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "XtreemRgbToggle",
            "state.txt");

    public static LightState Load()
    {
        try
        {
            string path = StatePath;
            if (!File.Exists(path)) return LightState.Unknown;
            string text = File.ReadAllText(path).Trim();
            return text.ToLowerInvariant() switch
            {
                "off" => LightState.Off,
                "on" => LightState.On,
                _ => LightState.Unknown
            };
        }
        catch
        {
            return LightState.Unknown;
        }
    }

    public static void Save(LightState state)
    {
        string path = StatePath;
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllText(path, state == LightState.Off ? "off" : "on");
    }
}
