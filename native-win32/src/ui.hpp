#pragma once
#include "hw_worker.hpp"
#include <string>
#include <windows.h>

enum class GuiMode { Normal, Tray, SelfTest };

int run_gui(HINSTANCE inst, const wchar_t* exe_dir, GuiMode mode, FakeHw* fake = nullptr);
int run_tray(HINSTANCE inst, const wchar_t* exe_dir);
int run_gui_self_test(HINSTANCE inst);
std::wstring exe_directory();
void open_pawnio_site();
void detach_console_for_gui();
