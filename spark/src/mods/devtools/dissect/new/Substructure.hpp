#pragma once

#include <cstdint>

#include "imgui.h"
#include "State.hpp"
#include "Constants.hpp"
#include "Hints.hpp"
#include "Functions.hpp"

namespace Mod::DevTools::DissectTagNew {

    void renderStructure(RenderContext& renderCtx, StructureRef& structure, size_t fallbackSize);

    void windowHeader();

    inline void renderSubstructure(RenderContext& renderCtx, StructureRef& structure, FieldRef& field, size_t fallbackSize = 0) {
        if (!structure.valid()) {
            ImGui::Text("[invalid structure]");
            return;
        }

        ImGui::PushID(field.address);
        // ImGui::NewLine();
        ImGui::Indent();
        ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));

        char buffer[256];
        // snprintf(buffer, sizeof(buffer), "%s @%p", structure.node->name, structure.address);
        snprintf(buffer, sizeof(buffer), "%s", structure.node->name);
        
        ImGui::SameLine();
        if (ImGui::CollapsingHeader(buffer)) {
            if (structure.node->id == NullId) {
                if (ImGui::Button("Create Structure")) {
                    field.createStructure();
                }
            } else {
                ImGui::SetNextItemWidth(ImGui::GetFontSize() * 16);
                ImGui::InputText("Structure Name", structure.node->name, IM_ARRAYSIZE(structure.node->name));

                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetFontSize() * 16);
                int size = structure.node->size;
                ImGui::InputInt("Size", &size, 4, 16, ImGuiInputTextFlags_CharsHexadecimal);
                structure.node->size = size > 4 ? size : 4;
            }
    
            renderStructure(renderCtx, structure, fallbackSize);
        }
        ImGui::PopStyleColor();
        ImGui::Unindent();
        ImGui::SameLine();
        ImGui::PopID();
    }

}
