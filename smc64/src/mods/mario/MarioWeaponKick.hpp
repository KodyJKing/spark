#pragma once

#include "spark/mod/ModId.hpp"

namespace HaloCE::Mod::Mario::MarioWeaponKick {
    // Registers the SpawnProjectile hook that detects player shots and applies kick.
    void registerHandlers(Spark::ModId modId);
}
