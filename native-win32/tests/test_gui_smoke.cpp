// GUI smoke only: create/destroy a Win32 window. Does not open PawnIO or SMBus.
#define UNICODE
#include <windows.h>

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = inst;
    wc.lpszClassName = L"XtreemGuiSmoke";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    if (!RegisterClassW(&wc)) return 1;
    HWND w = CreateWindowExW(0, wc.lpszClassName, L"XTREEM GUI smoke (no hardware)",
                             WS_OVERLAPPEDWINDOW, 40, 40, 320, 120, nullptr, nullptr, inst, nullptr);
    if (!w) return 2;
    ShowWindow(w, SW_SHOW);
    UpdateWindow(w);
    MSG msg;
    DWORD start = GetTickCount();
    while (GetTickCount() - start < 300) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        Sleep(10);
    }
    DestroyWindow(w);
    return 0;
}
