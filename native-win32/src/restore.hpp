#pragma once
#include "persist.hpp"
#include "xtreem.hpp"
#include <vector>
#include <windows.h>

// Process exit codes used by this exe. schtasks Last Run Result shows these as DWORD.
// 3 is this program's "detect returned no modules after a Ready probe", not Win32
// ERROR_PATH_NOT_FOUND (also numeric 3) and not HRESULT 0x80070003.
inline constexpr int kExitOk = 0;
inline constexpr int kExitError = 1;
inline constexpr int kExitUsage = 2;
inline constexpr int kExitNoModules = 3;
inline constexpr int kExitDriver = 4;
inline constexpr int kExitCorruptProfile = 5;
inline constexpr int kExitNoProfileMatch = 6;

// Between detect attempts only (logon SMBus/PawnIO often not ready on first try).
// Does not change per-register remap 10ms / apply 20ms sleeps.
inline constexpr int kRestoreDetectAttempts = 8;
inline constexpr int kRestoreDetectDelayMs = 250;

struct RestoreMatch {
    XtreemDevice device;
    ModuleProfile saved;
};

struct RestoreMatchResult {
    std::vector<RestoreMatch> matched;
    std::vector<uint8_t> stale_profile_addrs;
    std::vector<uint8_t> unmatched_found_addrs;
};

// Match saved rows to currently detected modules by 7-bit address only.
// Never map a stale address onto a different live module.
inline RestoreMatchResult match_profile_to_found(const Profile& p, const std::vector<XtreemDevice>& found) {
    RestoreMatchResult r;
    for (const auto& m : p.modules) {
        const XtreemDevice* hit = nullptr;
        for (const auto& d : found) {
            if (d.address7 == m.address) { hit = &d; break; }
        }
        if (hit) r.matched.push_back(RestoreMatch{ *hit, m });
        else r.stale_profile_addrs.push_back(m.address);
    }
    for (const auto& d : found) {
        bool used = false;
        for (const auto& x : r.matched) {
            if (x.device.address7 == d.address7) { used = true; break; }
        }
        if (!used) r.unmatched_found_addrs.push_back(d.address7);
    }
    return r;
}

inline int restore_result_exit(RestoreAction act, bool driver_ok, bool detect_threw,
                               const std::vector<XtreemDevice>& found, const RestoreMatchResult& m) {
    if (act == RestoreAction::SkipCorrupt) return kExitCorruptProfile;
    if (act != RestoreAction::Apply) return kExitOk;
    if (detect_threw && found.empty()) return kExitError;
    if (!driver_ok) return kExitDriver;
    if (found.empty()) return kExitNoModules;
    if (m.matched.empty()) return kExitNoProfileMatch;
    return kExitOk;
}

template <typename DetectFn>
std::vector<XtreemDevice> detect_with_retry(DetectFn&& detect, int attempts, int delay_ms, int* attempts_used) {
    std::vector<XtreemDevice> d;
    if (attempts < 1) attempts = 1;
    for (int i = 0; i < attempts; i++) {
        d = detect();
        if (attempts_used) *attempts_used = i + 1;
        if (!d.empty()) return d;
        if (i + 1 < attempts && delay_ms > 0)
            Sleep((DWORD)delay_ms);
    }
    return d;
}
