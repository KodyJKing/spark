#pragma once

#include <Windows.h>
#include <stdexcept>
#include <string>

// Loads halo1.dll standalone, outside the MCC host process, so that
// self-contained ("pure") functions can be called directly with synthetic
// inputs and their output compared against hand-derived expected values.
//
// This does NOT initialize the game engine. Only call functions that don't
// depend on live game-world state (entity tables, loaded map/BSP data,
// globals populated by the normal init path, etc). Verify a function is
// safe to call this way by decompiling it in Ghidra first and checking
// what it reads besides its own parameters and .rdata constants.
class Halo1Module {
public:
    // Matches the install path used by scripts/install_package.ps1 and
    // scripts/build_libsm64.ps1.
    static constexpr const char* kDefaultPath =
        R"(C:\Program Files (x86)\Steam\steamapps\common\Halo The Master Chief Collection\halo1\halo1.dll)";

    explicit Halo1Module(const std::string& path = kDefaultPath) {
        // halo1.dll depends on bink2w64.dll, which lives alongside it but
        // is not on the default DLL search path when loading by full path
        // from a different directory. AddDllDirectory + the SEARCH_USER_DIRS
        // flag resolves it without touching the process-wide search order.
        std::string dir = path.substr(0, path.find_last_of("\\/"));
        std::wstring wdir(dir.begin(), dir.end());
        AddDllDirectory(wdir.c_str());

        m_module = LoadLibraryExA(
            path.c_str(),
            nullptr,
            LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS);

        if (!m_module) {
            throw std::runtime_error(
                "Failed to load halo1.dll, GetLastError=" + std::to_string(GetLastError()));
        }
    }

    ~Halo1Module() {
        if (m_module) {
            FreeLibrary(m_module);
        }
    }

    Halo1Module(const Halo1Module&) = delete;
    Halo1Module& operator=(const Halo1Module&) = delete;

    uintptr_t base() const { return reinterpret_cast<uintptr_t>(m_module); }

    // Resolves a module-relative offset (as reported by
    // .github/agents/scripts/ghidra_offset.py) to a callable function
    // pointer of the given type, e.g.:
    //
    //   using LineTestVsBSPFn = bool(void* bsp, Vec3* origin, Vec3* dir, float* outT);
    //   auto fn = halo1.at<LineTestVsBSPFn>(0xC746E8);
    template <typename Fn>
    Fn* at(uintptr_t offset) const {
        return reinterpret_cast<Fn*>(base() + offset);
    }

private:
    HMODULE m_module = nullptr;
};
