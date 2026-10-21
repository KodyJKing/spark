#pragma once

#include <stdint.h>
#include "math/Vectors.hpp"
#include "memory/Memory.hpp"
#include "../common.hpp"
#include "../map.hpp"
#include "../tag.hpp"

namespace Engine {
    #pragma pack(push, 1)

    struct U32Flagged {
        union {
            struct {
                uint32_t value:31;
                uint32_t flag:1;
            };
            uint32_t raw;
        };
    };

    struct BSPVertex {
        Vec3 pos;
        uint32_t firstEdgeIndex;
    };

    struct BSPEdge {
        uint32_t startVertex, endVertex;    // Indices into the BSPVertex array
        uint32_t forwardEdge, backwardEdge; // Indices into the BSPEdge array
        uint32_t leftSurface, rightSurface; // Indices into the BSPSurface array
    };

    struct BSPPlane {
        Vec3 normal; // Normal vector of the plane
        float distance; // Distance from the origin to the plane
    };

    struct BSPSurface {
        uint32_t planeIndex:31; // Index into the BSPPlane array
        uint32_t isFlipped:1;
        
        uint32_t firstEdgeIndex; // Index into the BSPEdge array

        union {
            uint8_t flags;
            struct {
                uint8_t twoSided:1;
                uint8_t invisible:1;
                uint8_t climbable:1;
                uint8_t breakable:5;
            } bits;
        };
        
        uint8_t breakableSurface;
        uint16_t material;
    };
    static_assert(sizeof(BSPSurface) == 0x0C, "BSPSurface must be 12 bytes long");

    struct BSP3DNode {
        uint32_t plane; // Index into the BSPPlane array
        U32Flagged frontChild; // Index into the BSP3DNode array for the front child (or BSPLeaf if flagged). 0xFFFFFFFF represents solid space.
        U32Flagged backChild;  // Index into the BSP3DNode array for the back child (or BSPLeaf if flagged). 0xFFFFFFFF represents solid space.
    };
    static_assert(sizeof(BSP3DNode) == 0x0C, "BSP3DNode must be 12 bytes long");

    struct BSPLeaf {
        uint16_t flags;
        uint16_t bsp2dRefCount; // Index into the BSPSurface array
        uint32_t firstBSP2DRef; // Index into the BSP2DRef array
    };
    static_assert(sizeof(BSPLeaf) == 0x08, "BSPLeaf must be 8 bytes long");

    struct BSP2DRef {
        U32Flagged plane; // Index into the BSPPlane array, 2D normal will be inverted if flagged.
        U32Flagged bsp2dNode; // Index into the BSP2DNode array (or BSPSurface if flagged).
    };
    static_assert(sizeof(BSP2DRef) == 0x08, "BSP2DRef must be 8 bytes long");

    struct BSP2DNode {
        Vec2 normal;
        float distance; // Distance from the origin to the line in 2D
        U32Flagged leftChild; // Index into the BSP2DNode array (or BSPSurface if flagged).
        U32Flagged rightChild; // Index into the BSP2DNode array (or BSPSurface if flagged).
    };
    static_assert(sizeof(BSP2DNode) == 0x014, "BSP2DNode must be 20 bytes long");

    struct CollisionBSP {
        BlockPointerTyped<BSP3DNode> bsp3DNodes;
        BlockPointerTyped<BSPPlane> planes;
        BlockPointerTyped<BSPLeaf> leaves;
        BlockPointerTyped<BSP2DRef> bsp2DRefs;
        BlockPointerTyped<BSP2DNode> bsp2DNodes;
        BlockPointerTyped<BSPSurface> surfaces;
        BlockPointerTyped<BSPEdge> edges;
        BlockPointerTyped<BSPVertex> vertices;
    };
    #pragma pack(pop)

    static uint64_t bspSignature(CollisionBSP* bsp) {
        if (!bsp) return 0;
        // Hash the count and offset for all BlockPointers on the bsp.
        uint64_t hash = 0;
        const BlockPointer* pointers[] = {
            reinterpret_cast<const BlockPointer*>(&bsp->bsp3DNodes), 
            reinterpret_cast<const BlockPointer*>(&bsp->planes), 
            reinterpret_cast<const BlockPointer*>(&bsp->leaves), 
            reinterpret_cast<const BlockPointer*>(&bsp->bsp2DRefs),
            reinterpret_cast<const BlockPointer*>(&bsp->bsp2DNodes), 
            reinterpret_cast<const BlockPointer*>(&bsp->surfaces),
            reinterpret_cast<const BlockPointer*>(&bsp->edges),
            reinterpret_cast<const BlockPointer*>(&bsp->vertices)
        };
        for (const auto* ptr : pointers) {
            hash ^= std::hash<uint64_t>()(ptr->count);
            hash ^= std::hash<uint64_t>()(ptr->offset);
        }
        return hash;
    }
}
