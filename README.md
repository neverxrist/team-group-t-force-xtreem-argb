# XTREEM RGB Control

Lightweight Windows utility for controlling TEAMGROUP T-Force XTREEM ARGB DDR4 RAM RGB lighting on AMD Ryzen platforms.

## Features

- **26 Effect Modes** - All modes from OpenRGB source (Static, Rainbow, Breathing, Chase, etc.)
- **Live Color Preview** - Real-time hardware updates while dragging sliders
- **Per-Module Control** - Control each RAM stick individually or all at once
- **System Tray** - Quick toggle from taskbar
- **Autostart** - Optional OFF at login (scheduled task `XtreemRgbToggle-Off`)
- **CLI Support** - Command-line for scripts/automation
- **WinForms .NET 8** - No Electron/Chromium

## Requirements

- **Hardware**: TEAMGROUP T-Force XTREEM ARGB DDR4 on AMD Ryzen platform
- **OS**: Windows 10/11 x64
- **Privileges**: Administrator (SMBus hardware access required)
- **PawnIO kernel driver**: must already be installed from [pawnio.eu](https://pawnio.eu). Bundled `PawnIOLib.dll` and `SmbusPIIX4.bin` are user-mode pieces only; they do **not** install the kernel driver.
- **.NET 8 Desktop Runtime**: required only for the smaller framework-dependent ZIP. The self-contained ZIP bundles the runtime.

## Installation

### Portable self-contained (recommended)

No separate .NET install. Extract the self-contained ZIP and run `XtreemRgbToggle.exe` as administrator. Size is tens of MB because the .NET 8 Windows Desktop runtime is included as a folder (not a single-file bundle: single-file extracts on startup; trimming/AOT was not used because WinForms is a poor fit).

You still need the PawnIO **driver** installed on the machine.

### Portable framework-dependent (optional)

Smaller ZIP. Requires [.NET 8 Desktop Runtime x64](https://dotnet.microsoft.com/download/dotnet/8.0) plus the PawnIO driver.

### Installer

The Inno Setup script is not a finished/safe installer (dependency download verification incomplete). Do not treat Setup.exe as a published product.

## Usage

### GUI (Default)

Double-click `XtreemRgbToggle.exe` or run without arguments.

**Quick Controls:**
- **OFF** - Turn off all LEDs
- **ON** - T-Force Default effect (factory animation)

**Effects:**
- Select from 26 modes (source-confirmed from OpenRGB)
- Adjust speed (Fastest → Slowest)
- Reverse direction (where supported)

**Static Color:**
- Pick Color button for full color picker
- R/G/B sliders for precise control
- Live preview checkbox for real-time hardware updates

**Module Selection:**
- All modules (both sticks)
- Individual modules (0x39, 0x3A)

### CLI

```batch
XtreemRgbToggle.exe off          # Turn off
XtreemRgbToggle.exe on           # Turn on (T-Force Default)
XtreemRgbToggle.exe toggle       # Toggle on/off
XtreemRgbToggle.exe tray         # System tray icon
XtreemRgbToggle.exe status       # Show current state
XtreemRgbToggle.exe detect       # Detect modules
```

### Autostart (Lights OFF at Login)

1. Open GUI
2. Click **Install Autostart**
3. Task runs at login with highest privileges, zero delay
4. Elevated tasks do not appear in Task Manager Startup tab (Windows security design)

## Building from Source

```batch
dotnet restore XtreemRgbToggle.sln
dotnet test XtreemRgbToggle\XtreemRgbToggle.Tests\XtreemRgbToggle.Tests.csproj -c Release
dotnet publish XtreemRgbToggle\XtreemRgbToggle\XtreemRgbToggle.csproj -c Release -r win-x64 --self-contained true -p:PublishSingleFile=false -p:PublishTrimmed=false
```

## Supported Hardware

### Verified
- TEAMGROUP T-Force XTREEM ARGB DDR4
- AMD Ryzen 5 5600X (PIIX4 SMBus)
- Module addresses: 0x39, 0x3A on Port 0

### Unverified
- Intel platforms (different SMBus controller)
- Other DDR4 RGB modules
- DDR5 modules
- Laptop platforms

## Protocol & Modes

Effect modes researched from [OpenRGB](https://gitlab.com/CalcProgrammer1/OpenRGB/-/tree/master/Controllers/TForceXtreemController):

- **Source-confirmed**: All 26 mode IDs (0x00-0x19) in `XtreemRgbToggle/XtreemModes.cs`
- **Hardware-verified**: Basic OFF/ON register writes
- **Hardware-unverified**: Visual appearance of effects

## Limitations

- **SMBus access**: Requires administrator privileges
- **AMD only**: Uses AMD SMBus/PIIX4 (Intel platforms have different controllers)
- **Single module type**: Protocol specific to T-Force XTREEM ARGB DDR4
- Hardware settling delays in the protocol path are kept; they were not retuned without hardware evidence

## Troubleshooting

**No modules detected:**
- Run as Administrator
- Confirm the PawnIO kernel driver is installed (`https://pawnio.eu`), not only the DLL/bin next to the exe
- Stop ASUS Aura/Armoury Crate (conflicts with direct SMBus access)
- Verify `PawnIOLib.dll` and `SmbusPIIX4.bin` are in the same folder as the exe

**Slow startup / UI:**
Opt-in diagnostic log (operation names and milliseconds only, no secrets):

```cmd
set XTREEM_DIAG=1
XtreemRgbToggle.exe
```

Log file: `%LocalAppData%\XtreemRgbToggle\diagnostic.log`

This environment did not run a Windows GUI, so there are no measured first-paint times here. Use the log on the target PC.

**Autostart doesn't work:**
- Open Task Scheduler → Find `XtreemRgbToggle-Off`
- Verify: Enabled, "Run with highest privileges" checked
- Trigger: "At log on" with PT0S (zero) delay

## License

MIT License - See [LICENSE](LICENSE)

**Third-party components:**
- OpenRGB protocol research: GPL-2.0-or-later ([OpenRGB Project](https://gitlab.com/CalcProgrammer1/OpenRGB))
- PawnIO user-mode binaries: See [native/COPYING.PawnIO.Modules](native/COPYING.PawnIO.Modules)
- Bundled .NET 8 runtime (self-contained ZIP): Microsoft software license

## Credits

- Protocol research based on [OpenRGB T-Force Xtreem controller](https://gitlab.com/CalcProgrammer1/OpenRGB/-/tree/master/Controllers/TForceXtreemController) by Milan Cermak (krysmanta)
- PawnIO SMBus access (kernel driver from vendor; user-mode binaries redistributed)

## Contributing

Issues and pull requests welcome. Please test on actual hardware before submitting.

## Disclaimer

**USE AT YOUR OWN RISK.** Direct SMBus access can potentially cause hardware issues if misused. This tool writes to documented RAM RGB controller registers only. Always run reputable RGB control software to verify your hardware works before using alternative tools.
