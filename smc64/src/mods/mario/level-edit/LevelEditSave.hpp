#pragma once

#include "EditorState.hpp"

#ifdef ENABLE_LEVEL_EDITOR

namespace Mod::Mario::LevelEdit {

void saveLevelEdits(EditorState& state);

} // namespace Mod::Mario::LevelEdit

#endif // ENABLE_LEVEL_EDITOR
