#include "mods/devtools/ESPWindow.hpp"
#include "imgui.h"
#include "spark/overlay/ESP.hpp"
#include "engine/halo1.hpp"
#include "halomcc/HaloMCC.hpp"
#include "memory/Memory.hpp"
#include "mods/devtools/cheatengine/Messages.hpp"
#include "utils/ImGuiUtils.hpp"
#include <string>

namespace Mod::DevTools {

    void filterSettings() {
        ImGui::Checkbox("Biped",      &espSettings.filter.biped);
        ImGui::Checkbox("Vehicle",    &espSettings.filter.vehicle);
        ImGui::Checkbox("Weapon",     &espSettings.filter.weapon);
        ImGui::Checkbox("Projectile", &espSettings.filter.projectile);
        ImGui::Checkbox("Scenery",    &espSettings.filter.scenery);
        ImGui::Checkbox("Equipment",  &espSettings.filter.equipment);
        ImGui::Checkbox("Other",      &espSettings.filter.other);
    }

    void highlightedEntityDetails() {
        Engine::Entity* entity = highlightEntity;
        if (entity == nullptr || !Memory::isAllocated((uintptr_t)entity))
            return;

        bool paused = HaloMCC::isPauseMenuOpen();

#define VIEW_TOGGLE(name)                         \
    if (paused) {                                 \
        ImGui::Checkbox("##" #name, &view.name);  \
        ImGui::SameLine();                        \
    }

        auto tag = entity->tag();
        if ((!paused || ImGui::CollapsingHeader("Tag")) && tag) {
            VIEW_TOGGLE(tag);
            if (paused || view.tag)
                ImGui::Text("Tag: %p", tag);

            VIEW_TOGGLE(tagID);
            if (paused || view.tagID)
                ImGui::Text("Tag ID: %X", entity->tagID);

            std::string groupID = tag->groupIDStr();
            VIEW_TOGGLE(tagCC);
            if (paused || view.tagCC)
                ImGui::Text("Tag GroupID: %s", groupID.c_str());

            VIEW_TOGGLE(tagPath);
            const char* path = tag->getResourcePath();
            const char* nullPath = "null";
            if (!path) path = nullPath;
            if (paused || view.tagPath)
                ImGui::Text("Tag Path: %s", path);

            // Category
            auto category = entity->entityCategory;
            VIEW_TOGGLE(tagCategory);
            if (paused || view.tagCategory)
                ImGui::Text("Category: %d", category);   
        }

        if (!paused || ImGui::CollapsingHeader("Entity")) {
            auto pos = entity->pos;
            auto vel = entity->vel;
            VIEW_TOGGLE(pos);
            if (paused || view.pos)
                ImGui::Text("Position: %.2f, %.2f, %.2f", pos.x, pos.y, pos.z);
            VIEW_TOGGLE(vel);
            if (paused || view.vel)
                ImGui::Text("Velocity: %.2f, %.2f, %.2f", vel.x, vel.y, vel.z);
            VIEW_TOGGLE(fwd);
            if (paused || view.fwd)
                ImGui::Text("Forward: %.2f, %.2f, %.2f", entity->fwd.x, entity->fwd.y, entity->fwd.z);
            VIEW_TOGGLE(up);
            if (paused || view.up)
                ImGui::Text("Up: %.2f, %.2f, %.2f", entity->up.x, entity->up.y, entity->up.z);
            VIEW_TOGGLE(angles);
            if (paused || view.angles) {
                Vec3 angles = orientationToEulerAngles(entity->fwd, entity->up) * (180.0f / 3.14159265f);
                ImGui::Text("Angles: Yaw: %.2f, Pitch: %.2f, Roll: %.2f", angles.x, angles.y, angles.z);
            }
            VIEW_TOGGLE(health);
            if (paused || view.health)
                ImGui::Text("Health: %.2f", entity->health);
            VIEW_TOGGLE(shield);
            if (paused || view.shield)
                ImGui::Text("Shield: %.2f", entity->shield);
            VIEW_TOGGLE(animation);
            if (paused || view.animation)
                ImGui::Text("AnimSet: %X, Anim: %d, Frame: %d, %d bones",
                    entity->animSetTagID, entity->animId, entity->animFrame, entity->bones.count());
            VIEW_TOGGLE(bones);
            if (paused || view.bones) {
                auto boneTransforms = entity->getBoneTransforms();
                ImGui::Text("Bones: %p", boneTransforms);
                if (view.bones && boneTransforms) {
                    ImGui::BeginChild("Bones", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AutoResizeX);
                    for (int i = 0; i < entity->bones.count(); i++) {
                        auto bone = boneTransforms[i];
                        auto bpos = bone.translation;
                        auto rot  = bone.rotation;
                        ImGui::Text("%02d {%.2f, %.2f, %.2f, %.2f} {%.2f, %.2f, %.2f}",
                            i, rot.x, rot.y, rot.z, rot.w, bpos.x, bpos.y, bpos.z);
                    }
                    ImGui::EndChild();
                }
            }
            VIEW_TOGGLE(worldBones);
            if (paused || view.worldBones) {
                auto numberOfWorldBones = entity->worldBones.count();
                (void)entity->worldBones.get(entity, 0);
                ImGui::Text("World Bones: %d", numberOfWorldBones);
            }
            VIEW_TOGGLE(collision);
            if (paused) ImGui::Text("Collision");
        }
#undef VIEW_TOGGLE
    }

    void entitiesTab(bool paused) {
        ImGui::Text("%p", highlightEntity);

        ImGui::SameLine();
        bool copyHotkeyDown = ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(ImGuiKey_C, false);
        if (copyHotkeyDown) ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 255, 0, 255));
        if (ImGui::Button("copy") || copyHotkeyDown) {
            char text[255] = {0};
            snprintf(text, 255, "%p", highlightEntity);
            ImGui::SetClipboardText(text);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Copy pointer to clipboard (Shift+C)");
        if (copyHotkeyDown) ImGui::PopStyleColor();

        ImGui::SameLine();
        if (ImGui::Button("browse"))
            CE::Messages::openHexView((uintptr_t)highlightEntity);

        ImGui::SameLine();
        ImGui::Checkbox("anchor", &espSettings.anchorHighlight);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Keep current entity highlighted (Shift+V)");
        bool anchorHotkeyDown = ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(ImGuiKey_V, false);
        if (anchorHotkeyDown) espSettings.anchorHighlight = !espSettings.anchorHighlight;

        if (paused && ImGui::CollapsingHeader("Filter")) {
            int maxDistInt = (int)espSettings.maxDistance;
            ImGui::SliderInt("Max Distance", &maxDistInt, 1, 200, "%d");
            espSettings.maxDistance = (float)maxDistInt;
            filterSettings();
        }

        highlightedEntityDetails();
    }

