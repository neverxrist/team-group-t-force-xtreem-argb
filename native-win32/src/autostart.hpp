#pragma once
#include <string>

enum class TaskQueryResult { Present, Absent, Unknown };
enum class AutostartInstallState { Unknown, Installed, NotInstalled };

AutostartInstallState autostart_combine(TaskQueryResult current, TaskQueryResult legacy);
bool autostart_can_cache(AutostartInstallState s);
// Tests and WM_ENDSESSION must not spawn schtasks.exe / cmd.exe.
void autostart_set_allow_child_processes(bool allow);
bool autostart_allow_child_processes();

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
