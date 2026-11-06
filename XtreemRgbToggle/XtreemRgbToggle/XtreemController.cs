namespace XtreemRgbToggle;

/// <summary>
/// TEAMGROUP T-Force XTREEM ARGB DDR4 protocol.
/// VERIFIED: Mode 0x00 (Off), Mode 0x19 (T-Force Default), Direct colors
/// Color order: R/B/G (verified from original code, not RGB)
/// </summary>
internal static class XtreemRegs
{
    public const ushort Direct = 0xE020;
    public const ushort Mode = 0xE021;
    public const ushort Speed = 0xE022;
    public const ushort Direction = 0xE023;
    public const ushort Apply = 0xE02F;
    public const ushort SlotIndex = 0xE0F8;
    public const ushort I2cAddress = 0xE0F9;

    public const byte ApplyVal = 0x01;
    public const byte ModeOff = 0x00;
    public const byte ModeXtreemDefault = 0x19;

    public const byte BootAddress = 0x77;
}

internal sealed class XtreemDevice
{
    public byte Address7 { get; init; }
    public int BusPort { get; init; }

    public override string ToString() => $"XTREEM @ 0x{Address7:X2} (PIIX4 port {BusPort})";
}

internal sealed class XtreemService
{
    public static Action<string>? Trace { get; set; }

    private static void Say(string msg)
    {
        Console.WriteLine(msg);
        try { Trace?.Invoke(msg); } catch { }
    }

    private static readonly byte[] RemapAddresses =
    {
        0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x78,
        0x39, 0x3A, 0x3B, 0x3C, 0x3D
    };

    private readonly string _modulePath;

    public XtreemService(string? modulePath = null)
    {
        string baseDir = AppContext.BaseDirectory;
        _modulePath = modulePath ?? Path.Combine(baseDir, "SmbusPIIX4.bin");
    }

    public List<XtreemDevice> DetectAndRemap(bool verbose = true)
    {
        var found = new List<XtreemDevice>();

        foreach (int port in new[] { 0, 1 })
        {
            if (verbose) Console.WriteLine($"Scanning AMD SMBus port {port}...");

            AmdSmBus? bus = null;
            try
            {
                bus = AmdSmBus.Open(_modulePath, port);
            }
            catch (Exception ex)
            {
                if (verbose) Console.WriteLine($"  port {port}: failed to open ({ex.Message})");
                continue;
            }

            using (bus)
            {
                RemapModules(bus, verbose);
                foreach (byte addr in EnumerateCandidateAddresses())
                {
                    if (TestForXtreem(bus, addr, verbose))
                    {
                        var dev = new XtreemDevice { Address7 = addr, BusPort = port };
                        found.Add(dev);
                        if (verbose) Console.WriteLine($"  found {dev}");
                    }
                }
            }
        }

        return found
            .GroupBy(d => d.Address7)
            .Select(g => g.First())
            .ToList();
    }

    private static IEnumerable<byte> EnumerateCandidateAddresses()
    {
        yield return XtreemRegs.BootAddress;
        foreach (byte a in RemapAddresses) yield return a;
        foreach (byte a in new byte[] { 0x4F, 0x66, 0x67 }) yield return a;
    }

    private static void RemapModules(AmdSmBus bus, bool verbose)
    {
        if (!DevicePresent(bus, XtreemRegs.BootAddress))
        {
            if (verbose) Console.WriteLine("  nothing on 0x77 — skip remap");
            return;
        }

        if (!TestForXtreem(bus, XtreemRegs.BootAddress, verbose: false) && verbose)
            Console.WriteLine("  0x77 responds, XTREEM signature incomplete — still trying slot remap");

        int addrIdx = 0;
        for (int slot = 0; slot <= 8; slot++)
        {
            if (!DevicePresent(bus, XtreemRegs.BootAddress))
                break;

            byte? free = null;
            while (addrIdx < RemapAddresses.Length)
            {
                byte candidate = RemapAddresses[addrIdx++];
                if (!DevicePresent(bus, candidate))
                {
                    free = candidate;
                    break;
                }
            }

            if (free is null)
            {
                if (verbose) Console.WriteLine("  no free remap addresses left");
                break;
            }

            if (verbose) Console.WriteLine($"  trying remap slot {slot} -> 0x{free.Value:X2}");
            RegisterWrite(bus, XtreemRegs.BootAddress, XtreemRegs.SlotIndex, (byte)slot);
            RegisterWrite(bus, XtreemRegs.BootAddress, XtreemRegs.I2cAddress, (byte)(free.Value << 1));
            Thread.Sleep(10);

            if (TestForXtreem(bus, free.Value, verbose: false))
            {
                if (verbose) Console.WriteLine($"  OK: module now at 0x{free.Value:X2}");
            }
            else
            {
                addrIdx--;
                if (verbose) Console.WriteLine($"  slot {slot} empty, address 0x{free.Value:X2} still free");
            }
        }
    }

