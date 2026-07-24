#pragma once
#include <cstdint>
#include <cstddef>
#include "math/Vectors.hpp"
#include "engine/datum/types.hpp"

namespace Engine::Decomp {

    // Ghidra struct: CollisionData (size 0x50) - output of a line test. Currently only
    // entity-model hits (query type 3, see CollisionQueryObject) are understood; other
    // result types are unexplored. Some fields are unverified - see
    // reversing/notes/LineTestSystem.md for details, dynamic validation, and open
    // questions (particularly _unk44 and the exact role of _backfaceSign).
    struct CollisionData {
        int16_t resultType;        // 0x00 - 0xFFFF = miss, 3 = entity model hit
        char _pad0002[0x12];       // 0x02 - unverified
        float fraction;            // 0x14 - parametric fraction along the ray (0 = origin, 1 = tip)
        float hitPointX;           // 0x18 - world-space hit point
        float hitPointY;           // 0x1c
        float hitPointZ;           // 0x20
        Vec4 plane;                // 0x24 - hit plane normal + distance, world space
        int16_t materialType;      // 0x34 - surface material index (from collision tag's surface table); 0xFFFF if not looked up
        Engine::EntityHandle entityHandle; // 0x38 - entity handle (copied from query)
        int16_t permutationIndex;  // 0x3c - region's active permutation at hit time
        int16_t boneIndex;         // 0x3e - bone index of the closest hit region
        int16_t subRegionIndex;    // 0x40 - sub-region index within the permutation
        int32_t _unk44;            // 0x44 - unverified; written by the recursive BSP walk, semantics unknown
        int32_t _backfaceSign;     // 0x48 - plane (normal + distance) is negated when this is < 0
        uint8_t flags0;            // 0x4c - unverified flags byte
        uint8_t flags1;            // 0x4d - unverified flags byte
        int16_t surfaceIndex;      // 0x4e - surface index within the BSP surface list; -1 if none
    };

    static_assert(sizeof(CollisionData) == 0x50);
    static_assert(offsetof(CollisionData, fraction) == 0x14);
    static_assert(offsetof(CollisionData, plane) == 0x24);
    static_assert(offsetof(CollisionData, entityHandle) == 0x38);
    static_assert(offsetof(CollisionData, surfaceIndex) == 0x4e);

}
