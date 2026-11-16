#pragma once

#include <cstdint>
#include <algorithm>
#include <cstring>

#include "engine/map/schema/schema.hpp"
#include "engine/map/schema/reference.hpp"
#include "engine/map/schema/type.hpp"

#include "imgui.h"
#include "imgui_internal.h"

#define DEBUG

#ifdef DEBUG
#include "utils/Debugging.hpp"
#include <iostream>
#define LOG(X) std::cout << "[DissectTag] " << X << std::endl;
#else
#define LOG(X)
#endif


namespace SparkLib::UI::DissectTag {

    using namespace Engine::Map;

    inline void renderRowAddress(uintptr_t address, uintptr_t structureBase) {
        char buffer[256];
        ImGui::NewLine();
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(128, 128, 128, 255));
        snprintf(buffer, sizeof(buffer), "%p", address);
        if(ImGui::TextLink(buffer)) {
            ImGui::SetClipboardText(buffer);
        }
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

    inline void renderStructureLabel(const StructureNode& node) {
        ImGui::TextUnformatted(node.name[0] ? node.name : "[unnamed]");
        ImGui::SameLine();
        ImGui::TextDisabled("%s", node.id.c_str());
    }

    inline bool renderStructureTypePicker(FieldRef& field) {
        if (!field.is(Type::Structure) && !field.is(Type::BlockPointer)) return false;

        bool changed = false;
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
        if (ImGui::BeginCombo("##StructureType", nullptr,
            ImGuiComboFlags_CustomPreview | ImGuiComboFlags_HeightLarge)) {
            std::vector<StructureNode*> structures;
            for (auto& [id, node] : field.context->schema->structures)
                if (!node.readonly()) structures.push_back(&node);
            std::sort(structures.begin(), structures.end(), [](const auto* a, const auto* b) {
                int order = std::strcmp(a->name, b->name);
                return order != 0 ? order < 0 : a->id < b->id;
            });

            for (auto* node : structures) {
                ImGui::PushID(node->id.c_str());
                bool selected = field.node->type.ref == node->id;
                ImVec2 start = ImGui::GetCursorScreenPos();
                float width = ImGui::CalcTextSize(node->name[0] ? node->name : "[unnamed]").x
                    + ImGui::GetStyle().ItemSpacing.x + ImGui::CalcTextSize(node->id.c_str()).x;
                if (ImGui::Selectable("##Structure", selected, 0,
                    ImVec2((std::max)(width, ImGui::GetContentRegionAvail().x), 0))) {
                    field.node->type.ref = node->id;
                    changed = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
                ImVec2 next = ImGui::GetCursorScreenPos();
                ImGui::SetCursorScreenPos(start);
                renderStructureLabel(*node);
                ImGui::SetCursorScreenPos(next);
                ImGui::PopID();
            }

            if (!structures.empty()) ImGui::Separator();
            if (ImGui::Selectable("Create new structure...")) {
                // Inline structures start small to avoid shifting the surrounding fields.
                changed = field.createStructure(field.is(Type::Structure) ? 0x4 : 0x100);
            }
            ImGui::EndCombo();
        }
        if (ImGui::BeginComboPreview()) {
            auto* node = field.getReferencedType();
            if (node && !node->readonly()) {
                renderStructureLabel(*node);
            } else {
                ImGui::TextUnformatted("Select structure...");
                if (field.node->type.ref != NullId) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("[missing: %s]", field.node->type.ref.c_str());
                }
            }
            ImGui::EndComboPreview();
        }
        if (field.node->type.ref == NullId) {
            ImGui::SameLine();
            // New button
            if (ImGui::Button("New")) {
                changed = field.createStructure(field.is(Type::Structure) ? 0x4 : 0x100);
            }
        }
        return changed;
    }

}
