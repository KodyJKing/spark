#pragma once

#include "span_map.hpp"
#include <vector>

#define MAX_PROBLEMS 128

namespace Engine::Map::Analysis {

    // Anything suspicious about a tag's parsed layout, tied to a byte range so
    // it can be located in UI. Add new kinds here as new detectors are written.
    enum class ProblemKind {
        Overlap,              // Two or more parsed spans claim the same bytes.
        Gap,                  // Bytes inside the tag that no parsed span covers.
        ProbableBlockPointer, // Looks like a block pointer but isn't labelled as one.
        ProbableTagReference, // Looks like a tag reference but isn't labelled as one.
    };

    struct Problem {
        ProblemKind kind;
        char* start;
        char* end;
    };

    inline const char* problemKindName(ProblemKind kind) {
        switch (kind) {
            case ProblemKind::Overlap: return "Overlap";
            case ProblemKind::Gap: return "Gap";
            case ProblemKind::ProbableBlockPointer: return "Probable BlockPointer";
            case ProblemKind::ProbableTagReference: return "Probable TagReference";
        }
        return "?";
    }

    // Overlapping regions are reported once per contiguous run, not per span.
    inline void findOverlapProblems(const std::vector<SpanInfo>& spans, std::vector<Problem>& out) {
        if (out.size() >= MAX_PROBLEMS) return;
        
        char* maxEnd = nullptr;
        Problem* last = nullptr;
        for (const auto& info : spans) {
            char* s = info.span.start;
            char* e = info.span.end;
            if (maxEnd != nullptr && s < maxEnd) {
                char* oe = e < maxEnd ? e : maxEnd;
                if (last && s <= last->end) {
                    if (oe > last->end) last->end = oe;
                } else {
                    out.push_back({ProblemKind::Overlap, s, oe});
                    last = &out.back();

                    if (out.size() >= MAX_PROBLEMS) return;
                }
            }
            if (maxEnd == nullptr || e > maxEnd) maxEnd = e;
        }
    }

    struct ProblemOptions {
        // DataPointer payloads are opaque and may be followed by padding to
        // restore 4-byte alignment, so gaps right after them are usually benign.
        bool ignoreGapsAfterDataPointer = true;
    };

    // Expects spans sorted by start (as produced by analyzeOverlaps).
    inline void findGapProblems(
        const std::vector<SpanInfo>& spans,
        char* rangeStart,
        char* rangeEnd,
        std::vector<Problem>& out,
        const ProblemOptions& options = {}
    ) {
        if (out.size() >= MAX_PROBLEMS) return;

        char* cursor = rangeStart;
        // The span whose end defines the cursor, i.e. the one a gap would follow.
        const Span* cursorSpan = nullptr;
        auto reportGap = [&](char* start, char* end) {
            if (options.ignoreGapsAfterDataPointer && cursorSpan &&
                cursorSpan->fieldType == Type::DataPointer) return;
            out.push_back({ProblemKind::Gap, start, end});
        };
        for (const auto& info : spans) {
            char* s = info.span.start;
            char* e = info.span.end;
            if (e <= rangeStart || s >= rangeEnd) continue;
            if (s > cursor) reportGap(cursor, s);
            if (e > cursor) {
                cursor = e;
                cursorSpan = &info.span;
            }

            if (out.size() >= MAX_PROBLEMS) return;
        }
        if (cursor < rangeEnd) reportGap(cursor, rangeEnd);
    }

    inline std::vector<Problem> findProblems(
        const std::vector<SpanInfo>& spans,
        char* rangeStart,
        char* rangeEnd,
        const ProblemOptions& options = {}
    ) {
        std::vector<Problem> problems;
        findOverlapProblems(spans, problems);
        findGapProblems(spans, rangeStart, rangeEnd, problems, options);
        return problems;
    }

}
