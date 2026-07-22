#pragma once

#include "math/Vectors.hpp"
#include "imgui.h"
#include "spark/SparkAPI.h"
#include <string>

/**
 * This namespace contains functions and utilities for rendering gizmos
 * within the Spark overlay. Gizmos are visual aids used for debugging
 * and visualization purposes in the 3D environment.
 * 
 * They offer means to queue debug draws in a way that is decoupled from the render thread.
 */
namespace Spark::Overlay::Gizmos {
    SPARK_API void drawLine(Vec3& start, Vec3& end, ImU32 color, uint32_t durationFrames);
    SPARK_API void drawPoint(Vec3& position, ImU32 color, uint32_t durationFrames);
    SPARK_API void drawText(Vec3& center, const char* text, ImU32 color, uint32_t durationFrames);

    // Flush all queued gizmos through ESP, then age them and drop expired ones.
    // Must be called on the render thread between ESP::beginESPWindow()/endESPWindow(),
    // after ESP::updateCamera(), once per frame.
    SPARK_API void render();
}
