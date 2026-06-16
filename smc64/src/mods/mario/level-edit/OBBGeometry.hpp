#pragma once

#include "EditorState.hpp"

#ifdef ENABLE_LEVEL_EDITOR

#include "math/OBBIntersect.hpp"
#include <array>

namespace Mod::Mario::LevelEdit {

inline Matrix3 getAxes(const OrientedBoundingBox& obb) {
    Vec3 euler = obb.orientation;
    return Matrix3::fromEulerYXZ(euler);
}

// Returns all 8 corners. Indices encode ±X, ±Y, ±Z as bits 2,1,0 (0=+, 1=-).
inline std::array<Vec3, 8> getCorners(const OrientedBoundingBox& obb) {
    auto ax = getAxes(obb);
    Vec3 ex = ax.columns.x * obb.halfExtents.x;
    Vec3 ey = ax.columns.y * obb.halfExtents.y;
    Vec3 ez = ax.columns.z * obb.halfExtents.z;
    Vec3 c  = obb.center; // non-const copy — Vec3 operators are not const-qualified
    return {
        c + ex + ey + ez,  // 000
        c + ex + ey - ez,  // 001
        c + ex - ey + ez,  // 010
        c + ex - ey - ez,  // 011
        c - ex + ey + ez,  // 100
        c - ex + ey - ez,  // 101
        c - ex - ey + ez,  // 110
        c - ex - ey - ez,  // 111
    };
}

} // namespace Mod::Mario::LevelEdit

#endif // ENABLE_LEVEL_EDITOR
