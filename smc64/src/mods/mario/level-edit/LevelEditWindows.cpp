#include "LevelEditWindows.hpp"

#ifdef ENABLE_LEVEL_EDITOR

#include "OBBGeometry.hpp"
#include "OBBRender.hpp"
#include "LevelEditSave.hpp"
#include "GizmoWidgets.hpp"
#include "level-edits/index.hpp"
#include "spark/overlay/ESP.hpp"
#include "engine/raycast.hpp"
#include "engine/player.hpp"
#include "math/Math.hpp"
#include "math/OBBIntersect.hpp"
#include <array>

namespace Mod::Mario::LevelEdit {

using namespace Spark::Overlay;

// ── Camera ────────────────────────────────────────────────────────────────────

static Camera buildEditorCamera() {
    auto* ec = Engine::getPlayerCameraPointer();
    ImGuiIO& io = ImGui::GetIO();
    Camera cam;
    cam.width  = io.DisplaySize.x;
    cam.height = io.DisplaySize.y;
    if (ec) {
        cam.pos = ec->pos;
        cam.fwd = ec->fwd;
        cam.up  = ec->up;
        // Engine FOV is horizontal — convert to vertical to match Camera::project / mouseRay.
        cam.fov = Math::convertFov(ec->fov, cam.width, cam.height);
    }
    cam.verticalFov = true;
    return cam;
}

// ── Placement + picking ───────────────────────────────────────────────────────

static void placeOBB(EditorState& state, const Ray& mouseRay) {
    Beep(440, 100);

    if (!state.currentEdits) return;
    Engine::RaycastResult result;
    Vec3 origin = mouseRay.origin;
    Vec3 dir    = mouseRay.direction;
    Vec3 displ  = dir * 200.f;
    Engine::raycast(ENGINE_RAYCAST_PROJECTILE_FLAGS, &origin, &displ, 0, &result);
    if (result.hitType == Engine::HitType_Nothing) return;

    OrientedBoundingBox obb;
    obb.center      = result.pos;
    obb.halfExtents = { 0.5f, 0.5f, 0.5f };
    obb.orientation = { 0.f,  0.f,  0.f  };

    Vec3 axisZ = result.normal;
    Vec3 axisX = Vec3::orthonormalize(axisZ, { 0.f, 0.f, 1.f });
    obb.orientation = Vec3::orientationToEuler_YXZ(axisX, axisZ);

    state.currentEdits->orientedBoundingBoxes.push_back(obb);
    state.selectedIdx = (int)state.currentEdits->orientedBoundingBoxes.size() - 1;
}

static void pickOBB(EditorState& state, const Ray& mouseRay) {
    if (!state.currentEdits) return;

    Ray ray = mouseRay;
    auto& obbs = state.currentEdits->orientedBoundingBoxes;

    int   bestIdx = -1;
    float bestT   = 1e30f;
    for (int i = 0; i < (int)obbs.size(); i++) {
        auto axes = getAxes(obbs[i]);
        float t;
        if (Math::rayIntersectsOBB(ray, obbs[i].center, axes, obbs[i].halfExtents, t)) {
            if (t < bestT) { bestT = t; bestIdx = i; }
        }
    }
    if (bestIdx >= 0) state.selectedIdx = bestIdx;
}

// ── World rendering ───────────────────────────────────────────────────────────

void renderWorld(EditorState& state) {
    if (!state.editorOpen || !state.currentEdits) return;

    Camera cam = buildEditorCamera();
    Gizmo::beginGizmos(cam);

    ESP::DX11::begin();
    auto& obbs = state.currentEdits->orientedBoundingBoxes;
    for (int i = 0; i < (int)obbs.size(); i++) {
        ImU32 wireColor = (i == state.selectedIdx)
            ? IM_COL32(255, 220, 0,   255)   // selected — yellow
            : IM_COL32(255, 255, 255, 200);  // default  — white
        drawOBBWireframe(obbs[i], wireColor);
        drawOBBAxes(obbs[i], i, state.selectedIdx);
    }

    // ── Debug: visualize mouse ray hit ────────────────────────────────────────
    // Draws a short magenta vertical post wherever the mouse ray hits geometry.
    // Remove once picking / axis drag are confirmed correct.
    {
        ImGuiIO& io = ImGui::GetIO();
        Ray mRay   = cam.mouseRay(io.MousePos.x, io.MousePos.y);
        Vec3 origin = mRay.origin;
        Vec3 displ  = mRay.direction * 200.f;
        Engine::RaycastResult hit;
        Engine::raycast(ENGINE_RAYCAST_PROJECTILE_FLAGS, &origin, &displ, 0, &hit);
        if (hit.hitType != Engine::HitType_Nothing) {
            Vec3 lo = hit.pos + Vec3{ 0.f, 0.f, -0.05f };
            Vec3 hi = hit.pos + Vec3{ 0.f, 0.f,  0.5f  };
            ESP::DX11::drawLine(lo, hi, IM_COL32(255, 0, 255, 255));
        }
    }

    ESP::DX11::end();

    bool gizmoDrag = Gizmo::endGizmos();
    if (gizmoDrag && !state.editorInputEnabled) {
        state.editorInputEnabled = true;
    }
}

// ── ImGui helpers ─────────────────────────────────────────────────────────────

// DragFloat3 with per-component XYZ background tinting.
static bool dragFloat3XYZ(const char* label, float* v, float speed, float vMin = 0.f, float vMax = 0.f) {
    bool changed = false;
    const ImGuiStyle& style = ImGui::GetStyle();
    float totalWidth = ImGui::CalcItemWidth();
    float compWidth  = (totalWidth - style.ItemInnerSpacing.x * 2.f) / 3.f;
    const char* ids[3] = { "##x", "##y", "##z" };
    ImGui::PushID(label);
    for (int i = 0; i < 3; i++) {
        if (i > 0) ImGui::SameLine(0.f, style.ItemInnerSpacing.x);
        ImVec4 bgHov = { kAxisBg[i].x * 1.35f, kAxisBg[i].y * 1.35f, kAxisBg[i].z * 1.35f, 1.f };
        ImVec4 bgAct = { kAxisBg[i].x * 1.65f, kAxisBg[i].y * 1.65f, kAxisBg[i].z * 1.65f, 1.f };
        ImGui::PushStyleColor(ImGuiCol_FrameBg,        kAxisBg[i]);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, bgHov);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  bgAct);
        ImGui::SetNextItemWidth(compWidth);
        changed |= ImGui::DragFloat(ids[i], v + i, speed, vMin, vMax);
        ImGui::PopStyleColor(3);
    }
    ImGui::SameLine(0.f, style.ItemInnerSpacing.x);
    ImGui::TextUnformatted(label);
    ImGui::PopID();
    return changed;
}

