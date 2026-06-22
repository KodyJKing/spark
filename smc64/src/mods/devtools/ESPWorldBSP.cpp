#include "mods/devtools/ESPWorld.hpp"
#include "imgui.h"
#include "spark/overlay/ESP.hpp"
#include "halomcc/HaloMCC.hpp"
#include "engine/halo1.hpp"

namespace Mod::DevTools {

    struct Highlight {
        Vec3 center;
        char text[255];
        float score;
        bool exists = false;

        Vec3 p0, p1, p2;
        Vec3 normal;
    };

    static Highlight currentHighlight;

    void maybeHighlight(Vec3 center, const char* text, Vec3 p0, Vec3 p1, Vec3 p2, Vec3 normal) {
        Vec3 toCenter = center - Spark::Overlay::ESP::camera.pos;
        Vec3 direction = toCenter.normalize();
        float score = direction.dot(Spark::Overlay::ESP::camera.fwd);
        if (!currentHighlight.exists || score > currentHighlight.score) {
            currentHighlight.center = center;
            strncpy(currentHighlight.text, text, 254);
            currentHighlight.text[254] = '\0';
            currentHighlight.score = score;
            currentHighlight.p0 = p0;
            currentHighlight.p1 = p1;
            currentHighlight.p2 = p2;
            currentHighlight.normal = normal;
            currentHighlight.exists = true;
        }
    }

    void renderHighlight() {
        namespace ESP = Spark::Overlay::ESP;

        if (currentHighlight.exists) {
            auto color = IM_COL32(255, 255, 0, 0x40);
            
            ESP::drawLine(currentHighlight.p0, currentHighlight.p1, color);
            ESP::drawLine(currentHighlight.p1, currentHighlight.p2, color);
            ESP::drawLine(currentHighlight.p2, currentHighlight.p0, color);
            
            Spark::Overlay::ESP::drawText(currentHighlight.center, currentHighlight.text, color);

            ESP::drawLine(currentHighlight.center, currentHighlight.center + currentHighlight.normal * 0.025f, color);

            // Copy on ctrl-c
            if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_C)) {
                ImGui::SetClipboardText(currentHighlight.text);
            }
        }
    }

    void renderESP_BSP() {
        namespace ESP = Spark::Overlay::ESP;
        Camera& camera = ESP::camera;

        uint32_t bspVertexCount = Engine::getBSPVertexCount();
        Engine::BSPVertex* bspVertices = Engine::getBSPVertexArray();
        if (!bspVertices || bspVertexCount == 0) return;

        uint32_t bspEdgeCount = Engine::getBSPEdgeCount();
        Engine::BSPEdge* bspEdges = Engine::getBSPEdgeArray();
        if (!bspEdges || bspEdgeCount == 0) return;

        uint32_t bspSurfaceCount = Engine::getBSPSurfaceCount();
        Engine::BSPSurface* bspSurfaces = Engine::getBSPSurfaceArray();
        if (!bspSurfaces || bspSurfaceCount == 0) return;

        uint32_t planeCount = Engine::getBSPPlaneCount();
        auto planes = Engine::getBSPPlaneArray();
        if (!planes || planeCount == 0) return;

        bool gamePaused = HaloMCC::isPauseMenuOpen();
        byte alpha = gamePaused ? 0x10 : 0x20;
        auto color = IM_COL32(0, 255, 0, alpha);

        // Reset the current highlight at the beginning of each frame
        currentHighlight.exists = false;

        for (uint32_t i = 0; i < bspEdgeCount; i++) {
            auto edge = &bspEdges[i];
            auto p0   = &bspVertices[edge->startVertex];
            auto p1   = &bspVertices[edge->endVertex];

            if ((p0->pos - camera.pos).length() > espSettings.maxBSPVertexDistance ||
                (p1->pos - camera.pos).length() > espSettings.maxBSPVertexDistance)
                continue;

            uint32_t surfaces[2] = {edge->leftSurface, edge->rightSurface};
            for (uint32_t j = 0; j < 2; j++) {
                if (surfaces[j] >= bspSurfaceCount) continue;
                auto surface = &bspSurfaces[surfaces[j]];
                if (surface->firstEdgeIndex >= bspEdgeCount) continue;
                auto firstEdge = &bspEdges[surface->firstEdgeIndex];
                if (firstEdge->startVertex >= bspVertexCount) continue;
                auto p2 = &bspVertices[firstEdge->startVertex];
                if (edge->startVertex == firstEdge->startVertex ||
                    edge->endVertex   == firstEdge->startVertex) continue;
                if ((p2->pos - camera.pos).length() > espSettings.maxBSPVertexDistance) continue;

                ESP::drawLine(p0->pos, p1->pos, color);
                ESP::drawLine(p1->pos, p2->pos, color);
                ESP::drawLine(p2->pos, p0->pos, color);

                auto planeIndex = surface->planeIndex;
                
                // // Render text for surface material
                // Vec3 textPos = (p0->pos + p1->pos + p2->pos) / 3.0f;
                // auto material = surface->material;
                // char materialText[255] = {0};
                // sprintf( materialText, "%X", material );
                // ESP::drawText( textPos, materialText, color );

                // Render plane index
                char planeText[255] = {0};
                sprintf(planeText, "%X", planeIndex);
                Vec3 textPos = (p0->pos + p1->pos + p2->pos) / 3.0f;
                ESP::drawText(textPos, planeText, color);

                maybeHighlight(textPos, planeText, p0->pos, p1->pos, p2->pos, planeIndex < planeCount ? planes[planeIndex].normal : Vec3());

                // Render normal:
                if (planeIndex < planeCount) {
                    auto plane = &planes[planeIndex];
                    Vec3 normal = plane->normal;
                    Vec3 triCenter = ( p0->pos + p1->pos + p2->pos ) / 3.0f;
                    Vec3 normalEnd = triCenter + plane->normal * 0.025f;
                    ESP::drawLine( triCenter, normalEnd, IM_COL32( 255, 0, 0, 255 ) );
                    ESP::drawPoint( triCenter, IM_COL32( 255, 0, 0, 255 ) );
                }
            }
        }

        renderHighlight();
    }

} // namespace Mod::DevTools
