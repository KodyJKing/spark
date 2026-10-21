#pragma once
#include <cstdint>
#include <cstddef>
#include "engine/datum/types.hpp"

namespace Engine::Decomp {

    // Ghidra struct: CollisionQueryObject (size 0x3C). Partial layout - only the fields
    // accessed by _lineTestVsEntityModel are known. Unidentified - keep here (not
    // engine/types) until more of it is confirmed, then flag for promotion.
    // See reversing/notes/LineTestSystem.md for what's known about the query dispatch.
    struct CollisionQueryObject {
        int16_t queryType;              // 0x00 - dispatch type; 3 = entity model line test
        char _pad0002[0x36];            // 0x02 - unverified
        Engine::EntityHandle entityHandle; // 0x38 - entity to test against (type 3 only)
    };

    static_assert(sizeof(CollisionQueryObject) == 0x3C);
    static_assert(offsetof(CollisionQueryObject, entityHandle) == 0x38);

}
