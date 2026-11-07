#pragma once
#include "smbus.hpp"
#include "modes.hpp"
#include "persist.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>

inline constexpr uint16_t REG_DIRECT = 0xE020;
inline constexpr uint16_t REG_MODE = 0xE021;
inline constexpr uint16_t REG_SPEED = 0xE022;
inline constexpr uint16_t REG_DIR = 0xE023;
inline constexpr uint16_t REG_APPLY = 0xE02F;
inline constexpr uint16_t REG_SLOT = 0xE0F8;
inline constexpr uint16_t REG_I2CADDR = 0xE0F9;
inline constexpr uint8_t APPLY_VAL = 0x01;
inline constexpr uint8_t BOOT_ADDR = 0x77;

struct XtreemDevice {
    uint8_t address7 = 0;
    int bus_port = 0;
};

inline const uint8_t kRemapAddresses[] = {
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x78,
    0x39, 0x3A, 0x3B, 0x3C, 0x3D
};

bool device_present(ISmBus& bus, uint8_t addr);
bool test_for_xtreem(ISmBus& bus, uint8_t addr);
void remap_modules(ISmBus& bus);
void apply_effect(ISmBus& bus, const std::vector<XtreemDevice>& on_this_port,
                  uint8_t mode, uint8_t speed, uint8_t dir);
void set_direct_flag(ISmBus& bus, const std::vector<XtreemDevice>& on_this_port, uint8_t direct);
void set_direct_black(ISmBus& bus, const std::vector<XtreemDevice>& on_this_port);
void apply_static_color(ISmBus& bus, const std::vector<XtreemDevice>& on_this_port,
                        uint8_t r, uint8_t g, uint8_t b);

using BusFactory = std::function<std::unique_ptr<ISmBus>(int port)>;
std::vector<XtreemDevice> detect_and_remap(const BusFactory& open_port);

class XtreemHardware {
public:
    explicit XtreemHardware(std::wstring exe_dir);
    ~XtreemHardware();
    PawnioProbe last_probe;
    void refresh_probe();
    std::vector<XtreemDevice> detect();
    void apply_off(const std::vector<XtreemDevice>& devs);
    void apply_on(const std::vector<XtreemDevice>& devs);
    void apply_effect_all(const std::vector<XtreemDevice>& devs, uint8_t mode, uint8_t speed, uint8_t dir);
    void apply_color(const std::vector<XtreemDevice>& devs, uint8_t r, uint8_t g, uint8_t b);
    void restore_profile(const std::vector<XtreemDevice>& found, const Profile& p);
    std::wstring dump(const std::vector<XtreemDevice>& devs);
private:
    std::wstring dir_;
    PawnioApi api_;
    std::wstring bin_path_;
    void foreach_port(const std::vector<XtreemDevice>& devs,
                      const std::function<void(ISmBus&, const std::vector<XtreemDevice>&)>& fn);
};
