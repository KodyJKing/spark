#pragma once

#include <vector>
#include <cstdint>

#include "math/Vectors.hpp"
#include "spark/mod/ModId.hpp"
#include "libsm64.h"

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
        Vec3 orientation; // Euler angles in degrees (YXZ intrinsic: yaw, pitch, roll)
    };

    struct LevelEdits {
        std::vector<OrientedBoundingBox> orientedBoundingBoxes;
    };

    struct LevelEditContext {
        uint64_t bspSignature = 0;
    };

    // Returns a pointer to the static LevelEdits for this context, or nullptr.
    // Caller does not own the returned pointer.
    LevelEdits* getLevelEdits(LevelEditContext& context);

    // Call this whenever the active level changes (e.g. on BSP load).
    // Sets the current context and refreshes the internal LevelEdits pointer.
    void setContext(LevelEditContext context);

    // Registers render and UI handlers. Must be called from the mod's init().
    void initHandlers(Spark::ModId modId);

    // Returns true when the level editor is open and an ImGui window has focus,
    // indicating that game input should be suppressed.
    bool isInputSuppressed();

    // Get a vector of static surfaces to add the the given BSP.
    std::vector<SM64Surface> getStaticSurfaces(uint64_t bspSignature);

    // Get a vector of static surfaces to add to the current BSP.
    std::vector<SM64Surface> getStaticSurfaces();
}
