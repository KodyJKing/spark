#pragma once

#include "EditorState.hpp"

#ifdef ENABLE_LEVEL_EDITOR

namespace Mod::Mario::LevelEdit {

void drawOBBWireframe(const OrientedBoundingBox& obb, ImU32 color);

// Draws the three axis arrows for obb and, when obbIdx == selectedIdx,
// submits translate gizmo widgets for this frame.
// Must be called between Gizmo::beginGizmos() and Gizmo::endGizmos().
void drawOBBAxes(OrientedBoundingBox& obb, int obbIdx, int selectedIdx);

} // namespace Mod::Mario::LevelEdit

#endif // ENABLE_LEVEL_EDITOR
