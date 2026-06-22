#pragma once

#include "math/Vectors.hpp"
#include "spark/mod/ModId.hpp"

namespace Mod::Mario::MarioCamera {
    void registerHandlers(Spark::ModId modId);
    void onUpdate(Vec3 marioWorldPos);
    void onDisable();
}
