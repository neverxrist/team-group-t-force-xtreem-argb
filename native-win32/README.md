# Native Win32 build (C++)

Compact portable Windows x64 rewrite of the C# WinForms app. The C# tree is unchanged and remains the rollback.

## Run

Extract the portable ZIP. Install the **PawnIO kernel driver** yourself from https://pawnio.eu (official site; “Download Now” is the vendor installer). This program does **not** download or run that installer.

`PawnIOLib.dll` and `SmbusPIIX4.bin` in the ZIP are user-mode files. They are not the kernel driver.

Right-click `XtreemRgbToggle.exe` → Run as administrator.

On startup the GUI **probes PawnIO before any SMBus work** and shows a specific status:

| Status | Meaning | UI |
|---|---|---|
| Ready | Driver usable | Detect modules |
| Driver missing | No PawnIO service | Open PawnIO site + Retry |
| Driver stopped | Service installed, not running | Open site + Retry |
| Access denied | HRESULT 0x80070005 / admin | Run as Administrator |
| Other init failure | HRESULT shown; **not** labeled “missing driver” | Retry |
| DLL/bin missing | Bundled user-mode files absent | Restore ZIP files |

**Open PawnIO site** opens https://pawnio.eu (verified 2026-11-07). **Retry** re-runs the probe after you install manually.

CLI: `off` `on` `toggle` `detect` `status` `debug` `restore` `install-autostart` `uninstall-autostart` `tray`. `restore` applies `%LocalAppData%\XtreemRgbToggle\profile.json` then exits (`state.txt` off/on if no JSON). `XTREEM_DIAG=1` or `--diag` writes `%LocalAppData%\XtreemRgbToggle\diagnostic.log`.

## Build (Windows, MSVC)

```bat
cmake -S native-win32 -B native-win32/build -G "Visual Studio 17 2022" -A x64
cmake --build native-win32/build --config Release
native-win32\build\Release\xtreem_logic_tests.exe
native-win32\build\Release\xtreem_gui_smoke.exe
```

Static CRT (`/MT`). No extra runtime install. Hardware settling sleeps match the C# source (10 ms remap, 20 ms after apply).

## What the C# build actually does

C# does **not** probe the driver before UI hardware work. `AmdSmBus.Open` calls `pawnio_version`; any failure is one message: “PawnIO is not installed or the driver is not loaded.” `pawnio_open` only special-cases `0x80070005` as “Administrator rights required.” The GUI shows a generic detection MessageBox. There is no Retry control and no button to pawnio.eu. README mentions the URL only.

## Limits

No Windows GUI/hardware timing was measured in this Linux environment. Logic tests and a GUI-smoke window (no PawnIO) run in CI. No Setup.exe.
