#define SPARK_EXPORTS
#include "spark/Spark.hpp"
#include "spark/SparkHost.hpp"
#include <Windows.h>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "utils/Utils.hpp"
#include "utils/UnloadLock.hpp"
#include "math/Math.hpp"
#include "memory/Memory.hpp"
#include "engine/halo1.hpp"
#include "engine/decomp/index.hpp"
#include "mods/freecam/FreecamMod.hpp"
#include "spark/hook/Hooks.hpp"
#include "spark/mod/ModRegistry.hpp"

namespace Spark {
    bool showDebugOverlay = false;
}
#include "spark/mod/ModLoader.hpp"
#include "mods/devtools/DevToolsMod.hpp"
#include "mods/hooklog/HookLogMod.hpp"

namespace Spark {

    uintptr_t halo1 = 0;
    ModRegistry registry;
    ModLoader modLoader;

    bool isInstalled = false;

    void init() {
        if (isInstalled)
            return;
        isInstalled = true;

        const std::string moduleName = "halo1.dll";
        halo1 = (uintptr_t) Utils::waitForModule(moduleName);
        std::cout << moduleName << ": " << (void*) halo1 << std::endl;

        registry.add(new FreecamMod());
        registry.add(new DevToolsMod());
        registry.add(new HookLogMod());
        // modLoader.loadAll(); // LoadLibrary's *.dll from the mods directory (e.g. smc64.dll), calling spark_registerMod() for each.
        registry.initAll(halo1);

        Engine::Decomp::install(halo1);

        std::cout << "Mods installed." << std::endl;
    }

    void free() {
        if (!isInstalled)
            return;
        isInstalled = false;

        Engine::Decomp::uninstall(halo1);

        registry.freeAll();
        modLoader.unloadAll(); // FreeLibrary each mod DLL only after its IMod::free() has already run above.

        std::cout << "Mods uninstalled." << std::endl;
    }

    void modThreadUpdate() {
        if (Engine::isGameLoaded()) {
            init();
        } else if (isInstalled) {
            SparkHost::reinitialize();
        }
    }

}
