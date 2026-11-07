using Xunit;

namespace XtreemRgbToggle.Tests;

public class StateStoreTests
{
    private static string GetTestStatePath()
    {
        string tempDir = Path.Combine(Path.GetTempPath(), "XtreemRgbToggle.Tests", Guid.NewGuid().ToString());
        Directory.CreateDirectory(tempDir);
        return Path.Combine(tempDir, "state.txt");
    }

    [Fact]
    public void Save_And_Load_Off_State()
    {
        string testPath = GetTestStatePath();
        try
        {
            // Arrange & Act
            StateStore.Save(LightState.Off, testPath);
            var loaded = StateStore.Load(testPath);

            // Assert
            Assert.Equal(LightState.Off, loaded);
        }
        finally
        {
            if (File.Exists(testPath))
                File.Delete(testPath);
        }
    }

    [Fact]
    public void Save_And_Load_On_State()
    {
        string testPath = GetTestStatePath();
        try
        {
            // Arrange & Act
            StateStore.Save(LightState.On, testPath);
            var loaded = StateStore.Load(testPath);

            // Assert
            Assert.Equal(LightState.On, loaded);
        }
        finally
        {
            if (File.Exists(testPath))
                File.Delete(testPath);
        }
    }

    [Fact]
    public void Load_Nonexistent_File_Returns_Unknown()
    {
        // Arrange
        string nonexistentPath = Path.Combine(Path.GetTempPath(), $"nonexistent_{Guid.NewGuid()}.txt");

        // Act
        var state = StateStore.Load(nonexistentPath);

        // Assert
        Assert.Equal(LightState.Unknown, state);
    }

    [Fact]
    public void Load_Invalid_Content_Returns_Unknown()
    {
        string testPath = GetTestStatePath();
        try
        {
            // Arrange
            File.WriteAllText(testPath, "invalid content");

            // Act
            var state = StateStore.Load(testPath);

            // Assert
            Assert.Equal(LightState.Unknown, state);
        }
        finally
        {
            if (File.Exists(testPath))
                File.Delete(testPath);
        }
    }

    [Fact]
    public void Multiple_Saves_Overwrites_Previous()
    {
        string testPath = GetTestStatePath();
        try
        {
            // Arrange & Act
            StateStore.Save(LightState.Off, testPath);
            StateStore.Save(LightState.On, testPath);
            var loaded = StateStore.Load(testPath);

            // Assert
            Assert.Equal(LightState.On, loaded);
        }
        finally
        {
            if (File.Exists(testPath))
                File.Delete(testPath);
        }
    }
}
