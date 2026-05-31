#pragma once

#define ENABLE_MARIO 1

#include "MarioState.hpp"
#include "MarioModel.hpp"
#include "math/Vectors.hpp"
#include "spark/mod/ModId.hpp"

namespace HaloCE::Mod::Mario {
    void init(Spark::ModId modId);
    void free();
    void update();
    void debugRender();

    // Helper: create a temporary spawn platform below Mario's spawn point.
    void createSpawnPlatform(const Vec3& localPos);

    // Mario's true world position in mario units (marioChunk origin + local pos).
    Vec3 marioWorldPositionMario();
    // Mario's true world position in halo units.
    Vec3 marioWorldPosition();
    // Mario's velocity in halo units.
    Vec3 marioWorldVelocity();
}
