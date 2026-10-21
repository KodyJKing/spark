#include "imgui.h"

namespace ImGuiUtils {

    void renderCopyableText(const char* label, const char* text) {
        if (strnlen(label, 1) == 0)
            ImGui::Text("%s", text);
        else
            ImGui::Text("%s %s", label, text);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Right click to copy to clipboard");
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            ImGui::SetClipboardText(text);
        }
    }

}