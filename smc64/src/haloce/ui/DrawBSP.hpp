#pragma once

#include "engine/halo1.hpp"

namespace HaloCE::Mod::UI  {
    void drawBSPPoints(Engine::CollisionBSP* bsp, Vec3 origin, Vec3 x, Vec3 y, Vec3 z);
    void drawBSP(Engine::CollisionBSP* bsp, Vec3 origin, Vec3 x, Vec3 y, Vec3 z);
}
