#pragma once

#include <cstdint>

#include "imgui.h"
#include "State.hpp"
#include "Constants.hpp"
#include "Hints.hpp"
#include "Functions.hpp"

namespace Mod::DevTools::DissectTagNew {

    void renderStructure(RenderContext& renderCtx, StructureRef& structure, size_t fallbackSize);

    inline void renderSubstructure(RenderContext& renderCtx, StructureRef& structure, FieldRef& field, size_t fallbackSize = 0) {
        if (!structure.valid()) {
            ImGui::Text("[invalid structure]");
            return;
        }

        ImGui::PushID(structure.address);
        ImGui::NewLine();
        ImGui::Indent();
        ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
        if (ImGui::CollapsingHeader("Substructure")) {
            if (structure.node->id == NullId) {
                if (ImGui::Button("Create Structure")) {
                    field.createStructure();
                }
            } else {
                ImGui::InputText("Structure Name", structure.node->name, IM_ARRAYSIZE(structure.node->name));
            }
    
            renderStructure(renderCtx, structure, fallbackSize);
        }
        ImGui::PopStyleColor();
        ImGui::Unindent();
        ImGui::PopID();
    }

}
