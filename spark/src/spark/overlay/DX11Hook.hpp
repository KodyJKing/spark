#pragma once
#include <Windows.h>

namespace Spark::Overlay::DX11Hook {
    HWND findMainWindow();
    
    void hook(HWND hwnd);
    void unhook();
}