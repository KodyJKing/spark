#include "State.hpp"
#include <imgui.h>

namespace Mod::DevTools::FrameDebugger {

    State state;

    void showWindow() {
        state.windowOpen = true;
    }

    void deinit() {
        state.deinit();
    }

    void render() {
        state.frameCount++;
        state.drawsLastFrame = state.drawsThisFrame;
        state.drawsThisFrame = 0;

        if (!state.windowOpen) return;

        ImGui::Begin("Frame Dissector", &state.windowOpen);

        // Enable checkbox for the frame debugger
        bool enabled = state.enabled();
        ImGui::Checkbox("Enabled", &enabled);
        state.setEnabled(enabled);

        ImGui::InputInt("Draws To Allow", reinterpret_cast<int*>(&state.drawsToAllow), 1, 10);
        if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) state.drawsToAllow += 100;
        if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) state.drawsToAllow -= 100;
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) state.drawsToAllow += 10;
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) state.drawsToAllow -= 10;
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) state.drawsToAllow -= 1;
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) state.drawsToAllow += 1;

        if (ImGui::Button("Debug")) {
            state.debug = true;
        }

        // Info:
        ImGui::Text("Draws Last Frame: %llu", state.drawsLastFrame);
        ImGui::Text("Final Call Return Address: %p", state.finalCallReturnAddress);
        
        ImGui::End();
    }

}
