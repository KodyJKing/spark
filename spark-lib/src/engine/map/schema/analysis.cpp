#include "analysis.hpp"
#include "hints.hpp"

#include <stdexcept>

namespace Engine::Map::Analysis {

    StructureAnalysis analyzeStructure(
        StructureRef& root, void* tagStart, void* tagEnd,
        const ProblemOptions& options, size_t maxProblems, const std::atomic<bool>* cancel
    ) {
        if (!root.valid() || !root.node)
            throw std::runtime_error("Invalid structure reference");
        if (!tagStart || reinterpret_cast<uintptr_t>(tagEnd) <= reinterpret_cast<uintptr_t>(tagStart))
            throw std::runtime_error("Invalid tag data range");

        SpanMap spans;
        if (root.node->size > 0) {
            char* start = static_cast<char*>(root.address);
            spans.spans.insert({start, spanEnd(start, root.node->size), root.node, Type::Structure});
        }
        fillSpanMap(root, spans, tagStart, tagEnd, 0, cancel);
        StructureAnalysis result;
        if (cancel && cancel->load()) return result;
        result.overlaps = analyzeOverlaps(spans);
        result.problems = findProblems(result.overlaps, static_cast<char*>(tagStart),
            static_cast<char*>(tagEnd), options, maxProblems);
        if (cancel && cancel->load()) return result;
        Hints::findProbableTypeProblems(root.context, result.overlaps, tagStart, tagEnd, result.problems, cancel);
        return result;
    }

}
