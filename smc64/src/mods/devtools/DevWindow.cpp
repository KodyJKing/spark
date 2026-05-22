#include "mods/devtools/DevWindow.hpp"
#include "imgui.h"
#include "haloce/mod/Mod.hpp"
#include "engine/halo1.hpp"
#include "memory/Memory.hpp"
#include "mods/devtools/Interpretations.hpp"
#include "mods/devtools/TagBrowser.hpp"
#include "utils/Strings.hpp"
#include "spark/Spark.hpp"
#include <string>

namespace Mod::DevTools {

    void checkHotKeys() {
        if (ImGui::IsKeyPressed(ImGuiKey_F2, false))
            Mod::DevTools::freezeTime = !Mod::DevTools::freezeTime;
    }

    void devTab() {
        ImGui::Checkbox("Freeze Time", &Mod::DevTools::freezeTime);
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
                interpretations(value);
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
                interpretPointer(entity);
                ImGui::End();
            } else {
                ImGui::Text("Entity not found for handle %X", pointerValue);
            }
            ImGui::EndChild();

            if (ImGui::Button("Tag Browser"))
                showTagBrowser = !showTagBrowser;
            if (showTagBrowser)
                tagBrowser();

            ImGui::Checkbox("ESP", &Spark::showDebugOverlay);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Toggle ESP (F1)");
        }
    }

    void renderPauseMenuTabs() {
        if (ImGui::BeginTabItem("Dev")) {
            devTab();
            ImGui::EndTabItem();
        }
    }

} // namespace Mod::DevTools
