#include "ui.hpp"
#include "autostart.hpp"
#include "persist.hpp"
#include "modes.hpp"
#include "diag.hpp"
#include "layout.hpp"
#include <stdexcept>
#include <algorithm>
#include <functional>
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <cstring>

enum {
    IDC_STATUS = 1001, IDC_PAWNIO, IDC_PAWNIOLINK, IDC_OPENPAWNIO, IDC_RETRY, IDC_MODULES, IDC_RESCAN,
    IDC_ALL = 1100, IDC_MOD0 = 1101, IDC_EFFECT = 1202, IDC_SPEED, IDC_SPEEDL,
    IDC_REV, IDC_APPLYEFF, IDC_LIVE, IDC_PICK, IDC_R, IDC_G, IDC_B, IDC_VR, IDC_VG, IDC_VB,
    IDC_SWATCH, IDC_APPLYCOL, IDC_INSTAS, IDC_REMAS,
    ID_TRAY_OPEN = 1401, ID_TRAY_EXIT = 1402
};
static constexpr UINT WM_HW = WM_APP + 1;
static constexpr UINT WM_TRAY = WM_APP + 2;

struct UiState {
    HWND wnd{};
    std::wstring dir;
    HwWorker* worker{};
    FakeHw* fake{};
    Autostart autostart;
    std::vector<XtreemDevice> devices;
    PawnioProbe probe;
    bool busy = false;
    bool exiting = false;
    bool restored_once = false;
    bool syncing = false;
    int dpi = 96;
    GuiMode mode = GuiMode::Normal;
    HWND hStatus, hPawnio, hLink, hOpen, hRetry, hMods, hRescan, hGrp, hAll;
    HWND hLblEff, hEff, hLblSpeed, hSpeed, hSpeedL, hRev, hApplyEff, hLive, hPick;
    HWND hLblR, hLblG, hLblB, hR, hG, hB, hVR, hVG, hVB, hSw, hApplyCol;
    HWND hInst, hRem, hHint;
    HWND hModRb[8]{};
    int nmod = 0;
    HFONT font{};
    NOTIFYICONDATAW nid{};
    bool tray_added = false;
};

static UiState* g;

std::wstring exe_directory() {
    wchar_t buf[MAX_PATH]{};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p = buf;
    auto sl = p.find_last_of(L"\\/");
    return sl == std::wstring::npos ? L"." : p.substr(0, sl);
}

void detach_console_for_gui() {
    FreeConsole();
}

void open_pawnio_site() {
    if (g && g->mode == GuiMode::SelfTest) return;
    INT_PTR r = (INT_PTR)ShellExecuteW(nullptr, L"open", kPawnioOfficialPage, nullptr, nullptr, SW_SHOWNORMAL);
    if (r <= 32) {
        MessageBoxW(nullptr,
                    L"Could not open https://pawnio.eu\n\nOpen that official page in a browser yourself and install the kernel driver there.\nThis program does not download or run PawnIO_setup.exe.",
                    L"PawnIO page", MB_OK | MB_ICONINFORMATION);
    }
}

static HWND mk(HWND p, const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, p, (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr);
    if (g && g->font) SendMessageW(c, WM_SETFONT, (WPARAM)g->font, TRUE);
    return c;
}

static int window_dpi(HWND w) {
    using Fn = UINT (WINAPI*)(HWND);
    auto p = (Fn)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    if (p) {
        UINT d = p(w);
        if (d) return (int)d;
    }
    HDC dc = GetDC(w);
    int d = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
    if (dc) ReleaseDC(w, dc);
    return d > 0 ? d : 96;
}

static int S(int v) { return scale_px(v, g && g->dpi ? g->dpi : 96); }

static void fill_persist(HwJob& j, const char* kind) {
    j.persist = true;
    memset(j.persist_kind, 0, sizeof(j.persist_kind));
    if (kind) strncpy(j.persist_kind, kind, sizeof(j.persist_kind) - 1);
}

