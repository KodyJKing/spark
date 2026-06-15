// Utilities for drawing things in world space.

#pragma once

#include "math/Vectors.hpp"
#include "imgui.h"

namespace Spark::Overlay::ESP {
    extern Camera camera;

    // Sync ESP::camera from the player camera. Call once per frame before any ESP draws.
    void updateCamera();

    void updateScreenSize();

    Vec3 worldToScreen(Vec3 worldPos);

    void drawPoint(Vec3 pos, ImU32 color);

    void drawLine(Vec3 start, Vec3 end, ImU32 color);

    void drawBox(Vec3 center, Vec3 size, ImU32 color);

    void drawText(Vec3 pos, std::string text, ImU32 color);

    void drawCircle(Vec3 center, float radius, ImU32 color, bool perspective = true, bool filled = false);

    void beginESPWindow(const char * name);

    void endESPWindow();

    namespace DX11 {
        void init();
        void free();

        // Call begin() before a batch of drawLine calls, end() after.
        // The depth bias resets to 0 each begin().
        void begin();
        void end();

        // Shift vertices toward the camera by this many world units before depth
        // testing. Useful to prevent z-fighting with coplanar geometry.
        void setDepthBias(float bias);

        void drawLine(Vec3 a, Vec3 b, ImU32 color);
    }
}