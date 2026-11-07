#include "persist.hpp"
#include "autostart.hpp"
#include "modes.hpp"
#include "smbus.hpp"
#include "xtreem.hpp"
#include "pawnio.hpp"
#include "cli.hpp"
#include "hw_worker.hpp"
#include <windows.h>
#include <winsvc.h>
#include <iostream>
#include <map>
#include <stdexcept>
#include <filesystem>

static int g_fail = 0;
#define CHECK(cond) do { if (!(cond)) { std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " #cond "\n"; g_fail++; } } while(0)

struct FakeBus : ISmBus {
    std::map<uint32_t, uint8_t> bytes;
    std::map<uint32_t, uint16_t> words;
    std::map<uint8_t, bool> present;
    std::map<uint8_t, bool> xtreem;
    uint32_t key(uint8_t a, uint8_t c) { return ((uint32_t)a << 16) | c; }
    bool probe(uint8_t addr7) override { return present[addr7]; }
    uint8_t read_byte_data(uint8_t addr7, uint8_t command) override {
        if (command >= 0x90 && command <= 0xA0 && xtreem[addr7])
            return (uint8_t)(command - 0x80);
        auto it = bytes.find(key(addr7, command));
        if (it == bytes.end()) throw std::runtime_error("nack");
        return it->second;
    }
    void write_byte_data(uint8_t addr7, uint8_t command, uint8_t value) override {
        bytes[key(addr7, command)] = value;
    }
    void write_word_data(uint8_t addr7, uint8_t command, uint16_t value) override {
        words[key(addr7, command)] = value;
    }
};

