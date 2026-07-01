#pragma once

#include "math/Vectors.hpp"

namespace Mod::Mario {
    // Mario's true world position in mario units (marioChunk origin + local pos).
    Vec3 marioWorldPositionMario();
    // Mario's true world position in halo units.
    Vec3 marioWorldPosition();
    // Mario's velocity in halo units.
    Vec3 marioWorldVelocity();
}
