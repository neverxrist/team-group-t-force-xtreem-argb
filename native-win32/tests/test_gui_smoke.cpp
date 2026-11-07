// Real main-window smoke with FakeHw. Never opens PawnIO/SMBus.
#define UNICODE
#include "ui.hpp"

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
    return run_gui_self_test(inst);
}
