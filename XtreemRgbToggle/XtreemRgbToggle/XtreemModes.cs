namespace XtreemRgbToggle;

/// <summary>
/// XTREEM mode definitions from OpenRGB source
/// Source: https://gitlab.com/CalcProgrammer1/OpenRGB/-/raw/master/Controllers/TForceXtreemController/
/// - TForceXtreemController.h (mode enums)
/// - RGBController_TForceXtreem.cpp (mode metadata)
/// Date: Retrieved Nov 6, 2026
/// </summary>
public static class XtreemModes
{
    // Mode IDs from TForceXtreemController.h
    public const byte OFF = 0;
    public const byte STATIC = 1;
    public const byte BREATHING = 2;
    public const byte FLASHING = 3;
    public const byte SPECTRUM_CYCLE = 4;
    public const byte RAINBOW = 5;
    public const byte SPECTRUM_CYCLE_BREATHING = 6;
    public const byte CHASE_FADE = 7;
    public const byte SPECTRUM_CYCLE_CHASE_FADE = 8;
    public const byte CHASE = 9;
    public const byte SPECTRUM_CYCLE_CHASE = 10;
    public const byte SPECTRUM_CYCLE_WAVE = 11;
    public const byte CHASE_RAINBOW_PULSE = 12;
    public const byte RANDOM_FLICKER = 13;
    public const byte STACK = 14;
    public const byte PONG = 15;
    public const byte FILLUP = 16;
    public const byte NEON = 17;
    public const byte COLOR_WAVE = 18;
    public const byte COLOR_DOUBLE_WAVE = 19;
    public const byte MIXER = 20;
    public const byte SPECTRUM_CYCLE_2 = 21;
    public const byte FIRE_BREATHING = 22;
    public const byte SPECTRUM_CYCLE_3 = 23;
    public const byte SLITHER = 24;
    public const byte TFORCE_XTREEM_DEFAULT = 25;

    // Speed values (from TForceXtreemController.h)
    public const byte SPEED_SLOWEST = 0x04;
    public const byte SPEED_SLOW = 0x03;
    public const byte SPEED_NORMAL = 0x02;
    public const byte SPEED_FAST = 0x01;
    public const byte SPEED_FASTEST = 0x00;

    // Direction values
    public const byte DIRECTION_FORWARD = 0x00;
    public const byte DIRECTION_REVERSE = 0x01;

    public static readonly ModeInfo[] AllModes = new[]
    {
        new ModeInfo(OFF, "Off", false, false, false),
        new ModeInfo(STATIC, "Static", true, false, false),
        new ModeInfo(BREATHING, "Breathing", true, true, false),
        new ModeInfo(FLASHING, "Flashing", true, true, false),
        new ModeInfo(SPECTRUM_CYCLE, "Spectrum Cycle", false, true, false),
        new ModeInfo(RAINBOW, "Rainbow", false, true, true),
        new ModeInfo(SPECTRUM_CYCLE_BREATHING, "Spectrum Cycle Breathing", false, true, false),
        new ModeInfo(CHASE_FADE, "Chase Fade", true, true, true),
        new ModeInfo(SPECTRUM_CYCLE_CHASE_FADE, "Spectrum Cycle Chase Fade", false, true, true),
        new ModeInfo(CHASE, "Chase", true, true, true),
        new ModeInfo(SPECTRUM_CYCLE_CHASE, "Spectrum Cycle Chase", false, true, true),
        new ModeInfo(SPECTRUM_CYCLE_WAVE, "Spectrum Cycle Wave", false, true, false),
        new ModeInfo(CHASE_RAINBOW_PULSE, "Chase Rainbow Pulse", false, true, false),
        new ModeInfo(RANDOM_FLICKER, "Random Flicker", false, true, false),
        new ModeInfo(STACK, "Stack", false, true, true),
        new ModeInfo(PONG, "Pong", false, true, true),
        new ModeInfo(FILLUP, "Fill Up", false, true, true),
        new ModeInfo(NEON, "Neon Sign", false, true, false),
        new ModeInfo(COLOR_WAVE, "Wave", false, true, false),
        new ModeInfo(COLOR_DOUBLE_WAVE, "Double Wave", false, true, false),
        new ModeInfo(MIXER, "Mixer", false, true, false),
        new ModeInfo(SPECTRUM_CYCLE_2, "Spectrum Cycle 2", false, true, false),
        new ModeInfo(FIRE_BREATHING, "Fire Breathing", false, true, false),
        new ModeInfo(SPECTRUM_CYCLE_3, "Spectrum Cycle 3", false, true, false),
        new ModeInfo(SLITHER, "Slither", false, true, false),
        new ModeInfo(TFORCE_XTREEM_DEFAULT, "T-Force Xtreem Default", false, true, false),
    };
}

public record ModeInfo(byte Value, string Name, bool SupportsColor, bool SupportsSpeed, bool SupportsDirection);
