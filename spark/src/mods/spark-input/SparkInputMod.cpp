#include "SparkInputMod.hpp"
#include "spark/hook/Hooks.hpp"
#include "spark/input/Input.hpp"
#include "spark/input/Bindings.hpp"
#include "spark/RenderBuses.hpp"
#include "imgui.h"

// Spark-internal; not exported to mod DLLs
namespace Spark::Input { void activeButtons(ButtonCode* buffer, int bufferSize, int* activeCount); }

void SparkInputMod::init() {
    Spark::UpdateAllEntities::addHandler(modId_, +[](void* ctx, auto next) {
        Spark::Input::update();
        next();
    }, nullptr);

    Spark::onRenderPauseMenuTabs.addHandler(modId_, +[](void*, auto next) {
        if (ImGui::BeginTabItem("Input")) {
            static std::string s_listening;
            static bool s_waitForRelease = false;

            // Capture binding: wait for the click to be released before sampling
            if (!s_listening.empty()) {
                Spark::Input::update();
                Spark::Input::ButtonCode active[1];
                int count = 0;
                Spark::Input::activeButtons(active, 1, &count);
                if (s_waitForRelease) {
                    if (count == 0) s_waitForRelease = false;
                } else if (count > 0) {
                    Spark::Input::bindAction(s_listening.c_str(), active[0]);
                    s_listening.clear();
                }
            }

            if (ImGui::BeginTable("bindings", 2, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableSetupColumn("Action");
                ImGui::TableSetupColumn("Keys");
                ImGui::TableHeadersRow();

                for (auto& [actionName, defaultBtn] : Spark::Input::defaultBindings) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(actionName.c_str());
                    ImGui::TableSetColumnIndex(1);

                    const auto& bound = Spark::Input::getBoundButtons(actionName.c_str());
                    for (Spark::Input::ButtonCode btn : bound) {
                        char* keyName = Spark::Input::getButtonName(btn);
                        char label[64];
                        if (keyName)
                            snprintf(label, sizeof(label), "%s x##rm_%s_%u", keyName, actionName.c_str(), btn);
                        else
                            snprintf(label, sizeof(label), "Btn%u x##rm_%s_%u", btn, actionName.c_str(), btn);
                        if (ImGui::SmallButton(label))
                            Spark::Input::unbindAction(actionName.c_str(), btn);
                        ImGui::SameLine();
                    }

                    bool listening = s_listening == actionName;
                    char addLabel[64];
                    snprintf(addLabel, sizeof(addLabel), "%s##add_%s", listening ? "..." : "+", actionName.c_str());
                    if (listening) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.3f, 0.3f, 1.0f));
                    if (ImGui::SmallButton(addLabel)) {
                        s_listening = actionName;
                        s_waitForRelease = true;
                    }
                    if (listening) ImGui::PopStyleColor();
                }

                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        next();
    }, nullptr);
}
