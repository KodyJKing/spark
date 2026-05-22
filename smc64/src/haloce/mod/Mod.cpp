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

    typedef void (*damageEntity_t)(uint32_t entityHandle, uint16_t param_2, uint16_t param_3, uint64_t param_4, uint8_t* param_5, void* param_6, uint64_t param_7, uint64_t param_8, uint32_t* param_9, float* param_10, uint32_t* param_11, float damage, char param_13);
    damageEntity_t originalDamageEntity = nullptr;
    //
    void hkDamageEntity(uint32_t entityHandle, uint16_t param_2, uint16_t param_3, uint64_t param_4, uint8_t* param_5, void* param_6, uint64_t param_7, uint64_t param_8, uint32_t* param_9, float* param_10, uint32_t* param_11, float damage, char param_13) {
        UnloadLock lock; // No unloading while we're still executing hook code.
        
        if (!settings.enableTimeScale)
            return originalDamageEntity(entityHandle, param_2, param_3, param_4, param_5, param_6, param_7, param_8, param_9, param_10, param_11, damage, param_13);
        
        auto rec = Halo1::getEntityRecord( entityHandle );
        if (!rec)
            return originalDamageEntity(entityHandle, param_2, param_3, param_4, param_5, param_6, param_7, param_8, param_9, param_10, param_11, damage, param_13);

        if (rec->typeId == Halo1::TypeID_Player)
            damage *= settings.playerDamageScale;
        else
            damage *= settings.npcDamageScale;


        float oldHealth = 0.0f;
        if (rec->entity())
            oldHealth = rec->entity()->health;

        originalDamageEntity(entityHandle, param_2, param_3, param_4, param_5, param_6, param_7, param_8, param_9, param_10, param_11, damage, param_13);

        float newHealth = 0.0f;
        if (rec->entity())
            newHealth = rec->entity()->health;
        
        if (newHealth <= 0.0 && oldHealth > 0.0) {
            std::cout << "Entity " << entityHandle << " died." << std::endl;
        }
    }

    typedef float(*getShieldDamageResist_t)(uint32_t entityHandle, bool useExtraScalar);
    getShieldDamageResist_t originalGetShieldDamageResist = nullptr;
    //
    float hkGetShieldDamageResist(uint32_t entityHandle, bool useExtraScalar) {
        UnloadLock lock; // No unloading while we're still executing hook code.

        auto rec = Halo1::getEntityRecord( entityHandle );
        if (!rec)
            return originalGetShieldDamageResist(entityHandle, useExtraScalar);

        float result = originalGetShieldDamageResist(entityHandle, useExtraScalar);
        if (rec->typeId == Halo1::TypeID_Player)
            result /= settings.playerDamageScale;
        else
            result /= settings.npcDamageScale;

        return result;
    }

    // updateActor updates AIs.
    typedef void (*updateActor)(uint64_t actorHandle);
    updateActor originalUpdateActor = nullptr;
    //
    void hkUpdateActor(uint64_t actorHandle) {
        UnloadLock lock; // No unloading while we're still executing hook code.
        // ...
        originalUpdateActor(actorHandle);
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
