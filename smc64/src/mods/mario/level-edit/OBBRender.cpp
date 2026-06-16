#include "OBBRender.hpp"

#ifdef ENABLE_LEVEL_EDITOR

#include "OBBGeometry.hpp"
#include "GizmoWidgets.hpp"
#include "spark/overlay/ESP.hpp"

namespace Mod::Mario::LevelEdit {

using namespace Spark::Overlay;

// ── Internal helpers ──────────────────────────────────────────────────────────

// Encode (obb index 0-255, axis 0-2) into a stable non-zero uint32_t gizmo id.
static uint32_t gizmoId(int obbIdx, int axis) {
    return (uint32_t)(obbIdx + 1) << 4 | (uint32_t)axis;
}

struct TranslateDragCtx {
    OrientedBoundingBox* obb;
    Vec3                 axisDir; // world-space normalized axis
};

static void onTranslateDrag(void* ctx, Ray current, Ray prev) {
    auto* c = static_cast<TranslateDragCtx*>(ctx);
    Vec3 pCur  = closestPointOnAxisToRay(current, c->obb->center, c->axisDir);
    Vec3 pPrev = closestPointOnAxisToRay(prev,    c->obb->center, c->axisDir);
    Vec3 delta = pCur - pPrev;
    float d    = c->axisDir.dot(delta);
    c->obb->center += c->axisDir * d;
}

static TranslateDragCtx s_dragCtx[3]; // one per axis, reused each frame

// ── Public draw functions ─────────────────────────────────────────────────────

void drawOBBWireframe(const OrientedBoundingBox& obb, ImU32 color) {
    auto c = getCorners(obb);
    // 4 edges parallel to local X
    ESP::DX11::drawLine(c[0], c[4], color);
    ESP::DX11::drawLine(c[1], c[5], color);
    ESP::DX11::drawLine(c[2], c[6], color);
    ESP::DX11::drawLine(c[3], c[7], color);
    // 4 edges parallel to local Y
    ESP::DX11::drawLine(c[0], c[2], color);
    ESP::DX11::drawLine(c[1], c[3], color);
    ESP::DX11::drawLine(c[4], c[6], color);
    ESP::DX11::drawLine(c[5], c[7], color);
    // 4 edges parallel to local Z
    ESP::DX11::drawLine(c[0], c[1], color);
    ESP::DX11::drawLine(c[2], c[3], color);
    ESP::DX11::drawLine(c[4], c[5], color);
    ESP::DX11::drawLine(c[6], c[7], color);
}

void drawOBBAxes(OrientedBoundingBox& obb, int obbIdx, int selectedIdx) {
    static const ImU32 kNormal[3] = {
        IM_COL32(200,  48,  48, 220), // X — red
        IM_COL32( 48, 200,  48, 220), // Y — green
        IM_COL32( 48,  48, 200, 220), // Z — blue
    };
    static const ImU32 kHot[3] = {
        IM_COL32(255, 120, 120, 255),
        IM_COL32(120, 255, 120, 255),
        IM_COL32(120, 120, 255, 255),
    };
    static const ImU32 kActive[3] = {
        IM_COL32(255, 255,  80, 255), // active — yellow for all axes
        IM_COL32(255, 255,  80, 255),
        IM_COL32(255, 255,  80, 255),
    };

    auto axes = getAxes(obb);
    Vec3 axArr[3] = { axes.columns.x, axes.columns.y, axes.columns.z };
    const float he[3] = { obb.halfExtents.x, obb.halfExtents.y, obb.halfExtents.z };
    Vec3 center  = obb.center;
    bool selected = (obbIdx == selectedIdx);

    ESP::DX11::setDepthBias(1.0f);

    for (int i = 0; i < 3; i++) {
        uint32_t id = gizmoId(obbIdx, i);
        ImU32 col = Gizmo::isActive(id) ? kActive[i]
                  : Gizmo::isHot(id)    ? kHot[i]
                  :                       kNormal[i];

        Vec3 tip      = center + axArr[i] * (he[i] * 1.2f);
        ESP::DX11::drawLine(center, tip, col);

        Vec3  perp     = axArr[(i + 1) % 3];
        float headSize = he[i] * 0.15f;
        Vec3  headBase = center + axArr[i] * (he[i] * 1.2f * 0.8f);
        ESP::DX11::drawLine(tip, headBase + perp * headSize, col);
        ESP::DX11::drawLine(tip, headBase - perp * headSize, col);

        if (selected) {
            s_dragCtx[i].obb     = &obb;
            s_dragCtx[i].axisDir = axArr[i];

            Gizmo::GizmoWidget w = {};
            w.id        = id;
            w.numPoints = 2;
            w.points[0] = center;
            w.points[1] = tip;
            w.onDrag    = onTranslateDrag;
            w.ctx       = &s_dragCtx[i];
            Gizmo::submitGizmo(w);
        }
    }

    ESP::DX11::setDepthBias(0.0f);
}

} // namespace Mod::Mario::LevelEdit

#endif // ENABLE_LEVEL_EDITOR
