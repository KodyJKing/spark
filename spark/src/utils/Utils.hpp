#pragma once

#include <Windows.h>
#include <string>
#include <filesystem>

namespace Utils {
    HMODULE waitForModule(std::string moduleName, DWORD sleepTime = 100, DWORD timeout = 0);
    bool isInjected();

    // The mods directory, resolved relative to the running process' own image
    // (GetModuleFileNameA(NULL, ...)), NOT this DLL's own module path. Spark itself may be
    // injected from an arbitrary location (e.g. the repo's build output during dev), but
    // mods must always be found next to the actual game executable.
    std::filesystem::path getModsDirectory();
}
