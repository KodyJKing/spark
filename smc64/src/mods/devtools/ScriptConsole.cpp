#include "ScriptConsole.hpp"
#include "imgui.h"
#include "engine/scripting/Scripting.hpp"
#include "engine/scripting/TerminalOutput.hpp"
#include <string>
#include <vector>

namespace Mod::DevTools {

    struct HistoryEntry {
        std::string text;
        float color[4]; // RGBA; {0,0,0,0} = use default text color
    };

    static char  s_inputBuf[512] = {};
    static std::vector<HistoryEntry> s_history;
    static bool  s_scrollToBottom = false;

    static void pushEngineOutput() {
        auto entries = Engine::TerminalOutput::poll();
        if (entries.empty()) return;
        for (auto& e : entries) {
            HistoryEntry h;
            h.text = std::move(e.text);
            ::memcpy(h.color, e.color, sizeof(h.color));
            s_history.push_back(std::move(h));
        }
        s_scrollToBottom = true;
    }

    static void submitInput() {
        if (s_inputBuf[0] == '\0') return;
        Engine::Scripting::submit(s_inputBuf);
        HistoryEntry h;
        h.text  = std::string("> ") + s_inputBuf;
        h.color[0] = 0.6f; h.color[1] = 0.6f; h.color[2] = 0.6f; h.color[3] = 1.0f; // dim grey echo
        s_history.push_back(std::move(h));
        s_inputBuf[0]  = '\0';
        s_scrollToBottom = true;
    }

    void renderScriptConsoleTab() {
        pushEngineOutput();

        // History pane
        float historyHeight = ImGui::GetContentRegionAvail().y
            - ImGui::GetFrameHeightWithSpacing()
            - ImGui::GetStyle().ItemSpacing.y;
        ImGui::BeginChild("##ConsoleHistory", ImVec2(0, historyHeight), ImGuiChildFlags_Borders);
        for (const auto& entry : s_history) {
            bool colored = entry.color[3] > 0.0f;
            if (colored)
                ImGui::PushStyleColor(ImGuiCol_Text,
                    ImVec4(entry.color[0], entry.color[1], entry.color[2], entry.color[3]));
            ImGui::TextUnformatted(entry.text.c_str());
            if (colored)
                ImGui::PopStyleColor();
        }
        if (s_scrollToBottom) {
            ImGui::SetScrollHereY(1.0f);
            s_scrollToBottom = false;
        }
        ImGui::EndChild();

        // Input row
        ImGui::SetNextItemWidth(
            ImGui::GetContentRegionAvail().x
            - ImGui::CalcTextSize("Submit").x
            - ImGui::GetStyle().ItemSpacing.x * 2
            - ImGui::GetStyle().FramePadding.x * 2);
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
