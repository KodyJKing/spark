#pragma once

#include <stdint.h>
#include <string>

#include "memory/Memory.hpp"

#include "../tag.hpp"
#include "../bsp/index.hpp"
#include "spark/SparkAPI.h"

namespace Engine {

    struct CollisionTagData {
        char pad[0x28C];
        BlockPointer collisionNodes;
    };
    static_assert(offsetof(CollisionTagData, collisionNodes.offset) == 0x290, "CollisionTagData::collisionNodes.count offset is not 0x290 bytes");

    struct CollisionNode {
        // NOTE (2026-07-23, see reversing/notes/LineTestSystem.md "_lineTestVsEntityCollisionRegions"
        // section): `name`/`region`/`parentNode`/`nextSiblingNode`/`firstChildNode` below are an
        // UNVERIFIED guess. A live decompile of _lineTestVsEntityCollisionRegions (0xC47940) reads a
        // binary int16_t permutation-selector at +0x20 (used to index Entity+0x13c's per-instance
        // permutation-state array, clamped to collisionBsps.count, to pick the ONE active BSP for
        // this node) - that falls inside this guessed 0x2C-byte name field. Halo tag names are
        // conventionally 32 bytes (0x20), which would put a real field boundary exactly at +0x20.
        // Only `collisionBsps` (+0x34/+0x38, asserted below) is confirmed correct independently.
        char name[0x20];
        uint16_t permutationIndex;
        char pad_0x22[0xA];
        uint16_t region;
        uint16_t parentNode;
        uint16_t nextSiblingNode;
        uint16_t firstChildNode;
        BlockPointer collisionBsps;
    };
    static_assert(sizeof(CollisionNode) == 0x40, "CollisionNode size is not 0x40 bytes");
    static_assert(offsetof(CollisionNode, collisionBsps.offset) == 0x38, "CollisionNode::collisionBsps offset is not 0x38 bytes");

    SPARK_API uint32_t collisionGeometryTagId(Tag* objectTag);

    SPARK_API Tag* getCollisionGeometryTag( Tag* objectTag );

    SPARK_API CollisionBSP* getObjectCollisionBSP( Tag* objectCollTag );
}
