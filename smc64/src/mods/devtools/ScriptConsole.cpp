#include "ScriptConsole.hpp"
#include "imgui.h"
#include "engine/scripting/Scripting.hpp"
#include <string>
#include <vector>

namespace Mod::DevTools {

    static char s_inputBuf[512] = {};
    static std::vector<std::string> s_history;
    static bool s_scrollToBottom = false;

    static void submitInput() {
        if (s_inputBuf[0] == '\0') return;
        std::string cmd(s_inputBuf);
        Engine::Scripting::submit(s_inputBuf);
        s_history.push_back(cmd);
        s_inputBuf[0] = '\0';
        s_scrollToBottom = true;
    }

    void renderScriptConsoleTab() {
        // History
        float historyHeight = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() - ImGui::GetStyle().ItemSpacing.y;
        ImGui::BeginChild("##ConsoleHistory", ImVec2(0, historyHeight), ImGuiChildFlags_Borders);
        for (const auto& entry : s_history)
            ImGui::TextUnformatted(entry.c_str());
        if (s_scrollToBottom) {
            ImGui::SetScrollHereY(1.0f);
            s_scrollToBottom = false;
        }
        ImGui::EndChild();

        // Input row
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Submit").x - ImGui::GetStyle().ItemSpacing.x * 2 - ImGui::GetStyle().FramePadding.x * 2);
        bool enterPressed = ImGui::InputText(
            "##ConsoleInput", s_inputBuf, sizeof(s_inputBuf),
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("Submit") || enterPressed) {
            submitInput();
            ImGui::SetKeyboardFocusHere(-1);
        }
    }

} // namespace Mod::DevTools
