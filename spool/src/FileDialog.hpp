#pragma once

#include <string>
#include <windows.h>

namespace Spool {
    // Shows the native "Open" dialog filtered to .map files. Returns an empty string if cancelled.
    std::wstring pickMapFile(HWND owner);
}
