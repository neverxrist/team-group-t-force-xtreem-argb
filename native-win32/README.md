# Native Win32 build (C++)

Compact portable Windows x64 rewrite of the C# WinForms app. The C# tree is unchanged and remains the rollback.

## PawnIO check / install (native GUI — this is where it lives)

The driver check is the **top of the main window**, before effects/color. Hardware controls stay disabled until the probe reports **Ready**. Large ON/OFF buttons are removed; **Off (LEDs black)** and **T-Force Default** are in the Effect list. Off is not Static (0x01).

On startup (and on **Retry after installing** / **Rescan modules**):

1. Status line: `Checking PawnIO driver...` — **no SMBus / module detect yet**.
2. Probe looks at, in order: bundled `PawnIOLib.dll`, bundled `SmbusPIIX4.bin`, Windows service `PawnIO`, then `pawnio_version` + `pawnio_open`.
3. The PawnIO status label shows one classified result (HRESULT included). Failures are **not** all labeled “missing driver”.
4. If the probe is not Ready, a dialog explains the **specific** class and the next step. Effect/color stay disabled.
5. **Official PawnIO download/install page** is a SysLink `https://pawnio.eu` plus the button **Open official PawnIO page**. Both only `ShellExecute` the official site. Re-verified 2026-11-07: that page’s “Download Now” is `https://github.com/namazso/PawnIO.Setup/releases/latest/download/PawnIO_setup.exe`. This program does **not** download or run that installer.
6. After you install the kernel driver yourself, click **Retry after installing**. The probe runs again; only then is module detect allowed.

| Status | Meaning | What the UI tells you |
|---|---|---|
| Ready | `pawnio_version` and `pawnio_open` succeeded | Detect modules. DLL/bin are still not the driver. |
| Driver missing | Service does not exist / HRESULT `0x80070424` | Understandable “not installed” text + official page + Retry. |
| Driver stopped (unavailable) | Installed, not running / `0x80070426` | Start the `PawnIO` service or reinstall from the official page, then Retry. |
| Access denied | `0x80070005` / SCM `ERROR_ACCESS_DENIED` | Run as Administrator. **Not** diagnosed as missing driver. |
| Other init failure | Any other HRESULT | HRESULT shown; **not** labeled missing driver. Retry. |
| DLL/bin missing | User-mode files absent next to the exe | Restore ZIP files. Those files are not the kernel driver. |

CLI (`detect` / `off` / `on` / `status` / …) runs the same probe first and prints the classified message + next step. `status` does not touch SMBus.

No automatic installer. Bundled DLL/bin are never treated as an installed kernel driver. This tree does not ship a finished Setup.exe.

## Run

Extract the portable ZIP. Right-click `XtreemRgbToggle.exe` → Run as administrator. The exe is `/SUBSYSTEM:WINDOWS`. Double-click / `gui` / `tray` / `restore` / `off` / `on` / `toggle` do **not** AllocConsole (no extra console titled with the exe path). From cmd.exe, those commands AttachConsole to the parent so you still see output. `detect` / `status` / `debug` / `help` allocate a console only if there is no parent.

- **Close** and tray **Exit** quit (icon removed, worker joined). **Minimize** hides to tray; tray **Open** (or double-click) restores. `tray` starts hidden with an icon.
- Autostart still runs `off` at logon (silent, no new console). Child process is `schtasks.exe` with `CREATE_NO_WINDOW`; this app does not launch `cmd.exe`.

CLI: `off` `on` `toggle` `detect` `status` `debug` `restore` `install-autostart` `uninstall-autostart` `tray`. `restore` applies `%LocalAppData%\XtreemRgbToggle\profile.json` then exits (`state.txt` off/on if no JSON). `XTREEM_DIAG=1` or `--diag` writes `%LocalAppData%\XtreemRgbToggle\diagnostic.log`. `--diag` alone still opens the GUI.

## Build (Windows, MSVC)

```bat
cmake -S native-win32 -B native-win32/build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build native-win32/build
native-win32\build\out\xtreem_logic_tests.exe
native-win32\build\out\xtreem_gui_smoke.exe
```

`xtreem_gui_smoke` runs the real main window against an in-process `FakeHw` (no PawnIO/SMBus): probe/detect, Off vs T-Force vs effect dispatch, module select, color, live-preview coalescing, profile persist, driver-missing Retry, minimize/tray Open, close while a job is pending.

Visual Studio generator also works if vswhere finds it (`-G "Visual Studio 17 2022" -A x64`); binaries still go to `native-win32/build/out/`.

Static CRT (`/MT`). No extra runtime install. Hardware settling sleeps match the C# source (10 ms remap, 20 ms after apply).

## What the C# build actually does

Looked up in `XtreemRgbToggle/XtreemRgbToggle/AmdSmBus.cs` and `RgbControlForm.cs` (cheap, in-tree):

- There is **no** pre-hardware PawnIO probe and **no** driver-status panel.
- GUI startup calls `StartDetectAsync` → `DetectAndRemap` → `AmdSmBus.Open` on the UI worker. First driver contact is that Open.
- `pawnio_version` failure is **one** message for every HRESULT: “PawnIO is not installed or the driver is not loaded. Install from https://pawnio.eu and run as Administrator.” Permission errors and other init failures are lumped in as missing/not loaded.
- `pawnio_open` only special-cases `0x80070005` as “(Administrator rights required)”; other open HRESULTs are `pawnio_open failed` with no missing-vs-other split.
- The form shows `Detection error: {ex.Message}` in a generic MessageBox. Empty modules: “check PawnIO, run as admin, stop Aura”.
- **No** Retry control, **no** button/SysLink to pawnio.eu. The URL is in the CLI help / README only.
- C# `status` does not probe PawnIO at all (saved `state.txt` + autostart only).

## Limits

No Windows GUI/hardware timing was measured in this Linux environment. Logic tests and the FakeHw main-window smoke run in CI. One-off cmd.exe errors at Windows shutdown were not reproduced here and are not diagnosed as a missing-driver or AllocConsole issue; shutdown path now skips schtasks spawn, drops queued jobs, and joins the worker. No Setup.exe. Automatic PawnIO install remains out of scope unless a later change verifies the vendor installer and is separately authorized.
