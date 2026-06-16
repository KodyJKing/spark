#pragma once

#include "math/Vectors.hpp"
#include "imgui.h"

// ── Immediate-mode gizmo widget system ────────────────────────────────────────
//
// Pure input router — no draw calls. Caller owns all rendering and queries
// isHot() / isActive() to choose colors.
//
// Usage per frame (inside ESP::DX11::begin/end is fine since we never draw):
//
//   Gizmo::beginGizmos(cam);
//
//   GizmoWidget w;
//   w.id        = myId;
//   w.numPoints = 2;
//   w.points[0] = worldPtA;
//   w.points[1] = worldPtB;
//   w.onDragBegin = nullptr;           // optional
//   w.onDrag      = myDragCallback;
//   w.ctx         = myCtx;
//   Gizmo::submitGizmo(w);
//
//   bool dragging = Gizmo::endGizmos();
//
// The onDrag signature is: void(void* ctx, Ray current, Ray prev)
// Both rays originate at the camera and are normalized world-space directions.
// The handler does whatever geometry it likes (axis projection, plane intersection, etc.)

namespace Gizmo {

static constexpr int MAX_WIDGET_POINTS = 16;

struct GizmoWidget {
    uint32_t id;                               // stable caller-assigned id (non-zero)

    // World-space points defining the widget's hit shape as a polyline.
    // segments are [0→1], [1→2], etc.  If closed=true the last segment wraps to 0.
    Vec3 points[MAX_WIDGET_POINTS];
    int  numPoints = 0;
    bool closed    = false;

    // Called once when LMB is pressed on this widget. May be nullptr.
    void (*onDragBegin)(void* ctx, Ray mouseRay) = nullptr;

    // Called every frame while LMB is held after capture.
    // current = this frame's mouse ray; prev = last frame's mouse ray.
    void (*onDrag)(void* ctx, Ray current, Ray prev) = nullptr;

    void* ctx = nullptr;
};

// ── API ───────────────────────────────────────────────────────────────────────

// Call once per frame before submitting widgets.
// Snapshots the camera and clears the submission buffer.
void beginGizmos(Camera cam);

// Submit a widget into the buffer for this frame's hit-test pass.
void submitGizmo(GizmoWidget w);

// Resolve hover, drag start/continue/end, invoke callbacks.
// Must be called after all submitGizmo() calls for the frame.
// Returns true if a drag is currently active (caller may want to suppress game input).
bool endGizmos();

// Query hot/active state for a widget id — use to tint your own draw calls.
// Valid after endGizmos() returns.
bool isHot   (uint32_t id);
bool isActive(uint32_t id);

} // namespace Gizmo
