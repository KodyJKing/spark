#pragma once
#include <Windows.h>

namespace Spark::Overlay {
        HWND getGameWindow();
        void initializeContext(HWND targetWindow);
        bool hasInputCapture();
        void render();
        void init();
        void free();
}