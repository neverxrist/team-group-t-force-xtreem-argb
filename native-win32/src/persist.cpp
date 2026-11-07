#include "persist.hpp"
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cctype>

std::wstring appdata_dir() {
    std::error_code ec;
    if (const wchar_t* e = _wgetenv(L"XTREEM_STATE_DIR")) {
        if (e[0]) {
            std::filesystem::create_directories(e, ec);
            return e;
        }
    }
    wchar_t buf[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, buf)))
        return L"";
    std::filesystem::path p = buf;
    p /= L"XtreemRgbToggle";
    std::filesystem::create_directories(p, ec);
    return p.wstring();
}

std::wstring state_path(const wchar_t* override_path) {
    if (override_path && override_path[0]) return override_path;
    return (std::filesystem::path(appdata_dir()) / L"state.txt").wstring();
}
std::wstring profile_path(const wchar_t* override_path) {
    if (override_path && override_path[0]) return override_path;
    return (std::filesystem::path(appdata_dir()) / L"profile.json").wstring();
}

static std::string read_all(const std::wstring& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
static bool write_all(const std::wstring& path, const std::string& s) {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << s;
    return (bool)f;
}

LightState load_state(const wchar_t* path) {
    auto t = read_all(state_path(path));
    while (!t.empty() && (t.back() == '\n' || t.back() == '\r' || t.back() == ' ')) t.pop_back();
    for (auto& c : t) c = (char)tolower((unsigned char)c);
    if (t == "off") return LightState::Off;
    if (t == "on") return LightState::On;
    return LightState::Unknown;
}
void save_state(LightState s, const wchar_t* path) {
    write_all(state_path(path), s == LightState::Off ? "off" : "on");
}

static int json_int_field(const std::string& obj, const char* key, int def) {
    std::string k = std::string("\"") + key + "\"";
    auto p = obj.find(k);
    if (p == std::string::npos) return def;
    p = obj.find(':', p);
    if (p == std::string::npos) return def;
    p++;
    while (p < obj.size() && isspace((unsigned char)obj[p])) p++;
    if (p < obj.size() && obj[p] == '"') {
        // hex or decimal in string
        auto e = obj.find('"', p + 1);
        if (e == std::string::npos) return def;
        auto s = obj.substr(p + 1, e - p - 1);
        return (int)strtol(s.c_str(), nullptr, 0);
    }
    return (int)strtol(obj.c_str() + p, nullptr, 0);
}
static std::string json_str_field(const std::string& obj, const char* key) {
    std::string k = std::string("\"") + key + "\"";
    auto p = obj.find(k);
    if (p == std::string::npos) return {};
    p = obj.find(':', p);
    if (p == std::string::npos) return {};
    p = obj.find('"', p);
    if (p == std::string::npos) return {};
    auto e = obj.find('"', p + 1);
    if (e == std::string::npos) return {};
    return obj.substr(p + 1, e - p - 1);
}

std::string profile_to_json(const Profile& p) {
    std::ostringstream o;
    o << "{\n  \"version\": " << p.version << ",\n  \"modules\": [\n";
    for (size_t i = 0; i < p.modules.size(); i++) {
        const auto& m = p.modules[i];
        o << "    {\"address\":" << (int)m.address
          << ",\"port\":" << m.port
          << ",\"kind\":\"" << m.kind << "\""
          << ",\"mode\":" << (int)m.mode
          << ",\"speed\":" << (int)m.speed
          << ",\"direction\":" << (int)m.direction
          << ",\"r\":" << (int)m.r
          << ",\"g\":" << (int)m.g
          << ",\"b\":" << (int)m.b << "}";
        if (i + 1 < p.modules.size()) o << ",";
        o << "\n";
    }
    o << "  ]\n}\n";
    return o.str();
}

bool profile_from_json(const std::string& json, Profile& p) {
    p = {};
    p.version = json_int_field(json, "version", 1);
    auto arr = json.find("\"modules\"");
    if (arr == std::string::npos) return false;
    auto lb = json.find('[', arr);
    auto rb = json.rfind(']');
    if (lb == std::string::npos || rb == std::string::npos || rb <= lb) return false;
    size_t i = lb + 1;
    while (i < rb) {
        auto os = json.find('{', i);
        if (os == std::string::npos || os >= rb) break;
        auto oe = json.find('}', os);
        if (oe == std::string::npos || oe >= rb) break;
        auto obj = json.substr(os, oe - os + 1);
        ModuleProfile m;
        m.address = (uint8_t)json_int_field(obj, "address", 0);
        m.port = json_int_field(obj, "port", 0);
        m.kind = json_str_field(obj, "kind");
        if (m.kind.empty()) m.kind = "on";
        m.mode = (uint8_t)json_int_field(obj, "mode", 0);
        m.speed = (uint8_t)json_int_field(obj, "speed", 2);
        m.direction = (uint8_t)json_int_field(obj, "direction", 0);
        m.r = (uint8_t)json_int_field(obj, "r", 0);
        m.g = (uint8_t)json_int_field(obj, "g", 0);
        m.b = (uint8_t)json_int_field(obj, "b", 0);
        p.modules.push_back(m);
        i = oe + 1;
    }
    return true;
}

bool save_profile(const Profile& p, const wchar_t* path) {
    return write_all(profile_path(path), profile_to_json(p));
}
bool load_profile(Profile& p, const wchar_t* path) {
    auto s = read_all(profile_path(path));
    if (s.empty()) return false;
    return profile_from_json(s, p);
}
