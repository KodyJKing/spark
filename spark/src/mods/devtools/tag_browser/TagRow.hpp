#pragma once

#include "engine/halo1.hpp"
#include "imgui.h"
#include "utils/ImGuiUtils.hpp"
#include "mods/devtools/dissect/DissectTag.hpp"
#include "mods/devtools/dissect/new/DissectTagNew.hpp"

namespace Mod::DevTools {

    inline void renderTagRow(int index) {
        if (index < 0) {
            ImGui::Text("");
            return;
        }

        auto &map = *state.getMap();
        auto tag = map.getTag(index);

        // ImGuiUtils::renderCopyableTextf("Address", "%p", tag);
        // return;

        if (state.tagExists(index)) {
            ImGui::PushID(index);
            
            auto path = map.getTagPath(tag);
            if (!state.validTagPath(path)) {
                ImGui::Text("%04d NULL", index);
            } else {
                char text[2048] = {0};

                auto inspectOnLeftClick = [&]() {
                    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                        // Mod::DevTools::DissectTag::openWindow(tag->tagID);
                        Mod::DevTools::DissectTagNew::openWindow(state.getMap(), tag->tagHandle);
                    }
                };

                ImGui::Text("%04d", index);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Right click to copy index to clipboard");
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) ImGui::SetClipboardText( std::to_string(index).c_str() );
                inspectOnLeftClick();

                ImGui::SameLine();
                sprintf_s( text, "%X", tag->tagHandle );
                ImGui::Text(text);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Right click to copy TagID to clipboard");
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) ImGui::SetClipboardText( text );
                inspectOnLeftClick();

                // ImGui::SameLine();
                // sprintf_s( text, "%p", tag );
                // ImGui::Text( "%s", text );
                // if (ImGui::IsItemHovered()) ImGui::SetTooltip("Right click to copy tag pointer to clipboard");
                // if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) ImGui::SetClipboardText( text );
                // inspectOnLeftClick();

                ImGui::SameLine();
                auto groupID = tag->groupIdString();
                ImGui::Text("%s", groupID.c_str());
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Right click to copy GroupID to clipboard");
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) ImGui::SetClipboardText( groupID.c_str() );
                inspectOnLeftClick();

                ImGui::SameLine();
                auto pDataAddress = &tag->data.offset;
                sprintf_s( text, "%p", pDataAddress );
                ImGui::Text("%s", text);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Right click to copy data address pointer to clipboard");
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) ImGui::SetClipboardText( text );
                inspectOnLeftClick();

                ImGui::SameLine();
                auto data = map.getTagData(tag);
                sprintf_s( text, "%p", data );
                ImGui::Text("%s", text);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Right click to copy data pointer to clipboard");
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) ImGui::SetClipboardText( text );
                inspectOnLeftClick();

                ImGui::SameLine();
                ImGui::Text("%s", path);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Right click to copy path to clipboard");
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) ImGui::SetClipboardText( path );
                inspectOnLeftClick();
            }
            
            ImGui::PopID();
        } else {
            ImGui::Text("%04d NULL", index);
        }
    }

}
