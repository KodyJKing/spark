#include "mods/devtools/DrawBSP.hpp"

#include "spark/overlay/ESP.hpp"

namespace Mod::DevTools {

    void drawBSPPoints(Engine::CollisionBSP* bsp, Vec3 origin, Vec3 x, Vec3 y, Vec3 z) {
        if ( !bsp ) return;

        uint64_t vertexArrayAddress = Engine::translateMapAddress( bsp->vertices.offset );
        if ( !vertexArrayAddress ) return;
        Engine::BSPVertex* vertices = (Engine::BSPVertex*) vertexArrayAddress;

        for ( uint32_t i = 0; i < bsp->vertices.count; i++ ) {
            Engine::BSPVertex& vertex = vertices[i];
            Vec3 worldPos = x * vertex.pos.x + y * vertex.pos.y + z * vertex.pos.z + origin;
            Spark::Overlay::ESP::drawPoint( worldPos, IM_COL32( 0, 255, 255, 255 ) );
        }
    }

    // This is for small BSPs, so don't use any special culling logic.
    // In all other respects, this is like the old implementation (renderESP_BSP).
    void drawBSP(Engine::CollisionBSP* bsp, Vec3 origin, Vec3 x, Vec3 y, Vec3 z) {
        namespace ESP = Spark::Overlay::ESP;
        Camera &camera = ESP::camera;

        auto toWorld = [&]( Vec3 localPos ) -> Vec3 {
            return x * localPos.x + y * localPos.y + z * localPos.z + origin;
        };

        auto bspVertexCount = bsp->vertices.count;
        if ( bspVertexCount == 0 ) return;
        Engine::BSPVertex* bspVertices = (Engine::BSPVertex*) Engine::translateMapAddress( bsp->vertices.offset );
        if (!bspVertices ) return;

        auto bspEdgeCount = bsp->edges.count;
        if ( bspEdgeCount == 0 ) return;
        auto edgeArrayAddress = Engine::translateMapAddress( bsp->edges.offset );
        if ( !edgeArrayAddress ) return;
        Engine::BSPEdge* bspEdges = (Engine::BSPEdge*) edgeArrayAddress;

        auto bspSurfaceCount = bsp->surfaces.count;
        if ( bspSurfaceCount == 0 ) return;
        Engine::BSPSurface* bspSurfaces = (Engine::BSPSurface*) Engine::translateMapAddress( bsp->surfaces.offset );
        if ( !bspSurfaces ) return;

        auto planes = bsp->planes.get<Engine::BSPPlane>(0);
        
        uint8_t alpha = 0xFF;
        auto color = IM_COL32(255, 255, 0, alpha);
        
        // //// Draw BSP vertices
        // for (uint32_t i = 0; i < bspVertexCount; i++) {
        //     auto vertex = &bspVertices[i];
        //     Vec3 pos = vertex->pos;
        //     ESP::drawPoint(toWorld(pos), color);
        // }

        //// Draw BSP surfaces

        alpha = 0x20;
        color = IM_COL32(255, 255, 255, alpha);

        for (uint32_t i = 0; i < bspEdgeCount; i++) {
            auto edge = &bspEdges[i];

            auto p0 = &bspVertices[edge->startVertex];
            auto p1 = &bspVertices[edge->endVertex];

            uint32_t surfaces[2] = {edge->leftSurface, edge->rightSurface};
            for (uint32_t j = 0; j < 2; j++) {
                if (surfaces[j] >= bspSurfaceCount)
                    continue; // Invalid surface, skip it.
                auto surface = &bspSurfaces[surfaces[j]];
                // Get the first vertex of the first edge of the surface.
                if (surface->firstEdgeIndex >= bspEdgeCount)
                    continue; // Invalid surface, skip it.

                auto firstEdge = &bspEdges[surface->firstEdgeIndex];
                if (firstEdge->startVertex >= bspVertexCount)
                    continue; // Invalid edge, skip it.
                auto firstVertex = &bspVertices[firstEdge->startVertex];

                // // Draw triangle formed between edge and firstVertex, if it's not degenerate.
                if (edge->startVertex == firstEdge->startVertex ||
                    edge->endVertex == firstEdge->startVertex)
                {
                    // Degenerate, skip.
                    continue;
                }

                Engine::BSPPlane* plane = &planes[surface->planeIndex];
                Vec3 normal = plane->normal;
                Vec3 worldNormal = x * normal.x + y * normal.y + z * normal.z;
                Vec3 toCamera = ( camera.pos - toWorld( firstVertex->pos ) );
                if ( worldNormal.dot( toCamera ) < 0.0f )
                    continue;

                auto p2 = firstVertex;

                // Draw the triangle.
                ESP::drawLine(toWorld(p0->pos), toWorld(p1->pos), color);
                ESP::drawLine(toWorld(p1->pos), toWorld(p2->pos), color);
                ESP::drawLine(toWorld(p2->pos), toWorld(p0->pos), color);

                // // Render text for surface material.
                // Vec3 textPos = (p0->pos + p1->pos + p2->pos) / 3.0f;
                // auto material = surface->material;
                // char materialText[255] = {0};
                // sprintf( materialText, "%X", material );
                // ESP::drawText( textPos, materialText, color );

                // // Render normal:
                // if ( plane ) {
                //     Vec3 triCenter = ( p0->pos + p1->pos + p2->pos ) / 3.0f;
                //     Vec3 normalEnd = triCenter + plane->normal * 0.025f;
                //     ESP::drawLine( toWorld( triCenter ), toWorld( normalEnd ), IM_COL32( 255, 0, 0, 255 ) );
                //     ESP::drawPoint( toWorld( triCenter ), IM_COL32( 255, 0, 0, 255 ) );
                // }

            }
        }
    }
    
}
