#include "mods/devtools/ESPWorld.hpp"
#include "imgui.h"
#include "spark/overlay/ESP.hpp"
#include "halomcc/HaloMCC.hpp"
#include "memory/Memory.hpp"
#include "mods/devtools/VectorProfiler.hpp"
#include "mods/devtools/DrawEntityCollision.hpp"
#include "mods/devtools/DrawBones.hpp"
#include <vector>

namespace Mod::DevTools {

    static bool shouldDisplay(Engine::Entity* entity) {
        auto c = (Engine::EntityCategory)entity->entityCategory;
        if (c == Engine::EntityCategory_Biped)      return espSettings.filter.biped;
        if (c == Engine::EntityCategory_Vehicle)    return espSettings.filter.vehicle;
        if (c == Engine::EntityCategory_Weapon)     return espSettings.filter.weapon;
        if (c == Engine::EntityCategory_Projectile) return espSettings.filter.projectile;
        if (c == Engine::EntityCategory_Scenery)    return espSettings.filter.scenery;
        if (c == Engine::EntityCategory_Equipment)  return espSettings.filter.equipment;
        return espSettings.filter.other;
    }

    static Vec3 displayPos(Engine::Entity* entity) {
        return entity->rootBonePos;
        if (entity->worldBones.count() > 0) {
            auto bone = entity->worldBones.get(entity, 0);
            if (bone) return bone->pos;
        }
        return entity->rootBonePos;
    }

    void renderDebugWorld() {
        renderESP_entities();

        if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) view.worldBones = !view.worldBones;
        if (ImGui::IsKeyPressed(ImGuiKey_F6, false)) view.collision  = !view.collision;
        if (highlightEntity != nullptr && Memory::isAllocated((uintptr_t)highlightEntity)) {
            if (view.collision)  drawEntityCollision(highlightEntity);
            if (view.worldBones) drawBones(highlightEntity);
        }

        if (ImGui::IsKeyPressed(ImGuiKey_F7, false)) view.renderBsp = !view.renderBsp;
        if (view.renderBsp) renderESP_BSP();

        Mod::DevTools::VectorProfiler::render();
    }

    void renderESP_entities() {
        namespace ESP = Spark::Overlay::ESP;
        Camera& camera = ESP::camera;

        std::vector<Engine::Entity*> entitiesToDraw;
        Engine::foreachEntityRecord([&](Engine::EntityRecord* rec) {
            auto entity = rec->entity();
            if (shouldDisplay(entity)) {
                Vec3 pos = displayPos(entity);
                if ((pos - camera.pos).length() < espSettings.maxDistance)
                    entitiesToDraw.push_back(entity);
            }
        });

        if (!espSettings.anchorHighlight) {
            int   highlightIndex = -1;
            float maxDot         = -1.0f;
            for (int i = 0; i < (int)entitiesToDraw.size(); i++) {
                auto entity   = entitiesToDraw[i];
                Vec3 pos      = displayPos(entity);
                Vec3 toEntity = (pos - camera.pos);
                if (toEntity.length() > espSettings.maxDistance) continue;
                toEntity = toEntity.normalize();
                float dot = toEntity.dot(camera.fwd);
                if (dot > maxDot) { maxDot = dot; highlightIndex = i; }
            }
            highlightEntity = highlightIndex >= 0 ? entitiesToDraw[highlightIndex] : nullptr;
        }

        bool gamePaused = HaloMCC::isPauseMenuOpen();
        byte alpha = gamePaused ? 0x40 : 0xFF;
        for (Engine::Entity* entity : entitiesToDraw) {
            auto  color  = IM_COL32(255, 255, 255, alpha);
            float radius = 0.1f;
            if (entity == highlightEntity) {
                color  = IM_COL32(64, 64, 255, alpha);
                radius = 0.2f;

                float nudgeSpeed = 0.1f;
                if (ImGui::GetIO().KeyShift) nudgeSpeed *= 0.5f;
                if (ImGui::IsKeyPressed(ImGuiKey_Keypad4, false)) entity->pos.x -= nudgeSpeed;
                if (ImGui::IsKeyPressed(ImGuiKey_Keypad6, false)) entity->pos.x += nudgeSpeed;
                if (ImGui::IsKeyPressed(ImGuiKey_Keypad8, false)) entity->pos.y += nudgeSpeed;
                if (ImGui::IsKeyPressed(ImGuiKey_Keypad5, false)) entity->pos.y -= nudgeSpeed;
                if (ImGui::IsKeyPressed(ImGuiKey_Keypad7, false)) entity->pos.z += nudgeSpeed;
                if (ImGui::IsKeyPressed(ImGuiKey_Keypad9, false)) entity->pos.z -= nudgeSpeed;
            }
            ESP::drawCircle(displayPos(entity), radius, color, true);
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
                
                // Render text for surface material
                Vec3 textPos = (p0->pos + p1->pos + p2->pos) / 3.0f;
                auto material = surface->material;
                char materialText[255] = {0};
                sprintf( materialText, "%X", material );
                ESP::drawText( textPos, materialText, color );

                // // Render plane index
                // char planeText[255] = {0};
                // sprintf(planeText, "%X", planeIndex);
                // Vec3 textPos = (p0->pos + p1->pos + p2->pos) / 3.0f;
                // ESP::drawText(textPos, planeText, color);

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
    }

} // namespace Mod::DevTools
