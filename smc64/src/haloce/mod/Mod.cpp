#include "Mod.hpp"
#include <Windows.h>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "MinHook.h"
#include "utils/Utils.hpp"
#include "utils/UnloadLock.hpp"
#include "math/Math.hpp"
#include "asm/AsmHelper.hpp"
#include "memory/Memory.hpp"
#include "engine/halo1.hpp"
#include "DllMain.hpp"
#include "modules/mario/Mario.hpp"
#include "mods/freecam/FreecamMod.hpp"
#include "overlay/VectorProfiler.hpp"
#include "hook/Hooks.hpp"
#include "mod/ModRegistry.hpp"

namespace HaloCE::Mod {

    uintptr_t halo1 = 0;
    ModRegistry registry;

    //////////////////////////////////////////////////////////////////
    // Hook handlers

    void registerHandlers() {
        UpdateAllEntities::addHandler(0, [](UpdateAllEntities::Next next) {
            Overlay::ESP::VectorProfiler::start(GetCurrentThreadId());
            Mario::update();
            next();
        });

        UpdateEntity::addHandler(0, [](UpdateEntity::Next next, uint32_t entityHandle) -> uint64_t {
            auto rec = Engine::getEntityRecord(entityHandle);
            if (!rec) return next(entityHandle);
            auto entity = rec->entity();
            if (!entity) return next(entityHandle);
            if (settings.freezeTime) {
                auto playerRec = Engine::getPlayerRecord();
                if (playerRec && rec->id != playerRec->id) return 0;
            }
            return next(entityHandle);
        });

        UpdateWorldBones::addHandler(0, [](UpdateWorldBones::Next next, uint32_t entityHandle) {
            auto rec = Engine::getEntityRecord(entityHandle);
            if (!rec) return next(entityHandle);
            auto entity = rec->entity();
            if (!entity) return next(entityHandle);
            next(entityHandle);
            Mario::MarioModel::processEntity(entityHandle, entity);
        });

        RenderEntity::addHandler(0, [](RenderEntity::Next next, Engine::RenderEntityRequest* request) {
            next(request);
            Mario::MarioModel::renderEntity(request, RenderEntity::original);
        });
    }

    //////////////////////////////////////////////////////////////////

    bool isInstalled = false;

    void init() {
        if (isInstalled)
            return;
        isInstalled = true;

        const std::string moduleName = "halo1.dll";
        halo1 = (uintptr_t) Utils::waitForModule(moduleName);
        std::cout << moduleName << ": " << (void*) halo1 << std::endl;

        registerHandlers();
        registry.add(std::make_unique<FreecamMod>());
        registry.initAll(halo1);
        Mario::init();

        std::cout << "Mod installed." << std::endl;
    }

    void free() {
        if (!isInstalled)
            return;
        isInstalled = false;

        Mario::free();

        Overlay::ESP::VectorProfiler::stop();

        registry.freeAll();

        std::cout << "Mod uninstalled." << std::endl;
    }

    // Called by mod dll's thread regularly.
    void modThreadUpdate() {
        if (Engine::isGameLoaded()) {
            init();
        } else if (isInstalled) {
            ModHost::reinitialize();
        }
    }

}
