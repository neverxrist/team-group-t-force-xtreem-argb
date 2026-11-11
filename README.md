# XTREEM RGB Control

Native Win32 C++ utility for TEAMGROUP T-Force XTREEM ARGB DDR4 lighting on AMD Ryzen (PIIX4 SMBus). No .NET runtime. Portable ZIP is about 260 KB.

**Download:** [v2.0.1](https://github.com/neverxrist/team-group-t-force-xtreem-argb/releases/tag/v2.0.1) (`XtreemRgbToggle-native-win-x64.zip`)

A C# WinForms tree remains in this repo for reference only. Use the native ZIP.

## Requirements

- Windows 10/11 x64
- Run as Administrator
- TEAMGROUP T-Force XTREEM ARGB DDR4 on AMD Ryzen (SMBus)
- **PawnIO kernel driver** installed by you from [pawnio.eu](https://pawnio.eu). This app never downloads or runs that installer. Bundled `PawnIOLib.dll` and `SmbusPIIX4.bin` are user-mode files, not the driver.

## Quick start

1. Unzip to a **permanent** folder (not a temp extract). Keep the DLL and `.bin` next to `XtreemRgbToggle.exe`.
2. Right-click `XtreemRgbToggle.exe` → Run as administrator.
3. If PawnIO is missing, use **Open official PawnIO page**, install the driver yourself, then **Retry after installing**.

## Features

- Module selection (all or one stick)
- 26 effects, including **Off** (LEDs black, not Static) and **T-Force Default**
- Static color, live preview while dragging
- Per-module profile saved only after a successful **Apply**; restored once after detect on GUI start
- Optional autostart: Task Scheduler task `XtreemRgbToggle-Off` runs silent `restore` at logon (not unconditional off)
- Minimize goes to tray; Close and tray **Exit** quit. No extra console window on GUI/tray/restore/off.

## Logs

- `%LocalAppData%\XtreemRgbToggle\restore.log` — always written on `restore` (appends; truncated when larger than 48 KiB)
- `%LocalAppData%\XtreemRgbToggle\profile.json` — applied colors/effects
- `XTREEM_DIAG=1` or `--diag` → `%LocalAppData%\XtreemRgbToggle\diagnostic.log` (operation names and milliseconds, no secrets)

## CLI

```batch
XtreemRgbToggle.exe              GUI
XtreemRgbToggle.exe tray         start in tray
XtreemRgbToggle.exe off|on|toggle|restore
XtreemRgbToggle.exe detect|status|debug|help
XtreemRgbToggle.exe install-autostart
XtreemRgbToggle.exe uninstall-autostart
```

`restore` exit **3** means detect found no modules after a Ready PawnIO probe (this program’s code, not Win32 path-not-found). Exit **6** means found modules did not match saved addresses; nothing is applied. Details go to `restore.log`.

## Limitations

- Tested by the owner on one PC. That is not a broad hardware validation.
- RGB **Off does not persist across a full power loss**; there is no documented hardware save. Autostart `restore` re-applies the saved profile after logon.
- No installer / Setup.exe.
- AMD SMBus only. Intel, DDR5, and other RGB modules are unsupported.
- Direct SMBus access can interfere with other RGB software (turn off Aura/Armoury Crate).

## Build from source (Windows, MSVC)

```bat
cmake -S native-win32 -B native-win32/build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build native-win32/build
native-win32\build\out\xtreem_logic_tests.exe
native-win32\build\out\xtreem_gui_smoke.exe
```

Logic tests and FakeHw GUI smoke do not talk to real SMBus. The C# project (`XtreemRgbToggle.sln`) is optional reference.

## License

MIT — see [LICENSE](LICENSE).

- OpenRGB protocol research (reference only, no source copied): GPL-2.0-or-later, [T-Force Xtreem controller](https://gitlab.com/CalcProgrammer1/OpenRGB/-/tree/master/Controllers/TForceXtreemController) (Milan Cermak / krysmanta)
- PawnIO user-mode binaries: LGPL — [native/COPYING.PawnIO.Modules](native/COPYING.PawnIO.Modules)
