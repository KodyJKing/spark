#include "DevToolsMod.hpp"
#include "imgui.h"
#include "haloce/mod/Mod.hpp"
#include "overlay/ESP.hpp"
#include "overlay/VectorProfiler.hpp"
#include "engine/halo1.hpp"
#include "haloce/mod/modules/mario/Mario.hpp"
#include "halomcc/HaloMCC.hpp"
#include "utils/ImGuiUtils.hpp"
#include <vector>
#include <string>
#include "utils/Utils.hpp"
#include "utils/Strings.hpp"
#include "memory/Memory.hpp"
#include "mods/devtools/TagBrowser.hpp"
#include "mods/devtools/Interpretations.hpp"
#include "cheatengine/Messages.hpp"
#include "mods/devtools/DrawBSP.hpp"
#include "mods/devtools/DrawEntity.hpp"
#include "mods/devtools/DrawBones.hpp"
#include "mods/devtools/DrawEntityCollision.hpp"
#include "spark/RenderBuses.hpp"

#include <Windows.h>

namespace {

    ///////////////////////////////////////////////////////////////////////
    // Forward declarations

    void renderESP();
    void renderESP_world();
    void checkHotKeys();
    void settingsTab();
    void devTab();
    void espWindow();
    void filterSettings();
    void highlightedEntityDetails();
    void renderESP_entities();
    void renderESP_BSP();

    ///////////////////////////////////////////////////////////////////////
    // State

    bool showEsp = false;
    Engine::Entity* highlightEntity = nullptr;

    struct ESPSettings {
        bool anchorHighlight = false;
        float fovScale = 0.627f;
        float maxDistance = 20.0f;
        float maxBSPVertexDistance = 10.0f;
        struct Filter {
            bool biped      = false;
            bool vehicle    = false;
            bool weapon     = false;
            bool projectile = false;
            bool scenery    = false;
            bool equipment  = false;
            bool other      = false;
        } filter = {};
    } espSettings = {};

    struct View {
        bool pos       = true;
        bool vel       = false;
        bool fwd       = false;
        bool up        = false;
        bool angles    = false;
        bool health    = false;
        bool shield    = false;
        bool tag       = false;
        bool tagID     = false;
        bool tagCC     = false;
        bool tagPath   = true;
        bool animation = false;
        bool bones     = false;
        bool worldBones  = true;
        bool collision   = false;
        bool renderBsp   = false;
    } view = {};

    ///////////////////////////////////////////////////////////////////////
    // Render phases (registered on buses)

    void renderDebugUI() {
        checkHotKeys();
        if (showEsp)
            renderESP();
    }

    void renderPauseMenuTabs() {
        if (ImGui::BeginTabItem("Settings")) {
            settingsTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Dev")) {
            devTab();
            ImGui::EndTabItem();
        }
    }

    void renderDebugWorld() {
        renderESP_entities();

        if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) view.worldBones = !view.worldBones;
        if (ImGui::IsKeyPressed(ImGuiKey_F6, false)) view.collision  = !view.collision;
        if (highlightEntity != nullptr && Memory::isAllocated((uintptr_t)highlightEntity)) {
            if (view.collision)  Mod::DevTools::drawEntityCollision(highlightEntity);
            if (view.worldBones) Mod::DevTools::drawBones(highlightEntity);
        }

        if (ImGui::IsKeyPressed(ImGuiKey_F7, false)) view.renderBsp = !view.renderBsp;
        if (view.renderBsp) renderESP_BSP();

