// Disable picking for Mario model.

#include "MarioPickingFix.hpp"
#include "MarioModel.hpp"
#include "spark/hook/Hooks.hpp"

namespace Mod::Mario::MarioPickingFix {

    void registerHandlers(Spark::ModId modId) {
        Spark::TryPickInteractable::addHandler(modId, +[](void* /*ctx*/, auto next, uint16_t p1, int16_t p2, uint32_t candidateEntityHandle, int16_t p4) {
            if (MarioModel::isMario(candidateEntityHandle)) {
                return; // Suppress picking for the Mario model.
            }
            next(p1, p2, candidateEntityHandle, p4);
        }, nullptr);
    }

}

