#include "OBBRender.hpp"

#ifdef ENABLE_LEVEL_EDITOR

#include "OBBGeometry.hpp"
#include "GizmoWidgets.hpp"
#include "spark/overlay/ESP.hpp"
#include <cmath>

namespace Mod::Mario::LevelEdit {

using namespace Spark::Overlay;

static constexpr float kPi              = 3.14159265358979f;
static constexpr int   kRingSegments    = 32;
static constexpr float kRingRadiusScale = 1.8f; // relative to max half-extent
static constexpr float kRingRadiusMin   = 0.4f; // world-unit minimum

// ── Gizmo ID encoding ────────────────────────────────────────────────────────
// Lower nibble: axis 0-2 = translate, axis 4-6 = rotate.
// Upper bits: (obbIdx + 1) to ensure non-zero.

static uint32_t translateId(int obbIdx, int axis) {
    return (uint32_t)(obbIdx + 1) << 4 | (uint32_t)axis;
}
static uint32_t rotateId(int obbIdx, int axis) {
    return (uint32_t)(obbIdx + 1) << 4 | 4u | (uint32_t)axis;
}

// ── Translate drag ────────────────────────────────────────────────────────────

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

// ── Rotate drag ───────────────────────────────────────────────────────────────

struct RotateDragCtx {
    OrientedBoundingBox* obb;
    Vec3                 axisDir; // world-space local axis to rotate around
};

static void onRotateDrag(void* ctx, Ray current, Ray prev) {
    auto* c    = static_cast<RotateDragCtx*>(ctx);
    Vec3 center = c->obb->center;   // non-const copy
    Vec3 axis   = c->axisDir;       // non-const copy

    // Intersect a mouse ray with the plane through center, normal=axis.
    // Returns center when the ray is parallel to the plane or hits behind its origin.
    auto planeHit = [&](const Ray& r) -> Vec3 {
        Vec3  ro    = r.origin;
        Vec3  rd    = r.direction;
        float denom = axis.dot(rd);
        if (fabsf(denom) < 1e-6f) return center;
        Vec3  w = center - ro;
        float t = axis.dot(w) / denom;
        if (t < 0.f) return center;
        return ro + rd * t;
    };

    Vec3 pCur  = planeHit(current);
    Vec3 pPrev = planeHit(prev);

    Vec3 vCur  = (pCur  - center).normalize();
    Vec3 vPrev = (pPrev - center).normalize();

    // Signed angle from vPrev to vCur, around axis
    Vec3  crossProd = vPrev.cross(vCur);
    float sinA      = crossProd.dot(axis);
    float cosA      = vPrev.dot(vCur);
    float dTheta    = atan2f(sinA, cosA);
    if (fabsf(dTheta) < 1e-7f) return;

    // Rodrigues: rotate vector v around axis by dTheta
    float cosT = cosf(dTheta);
    float sinT = sinf(dTheta);
    auto rodrigues = [&](Vec3 v) -> Vec3 {
        Vec3  ax = axis;
        float d  = ax.dot(v);
        Vec3  cr = ax.cross(v);
        return v * cosT + cr * sinT + ax * (d * (1.f - cosT));
    };

    auto obbAxes = getAxes(*c->obb);
    Vec3 newX = rodrigues(obbAxes.columns.x).normalize();
    Vec3 newZ = rodrigues(obbAxes.columns.z).normalize();
    c->obb->orientation = Vec3::orientationToEuler_YXZ(newX, newZ);
}

static RotateDragCtx s_rotCtx[3]; // one per axis, reused each frame

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
    static const ImU32 kArrowNormal[3] = {
        IM_COL32(200,  48,  48, 220), // X — red
        IM_COL32( 48, 200,  48, 220), // Y — green
        IM_COL32( 48,  48, 200, 220), // Z — blue
    };
    static const ImU32 kRingNormal[3] = {
        IM_COL32(200,  48,  48, 140), // X ring — red, dimmer
        IM_COL32( 48, 200,  48, 140), // Y ring — green, dimmer
        IM_COL32( 48,  48, 200, 140), // Z ring — blue, dimmer
    };
    static const ImU32 kHot[3] = {
        IM_COL32(255, 140, 140, 255),
        IM_COL32(140, 255, 140, 255),
        IM_COL32(140, 140, 255, 255),
    };
    static const ImU32 kActive[3] = {
        IM_COL32(255, 255,  80, 255),
        IM_COL32(255, 255,  80, 255),
        IM_COL32(255, 255,  80, 255),
    };

    auto axes = getAxes(obb);
    Vec3 axArr[3] = { axes.columns.x, axes.columns.y, axes.columns.z };
    const float he[3] = { obb.halfExtents.x, obb.halfExtents.y, obb.halfExtents.z };
    Vec3  center   = obb.center;
    bool  selected = (obbIdx == selectedIdx);
    float maxHe    = fmaxf(fmaxf(he[0], he[1]), he[2]);
    float ringR    = fmaxf(maxHe * kRingRadiusScale, kRingRadiusMin);

    ESP::DX11::setDepthBias(1.0f);

    // ── Translate arrows ──────────────────────────────────────────────────────
    for (int i = 0; i < 3; i++) {
        uint32_t id  = translateId(obbIdx, i);
        ImU32    col = Gizmo::isActive(id) ? kActive[i]
                     : Gizmo::isHot(id)    ? kHot[i]
                     :                       kArrowNormal[i];

        Vec3  tip      = center + axArr[i] * (he[i] * 1.2f);
        Vec3  perp     = axArr[(i + 1) % 3];
        float headSize = he[i] * 0.15f;
        Vec3  headBase = center + axArr[i] * (he[i] * 1.2f * 0.8f);
        ESP::DX11::drawLine(center, tip, col);
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

    // ── Rotation rings ────────────────────────────────────────────────────────
    // Ring i rotates around local axis axArr[i], drawn in the plane spanned by
    // axArr[(i+1)%3] and axArr[(i+2)%3].
    for (int i = 0; i < 3; i++) {
        uint32_t rid = rotateId(obbIdx, i);
        ImU32    col = Gizmo::isActive(rid) ? kActive[i]
                     : Gizmo::isHot(rid)    ? kHot[i]
                     :                        kRingNormal[i];

        Vec3 u = axArr[(i + 1) % 3];
        Vec3 v = axArr[(i + 2) % 3];

        // Generate ring points and draw line segments
        Vec3 prev = center + u * ringR;
        for (int k = 1; k <= kRingSegments; k++) {
            float ang  = (2.f * kPi * k) / (float)kRingSegments;
            Vec3  next = center + u * (cosf(ang) * ringR) + v * (sinf(ang) * ringR);
            ESP::DX11::drawLine(prev, next, col);
            prev = next;
        }

        if (selected) {
            s_rotCtx[i].obb     = &obb;
            s_rotCtx[i].axisDir = axArr[i];

            Gizmo::GizmoWidget w = {};
            w.id       = rid;
            w.closed   = true;
            w.numPoints = kRingSegments;
            for (int k = 0; k < kRingSegments; k++) {
                float ang    = (2.f * kPi * k) / (float)kRingSegments;
                w.points[k]  = center + u * (cosf(ang) * ringR) + v * (sinf(ang) * ringR);
            }
            w.onDrag = onRotateDrag;
            w.ctx    = &s_rotCtx[i];
            Gizmo::submitGizmo(w);
        }
    }

    ESP::DX11::setDepthBias(0.0f);
}

} // namespace Mod::Mario::LevelEdit

#endif // ENABLE_LEVEL_EDITOR