static bool all_checked() {
    return g && SendMessageW(g->hAll, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

static RECT child_rect(HWND parent, HWND child) {
    RECT r{};
    if (!child) return r;
    GetWindowRect(child, &r);
    MapWindowPoints(HWND_DESKTOP, parent, (POINT*)&r, 2);
    return r;
}
static bool rect_contains(const RECT& outer, const RECT& inner) {
    return inner.left >= outer.left && inner.right <= outer.right
        && inner.top >= outer.top && inner.bottom <= outer.bottom;
}
static bool layout_radios_inside_group() {
    if (!g || !g->hGrp || !g->hAll) return false;
    RECT grp = child_rect(g->wnd, g->hGrp);
    if (!rect_contains(grp, child_rect(g->wnd, g->hAll))) return false;
    for (int i = 0; i < g->nmod; i++) {
        if (!g->hModRb[i]) return false;
        if (!rect_contains(grp, child_rect(g->wnd, g->hModRb[i]))) return false;
    }
    RECT below = child_rect(g->wnd, g->hLblEff);
    return below.top >= grp.bottom;
}

static void apply_font(HWND h) {
    if (h && g && g->font) SendMessageW(h, WM_SETFONT, (WPARAM)g->font, TRUE);
}

static void remake_font() {
    if (!g) return;
    HFONT old = g->font;
    g->font = CreateFontW(-S(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                          0, 0, 0, 0, L"Segoe UI");
    HWND ctrls[] = {
        g->hStatus, g->hPawnio, g->hLink, g->hOpen, g->hRetry, g->hMods, g->hRescan, g->hGrp, g->hAll,
        g->hLblEff, g->hEff, g->hLblSpeed, g->hSpeed, g->hSpeedL, g->hRev, g->hApplyEff, g->hLive, g->hPick,
        g->hLblR, g->hLblG, g->hLblB, g->hR, g->hG, g->hB, g->hVR, g->hVG, g->hVB, g->hSw, g->hApplyCol,
        g->hInst, g->hRem, g->hHint
    };
    for (HWND h : ctrls) apply_font(h);
    for (int i = 0; i < 8; i++) apply_font(g->hModRb[i]);
    if (old) DeleteObject(old);
}

static void relayout();
static void sync_ui_from_selection();
static void maybe_restore_once();

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
    if (g->exiting || g->mode == GuiMode::SelfTest || !autostart_allow_child_processes())
        return;
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
    EnableWindow(g->hOpen, TRUE);
    EnableWindow(g->hRetry, g->busy ? FALSE : TRUE);
    EnableWindow(g->hLink, TRUE);
    set_hw_enabled(!g->busy && g->probe.can_use_hardware() && !g->devices.empty());
    EnableWindow(g->hRescan, !g->busy && g->probe.can_use_hardware());
}

static void tray_del() {
    if (!g || !g->tray_added) return;
    Shell_NotifyIconW(NIM_DELETE, &g->nid);
    g->tray_added = false;
}
static void tray_add() {
    if (!g || g->tray_added) return;
    memset(&g->nid, 0, sizeof(g->nid));
    g->nid.cbSize = sizeof(g->nid);
    g->nid.hWnd = g->wnd;
    g->nid.uID = 1;
    g->nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g->nid.uCallbackMessage = WM_TRAY;
    g->nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    lstrcpynW(g->nid.szTip, L"XTREEM RGB Control", ARRAYSIZE(g->nid.szTip));
    if (Shell_NotifyIconW(NIM_ADD, &g->nid))
        g->tray_added = true;
}

static void rebuild_mod_radios() {
    for (int i = 0; i < 8; i++) {
        if (g->hModRb[i]) { DestroyWindow(g->hModRb[i]); g->hModRb[i] = nullptr; }
    }
    g->nmod = (int)((g->devices.size() < 8) ? g->devices.size() : 8);
    for (int i = 0; i < g->nmod; i++) {
        wchar_t t[64];
        swprintf(t, 64, L"Module 0x%02X (Port %d)", g->devices[i].address7, g->devices[i].bus_port);
        g->hModRb[i] = mk(g->wnd, L"BUTTON", t, BS_AUTORADIOBUTTON, 0, 0, 1, 1, IDC_MOD0 + i);
    }
    wchar_t m[128];
    if (g->devices.empty())
        swprintf(m, 128, L"Modules: none");
    else
        swprintf(m, 128, L"Modules: %d", (int)g->devices.size());
    SetWindowTextW(g->hMods, m);
    relayout();
}

static void relayout() {
    if (!g || !g->wnd) return;
    int dpi = g->dpi > 0 ? g->dpi : 96;
    int gy = scale_px(186, dpi);
    auto blk = module_block(g->nmod, dpi, gy);
    auto mv = [&](HWND h, int x, int y, int w, int hgt) {
        if (h) MoveWindow(h, scale_px(x, dpi), scale_px(y, dpi), scale_px(w, dpi), scale_px(hgt, dpi), TRUE);
    };
    mv(g->hStatus, 12, 8, 520, 18);
    mv(g->hPawnio, 12, 28, 520, 48);
    mv(g->hLink, 12, 78, 520, 22);
    mv(g->hOpen, 12, 102, 250, 26);
    mv(g->hRetry, 272, 102, 250, 26);
    mv(g->hMods, 12, 134, 520, 18);
    mv(g->hRescan, 12, 154, 200, 26);
    if (g->hGrp)
        MoveWindow(g->hGrp, blk.group_x, blk.group_y, blk.group_w, blk.group_h, TRUE);
    if (g->hAll)
        MoveWindow(g->hAll, blk.radio_x, blk.all_y, scale_px(200, dpi), blk.radio_h, TRUE);
    for (int i = 0; i < g->nmod; i++) {
        if (g->hModRb[i])
            MoveWindow(g->hModRb[i], blk.radio_x, blk.radio_y[i], blk.radio_w, blk.radio_h, TRUE);
    }
    int by = blk.below_y;
    auto my = [&](HWND h, int dx, int dy, int w, int hgt) {
        if (h) MoveWindow(h, scale_px(dx, dpi), by + scale_px(dy, dpi), scale_px(w, dpi), scale_px(hgt, dpi), TRUE);
    };
    my(g->hLblEff, 12, 4, 60, 18);
    my(g->hEff, 80, 0, 440, 200);
    my(g->hLblSpeed, 12, 36, 60, 18);
    my(g->hSpeed, 80, 32, 280, 30);
    my(g->hSpeedL, 370, 36, 80, 18);
    my(g->hRev, 12, 68, 200, 20);
    my(g->hApplyEff, 12, 92, 200, 26);
    my(g->hLive, 12, 130, 140, 20);
    my(g->hPick, 160, 126, 120, 26);
    my(g->hLblR, 12, 164, 16, 16);
    my(g->hR, 32, 160, 220, 28);
    my(g->hVR, 260, 164, 40, 16);
    my(g->hLblG, 12, 196, 16, 16);
    my(g->hG, 32, 192, 220, 28);
    my(g->hVG, 260, 196, 40, 16);
    my(g->hLblB, 12, 228, 16, 16);
    my(g->hB, 32, 224, 220, 28);
    my(g->hVB, 260, 228, 40, 16);
    my(g->hSw, 320, 160, 80, 80);
    my(g->hApplyCol, 12, 260, 200, 26);
    my(g->hInst, 12, 300, 250, 26);
    my(g->hRem, 272, 300, 250, 26);
    my(g->hHint, 12, 336, 520, 36);
    int client_w = scale_px(560, dpi);
    int client_h = by + scale_px(336 + 36 + 16, dpi);
    RECT wr{ 0, 0, client_w, client_h };
    DWORD style = (DWORD)GetWindowLongPtrW(g->wnd, GWL_STYLE);
    AdjustWindowRect(&wr, style, FALSE);
    SetWindowPos(g->wnd, nullptr, 0, 0, wr.right - wr.left, wr.bottom - wr.top,
                 SWP_NOMOVE | SWP_NOZORDER);
}

static bool same_saved(const ModuleProfile& a, const ModuleProfile& b) {
    return a.kind == b.kind && a.mode == b.mode && a.speed == b.speed && a.direction == b.direction
        && a.r == b.r && a.g == b.g && a.b == b.b;
}
static const ModuleProfile* find_saved(const Profile& p, uint8_t addr) {
    for (auto& m : p.modules)
        if (m.address == addr) return &m;
    return nullptr;
}
static int combo_index_for(const ModuleProfile& m) {
    if (m.kind == "off") return 0;
    if (m.kind == "on") return 25;
    for (int i = 0; i < kModeCount; i++)
        if (kModes[i].value == m.mode) return i;
    if (m.kind == "static") return 1;
    return -1;
}
static void apply_module_to_ui(const ModuleProfile& m) {
    int idx = combo_index_for(m);
    if (idx >= 0) {
        SendMessageW(g->hEff, CB_SETCURSEL, idx, 0);
        EnableWindow(g->hSpeed, kModes[idx].supports_speed);
        EnableWindow(g->hRev, kModes[idx].supports_direction);
    }
    int sp = slider_from_speed(m.speed);
    SendMessageW(g->hSpeed, TBM_SETPOS, TRUE, sp);
    SetWindowTextA(g->hSpeedL, speed_label(sp));
    SendMessageW(g->hRev, BM_SETCHECK, m.direction == DIR_REVERSE ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(g->hR, TBM_SETPOS, TRUE, m.r);
    SendMessageW(g->hG, TBM_SETPOS, TRUE, m.g);
    SendMessageW(g->hB, TBM_SETPOS, TRUE, m.b);
    wchar_t b[16];
    swprintf(b, 16, L"%d", (int)m.r); SetWindowTextW(g->hVR, b);
    swprintf(b, 16, L"%d", (int)m.g); SetWindowTextW(g->hVG, b);
    swprintf(b, 16, L"%d", (int)m.b); SetWindowTextW(g->hVB, b);
    HBRUSH br = CreateSolidBrush(RGB(m.r, m.g, m.b));
    HBRUSH old = (HBRUSH)SetClassLongPtrW(g->hSw, GCLP_HBRBACKGROUND, (LONG_PTR)br);
    if (old) DeleteObject(old);
    InvalidateRect(g->hSw, nullptr, TRUE);
}

static void sync_ui_from_selection() {
    if (!g) return;
    g->syncing = true;
    Profile p;
    auto st = load_profile_status(p);
    if (st != ProfileStatus::Ok || p.modules.empty()) {
        g->syncing = false;
        return;
    }
    if (!all_checked()) {
        for (int i = 0; i < g->nmod; i++) {
            if (SendMessageW(g->hModRb[i], BM_GETCHECK, 0, 0) == BST_CHECKED) {
                if (auto* m = find_saved(p, g->devices[(size_t)i].address7))
                    apply_module_to_ui(*m);
                g->syncing = false;
                return;
            }
        }
        g->syncing = false;
        return;
    }
    const ModuleProfile* first = nullptr;
    bool mixed = false;
    int seen = 0;
    for (auto& d : g->devices) {
        auto* m = find_saved(p, d.address7);
        if (!m) { mixed = true; continue; }
        seen++;
        if (!first) first = m;
        else if (!same_saved(*first, *m)) mixed = true;
    }
    if (mixed || seen == 0 || (size_t)seen != g->devices.size())
        SendMessageW(g->hEff, CB_SETCURSEL, (WPARAM)-1, 0);
    else if (first)
        apply_module_to_ui(*first);
    g->syncing = false;
}

static void maybe_restore_once() {
    if (!g || g->restored_once || g->exiting) return;
    g->restored_once = true;
    Profile p;
    auto st = load_profile_status(p);
    auto act = profile_restore_action(st, p.modules.size());
    if (act != RestoreAction::Apply) {
        SetWindowTextW(g->hStatus, restore_skip_message(act));
        return;
    }
    HwJob j;
    j.kind = HwKind::Restore;
    j.profile = std::move(p);
    g->busy = true;
    SetWindowTextW(g->hStatus, L"Status: Restoring saved profile...");
    g->worker->post(j);
}

static void post_probe_then_detect() {
    if (g->exiting) return;
    g->busy = true;
    SetWindowTextW(g->hStatus, L"Status: Checking PawnIO driver...");
    apply_pawnio_ui();
    HwJob j; j.kind = HwKind::Probe;
    g->worker->post(j);
}

static void queue_color(bool preview) {
    if (g->exiting || !g->probe.can_use_hardware() || g->devices.empty()) return;
    HwJob j;
    j.kind = preview ? HwKind::PreviewColor : HwKind::Color;
    j.devices = selected();
    j.r = (uint8_t)SendMessageW(g->hR, TBM_GETPOS, 0, 0);
    j.g = (uint8_t)SendMessageW(g->hG, TBM_GETPOS, 0, 0);
    j.b = (uint8_t)SendMessageW(g->hB, TBM_GETPOS, 0, 0);
    if (!preview) {
        g->busy = true;
        SetWindowTextW(g->hStatus, L"Status: Applying color...");
        fill_persist(j, "static");
        j.mode = MODE_STATIC;
        j.speed = SPEED_NORMAL;
        j.dir = DIR_FORWARD;
    }
    g->worker->post(j);
}

static void begin_exit(HWND w) {
    if (g->exiting) return;
    g->exiting = true;
    autostart_set_allow_child_processes(false);
    tray_del();
    if (g->worker) g->worker->request_stop();
    DestroyWindow(w);
}

static void layout(HWND w) {
    g->hStatus = mk(w, L"STATIC", L"Status: Starting — PawnIO not checked yet", 0, 0, 0, 1, 1, IDC_STATUS);
    g->hPawnio = mk(w, L"STATIC", L"PawnIO: not checked yet (no hardware access until this passes)", 0, 0, 0, 1, 1, IDC_PAWNIO);
    g->hLink = CreateWindowExW(0, WC_LINK,
        L"Official PawnIO download/install page (you open it; this app never downloads): <a href=\"https://pawnio.eu\">https://pawnio.eu</a>",
        WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, w, (HMENU)(INT_PTR)IDC_PAWNIOLINK, GetModuleHandleW(nullptr), nullptr);
    if (g->font) SendMessageW(g->hLink, WM_SETFONT, (WPARAM)g->font, TRUE);
    g->hOpen = mk(w, L"BUTTON", L"Open official PawnIO page", 0, 0, 0, 1, 1, IDC_OPENPAWNIO);
    g->hRetry = mk(w, L"BUTTON", L"Retry after installing", 0, 0, 0, 1, 1, IDC_RETRY);
    g->hMods = mk(w, L"STATIC", L"Modules: —", 0, 0, 0, 1, 1, IDC_MODULES);
    g->hRescan = mk(w, L"BUTTON", L"Rescan modules", 0, 0, 0, 1, 1, IDC_RESCAN);
    g->hGrp = mk(w, L"BUTTON", L"Module selection", BS_GROUPBOX, 0, 0, 1, 1, 0);
    g->hAll = mk(w, L"BUTTON", L"All modules", BS_AUTORADIOBUTTON | WS_GROUP, 0, 0, 1, 1, IDC_ALL);
    SendMessageW(g->hAll, BM_SETCHECK, BST_CHECKED, 0);
    g->hLblEff = mk(w, L"STATIC", L"Effect:", 0, 0, 0, 1, 1, 0);
    g->hEff = mk(w, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 1, 1, IDC_EFFECT);
    for (int i = 0; i < kModeCount; i++) {
        wchar_t t[96];
        format_effect_label(t, 96, kModes[i]);
        SendMessageW(g->hEff, CB_ADDSTRING, 0, (LPARAM)t);
    }
    SendMessageW(g->hEff, CB_SETCURSEL, 25, 0);
    g->hLblSpeed = mk(w, L"STATIC", L"Speed:", 0, 0, 0, 1, 1, 0);
    g->hSpeed = mk(w, TRACKBAR_CLASSW, L"", TBS_AUTOTICKS, 0, 0, 1, 1, IDC_SPEED);
    SendMessageW(g->hSpeed, TBM_SETRANGE, TRUE, MAKELPARAM(0, 4));
    SendMessageW(g->hSpeed, TBM_SETPOS, TRUE, 2);
    g->hSpeedL = mk(w, L"STATIC", L"Normal", 0, 0, 0, 1, 1, IDC_SPEEDL);
    g->hRev = mk(w, L"BUTTON", L"Reverse direction", BS_AUTOCHECKBOX, 0, 0, 1, 1, IDC_REV);
    g->hApplyEff = mk(w, L"BUTTON", L"Apply effect", 0, 0, 0, 1, 1, IDC_APPLYEFF);
    g->hLive = mk(w, L"BUTTON", L"Live preview", BS_AUTOCHECKBOX, 0, 0, 1, 1, IDC_LIVE);
    SendMessageW(g->hLive, BM_SETCHECK, BST_CHECKED, 0);
    g->hPick = mk(w, L"BUTTON", L"Pick color", 0, 0, 0, 1, 1, IDC_PICK);
    g->hLblR = mk(w, L"STATIC", L"R", 0, 0, 0, 1, 1, 0);
    g->hR = mk(w, TRACKBAR_CLASSW, L"", TBS_AUTOTICKS, 0, 0, 1, 1, IDC_R);
    g->hVR = mk(w, L"STATIC", L"0", 0, 0, 0, 1, 1, IDC_VR);
    g->hLblG = mk(w, L"STATIC", L"G", 0, 0, 0, 1, 1, 0);
    g->hG = mk(w, TRACKBAR_CLASSW, L"", TBS_AUTOTICKS, 0, 0, 1, 1, IDC_G);
    g->hVG = mk(w, L"STATIC", L"0", 0, 0, 0, 1, 1, IDC_VG);
    g->hLblB = mk(w, L"STATIC", L"B", 0, 0, 0, 1, 1, 0);
    g->hB = mk(w, TRACKBAR_CLASSW, L"", TBS_AUTOTICKS, 0, 0, 1, 1, IDC_B);
    g->hVB = mk(w, L"STATIC", L"0", 0, 0, 0, 1, 1, IDC_VB);
    SendMessageW(g->hR, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
    SendMessageW(g->hG, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
    SendMessageW(g->hB, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
    g->hSw = mk(w, L"STATIC", L"", WS_BORDER, 0, 0, 1, 1, IDC_SWATCH);
    g->hApplyCol = mk(w, L"BUTTON", L"Apply static color", 0, 0, 0, 1, 1, IDC_APPLYCOL);
    g->hInst = mk(w, L"BUTTON", L"Install Autostart", 0, 0, 0, 1, 1, IDC_INSTAS);
    g->hRem = mk(w, L"BUTTON", L"Remove Autostart", 0, 0, 0, 1, 1, IDC_REMAS);
    g->hHint = mk(w, L"STATIC", L"Minimize goes to tray. Close / tray Exit quits. Off and T-Force Default are in Effect (Off is not Static).",
                  0, 0, 0, 1, 1, 0);
    relayout();
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
    if (!g->syncing && SendMessageW(g->hLive, BM_GETCHECK, 0, 0) == BST_CHECKED && g->probe.can_use_hardware() && !g->devices.empty())
        queue_color(true);
}

static void apply_effect_from_combo() {
    if (g->exiting || !g->probe.can_use_hardware()) return;
    int idx = (int)SendMessageW(g->hEff, CB_GETCURSEL, 0, 0);
    if (idx < 0 || idx >= kModeCount) return;
    uint8_t mode = kModes[idx].value;
    int sp = (int)SendMessageW(g->hSpeed, TBM_GETPOS, 0, 0);
    uint8_t dir = SendMessageW(g->hRev, BM_GETCHECK, 0, 0) == BST_CHECKED ? DIR_REVERSE : DIR_FORWARD;
    HwJob j;
    j.devices = selected();
    auto path = effect_apply_path(mode);
    if (path == EffectApplyPath::LightsOff) {
        j.kind = HwKind::Off;
        fill_persist(j, "off");
        j.mode = MODE_OFF; j.speed = SPEED_NORMAL; j.dir = DIR_FORWARD;
        j.r = j.g = j.b = 0;
        SetWindowTextW(g->hStatus, L"Status: Turning OFF...");
    } else if (path == EffectApplyPath::TforceDefault) {
        j.kind = HwKind::On;
        fill_persist(j, "on");
        j.mode = MODE_TFORCE_DEFAULT; j.speed = SPEED_NORMAL; j.dir = DIR_FORWARD;
        SetWindowTextW(g->hStatus, L"Status: T-Force Default...");
    } else {
        j.kind = HwKind::Effect;
        j.mode = mode; j.speed = speed_from_slider(sp); j.dir = dir;
        fill_persist(j, "effect");
        SetWindowTextW(g->hStatus, L"Status: Applying effect...");
    }
    g->busy = true;
    g->worker->post(j);
}

struct HwNotify { bool ok; std::wstring msg; std::vector<XtreemDevice> devices; PawnioProbe probe; HwKind kind; };

static LRESULT CALLBACK WndProc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    if (m == WM_CREATE) {
        g->wnd = w;
        INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES | ICC_LINK_CLASS };
        InitCommonControlsEx(&icc);
        g->dpi = window_dpi(w);
        g->font = CreateFontW(-S(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              0, 0, 0, 0, L"Segoe UI");
        layout(w);
        g->worker->set_on_done([w](HwKind kind, bool ok, std::wstring msg, std::vector<XtreemDevice> d, PawnioProbe p) {
            if (!IsWindow(w)) return;
            auto* n = new HwNotify{ ok, std::move(msg), std::move(d), p, kind };
            if (!PostMessageW(w, WM_HW, 0, (LPARAM)n))
                delete n;
        });
        post_probe_then_detect();
        return 0;
    }
    if (m == WM_HW) {
        auto* n = (HwNotify*)lp;
        if (g->exiting) { delete n; return 0; }
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
            } else if (g->mode != GuiMode::SelfTest && g->mode != GuiMode::Tray) {
                UINT icon = g->probe.offer_install_page() ? MB_ICONWARNING : MB_ICONINFORMATION;
                MessageBoxW(w, pawnio_dialog_text(g->probe).c_str(), L"PawnIO driver check", MB_OK | icon);
            }
        }
        if (n->kind == HwKind::Detect) {
            rebuild_mod_radios();
            if (n->ok && g->probe.can_use_hardware())
                maybe_restore_once();
            sync_ui_from_selection();
        }
        if (n->ok && (n->kind == HwKind::Off || n->kind == HwKind::On || n->kind == HwKind::Effect || n->kind == HwKind::Color)) {
            if (n->kind == HwKind::Off && all_checked()) save_state(LightState::Off);
            if ((n->kind == HwKind::On || n->kind == HwKind::Color) && all_checked()) save_state(LightState::On);
            sync_ui_from_selection();
        }
        if (n->kind == HwKind::Restore)
            sync_ui_from_selection();
        apply_pawnio_ui();
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
        if (id == IDC_ALL || (id >= IDC_MOD0 && id < IDC_MOD0 + 8))
            sync_ui_from_selection();
        if (id == IDC_APPLYEFF) apply_effect_from_combo();
        if (id == IDC_APPLYCOL) queue_color(false);
        if (id == IDC_PICK && g->mode != GuiMode::SelfTest) {
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
        if (id == IDC_INSTAS && g->mode != GuiMode::SelfTest) {
            try {
                wchar_t exe[MAX_PATH]{}; GetModuleFileNameW(nullptr, exe, MAX_PATH);
                g->autostart.install(exe);
                MessageBoxW(w, L"Autostart installed (task XtreemRgbToggle-Off runs silent restore at logon, highest, no delay).", L"Autostart", MB_OK);
            } catch (const std::exception& ex) {
                MessageBoxA(w, ex.what(), "Autostart", MB_ICONERROR);
            }
            refresh_autostart();
        }
        if (id == IDC_REMAS && g->mode != GuiMode::SelfTest) {
            try {
                g->autostart.uninstall();
                MessageBoxW(w, L"Autostart removed.", L"Autostart", MB_OK);
            } catch (const std::exception& ex) {
                MessageBoxA(w, ex.what(), "Autostart", MB_ICONERROR);
            }
            refresh_autostart();
        }
        if (id == ID_TRAY_OPEN) {
            ShowWindow(w, SW_RESTORE);
            ShowWindow(w, SW_SHOW);
            SetForegroundWindow(w);
        }
        if (id == ID_TRAY_EXIT) begin_exit(w);
    }
    if (m == WM_NOTIFY) {
        auto* hdr = (NMHDR*)lp;
        if (hdr && hdr->idFrom == IDC_PAWNIOLINK && (hdr->code == NM_CLICK || hdr->code == NM_RETURN)) {
            open_pawnio_site();
            return 0;
        }
    }
    if (m == WM_TRAY) {
        if (lp == WM_LBUTTONDBLCLK || lp == WM_LBUTTONUP) {
            ShowWindow(w, SW_RESTORE);
            ShowWindow(w, SW_SHOW);
            SetForegroundWindow(w);
        }
        if (lp == WM_RBUTTONUP) {
            POINT pt; GetCursorPos(&pt);
            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING, ID_TRAY_OPEN, L"Open");
            AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"Exit");
            SetForegroundWindow(w);
            TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, w, nullptr);
            DestroyMenu(menu);
        }
        return 0;
    }
    if (m == 0x02E0 /* WM_DPICHANGED */) {
        g->dpi = (int)HIWORD(wp);
        remake_font();
        relayout();
        return 0;
    }
    if (m == WM_SYSCOMMAND && (wp & 0xFFF0) == SC_MINIMIZE) {
        tray_add();
        ShowWindow(w, SW_HIDE);
        return 0;
    }
    if (m == WM_CLOSE) {
        begin_exit(w);
        return 0;
    }
    if (m == WM_QUERYENDSESSION) {
        autostart_set_allow_child_processes(false);
        return TRUE;
    }
    if (m == WM_ENDSESSION) {
        if (wp) {
            g->exiting = true;
            autostart_set_allow_child_processes(false);
            tray_del();
            if (g->worker) g->worker->request_stop();
        }
        return 0;
    }
    if (m == WM_DESTROY) {
        tray_del();
        if (g->worker) g->worker->request_stop();
        if (g->font) { DeleteObject(g->font); g->font = nullptr; }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(w, m, wp, lp);
}

static bool pump_until(DWORD timeout_ms, const std::function<bool()>& pred) {
    DWORD start = GetTickCount();
    while (GetTickCount() - start < timeout_ms) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return pred();
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (pred()) return true;
        Sleep(10);
    }
    return pred();
}

static int gui_self_test_run(HWND w) {
    int fail = 0;
    auto boom = [&](const char* m) {
        fail++;
        OutputDebugStringA(m);
        OutputDebugStringA("\n");
        auto p = std::filesystem::path(g->dir) / L"gui_test.log";
        std::ofstream f(p, std::ios::app);
        if (f) f << m << "\n";
    };
#define TCHECK(c) do { if (!(c)) boom("FAIL " #c); } while (0)

    auto status_has = [&](const wchar_t* needle) {
        wchar_t buf[256]{};
        GetWindowTextW(g->hStatus, buf, 256);
        return wcsstr(buf, needle) != nullptr;
    };
    auto force_paint = [&] {
        UpdateWindow(w);
        RedrawWindow(w, nullptr, nullptr, RDW_UPDATENOW | RDW_ALLCHILDREN);
    };

    if (g->fake && g->fake->expect_restore) {
        TCHECK(pump_until(4000, [&] { return !g->busy && g->fake->restore_n >= 1 && g->devices.size() == 2; }));
        TCHECK(g->fake->restore_n == 1);
        TCHECK(!g->fake->used_real_hardware);
        SendMessageW(g->hAll, BM_SETCHECK, BST_UNCHECKED, 0);
        if (g->hModRb[0]) SendMessageW(g->hModRb[0], BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(w, WM_COMMAND, MAKEWPARAM(IDC_MOD0, BN_CLICKED), (LPARAM)g->hModRb[0]);
        TCHECK(SendMessageW(g->hR, TBM_GETPOS, 0, 0) == 9);
        SendMessageW(w, WM_COMMAND, MAKEWPARAM(IDC_RETRY, BN_CLICKED), (LPARAM)g->hRetry);
        TCHECK(pump_until(4000, [&] { return !g->busy && g->devices.size() == 2; }));
        TCHECK(g->fake->restore_n == 1);
        begin_exit(w);
        return fail ? 1 : 0;
    }
    if (g->fake && g->fake->expect_corrupt_profile) {
        TCHECK(pump_until(4000, [&] { return !g->busy && g->devices.size() == 2; }));
        TCHECK(g->fake->restore_n == 0);
        TCHECK(status_has(L"unreadable"));
        TCHECK(!g->fake->used_real_hardware);
        begin_exit(w);
        return fail ? 1 : 0;
    }

    TCHECK(pump_until(4000, [&] { return !g->busy && g->devices.size() == 2; }));
    TCHECK(IsWindowEnabled(g->hApplyEff));
    TCHECK(g->fake && !g->fake->used_real_hardware);
    TCHECK(g->fake->restore_n == 0);
    TCHECK(status_has(L"No saved profile"));
    force_paint();
    TCHECK(layout_radios_inside_group());
    TCHECK(g->dpi == 96 || g->dpi == 120 || g->dpi == 144 || g->dpi == 192 || g->dpi > 0);

    g->fake->devices.clear();
    SendMessageW(w, WM_COMMAND, MAKEWPARAM(IDC_RESCAN, BN_CLICKED), (LPARAM)g->hRescan);
    TCHECK(pump_until(4000, [&] { return !g->busy && g->devices.empty(); }));
    force_paint();
    TCHECK(g->nmod == 0);
    TCHECK(layout_radios_inside_group());
    TCHECK(g->fake->restore_n == 0);

    g->fake->devices = { {0x71, 0} };
    SendMessageW(w, WM_COMMAND, MAKEWPARAM(IDC_RESCAN, BN_CLICKED), (LPARAM)g->hRescan);
    TCHECK(pump_until(4000, [&] { return !g->busy && g->devices.size() == 1; }));
    force_paint();
    TCHECK(g->nmod == 1);
    TCHECK(layout_radios_inside_group());

    g->fake->devices = { {0x71, 0}, {0x72, 0} };
    SendMessageW(w, WM_COMMAND, MAKEWPARAM(IDC_RESCAN, BN_CLICKED), (LPARAM)g->hRescan);
    TCHECK(pump_until(4000, [&] { return !g->busy && g->devices.size() == 2; }));
    force_paint();
    TCHECK(g->nmod == 2);
    TCHECK(layout_radios_inside_group());

    g->fake->devices = { {0x39, 0}, {0x3A, 0} };
    SendMessageW(w, WM_COMMAND, MAKEWPARAM(IDC_RESCAN, BN_CLICKED), (LPARAM)g->hRescan);
    TCHECK(pump_until(4000, [&] { return !g->busy && g->devices.size() == 2 && g->devices[0].address7 == 0x39; }));
    TCHECK(g->fake->restore_n == 0);

    SendMessageW(g->hEff, CB_SETCURSEL, 0, 0);
    SendMessageW(w, WM_COMMAND, MAKEWPARAM(IDC_APPLYEFF, BN_CLICKED), (LPARAM)g->hApplyEff);
    TCHECK(pump_until(4000, [&] { return !g->busy && g->fake->off_n >= 1; }));

    SendMessageW(g->hEff, CB_SETCURSEL, 25, 0);
    SendMessageW(w, WM_COMMAND, MAKEWPARAM(IDC_APPLYEFF, BN_CLICKED), (LPARAM)g->hApplyEff);
    TCHECK(pump_until(4000, [&] { return !g->busy && g->fake->on_n >= 1; }));

    SendMessageW(g->hEff, CB_SETCURSEL, 5, 0);
    SendMessageW(w, WM_COMMAND, MAKEWPARAM(IDC_APPLYEFF, BN_CLICKED), (LPARAM)g->hApplyEff);
    TCHECK(pump_until(4000, [&] { return !g->busy && g->fake->last_mode == 5; }));

    SendMessageW(g->hAll, BM_SETCHECK, BST_UNCHECKED, 0);
    if (g->hModRb[1]) SendMessageW(g->hModRb[1], BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(g->hR, TBM_SETPOS, TRUE, 12);
    SendMessageW(g->hG, TBM_SETPOS, TRUE, 34);
    SendMessageW(g->hB, TBM_SETPOS, TRUE, 56);
    SendMessageW(g->hLive, BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessageW(w, WM_COMMAND, MAKEWPARAM(IDC_APPLYCOL, BN_CLICKED), (LPARAM)g->hApplyCol);
    TCHECK(pump_until(4000, [&] { return !g->busy && g->fake->color_n >= 1 && g->fake->last_addr == 0x3A; }));

    Profile prof;
    TCHECK(load_profile(prof));
    TCHECK(!prof.modules.empty());
    uint8_t saved_r = 0;
    for (auto& m : prof.modules) if (m.address == 0x3A) saved_r = m.r;
    TCHECK(saved_r == 12);

    g->fake->stall_ms = 80;
    int previews_before = g->fake->preview_n;
    SendMessageW(g->hAll, BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(g->hLive, BM_SETCHECK, BST_CHECKED, 0);
    for (int v = 10; v <= 90; v += 10) {
        SendMessageW(g->hR, TBM_SETPOS, TRUE, v);
        SendMessageW(w, WM_HSCROLL, TB_THUMBTRACK, (LPARAM)g->hR);
    }
    TCHECK(pump_until(4000, [&] { return g->fake->last_r == 90 && g->fake->preview_n > previews_before; }));
    TCHECK(g->fake->preview_n - previews_before <= 5);
    {
        Profile after;
        TCHECK(load_profile(after));
        uint8_t r3a = 0;
        for (auto& m : after.modules) if (m.address == 0x3A) r3a = m.r;
        TCHECK(r3a == 12);
    }
    SendMessageW(g->hLive, BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessageW(g->hAll, BM_SETCHECK, BST_UNCHECKED, 0);
    if (g->hModRb[0]) {
        SendMessageW(g->hModRb[0], BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(w, WM_COMMAND, MAKEWPARAM(IDC_MOD0, BN_CLICKED), (LPARAM)g->hModRb[0]);
    }
    SendMessageW(g->hR, TBM_SETPOS, TRUE, 44);
    SendMessageW(g->hG, TBM_SETPOS, TRUE, 55);
    SendMessageW(g->hB, TBM_SETPOS, TRUE, 66);
    SendMessageW(w, WM_COMMAND, MAKEWPARAM(IDC_APPLYCOL, BN_CLICKED), (LPARAM)g->hApplyCol);
    TCHECK(pump_until(4000, [&] { return !g->busy && g->fake->last_addr == 0x39; }));
    {
        Profile both;
        TCHECK(load_profile(both));
        const ModuleProfile *a = nullptr, *b = nullptr;
        for (auto& m : both.modules) {
            if (m.address == 0x39) a = &m;
            if (m.address == 0x3A) b = &m;
        }
        TCHECK(a && b);
        TCHECK(a->r == 44 && a->g == 55 && a->b == 66);
        TCHECK(b->r == 12 && b->g == 34 && b->b == 56);
        SendMessageW(g->hAll, BM_SETCHECK, BST_CHECKED, 0);
        if (g->hModRb[0]) SendMessageW(g->hModRb[0], BM_SETCHECK, BST_UNCHECKED, 0);
        SendMessageW(w, WM_COMMAND, MAKEWPARAM(IDC_ALL, BN_CLICKED), (LPARAM)g->hAll);
        TCHECK(SendMessageW(g->hEff, CB_GETCURSEL, 0, 0) == (LRESULT)CB_ERR);
    }

    g->fake->stall_ms = 0;
    g->fake->probe.status = PawnioStatus::DriverMissing;
    g->fake->probe.message = L"PawnIO kernel driver is not installed.";
    g->fake->devices.clear();
    SendMessageW(w, WM_COMMAND, MAKEWPARAM(IDC_RETRY, BN_CLICKED), (LPARAM)g->hRetry);
    TCHECK(pump_until(4000, [&] { return !g->busy && g->probe.status == PawnioStatus::DriverMissing; }));
    TCHECK(!IsWindowEnabled(g->hApplyEff));
    TCHECK(IsWindowEnabled(g->hOpen));
    TCHECK(IsWindowEnabled(g->hRetry));

    SendMessageW(w, WM_SYSCOMMAND, SC_MINIMIZE, 0);
    TCHECK(!IsWindowVisible(w) || g->tray_added);
    SendMessageW(w, WM_COMMAND, ID_TRAY_OPEN, 0);

    g->fake->probe.status = PawnioStatus::Ready;
    g->fake->probe.message = L"PawnIO kernel driver ready (fake).";
    g->fake->devices = { {0x39, 0}, {0x3A, 0} };
    SendMessageW(w, WM_COMMAND, MAKEWPARAM(IDC_RETRY, BN_CLICKED), (LPARAM)g->hRetry);
    TCHECK(pump_until(4000, [&] { return !g->busy && g->devices.size() == 2; }));
    g->fake->stall_ms = 200;
    SendMessageW(g->hEff, CB_SETCURSEL, 25, 0);
    SendMessageW(w, WM_COMMAND, MAKEWPARAM(IDC_APPLYEFF, BN_CLICKED), (LPARAM)g->hApplyEff);
    begin_exit(w);
    TCHECK(pump_until(4000, [&] { return !IsWindow(w); }));
    TCHECK(g->fake && !g->fake->used_real_hardware);
#undef TCHECK
    return fail ? 1 : 0;
}

int run_gui(HINSTANCE inst, const wchar_t* exe_dir, GuiMode mode, FakeHw* fake) {
    detach_console_for_gui();
    UiState st{};
    g = &st;
    st.dir = exe_dir;
    st.mode = mode;
    st.fake = fake;
    HwWorker worker(st.dir, fake);
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
    if (mode == GuiMode::Tray) {
        ShowWindow(w, SW_HIDE);
        tray_add();
    } else {
        ShowWindow(w, SW_SHOW);
    }
    UpdateWindow(w);
    if (mode == GuiMode::SelfTest) {
        int rc = gui_self_test_run(w);
        if (IsWindow(w)) begin_exit(w);
        MSG msg;
        DWORD start = GetTickCount();
        while (GetTickCount() - start < 3000) {
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            if (!IsWindow(w)) break;
            Sleep(10);
        }
        g = nullptr;
        return rc;
    }
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    g = nullptr;
    return (int)msg.wParam;
}

int run_tray(HINSTANCE inst, const wchar_t* exe_dir) {
    return run_gui(inst, exe_dir, GuiMode::Tray, nullptr);
}

int run_gui_self_test(HINSTANCE inst) {
    wchar_t tmp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmp);
    std::wstring dir = std::wstring(tmp) + L"xtreem_gui_test";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    SetEnvironmentVariableW(L"XTREEM_STATE_DIR", dir.c_str());
    autostart_set_allow_child_processes(false);
    FakeHw fake;
    fake.probe.status = PawnioStatus::Ready;
    fake.probe.message = L"PawnIO kernel driver ready (fake). Bundled DLL/bin are not the driver.";
    fake.devices = { {0x39, 0}, {0x3A, 0} };
    int rc = run_gui(inst, dir.c_str(), GuiMode::SelfTest, &fake);
    if (fake.used_real_hardware) rc = 1;

    {
        Profile p;
        p.modules.push_back(ModuleProfile{ 0x39, 0, "static", 1, 2, 0, 9, 8, 7 });
        save_profile(p);
        FakeHw f2;
        f2.expect_restore = true;
        f2.probe.status = PawnioStatus::Ready;
        f2.probe.message = L"PawnIO kernel driver ready (fake). Bundled DLL/bin are not the driver.";
        f2.devices = { {0x39, 0}, {0x3A, 0} };
        rc |= run_gui(inst, dir.c_str(), GuiMode::SelfTest, &f2);
        if (f2.used_real_hardware || f2.restore_n != 1) rc = 1;
    }
    {
        auto path = profile_path();
        {
            std::ofstream bad(path, std::ios::binary | std::ios::trunc);
            bad << "{not-a-profile";
        }
        FakeHw f3;
        f3.expect_corrupt_profile = true;
        f3.probe.status = PawnioStatus::Ready;
        f3.probe.message = L"PawnIO kernel driver ready (fake). Bundled DLL/bin are not the driver.";
        f3.devices = { {0x39, 0}, {0x3A, 0} };
        rc |= run_gui(inst, dir.c_str(), GuiMode::SelfTest, &f3);
        if (f3.used_real_hardware || f3.restore_n != 0) rc = 1;
    }
    return rc;
}
