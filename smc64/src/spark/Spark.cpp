#include "spark/Spark.hpp"
#include "spark/SparkHost.hpp"
#include <Windows.h>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "MinHook.h"
#include "utils/Utils.hpp"
#include "utils/UnloadLock.hpp"
#include "math/Math.hpp"
#include "memory/Memory.hpp"
#include "engine/halo1.hpp"
#include "mods/freecam/FreecamMod.hpp"
#include "spark/hook/Hooks.hpp"
#include "spark/mod/ModRegistry.hpp"
#include "mods/devtools/DevToolsMod.hpp"
#include "mods/mario/MarioMod.hpp"

namespace Spark {

    uintptr_t halo1 = 0;
    ModRegistry registry;

    bool isInstalled = false;

    void init() {
        if (isInstalled)
            return;
        isInstalled = true;

        const std::string moduleName = "halo1.dll";
        halo1 = (uintptr_t) Utils::waitForModule(moduleName);
        std::cout << moduleName << ": " << (void*) halo1 << std::endl;

        registry.add(std::make_unique<FreecamMod>());
        registry.add(std::make_unique<DevToolsMod>());
        registry.add(std::make_unique<MarioMod>());
        registry.initAll(halo1);

        std::cout << "Mods installed." << std::endl;
    }

    void free() {
        if (!isInstalled)
            return;
        isInstalled = false;

        registry.freeAll();

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
