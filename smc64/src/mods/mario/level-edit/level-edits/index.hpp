#pragma once
#include "../MarioLevelEdit.hpp"
#include <unordered_map>

namespace Mod::Mario::LevelEdit::Index {

    // All level edits, keyed by BSP signature (Engine::getBSPSignature()).
    // To add a new level: run the game on that level, read the signature from
    // Engine::getBSPSignature(), and add an entry here.
    inline std::unordered_map<uint64_t, LevelEdits> s_allEdits = {
        
        // BSP 0x0x5B4FEA6A0330002AULL
        { 0x5B4FEA6A0330002AULL, LevelEdits {
            .orientedBoundingBoxes = {
                { { 53.3375f, -90.9193f, -84.5287f }, { 7.16046f, 5.90961f, 0.5f }, { -0.f, -0.f, 90.f } },
            }
        }},

    };

    // Returns a mutable pointer to the LevelEdits for the given BSP signature,
    // or nullptr if no edits are registered for that level.
    inline LevelEdits* lookup(uint64_t bspSignature) {
        auto it = s_allEdits.find(bspSignature);
        return (it != s_allEdits.end()) ? &it->second : nullptr;
    }

    // Returns a mutable pointer to the LevelEdits for the given BSP signature,
    // inserting an empty entry if none exists yet.
    inline LevelEdits* openOrCreate(uint64_t bspSignature) {
        return &s_allEdits[bspSignature];
    }

}
