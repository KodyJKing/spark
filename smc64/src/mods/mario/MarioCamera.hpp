#pragma once

#include "math/Vectors.hpp"
#include "mod/ModId.hpp"

namespace HaloCE::Mod::Mario::MarioCamera {
    void registerHandlers(ModId modId);
    void onUpdate(Vec3 marioWorldPos);
    void onDisable();
}
