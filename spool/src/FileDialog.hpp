#pragma once

#include <string>
#include <filesystem>
#include <vector>
#include <windows.h>

namespace Spool {
    // Native map dialogs return an empty string on cancellation and throw on errors.
    std::wstring pickMapFile(HWND owner);
    std::vector<std::filesystem::path> pickMapFiles(HWND owner);
    std::wstring pickMapSaveFile(HWND owner, const std::filesystem::path& suggestedPath);
}