        HaloCE::Mod::Mario::debugRender();
        Overlay::ESP::VectorProfiler::render();
    }

    ///////////////////////////////////////////////////////////////////////
    // Hotkeys / settings

    void checkHotKeys() {
        if (ImGui::IsKeyPressed(ImGuiKey_F1, false))
            showEsp = !showEsp;
        if (ImGui::IsKeyPressed(ImGuiKey_F2, false))
            HaloCE::Mod::settings.freezeTime = !HaloCE::Mod::settings.freezeTime;
    }

    void settingsTab() {
        ImGui::SeparatorText("Damage Scale");
        ImGui::SliderFloat("Player", &HaloCE::Mod::settings.playerDamageScale, 0.0f, 5.0f, "%.1f");
        ImGui::SliderFloat("NPC",    &HaloCE::Mod::settings.npcDamageScale,    0.0f, 5.0f, "%.1f");
    }

    void devTab() {
        ImGui::Checkbox("Freeze Time", &HaloCE::Mod::settings.freezeTime);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle freezing of in-game time. (F2)");

        if (ImGui::CollapsingHeader("Tools")) {
            ImGui::BeginChild("##Translate Map Address", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);

            auto renderAddressInput = [](const char* label, char* buffer, size_t bufferSize, const char* outputFormat, auto translateFunc)
            {
                ImGui::Text("%s", label);
                char labelId[255] = {0};
                snprintf(labelId, 255, "##%s", label);
                ImGui::InputText(labelId, buffer, bufferSize);
                uint64_t inputAddress = 0;
                try   { inputAddress = std::stoull(buffer, nullptr, 16); }
                catch (...) { inputAddress = 0; }
                if (inputAddress) {
                    uint64_t translated = 0;
                    if constexpr (std::is_invocable_v<decltype(translateFunc), uint32_t>)
                        translated = translateFunc(static_cast<uint32_t>(inputAddress));
                    else
                        translated = translateFunc(inputAddress);
                    ImGui::Text("-> %p", (void*)translated);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Right click to copy translated address to clipboard");
                    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                        char text[255] = {0};
                        snprintf(text, 255, outputFormat, (void*)translated);
                        ImGui::SetClipboardText(text);
                    }
                }
            };

            static char address[255]  = {0};
            static char address2[255] = {0};
            renderAddressInput("From Map Relative Address", address,  255, "%p", Engine::translateMapAddress);
            renderAddressInput("To Map Relative Address",   address2, 255, "%X", Engine::translateToMapAddress);
            ImGui::EndChild();

            ImGui::BeginChild("##Interpret U32", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
            static char u32input[255] = {0};
            ImGui::InputText("U32 Input", u32input, sizeof(u32input));
            uint32_t value = 0;
            try   { value = std::stoul(u32input, nullptr, 16); }
            catch (...) { value = 0; }
            if (value) {
                ImGui::Text("Interpretations of %X:", value);
                ImGui::Separator();
                Mod::DevTools::interpretations(value);
            }
            ImGui::EndChild();

            ImGui::BeginChild("##Interpret Object Fields", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
            static char addressInput[255] = {0};
            ImGui::InputText("Address", addressInput, sizeof(addressInput));
            uintptr_t pointerValue = 0;
            try   { pointerValue = std::stoull(addressInput, nullptr, 16); }
            catch (...) { pointerValue = 0; }
            Engine::Entity* entity = (Engine::Entity*)pointerValue;
            if (entity && Memory::isAllocated((uintptr_t)entity)) {
                ImGui::Text("Interpreting object fields %p", (void*)entity);
                ImGui::Separator();
                ImGui::Begin("Object Field Interpretations");
                Mod::DevTools::interpretPointer(entity);
                ImGui::End();
            } else {
                ImGui::Text("Entity not found for handle %X", pointerValue);
            }
            ImGui::EndChild();

            if (ImGui::Button("Tag Browser"))
                Mod::DevTools::showTagBrowser = !Mod::DevTools::showTagBrowser;
            if (Mod::DevTools::showTagBrowser)
                Mod::DevTools::tagBrowser();

            ImGui::Checkbox("ESP", &showEsp);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Toggle ESP (F1)");
        }
    }

    ///////////////////////////////////////////////////////////////////////
    // ESP settings window

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

    void espWindow() {
        bool paused = HaloMCC::isPauseMenuOpen();
        ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;
        if (!paused)
            flags |= ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMouseInputs;

        ImGui::Begin("ESP", 0, flags);

        if (ImGui::BeginTabBar("ESP Tabs")) {
            if (ImGui::BeginTabItem("Entities")) {
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
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Camera")) {
                if (paused) ImGui::SliderFloat("Fov Adjust", &espSettings.fovScale, 0.0f, 2.0f);
                Camera& camera = Overlay::ESP::camera;
                ImGui::Text("Pos: %.2f %.2f %.2f", camera.pos.x, camera.pos.y, camera.pos.z);
                ImGui::Text("Fwd: %.2f %.2f %.2f", camera.fwd.x, camera.fwd.y, camera.fwd.z);
                ImGui::Text("Fov: %.2f", camera.fov);
                ImGui::EndTabItem();
            }

#define PLAYER_CONTROLLER_TAB
#ifdef PLAYER_CONTROLLER_TAB
            if (ImGui::BeginTabItem("PlayerController")) {
                auto playerController = Engine::getPlayerControllerPointer();
                ImGui::Text("%p", playerController);

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
                ImGui::EndTabItem();
            }
#endif

#define BSP_TAB
#ifdef BSP_TAB
            if (ImGui::BeginTabItem("BSP")) {
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

                ImGui::EndTabItem();
            }
#endif
            ImGui::EndTabBar();
        }
        ImGui::End();
    }

    ///////////////////////////////////////////////////////////////////////
    // ESP world draw

    bool shouldDisplay(Engine::Entity* entity) {
        auto c = (Engine::EntityCategory)entity->entityCategory;
        if (c == Engine::EntityCategory_Biped)      return espSettings.filter.biped;
        if (c == Engine::EntityCategory_Vehicle)    return espSettings.filter.vehicle;
        if (c == Engine::EntityCategory_Weapon)     return espSettings.filter.weapon;
        if (c == Engine::EntityCategory_Projectile) return espSettings.filter.projectile;
        if (c == Engine::EntityCategory_Scenery)    return espSettings.filter.scenery;
        if (c == Engine::EntityCategory_Equipment)  return espSettings.filter.equipment;
        return espSettings.filter.other;
    }

    Vec3 displayPos(Engine::Entity* entity) {
        return entity->rootBonePos;
    }

    void renderESP_entities() {
        namespace ESP = Overlay::ESP;
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
        namespace ESP = Overlay::ESP;
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
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////
    // ESP settings window entry point

    void renderESP() {
        if (!Engine::isGameLoaded()) return;
        espWindow();
    }

} // namespace

///////////////////////////////////////////////////////////////////////
// DevToolsMod

void DevToolsMod::init() {
    using Bus = EventBus<void>;

    Spark::onRenderDebugUI.addHandler(modId_, +[](void*, Bus::Cursor next) {
        renderDebugUI();
        next();
    }, nullptr);

    Spark::onRenderPauseMenuTabs.addHandler(modId_, +[](void*, Bus::Cursor next) {
        renderPauseMenuTabs();
        next();
    }, nullptr);

    Spark::onRenderDebugWorld.addHandler(modId_, +[](void*, Bus::Cursor next) {
        if (showEsp) renderDebugWorld();
        next();
    }, nullptr);
}