// ── ImGui windows ─────────────────────────────────────────────────────────────

static void renderEditWindow(EditorState& state) {
    if (state.selectedIdx < 0 || !state.currentEdits) return;
    auto& obbs = state.currentEdits->orientedBoundingBoxes;
    if (state.selectedIdx >= (int)obbs.size()) { state.selectedIdx = -1; return; }

    auto& obb  = obbs[state.selectedIdx];
    auto  axes = getAxes(obb);
    Vec3 axArr[3] = { axes.columns.x, axes.columns.y, axes.columns.z };

    ImGui::Begin("OBB Edit", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("OBB #%d", state.selectedIdx);
    ImGui::Separator();
    dragFloat3XYZ("Center",       &obb.center.x,      0.05f);
    dragFloat3XYZ("Half Extents", &obb.halfExtents.x, 0.05f, 0.01f, 100.0f);
    dragFloat3XYZ("Orientation",  &obb.orientation.x, 1.0f, -180.0f, 180.0f);

    ImGui::Separator();
    ImGui::Text("Nudge (local axes):");
    static float s_nudge = 0.1f;
    ImGui::SetNextItemWidth(80.0f);
    ImGui::DragFloat("Step##nudge", &s_nudge, 0.01f, 0.001f, 10.0f, "%.3f");

    static const char* kLabels[3][2] = { {"+X","-X"}, {"+Y","-Y"}, {"+Z","-Z"} };
    for (int i = 0; i < 3; i++) {
        ImVec4 bg    = kAxisBg[i];
        ImVec4 bgHov = { bg.x * 1.35f, bg.y * 1.35f, bg.z * 1.35f, 1.f };
        ImVec4 bgAct = { bg.x * 1.65f, bg.y * 1.65f, bg.z * 1.65f, 1.f };
        ImGui::PushStyleColor(ImGuiCol_Button,        bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bgHov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  bgAct);
        if (ImGui::Button(kLabels[i][0])) obb.center += axArr[i] * s_nudge;
        ImGui::SameLine();
        if (ImGui::Button(kLabels[i][1])) obb.center -= axArr[i] * s_nudge;
        ImGui::PopStyleColor(3);
        if (i < 2) ImGui::SameLine();
    }
    ImGui::End();
}

static void renderManagerWindow(EditorState& state) {
    ImGui::Begin("OBB Manager");

    if (!state.currentEdits) {
        ImGui::TextDisabled("No level loaded.");
        ImGui::End();
        return;
    }

    auto& obbs = state.currentEdits->orientedBoundingBoxes;
    ImGui::Text("Level: %s   OBBs: %zu", state.currentContext.levelName.c_str(), obbs.size());
    ImGui::Separator();

    ImGui::BeginChild("##list", ImVec2(0, 180), true);
    for (int i = 0; i < (int)obbs.size(); i++) {
        char label[64];
        snprintf(label, sizeof(label), "OBB #%d  (%.1f, %.1f, %.1f)", i,
                 obbs[i].center.x, obbs[i].center.y, obbs[i].center.z);
        if (ImGui::Selectable(label, state.selectedIdx == i))
            state.selectedIdx = i;
    }
    ImGui::EndChild();

    ImGui::Separator();

    if (ImGui::Button("Add (raycast)")) {
        Camera cam = buildEditorCamera();
        ImGuiIO& io = ImGui::GetIO();
        placeOBB(state, cam.mouseRay(io.MousePos.x, io.MousePos.y));
    }
    ImGui::SameLine();
    if (ImGui::Button("Duplicate") && state.selectedIdx >= 0 && state.selectedIdx < (int)obbs.size()) {
        obbs.push_back(obbs[state.selectedIdx]);
        state.selectedIdx = (int)obbs.size() - 1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete") && state.selectedIdx >= 0 && state.selectedIdx < (int)obbs.size()) {
        obbs.erase(obbs.begin() + state.selectedIdx);
        if (state.selectedIdx >= (int)obbs.size()) state.selectedIdx = (int)obbs.size() - 1;
    }

    ImGui::Separator();
    if (ImGui::Button("Save .hpp")) {
        saveLevelEdits(state);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Re-compile to activate.");

    ImGui::End();
}

static void renderCursor(const EditorState& state) {
    if (!(state.editorOpen && state.editorInputEnabled)) return;

    ImVec2 mp = ImGui::GetMousePos();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const float R  = 8.0f;
    const float G  = 2.0f;
    const ImU32 kCursorColor  = IM_COL32(255, 255, 255, 220);
    const ImU32 kCursorShadow = IM_COL32(0,   0,   0,   120);
    dl->AddLine({mp.x - R + 1, mp.y + 1}, {mp.x + R + 1, mp.y + 1}, kCursorShadow, G);
    dl->AddLine({mp.x + 1, mp.y - R + 1}, {mp.x + 1, mp.y + R + 1}, kCursorShadow, G);
    dl->AddLine({mp.x - R, mp.y}, {mp.x + R, mp.y}, kCursorColor, G);
    dl->AddLine({mp.x, mp.y - R}, {mp.x, mp.y + R}, kCursorColor, G);
}

void renderUI(EditorState& state) {
    if (ImGui::IsKeyPressed(ImGuiKey_F6, false)) {
        state.editorOpen = !state.editorOpen;
        if (state.editorOpen) {
            // Refresh in case the level changed since last open.
            state.currentEdits = Index::lookup(state.currentContext.levelName);
        } else {
            state.editorInputEnabled = false;
        }
    }

    if (!state.editorOpen) return;

    if (ImGui::IsKeyPressed(ImGuiKey_Tab, false)) {
        state.editorInputEnabled = !state.editorInputEnabled;
    }

    renderEditWindow(state);
    renderManagerWindow(state);
    renderCursor(state);

    // Left click = pick OBB, right click = place OBB.
    // Guard against clicks that land inside an ImGui window.
    if (!ImGui::GetIO().WantCaptureMouse) {
        Camera cam = buildEditorCamera();
        ImGuiIO& io = ImGui::GetIO();
        Ray mRay = cam.mouseRay(io.MousePos.x, io.MousePos.y);
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left,  false)) pickOBB(state, mRay);
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right, false)) placeOBB(state, mRay);
    }
}

} // namespace Mod::Mario::LevelEdit

#endif // ENABLE_LEVEL_EDITOR
