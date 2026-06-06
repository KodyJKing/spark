#pragma once

#include <stdint.h>
#include "math/Vectors.hpp"
#include "memory/Memory.hpp"
#include "../common.hpp"
#include "../map.hpp"
#include "../tag.hpp"

namespace Engine {
    #pragma pack(push, 1)
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
        uint8_t flags;
        uint8_t breakableSurface;
        uint16_t material;
    };
    static_assert(sizeof(BSPSurface) == 12, "BSPSurface must be 12 bytes long");

    struct CollisionBSP {
        BlockPointer bsp3DNodes, planes, leaves, bsp2DRefs, bsp2DNodes, surfaces, edges, vertices;
    };
    #pragma pack(pop)

    static uint64_t bspSignature(CollisionBSP* bsp) {
        if (!bsp) return 0;
        // Hash the count and offset for all BlockPointers on the bsp.
        uint64_t hash = 0;
        const BlockPointer* pointers[] = {
            &bsp->bsp3DNodes, &bsp->planes, &bsp->leaves, &bsp->bsp2DRefs,
            &bsp->bsp2DNodes, &bsp->surfaces, &bsp->edges, &bsp->vertices
        };
        for (const auto* ptr : pointers) {
            hash ^= std::hash<uint64_t>()(ptr->count);
            hash ^= std::hash<uint64_t>()(ptr->offset);
        }
        return hash;
    }
}
