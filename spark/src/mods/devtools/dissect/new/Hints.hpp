#pragma once

#include "engine/map/schema/reference.hpp"
#include "imgui.h"

#include <cmath>
#include <unordered_map>
#include <vector>
#include <algorithm>

namespace Mod::DevTools::DissectTagNew::Hints {

    using namespace Engine::Map;

    struct HintContext {
        void* tagStart;
        void* tagEnd;
        StructureRef& structure;
    };

    /////////////////////////////////////////////////////
    // Heuristic evals for different types. (Scores are normalized with softmax for classification.)

    inline float blockPointerScore(
        HintContext& ctx,
        void* address
    ) {
        BlockPointer* blockPointer = reinterpret_cast<BlockPointer*>(address);
        void* blockBase = ctx.structure.context->mapFile->fromRelative(blockPointer->data);
        bool isWithinTag = blockBase >= ctx.tagStart && blockBase < ctx.tagEnd;
        if (!isWithinTag) return 0.0f;
        bool isEmpty = blockPointer->count == 0;
        if (isEmpty) return 0.0f;
        // Counts over 0xFFFF begin to count as negative evidence for being a valid block pointer.
        float saturation = static_cast<float>(blockPointer->count) / 0xFFFF;
        return - logf(saturation);
    }

    inline float floatScore(
        HintContext& ctx,
        void* address
    ) {
        if (*reinterpret_cast<uint32_t*>(address) == 0x00000000) return 0.0f;
        float* value = reinterpret_cast<float*>(address);
        if (std::isnan(*value)) return 0.0f;
        int exponent;
        std::frexp(*value, &exponent);
        // Heuristic: prefer floats with exponents in a reasonable range.
        if (exponent < -10 || exponent > 10) return 0.0f;
        return 1.0f;
    }

    inline float tagStringScore(
        HintContext& ctx,
        void* address
    ) {
        int alphaNumericCount = 0;
        char* value = reinterpret_cast<char*>(address);
        
        std::string valueStr(value);
        size_t length = std::strlen(value);
        if (length < 4) return 0.0f;

        if (value == nullptr) return 0.0f;
        for (size_t i = 0; value[i] != '\0'; ++i) {
            if (std::isalnum(static_cast<unsigned char>(value[i]))) {
                ++alphaNumericCount;
            }
        }

        float ratio = static_cast<float>(alphaNumericCount) / length;
        if (ratio < 0.5f) return 0.0f;

        return ratio;
    }

    inline constexpr float blockPointerWeight = 10.0f;
    inline constexpr float floatWeight = 1.0f;
    inline constexpr float tagStringWeight = 1.0f;

    inline void computeScore(
        HintContext& ctx,
        void* address,
        float& score,
        Type type
    ) {
        switch (type) {
            case Type::BlockPointer:
                score = blockPointerScore(ctx, address) * blockPointerWeight;
                break;
            case Type::F32:
                score = floatScore(ctx, address) * floatWeight;
                break;
            case Type::TagString:
                score = tagStringScore(ctx, address) * tagStringWeight;
                break;
            default:
                score = 0.0f;
                break;
        }
    }

    inline void softmax(float* scores, int count) {
        float maxScore = scores[0];
        for (int i = 1; i < count; ++i) {
            if (scores[i] > maxScore) {
                maxScore = scores[i];
            }
        }
        float sumExp = 0.0f;
        for (int i = 0; i < count; ++i) {
            scores[i] = std::exp(scores[i] - maxScore);
            sumExp += scores[i];
        }
        for (int i = 0; i < count; ++i) {
            scores[i] /= sumExp;
        }
    }

    /////////////////////////////////////////////////////
    // Public API

    inline void computeTypeScoreDistribution(
        HintContext& ctx,
        void* address,
        float* scores
    ) {
        for (int i = 0; i < static_cast<int>(TypeCount); ++i) {
            computeScore(ctx, address, scores[i], static_cast<Type>(i));
        }
        softmax(scores, static_cast<int>(TypeCount));
    }

    void renderHints(HintContext ctx, void* address, float notableThreshold) {
        ImGui::BeginChild("Hints", ImVec2(0, 0), 1 | ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeY );
        // Render probability distribution for each type.
        float scores[static_cast<int>(TypeCount)];
        computeTypeScoreDistribution(ctx, address, scores);

        // Create score/name pairs and sort by score
        std::vector<std::pair<std::string, float>> scorePairs;
        for (int i = 0; i < static_cast<int>(TypeCount); ++i) {
            if (scores[i] < notableThreshold) continue;
            scorePairs.emplace_back(TypeNames[i], scores[i]);
        }
        std::sort(scorePairs.begin(), scorePairs.end(), [](const auto& a, const auto& b) {
            return a.second > b.second;
        });

        // Render as table
        if (!scorePairs.empty()) {
            ImGui::BeginTable("HintTable", 2);
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

    struct HintCache {
        std::vector<float> scores;
        std::unordered_map<void*, size_t> cache;
        float* getScores(HintContext& ctx, void* address) {
            auto it = cache.find(address);
            if (it != cache.end()) {
                return &scores[it->second];
            }
            size_t oldSize = scores.size();
            scores.resize(oldSize + TypeCount);
            float* newScores = &scores[oldSize];
            cache[address] = oldSize;
            computeTypeScoreDistribution(ctx, address, newScores);
            return newScores;
        }
    };


}