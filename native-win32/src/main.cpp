#include "ui.hpp"
#include "cli.hpp"
#include "xtreem.hpp"
#include "persist.hpp"
#include "restore.hpp"
#include "autostart.hpp"
#include "diag.hpp"
#include <cstdio>
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
static void restore_log_addrs(const char* tag, const std::vector<uint8_t>& addrs) {
    char buf[256];
    int n = snprintf(buf, sizeof(buf), "%s n=%d", tag, (int)addrs.size());
    for (auto a : addrs) {
        if (n < 0 || n >= (int)sizeof(buf) - 8) break;
        n += snprintf(buf + n, sizeof(buf) - (size_t)n, " 0x%02X", a);
    }
    restore_event(buf);
}

static int cmd_restore(const std::wstring& dir) {
    restore_event("begin");
    Profile p;
    auto pst = load_profile_status(p);
    auto act = profile_restore_action(pst, p.modules.size());
    {
        char b[80];
        snprintf(b, sizeof(b), "profile status=%d modules=%d", (int)pst, (int)p.modules.size());
        restore_event(b);
    }
    std::vector<uint8_t> saved;
    for (auto& m : p.modules) saved.push_back(m.address);
    restore_log_addrs("profile_addrs", saved);
    if (act != RestoreAction::Apply) {
        restore_event(act == RestoreAction::SkipCorrupt ? "exit=5 corrupt" : "exit=0 skip");
        std::wcout << restore_skip_message(act) << L"\n";
        return act == RestoreAction::SkipCorrupt ? kExitCorruptProfile : kExitOk;
    }
    XtreemHardware hw(dir);
    if (!hw.last_probe.can_use_hardware()) {
        char b[96];
        snprintf(b, sizeof(b), "exit=4 driver status=%d", (int)hw.last_probe.status);
        restore_event(b);
        return fail_driver(hw.last_probe);
    }
    restore_event("probe ready; detect (retries are detect-only, protocol sleeps unchanged)");
    int attempts_used = 0;
    bool detect_threw = false;
    auto d = detect_with_retry([&]() -> std::vector<XtreemDevice> {
        try {
            return hw.detect();
        } catch (const std::exception& ex) {
            detect_threw = true;
            char b[192];
            snprintf(b, sizeof(b), "detect_throw %.120s", ex.what());
            restore_event(b);
            return {};
        }
    }, kRestoreDetectAttempts, kRestoreDetectDelayMs, &attempts_used);
    {
        char b[128];
        snprintf(b, sizeof(b), "detect attempts=%d found=%d ports_opened=%d ports_failed=%d hits=%d",
                 attempts_used, (int)d.size(), hw.last_detect_trace.ports_opened,
                 hw.last_detect_trace.ports_failed, hw.last_detect_trace.hits);
        restore_event(b);
    }
    std::vector<uint8_t> found_a;
    for (auto& x : d) found_a.push_back(x.address7);
    restore_log_addrs("found_addrs", found_a);
    auto plan = match_profile_to_found(p, d);
    restore_log_addrs("matched_addrs", [&] {
        std::vector<uint8_t> a;
        for (auto& m : plan.matched) a.push_back(m.device.address7);
        return a;
    }());
    restore_log_addrs("stale_profile_addrs", plan.stale_profile_addrs);
    int rc = restore_result_exit(act, true, detect_threw, d, plan);
    if (rc == kExitNoModules) {
        restore_event("exit=3 no modules after Ready probe (this exe code, not Win32 path-not-found)");
        std::wcout << L"No XTREEM modules found.\n";
        return rc;
    }
    if (rc == kExitNoProfileMatch) {
        restore_event("exit=6 detected modules none match saved addresses; nothing applied");
        std::wcout << L"Detected modules do not match saved profile addresses; nothing applied.\n";
        return rc;
    }
    if (rc == kExitError) {
        restore_event("exit=1 detect threw and found none");
        return rc;
    }
    hw.restore_profile(d, p);
    {
        char b[80];
        snprintf(b, sizeof(b), "exit=0 applied=%d stale=%d", (int)plan.matched.size(),
                 (int)plan.stale_profile_addrs.size());
        restore_event(b);
    }
    std::wcout << L"Restored " << plan.matched.size() << L" matching module(s), then exit.\n";
    return kExitOk;
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
            L"restore.log: %%LocalAppData%%\\XtreemRgbToggle\\restore.log (exit 3 = detect found no modules)\n"
            L"PawnIO kernel driver: https://pawnio.eu (not bundled, not auto-installed).\n"
            L"GUI: Effect list includes Off (LEDs black, not Static) and T-Force Default.\n";
        return tok == L"help" || tok == L"-h" || tok == L"--help" ? 0 : 2;
    } catch (const std::exception& ex) {
        setup_cli_console(true);
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
