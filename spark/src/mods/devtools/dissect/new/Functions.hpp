#pragma once

#include <cstdint>

#include "engine/map/schema/schema.hpp"
#include "engine/map/schema/type.hpp"

#include "imgui.h"

#define DEBUG

#ifdef DEBUG
#include "utils/Debugging.hpp"
#include <iostream>
#define LOG(X) std::cout << "[DissectTagNew] " << X << std::endl;
#else
#define LOG(X)
#endif


namespace Mod::DevTools::DissectTagNew {

    using namespace Engine::Map;

    inline void renderRowAddress(uintptr_t address, uintptr_t structureBase) {
        ImGui::NewLine();
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(128, 128, 128, 255));
        ImGui::Text("%p", (void*)address);
        ImGui::SameLine();
        ImGui::PopStyleColor();

        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Offset %X", (address - structureBase));
    }


    inline void renderTypePicker(Type* type) {
        if (!type) return;
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
        if (ImGui::BeginCombo("##Type", Engine::Map::TypeNames[static_cast<int>(*type)])) {
            for (int i = 0; i < static_cast<int>(Engine::Map::TypeCount); ++i) {
                bool isSelected = (*type == static_cast<Engine::Map::Type>(i));
                if (ImGui::Selectable(Engine::Map::TypeNames[i], isSelected)) {
                    *type = static_cast<Engine::Map::Type>(i);
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

}
