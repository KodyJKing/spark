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
static constexpr float kRingRadius      = 0.6f; // fixed world-unit ring radius
static constexpr float kArrowLength     = 0.7f; // fixed world-unit arrow shaft length
static constexpr float kArrowHeadSize   = 0.10f; // fixed world-unit arrowhead half-width

// ── Gizmo ID encoding ────────────────────────────────────────────────────────
// Lower nibble: axis 0-2 = translate, axis 4-6 = rotate.
// Upper bits: (obbIdx + 1) to ensure non-zero.

static uint32_t translateId(int obbIdx, int axis) {
    return (uint32_t)(obbIdx + 1) << 4 | (uint32_t)axis;
}
static uint32_t rotateId(int obbIdx, int axis) {
    return (uint32_t)(obbIdx + 1) << 4 | 4u | (uint32_t)axis;
}
// face = axis*2 + (sign>0 ? 0 : 1), values 0-5 → lower-nibble bits 8-13
static uint32_t faceId(int obbIdx, int face) {
    return (uint32_t)(obbIdx + 1) << 4 | (8u + (uint32_t)face);
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

// ── Face extrude drag ────────────────────────────────────────────────────────────

struct FaceDragCtx {
    OrientedBoundingBox* obb;
    Vec3                 axisDir; // outward face normal (world-space)
    int                  axis;   // 0=X, 1=Y, 2=Z
    float                sign;   // +1 = positive face, -1 = negative face
};

static void onFaceDrag(void* ctx, Ray current, Ray prev) {
    auto* c     = static_cast<FaceDragCtx*>(ctx);
    Vec3  ax    = c->axisDir;
    Vec3  pCur  = closestPointOnAxisToRay(current, c->obb->center, ax);
    Vec3  pPrev = closestPointOnAxisToRay(prev,    c->obb->center, ax);
    // raw: +ve = moved in +axisDir direction
    float raw  = ax.dot(pCur - pPrev);
    // hd: half the outward delta (applied symmetrically to he and center)
    float hd   = c->sign * raw * 0.5f;

    float& he = c->axis == 0 ? c->obb->halfExtents.x
              : c->axis == 1 ? c->obb->halfExtents.y
              :                 c->obb->halfExtents.z;
    if (he + hd < 0.02f) hd = 0.02f - he; // prevent inversion
    he += hd;
    c->obb->center += ax * (c->sign * hd);
}

static FaceDragCtx s_faceCtx[6]; // one per face (±X, ±Y, ±Z), reused each frame

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
    Vec3  center      = obb.center;
    // Anchor gizmos at the bottom face (local -Z) so they don't obscure the OBB.
    Vec3  gizmoOrigin = center - axArr[2] * he[2];
    bool  selected    = (obbIdx == selectedIdx);
    float ringR       = kRingRadius;

    ESP::DX11::setDepthBias(1.0f);

    // ── Translate arrows ──────────────────────────────────────────────────────
    for (int i = 0; i < 3; i++) {
        uint32_t id  = translateId(obbIdx, i);
        ImU32    col = Gizmo::isActive(id) ? kActive[i]
                     : Gizmo::isHot(id)    ? kHot[i]
                     :                       kArrowNormal[i];

        // Arrows are fixed-length, centered at the OBB center.
        Vec3  base     = center;
        Vec3  tip      = center + axArr[i] * (kArrowLength * 0.5f);
        Vec3  perp     = axArr[(i + 1) % 3];
        Vec3  headBase = center + axArr[i] * (kArrowLength * 0.5f * 0.75f);
        ESP::DX11::drawLine(base, tip, col);
        ESP::DX11::drawLine(tip, headBase + perp * kArrowHeadSize, col);
        ESP::DX11::drawLine(tip, headBase - perp * kArrowHeadSize, col);

        if (selected) {
            s_dragCtx[i].obb     = &obb;
            s_dragCtx[i].axisDir = axArr[i];

            Gizmo::GizmoWidget w = {};
            w.id        = id;
            w.numPoints = 2;
            w.points[0] = base;
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
            w.id        = rid;
            w.closed    = true;
            w.numPoints = kRingSegments;
            for (int k = 0; k < kRingSegments; k++) {
                float ang   = (2.f * kPi * k) / (float)kRingSegments;
                w.points[k] = center + u * (cosf(ang) * ringR) + v * (sinf(ang) * ringR);
            }
            w.onDrag = onRotateDrag;
            w.ctx    = &s_rotCtx[i];
            Gizmo::submitGizmo(w);
        }
    }

    // ── Face extrude handles ──────────────────────────────────────────────────────
    // Small crosshair icon at each face center; drag to extrude that face.
    // Opposite face stays fixed; the OBB center shifts by half the delta.
    static const ImU32 kFaceNormal[3] = {
        IM_COL32(220,  70,  70, 180),
        IM_COL32( 70, 220,  70, 180),
        IM_COL32( 70,  70, 220, 180),
    };
    static constexpr float kFaceIconScale = 0.30f;

    for (int i = 0; i < 3; i++) {
        Vec3  u  = axArr[(i + 1) % 3];
        Vec3  v  = axArr[(i + 2) % 3];
        float us = he[(i + 1) % 3];
        float vs = he[(i + 2) % 3];
        float iconR = fmaxf(fminf(us, vs) * kFaceIconScale, 0.05f);

        for (int si = 0; si < 2; si++) {
            float    s    = si == 0 ? 1.f : -1.f;
            int      face = i * 2 + si;
            uint32_t fid  = faceId(obbIdx, face);

            ImU32 col = Gizmo::isActive(fid) ? kActive[i]
                      : Gizmo::isHot(fid)    ? kHot[i]
                      : selected             ? kFaceNormal[i]
                      :                        IM_COL32(120, 120, 120, 80);

            Vec3 fc = center + axArr[i] * (he[i] * s); // face center

            // Cross icon in the face plane
            ESP::DX11::drawLine(fc + u * iconR, fc - u * iconR, col);
            ESP::DX11::drawLine(fc + v * iconR, fc - v * iconR, col);
            // Short outward tick
            ESP::DX11::drawLine(fc, fc + axArr[i] * (0.1f * s), col);

            if (selected) {
                s_faceCtx[face].obb     = &obb;
                s_faceCtx[face].axisDir = axArr[i];
                s_faceCtx[face].axis    = i;
                s_faceCtx[face].sign    = s;
                
                // Just use the cross icon already drawn in the face plane as the interactive handle
                Gizmo::GizmoWidget w = {};
                w.id        = fid;
                w.closed    = true;
                w.numPoints = 4;
                w.points[0] = fc + u * iconR;
                w.points[1] = fc - u * iconR;
                w.points[2] = fc + v * iconR;
                w.points[3] = fc - v * iconR;
                w.onDrag    = onFaceDrag;
                w.ctx       = &s_faceCtx[face];
                Gizmo::submitGizmo(w);
            }
        }
    }

    ESP::DX11::setDepthBias(0.0f);
}

} // namespace Mod::Mario::LevelEdit

#endif // ENABLE_LEVEL_EDITOR
