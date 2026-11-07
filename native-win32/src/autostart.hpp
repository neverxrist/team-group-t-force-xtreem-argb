#pragma once
#include <string>

enum class TaskQueryResult { Present, Absent, Unknown };
enum class AutostartInstallState { Unknown, Installed, NotInstalled };

AutostartInstallState autostart_combine(TaskQueryResult current, TaskQueryResult legacy);
bool autostart_can_cache(AutostartInstallState s);

struct Autostart {
    static constexpr const wchar_t* kTask = L"XtreemRgbToggle-Off";
    static constexpr const wchar_t* kLegacy = L"XtreemRgbToggle_AutoOff";
    AutostartInstallState query();
    void invalidate();
    void install(const std::wstring& exe);
    void uninstall();
private:
    AutostartInstallState cached_ = AutostartInstallState::Unknown;
    bool have_cache_ = false;
    TaskQueryResult query_task(const wchar_t* name);
    void delete_task(const wchar_t* name);
};
