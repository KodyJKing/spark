#pragma once

#include "EditorState.hpp"

#ifdef ENABLE_LEVEL_EDITOR

namespace Mod::Mario::LevelEdit {

// Renders world-space OBB overlays and processes gizmo input for one frame.
void renderWorld(EditorState& state);

// Renders all editor ImGui windows and handles keyboard/mouse input.
void renderUI(EditorState& state);

} // namespace Mod::Mario::LevelEdit

#endif // ENABLE_LEVEL_EDITOR
