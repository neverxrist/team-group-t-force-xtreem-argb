# XTREEM RGB Control

Lightweight Windows utility for controlling TEAMGROUP T-Force XTREEM ARGB DDR4 RAM RGB lighting on AMD Ryzen platforms.

## Features

- **26 Effect Modes** - All modes from OpenRGB source (Static, Rainbow, Breathing, Chase, etc.)
- **Live Color Preview** - Real-time hardware updates while dragging sliders
- **Per-Module Control** - Control each RAM stick individually or all at once
- **System Tray** - Quick toggle from taskbar
- **Autostart** - Optional OFF at login (scheduled task)
- **CLI Support** - Command-line for scripts/automation
- **No Bloat** - Native .NET 8, no Electron/Chromium, <1 MB

## Requirements

- **Hardware**: TEAMGROUP T-Force XTREEM ARGB DDR4 on AMD Ryzen platform
- **OS**: Windows 10/11 x64
- **Runtime**: [.NET 8.0 Desktop Runtime x64](https://dotnet.microsoft.com/download/dotnet/8.0)
- **Privileges**: Administrator (SMBus hardware access required)
- **Driver**: PawnIO (included)

## Installation

### Portable (Recommended)

1. Download latest release ZIP
2. Extract to any folder
3. Right-click `XtreemRgbToggle.exe` → **Run as administrator**

### Installer (Optional)

1. Compile installer on Windows:
   ```batch
   "C:\Program Files\Inno Setup 7\ISCC.exe" installer\XtreemRgbToggle-Setup.iss
   ```
2. Run `XtreemRgbToggle-Setup-x.x.x.exe`
3. Installer will download .NET runtime if missing

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
3. Task runs at login with highest priority, zero delay
4. **Note**: Elevated tasks don't appear in Task Manager Startup tab (Windows security design)

## Building from Source

```batch
# Prerequisites
- .NET 8.0 SDK
- Visual Studio 2022 or dotnet CLI

# Build
dotnet restore
dotnet publish -c Release -r win-x64 --self-contained false

# Output: bin\Release\net8.0-windows\win-x64\publish\
```

## Supported Hardware

### ✅ Verified
- TEAMGROUP T-Force XTREEM ARGB DDR4
- AMD Ryzen 5 5600X (PIIX4 SMBus)
- Module addresses: 0x39, 0x3A on Port 0

### ❌ Unverified
- Intel platforms (different SMBus controller)
- Other DDR4 RGB modules
- DDR5 modules
- Laptop platforms

## Protocol & Modes

Effect modes researched from [OpenRGB](https://gitlab.com/CalcProgrammer1/OpenRGB/-/tree/master/Controllers/TForceXtreemController):

- **Source-Confirmed**: All 26 mode IDs (0x00-0x19) and protocol from OpenRGB
- **Hardware-Verified**: Basic OFF/ON register writes
- **Hardware-Unverified**: Visual appearance of effects (no remote hardware access during development)

See [EFFECTS_RESEARCH.md](EFFECTS_RESEARCH.md) for complete mode table and register documentation.

## Limitations

- **SMBus Access**: Requires administrator privileges (hardware-level control)
- **AMD Only**: Uses AMD SMBus/PIIX4 driver (Intel platforms have different controllers)
- **Single Module Type**: Protocol specific to T-Force XTREEM ARGB DDR4
- **Task Manager Visibility**: Autostart task doesn't appear in Task Manager Startup tab (elevated tasks are excluded by Windows for security)

## Troubleshooting

**No modules detected:**
- Run as Administrator
- Stop ASUS Aura/Armoury Crate (conflicts with direct SMBus access)
- Verify PawnIOLib.dll and SmbusPIIX4.bin are in same folder as exe

**Performance issues / slow startup:**
- Enable diagnostic logging to measure actual timings:
  ```cmd
  set XTREEM_DIAG=1
  XtreemRgbToggle.exe
  ```
- Check `%LocalAppData%\XtreemRgbToggle\diagnostic.log` for operation timings
- Report log with any performance issues

**Autostart doesn't work:**
- Open Task Scheduler → Find `XtreemRgbToggle-Off`
- Verify: Enabled, "Run with highest privileges" checked
- Trigger: "At log on" with PT0S (zero) delay

## License

MIT License - See [LICENSE](LICENSE)

**Third-party components:**
- OpenRGB protocol research: GPL-2.0-or-later ([OpenRGB Project](https://gitlab.com/CalcProgrammer1/OpenRGB))
- PawnIO driver binaries: See [native/COPYING.PawnIO.Modules](native/COPYING.PawnIO.Modules)

## Credits

- Protocol research based on [OpenRGB T-Force Xtreem controller](https://gitlab.com/CalcProgrammer1/OpenRGB/-/tree/master/Controllers/TForceXtreemController) by Milan Cermak (krysmanta)
- PawnIO SMBus driver (binary distribution)

## Contributing

Issues and pull requests welcome. Please test on actual hardware before submitting.

## Disclaimer

**USE AT YOUR OWN RISK.** Direct SMBus access can potentially cause hardware issues if misused. This tool writes to documented RAM RGB controller registers only. Always run reputable RGB control software to verify your hardware works before using alternative tools.
