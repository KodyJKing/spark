#include "MarioLevelEdit.hpp"
#include "level-edits/index.hpp"
#include "../Coordinates.hpp"
#include "engine/bsp/level_bsp.hpp"
#include "libsm64.h"
#include "decomp/surface_terrains.h"

#include <array>

namespace Mod::Mario::LevelEdit {

// ── Helpers ───────────────────────────────────────────────────────────────────

static void addTriangle(std::vector<SM64Surface>& out,
                        const Vec3& a, const Vec3& b, const Vec3& c,
                        Vec3i chunk) {
    SM64Surface s = {};
    s.type    = SURFACE_DEFAULT;
    s.force   = 0;
    s.terrain = 0;

    const Vec3* verts[3] = { &a, &b, &c };
    for (int i = 0; i < 3; ++i) {
        Vec3 world = Coordinates::haloToMario(*verts[i]);
        Vec3 local = Coordinates::marioWorldToLocal(world, chunk);
        s.vertices[i][0] = (int32_t)local.x;
        s.vertices[i][1] = (int32_t)local.y;
        s.vertices[i][2] = (int32_t)local.z;
    }
    out.push_back(s);
}

// Emits 12 SM64 surfaces (2 triangles per face × 6 faces) for one OBB.
//
// Corners are indexed as 3-bit numbers: bit2=X (0=+), bit1=Y (0=+), bit0=Z (0=+).
// Winding is CCW viewed from the outward normal (right-hand rule: (B-A)×(C-A) = outward).
static void obbToSurfaces(const OrientedBoundingBox& obb, std::vector<SM64Surface>& out, Vec3i chunk) {
    auto ax = Matrix3::fromEulerYXZ(obb.orientation);
    Vec3 ex = ax.columns.x * obb.halfExtents.x;
    Vec3 ey = ax.columns.y * obb.halfExtents.y;
    Vec3 ez = ax.columns.z * obb.halfExtents.z;
    Vec3 o  = obb.center;

    Vec3 p[8] = {
        o + ex + ey + ez,  // 0  X+Y+Z+
        o + ex + ey - ez,  // 1  X+Y+Z-
        o + ex - ey + ez,  // 2  X+Y-Z+
        o + ex - ey - ez,  // 3  X+Y-Z-
        o - ex + ey + ez,  // 4  X-Y+Z+
        o - ex + ey - ez,  // 5  X-Y+Z-
        o - ex - ey + ez,  // 6  X-Y-Z+
        o - ex - ey - ez,  // 7  X-Y-Z-
    };

    addTriangle(out, p[0], p[1], p[2], chunk); addTriangle(out, p[2], p[1], p[3], chunk); // +X
    addTriangle(out, p[4], p[6], p[5], chunk); addTriangle(out, p[5], p[6], p[7], chunk); // -X
    addTriangle(out, p[0], p[4], p[1], chunk); addTriangle(out, p[1], p[4], p[5], chunk); // +Y
    addTriangle(out, p[2], p[3], p[6], chunk); addTriangle(out, p[6], p[3], p[7], chunk); // -Y
    addTriangle(out, p[0], p[2], p[4], chunk); addTriangle(out, p[4], p[2], p[6], chunk); // +Z
    addTriangle(out, p[1], p[5], p[3], chunk); addTriangle(out, p[3], p[5], p[7], chunk); // -Z
}

// ── Public API ────────────────────────────────────────────────────────────────

std::vector<SM64Surface> getStaticSurfaces(uint64_t bspSignature, Vec3i chunk) {
    std::vector<SM64Surface> result;
    LevelEdits* edits = Index::lookup(bspSignature);
    if (!edits) return result;
    for (const auto& obb : edits->orientedBoundingBoxes)
        obbToSurfaces(obb, result, chunk);
    return result;
}

std::vector<SM64Surface> getStaticSurfaces(Vec3i chunk) {
    return getStaticSurfaces(Engine::getBSPSignature(), chunk);
}

} // namespace Mod::Mario::LevelEdit
