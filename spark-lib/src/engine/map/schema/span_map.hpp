#pragma once

#include "schema.hpp"
#include "reference.hpp"
#include <set>
#include <vector>
#include <stdexcept>
#include <atomic>

namespace Engine::Map::Analysis {

    struct Span {
        char* start;
        char* end;
        StructureNode* type;
        Type fieldType;
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
        void* stopAddress,
        size_t depth = 0,
        const std::atomic<bool>* cancel = nullptr
    ) {
        constexpr size_t maxBlocksToScan = 1024;

        if (!structure.valid()) return;
        if (depth > 64) throw std::runtime_error("Block structure nesting exceeds 64 levels");
        MapFile* mapFile = structure.context->mapFile;
        if ((char*)structure.address < (char*)startAddress) return;
        if ((char*)structure.address >= (char*)stopAddress) return;

        auto fieldRefs = structure.getFieldRefs();
        for (auto& fieldRef : fieldRefs) {
            if (cancel && cancel->load()) return;
            if (!fieldRef.is(Type::DataPointer) && !fieldRef.is(Type::BlockPointer)) continue;
            auto fieldAddress = reinterpret_cast<uintptr_t>(fieldRef.address);
            auto start = reinterpret_cast<uintptr_t>(startAddress);
            auto stop = reinterpret_cast<uintptr_t>(stopAddress);
            if (fieldAddress < start || fieldAddress > stop || fieldRef.size() > stop - fieldAddress)
                throw std::runtime_error("Pointer field extends outside the tag data range");

            if (fieldRef.is(Type::DataPointer)) {
                DataPointer* dataPtr = (DataPointer*)fieldRef.address;
                void* start = mapFile->getDataPointer(*dataPtr);
                if (!start) continue;
                void* end = (char*)start + dataPtr->size;
                spanMap.spans.insert({(char*)start, (char*)end, nullptr, Type::DataPointer});
                continue;
            }

            if (!fieldRef.is(Type::BlockPointer)) continue;
            if ((char*)fieldRef.address < (char*)startAddress) break;
            if ((char*)fieldRef.address >= (char*)stopAddress) break;
            StructureNode* type = fieldRef.getReferencedType();
            size_t size = type ? type->size : 0;
            if (size == 0) continue;

            auto count = fieldRef.getBlockElementCount();
            if (count > maxBlocksToScan) {
                char* start = (char*) fieldRef.getBlockElement(0).address;
                char* end = start + size * count;
                spanMap.spans.insert({start, end, type, Type::BlockPointer});
                continue; // Skip scanning individual block elements if too many
            }

            for (int i = 0; i < count; ++i) {
                if (cancel && cancel->load()) return;
                StructureRef blockElement = fieldRef.getBlockElement(i);
                if ((char*)blockElement.address < (char*)startAddress) break;
                if ((char*)blockElement.address >= (char*)stopAddress) break;
                if (blockElement.valid()) {
                    char* start = (char*)blockElement.address;
                    spanMap.spans.insert({start, start + size, type, Type::BlockPointer});
                    fillSpanMap(blockElement, spanMap, startAddress, stopAddress, depth + 1, cancel);
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
