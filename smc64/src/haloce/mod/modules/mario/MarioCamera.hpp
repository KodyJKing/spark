#pragma once

#include "math/Vectors.hpp"

namespace HaloCE::Mod::Mario::MarioCamera {
    void registerHandlers();
    void onUpdate(Vec3 marioWorldPos);
    void onDisable();
}
