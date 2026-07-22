#pragma once

#define ENABLE_MARIO 1

#include "MarioState.hpp"
#include "MarioModel.hpp"
#include "functions/MarioWorldPosition.hpp"
#include "math/Vectors.hpp"
#include "spark/mod/ModId.hpp"

namespace Mod::Mario {
    
    void init(Spark::ModId modId);
    void free();
    void update();
    void debugRender();
    void deinitMario();
    
    // Helper: create a temporary spawn platform below Mario's spawn point.
    void createSpawnPlatform(const Vec3& localPos);


}