int main() {
    CHECK(kModeCount == 26);
    CHECK(kModes[0].value == 0);
    CHECK(kModes[25].value == 25);
    CHECK(MODE_TFORCE_DEFAULT == 0x19);
    CHECK(swap_reg(0xE020) == 0x20E0);
    CHECK(speed_from_slider(0) == SPEED_FASTEST);
    CHECK(speed_from_slider(4) == SPEED_SLOWEST);

    CHECK(autostart_combine(TaskQueryResult::Present, TaskQueryResult::Absent) == AutostartInstallState::Installed);
    CHECK(autostart_combine(TaskQueryResult::Absent, TaskQueryResult::Absent) == AutostartInstallState::NotInstalled);
    CHECK(autostart_combine(TaskQueryResult::Unknown, TaskQueryResult::Absent) == AutostartInstallState::Unknown);
    CHECK(!autostart_can_cache(AutostartInstallState::Unknown));
    CHECK(autostart_can_cache(AutostartInstallState::Installed));

    CHECK(classify_pawnio(false, true, true, 0, false, 0, 0, 0) == PawnioStatus::BundledDllMissing);
    CHECK(classify_pawnio(true, false, true, 0, false, 0, 0, 0) == PawnioStatus::BundledBinMissing);
    CHECK(classify_pawnio(true, true, true, ERROR_SERVICE_DOES_NOT_EXIST, false, 0, 0, (long)0x80070424) == PawnioStatus::DriverMissing);
    CHECK(classify_pawnio(true, true, true, ERROR_ACCESS_DENIED, false, 0, 0, (long)0x80070005) == PawnioStatus::PermissionDenied);
    CHECK(classify_pawnio(true, true, true, 0, true, SERVICE_STOPPED, 0, (long)0x80070426) == PawnioStatus::DriverStopped);
    CHECK(classify_pawnio(true, true, true, 0, true, SERVICE_RUNNING, 0, (long)0x80070005) == PawnioStatus::PermissionDenied);
    CHECK(classify_pawnio(true, true, true, 0, true, SERVICE_RUNNING, 0, (long)0x8000ABCD) == PawnioStatus::Other);
    CHECK(classify_pawnio(true, true, true, 0, true, SERVICE_RUNNING, 0, 0) == PawnioStatus::Ready);
    // Successful open wins over a "service missing" SCM query — not every failure is "missing driver".
    CHECK(classify_pawnio(true, true, true, ERROR_SERVICE_DOES_NOT_EXIST, false, 0, 0, 0) == PawnioStatus::Ready);
    CHECK(classify_pawnio(true, true, true, 0, true, SERVICE_RUNNING, (long)0x80004005, 0) == PawnioStatus::Other);
    {
        auto other = pawnio_status_text(PawnioStatus::Other, (long)0x8000ABCD, (long)0x8000ABCD, 0);
        CHECK(other.find(L"not classified as a missing driver") != std::wstring::npos);
        auto denied = pawnio_status_text(PawnioStatus::PermissionDenied, 0, (long)0x80070005, 0);
        CHECK(denied.find(L"not classified as a missing driver") != std::wstring::npos);
        CHECK(pawnio_next_step(PawnioStatus::DriverMissing).find(L"pawnio.eu") != std::wstring::npos);
        CHECK(pawnio_next_step(PawnioStatus::PermissionDenied).find(L"missing driver") != std::wstring::npos);
        PawnioProbe missing; missing.status = PawnioStatus::DriverMissing;
        PawnioProbe perm; perm.status = PawnioStatus::PermissionDenied;
        PawnioProbe otherp; otherp.status = PawnioStatus::Other;
        CHECK(missing.offer_install_page());
        CHECK(!perm.offer_install_page());
        CHECK(!otherp.offer_install_page());
        CHECK(!otherp.can_use_hardware());
    }

    auto tmp = std::filesystem::temp_directory_path() / "xtreem_test";
    std::filesystem::create_directories(tmp);
    auto st = (tmp / "state.txt").wstring();
    auto pr = (tmp / "profile.json").wstring();
    save_state(LightState::Off, st.c_str());
    CHECK(load_state(st.c_str()) == LightState::Off);
    save_state(LightState::On, st.c_str());
    CHECK(load_state(st.c_str()) == LightState::On);
    CHECK(load_state((tmp / L"nope.txt").c_str()) == LightState::Unknown);

    Profile p;
    p.modules.push_back(ModuleProfile{0x39, 0, "static", 1, 2, 0, 10, 20, 30});
    p.modules.push_back(ModuleProfile{0x3A, 0, "off", 0, 2, 0, 0, 0, 0});
    CHECK(save_profile(p, pr.c_str()));
    Profile q;
    CHECK(load_profile(q, pr.c_str()));
    CHECK(q.modules.size() == 2);
    CHECK(q.modules[0].address == 0x39);
    CHECK(q.modules[0].r == 10 && q.modules[0].b == 30);
    CHECK(q.modules[1].kind == "off");

    FakeBus bus;
    bus.present[0x39] = true;
    bus.xtreem[0x39] = true;
    CHECK(test_for_xtreem(bus, 0x39));
    CHECK(!test_for_xtreem(bus, 0x3A));
    apply_static_color(bus, { XtreemDevice{0x39, 0} }, 1, 2, 3);
    // last LED G write uses command 0x01 after word pointer — FakeBus records SMBus cmds not mapped LED regs.
    CHECK(device_present(bus, 0x39));

    auto found = detect_and_remap([&](int port) -> std::unique_ptr<ISmBus> {
        if (port != 0) throw std::runtime_error("no port 1");
        auto f = std::make_unique<FakeBus>();
        f->present[0x39] = true;
        f->xtreem[0x39] = true;
        return f;
    });
    CHECK(found.size() == 1);
    CHECK(found[0].address7 == 0x39);

    CHECK(classify_cli_token(cli_first_token(L"")) == CliKind::Gui);
    CHECK(classify_cli_token(cli_first_token(L"--diag")) == CliKind::Gui);
    CHECK(classify_cli_token(cli_first_token(L"  GUI")) == CliKind::Gui);
    CHECK(classify_cli_token(L"tray") == CliKind::Tray);
    CHECK(classify_cli_token(L"off") == CliKind::SilentHw);
    CHECK(classify_cli_token(L"restore") == CliKind::SilentHw);
    CHECK(classify_cli_token(L"detect") == CliKind::InteractiveCli);
    CHECK(effect_apply_path(MODE_OFF) == EffectApplyPath::LightsOff);
    CHECK(effect_apply_path(MODE_STATIC) == EffectApplyPath::Effect);
    CHECK(effect_apply_path(MODE_TFORCE_DEFAULT) == EffectApplyPath::TforceDefault);

    {
        auto tdir = tmp / "state_env";
        std::filesystem::create_directories(tdir);
        SetEnvironmentVariableW(L"XTREEM_STATE_DIR", tdir.wstring().c_str());
        save_state(LightState::Off);
        CHECK(load_state() == LightState::Off);
        SetEnvironmentVariableW(L"XTREEM_STATE_DIR", nullptr);
    }

    {
        FakeHw f;
        f.probe.status = PawnioStatus::Ready;
        f.probe.message = L"fake";
        f.devices = { {0x39, 0} };
        f.stall_ms = 70;
        {
            HwWorker w(L".", &f);
            HwJob a; a.kind = HwKind::PreviewColor; a.r = 1; a.devices = f.devices;
            w.post(a);
            Sleep(15);
            a.r = 2; w.post(a);
            a.r = 9; w.post(a);
            Sleep(400);
        }
        CHECK(f.last_r == 9);
        CHECK(f.preview_n >= 1);
        CHECK(f.preview_n <= 2);
        CHECK(!f.used_real_hardware);
    }
    {
        FakeHw f;
        f.probe.status = PawnioStatus::Ready;
        f.devices = { {0x39, 0} };
        f.stall_ms = 120;
        {
            HwWorker w(L".", &f);
            HwJob j; j.kind = HwKind::Off; j.devices = f.devices;
            w.post(j);
        }
        CHECK(!f.used_real_hardware);
    }

    if (g_fail) { std::cerr << g_fail << " failed\n"; return 1; }
    std::cout << "All logic tests passed\n";
    return 0;
}
