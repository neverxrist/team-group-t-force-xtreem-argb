#include "ui.hpp"
#include "hw_worker.hpp"
#include "autostart.hpp"
#include "persist.hpp"
#include "modes.hpp"
#include "diag.hpp"
#include <stdexcept>
#include <algorithm>
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <chrono>

enum {
    IDC_STATUS = 1001, IDC_PAWNIO, IDC_OPENPAWNIO, IDC_RETRY, IDC_MODULES, IDC_RESCAN,
    IDC_ALL = 1100, IDC_MOD0 = 1101, IDC_OFF = 1200, IDC_ON, IDC_EFFECT, IDC_SPEED, IDC_SPEEDL,
    IDC_REV, IDC_APPLYEFF, IDC_LIVE, IDC_PICK, IDC_R, IDC_G, IDC_B, IDC_VR, IDC_VG, IDC_VB,
    IDC_SWATCH, IDC_APPLYCOL, IDC_INSTAS, IDC_REMAS
};
static constexpr UINT WM_HW = WM_APP + 1;
static constexpr UINT WM_TRAY = WM_APP + 2;
static constexpr int PREVIEW_MS = 100;

struct UiState {
    HWND wnd{};
    std::wstring dir;
    HwWorker* worker{};
    Autostart autostart;
    std::vector<XtreemDevice> devices;
    PawnioProbe probe;
    bool busy = false;
    bool live = true;
    uint8_t pending_r = 0, pending_g = 0, pending_b = 0;
    bool pending_color = false;
    std::chrono::steady_clock::time_point last_preview{};
    HWND hStatus, hPawnio, hOpen, hRetry, hMods, hRescan, hAll, hOff, hOn;
    HWND hEff, hSpeed, hSpeedL, hRev, hApplyEff, hLive, hPick, hR, hG, hB, hVR, hVG, hVB, hSw, hApplyCol;
    HWND hInst, hRem;
    HWND hModRb[8]{};
    int nmod = 0;
    HFONT font{};
    bool smoke = false;
    NOTIFYICONDATAW nid{};
    bool tray = false;
};

static UiState* g;

std::wstring exe_directory() {
    wchar_t buf[MAX_PATH]{};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p = buf;
    auto sl = p.find_last_of(L"\\/");
    return sl == std::wstring::npos ? L"." : p.substr(0, sl);
}
void open_pawnio_site() {
    ShellExecuteW(nullptr, L"open", kPawnioOfficialPage, nullptr, nullptr, SW_SHOWNORMAL);
}

static HWND mk(HWND p, const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, p, (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr);
    if (g && g->font) SendMessageW(c, WM_SETFONT, (WPARAM)g->font, TRUE);
    return c;
}

static std::vector<XtreemDevice> selected() {
    if (!g) return {};
    if (SendMessageW(g->hAll, BM_GETCHECK, 0, 0) == BST_CHECKED) return g->devices;
    for (int i = 0; i < g->nmod && i < 8; i++) {
        if (SendMessageW(g->hModRb[i], BM_GETCHECK, 0, 0) == BST_CHECKED)
            return { g->devices[(size_t)i] };
    }
    return g->devices;
}

static void set_hw_enabled(bool on) {
    EnableWindow(g->hOff, on);
    EnableWindow(g->hOn, on);
    EnableWindow(g->hApplyEff, on);
    EnableWindow(g->hApplyCol, on);
    EnableWindow(g->hPick, on);
    EnableWindow(g->hLive, on);
    EnableWindow(g->hR, on);
    EnableWindow(g->hG, on);
    EnableWindow(g->hB, on);
    EnableWindow(g->hEff, on);
    EnableWindow(g->hRescan, !g->busy);
}

static void refresh_autostart() {
    auto st = g->autostart.query();
    EnableWindow(g->hInst, !g->busy && st == AutostartInstallState::NotInstalled);
    EnableWindow(g->hRem, !g->busy && st == AutostartInstallState::Installed);
    if (st == AutostartInstallState::Unknown) {
        SetWindowTextW(g->hInst, L"Install Autostart (status unknown)");
        SetWindowTextW(g->hRem, L"Remove Autostart (status unknown)");
        EnableWindow(g->hInst, FALSE);
        EnableWindow(g->hRem, FALSE);
    } else {
        SetWindowTextW(g->hInst, L"Install Autostart");
        SetWindowTextW(g->hRem, L"Remove Autostart");
    }
}

