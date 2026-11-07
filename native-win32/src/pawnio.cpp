#include "pawnio.hpp"
#include "diag.hpp"
#include <windows.h>
#include <winsvc.h>
#include <filesystem>

static bool win32_hr(long hr, unsigned long code) {
    unsigned long u = (unsigned long)hr;
    return u == code || u == (0x80070000ul | (code & 0xFFFFul));
}

PawnioStatus classify_pawnio(bool dll_ok, bool bin_ok, bool service_query_ok,
                             unsigned long service_winerr, bool service_present,
                             unsigned long service_state, long version_hr, long open_hr) {
    if (!dll_ok) return PawnioStatus::BundledDllMissing;
    if (!bin_ok) return PawnioStatus::BundledBinMissing;

    // A successful version+open means the kernel driver is actually usable.
    // Do not override that with a noisy SCM query (and never treat DLL/bin as the driver).
    if (version_hr >= 0 && open_hr >= 0)
        return PawnioStatus::Ready;

    if (win32_hr(open_hr, ERROR_ACCESS_DENIED) || win32_hr(version_hr, ERROR_ACCESS_DENIED) ||
        (service_query_ok && !service_present && service_winerr == ERROR_ACCESS_DENIED))
        return PawnioStatus::PermissionDenied;

    if (win32_hr(open_hr, ERROR_SERVICE_DOES_NOT_EXIST) ||
        win32_hr(version_hr, ERROR_SERVICE_DOES_NOT_EXIST) ||
        (service_query_ok && !service_present && service_winerr == ERROR_SERVICE_DOES_NOT_EXIST))
        return PawnioStatus::DriverMissing;

    if (win32_hr(open_hr, ERROR_SERVICE_NOT_ACTIVE) ||
        win32_hr(version_hr, ERROR_SERVICE_NOT_ACTIVE) ||
        (service_query_ok && service_present &&
         (service_state == SERVICE_STOPPED || service_state == SERVICE_STOP_PENDING ||
          service_state == SERVICE_PAUSED || service_state == SERVICE_PAUSE_PENDING)))
        return PawnioStatus::DriverStopped;

    return PawnioStatus::Other;
}

std::wstring pawnio_status_text(PawnioStatus s, long version_hr, long open_hr, uint32_t version) {
    wchar_t extra[96]{};
    swprintf(extra, 96, L" (version HRESULT=0x%08X open HRESULT=0x%08X)",
             (unsigned)version_hr, (unsigned)open_hr);
    switch (s) {
    case PawnioStatus::Ready: {
        wchar_t b[160];
        swprintf(b, 160, L"PawnIO kernel driver ready (v%u.%u.%u). Bundled DLL/bin are not the driver.",
                 (version >> 16) & 0xFF, (version >> 8) & 0xFF, version & 0xFF);
        return b;
    }
    case PawnioStatus::BundledDllMissing:
        return L"PawnIOLib.dll is missing next to the exe (user-mode library, not the kernel driver).";
    case PawnioStatus::BundledBinMissing:
        return L"SmbusPIIX4.bin is missing next to the exe (PawnIO module blob, not the kernel driver).";
    case PawnioStatus::DriverMissing:
        return std::wstring(L"PawnIO kernel driver is not installed.") + extra;
    case PawnioStatus::DriverStopped:
        return std::wstring(L"PawnIO kernel driver is installed but unavailable (service not running).") + extra;
    case PawnioStatus::PermissionDenied:
        return std::wstring(L"PawnIO access denied — not classified as a missing driver. Run as Administrator.") + extra;
    default:
        return std::wstring(L"PawnIO initialization failed (not classified as a missing driver).") + extra;
    }
}

std::wstring pawnio_next_step(PawnioStatus s) {
    switch (s) {
    case PawnioStatus::Ready:
        return L"Driver check passed. Hardware access may proceed.";
    case PawnioStatus::BundledDllMissing:
    case PawnioStatus::BundledBinMissing:
        return L"Restore PawnIOLib.dll and SmbusPIIX4.bin from the portable ZIP, then Retry. Those files are not the kernel driver.";
    case PawnioStatus::DriverMissing:
        return L"Click “Open official PawnIO page”, install the kernel driver yourself from https://pawnio.eu, then Retry. This app does not download or run the installer.";
    case PawnioStatus::DriverStopped:
        return L"Start the Windows service named PawnIO, or reinstall from https://pawnio.eu (page opened by you), then Retry.";
    case PawnioStatus::PermissionDenied:
        return L"Right-click XtreemRgbToggle.exe → Run as administrator, then Retry. Do not treat this as a missing driver.";
    default:
        return L"Retry after addressing the HRESULT. This is not labeled as a missing driver. The official page remains https://pawnio.eu if you still need to install.";
    }
}

std::wstring pawnio_dialog_text(const PawnioProbe& p) {
    return p.message + L"\n\n" + pawnio_next_step(p.status) +
           L"\n\nBundled PawnIOLib.dll / SmbusPIIX4.bin never install the kernel driver.";
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
        ok = true;
        present = false;
        return;
    }
    SC_HANDLE svc = OpenServiceW(scm, L"PawnIO", SERVICE_QUERY_STATUS);
    if (!svc) {
        winerr = GetLastError();
        present = (winerr != ERROR_SERVICE_DOES_NOT_EXIST);
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
    p.service_query_ok = sq_ok;
    p.service_winerr = swerr;
    p.service_present = present;
    p.service_state = sstate;

    long vhr = 0, ohr = 0;
    uint32_t ver = 0;
    if (dll_ok) {
        PawnioApi api;
        if (api.load_dll(exe_dir) && api.version && api.open) {
            vhr = api.version(&ver);
            p.version = ver;
            void* h = nullptr;
            ohr = api.open(&h);
            if (pawnio_ok((int)ohr) && h && api.close)
                api.close(h);
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
