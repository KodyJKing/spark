#pragma once

#include "engine/map/schema/hints.hpp"
#include "imgui.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace SparkLib::UI::DissectTag {

    inline bool colorAddress(
        Engine::Map::Hints::HintCache& cache,
        Engine::Map::Hints::HintContext& ctx,
        void* address,
        float notableThreshold
    ) {
        float* scores = cache.getScores(ctx, address);
        for (int i = 0; i < static_cast<int>(Engine::Map::TypeCount); ++i) {
            if (scores[i] >= notableThreshold) {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 64, 255));
                return true;
            }
        }
        return false;
    }

    inline void renderHints(Engine::Map::Hints::HintContext ctx, void* address, float notableThreshold) {
        using namespace Engine::Map;

        float scores[static_cast<int>(TypeCount)];
        Hints::computeTypeScoreDistribution(ctx, address, scores);

        std::vector<std::pair<std::string, float>> scorePairs;
        for (int i = 0; i < static_cast<int>(TypeCount); ++i) {
            if (scores[i] < notableThreshold) continue;
            scorePairs.emplace_back(TypeNames[i], scores[i]);
        }
        std::sort(scorePairs.begin(), scorePairs.end(), [](const auto& a, const auto& b) {
            return a.second > b.second;
        });

        if (!scorePairs.empty()) {
            ImGui::BeginChild("Hints", ImVec2(0, 0), 1 | ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeY);

            if (ImGui::BeginTable("HintTable", 2)) {
                ImGui::TableSetupColumn("Type");
                ImGui::TableSetupColumn("Score");
                ImGui::TableHeadersRow();
                for (const auto& pair : scorePairs) {
                    uint8_t minAlpha = 64;
                    uint8_t maxAlpha = 255;
                    uint8_t alpha = static_cast<uint8_t>(pair.second * (maxAlpha - minAlpha) + minAlpha);
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, alpha));

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%s", pair.first.c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%.2f", pair.second);

                    ImGui::PopStyleColor();
                }
                ImGui::EndTable();
            }

            ImGui::EndChild();
        }
    }

}
