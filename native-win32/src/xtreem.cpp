#include "xtreem.hpp"
#include "restore.hpp"
#include "diag.hpp"
#include <windows.h>
#include <map>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <filesystem>

bool device_present(ISmBus& bus, uint8_t addr) {
    if (bus.probe(addr)) return true;
    try { bus.read_byte_data(addr, 0x00); return true; }
    catch (...) { return false; }
}

bool test_for_xtreem(ISmBus& bus, uint8_t addr) {
    if (!device_present(bus, addr)) return false;
    try {
        for (int i = 0x90; i <= 0xA0; i++) {
            uint8_t expected = (uint8_t)(i - 0x80);
            uint8_t got = bus.read_byte_data(addr, (uint8_t)i);
            if (got != expected) return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

void remap_modules(ISmBus& bus) {
    if (!device_present(bus, BOOT_ADDR)) return;
    int addrIdx = 0;
    const int n = (int)(sizeof(kRemapAddresses) / sizeof(kRemapAddresses[0]));
    for (int slot = 0; slot <= 8; slot++) {
        if (!device_present(bus, BOOT_ADDR)) break;
        int free = -1;
        while (addrIdx < n) {
            uint8_t candidate = kRemapAddresses[addrIdx++];
            if (!device_present(bus, candidate)) { free = candidate; break; }
        }
        if (free < 0) break;
        register_write(bus, BOOT_ADDR, REG_SLOT, (uint8_t)slot);
        register_write(bus, BOOT_ADDR, REG_I2CADDR, (uint8_t)(free << 1));
        Sleep(10);
        if (!test_for_xtreem(bus, (uint8_t)free))
            addrIdx--;
    }
}

static std::vector<uint8_t> candidate_addrs() {
    std::vector<uint8_t> a;
    a.push_back(BOOT_ADDR);
    for (auto x : kRemapAddresses) a.push_back(x);
    a.push_back(0x4F); a.push_back(0x66); a.push_back(0x67);
    return a;
}

std::vector<XtreemDevice> detect_and_remap(const BusFactory& open_port, DetectTrace* trace) {
    diag::Timer t("detect_and_remap");
    std::vector<XtreemDevice> found;
    if (trace) *trace = {};
    for (int port : {0, 1}) {
        std::unique_ptr<ISmBus> bus;
        try { bus = open_port(port); }
        catch (...) {
            if (trace) trace->ports_failed++;
            continue;
        }
        if (!bus) {
            if (trace) trace->ports_failed++;
            continue;
        }
        if (trace) trace->ports_opened++;
        remap_modules(*bus);
        for (auto addr : candidate_addrs()) {
            if (test_for_xtreem(*bus, addr))
                found.push_back(XtreemDevice{addr, port});
        }
    }
    if (trace) trace->hits = (int)found.size();
    std::vector<XtreemDevice> uniq;
    for (auto& d : found) {
        bool seen = false;
        for (auto& u : uniq) if (u.address7 == d.address7) { seen = true; break; }
        if (!seen) uniq.push_back(d);
    }
    return uniq;
}

void apply_effect(ISmBus& bus, const std::vector<XtreemDevice>& on_this_port,
                  uint8_t mode, uint8_t speed, uint8_t dir) {
    for (auto& d : on_this_port) {
        register_write(bus, d.address7, REG_MODE, mode);
        register_write(bus, d.address7, REG_SPEED, speed);
        register_write(bus, d.address7, REG_DIR, dir);
        register_write(bus, d.address7, REG_APPLY, APPLY_VAL);
        Sleep(20);
    }
}
void set_direct_flag(ISmBus& bus, const std::vector<XtreemDevice>& on_this_port, uint8_t direct) {
    for (auto& d : on_this_port) {
        register_write(bus, d.address7, REG_DIRECT, direct);
        register_write(bus, d.address7, REG_APPLY, APPLY_VAL);
        Sleep(20);
    }
}
void set_direct_black(ISmBus& bus, const std::vector<XtreemDevice>& on_this_port) {
    for (auto& d : on_this_port) {
        for (int i = 0; i < 15; i++) {
            uint16_t reg = (uint16_t)(0xE100 + i * 3);
            register_write(bus, d.address7, reg, 0x00);           // R
            register_write(bus, d.address7, (uint16_t)(reg + 1), 0x00); // B
            register_write(bus, d.address7, (uint16_t)(reg + 2), 0x00); // G
        }
        register_write(bus, d.address7, REG_DIRECT, 0x01);
        register_write(bus, d.address7, REG_APPLY, APPLY_VAL);
        Sleep(20);
    }
}
void apply_static_color(ISmBus& bus, const std::vector<XtreemDevice>& on_this_port,
                        uint8_t r, uint8_t g, uint8_t b) {
    for (auto& d : on_this_port) {
        for (int i = 0; i < 15; i++) {
            uint16_t reg = (uint16_t)(0xE100 + i * 3);
            register_write(bus, d.address7, reg, r);                 // R
            register_write(bus, d.address7, (uint16_t)(reg + 1), b); // B (not G)
            register_write(bus, d.address7, (uint16_t)(reg + 2), g); // G (not B)
        }
        register_write(bus, d.address7, REG_DIRECT, 0x01);
        register_write(bus, d.address7, REG_APPLY, APPLY_VAL);
        Sleep(20);
    }
}

XtreemHardware::XtreemHardware(std::wstring exe_dir) : dir_(std::move(exe_dir)) {
    bin_path_ = (std::filesystem::path(dir_) / L"SmbusPIIX4.bin").wstring();
    last_probe = probe_pawnio(dir_);
    if (last_probe.can_use_hardware())
        api_.load_dll(dir_);
}
XtreemHardware::~XtreemHardware() { api_.unload(); }

void XtreemHardware::foreach_port(const std::vector<XtreemDevice>& devs,
    const std::function<void(ISmBus&, const std::vector<XtreemDevice>&)>& fn) {
    if (devs.empty()) throw std::runtime_error("No XTREEM modules detected.");
    std::map<int, std::vector<XtreemDevice>> g;
    for (auto& d : devs) g[d.bus_port].push_back(d);
    for (auto& kv : g) {
        auto bus = AmdSmBus::open(api_, bin_path_, kv.first);
        fn(bus, kv.second);
    }
}

void XtreemHardware::refresh_probe() {
    last_probe = probe_pawnio(dir_);
    if (last_probe.can_use_hardware() && !api_.mod)
        api_.load_dll(dir_);
}

std::vector<XtreemDevice> XtreemHardware::detect() {
    if (!last_probe.can_use_hardware() || !api_.mod)
        refresh_probe();
    if (!last_probe.can_use_hardware())
        throw std::runtime_error("PawnIO not ready (not every error is a missing driver); see classified status");
    auto dir = dir_;
    auto bin = bin_path_;
    auto* api = &api_;
    last_detect_trace = {};
    auto found = detect_and_remap([&](int port) -> std::unique_ptr<ISmBus> {
        return std::make_unique<AmdSmBus>(AmdSmBus::open(*api, bin, port));
    }, &last_detect_trace);
    return found;
}

void XtreemHardware::apply_off(const std::vector<XtreemDevice>& devs) {
    diag::Timer t("ApplyOff");
    apply_effect_all(devs, MODE_OFF, SPEED_NORMAL, DIR_FORWARD);
    foreach_port(devs, [](ISmBus& b, const auto& g) { set_direct_black(b, g); });
}
void XtreemHardware::apply_on(const std::vector<XtreemDevice>& devs) {
    diag::Timer t("ApplyOn");
    apply_effect_all(devs, MODE_TFORCE_DEFAULT, SPEED_NORMAL, DIR_FORWARD);
}
void XtreemHardware::apply_effect_all(const std::vector<XtreemDevice>& devs, uint8_t mode, uint8_t speed, uint8_t dir) {
    foreach_port(devs, [&](ISmBus& b, const auto& g) { apply_effect(b, g, mode, speed, dir); });
    foreach_port(devs, [&](ISmBus& b, const auto& g) { set_direct_flag(b, g, 0); });
}
void XtreemHardware::apply_color(const std::vector<XtreemDevice>& devs, uint8_t r, uint8_t g, uint8_t b) {
    diag::Timer t("ApplyStaticColor");
    foreach_port(devs, [&](ISmBus& bus, const auto& grp) { apply_static_color(bus, grp, r, g, b); });
}
void XtreemHardware::restore_profile(const std::vector<XtreemDevice>& found, const Profile& p) {
    auto plan = match_profile_to_found(p, found);
    for (auto& x : plan.matched) {
        std::vector<XtreemDevice> one{ x.device };
        const auto& m = x.saved;
        if (m.kind == "off") apply_off(one);
        else if (m.kind == "static") apply_color(one, m.r, m.g, m.b);
        else if (m.kind == "effect") apply_effect_all(one, m.mode, m.speed, m.direction);
        else apply_on(one);
    }
}
std::wstring XtreemHardware::dump(const std::vector<XtreemDevice>& devs) {
    std::wstringstream o;
    foreach_port(devs, [&](ISmBus& b, const auto& g) {
        for (auto& d : g) {
            auto direct = register_read(b, d.address7, REG_DIRECT);
            auto mode = register_read(b, d.address7, REG_MODE);
            wchar_t line[128];
            swprintf(line, 128, L"  0x%02X port %d Direct=0x%02X Mode=0x%02X\n", d.address7, d.bus_port, direct, mode);
            o << line;
        }
    });
    return o.str();
}
