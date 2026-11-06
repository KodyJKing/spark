#pragma once

#include "problems.hpp"

namespace Engine::Map::Analysis {

    struct StructureAnalysis {
        std::vector<SpanInfo> overlaps;
        std::vector<Problem> problems;
    };

    StructureAnalysis analyzeStructure(
        StructureRef& root,
        void* tagStart,
        void* tagEnd,
        const ProblemOptions& options = {},
        size_t maxProblems = defaultMaxProblems,
        const std::atomic<bool>* cancel = nullptr
    );

}
