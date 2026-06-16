#pragma once
#include "../MarioLevelEdit.hpp"
#include "a50.hpp"
#include <unordered_map>
#include <string>

namespace Mod::Mario::LevelEdit::Index {
    // Returns a mutable pointer to the static LevelEdits for the given level name,
    // or nullptr if no edits are registered for that level.
    inline LevelEdits* lookup(const std::string& levelName) {
        static std::unordered_map<std::string, LevelEdits*> s_map = {
            { "a50", &A50Edits::s_edits },
        };
        auto it = s_map.find(levelName);
        return (it != s_map.end()) ? it->second : nullptr;
    }
}