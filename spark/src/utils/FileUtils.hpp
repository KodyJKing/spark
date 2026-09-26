#pragma once

#include <functional>
#include <windows.h>

namespace FileUtils {

    // File callback type
    using FileCallback = std::function<void(const wchar_t*)>;

    inline void pickFile(FileCallback callback, OPENFILENAME& ofn) {
        // Open Windows file picker dialog, and invoke the callback with the selected file path.

        wchar_t filePath[MAX_PATH] = {0};

        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFile = filePath;
        ofn.nMaxFile = MAX_PATH;
        if (ofn.lpstrFilter == nullptr) {
            ofn.lpstrFilter = L"All Files\0*.*\0";
        }
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

        if (GetOpenFileName(&ofn)) {
            callback(filePath);
        }
    }

}
