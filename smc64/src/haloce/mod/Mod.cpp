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
#include "../halo1/halo1.hpp"
#include "DllMain.hpp"
#include "modules/mario/Mario.hpp"
#include "modules/freecam.hpp"
#include "overlay/VectorProfiler.hpp"
#include "hook/Hooks.hpp"

namespace HaloCE::Mod {

    uintptr_t halo1 = 0;

    //////////////////////////////////////////////////////////////////
    // Hook handlers

    void registerHandlers() {
        UpdateAllEntities::addHandler([](UpdateAllEntities::Next next) {
            Overlay::ESP::VectorProfiler::start(GetCurrentThreadId());
            Freecam::update();
            Mario::update();
            next();
        });

        UpdateEntity::addHandler([](UpdateEntity::Next next, uint32_t entityHandle) -> uint64_t {
            auto rec = Halo1::getEntityRecord(entityHandle);
            if (!rec) return next(entityHandle);
            auto entity = rec->entity();
            if (!entity) return next(entityHandle);
            if (settings.freezeTime) {
                auto playerRec = Halo1::getPlayerRecord();
                if (playerRec && rec->id != playerRec->id) return 0;
            }
            return next(entityHandle);
        });

        UpdateWorldBones::addHandler([](UpdateWorldBones::Next next, uint32_t entityHandle) {
            auto rec = Halo1::getEntityRecord(entityHandle);
            if (!rec) return next(entityHandle);
            auto entity = rec->entity();
            if (!entity) return next(entityHandle);
            next(entityHandle);
            Mario::MarioModel::processEntity(entityHandle, entity);
        });

        RenderEntity::addHandler([](RenderEntity::Next next, Halo1::RenderEntityRequest* request) {
            if (GetAsyncKeyState(VK_F10)) {
                return next(request);
            }
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
        SparkLoader::installAllHooks(halo1);

        Freecam::init( halo1 );
        Mario::init();

        std::cout << "Mod installed." << std::endl;
    }

    void free() {
        if (!isInstalled)
            return;
        isInstalled = false;

        Mario::free();
        Freecam::free();

        Overlay::ESP::VectorProfiler::stop();

        SparkLoader::uninstallAllHooks();

        std::cout << "Mod uninstalled." << std::endl;
    }

    // Called by mod dll's thread regularly.
    void modThreadUpdate() {
        if (Halo1::isGameLoaded()) {
            init();
        } else if (isInstalled) {
            ModHost::reinitialize();
        }
    }

}
