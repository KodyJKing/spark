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

namespace HaloCE::Mod {

    uintptr_t halo1 = 0;

    //////////////////////////////////////////////////////////////////
    // Hooks
    
    typedef void (*updateAllEntities_t)( void );
    updateAllEntities_t originalUpdateAllEntities = nullptr;
    //
    void hkUpdateAllEntities() {
        UnloadLock lock; // No unloading while we're still executing hook code.

        Overlay::ESP::VectorProfiler::start(GetCurrentThreadId());

        Freecam::update();
        Mario::update();

        originalUpdateAllEntities();
    }
    
    typedef uint64_t (*updateEntity_t)( uint32_t entityHandle );
    updateEntity_t originalUpdateEntity = nullptr;
    //
    uint64_t hkUpdateEntity( uint32_t entityHandle ) {
        UnloadLock lock; // No unloading while we're still executing hook code.

        auto rec = Halo1::getEntityRecord( entityHandle );
        if (!rec)  return originalUpdateEntity( entityHandle);
            
        auto entity = rec->entity();
        if (!entity) return originalUpdateEntity( entityHandle );

        if (settings.freezeTime) {
            auto playerRec = Halo1::getPlayerRecord();
            if (playerRec && rec->id != playerRec->id) {
                return 0;
            }
        }

        // Do update
        uint64_t result = originalUpdateEntity( entityHandle );

        // Mario::MarioModel::processEntity( entity );

        return result;
    }

    typedef void (*updateWorldBones_t)(uint32_t entityHandle);
    updateWorldBones_t originalUpdateWorldBones = nullptr;
    //
    void hkUpdateWorldBones(uint32_t entityHandle) {
        UnloadLock lock; // No unloading while we're still executing hook code.

        auto rec = Halo1::getEntityRecord( entityHandle );
        if (!rec)  return originalUpdateWorldBones( entityHandle);
            
        auto entity = rec->entity();
        if (!entity) return originalUpdateWorldBones( entityHandle );

        originalUpdateWorldBones( entityHandle );

        Mario::MarioModel::processEntity( entityHandle, entity );
    }

    // renderEntity
    Halo1::renderEntity_t originalRenderEntity = nullptr;
    void hkRenderEntity(Halo1::RenderEntityRequest* request) {
        UnloadLock lock; // No unloading while we're still executing hook code.

        if (GetAsyncKeyState(VK_F10)) {
            return originalRenderEntity(request);
        }

         auto rec = Halo1::getEntityRecord( request->entityHandle );

        originalRenderEntity(request);

        Mario::MarioModel::renderEntity(request, originalRenderEntity);
    }

    void hookFunctions() {
        std::cout << "\nHooking functions:\n" << std::endl;

        #define HOOK_FUNC( func, offset) \
            void* p##func = (void*) (halo1 + offset); \
            std::cout << #func << ": " << std::endl; \
            std::cout << AsmHelper::disassemble( (uint8_t*) p##func, 0x100 ) << std::endl; \
            MH_CreateHook( p##func, hk##func, (void**) &original##func ); \
            MH_EnableHook( p##func );

        HOOK_FUNC( UpdateEntity, 0xB3A06CU );
        HOOK_FUNC( UpdateAllEntities, 0xB35654U );
        HOOK_FUNC( UpdateWorldBones, 0xB3A614U );
        HOOK_FUNC( RenderEntity, RENDER_ENTITY_FUNC_OFFSET );

        #undef HOOK_FUNC
    }

    void unhookFunctions() {
        std::cout << "\nUnhooking functions." << std::endl;
        MH_RemoveHook( (void*) originalUpdateEntity );
        MH_RemoveHook( (void*) originalUpdateAllEntities );
        MH_RemoveHook( (void*) originalUpdateWorldBones );
        MH_RemoveHook( (void*) originalRenderEntity );
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

        hookFunctions();

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

        unhookFunctions();

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
