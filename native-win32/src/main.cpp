#include "ui.hpp"
#include "xtreem.hpp"
#include "persist.hpp"
#include "autostart.hpp"
#include "diag.hpp"
#include <windows.h>
#include <string>
#include <vector>
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
static void ensure_console() {
    if (!GetConsoleWindow()) {
        AllocConsole();
        FILE* f = nullptr;
        freopen_s(&f, "CONOUT$", "w", stdout);
        freopen_s(&f, "CONOUT$", "w", stderr);
        freopen_s(&f, "CONIN$", "r", stdin);
    }
}
static int fail_driver(const PawnioProbe& p) {
    std::wcerr << p.message << L"\n";
    std::wcerr << L"Install the kernel driver yourself from " << kPawnioOfficialPage
               << L" (this program does not download it).\n"
               << L"PawnIOLib.dll and SmbusPIIX4.bin are not the driver.\n";
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
    XtreemHardware hw(dir);
    if (!hw.last_probe.can_use_hardware()) return fail_driver(hw.last_probe);
    Profile p;
    if (!load_profile(p) || p.modules.empty()) {
        auto st = load_state();
        if (st == LightState::Unknown) { std::wcout << L"No profile.json or state.txt to restore.\n"; return 5; }
        return cmd_once(dir, st == LightState::Off);
    }
    auto d = hw.detect();
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

    auto first = low;
    // isolate first token
    while (!first.empty() && first[0] == L' ') first.erase(0, 1);
    auto sp = first.find(L' ');
    std::wstring tok = sp == std::wstring::npos ? first : first.substr(0, sp);
    if (tok.size() >= 2 && tok.front() == L'"') tok = tok.substr(1, tok.size() - 2);

    if (tok.empty() || tok == L"gui" || tok == L"ui")
        return run_gui(inst, dir.c_str(), false);
    if (tok == L"--gui-smoke")
        return run_gui(inst, dir.c_str(), true);
    if (tok == L"tray")
        return run_tray(inst, dir.c_str());

    ensure_console();
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
            std::wcout << pr.message << L"\n";
            return 0;
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
            L"XtreemRgbToggle (native Win32)\n"
            L"  (no args)  GUI\n"
            L"  off|on|toggle|detect|status|debug|restore\n"
            L"  install-autostart | uninstall-autostart | tray\n"
            L"PawnIO kernel driver: https://pawnio.eu (not bundled, not auto-installed)\n";
        return tok == L"help" || tok == L"-h" || tok == L"--help" ? 0 : 2;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
