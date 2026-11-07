#include "diag.hpp"
#include <windows.h>
#include <shlobj.h>
#include <chrono>
#include <mutex>
#include <fstream>
#include <filesystem>

namespace diag {
namespace {
bool g_on = false;
std::mutex g_mu;
auto g_start = std::chrono::steady_clock::now();

std::wstring log_path() {
    wchar_t buf[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, buf)))
        return L"";
    std::filesystem::path p = buf;
    p /= L"XtreemRgbToggle";
    std::error_code ec;
    std::filesystem::create_directories(p, ec);
    p /= L"diagnostic.log";
    return p.wstring();
}
}

void enable(bool on) { g_on = on; }
bool enabled() { return g_on; }

void log(const char* msg) {
    if (!g_on) return;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - g_start).count();
    SYSTEMTIME st{};
    GetLocalTime(&st);
    char line[1024];
    snprintf(line, sizeof(line), "[%04u-%02u-%02u %02u:%02u:%02u.%03u] %s (app+%lldms)\n",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
             msg, (long long)ms);
    std::lock_guard<std::mutex> lock(g_mu);
    OutputDebugStringA(line);
    auto path = log_path();
    if (path.empty()) return;
    std::ofstream f(path, std::ios::app);
    if (f) f << line;
}

Timer::Timer(const char* n) : name(n) {
    t0 = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - g_start).count();
    if (g_on) {
        char b[256];
        snprintf(b, sizeof(b), "START %s", name);
        log(b);
    }
}
Timer::~Timer() {
    if (!g_on) return;
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - g_start).count();
    char b[256];
    snprintf(b, sizeof(b), "DONE %s took %lldms", name, (long long)(now - t0));
    log(b);
}
}