static void apply_pawnio_ui() {
    SetWindowTextW(g->hPawnio, g->probe.message.c_str());
    BOOL offer = g->probe.offer_install_page() ? TRUE : FALSE;
    EnableWindow(g->hOpen, offer || g->probe.status != PawnioStatus::Ready);
    EnableWindow(g->hRetry, TRUE);
    set_hw_enabled(!g->busy && g->probe.can_use_hardware() && !g->devices.empty());
    EnableWindow(g->hRescan, !g->busy && g->probe.can_use_hardware());
}

static void rebuild_mod_radios() {
    for (int i = 0; i < 8; i++) {
        if (g->hModRb[i]) { DestroyWindow(g->hModRb[i]); g->hModRb[i] = nullptr; }
    }
    g->nmod = (int)((g->devices.size() < 8) ? g->devices.size() : 8);
    for (int i = 0; i < g->nmod; i++) {
        wchar_t t[64];
        swprintf(t, 64, L"Module 0x%02X (Port %d)", g->devices[i].address7, g->devices[i].bus_port);
        g->hModRb[i] = mk(g->wnd, L"BUTTON", t, BS_AUTORADIOBUTTON | WS_GROUP, 20, 168 + i * 22, 320, 20, IDC_MOD0 + i);
    }
    wchar_t m[128];
    if (g->devices.empty())
        swprintf(m, 128, L"Modules: none");
    else
        swprintf(m, 128, L"Modules: %d", (int)g->devices.size());
    SetWindowTextW(g->hMods, m);
}

static void post_probe_then_detect() {
    g->busy = true;
    SetWindowTextW(g->hStatus, L"Status: Checking PawnIO driver...");
    apply_pawnio_ui();
    HwJob j; j.kind = HwKind::Probe;
    g->worker->post(j);
}

static void save_selected_profile(const char* kind, uint8_t mode, uint8_t speed, uint8_t dir, uint8_t r, uint8_t gcol, uint8_t b) {
    Profile p;
    load_profile(p);
    auto sel = selected();
    for (auto& d : sel) {
        bool found = false;
        for (auto& m : p.modules) {
            if (m.address == d.address7) {
                m.port = d.bus_port; m.kind = kind; m.mode = mode; m.speed = speed; m.direction = dir;
                m.r = r; m.g = gcol; m.b = b; found = true; break;
            }
        }
        if (!found) {
            ModuleProfile m;
            m.address = d.address7; m.port = d.bus_port; m.kind = kind; m.mode = mode; m.speed = speed;
            m.direction = dir; m.r = r; m.g = gcol; m.b = b;
            p.modules.push_back(m);
        }
    }
    save_profile(p);
}

static void queue_color(bool preview) {
    if (!g->probe.can_use_hardware() || g->devices.empty()) return;
    HwJob j;
    j.kind = preview ? HwKind::PreviewColor : HwKind::Color;
    j.devices = selected();
    j.r = (uint8_t)SendMessageW(g->hR, TBM_GETPOS, 0, 0);
    j.g = (uint8_t)SendMessageW(g->hG, TBM_GETPOS, 0, 0);
    j.b = (uint8_t)SendMessageW(g->hB, TBM_GETPOS, 0, 0);
    if (preview) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - g->last_preview).count() < PREVIEW_MS)
            ; // worker still latest-wins
        g->last_preview = now;
    } else {
        g->busy = true;
        SetWindowTextW(g->hStatus, L"Status: Applying color...");
        save_selected_profile("static", MODE_STATIC, SPEED_NORMAL, DIR_FORWARD, j.r, j.g, j.b);
        if (SendMessageW(g->hAll, BM_GETCHECK, 0, 0) == BST_CHECKED)
            save_state(LightState::On);
    }
    g->worker->post(j);
}

