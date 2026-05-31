#pragma once

#include "libsm64.h"
#include "math/Vectors.hpp"

namespace HaloCE::Mod::Mario::DynamicGeometry {
    void update(SM64MarioState& marioState);
    void onLoadedChunkChanged(Vec3i oldChunk, Vec3i newChunk);
    void free();
}
