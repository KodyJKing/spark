#pragma once

#include <stdint.h>
#include "math/Vectors.hpp"

namespace Engine {

    struct SpawnObjectArgs {
        uint32_t objectTagId;
        uint32_t unknown1;
        uint32_t unknown2;
        uint32_t unknown3;
        uint32_t ownerEntityHandle;
        uint32_t unknown4;
        uint32_t unknown5;
        Vec3 spawnPosition;
    };

}