static void layout(HWND w) {
    g->hStatus = mk(w, L"STATIC", L"Status: Starting", 0, 12, 8, 520, 18, IDC_STATUS);
    g->hPawnio = mk(w, L"STATIC", L"PawnIO: not checked yet", 0, 12, 28, 520, 36, IDC_PAWNIO);
    g->hOpen = mk(w, L"BUTTON", L"Open PawnIO site (pawnio.eu)", 0, 12, 68, 250, 26, IDC_OPENPAWNIO);
    g->hRetry = mk(w, L"BUTTON", L"Retry driver / detect", 0, 272, 68, 180, 26, IDC_RETRY);
    g->hMods = mk(w, L"STATIC", L"Modules: —", 0, 12, 100, 520, 18, IDC_MODULES);
    g->hRescan = mk(w, L"BUTTON", L"Rescan modules", 0, 12, 120, 200, 26, IDC_RESCAN);
    mk(w, L"BUTTON", L"Module selection", BS_GROUPBOX, 12, 152, 520, 40, 0);
    g->hAll = mk(w, L"BUTTON", L"All modules", BS_AUTORADIOBUTTON | WS_GROUP, 24, 168, 160, 20, IDC_ALL);
    SendMessageW(g->hAll, BM_SETCHECK, BST_CHECKED, 0);
    g->hOff = mk(w, L"BUTTON", L"OFF", 0, 12, 300, 250, 32, IDC_OFF);
    g->hOn = mk(w, L"BUTTON", L"ON (T-Force Default)", 0, 272, 300, 250, 32, IDC_ON);
    mk(w, L"STATIC", L"Effect:", 0, 12, 344, 60, 18, 0);
    g->hEff = mk(w, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, 80, 340, 360, 200, IDC_EFFECT);
    for (int i = 0; i < kModeCount; i++) {
        wchar_t t[80];
        swprintf(t, 80, L"%S (0x%02X)", kModes[i].name, kModes[i].value);
        SendMessageW(g->hEff, CB_ADDSTRING, 0, (LPARAM)t);
    }
    SendMessageW(g->hEff, CB_SETCURSEL, 25, 0);
    mk(w, L"STATIC", L"Speed:", 0, 12, 376, 60, 18, 0);
    g->hSpeed = mk(w, TRACKBAR_CLASSW, L"", TBS_AUTOTICKS, 80, 372, 280, 30, IDC_SPEED);
    SendMessageW(g->hSpeed, TBM_SETRANGE, TRUE, MAKELPARAM(0, 4));
    SendMessageW(g->hSpeed, TBM_SETPOS, TRUE, 2);
    g->hSpeedL = mk(w, L"STATIC", L"Normal", 0, 370, 376, 80, 18, IDC_SPEEDL);
    g->hRev = mk(w, L"BUTTON", L"Reverse direction", BS_AUTOCHECKBOX, 12, 408, 200, 20, IDC_REV);
    g->hApplyEff = mk(w, L"BUTTON", L"Apply effect", 0, 12, 432, 200, 26, IDC_APPLYEFF);
    g->hLive = mk(w, L"BUTTON", L"Live preview", BS_AUTOCHECKBOX, 12, 470, 140, 20, IDC_LIVE);
    SendMessageW(g->hLive, BM_SETCHECK, BST_CHECKED, 0);
    g->hPick = mk(w, L"BUTTON", L"Pick color", 0, 160, 466, 120, 26, IDC_PICK);
    mk(w, L"STATIC", L"R", 0, 12, 504, 16, 16, 0);
    g->hR = mk(w, TRACKBAR_CLASSW, L"", TBS_AUTOTICKS, 32, 500, 220, 28, IDC_R);
    g->hVR = mk(w, L"STATIC", L"0", 0, 260, 504, 40, 16, IDC_VR);
    mk(w, L"STATIC", L"G", 0, 12, 536, 16, 16, 0);
    g->hG = mk(w, TRACKBAR_CLASSW, L"", TBS_AUTOTICKS, 32, 532, 220, 28, IDC_G);
    g->hVG = mk(w, L"STATIC", L"0", 0, 260, 536, 40, 16, IDC_VG);
    mk(w, L"STATIC", L"B", 0, 12, 568, 16, 16, 0);
    g->hB = mk(w, TRACKBAR_CLASSW, L"", TBS_AUTOTICKS, 32, 564, 220, 28, IDC_B);
    g->hVB = mk(w, L"STATIC", L"0", 0, 260, 568, 40, 16, IDC_VB);
    SendMessageW(g->hR, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
    SendMessageW(g->hG, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
    SendMessageW(g->hB, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
    g->hSw = mk(w, L"STATIC", L"", WS_BORDER, 320, 504, 80, 80, IDC_SWATCH);
    g->hApplyCol = mk(w, L"BUTTON", L"Apply static color", 0, 12, 600, 200, 26, IDC_APPLYCOL);
    g->hInst = mk(w, L"BUTTON", L"Install Autostart", 0, 12, 640, 250, 26, IDC_INSTAS);
    g->hRem = mk(w, L"BUTTON", L"Remove Autostart", 0, 272, 640, 250, 26, IDC_REMAS);
    mk(w, L"STATIC", L"PawnIO kernel driver is required (https://pawnio.eu). DLL/bin next to the exe do not install it. This app never downloads the installer.",
       0, 12, 676, 520, 36, 0);
}

static void update_rgb_labels() {
    wchar_t b[16];
    int r = (int)SendMessageW(g->hR, TBM_GETPOS, 0, 0);
    int gc = (int)SendMessageW(g->hG, TBM_GETPOS, 0, 0);
    int bl = (int)SendMessageW(g->hB, TBM_GETPOS, 0, 0);
    swprintf(b, 16, L"%d", r); SetWindowTextW(g->hVR, b);
    swprintf(b, 16, L"%d", gc); SetWindowTextW(g->hVG, b);
    swprintf(b, 16, L"%d", bl); SetWindowTextW(g->hVB, b);
    HBRUSH br = CreateSolidBrush(RGB(r, gc, bl));
    HBRUSH old = (HBRUSH)SetClassLongPtrW(g->hSw, GCLP_HBRBACKGROUND, (LONG_PTR)br);
    if (old) DeleteObject(old);
    InvalidateRect(g->hSw, nullptr, TRUE);
    if (SendMessageW(g->hLive, BM_GETCHECK, 0, 0) == BST_CHECKED && g->probe.can_use_hardware() && !g->devices.empty())
        queue_color(true);
}

struct HwNotify { bool ok; std::wstring msg; std::vector<XtreemDevice> devices; PawnioProbe probe; HwKind kind; };

static LRESULT CALLBACK WndProc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    if (m == WM_CREATE) {
        g->wnd = w;
        INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES };
        InitCommonControlsEx(&icc);
        g->font = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              0, 0, 0, 0, L"Segoe UI");
        layout(w);
        g->worker->on_done = [w](HwKind kind, bool ok, std::wstring msg, std::vector<XtreemDevice> d, PawnioProbe p) {
            auto* n = new HwNotify{ ok, std::move(msg), std::move(d), p, kind };
            PostMessageW(w, WM_HW, 0, (LPARAM)n);
        };
        if (g->smoke) {
            SetWindowTextW(g->hStatus, L"Status: GUI smoke (no hardware)");
            SetWindowTextW(g->hPawnio, L"GUI smoke test — hardware not touched");
            PostMessageW(w, WM_CLOSE, 0, 0);
            return 0;
        }
        post_probe_then_detect();
        return 0;
    }
    if (m == WM_HW) {
        auto* n = (HwNotify*)lp;
        g->busy = false;
        g->probe = n->probe;
        if (n->kind == HwKind::Detect)
            g->devices = n->devices;
        SetWindowTextW(g->hStatus, n->ok ? n->msg.c_str() : (L"Status: " + n->msg).c_str());
        apply_pawnio_ui();
        if (n->kind == HwKind::Probe) {
            if (n->ok && g->probe.can_use_hardware()) {
                HwJob j; j.kind = HwKind::Detect;
                g->busy = true;
                SetWindowTextW(g->hStatus, L"Status: Detecting modules...");
                g->worker->post(j);
            } else if (g->probe.offer_install_page() || g->probe.status == PawnioStatus::PermissionDenied
                       || g->probe.status == PawnioStatus::Other) {
                MessageBoxW(w, (n->msg + L"\n\nUse “Open PawnIO site” (https://pawnio.eu) to install the kernel driver yourself, then Retry.\nThis program does not download or run the installer.\nBundled PawnIOLib.dll / SmbusPIIX4.bin are not the driver.").c_str(),
                            L"PawnIO required", MB_OK | MB_ICONWARNING);
            }
        }
        if (n->kind == HwKind::Detect)
            rebuild_mod_radios();
        refresh_autostart();
        delete n;
        return 0;
    }
    if (m == WM_HSCROLL && (HWND)lp) {
        if ((HWND)lp == g->hSpeed)
            SetWindowTextA(g->hSpeedL, speed_label((int)SendMessageW(g->hSpeed, TBM_GETPOS, 0, 0)));
        else
            update_rgb_labels();
    }
    if (m == WM_COMMAND) {
        int id = LOWORD(wp);
        if (id == IDC_OPENPAWNIO) open_pawnio_site();
        if (id == IDC_RETRY || id == IDC_RESCAN) {
            g->devices.clear();
            post_probe_then_detect();
        }
        if (id == IDC_OFF && g->probe.can_use_hardware()) {
            HwJob j; j.kind = HwKind::Off; j.devices = selected();
            g->busy = true; SetWindowTextW(g->hStatus, L"Status: Turning OFF...");
            save_selected_profile("off", MODE_OFF, SPEED_NORMAL, DIR_FORWARD, 0, 0, 0);
            if (SendMessageW(g->hAll, BM_GETCHECK, 0, 0) == BST_CHECKED) save_state(LightState::Off);
            g->worker->post(j);
        }
        if (id == IDC_ON && g->probe.can_use_hardware()) {
            HwJob j; j.kind = HwKind::On; j.devices = selected();
            g->busy = true; SetWindowTextW(g->hStatus, L"Status: Turning ON...");
            save_selected_profile("on", MODE_TFORCE_DEFAULT, SPEED_NORMAL, DIR_FORWARD, 0, 0, 0);
            if (SendMessageW(g->hAll, BM_GETCHECK, 0, 0) == BST_CHECKED) save_state(LightState::On);
            g->worker->post(j);
        }
        if (id == IDC_APPLYEFF && g->probe.can_use_hardware()) {
            int idx = (int)SendMessageW(g->hEff, CB_GETCURSEL, 0, 0);
            if (idx >= 0 && idx < kModeCount) {
                int sp = (int)SendMessageW(g->hSpeed, TBM_GETPOS, 0, 0);
                HwJob j; j.kind = HwKind::Effect; j.devices = selected();
                j.mode = kModes[idx].value; j.speed = speed_from_slider(sp);
                j.dir = SendMessageW(g->hRev, BM_GETCHECK, 0, 0) == BST_CHECKED ? DIR_REVERSE : DIR_FORWARD;
                g->busy = true; SetWindowTextW(g->hStatus, L"Status: Applying effect...");
                save_selected_profile("effect", j.mode, j.speed, j.dir, 0, 0, 0);
                g->worker->post(j);
            }
        }
        if (id == IDC_APPLYCOL) queue_color(false);
        if (id == IDC_PICK) {
            static COLORREF cust[16]{};
            CHOOSECOLORW cc{};
            cc.lStructSize = sizeof(cc); cc.hwndOwner = w; cc.lpCustColors = cust;
            cc.rgbResult = RGB(SendMessageW(g->hR, TBM_GETPOS, 0, 0), SendMessageW(g->hG, TBM_GETPOS, 0, 0), SendMessageW(g->hB, TBM_GETPOS, 0, 0));
            cc.Flags = CC_FULLOPEN | CC_RGBINIT;
            if (ChooseColorW(&cc)) {
                SendMessageW(g->hR, TBM_SETPOS, TRUE, GetRValue(cc.rgbResult));
                SendMessageW(g->hG, TBM_SETPOS, TRUE, GetGValue(cc.rgbResult));
                SendMessageW(g->hB, TBM_SETPOS, TRUE, GetBValue(cc.rgbResult));
                update_rgb_labels();
            }
        }
        if (id == IDC_EFFECT) {
            int idx = (int)SendMessageW(g->hEff, CB_GETCURSEL, 0, 0);
            if (idx >= 0 && idx < kModeCount) {
                EnableWindow(g->hSpeed, kModes[idx].supports_speed);
                EnableWindow(g->hRev, kModes[idx].supports_direction);
            }
        }
        if (id == IDC_SPEED) SetWindowTextA(g->hSpeedL, speed_label((int)SendMessageW(g->hSpeed, TBM_GETPOS, 0, 0)));
        if (id == IDC_INSTAS) {
            try {
                wchar_t exe[MAX_PATH]{}; GetModuleFileNameW(nullptr, exe, MAX_PATH);
                g->autostart.install(exe);
                MessageBoxW(w, L"Autostart installed (task XtreemRgbToggle-Off, highest, no delay).", L"Autostart", MB_OK);
            } catch (const std::exception& ex) {
                MessageBoxA(w, ex.what(), "Autostart", MB_ICONERROR);
            }
            refresh_autostart();
        }
        if (id == IDC_REMAS) {
            try {
                g->autostart.uninstall();
                MessageBoxW(w, L"Autostart removed.", L"Autostart", MB_OK);
            } catch (const std::exception& ex) {
                MessageBoxA(w, ex.what(), "Autostart", MB_ICONERROR);
            }
            refresh_autostart();
        }
    }
    if (m == WM_DESTROY) {
        if (g->worker) g->worker->request_stop();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(w, m, wp, lp);
}

int run_gui(HINSTANCE inst, const wchar_t* exe_dir, bool smoke) {
    UiState st{};
    g = &st;
    st.dir = exe_dir;
    st.smoke = smoke;
    HwWorker worker(st.dir);
    st.worker = &worker;
    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.lpszClassName = L"XtreemRgbToggleWnd";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);
    HWND w = CreateWindowExW(0, wc.lpszClassName, L"XTREEM RGB Control",
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                             CW_USEDEFAULT, CW_USEDEFAULT, 560, 760, nullptr, nullptr, inst, nullptr);
    ShowWindow(w, SW_SHOW);
    UpdateWindow(w);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    g = nullptr;
    return (int)msg.wParam;
}

int run_tray(HINSTANCE inst, const wchar_t* exe_dir) {
    return run_gui(inst, exe_dir, false);
}
