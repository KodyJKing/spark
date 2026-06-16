#include "GizmoWidgets.hpp"
#include <vector>
#include <cmath>

namespace Gizmo {

// ── Frame state ───────────────────────────────────────────────────────────────

static std::vector<GizmoWidget> s_widgets;
static Camera    s_frameCamera    = {};
static uint32_t  s_hotId          = 0;
static uint32_t  s_activeId       = 0;
static Ray       s_prevMouseRay   = {};

// ── Helpers ───────────────────────────────────────────────────────────────────

static constexpr float kHitPx = 8.f;

// 2D point-to-segment distance (screen space, uses x/y only).
static float screenDistToSegment(ImVec2 p, ImVec2 a, ImVec2 b) {
    float abx = b.x - a.x, aby = b.y - a.y;
    float len2 = abx * abx + aby * aby;
    if (len2 < 1e-6f) {
        float dx = p.x - a.x, dy = p.y - a.y;
        return sqrtf(dx * dx + dy * dy);
    }
    float t = ((p.x - a.x) * abx + (p.y - a.y) * aby) / len2;
    if (t < 0.f) t = 0.f;
    if (t > 1.f) t = 1.f;
    float cx = a.x + t * abx, cy = a.y + t * aby;
    float dx = p.x - cx, dy = p.y - cy;
    return sqrtf(dx * dx + dy * dy);
}

// ── API implementation ────────────────────────────────────────────────────────

void beginGizmos(Camera cam) {
    s_frameCamera = cam;
    s_widgets.clear();
}

void submitGizmo(GizmoWidget w) {
    s_widgets.push_back(w);
}

bool endGizmos() {
    ImVec2 mouse = ImGui::GetMousePos();

    // ── Build this frame's mouse ray ─────────────────────────────────────────
    Ray mouseRay = s_frameCamera.mouseRay(mouse.x, mouse.y);

    // ── Hit-test: find closest widget within kHitPx ──────────────────────────
    uint32_t newHot  = 0;
    float    bestDst = kHitPx;

    for (auto& w : s_widgets) {
        if (w.numPoints < 2) continue;
        int segCount = w.closed ? w.numPoints : w.numPoints - 1;
        for (int s = 0; s < segCount; s++) {
            Vec3 sA = w.screenPoints[s];
            Vec3 sB = w.screenPoints[(s + 1) % w.numPoints];
            // Skip segments behind the camera (depth <= 0)
            if (sA.z <= 0.f || sB.z <= 0.f) continue;
            float d = screenDistToSegment(
                mouse,
                { sA.x, sA.y },
                { sB.x, sB.y }
            );
            if (d < bestDst) {
                bestDst = d;
                newHot  = w.id;
            }
        }
    }
    s_hotId = newHot;

    // ── LMB press: capture ───────────────────────────────────────────────────
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && s_hotId != 0 && s_activeId == 0) {
        s_activeId = s_hotId;
        // Find and invoke onDragBegin (optional)
        for (auto& w : s_widgets) {
            if (w.id == s_activeId) {
                if (w.onDragBegin) w.onDragBegin(w.ctx, mouseRay);
                break;
            }
        }
        s_prevMouseRay = mouseRay;
    }

    // ── LMB held: invoke onDrag ──────────────────────────────────────────────
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && s_activeId != 0) {
        for (auto& w : s_widgets) {
            if (w.id == s_activeId) {
                if (w.onDrag) w.onDrag(w.ctx, mouseRay, s_prevMouseRay);
                break;
            }
        }
        s_prevMouseRay = mouseRay;
    }

    // ── LMB released: clear capture ──────────────────────────────────────────
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        s_activeId = 0;
    }

    return s_activeId != 0;
}

bool isHot   (uint32_t id) { return s_hotId    == id; }
bool isActive(uint32_t id) { return s_activeId == id; }

} // namespace Gizmo
