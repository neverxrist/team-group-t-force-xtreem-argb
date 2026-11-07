#pragma once
#include <string>

enum class CliKind {
    Gui,
    Tray,
    GuiTest,
    SilentHw,       // off/on/toggle/restore: no new console
    InteractiveCli  // detect/status/debug/help/autostart: console if none
};

inline std::wstring cli_first_token(std::wstring cl) {
    for (auto& c : cl) if (c >= L'A' && c <= L'Z') c = (wchar_t)(c + 32);
    // drop --diag anywhere so `exe --diag` still launches the GUI
    for (;;) {
        auto p = cl.find(L"--diag");
        if (p == std::wstring::npos) break;
        cl.erase(p, 6);
    }
    while (!cl.empty() && cl[0] == L' ') cl.erase(0, 1);
    auto sp = cl.find(L' ');
    std::wstring tok = sp == std::wstring::npos ? cl : cl.substr(0, sp);
    if (tok.size() >= 2 && tok.front() == L'"') tok = tok.substr(1, tok.size() - 2);
    while (!tok.empty() && tok.back() == L' ') tok.pop_back();
    return tok;
}

inline CliKind classify_cli_token(const std::wstring& tok) {
    if (tok.empty() || tok == L"gui" || tok == L"ui") return CliKind::Gui;
    if (tok == L"tray") return CliKind::Tray;
    if (tok == L"--gui-test" || tok == L"--gui-smoke") return CliKind::GuiTest;
    if (tok == L"off" || tok == L"on" || tok == L"toggle" || tok == L"restore")
        return CliKind::SilentHw;
    return CliKind::InteractiveCli;
}
