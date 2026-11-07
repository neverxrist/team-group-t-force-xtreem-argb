#include "autostart.hpp"
#include "diag.hpp"
#include <windows.h>
#include <stdexcept>
#include <sstream>

AutostartInstallState autostart_combine(TaskQueryResult current, TaskQueryResult legacy) {
    if (current == TaskQueryResult::Present || legacy == TaskQueryResult::Present)
        return AutostartInstallState::Installed;
    if (current == TaskQueryResult::Absent && legacy == TaskQueryResult::Absent)
        return AutostartInstallState::NotInstalled;
    return AutostartInstallState::Unknown;
}
bool autostart_can_cache(AutostartInstallState s) {
    return s == AutostartInstallState::Installed || s == AutostartInstallState::NotInstalled;
}

static int run_hidden(const std::wstring& cmd, DWORD timeout_ms, DWORD* exit_out) {
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    std::wstring mutable_cmd = cmd;
    if (!CreateProcessW(nullptr, mutable_cmd.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        return -1;
    }
    DWORD w = WaitForSingleObject(pi.hProcess, timeout_ms);
    DWORD code = 1;
    if (w == WAIT_OBJECT_0) {
        GetExitCodeProcess(pi.hProcess, &code);
        if (exit_out) *exit_out = code;
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 0;
    }
    TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 1; // timeout
}

TaskQueryResult Autostart::query_task(const wchar_t* name) {
    diag::Timer t("QueryTask");
    wchar_t cmd[512];
    swprintf(cmd, 512, L"schtasks.exe /Query /TN \"%s\" /FO LIST", name);
    DWORD exit_code = 1;
    int r = run_hidden(cmd, 1000, &exit_code);
    if (r < 0) return TaskQueryResult::Unknown;
    if (r > 0) return TaskQueryResult::Unknown; // timeout cap, not a measured delay
    return exit_code == 0 ? TaskQueryResult::Present : TaskQueryResult::Absent;
}

AutostartInstallState Autostart::query() {
    if (have_cache_) return cached_;
    auto st = autostart_combine(query_task(kTask), query_task(kLegacy));
    if (autostart_can_cache(st)) {
        cached_ = st;
        have_cache_ = true;
    }
    return st;
}
void Autostart::invalidate() { have_cache_ = false; }

void Autostart::delete_task(const wchar_t* name) {
    wchar_t cmd[512];
    swprintf(cmd, 512, L"schtasks.exe /Delete /F /TN \"%s\"", name);
    run_hidden(cmd, 3000, nullptr);
}

void Autostart::install(const std::wstring& exe) {
    diag::Timer t("Autostart.install");
    if (query_task(kLegacy) == TaskQueryResult::Present)
        delete_task(kLegacy);
    std::wstring cmd = L"schtasks.exe /Create /F /TN \"";
    cmd += kTask;
    cmd += L"\" /TR \"\\\"";
    cmd += exe;
    cmd += L"\\\" off\" /SC ONLOGON /RL HIGHEST /DELAY 0000:00";
    DWORD exit_code = 1;
    if (run_hidden(cmd, 15000, &exit_code) != 0 || exit_code != 0)
        throw std::runtime_error("Task creation failed");
    Sleep(500);
    invalidate();
    if (query_task(kTask) != TaskQueryResult::Present)
        throw std::runtime_error("Task created but verification did not confirm it");
    cached_ = AutostartInstallState::Installed;
    have_cache_ = true;
}

void Autostart::uninstall() {
    diag::Timer t("Autostart.uninstall");
    invalidate();
    auto cur = query_task(kTask);
    auto leg = query_task(kLegacy);
    if (cur == TaskQueryResult::Present) delete_task(kTask);
    if (leg == TaskQueryResult::Present) delete_task(kLegacy);
    Sleep(500);
    invalidate();
    auto after = autostart_combine(query_task(kTask), query_task(kLegacy));
    if (after == AutostartInstallState::NotInstalled) {
        cached_ = after;
        have_cache_ = true;
        return;
    }
    if (after == AutostartInstallState::Unknown)
        throw std::runtime_error("Task deletion ran, but verification timed out");
    throw std::runtime_error("Task still exists after deletion");
}
