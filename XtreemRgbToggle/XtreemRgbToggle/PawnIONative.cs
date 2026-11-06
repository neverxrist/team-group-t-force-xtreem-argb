using System.Runtime.InteropServices;

namespace XtreemRgbToggle;

internal static class PawnIONative
{
    public const string SmbusMutexName = "Access_SMBUS.HTP.Method";

    public const int I2C_SMBUS_READ = 1;
    public const int I2C_SMBUS_WRITE = 0;
    public const int I2C_SMBUS_QUICK = 0;
    public const int I2C_SMBUS_BYTE = 1;
    public const int I2C_SMBUS_BYTE_DATA = 2;
    public const int I2C_SMBUS_WORD_DATA = 3;

    public const uint WAIT_OBJECT_0 = 0;
    public const uint WAIT_ABANDONED = 0x00000080;
    public const uint INFINITE = 0xFFFFFFFF;

    [DllImport("PawnIOLib.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern int pawnio_version(out uint version);

    [DllImport("PawnIOLib.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern int pawnio_open(out IntPtr handle);

    [DllImport("PawnIOLib.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern int pawnio_close(IntPtr handle);

    [DllImport("PawnIOLib.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern int pawnio_load(IntPtr handle, byte[] blob, nuint size);

    [DllImport("PawnIOLib.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern int pawnio_execute(
        IntPtr handle,
        [MarshalAs(UnmanagedType.LPStr)] string function,
        ulong[]? input,
        nuint inputCount,
        ulong[]? output,
        nuint outputCount,
        out nuint outputReturned);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr CreateMutexA(IntPtr lpMutexAttributes, bool bInitialOwner,
        [MarshalAs(UnmanagedType.LPStr)] string lpName);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern uint WaitForSingleObject(IntPtr hHandle, uint dwMilliseconds);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool ReleaseMutex(IntPtr hMutex);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool CloseHandle(IntPtr hObject);

    public static bool Succeeded(int hr) => hr >= 0;

    public static string FormatHResult(int hr)
    {
        uint u = unchecked((uint)hr);
        return $"0x{u:X8}";
    }
}
