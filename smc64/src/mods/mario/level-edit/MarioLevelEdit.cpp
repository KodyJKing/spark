#include "MarioLevelEdit.hpp"
#include "level-edits/index.hpp"

// ── Define to enable the live editor UI ──────────────────────────────────────
#define ENABLE_LEVEL_EDITOR

#ifdef ENABLE_LEVEL_EDITOR
#include "imgui.h"
#include "spark/overlay/ESP.hpp"   // world-space line drawing
#include "spark/RenderBuses.hpp"
#include "spark/hook/Hooks.hpp"
#include "engine/raycast.hpp"
#include "engine/player.hpp"       // getPlayerCameraPointer
#include "math/OBBIntersect.hpp"
#include <array>
#include <fstream>
#include <filesystem>
#endif

namespace Mod::Mario::LevelEdit {

#ifdef ENABLE_LEVEL_EDITOR
using namespace Spark::Overlay;
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Data layer (always compiled)
// ─────────────────────────────────────────────────────────────────────────────

LevelEdits* getLevelEdits(LevelEditContext& context) {
    return Index::lookup(context.levelName);
}

// ─────────────────────────────────────────────────────────────────────────────
// Editor (ENABLE_LEVEL_EDITOR only)
// ─────────────────────────────────────────────────────────────────────────────

#ifdef ENABLE_LEVEL_EDITOR

// ── Editor state ─────────────────────────────────────────────────────────────

static bool             s_editorOpen    = false;
static LevelEditContext s_currentContext = { "a50", 0 };
static LevelEdits*      s_currentEdits  = Index::lookup(s_currentContext.levelName);
static int              s_selectedIdx   = -1;

// ── OBB geometry helpers ─────────────────────────────────────────────────────

static std::array<Vec3, 3> getAxes(const OrientedBoundingBox& obb) {
    return Math::eulerDegreesToAxes(obb.orientation);
}

// Returns all 8 corners. Indices encode ±X, ±Y, ±Z as bits 2,1,0 (0=+, 1=-).
static std::array<Vec3, 8> getCorners(const OrientedBoundingBox& obb) {
    auto ax = getAxes(obb);
    Vec3 ex = ax[0] * obb.halfExtents.x;
    Vec3 ey = ax[1] * obb.halfExtents.y;
    Vec3 ez = ax[2] * obb.halfExtents.z;
    Vec3 c  = obb.center; // non-const copy — Vec3 operators are not const-qualified
    return {
        c + ex + ey + ez,   // 000
        c + ex + ey - ez,   // 001
        c + ex - ey + ez,   // 010
        c + ex - ey - ez,   // 011
        c - ex + ey + ez,   // 100
        c - ex + ey - ez,   // 101
        c - ex - ey + ez,   // 110
        c - ex - ey - ez,   // 111
    };
}

// ── World rendering ───────────────────────────────────────────────────────────

static void drawOBBWireframe(const OrientedBoundingBox& obb, ImU32 color) {
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

static void drawOBBAxes(const OrientedBoundingBox& obb) {
    static const ImU32 kAxisColors[3] = {
        IM_COL32(255, 64,  64,  255), // X — red
        IM_COL32( 64, 255, 64,  255), // Y — green
        IM_COL32( 64,  64, 255, 255), // Z — blue
    };
    auto axes = getAxes(obb);
    const float he[3] = { obb.halfExtents.x, obb.halfExtents.y, obb.halfExtents.z };
    Vec3 center = obb.center; // non-const copy — Vec3 operators are not const-qualified

    for (int i = 0; i < 3; i++) {
        Vec3 tip  = center + axes[i] * (he[i] * 1.2f);
        ESP::DX11::drawLine(center, tip, kAxisColors[i]);

        // Arrow head: two short lines angling back from the tip.
        // The head lies in the plane spanned by this axis and the next one.
        Vec3 perp      = axes[(i + 1) % 3];
        float headSize = he[i] * 0.15f;
        Vec3 headBase  = center + axes[i] * (he[i] * 1.2f * 0.8f);
        ESP::DX11::drawLine(tip, headBase + perp * headSize,  kAxisColors[i]);
        ESP::DX11::drawLine(tip, headBase - perp * headSize, kAxisColors[i]);
    }
}

static void renderWorld() {
    if (!s_editorOpen || !s_currentEdits) return;
    ESP::DX11::begin();
    auto& obbs = s_currentEdits->orientedBoundingBoxes;
    for (int i = 0; i < (int)obbs.size(); i++) {
        ImU32 wireColor = (i == s_selectedIdx)
            ? IM_COL32(255, 220, 0,   255)   // selected — yellow
            : IM_COL32(255, 255, 255, 200);  // default  — white
        drawOBBWireframe(obbs[i], wireColor);
        drawOBBAxes(obbs[i]);
    }
    ESP::DX11::end();
}

// ── Placement + picking ───────────────────────────────────────────────────────

static void placeOBB() {
    Beep(440, 100); // Play a beep sound at 440 Hz for 100 ms

    if (!s_currentEdits) return;
    Engine::RaycastResult result;
    Engine::raycastPlayerCrosshair(&result);
    if (result.hitType == Engine::HitType_Nothing) return;

    OrientedBoundingBox obb;
    obb.center      = result.pos;
    obb.halfExtents = { 0.5f, 0.5f, 0.5f };
    obb.orientation = { 0.f,  0.f,  0.f  };

    Vec3 axisZ = result.normal;
    Vec3 axisX = Vec3::orthonormalize(axisZ, { 0.f, 0.f, 1.f });
    obb.orientation = Vec3::orientationToEuler_YXZ(axisX, axisZ);

    s_currentEdits->orientedBoundingBoxes.push_back(obb);
    s_selectedIdx = (int)s_currentEdits->orientedBoundingBoxes.size() - 1;
}

static void pickOBB() {
    if (!s_currentEdits) return;
    auto* camera = Engine::getPlayerCameraPointer();
    if (!camera) return;

    Math::Ray ray{ camera->pos, camera->fwd };
    auto& obbs = s_currentEdits->orientedBoundingBoxes;

    int   bestIdx = -1;
    float bestT   = 1e30f;
    for (int i = 0; i < (int)obbs.size(); i++) {
        auto axes = getAxes(obbs[i]);
        float t;
        if (Math::rayIntersectsOBB(ray, obbs[i].center, axes, obbs[i].halfExtents, t)) {
            if (t < bestT) { bestT = t; bestIdx = i; }
        }
    }
    if (bestIdx >= 0) s_selectedIdx = bestIdx;
}

// ── Save ──────────────────────────────────────────────────────────────────────

// Produces the save path at compile time by walking up from __FILE__.
// This works on the dev machine where __FILE__ reflects the actual source tree.
static std::filesystem::path savePath(const std::string& levelName) {
    std::filesystem::path src = __FILE__;                         // .../level-edit/MarioLevelEdit.cpp
    return src.parent_path() / "level-edits" / (levelName + ".hpp");
}

static void saveLevelEdits() {
    if (!s_currentEdits) return;
    const std::string& name = s_currentContext.levelName;
    if (name.empty()) return;

    std::filesystem::path path = savePath(name);
    std::ofstream f(path);
    if (!f) {
        fprintf(stderr, "[LevelEdit] Failed to open %s for writing\n", path.string().c_str());
        return;
    }

    auto& obbs = s_currentEdits->orientedBoundingBoxes;
    std::string NS = name;
    // Capitalize first letter for namespace name
    if (!NS.empty()) NS[0] = (char)toupper((unsigned char)NS[0]);

    f << "#pragma once\n";
    f << "#include \"../MarioLevelEdit.hpp\"\n\n";
    f << "namespace Mod::Mario::LevelEdit::" << NS << "Edits {\n";
    f << "    inline LevelEdits s_edits = {\n";
    f << "        .orientedBoundingBoxes = {\n";
    for (const auto& obb : obbs) {
        f << "            { "
          << "{ " << obb.center.x      << "f, " << obb.center.y      << "f, " << obb.center.z      << "f }, "
          << "{ " << obb.halfExtents.x << "f, " << obb.halfExtents.y << "f, " << obb.halfExtents.z << "f }, "
          << "{ " << obb.orientation.x << "f, " << obb.orientation.y << "f, " << obb.orientation.z << "f } },\n";
    }
    f << "        }\n";
    f << "    };\n";
    f << "}\n";

    printf("[LevelEdit] Saved %zu OBB(s) to %s\n", obbs.size(), path.string().c_str());
}

// ── ImGui windows ─────────────────────────────────────────────────────────────

static void renderEditWindow() {
    if (s_selectedIdx < 0 || !s_currentEdits) return;
    auto& obbs = s_currentEdits->orientedBoundingBoxes;
    if (s_selectedIdx >= (int)obbs.size()) { s_selectedIdx = -1; return; }

    auto& obb  = obbs[s_selectedIdx];
    auto  axes = getAxes(obb);

    ImGui::Begin("OBB Edit", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("OBB #%d", s_selectedIdx);
    ImGui::Separator();
    ImGui::DragFloat3("Center",       &obb.center.x,      0.05f);
    ImGui::DragFloat3("Half Extents", &obb.halfExtents.x, 0.05f, 0.01f, 100.0f);
    ImGui::DragFloat3("Orientation",  &obb.orientation.x, 1.0f, -180.0f, 180.0f);

    ImGui::Separator();
    ImGui::Text("Nudge (local axes):");
    static float s_nudge = 0.1f;
    ImGui::SetNextItemWidth(80.0f);
    ImGui::DragFloat("Step##nudge", &s_nudge, 0.01f, 0.001f, 10.0f, "%.3f");

    static const char* kLabels[3][2] = { {"+X","-X"}, {"+Y","-Y"}, {"+Z","-Z"} };
    for (int i = 0; i < 3; i++) {
        if (ImGui::Button(kLabels[i][0])) obb.center += axes[i] * s_nudge;
        ImGui::SameLine();
        if (ImGui::Button(kLabels[i][1])) obb.center -= axes[i] * s_nudge;
        if (i < 2) ImGui::SameLine();
    }
    ImGui::End();
}

static void renderManagerWindow() {
    ImGui::Begin("OBB Manager");

    if (!s_currentEdits) {
        ImGui::TextDisabled("No level loaded.");
        ImGui::End();
        return;
    }

    auto& obbs = s_currentEdits->orientedBoundingBoxes;
    ImGui::Text("Level: %s   OBBs: %zu", s_currentContext.levelName.c_str(), obbs.size());
    ImGui::Separator();

    ImGui::BeginChild("##list", ImVec2(0, 180), true);
    for (int i = 0; i < (int)obbs.size(); i++) {
        char label[64];
        snprintf(label, sizeof(label), "OBB #%d  (%.1f, %.1f, %.1f)", i,
                 obbs[i].center.x, obbs[i].center.y, obbs[i].center.z);
        if (ImGui::Selectable(label, s_selectedIdx == i))
            s_selectedIdx = i;
    }
    ImGui::EndChild();

    ImGui::Separator();

    if (ImGui::Button("Add (raycast)")) {
        placeOBB();
    }
    ImGui::SameLine();
    if (ImGui::Button("Duplicate") && s_selectedIdx >= 0 && s_selectedIdx < (int)obbs.size()) {
        obbs.push_back(obbs[s_selectedIdx]);
        s_selectedIdx = (int)obbs.size() - 1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete") && s_selectedIdx >= 0 && s_selectedIdx < (int)obbs.size()) {
        obbs.erase(obbs.begin() + s_selectedIdx);
        if (s_selectedIdx >= (int)obbs.size()) s_selectedIdx = (int)obbs.size() - 1;
    }

    ImGui::Separator();
    if (ImGui::Button("Save .hpp")) {
        saveLevelEdits();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Re-compile to activate.");

    ImGui::End();
}

static void renderCursor() {
    if (!isInputSuppressed())
        return;

    // Halo hides the OS cursor — draw a simple crosshair on the ImGui foreground
    // draw list so the user can see where their cursor is while the editor is open.
    ImVec2 mp = ImGui::GetMousePos();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const float R  = 8.0f;
    const float G  = 2.0f;
    const ImU32 kCursorColor = IM_COL32(255, 255, 255, 220);
    const ImU32 kCursorShadow = IM_COL32(0, 0, 0, 120);
    // Shadow (offset by 1)
    dl->AddLine({mp.x - R + 1, mp.y + 1}, {mp.x + R + 1, mp.y + 1}, kCursorShadow, G);
    dl->AddLine({mp.x + 1, mp.y - R + 1}, {mp.x + 1, mp.y + R + 1}, kCursorShadow, G);
    // Crosshair
    dl->AddLine({mp.x - R, mp.y}, {mp.x + R, mp.y}, kCursorColor, G);
    dl->AddLine({mp.x, mp.y - R}, {mp.x, mp.y + R}, kCursorColor, G);
}

static void renderUI() {
    if (ImGui::IsKeyPressed(ImGuiKey_F6, false)) {
        s_editorOpen = !s_editorOpen;
        if (s_editorOpen) {
            // Refresh in case the level changed since last open.
            s_currentEdits = Index::lookup(s_currentContext.levelName);
        }
    }

    if (!s_editorOpen) return;

    // Tab toggles focus of the OBB Edit window.
    if (ImGui::IsKeyPressed(ImGuiKey_Tab, false)) {
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) {
            ImGui::SetWindowFocus(nullptr);
        } else {
            ImGui::SetWindowFocus("OBB Edit");
        }
    }

    renderEditWindow();
    renderManagerWindow();
    renderCursor();

    if (ImGui::IsKeyPressed(ImGuiKey_Keypad0, false)) placeOBB();
    if (ImGui::IsKeyPressed(ImGuiKey_Keypad1, false)) pickOBB();
}

#endif // ENABLE_LEVEL_EDITOR

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void setContext(LevelEditContext context) {
#ifdef ENABLE_LEVEL_EDITOR
    s_currentContext = std::move(context);
    s_currentEdits   = Index::lookup(s_currentContext.levelName);
    s_selectedIdx    = -1;
#else
    (void)context;
#endif
}

void initHandlers(Spark::ModId modId) {

#ifdef ENABLE_LEVEL_EDITOR
    using Bus = Spark::EventBus<void>;
    Spark::RenderBSPAlbedo::addHandler(modId, +[](void*, Spark::RenderBSPAlbedo::Cursor next) {
        next();
        renderWorld();
    }, nullptr);
    Spark::onRenderDebugUI.addHandler(modId, +[](void*, Bus::Cursor next) {
        renderUI();
        next();
    }, nullptr);
    Spark::UpdatePlayerControlsAndLook::addHandler(modId, +[](void* /*ctx*/, auto next, uint32_t param_1, uint32_t param_2) {
        if (!isInputSuppressed()) {
            next(param_1, param_2);
            return;
        }
    }, nullptr, 10);
#else
    (void)modId;
#endif
}

bool isInputSuppressed() {
#ifdef ENABLE_LEVEL_EDITOR
    return s_editorOpen && ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow);
#else
    return false;
#endif
}

} // namespace Mod::Mario::LevelEdit
