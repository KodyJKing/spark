#pragma once

#include <cstdint>

#include "engine/map/schema/schema.hpp"
#include "engine/map/schema/type.hpp"

#include "imgui.h"

namespace Mod::DevTools::DissectTagNew {

    using namespace Engine::Map;

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
