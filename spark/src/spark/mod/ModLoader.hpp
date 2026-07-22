#pragma once
#include <Windows.h>
#include <vector>

namespace Spark {

    // Loads every *.dll in Utils::getModsDirectory() (<exe-dir>/mods/) plus any extra
    // directories listed in the SPARK_MODS_PATH environment variable (semicolon-delimited,
    // like PATH), calling each one's exported spark_modLoad() entry point (if present).
    // Call loadAll() after registering any built-in mods but before ModRegistry::initAll(),
    // so loaded mods are included in the same registerHandlers -> installAllHooks -> init pass.
    //
    // Teardown ordering matters: ModRegistry::freeAll() must run before unloadAll(),
    // so a mod's free() still has a valid module to run in. See Spark::free().
    class ModLoader {
    public:
        void loadAll();
        void unloadAll();

    private:
        std::vector<HMODULE> modules_;
    };

}
