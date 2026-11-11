using Xunit;

namespace XtreemRgbToggle.Tests;

public class AutostartStatusLogicTests
{
    [Fact]
    public void Combine_PresentCurrent_IsInstalled()
    {
        Assert.Equal(
            AutostartInstallState.Installed,
            AutostartStatusLogic.Combine(TaskQueryResult.Present, TaskQueryResult.Absent));
    }

    [Fact]
    public void Combine_PresentLegacy_IsInstalled()
    {
        Assert.Equal(
            AutostartInstallState.Installed,
            AutostartStatusLogic.Combine(TaskQueryResult.Absent, TaskQueryResult.Present));
    }

    [Fact]
    public void Combine_BothAbsent_IsNotInstalled()
    {
        Assert.Equal(
            AutostartInstallState.NotInstalled,
            AutostartStatusLogic.Combine(TaskQueryResult.Absent, TaskQueryResult.Absent));
    }

    [Fact]
    public void Combine_TimeoutOnCurrent_IsUnknown_NotAbsent()
    {
        Assert.Equal(
            AutostartInstallState.Unknown,
            AutostartStatusLogic.Combine(TaskQueryResult.Unknown, TaskQueryResult.Absent));
    }

    [Fact]
    public void Combine_TimeoutOnLegacy_IsUnknown_NotAbsent()
    {
        Assert.Equal(
            AutostartInstallState.Unknown,
            AutostartStatusLogic.Combine(TaskQueryResult.Absent, TaskQueryResult.Unknown));
    }

    [Fact]
    public void Combine_BothUnknown_IsUnknown()
    {
        Assert.Equal(
            AutostartInstallState.Unknown,
            AutostartStatusLogic.Combine(TaskQueryResult.Unknown, TaskQueryResult.Unknown));
    }

    [Fact]
    public void Combine_PresentWinsOverUnknown()
    {
        Assert.Equal(
            AutostartInstallState.Installed,
            AutostartStatusLogic.Combine(TaskQueryResult.Present, TaskQueryResult.Unknown));
    }

    [Fact]
    public void CanCache_OnlyConfirmedStates()
    {
        Assert.True(AutostartStatusLogic.CanCache(AutostartInstallState.Installed));
        Assert.True(AutostartStatusLogic.CanCache(AutostartInstallState.NotInstalled));
        Assert.False(AutostartStatusLogic.CanCache(AutostartInstallState.Unknown));
    }
}
