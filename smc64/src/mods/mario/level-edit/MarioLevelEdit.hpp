#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "math/Vectors.hpp"

/**
 * This module gives us means for tweaking levels to resolve some edge cases for Mario's movement and interactions within the levels.
 * For now it just lets us add oriented collision bounding boxes to the levels.
 * When ENABLE_LEVEL_EDITOR is off, this module just hosts edits without any UI for editing them.
 * When ENABLE_LEVEL_EDITOR is on, this module provides a UI for adding and modifying these oriented bounding boxes within the levels.
 */

namespace Mod::Mario::LevelEdit {

    struct OrientedBoundingBox {
        Vec3 center;
        Vec3 halfExtents;
        Vec3 orientation; // Euler angles in degrees
    };

    struct LevelEdits {
        std::vector<OrientedBoundingBox> orientedBoundingBoxes;
    };

    struct LevelEditContext {
        std::string levelName;
        uint64_t bspSignature;
    };

    LevelEdits* getLevelEdits(LevelEditContext& context);

    void initHandlers();

}
