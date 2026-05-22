// Disable picking for Mario model.

#include "MarioPickingFix.hpp"
#include "MarioModel.hpp"
#include "hook/Hooks.hpp"

namespace HaloCE::Mod::Mario::MarioPickingFix {

    void registerHandlers(ModId modId) {
        TryPickInteractable::addHandler(modId, +[](void* /*ctx*/, TryPickInteractable::Cursor next, uint16_t p1, int16_t p2, uint32_t candidateEntityHandle, int16_t p4) {
            if (MarioModel::isMario(candidateEntityHandle)) {
                return; // Suppress picking for the Mario model.
            }
            next(p1, p2, candidateEntityHandle, p4);
        }, nullptr);
    }

}

