#pragma once

// ── Master toggle for the live editor UI ─────────────────────────────────────
// Comment out to compile without the editor.
#define ENABLE_LEVEL_EDITOR

#ifdef ENABLE_LEVEL_EDITOR

#include "MarioLevelEdit.hpp"
#include "imgui.h"

namespace Mod::Mario::LevelEdit {

// Per-axis UI tint colors used by world rendering and ImGui widgets.
// X=dark red, Y=dark green, Z=dark blue (XYZ→RGB convention).
inline const ImVec4 kAxisBg[3] = {
    { 0.42f, 0.10f, 0.10f, 1.f },  // X
    { 0.10f, 0.36f, 0.10f, 1.f },  // Y
    { 0.10f, 0.18f, 0.44f, 1.f },  // Z
};

struct EditorState {
    bool             editorOpen         = false;
    bool             editorInputEnabled = false; // Tab-toggled; suppresses game input
    LevelEditContext currentContext     = {};
    LevelEdits*      currentEdits       = nullptr;
    int              selectedIdx        = -1;
};

} // namespace Mod::Mario::LevelEdit

#endif // ENABLE_LEVEL_EDITOR
