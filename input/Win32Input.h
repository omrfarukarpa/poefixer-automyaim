#pragma once

#include <Windows.h>

namespace AutoMyAimInput {

// Absolute virtual-desktop SendInput move (DPI-correct regardless of the
// process's DPI-awareness). Same approach as the sibling loot plugin.
inline void MoveCursorScreen(int x, int y) {
    const int vsX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int vsY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vsW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vsH = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (vsW <= 0) vsW = 1;
    if (vsH <= 0) vsH = 1;

    const LONG nx = static_cast<LONG>(
        ((static_cast<long long>(x - vsX) * 65535) + (vsW - 1) / 2) / (vsW - 1 > 0 ? vsW - 1 : 1));
    const LONG ny = static_cast<LONG>(
        ((static_cast<long long>(y - vsY) * 65535) + (vsH - 1) / 2) / (vsH - 1 > 0 ? vsH - 1 : 1));

    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dx = nx;
    in.mi.dy = ny;
    in.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    SendInput(1, &in, sizeof(INPUT));
}

inline bool IsKeyDown(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

}
