namespace XtreemRgbToggle;

/// <summary>
/// AMD PIIX4 SMBus access through PawnIO module SmbusPIIX4.bin
/// (same path OpenRGB uses on Ryzen boards).
/// </summary>
internal sealed class AmdSmBus : IDisposable
{
    private readonly IntPtr _handle;
    private readonly IntPtr _mutex;
    private bool _disposed;

    private AmdSmBus(IntPtr handle, IntPtr mutex, int port)
    {
        _handle = handle;
        _mutex = mutex;
        Port = port;
    }

    public int Port { get; }

    public static AmdSmBus Open(string modulePath, int port)
    {
        if (!File.Exists(modulePath))
            throw new FileNotFoundException(
                $"PawnIO module not found: {modulePath}. Run scripts\\fetch-deps.ps1 or copy SmbusPIIX4.bin next to the exe.",
                modulePath);

        int verHr = PawnIONative.pawnio_version(out uint version);
        if (!PawnIONative.Succeeded(verHr))
            throw new InvalidOperationException(
                "PawnIO is not installed or the driver is not loaded. Install from https://pawnio.eu and run as Administrator. " +
                $"HRESULT={PawnIONative.FormatHResult(verHr)}");

        Console.WriteLine($"PawnIO version: {(version >> 16) & 0xFF}.{(version >> 8) & 0xFF}.{version & 0xFF}");

        byte[] blob = File.ReadAllBytes(modulePath);
        int openHr = PawnIONative.pawnio_open(out IntPtr handle);
        if (!PawnIONative.Succeeded(openHr))
        {
            string hint = unchecked((uint)openHr) == 0x80070005
                ? " (Administrator rights required)"
                : "";
            throw new InvalidOperationException(
                $"pawnio_open failed{hint}: {PawnIONative.FormatHResult(openHr)}");
        }

        int loadHr = PawnIONative.pawnio_load(handle, blob, (nuint)blob.Length);
        if (!PawnIONative.Succeeded(loadHr))
        {
            PawnIONative.pawnio_close(handle);
            throw new InvalidOperationException(
                $"pawnio_load(SmbusPIIX4.bin) failed: {PawnIONative.FormatHResult(loadHr)}. " +
                "On a non-AMD board the PIIX4 module may be unsupported.");
        }

        IntPtr mutex = PawnIONative.CreateMutexA(IntPtr.Zero, false, PawnIONative.SmbusMutexName);
        if (mutex == IntPtr.Zero)
        {
            PawnIONative.pawnio_close(handle);
            throw new InvalidOperationException("Failed to create SMBus mutex Access_SMBUS.HTP.Method");
        }

        var bus = new AmdSmBus(handle, mutex, port);
        try
        {
            bus.SelectPort(port);
            bus.SetSleepMode(2);
        }
        catch
        {
            bus.Dispose();
            throw;
        }

        return bus;
    }

    public void SelectPort(int port)
    {
        WithMutex(() =>
        {
            ulong[] input = { (ulong)port };
            ulong[] output = new ulong[1];
            int hr = PawnIONative.pawnio_execute(
                _handle, "ioctl_piix4_port_sel",
                input, 1, output, 1, out _);
            if (!PawnIONative.Succeeded(hr))
                throw new InvalidOperationException(
                    $"ioctl_piix4_port_sel({port}) failed: {PawnIONative.FormatHResult(hr)}");
        });
    }

    public void SetSleepMode(int mode)
    {
        ulong[] input = { (ulong)mode };
        int hr = PawnIONative.pawnio_execute(
            _handle, "ioctl_set_sleep_mode",
            input, 1, null, 0, out _);
        if (!PawnIONative.Succeeded(hr))
            Console.WriteLine($"Warning: ioctl_set_sleep_mode -> {PawnIONative.FormatHResult(hr)}");
    }

    public bool Probe(byte addr7)
    {
        try
        {
            Xfer(addr7, PawnIONative.I2C_SMBUS_WRITE, 0, PawnIONative.I2C_SMBUS_QUICK, 0);
            return true;
        }
        catch
        {
            return false;
        }
    }

    public byte ReadByteData(byte addr7, byte command)
    {
        ulong data = Xfer(addr7, PawnIONative.I2C_SMBUS_READ, command, PawnIONative.I2C_SMBUS_BYTE_DATA, 0);
        return (byte)(data & 0xFF);
    }

    public void WriteByteData(byte addr7, byte command, byte value)
    {
        Xfer(addr7, PawnIONative.I2C_SMBUS_WRITE, command, PawnIONative.I2C_SMBUS_BYTE_DATA, value);
    }

    public void WriteWordData(byte addr7, byte command, ushort value)
    {
        Xfer(addr7, PawnIONative.I2C_SMBUS_WRITE, command, PawnIONative.I2C_SMBUS_WORD_DATA, value);
    }

    private ulong Xfer(byte addr7, int readWrite, byte command, int protocol, ulong dataIn)
    {
        return WithMutex(() =>
        {
            ulong[] input = new ulong[9];
            input[0] = addr7;
            input[1] = (ulong)readWrite;
            input[2] = command;
            input[3] = (ulong)protocol;
            input[4] = dataIn;

            ulong[] output = new ulong[5];
            int hr = PawnIONative.pawnio_execute(
                _handle, "ioctl_smbus_xfer",
                input, 9, output, 5, out _);

            if (!PawnIONative.Succeeded(hr))
                throw new IOException(
                    $"SMBus xfer addr=0x{addr7:X2} cmd=0x{command:X2} proto={protocol} failed: {PawnIONative.FormatHResult(hr)}");

            return output[0];
        });
    }

    private void WithMutex(Action action)
    {
        WithMutex(() =>
        {
            action();
            return 0UL;
        });
    }

    private T WithMutex<T>(Func<T> func)
    {
        uint wait = PawnIONative.WaitForSingleObject(_mutex, PawnIONative.INFINITE);
        if (wait != PawnIONative.WAIT_OBJECT_0 && wait != PawnIONative.WAIT_ABANDONED)
            throw new InvalidOperationException($"WaitForSingleObject(SMBus mutex) = {wait}");

        try
        {
            return func();
        }
        finally
        {
            PawnIONative.ReleaseMutex(_mutex);
        }
    }

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        if (_mutex != IntPtr.Zero) PawnIONative.CloseHandle(_mutex);
        if (_handle != IntPtr.Zero) PawnIONative.pawnio_close(_handle);
    }
}
