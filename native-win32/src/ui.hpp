#pragma once
#include <string>
#include <windows.h>
int run_gui(HINSTANCE inst, const wchar_t* exe_dir, bool smoke);
int run_tray(HINSTANCE inst, const wchar_t* exe_dir);
std::wstring exe_directory();
void open_pawnio_site();
