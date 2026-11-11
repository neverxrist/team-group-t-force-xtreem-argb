using Xunit;

namespace XtreemRgbToggle.Tests;

public class DiagnosticTimerTests
{
    [Fact]
    public void Disabled_By_Default()
    {
        // Diagnostic should be disabled by default (no logs unless enabled)
        Assert.False(DiagnosticTimer.Enabled);
    }

    [Fact]
    public void Can_Enable_And_Disable()
    {
        try
        {
            DiagnosticTimer.Enabled = true;
            Assert.True(DiagnosticTimer.Enabled);

            DiagnosticTimer.Enabled = false;
            Assert.False(DiagnosticTimer.Enabled);
        }
        finally
        {
            DiagnosticTimer.Enabled = false;
        }
    }

    [Fact]
    public void Start_Returns_Disposable()
    {
        using var timer = DiagnosticTimer.Start("test");
        Assert.NotNull(timer);
    }

    [Fact]
    public void Dispose_Does_Not_Throw()
    {
        var timer = DiagnosticTimer.Start("test");
        var exception = Record.Exception(() => timer.Dispose());
        Assert.Null(exception);
    }

    [Fact]
    public void Log_Does_Not_Throw_When_Disabled()
    {
        DiagnosticTimer.Enabled = false;
        var exception = Record.Exception(() => DiagnosticTimer.Log("test message"));
        Assert.Null(exception);
    }

    [Fact]
    public void Multiple_Operations_Dont_Interfere()
    {
        try
        {
            DiagnosticTimer.Enabled = true;

            using (var timer1 = DiagnosticTimer.Start("operation1"))
            {
                using (var timer2 = DiagnosticTimer.Start("operation2"))
                {
                    DiagnosticTimer.Log("nested operation");
                }
            }

            // Should complete without errors
            Assert.True(true);
        }
        finally
        {
            DiagnosticTimer.Enabled = false;
        }
    }
}
