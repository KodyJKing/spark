#pragma once

#include <cstdint>
#include <vector>
#include "libsm64.h"
#include "math/Vectors.hpp"
#include "engine/halo1.hpp"

namespace Mod::Mario {
    extern std::vector<Engine::WorldTransform> marioPose;

    void updateMarioPose(SM64MarioGeometryBuffers &marioGeometry);
    void drawMarioBones(SM64MarioGeometryBuffers& marioGeometry);
    void dumpSkeleton(SM64MarioGeometryBuffers &marioGeometry, Vec3 marioPos, FILE *file);
    Engine::WorldTransform getMarioBoneByName(const char *name);
    Engine::WorldTransform* getMarioBonePointerByName(const char* name);
}
