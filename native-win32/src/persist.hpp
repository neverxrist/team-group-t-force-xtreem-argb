#pragma once
#include <cstdint>
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
