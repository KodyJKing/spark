#include "mods/devtools/ESPWorld.hpp"
#include "imgui.h"
#include "spark/overlay/ESP.hpp"
#include "halomcc/HaloMCC.hpp"
#include "memory/Memory.hpp"
#include "mods/devtools/VectorProfiler.hpp"
#include "mods/devtools/DrawEntityCollision.hpp"
#include "mods/devtools/DrawBones.hpp"
#include "mods/devtools/DrawEntity.hpp"
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
            drawEntityFrame(highlightEntity);
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

} // namespace Mod::DevTools
