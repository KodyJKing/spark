#pragma once

#include "schema.hpp"
#include "reference.hpp"
#include <set>
#include <vector>

namespace Engine::Map::Analysis {

    struct Span {
        char* start;
        char* end;
        StructureNode* type;
        bool operator<(const Span& other) const {
            return start < other.start;
        }
    };

    struct SpanMap {
        // multiset: distinct spans sharing a start address must both be kept
        // (they are exactly the overlaps we want to surface).
        std::multiset<Span> spans;
    };


    inline void fillSpanMap(
        StructureRef& structure, 
        SpanMap& spanMap, 
        void* startAddress,
        void* stopAddress
    ) {
        constexpr size_t maxBlocksToScan = 1024;
        
        if (!structure.valid()) return;
        if ((char*)structure.address < (char*)startAddress) return;
        if ((char*)structure.address >= (char*)stopAddress) return;

        auto fieldRefs = structure.getFieldRefs();
        for (auto& fieldRef : fieldRefs) {
            if (!fieldRef.is(Type::BlockPointer)) continue;
            if ((char*)fieldRef.address < (char*)startAddress) break;
            if ((char*)fieldRef.address >= (char*)stopAddress) break;
            StructureNode* type = fieldRef.getReferencedType();
            size_t size = type ? type->size : 0;
            if (size == 0) continue;

            for (int i = 0; i < fieldRef.getBlockElementCount() && i < maxBlocksToScan; ++i) {
                StructureRef blockElement = fieldRef.getBlockElement(i);
                if ((char*)blockElement.address < (char*)startAddress) break;
                if ((char*)blockElement.address >= (char*)stopAddress) break;
                if (blockElement.valid()) {
                    char* start = (char*)blockElement.address;
                    spanMap.spans.insert({start, start + size, type});
                    fillSpanMap(blockElement, spanMap, startAddress, stopAddress);
                }
            }
        }

        return;
    }

    struct SpanInfo {
        Span span;
        bool overlaps = false;
    };

    // Flattens the span map into start-sorted order and flags every span that
    // intersects another. Since block elements are separately allocated, any
    // overlap is unexpected and worth highlighting.
    inline std::vector<SpanInfo> analyzeOverlaps(const SpanMap& spanMap) {
        std::vector<SpanInfo> result;
        result.reserve(spanMap.spans.size());
        for (const auto& span : spanMap.spans)
            result.push_back({span, false});

        // Sweep in start order, tracking the interval reaching furthest right.
        char* maxEnd = nullptr;
        size_t maxIndex = 0;
        for (size_t i = 0; i < result.size(); ++i) {
            char* start = result[i].span.start;
            if (maxEnd != nullptr && start < maxEnd) {
                result[i].overlaps = true;
                result[maxIndex].overlaps = true;
            }
            if (maxEnd == nullptr || result[i].span.end > maxEnd) {
                maxEnd = result[i].span.end;
                maxIndex = i;
            }
        }

        return result;
    }

}
