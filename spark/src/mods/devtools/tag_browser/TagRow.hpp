// This is a private mixin. Only import from one translation unit.

#include "engine/halo1.hpp"
#include "imgui.h"
#include "utils/ImGuiUtils.hpp"
#include "mods/devtools/dissect/DissectTag.hpp"
#include "mods/devtools/dissect/new/DissectTagNew.hpp"

#define COLOR_SELECTED_ROW IM_COL32(255, 255, 0, 255)

namespace Mod::DevTools {

    void renderContextMenu(int index, Tag* tag) {
        if (index < 0) return;
        if (ImGui::BeginPopupContextItem(0, ImGuiPopupFlags_MouseButtonLeft)) {
            state.selection = index;
            
            if (ImGui::MenuItem("Inspect Tag")) {
                Mod::DevTools::DissectTagNew::openWindow(state.getMap(), tag->tagHandle);
            }

            if (state.isFileOpen()) {
                if (ImGui::MenuItem("Load Tag")) {
                    state.copyTagToRuntime(tag->tagHandle);
                }
            }

            ImGui::EndPopup();
        } else {
            if (state.selection == index) 
                state.selection = -1;
        }
    }

    void renderTagRow(int index) {
        if (index < 0) {
            ImGui::Text("");
            return;
        }

        auto &map = *state.getMap();
        auto tag = map.getTag(index);

        char text[2048] = {0};

        auto copyTooltip = [&](const char* name, const char* buffer = 0) {
            if (!buffer) buffer = text;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Right click to copy %s to clipboard", name);
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) ImGui::SetClipboardText( buffer );
        };

        bool hasSelection = state.selection != -1;
        bool isSelected = state.selection == index;
        bool isHovered = state.hovered == index && !hasSelection;
        bool isHighlighted = isHovered || isSelected;
        if (isHighlighted) ImGui::PushStyleColor(ImGuiCol_Text, COLOR_SELECTED_ROW);

        bool hovered = false;
        auto postDrawCell = [&]() {
            if (ImGui::IsItemHovered()) {
                hovered = true;
            }
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && ImGui::GetIO().KeyShift) {
                Mod::DevTools::DissectTagNew::openWindow(state.getMap(), tag->tagHandle);
            }
        };

        if (state.tagExists(index)) {
            ImGui::PushID(index);
            
            auto path = map.getTagPath(tag);
            if (!state.validTagPath(path)) {
                ImGui::Text("%04d NULL", index);
            } else {
                sprintf_s(text, "%04d", index);
                ImGui::SmallButton(text);
                renderContextMenu(index, tag);

                ImGui::SameLine();
                sprintf_s( text, "%X", tag->tagHandle );
                ImGui::Text(text);
                postDrawCell();
                copyTooltip("TagID");

                ImGui::SameLine();
                auto groupID = tag->groupIdString();
                sprintf_s(text, "%s", groupID.c_str());
                ImGui::Text("%s", text);
                postDrawCell();
                copyTooltip("GroupID");

                ImGui::SameLine();
                auto pDataAddress = &tag->data.offset;
                sprintf_s( text, "%p", pDataAddress );
                ImGui::Text("%s", text);
                postDrawCell();
                copyTooltip("data address");

                ImGui::SameLine();
                auto data = map.getTagData(tag);
                sprintf_s( text, "%p", data );
                ImGui::Text("%s", text);
                postDrawCell();
                copyTooltip("data pointer");

                ImGui::SameLine();
                ImGui::Text("%s", path);
                postDrawCell();
                copyTooltip("path", path);
            }
            
            ImGui::PopID();
        } else {
            ImGui::Text("%04d NULL", index);
        }

        if (isHighlighted) ImGui::PopStyleColor();

        if (hovered) {
            state.hovered = index;
        } else if (state.hovered == index) {
            state.hovered = -1;
        }
    }

}
