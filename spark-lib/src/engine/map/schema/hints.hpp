#pragma once

#include "reference.hpp"
#include "problems.hpp"

#include <unordered_map>
#include <vector>

namespace Engine::Map::Hints {

    struct HintContext {
        void* tagStart;
        void* tagEnd;
        StructureRef& structure;
    };

    float blockPointerScore(HintContext& ctx, void* address);
    float floatScore(HintContext& ctx, void* address);
    float tagStringScore(HintContext& ctx, void* address);
    float tagReferenceScore(HintContext& ctx, void* address);
    float vecNScore(HintContext& ctx, void* address, int dimension);

    void computeTypeScoreDistribution(HintContext& ctx, void* address, float* scores);

    struct HintCache {
        std::vector<float> scores;
        std::unordered_map<void*, size_t> cache;

        float* getScores(HintContext& ctx, void* address);
        Type suggestedType(HintContext& ctx, void* address, float notableThreshold);
    };

    void findProbableTypeProblems(
        Context* context,
        const std::vector<Analysis::SpanInfo>& spans,
        void* tagStart,
        void* tagEnd,
        std::vector<Analysis::Problem>& out,
        const std::atomic<bool>* cancel = nullptr
    );

}
