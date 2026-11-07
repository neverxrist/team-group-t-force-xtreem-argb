#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <wchar.h>

struct ModeInfo {
    uint8_t value;
    const char* name;
    bool supports_color;
    bool supports_speed;
    bool supports_direction;
};

inline constexpr uint8_t MODE_OFF = 0;
inline constexpr uint8_t MODE_STATIC = 1;
inline constexpr uint8_t MODE_TFORCE_DEFAULT = 25;
inline constexpr uint8_t SPEED_FASTEST = 0x00;
inline constexpr uint8_t SPEED_FAST = 0x01;
inline constexpr uint8_t SPEED_NORMAL = 0x02;
inline constexpr uint8_t SPEED_SLOW = 0x03;
inline constexpr uint8_t SPEED_SLOWEST = 0x04;
inline constexpr uint8_t DIR_FORWARD = 0x00;
inline constexpr uint8_t DIR_REVERSE = 0x01;

inline constexpr ModeInfo kModes[] = {
    {0, "Off", false, false, false},
    {1, "Static", true, false, false},
    {2, "Breathing", true, true, false},
    {3, "Flashing", true, true, false},
    {4, "Spectrum Cycle", false, true, false},
    {5, "Rainbow", false, true, true},
    {6, "Spectrum Cycle Breathing", false, true, false},
    {7, "Chase Fade", true, true, true},
    {8, "Spectrum Cycle Chase Fade", false, true, true},
    {9, "Chase", true, true, true},
    {10, "Spectrum Cycle Chase", false, true, true},
    {11, "Spectrum Cycle Wave", false, true, false},
    {12, "Chase Rainbow Pulse", false, true, false},
    {13, "Random Flicker", false, true, false},
    {14, "Stack", false, true, true},
    {15, "Pong", false, true, true},
    {16, "Fill Up", false, true, true},
    {17, "Neon Sign", false, true, false},
    {18, "Wave", false, true, false},
    {19, "Double Wave", false, true, false},
    {20, "Mixer", false, true, false},
    {21, "Spectrum Cycle 2", false, true, false},
    {22, "Fire Breathing", false, true, false},
    {23, "Spectrum Cycle 3", false, true, false},
    {24, "Slither", false, true, false},
    {25, "T-Force Xtreem Default", false, true, false},
};
inline constexpr int kModeCount = (int)(sizeof(kModes) / sizeof(kModes[0]));

// Off (0x00) is lights-off (mode OFF + direct black), not Static (0x01).
enum class EffectApplyPath { LightsOff, TforceDefault, Effect };
inline EffectApplyPath effect_apply_path(uint8_t mode) {
    if (mode == MODE_OFF) return EffectApplyPath::LightsOff;
    if (mode == MODE_TFORCE_DEFAULT) return EffectApplyPath::TforceDefault;
    return EffectApplyPath::Effect;
}
inline void format_effect_label(wchar_t* out, size_t n, const ModeInfo& m) {
    if (m.value == MODE_OFF)
        swprintf(out, n, L"Off (LEDs black) 0x00 — not Static");
    else if (m.value == MODE_STATIC)
        swprintf(out, n, L"Static (mode 0x01) — not Off");
    else if (m.value == MODE_TFORCE_DEFAULT)
        swprintf(out, n, L"T-Force Default 0x19");
    else
        swprintf(out, n, L"%S (0x%02X)", m.name, m.value);
}

inline uint8_t speed_from_slider(int v) {
    switch (v) {
    case 0: return SPEED_FASTEST;
    case 1: return SPEED_FAST;
    case 2: return SPEED_NORMAL;
    case 3: return SPEED_SLOW;
    case 4: return SPEED_SLOWEST;
    default: return SPEED_NORMAL;
    }
}
inline int slider_from_speed(uint8_t s) {
    switch (s) {
    case SPEED_FASTEST: return 0;
    case SPEED_FAST: return 1;
    case SPEED_NORMAL: return 2;
    case SPEED_SLOW: return 3;
    case SPEED_SLOWEST: return 4;
    default: return 2;
    }
}
inline const char* speed_label(int v) {
    switch (v) {
    case 0: return "Fastest";
    case 1: return "Fast";
    case 2: return "Normal";
    case 3: return "Slow";
    case 4: return "Slowest";
    default: return "Normal";
    }
}
