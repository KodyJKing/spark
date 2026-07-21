#include "HookLogWindow.hpp"
#include "HookLogState.hpp"
#include "imgui.h"

namespace Mod::HookLog {

    static void hookLogTab() {
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputInt("Debounce (ms)", &debounceMilliseconds, 0);
        if (debounceMilliseconds < 0) debounceMilliseconds = 0;
        ImGui::Separator();
#define HOOK(Name, ...) ImGui::Checkbox(#Name, &toggles.Name);
#include "spark/hook/HookTable.hpp"
#undef HOOK
    }

    void renderPauseMenuTabs() {
        if (ImGui::BeginTabItem("Hook Log")) {
            hookLogTab();
            ImGui::EndTabItem();
        }
    }

} // namespace Mod::HookLog
