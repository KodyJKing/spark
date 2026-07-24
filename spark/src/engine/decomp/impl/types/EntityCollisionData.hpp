#pragma once
#include <cstdint>
#include <cstddef>
#include "engine/datum/types.hpp"
#include "engine/math.hpp"
#include "engine/tags/collision.hpp"

namespace Engine::Decomp {

    // Ghidra struct: EntityCollisionData (size 0x20). Populated by getEntityCollisionData
    // (offset 0xC47798, not yet hand-decompiled - small/clean enough per decomp.md's
    // scope-selection rule) and consumed by _lineTestVsEntityCollisionRegions (offset
    // 0xC47940, not yet hand-decompiled) and _lineTestVsEntityModel.
    // See reversing/notes/LineTestSystem.md.
    struct EntityCollisionData {
        Engine::EntityHandle entityHandle;           // 0x00
        Engine::CollisionTagData* collisionTagData;  // 0x08 - matches Engine::CollisionTagData
                                                      //        (collisionNodes field lines up at 0x290)
        void* _unk10;                                // 0x10 - per-bone permutation-selector byte array
                                                      //        on the entity (around Entity+0x13c); unverified
        Engine::Transform* boneTransforms;           // 0x18 - world-space bone transform array
    };

    static_assert(sizeof(EntityCollisionData) == 0x20);
    static_assert(offsetof(EntityCollisionData, collisionTagData) == 0x08);
    static_assert(offsetof(EntityCollisionData, boneTransforms) == 0x18);

}
