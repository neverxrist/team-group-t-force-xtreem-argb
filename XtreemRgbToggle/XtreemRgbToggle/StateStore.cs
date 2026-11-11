namespace XtreemRgbToggle;

public enum LightState
{
    Unknown = 0,
    Off = 1,
    On = 2
}

public static class StateStore
{
    private static string DefaultStatePath =>
        Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "XtreemRgbToggle",
            "state.txt");

    public static LightState Load(string? path = null)
    {
        try
        {
            path ??= DefaultStatePath;
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

    public static void Save(LightState state, string? path = null)
    {
        path ??= DefaultStatePath;
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllText(path, state == LightState.Off ? "off" : "on");
    }
}