    private static bool DevicePresent(AmdSmBus bus, byte addr)
    {
        if (bus.Probe(addr)) return true;
        try
        {
            bus.ReadByteData(addr, 0x00);
            return true;
        }
        catch
        {
            return false;
        }
    }

    public static bool TestForXtreem(AmdSmBus bus, byte addr, bool verbose)
    {
        if (!DevicePresent(bus, addr))
            return false;

        try
        {
            for (int i = 0x90; i <= 0xA0; i++)
            {
                byte expected = (byte)(i - 0x80);
                byte got = bus.ReadByteData(addr, (byte)i);
                if (got != expected)
                {
                    if (verbose)
                        Console.WriteLine($"  0x{addr:X2}: reg 0x{i:X2} expected 0x{expected:X2}, got 0x{got:X2} — not XTREEM");
                    return false;
                }
            }
            return true;
        }
        catch (Exception ex)
        {
            if (verbose) Console.WriteLine($"  0x{addr:X2}: signature read error ({ex.Message})");
            return false;
        }
    }

    /// <summary>Off: mode Off + Direct black (Direct=1). VERIFIED working.</summary>
    public void ApplyOff(IReadOnlyList<XtreemDevice> devices, bool verbose = true)
    {
        ApplyEffect(devices, XtreemModes.OFF, XtreemModes.SPEED_NORMAL, XtreemModes.DIRECTION_FORWARD, verbose);
        SetDirectBlack(devices, verbose);
    }

    /// <summary>On: T-Force Default (0x19) + Direct=0. Legacy shortcut.</summary>
    public void ApplyOn(IReadOnlyList<XtreemDevice> devices, bool verbose = true)
    {
        ApplyEffect(devices, XtreemModes.TFORCE_XTREEM_DEFAULT, XtreemModes.SPEED_NORMAL, XtreemModes.DIRECTION_FORWARD, verbose);
    }

    /// <summary>Apply effect mode with speed and direction. Source-confirmed from OpenRGB.</summary>
    public void ApplyEffect(IReadOnlyList<XtreemDevice> devices, byte mode, byte speed, byte direction, bool verbose = true)
    {
        if (devices.Count == 0)
            throw new InvalidOperationException("No XTREEM modules detected.");

        foreach (var group in devices.GroupBy(d => d.BusPort))
        {
            using var bus = AmdSmBus.Open(_modulePath, group.Key);
            foreach (var dev in group)
            {
                if (verbose) Say($"  {dev}: mode=0x{mode:X2} speed=0x{speed:X2} dir=0x{direction:X2}");
                RegisterWrite(bus, dev.Address7, XtreemRegs.Mode, mode);
                RegisterWrite(bus, dev.Address7, XtreemRegs.Speed, speed);
                RegisterWrite(bus, dev.Address7, XtreemRegs.Direction, direction);
                RegisterWrite(bus, dev.Address7, XtreemRegs.Apply, XtreemRegs.ApplyVal);
                Thread.Sleep(20);
            }
        }
        
        SetDirectFlag(devices, direct: 0, verbose);
    }

    private void SetDirectFlag(IReadOnlyList<XtreemDevice> devices, byte direct, bool verbose)
    {
        if (devices.Count == 0)
            throw new InvalidOperationException("No XTREEM modules detected.");

        foreach (var group in devices.GroupBy(d => d.BusPort))
        {
            using var bus = AmdSmBus.Open(_modulePath, group.Key);
            foreach (var dev in group)
            {
                if (verbose) Say($"  {dev}: Direct=0x{direct:X2}");
                RegisterWrite(bus, dev.Address7, XtreemRegs.Direct, direct);
                RegisterWrite(bus, dev.Address7, XtreemRegs.Apply, XtreemRegs.ApplyVal);
                Thread.Sleep(20);
            }
        }
    }

