#include "InspectDX11.hpp"
#include "DX11Defs.hpp"
#include "InspectDX11State.hpp"
#include "engine/rendering/index.hpp"
#include "utils/ImGuiUtils.hpp"
#include "imgui.h"

#include <cstdint>
#include <vector>

namespace Mod::DevTools::InspectDX11 {

    static bool isOpen = false;
    
    void open() {
        isOpen = true;
    }

    void renderTableLabel(const char* label) {
        ImGui::Text("%s", label);
    }

    void renderVFTable(void* table, const std::vector<const char*>& functionNames, const char* searchFilter) {
        if (!table) return;

        static int hoveredRow = -1;
        auto checkHovered = [&](int row) {
            if (ImGui::IsItemHovered()) {
                hoveredRow = row;
            }
        };
        
        ImGui::BeginTable("VTable", 3);
        ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, 4 * ImGui::GetFontSize());
        ImGui::TableSetupColumn("Function");
        ImGui::TableSetupColumn("Address"); 
        
        ImGui::TableHeadersRow();
        for (size_t i = 0; i < functionNames.size(); ++i) {
            if (searchFilter && searchFilter[0] != '\0' && !strstr(functionNames[i], searchFilter)) {
                continue;
            }

            bool isHovered = (hoveredRow == i);
            if (isHovered) {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255));
            }
            
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%zu", i);
            checkHovered(i);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", functionNames[i]);
            checkHovered(i);
            ImGui::TableSetColumnIndex(2);
            ImGuiUtils::renderCopyableTextf("", "%p", (void*)((uintptr_t*)table)[i]);
            checkHovered(i);

            if (isHovered) {
                ImGui::PopStyleColor();
            }
        }
        ImGui::EndTable();
    }

    void renderResources() {
        if (State::collectingMappedResources) {
            if (ImGui::Button("Stop Collecting Mapped Resources")) {
                State::setCollectingMappedResources(false);
            }
        } else {
            if (ImGui::Button("Start Collecting Mapped Resources")) {
                State::setCollectingMappedResources(true);
            }
        }

        D3D11_RESOURCE_DIMENSION resourceDimension;

        ImGui::BeginTable("Resources", 3);
        ImGui::TableSetupColumn("Resource");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("BindFlags");
        ImGui::TableHeadersRow();
        for (const auto& resource : State::resources) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGuiUtils::renderCopyableTextf("", "%p", (void*)resource);
            ImGui::TableSetColumnIndex(1);

            resource->GetType(&resourceDimension);
            const char* resourceTypeStr = "";
            switch (resourceDimension) {
                case D3D11_RESOURCE_DIMENSION_BUFFER: resourceTypeStr = "Buffer"; break;
                case D3D11_RESOURCE_DIMENSION_TEXTURE1D: resourceTypeStr = "Texture1D"; break;
                case D3D11_RESOURCE_DIMENSION_TEXTURE2D: resourceTypeStr = "Texture2D"; break;
                case D3D11_RESOURCE_DIMENSION_TEXTURE3D: resourceTypeStr = "Texture3D"; break;
                default: resourceTypeStr = "Unknown"; break;
            }
            ImGui::Text("%s", resourceTypeStr);

            if (resourceDimension == D3D11_RESOURCE_DIMENSION_BUFFER) {
                ID3D11Buffer* buffer = static_cast<ID3D11Buffer*>(resource);
                D3D11_BUFFER_DESC desc;
                buffer->GetDesc(&desc);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%u", desc.BindFlags);
            } else {
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("-");
            }
        }
        ImGui::EndTable();
    }

    void renderVFTables() {
        ID3D11DeviceContext* context = Engine::getD3D11Context();
        ID3D11Device* device = nullptr;
        context->GetDevice(&device);

        static char searchBuffer[256] = "";
        ImGui::InputText("Search", searchBuffer, sizeof(searchBuffer));
        
        renderTableLabel("ID3D11Device VTable");
        renderVFTable(*(void**)device, ID3D11Device_VirtualFunctionNames, searchBuffer);

        renderTableLabel("ID3D11DeviceContext VTable");
        renderVFTable(*(void**)context, ID3D11DeviceContext_VirtualFunctionNames, searchBuffer);
    }

    void render() {
        if (!isOpen) return;
        
        ImGui::Begin("Inspect DX11", &isOpen);

        if (ImGui::CollapsingHeader("VFTables")) {
            renderVFTables();
        }
        
        if (ImGui::CollapsingHeader("Resources")) {
            renderResources();
        }
        
        ImGui::End();
    }
}