    void cameraTab(bool paused) {
        if (paused) ImGui::SliderFloat("Fov Adjust", &espSettings.fovScale, 0.0f, 2.0f);
        Camera& camera = Spark::Overlay::ESP::camera;
        ImGui::Text("Pos: %.2f %.2f %.2f", camera.pos.x, camera.pos.y, camera.pos.z);
        ImGui::Text("Fwd: %.2f %.2f %.2f", camera.fwd.x, camera.fwd.y, camera.fwd.z);
        ImGui::Text("Fov: %.2f", camera.fov);
    }

#define PLAYER_CONTROLLER_TAB
#ifdef PLAYER_CONTROLLER_TAB
    void playerTab() {
        auto playerHandle = Engine::getPlayerHandle();
        ImGui::Text("Player Controller Handle: %X", playerHandle);

        auto playerEntity = Engine::getPlayerEntity();
        ImGui::Text("Player Entity: %p", playerEntity);

        if (playerEntity) {
            // Vehicle handle:
            uint32_t vehicleHandle = playerEntity->vehicleHandle;
            ImGui::Text("Vehicle Handle: %X", vehicleHandle);

            // Parent handle:
            uint32_t parentHandle = playerEntity->parentHandle;
            ImGui::Text("Parent Handle: %X", parentHandle);

            // Child handle:
            uint32_t childHandle = playerEntity->childHandle;
            ImGui::Text("Child Handle: %X", childHandle);
        }

        // Player input:
        bool inputDisabled = Engine::isPlayerInputDisabled();
        ImGui::Text("Player Input Disabled: %s", inputDisabled ? "Yes" : "No");

        auto playerController = Engine::getPlayerControllerPointer();
        ImGui::Text("Player Controller: %p", playerController);

        uint32_t actions = playerController->actions;
        for (int i = 0; i < 32; i++) {
            if (i > 0 && i % 8 != 0) ImGui::SameLine();
            char text[255] = {0};
            snprintf(text, 255, "##flag%d", i);
            ImGui::CheckboxFlags(text, &actions, 1 << i);
        }

        ImVec4 green = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
        ImVec4 white = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
#define SHOW_ACTION_1(name) ImGui::TextColored((actions & Engine::PlayerActionFlags::name) ? green : white, #name);
#define SHOW_ACTION_2(name) ImGui::SameLine(); SHOW_ACTION_1(name);
        SHOW_ACTION_1(crouch); SHOW_ACTION_2(jump);  SHOW_ACTION_2(flash); SHOW_ACTION_2(melee);
        SHOW_ACTION_1(reload); SHOW_ACTION_2(shoot); SHOW_ACTION_2(grenade1); SHOW_ACTION_2(grenade2);
#undef SHOW_ACTION_2
#undef SHOW_ACTION_1
        ImGui::Text("WalkX %.2f", playerController->walkX);
        ImGui::Text("WalkY %.2f", playerController->walkY);
    }
#endif

#define BSP_TAB
#ifdef BSP_TAB
    void bspTab() {
        ImGui::Checkbox("Render BSP", &view.renderBsp);

        uintptr_t bspPointer = Engine::getBSPPointer();
        char bspPointerStr[255] = {0};
        snprintf(bspPointerStr, 255, "%p", (void*)bspPointer);
        ImGuiUtils::renderCopyableText("BSP Pointer", bspPointerStr);

        uint32_t bspVertexCount = Engine::getBSPVertexCount();
        ImGui::Text("BSP Vertices: %d", bspVertexCount);

        Engine::BSPVertex* bspVertices = Engine::getBSPVertexArray();
        char bspVerticesStr[255] = {0};
        snprintf(bspVerticesStr, 255, "%p", bspVertices);
        ImGuiUtils::renderCopyableText("BSP Vertex Array", bspVerticesStr);

        uint32_t bspEdgeCount = Engine::getBSPEdgeCount();
        ImGui::Text("BSP Edges: %d", bspEdgeCount);

        Engine::BSPEdge* bspEdges = Engine::getBSPEdgeArray();
        char bspEdgesStr[255] = {0};
        snprintf(bspEdgesStr, 255, "%p", bspEdges);
        ImGuiUtils::renderCopyableText("BSP Edge Array", bspEdgesStr);

        uint64_t bspSignature = Engine::getBSPSignature();
        ImGui::Text("BSP Signature: %p", (void*)bspSignature);
    }
#endif

    void espWindow() {
        bool paused = HaloMCC::isPauseMenuOpen();
        ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;
        if (!paused)
            flags |= ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMouseInputs;

        ImGui::Begin("ESP", 0, flags);

        if (ImGui::BeginTabBar("ESP Tabs")) {
            if (ImGui::BeginTabItem("Entities")) {
                entitiesTab(paused);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Camera")) {
                cameraTab(paused);
                ImGui::EndTabItem();
            }
#ifdef PLAYER_CONTROLLER_TAB
            if (ImGui::BeginTabItem("Player")) {
                playerTab();
                ImGui::EndTabItem();
            }
#endif
#ifdef BSP_TAB
            if (ImGui::BeginTabItem("BSP")) {
                bspTab();
                ImGui::EndTabItem();
            }
#endif
            ImGui::EndTabBar();
        }
        ImGui::End();
    }

    void renderESP() {
        if (!Engine::isGameLoaded()) return;
        espWindow();
    }

} // namespace Mod::DevTools
