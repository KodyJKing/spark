#include "EngineInfo.hpp"

#include <string>
#include <vector>
#include <functional>

#include "engine/halo1.hpp"
#include "engine/rendering/index.hpp"

#include "utils/ImGuiUtils.hpp"
#include "imgui.h"

namespace {
    // Widget registry:
    struct Widget {
        std::string name;
        std::function<void()> render;
    };

    std::vector<Widget> widgets;

    struct WidgetRegistrar {
        WidgetRegistrar(const Widget& widget) {
            widgets.push_back(widget);
        }
    };
    
    #define REGISTER_WIDGET(name, renderFunc) \
        static WidgetRegistrar _registrar_##name({#name, renderFunc});
    
    // Widgets:

    REGISTER_WIDGET(Model_Data, []() {
        void* modelData = Engine::getModelDataPointer();
        ImGuiUtils::renderCopyableTextf("Model Data Pointer: ", "%p", modelData);

        // First 32 bytes of the model data:
        ImGui::Text("Bytes:");
        uint8_t* bytes = reinterpret_cast<uint8_t*>(modelData);
        for (int i = 0; i < 32; ++i) {
            ImGui::SameLine();
            ImGui::Text("%02X", bytes[i]);
        }
        ImGui::SameLine();
        ImGui::Text("...");

        // First 8 floats of model data:
        ImGui::Text("Floats:");
        float* floats = reinterpret_cast<float*>(modelData);
        for (int i = 0; i < 8; ++i) {
            ImGui::SameLine();
            ImGui::Text("%f", floats[i]);
        }
        ImGui::SameLine();
        ImGui::Text("...");
    });
}

namespace Mod::DevTools::EngineInfo {
    static bool isOpen = false;
    void open() { isOpen = true; }
    
    static char search[128] = "";
    void searchBar() {
        ImGui::InputText("Search", search, sizeof(search));
    }

    void render() {
        if (!isOpen) return;
        ImGui::Begin("Engine Info", &isOpen);
        searchBar();
        for (const auto& widget : widgets) {
            if (std::string(widget.name).find(search) != std::string::npos) {
                if (ImGui::CollapsingHeader(widget.name.c_str())) {
                    widget.render();
                }
            }
        }
        ImGui::End();
    }
}
