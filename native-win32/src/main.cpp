#include "ui.hpp"
#include "cli.hpp"
#include "xtreem.hpp"
#include "persist.hpp"
#include "autostart.hpp"
#include "diag.hpp"
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <iostream>
#include <cstdlib>
#include <stdexcept>

static std::wstring exe_path() {
    wchar_t buf[MAX_PATH]{};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return buf;
}

// GUI/tray/restore/off never AllocConsole (no window titled with this exe path).
// Interactive CLI attaches to a parent console if present, otherwise AllocConsole.
static void setup_cli_console(bool alloc_if_none) {
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE* f = nullptr;
        freopen_s(&f, "CONOUT$", "w", stdout);
        freopen_s(&f, "CONOUT$", "w", stderr);
        freopen_s(&f, "CONIN$", "r", stdin);
        return;
    }
    if (!alloc_if_none) return;
    if (!GetConsoleWindow()) {
        AllocConsole();
        FILE* f = nullptr;
        freopen_s(&f, "CONOUT$", "w", stdout);
        freopen_s(&f, "CONOUT$", "w", stderr);
        freopen_s(&f, "CONIN$", "r", stdin);
    }
}

static int fail_driver(const PawnioProbe& p) {
    std::wcerr << p.message << L"\n" << pawnio_next_step(p.status) << L"\n"
               << L"PawnIOLib.dll and SmbusPIIX4.bin are user-mode files, not the kernel driver.\n";
    return 4;
}

static int cmd_detect(const std::wstring& dir) {
    XtreemHardware hw(dir);
    if (!hw.last_probe.can_use_hardware()) return fail_driver(hw.last_probe);
    auto d = hw.detect();
    std::wcout << L"Modules found: " << d.size() << L"\n";
    for (auto& x : d)
        std::wcout << L"  0x" << std::hex << (int)x.address7 << L" port " << std::dec << x.bus_port << L"\n";
    return d.empty() ? 3 : 0;
}
static int cmd_once(const std::wstring& dir, bool off) {
    XtreemHardware hw(dir);
    if (!hw.last_probe.can_use_hardware()) return fail_driver(hw.last_probe);
    auto d = hw.detect();
    if (d.empty()) { std::wcout << L"No XTREEM modules found.\n"; return 3; }
    if (off) { hw.apply_off(d); save_state(LightState::Off); std::wcout << L"OK: OFF\n"; }
    else { hw.apply_on(d); save_state(LightState::On); std::wcout << L"OK: ON\n"; }
    std::wcout << hw.dump(d);
    return 0;
}
static int cmd_restore(const std::wstring& dir) {
    Profile p;
    auto pst = load_profile_status(p);
    auto act = profile_restore_action(pst, p.modules.size());
    if (act != RestoreAction::Apply) {
        std::wcout << restore_skip_message(act) << L"\n";
        return act == RestoreAction::SkipCorrupt ? 5 : 0;
    }
    XtreemHardware hw(dir);
    if (!hw.last_probe.can_use_hardware()) return fail_driver(hw.last_probe);
    auto d = hw.detect();
    if (d.empty()) { std::wcout << L"No XTREEM modules found.\n"; return 3; }
    hw.restore_profile(d, p);
    std::wcout << L"Restored " << p.modules.size() << L" module profile(s), then exit.\n";
    return 0;
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR cmd, int) {
    auto dir = exe_directory();
    std::wstring cl = cmd ? cmd : L"";
    std::wstring low = cl;
    for (auto& c : low) if (c >= L'A' && c <= L'Z') c = (wchar_t)(c + 32);
    if (low.find(L"--diag") != std::wstring::npos || getenv("XTREEM_DIAG"))
        diag::enable(true);

    auto tok = cli_first_token(cl);
    auto kind = classify_cli_token(tok);

    if (kind == CliKind::Gui) {
        detach_console_for_gui();
        return run_gui(inst, dir.c_str(), GuiMode::Normal, nullptr);
    }
    if (kind == CliKind::Tray) {
        detach_console_for_gui();
        return run_tray(inst, dir.c_str());
    }
    if (kind == CliKind::GuiTest)
        return run_gui_self_test(inst);

    setup_cli_console(kind == CliKind::InteractiveCli);
    try {
        if (tok == L"off") return cmd_once(dir, true);
        if (tok == L"on") return cmd_once(dir, false);
        if (tok == L"toggle") {
            auto st = load_state();
            return cmd_once(dir, st != LightState::Off);
        }
        if (tok == L"detect") return cmd_detect(dir);
        if (tok == L"restore") return cmd_restore(dir);
        if (tok == L"status") {
            auto st = load_state();
            std::wcout << L"Saved state: " << (st == LightState::Off ? L"Off" : st == LightState::On ? L"On" : L"Unknown") << L"\n";
            Autostart a;
            auto as = a.query();
            std::wcout << L"Autostart: " << (as == AutostartInstallState::Installed ? L"Installed" :
                as == AutostartInstallState::NotInstalled ? L"NotInstalled" : L"Unknown") << L"\n";
            auto pr = probe_pawnio(dir);
            std::wcout << pr.message << L"\n" << pawnio_next_step(pr.status) << L"\n";
            return pr.can_use_hardware() ? 0 : 4;
        }
        if (tok == L"debug" || tok == L"dump") {
            XtreemHardware hw(dir);
            if (!hw.last_probe.can_use_hardware()) return fail_driver(hw.last_probe);
            auto d = hw.detect();
            if (d.empty()) return 3;
            std::wcout << hw.dump(d);
            return 0;
        }
        if (tok == L"install-autostart") {
            Autostart a; a.install(exe_path());
            std::wcout << L"OK: installed " << Autostart::kTask << L"\n";
            return 0;
        }
        if (tok == L"uninstall-autostart") {
            Autostart a; a.uninstall();
            std::wcout << L"OK: removed autostart task if present\n";
            return 0;
        }
        std::wcout <<
            L"XtreemRgbToggle (native Win32, /SUBSYSTEM:WINDOWS)\n"
            L"  (no args) | gui     GUI (no console window)\n"
            L"  tray               start in tray; Open / Exit on the icon. Minimize hides to tray. Close quits.\n"
            L"  off|on|toggle|restore   hardware then exit (no new console; attach parent if launched from cmd)\n"
            L"  detect|status|debug|help   print to a console (allocates one only if no parent)\n"
            L"  install-autostart | uninstall-autostart  (logon task runs silent restore)\n"
            L"PawnIO kernel driver: https://pawnio.eu (not bundled, not auto-installed).\n"
            L"GUI: Effect list includes Off (LEDs black, not Static) and T-Force Default.\n";
        return tok == L"help" || tok == L"-h" || tok == L"--help" ? 0 : 2;
    } catch (const std::exception& ex) {
        setup_cli_console(true);
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
