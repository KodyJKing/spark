#include "MarioWeaponOffset.hpp"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "spark/RenderBuses.hpp"
#include "spark/EventBus.hpp"
#include "MarioModel.hpp"
#include "engine/entity/entity_list.hpp"

#include <cstdio>
#include <string>
#include <unordered_map>

// #define MARIO_WEAPON_OFFSET_UI 1

namespace HaloCE::Mod::Mario::MarioWeaponOffset {

    std::unordered_map<std::string, Offset> offsetMap = {
        { "weapons\\pistol\\pistol", { 0.0450f, -0.0010f, 0.0120f } },
        { "weapons\\plasma pistol\\plasma pistol", { 0.0560f, -0.0020f, 0.0410f } },
        { "weapons\\plasma rifle\\plasma rifle", { 0.0450f, -0.0010f, 0.0060f } },
        { "weapons\\assault rifle\\assault rifle", { 0.0480f, 0.0160f, -0.0210f } },
        { "weapons\\needler\\needler", { 0.0620f, -0.0020f, -0.0090f } },
        { "weapons\\rocket launcher\\rocket launcher", { -0.0570f, -0.0020f, 0.0180f } },
        { "weapons\\shotgun\\shotgun", { 0.1230f, 0.0090f, 0.0380f } },
    };

    Vec3 offset = { 0.05f, 0.0f, 0.0f };

    static bool  sOpen = true;
    static float sStep = 0.001f;

    void registerHandlers(Spark::ModId modId) {
        #ifdef MARIO_WEAPON_OFFSET_UI
        using Bus = Spark::EventBus<void>;

        Spark::onRenderDebugUI.addHandler(modId, +[](void*, Bus::Cursor next) {

            // Numpad 5 toggles the window.
            if (ImGui::IsKeyPressed(ImGuiKey_Keypad5, false))
                sOpen = !sOpen;

            if (sOpen) {
                ImGui::Begin("Weapon Offset", &sOpen, ImGuiWindowFlags_AlwaysAutoResize);

                ImGui::TextUnformatted("Position offset in left-hand bone local space");
                ImGui::Separator();

                ImGui::DragFloat("X (fwd)",   &offset.x, 0.001f, -1.f, 1.f, "%.4f");
                ImGui::DragFloat("Y (right)",  &offset.y, 0.001f, -1.f, 1.f, "%.4f");
                ImGui::DragFloat("Z (up)",     &offset.z, 0.001f, -1.f, 1.f, "%.4f");

                ImGui::Spacing();
                ImGui::DragFloat("Nudge step", &sStep, 0.0001f, 0.0001f, 0.1f, "%.4f");
                ImGui::Spacing();

                // Numpad nudge keys — active when the window is focused.
                // Ctrl = 0.1× step.
                const float step = ImGui::GetIO().KeyCtrl ? sStep * 0.1f : sStep;
                if (ImGui::IsKeyPressed(ImGuiKey_Keypad4, true)) offset.x -= step;
                if (ImGui::IsKeyPressed(ImGuiKey_Keypad6, true)) offset.x += step;
                if (ImGui::IsKeyPressed(ImGuiKey_Keypad2, true)) offset.y -= step;
                if (ImGui::IsKeyPressed(ImGuiKey_Keypad8, true)) offset.y += step;
                if (ImGui::IsKeyPressed(ImGuiKey_Keypad3, true)) offset.z -= step;
                if (ImGui::IsKeyPressed(ImGuiKey_Keypad9, true)) offset.z += step;

                ImGui::TextDisabled("Numpad 4/6 = X  |  2/8 = Y  |  3/9 = Z");
                ImGui::TextDisabled("Hold Ctrl for 0.1x step   |   Numpad 5 = toggle");

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::Button("Reset")) {
                    offset = Offset{};
                }
                ImGui::SameLine();
                if (ImGui::Button("Copy")) {
                    // Resolve the held weapon's tag path for context.
                    std::string tagPath;
                    uint32_t weaponHandle = MarioModel::playerWeaponHandle();
                    if (weaponHandle != 0xFFFFFFFF) {
                        auto* rec = Engine::getEntityRecord(weaponHandle);
                        if (rec) {
                            auto* ent = rec->entity();
                            if (ent) {
                                const char* path = ent->getTagResourcePath();
                                if (path) tagPath = path;
                            }
                        }
                    }

                    char buf[512];
                    if (!tagPath.empty()) {
                        // Escape backslashes for a C++ string literal.
                        std::string escaped;
                        for (char c : tagPath)
                            escaped += (c == '\\') ? "\\\\" : std::string(1, c);
                        snprintf(buf, sizeof(buf),
                            "{ \"%s\", { %.4ff, %.4ff, %.4ff } },",
                            escaped.c_str(), offset.x, offset.y, offset.z);
                    } else {
                        snprintf(buf, sizeof(buf), "{ %.4ff, %.4ff, %.4ff }",
                            offset.x, offset.y, offset.z);
                    }
                    ImGui::SetClipboardText(buf);
                }

                ImGui::End();
            }

            next();
        }, nullptr);
        #endif
    }

    void getWeaponOffset(uint32_t weaponHandle, Offset& outOffset) {
        if (weaponHandle != 0xFFFFFFFF) {
            auto* rec = Engine::getEntityRecord(weaponHandle);
            if (rec) {
                auto* ent = rec->entity();
                if (ent) {
                    const char* path = ent->getTagResourcePath();
                    if (path) {
                        auto it = offsetMap.find(path);
                        if (it != offsetMap.end()) {
                            outOffset = it->second;
                            return;
                        }
                    }
                }
            }
        }
        outOffset = offset;
    }

}
