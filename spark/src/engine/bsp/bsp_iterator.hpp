#pragma once

#include "spark/SparkAPI.h"
#include "bsp.hpp"
#include <stack>
#include <queue>

namespace Engine::BSP {

    struct BSPPointers {
        BSP3DNode* bsp3DNodes;
        BSP2DNode* bsp2DNodes;
        BSP2DRef* bsp2DRefs;
        BSPPlane* planes;
        BSPLeaf* leafs;
        BSPSurface* surfaces;
        BSPEdge* edges;
        BSPVertex* vertices;
    }; 

    struct BSPQuery {
        BSPPointers* bsp;
        Vec3 center;
        float radius;
    };

    inline BSPPointers makeBSPPointers(CollisionBSP* bsp) {
        BSPPointers pointers;
        pointers.bsp3DNodes = bsp->bsp3DNodes.get(0, false);
        pointers.bsp2DNodes = bsp->bsp2DNodes.get(0, false);
        pointers.bsp2DRefs = bsp->bsp2DRefs.get(0, false);
        pointers.planes = bsp->planes.get(0, false);
        pointers.leafs = bsp->leaves.get(0, false);
        pointers.surfaces = bsp->surfaces.get(0, false);
        pointers.edges = bsp->edges.get(0, false);
        pointers.vertices = bsp->vertices.get(0, false);
        return pointers;
    }
    
    template<typename Fn>
    inline void forEachLeafOfNode(
        const BSP3DNode* node,
        const BSPQuery& query,
        Fn fn
    ) {
        if (!node) return;
        auto& bsp = *query.bsp;
        BSPPlane* plane = &bsp.planes[node->plane];
        float queryDistance = plane->normal.dot(query.center);
        struct Side { bool intersects; U32Flagged index; };
        Side sides[2] = {
            { queryDistance - query.radius < plane->distance, node->frontChild },
            { queryDistance + query.radius > plane->distance, node->backChild },
        };
        for (auto side : sides) {
            if (side.index.raw == 0xFFFFFFFF) continue;
            if (!side.intersects) continue;
            if (!side.index.flag) {
                BSP3DNode* child = &bsp.bsp3DNodes[side.index.value];
                forEachLeafOfNode(child, query, fn);
            } else {
                BSPLeaf* leaf = &bsp.leafs[side.index.value];
                fn(leaf);
            }
        }
    }

    template<typename Fn>
    inline void forEachLeaf(
        const BSPQuery& query,
        Fn fn
    ) {
        auto& bsp = *query.bsp;
        BSP3DNode* root = &bsp.bsp3DNodes[0];
        forEachLeafOfNode(root, query, fn);
    }

    template<typename Fn>
    inline void forEachSurfaceOf2DNode(
        const BSP2DNode* node,
        const BSPQuery& query,
        Fn fn
    ) {
        if (!node) return;
        auto& bsp = *query.bsp;
        U32Flagged children[2] = {node->leftChild, node->rightChild};
        for (auto const child : children) {
            if (child.flag) {
                BSPSurface* surface = &bsp.surfaces[child.value];
                fn(surface);
            } else {
                BSP2DNode* childNode = &bsp.bsp2DNodes[child.value];
                forEachSurfaceOf2DNode(childNode, query, fn);
            }
        }
    }

    template<typename Fn>
    inline void forEachSurfaceOfLeaf(
        const BSPLeaf* leaf,
        const BSPQuery& query, // Ignored for now.
        Fn fn
    ) {
        if (!leaf) return;
        auto& bsp = *query.bsp;
        for (size_t i = 0; i < leaf->bsp2dRefCount; ++i) {
            BSP2DRef* bsp2dRef = &bsp.bsp2DRefs[leaf->firstBSP2DRef + i];
            if (bsp2dRef->bsp2dNode.flag) {
                BSPSurface* surface = &bsp.surfaces[bsp2dRef->bsp2dNode.value];
                fn(surface);
            } else {
                BSP2DNode* node = &bsp.bsp2DNodes[bsp2dRef->bsp2dNode.value];
                forEachSurfaceOf2DNode(node, query, fn);
            }
        }
    }

    template<typename Fn>
    inline void forEachSurface(
        const BSPPointers* bsp,
        const BSPQuery& query,
        Fn fn
    ) {
        forEachLeaf(query, [&](BSPLeaf* leaf) {
            forEachSurfaceOfLeaf(leaf, query, fn);
        });
    }

    template<typename Fn>
    inline void forEachEdgeOfSurface(
        const BSPPointers* bsp,
        const BSPSurface* surface, 
        Fn fn
    ) {
        if (!surface) return;
        BSPEdge* startEdge = &bsp->edges[surface->firstEdgeIndex];
        BSPEdge* currentEdge = startEdge;
        int limit = 8;
        do {
            fn(currentEdge);
            BSPSurface* left = &bsp->surfaces[currentEdge->leftSurface];
            if (left == surface) {
                currentEdge = &bsp->edges[currentEdge->forwardEdge];
            } else {
                currentEdge = &bsp->edges[currentEdge->backwardEdge];
            }
        } while (currentEdge != startEdge && --limit > 0);
    }

}
