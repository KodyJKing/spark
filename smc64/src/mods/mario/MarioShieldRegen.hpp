#pragma once

#include "engine/halo1.hpp"

namespace Mod::Mario {
    void regenerateShield(Engine::Entity* player, float amount, bool allowOvershield);

    void updateShieldRegen(Engine::Entity* player);
}
