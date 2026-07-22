// Utilities for drawing things in world space.

#pragma once

#include "math/Vectors.hpp"
#include "imgui.h"
#include "spark/SparkAPI.h"

namespace Spark::Overlay::ESP {
    SPARK_API extern Camera camera;

    // Sync ESP::camera from the player camera. Call once per frame before any ESP draws.
    SPARK_API void updateCamera();

    SPARK_API void updateScreenSize();

    SPARK_API Vec3 worldToScreen(Vec3 worldPos);

    SPARK_API void drawPoint(Vec3 pos, ImU32 color);

    SPARK_API void drawLine(Vec3 start, Vec3 end, ImU32 color);

    SPARK_API void drawBox(Vec3 center, Vec3 size, ImU32 color);

    SPARK_API void drawCircle(Vec3 center, float radius, ImU32 color, bool perspective = true, bool filled = false);

    SPARK_API void drawText(Vec3 pos, const char* text, ImU32 color);

    SPARK_API void beginESPWindow(const char * name);

    SPARK_API void endESPWindow();

    namespace DX11 {
        SPARK_API void init();
        SPARK_API void free();

        // Call begin() before a batch of drawLine calls, end() after.
        // The depth bias resets to 0 each begin().
        SPARK_API void begin();
        SPARK_API void end();

        // Shift vertices toward the camera by this many world units before depth
        // testing. Useful to prevent z-fighting with coplanar geometry.
        SPARK_API void setDepthBias(float bias);

        SPARK_API void drawLine(Vec3 a, Vec3 b, ImU32 color);
    }
}