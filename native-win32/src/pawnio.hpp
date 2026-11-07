#pragma once
#include <cstdint>
#include <string>
#include <windows.h>

// Official vendor site, re-verified 2026-11-07:
//   https://pawnio.eu  (HTTP 200)
// That page's "Download Now" button is a user-facing installer link:
//   https://github.com/namazso/PawnIO.Setup/releases/latest/download/PawnIO_setup.exe
// This app never downloads or runs that installer. It only ShellExecute's the
// official page when the user clicks the button / SysLink.
inline constexpr const wchar_t* kPawnioOfficialPage = L"https://pawnio.eu";

enum class PawnioStatus {
    Ready,
    BundledDllMissing,   // user-mode PawnIOLib.dll next to exe
    BundledBinMissing,   // user-mode SmbusPIIX4.bin next to exe
    DriverMissing,       // kernel driver / PawnIO service not installed
    DriverStopped,       // installed but unavailable (not running)
    PermissionDenied,    // admin / ACL; not a missing-driver diagnosis
    Other                // HRESULT shown; not labeled missing
};

struct PawnioProbe {
    PawnioStatus status = PawnioStatus::Other;
    uint32_t version = 0;
    long version_hr = 0;
    long open_hr = 0;
    bool service_query_ok = false;
    unsigned long service_winerr = 0;
    bool service_present = false;
    unsigned long service_state = 0;
    std::wstring message;
    bool can_use_hardware() const { return status == PawnioStatus::Ready; }
    bool offer_install_page() const {
        return status == PawnioStatus::DriverMissing || status == PawnioStatus::DriverStopped;
    }
};

PawnioStatus classify_pawnio(bool dll_ok, bool bin_ok, bool service_query_ok,
                             unsigned long service_winerr, bool service_present,
                             unsigned long service_state, long version_hr, long open_hr);

std::wstring pawnio_status_text(PawnioStatus s, long version_hr, long open_hr, uint32_t version);
std::wstring pawnio_next_step(PawnioStatus s);
std::wstring pawnio_dialog_text(const PawnioProbe& p);
PawnioProbe probe_pawnio(const std::wstring& exe_dir);

struct PawnioApi {
    HMODULE mod = nullptr;
    int (__cdecl *version)(unsigned int* v) = nullptr;
    int (__cdecl *open)(void** handle) = nullptr;
    int (__cdecl *close)(void* handle) = nullptr;
    int (__cdecl *load)(void* handle, const unsigned char* blob, size_t size) = nullptr;
    int (__cdecl *execute)(void* handle, const char* fn, const unsigned long long* in, size_t nic,
                           unsigned long long* out, size_t noc, size_t* nret) = nullptr;
    bool load_dll(const std::wstring& dir);
    void unload();
};

inline bool pawnio_ok(int hr) { return hr >= 0; }
inline const char* pawnio_mutex_name() { return "Access_SMBUS.HTP.Method"; }