    /// <summary>Direct mode + all LEDs black. Color order: R/B/G (verified).</summary>
    private void SetDirectBlack(IReadOnlyList<XtreemDevice> devices, bool verbose)
    {
        if (devices.Count == 0)
            throw new InvalidOperationException("No XTREEM modules detected.");

        foreach (var group in devices.GroupBy(d => d.BusPort))
        {
            using var bus = AmdSmBus.Open(_modulePath, group.Key);
            foreach (var dev in group)
            {
                if (verbose) Say($"  {dev}: Direct black");
                for (int i = 0; i < 15; i++)
                {
                    ushort reg = (ushort)(0xE100 + i * 3);
                    RegisterWrite(bus, dev.Address7, reg, 0x00);           // R
                    RegisterWrite(bus, dev.Address7, (ushort)(reg + 1), 0x00); // B
                    RegisterWrite(bus, dev.Address7, (ushort)(reg + 2), 0x00); // G
                }
                RegisterWrite(bus, dev.Address7, XtreemRegs.Direct, 0x01);
                RegisterWrite(bus, dev.Address7, XtreemRegs.Apply, XtreemRegs.ApplyVal);
                Thread.Sleep(20);
            }
        }
    }

    /// <summary>
    /// Apply static color to all 15 LEDs. Color order: R/B/G (verified from original code).
    /// VERIFIED: Uniform color works.
    /// </summary>
    public void ApplyStaticColor(IReadOnlyList<XtreemDevice> devices, byte r, byte g, byte b, bool verbose = true)
    {
        if (devices.Count == 0)
            throw new InvalidOperationException("No XTREEM modules detected.");

        foreach (var group in devices.GroupBy(d => d.BusPort))
        {
            using var bus = AmdSmBus.Open(_modulePath, group.Key);
            foreach (var dev in group)
            {
                if (verbose) Say($"  {dev}: Direct RGB({r},{g},{b}) × 15 LEDs");

                for (int i = 0; i < 15; i++)
                {
                    ushort reg = (ushort)(0xE100 + i * 3);
                    RegisterWrite(bus, dev.Address7, reg, r);           // R
                    RegisterWrite(bus, dev.Address7, (ushort)(reg + 1), b); // B (not G!)
                    RegisterWrite(bus, dev.Address7, (ushort)(reg + 2), g); // G (not B!)
                }

                RegisterWrite(bus, dev.Address7, XtreemRegs.Direct, 0x01);
                RegisterWrite(bus, dev.Address7, XtreemRegs.Apply, XtreemRegs.ApplyVal);
                Thread.Sleep(20);
            }
        }
    }

    public void DumpRegisters(IReadOnlyList<XtreemDevice> devices, bool verbose = true)
    {
        if (devices.Count == 0)
            throw new InvalidOperationException("No XTREEM modules detected.");

        foreach (var group in devices.GroupBy(d => d.BusPort))
        {
            using var bus = AmdSmBus.Open(_modulePath, group.Key);
            foreach (var dev in group)
            {
                byte direct = RegisterRead(bus, dev.Address7, XtreemRegs.Direct);
                byte mode = RegisterRead(bus, dev.Address7, XtreemRegs.Mode);
                byte speed = RegisterRead(bus, dev.Address7, XtreemRegs.Speed);
                byte direction = RegisterRead(bus, dev.Address7, XtreemRegs.Direction);
                Say($"  {dev}: Direct(0xE020)=0x{direct:X2} Mode(0xE021)=0x{mode:X2} Speed=0x{speed:X2} Dir=0x{direction:X2}");
            }
        }
    }

    public static void RegisterWrite(AmdSmBus bus, byte addr7, ushort reg, byte value)
    {
        ushort swapped = (ushort)(((reg << 8) & 0xFF00) | ((reg >> 8) & 0x00FF));
        bus.WriteWordData(addr7, 0x00, swapped);
        bus.WriteByteData(addr7, 0x01, value);
    }

    public static byte RegisterRead(AmdSmBus bus, byte addr7, ushort reg)
    {
        ushort swapped = (ushort)(((reg << 8) & 0xFF00) | ((reg >> 8) & 0x00FF));
        bus.WriteWordData(addr7, 0x00, swapped);
        return bus.ReadByteData(addr7, 0x81);
    }
}
