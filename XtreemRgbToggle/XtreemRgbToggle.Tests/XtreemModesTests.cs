using Xunit;

namespace XtreemRgbToggle.Tests;

public class XtreemModesTests
{
    [Fact]
    public void AllModes_Has_26_Entries()
    {
        // From OpenRGB source: modes 0x00 through 0x19 = 26 modes
        Assert.Equal(26, XtreemModes.AllModes.Length);
    }

    [Fact]
    public void OFF_Mode_Is_0x00()
    {
        Assert.Equal(0x00, XtreemModes.OFF);
    }

    [Fact]
    public void TFORCE_XTREEM_DEFAULT_Is_0x19()
    {
        Assert.Equal(0x19, XtreemModes.TFORCE_XTREEM_DEFAULT);
    }

    [Fact]
    public void Mode_Values_Are_Sequential()
    {
        for (int i = 0; i < XtreemModes.AllModes.Length; i++)
        {
            Assert.Equal((byte)i, XtreemModes.AllModes[i].Value);
        }
    }

    [Fact]
    public void All_Modes_Have_Names()
    {
        foreach (var mode in XtreemModes.AllModes)
        {
            Assert.False(string.IsNullOrWhiteSpace(mode.Name), 
                $"Mode 0x{mode.Value:X2} has no name");
        }
    }

    [Fact]
    public void Speed_Constants_Are_Valid_Range()
    {
        // Speed should be 0-4
        Assert.InRange(XtreemModes.SPEED_FASTEST, (byte)0, (byte)4);
        Assert.InRange(XtreemModes.SPEED_FAST, (byte)0, (byte)4);
        Assert.InRange(XtreemModes.SPEED_NORMAL, (byte)0, (byte)4);
        Assert.InRange(XtreemModes.SPEED_SLOW, (byte)0, (byte)4);
        Assert.InRange(XtreemModes.SPEED_SLOWEST, (byte)0, (byte)4);
    }

    [Fact]
    public void Direction_Constants_Are_0_Or_1()
    {
        Assert.Equal((byte)0, XtreemModes.DIRECTION_FORWARD);
        Assert.Equal((byte)1, XtreemModes.DIRECTION_REVERSE);
    }
}
