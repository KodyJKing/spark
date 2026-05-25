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

    // Command history for up/down navigation (submitted commands only, not engine output)
    static std::vector<std::string> s_cmdHistory;
    static int   s_historyPos  = -1;   // -1 = editing new input
    static char  s_inputSave[512] = {}; // stash in-progress input before navigating

    static int inputTextCallback(ImGuiInputTextCallbackData* data) {
        if (data->EventFlag != ImGuiInputTextFlags_CallbackHistory)
            return 0;

        const int prev = s_historyPos;
        if (data->EventKey == ImGuiKey_UpArrow) {
            if (s_historyPos == -1) {
                ::strncpy(s_inputSave, data->Buf, sizeof(s_inputSave));
                s_historyPos = (int)s_cmdHistory.size() - 1;
            } else if (s_historyPos > 0) {
                --s_historyPos;
            }
        } else if (data->EventKey == ImGuiKey_DownArrow) {
            if (s_historyPos != -1) {
                if (++s_historyPos >= (int)s_cmdHistory.size())
                    s_historyPos = -1;
            }
        }

        if (prev != s_historyPos) {
            const char* text = (s_historyPos == -1) ? s_inputSave : s_cmdHistory[s_historyPos].c_str();
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, text);
        }
        return 0;
    }

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
        // Add to command history (skip if identical to last entry)
        if (s_cmdHistory.empty() || s_cmdHistory.back() != s_inputBuf)
            s_cmdHistory.emplace_back(s_inputBuf);
        s_historyPos  = -1;
        s_inputSave[0] = '\0';
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
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory,
            inputTextCallback);
        ImGui::SameLine();
        if (ImGui::Button("Submit") || enterPressed) {
            submitInput();
            ImGui::SetKeyboardFocusHere(-1);
        }
    }

} // namespace Mod::DevTools
