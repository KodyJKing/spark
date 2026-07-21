#pragma once

#include <cstdint>
#include <vector>
#include "libsm64.h"
#include "math/Vectors.hpp"
#include "engine/halo1.hpp"

namespace Mod::Mario {
    extern std::vector<Engine::Transform> marioPose;
    void updateMarioPose(SM64Matrix4f* marioBoneMatrices);
    Engine::Transform getMarioBoneByName(const char *name);
    Engine::Transform* getMarioBonePointerByName(const char* name);
}
