#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

enum class LightState { Unknown = 0, Off = 1, On = 2 };

struct ModuleProfile {
    uint8_t address = 0;
    int port = 0;
    std::string kind; // off | on | static | effect
    uint8_t mode = 0;
    uint8_t speed = 2;
    uint8_t direction = 0;
    uint8_t r = 0, g = 0, b = 0;
};

struct Profile {
    int version = 1;
    std::vector<ModuleProfile> modules;
};

std::wstring appdata_dir();
std::wstring state_path(const wchar_t* override_path = nullptr);
std::wstring profile_path(const wchar_t* override_path = nullptr);

LightState load_state(const wchar_t* path = nullptr);
void save_state(LightState s, const wchar_t* path = nullptr);

bool save_profile(const Profile& p, const wchar_t* path = nullptr);
bool load_profile(Profile& p, const wchar_t* path = nullptr);
std::string profile_to_json(const Profile& p);
bool profile_from_json(const std::string& json, Profile& p);

enum class ProfileStatus { Missing, Corrupt, Ok };
ProfileStatus load_profile_status(Profile& p, const wchar_t* path = nullptr);
// Merge one module's explicitly applied settings. Does not write on preview.
void persist_applied_one(uint8_t address, int port, const char* kind,
                         uint8_t mode, uint8_t speed, uint8_t dir, uint8_t r, uint8_t g, uint8_t b);

enum class RestoreAction { SkipMissing, SkipEmpty, SkipCorrupt, Apply };
inline RestoreAction profile_restore_action(ProfileStatus st, size_t nmods) {
    if (st == ProfileStatus::Missing) return RestoreAction::SkipMissing;
    if (st == ProfileStatus::Corrupt) return RestoreAction::SkipCorrupt;
    if (nmods == 0) return RestoreAction::SkipEmpty;
    return RestoreAction::Apply;
}
std::wstring restore_log_path();
// Always-on restore session log (not gated on XTREEM_DIAG). No paths/secrets.
void restore_event(const char* msg);

inline const wchar_t* restore_skip_message(RestoreAction a) {
    switch (a) {
    case RestoreAction::SkipCorrupt: return L"Saved profile unreadable; nothing restored.";
    case RestoreAction::SkipMissing:
    case RestoreAction::SkipEmpty: return L"No saved profile; nothing restored.";
    default: return L"";
    }
}
