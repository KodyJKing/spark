#include "../common.hpp"
#include "MarioModel.hpp"

namespace HaloCE::Mod::Mario::MarioModel::LightingFix {

    typedef void (*updateEntityLighting_t)(uint32_t lightHandle, uint32_t entityHandle, float param_3, char forceUpdate);
    updateEntityLighting_t originalUpdateEntityLighting = nullptr;
    void hkUpdateEntityLighting(uint32_t lightHandle, uint32_t entityHandle, float param_3, char forceUpdate) {
        UnloadLock lock; // No unloading while we're still executing hook code.
        if (isMario(entityHandle)) forceUpdate = 1;
        originalUpdateEntityLighting(lightHandle, entityHandle, param_3, forceUpdate);
    }

    void init(uintptr_t halo1) {
        // Hook entity lighting update function
        HOOK_FUNC( UpdateEntityLighting, 0xB494A0U );
    }

    void free() {
        // Unhook entity lighting update function
        MH_RemoveHook( (void*) originalUpdateEntityLighting );
    }

}
