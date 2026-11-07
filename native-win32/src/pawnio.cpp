#include "pawnio.hpp"
#include "diag.hpp"
#include <windows.h>
#include <winsvc.h>
#include <filesystem>

PawnioStatus classify_pawnio(bool dll_ok, bool bin_ok, bool service_query_ok,
                             unsigned long service_winerr, bool service_present,
                             unsigned long service_state, long version_hr, long open_hr) {
    if (!dll_ok) return PawnioStatus::BundledDllMissing;
    if (!bin_ok) return PawnioStatus::BundledBinMissing;
    if (service_query_ok && !service_present) {
        if (service_winerr == ERROR_ACCESS_DENIED)
            return PawnioStatus::PermissionDenied;
        if (service_winerr == ERROR_SERVICE_DOES_NOT_EXIST)
            return PawnioStatus::DriverMissing;
    }
    if (service_query_ok && service_present &&
        service_state != SERVICE_RUNNING && service_state != SERVICE_START_PENDING)
        return PawnioStatus::DriverStopped;
    if (open_hr < 0) {
        auto u = (unsigned long)open_hr;
        if (u == 0x80070005ul || u == 5)
            return PawnioStatus::PermissionDenied;
        if (u == 0x80070424ul || u == ERROR_SERVICE_DOES_NOT_EXIST)
            return PawnioStatus::DriverMissing;
        if (u == 0x80070426ul || u == ERROR_SERVICE_NOT_ACTIVE)
            return PawnioStatus::DriverStopped;
        return PawnioStatus::Other;
    }
    if (version_hr < 0) return PawnioStatus::Other;
    return PawnioStatus::Ready;
}

std::wstring pawnio_status_text(PawnioStatus s, long version_hr, long open_hr, uint32_t version) {
    wchar_t extra[80]{};
    swprintf(extra, 80, L" (version HRESULT=0x%08X open HRESULT=0x%08X)",
             (unsigned)version_hr, (unsigned)open_hr);
    switch (s) {
    case PawnioStatus::Ready: {
        wchar_t b[128];
        swprintf(b, 128, L"PawnIO driver ready (v%u.%u.%u). Bundled DLL/bin are not the kernel driver.",
                 (version >> 16) & 0xFF, (version >> 8) & 0xFF, version & 0xFF);
        return b;
    }
    case PawnioStatus::BundledDllMissing:
        return L"PawnIOLib.dll is missing next to the exe (user-mode library, not the kernel driver).";
    case PawnioStatus::BundledBinMissing:
        return L"SmbusPIIX4.bin is missing next to the exe (PawnIO module blob, not the kernel driver).";
    case PawnioStatus::DriverMissing:
        return std::wstring(L"PawnIO kernel driver is not installed. Install it from https://pawnio.eu (this app will not download it).") + extra;
    case PawnioStatus::DriverStopped:
        return std::wstring(L"PawnIO kernel driver is installed but not running. Start the PawnIO service, or reinstall from https://pawnio.eu") + extra;
    case PawnioStatus::PermissionDenied:
        return std::wstring(L"PawnIO access denied. Run this program as Administrator.") + extra;
    default:
        return std::wstring(L"PawnIO initialization failed (not classified as missing driver).") + extra;
    }
}

bool PawnioApi::load_dll(const std::wstring& dir) {
    unload();
    SetDllDirectoryW(dir.c_str());
    auto path = (std::filesystem::path(dir) / L"PawnIOLib.dll").wstring();
    mod = LoadLibraryW(path.c_str());
    if (!mod) return false;
    version = (decltype(version))GetProcAddress(mod, "pawnio_version");
    open = (decltype(open))GetProcAddress(mod, "pawnio_open");
    close = (decltype(close))GetProcAddress(mod, "pawnio_close");
    load = (decltype(load))GetProcAddress(mod, "pawnio_load");
    execute = (decltype(execute))GetProcAddress(mod, "pawnio_execute");
    return version && open && close && load && execute;
}
void PawnioApi::unload() {
    if (mod) { FreeLibrary(mod); mod = nullptr; }
    version = nullptr; open = nullptr; close = nullptr; load = nullptr; execute = nullptr;
}

static void query_service(bool& ok, unsigned long& winerr, bool& present, unsigned long& state) {
    ok = false; winerr = 0; present = false; state = 0;
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) {
        winerr = GetLastError();
        ok = true; // we did query; ACCESS_DENIED vs other
        present = false;
        return;
    }
    SC_HANDLE svc = OpenServiceW(scm, L"PawnIO", SERVICE_QUERY_STATUS);
    if (!svc) {
        winerr = GetLastError();
        present = (winerr != ERROR_SERVICE_DOES_NOT_EXIST);
        if (winerr == ERROR_SERVICE_DOES_NOT_EXIST) present = false;
        ok = true;
        CloseServiceHandle(scm);
        return;
    }
    present = true;
    SERVICE_STATUS_PROCESS ssp{};
    DWORD needed = 0;
    if (QueryServiceStatusEx(svc, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(ssp), &needed)) {
        state = ssp.dwCurrentState;
        ok = true;
    } else {
        winerr = GetLastError();
        ok = true;
    }
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
}

PawnioProbe probe_pawnio(const std::wstring& exe_dir) {
    diag::Timer t("probe_pawnio");
    PawnioProbe p;
    auto dll = std::filesystem::path(exe_dir) / L"PawnIOLib.dll";
    auto bin = std::filesystem::path(exe_dir) / L"SmbusPIIX4.bin";
    bool dll_ok = std::filesystem::exists(dll);
    bool bin_ok = std::filesystem::exists(bin);
    bool sq_ok = false, present = false;
    unsigned long swerr = 0, sstate = 0;
    query_service(sq_ok, swerr, present, sstate);
    p.service_query_ok = true;
    p.service_winerr = swerr;
    p.service_present = present;
    p.service_state = sstate;

    long vhr = 0, ohr = 0;
    uint32_t ver = 0;
    if (dll_ok) {
        PawnioApi api;
        if (api.load_dll(exe_dir) && api.version) {
            vhr = api.version(&ver);
            p.version = ver;
            if (pawnio_ok((int)vhr) && api.open) {
                void* h = nullptr;
                ohr = api.open(&h);
                if (pawnio_ok((int)ohr) && h && api.close)
                    api.close(h);
            }
        } else {
            dll_ok = false;
        }
        api.unload();
    }
    p.version_hr = vhr;
    p.open_hr = ohr;
    p.status = classify_pawnio(dll_ok, bin_ok, sq_ok, swerr, present, sstate, vhr, ohr);
    p.message = pawnio_status_text(p.status, vhr, ohr, ver);
    return p;
}
