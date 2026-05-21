// Disable picking for Mario model.

#include "MarioPickingFix.hpp"
#include "MarioModel.hpp"
#include "../common.hpp"

namespace HaloCE::Mod::Mario::MarioPickingFix {

    typedef void (*tryPickInteractable_t)(uint16_t param_1, int16_t param_2, uint32_t candidateEntityHandle, int16_t param_4);
    tryPickInteractable_t originalTryPickInteractable = nullptr;

    void hkTryPickInteractable(uint16_t param_1, int16_t param_2, uint32_t candidateEntityHandle, int16_t param_4) {
        UnloadLock lock;

        if (MarioModel::isMario(candidateEntityHandle)) {
            return; // Suppress picking for the Mario model.
        }

        originalTryPickInteractable(param_1, param_2, candidateEntityHandle, param_4);
    }

    void init(uintptr_t halo1) {
        HOOK_FUNC( TryPickInteractable, 0xAD559CU );
    }

    void free() {
        MH_RemoveHook( (void*) originalTryPickInteractable );
    }

}

