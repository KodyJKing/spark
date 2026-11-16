#pragma once

#include "schema.hpp"
#include "reference.hpp"
#include <set>
#include <vector>
#include <stdexcept>
#include <atomic>
#include <limits>

namespace Engine::Map::Analysis {

    struct SpanAncestor {
        char* address;
        StructureNode* type;
        // Position within the parent's block, or -1 for roots. Count is the block's element count.
        int64_t index = -1;
        size_t count = 0;
    };

    struct Span {
        char* start;
        char* end;
        StructureNode* type;
        Type fieldType;
        // Root-to-parent ownership, independent of where the payload was allocated.
        std::vector<SpanAncestor> ancestors;
        // Element index within its block, or -1 for non-elements and collapsed multi-element spans.
        int64_t index = -1;
        size_t count = 0;
        bool operator<(const Span& other) const {
            return start < other.start;
        }
    };

    struct SpanMap {
        // multiset: distinct spans sharing a start address must both be kept
        // (they are exactly the overlaps we want to surface).
        std::multiset<Span> spans;
    };

    inline char* spanEnd(void* address, size_t size) {
        auto start = reinterpret_cast<uintptr_t>(address);
        if (size > (std::numeric_limits<uintptr_t>::max)() - start)
            throw std::runtime_error("Span address overflow");
        return reinterpret_cast<char*>(start + size);
    }

    inline void fillSpanMap(
        StructureRef& structure, 
        SpanMap& spanMap, 
        void* startAddress,
        void* stopAddress,
        size_t depth = 0,
        const std::atomic<bool>* cancel = nullptr,
        std::vector<SpanAncestor> ancestors = {},
        int64_t elementIndex = -1,
        size_t elementCount = 0
    ) {
        constexpr size_t maxBlocksToScan = 1024;

        if (!structure.valid()) return;
        if (depth > 64) throw std::runtime_error("Block structure nesting exceeds 64 levels");
        MapFile* mapFile = structure.context->mapFile;
        if ((char*)structure.address < (char*)startAddress) return;
        if ((char*)structure.address >= (char*)stopAddress) return;
        ancestors.push_back({static_cast<char*>(structure.address), structure.node, elementIndex, elementCount});

        auto fieldRefs = structure.getFieldRefs();
        for (auto& fieldRef : fieldRefs) {
            if (cancel && cancel->load()) return;
            auto fieldAddress = reinterpret_cast<uintptr_t>(fieldRef.address);
            auto start = reinterpret_cast<uintptr_t>(startAddress);
            auto stop = reinterpret_cast<uintptr_t>(stopAddress);
            if (fieldAddress < start || fieldAddress > stop || fieldRef.size() > stop - fieldAddress) {
                spanMap.spans.insert({static_cast<char*>(fieldRef.address),
                    spanEnd(fieldRef.address, fieldRef.size()), nullptr, fieldRef.node->type.type, ancestors});
                continue;
            }

            if (fieldRef.is(Type::DataPointer) && !fieldRef.node->external) {
                DataPointer* dataPtr = (DataPointer*)fieldRef.address;
                if (dataPtr->size < 0) throw std::runtime_error("Negative data payload size");
                if (!dataPtr->size) continue;
                void* start = mapFile->getDataPointer(*dataPtr);
                void* end = spanEnd(start, static_cast<size_t>(dataPtr->size));
                spanMap.spans.insert({(char*)start, (char*)end, nullptr, Type::DataPointer, ancestors});
                continue;
            }

            if (fieldRef.is(Type::Structure)) {
                StructureRef subStructure = fieldRef.getStructure();
                if (subStructure.valid()) {
                    fillSpanMap(subStructure, spanMap, startAddress, stopAddress, depth + 1, cancel, ancestors);
                }
                continue;
            }

            if (!fieldRef.is(Type::BlockPointer)) continue;
            if ((char*)fieldRef.address < (char*)startAddress) break;
            if ((char*)fieldRef.address >= (char*)stopAddress) break;
            StructureNode* type = fieldRef.getReferencedType();
            size_t size = type ? type->size : 0;
            if (size == 0) continue;

            auto count = fieldRef.getBlockElementCount();
            if (count > (std::numeric_limits<size_t>::max)() / size)
                throw std::runtime_error("Block span size overflow");
            if (count > maxBlocksToScan) {
                char* start = (char*) fieldRef.getBlockElement(0).address;
                char* end = spanEnd(start, size * count);
                spanMap.spans.insert({start, end, type, Type::BlockPointer, ancestors, -1, count});
                continue; // Skip scanning individual block elements if too many
            }

            for (int i = 0; i < count; ++i) {
                if (cancel && cancel->load()) return;
                StructureRef blockElement = fieldRef.getBlockElement(i);
                char* start = static_cast<char*>(blockElement.address);
                char* end = spanEnd(start, size);
                auto address = reinterpret_cast<uintptr_t>(start);
                if (address < reinterpret_cast<uintptr_t>(startAddress) ||
                    address >= reinterpret_cast<uintptr_t>(stopAddress) ||
                    size > reinterpret_cast<uintptr_t>(stopAddress) - address) {
                    // Retain the offending extent, but never dereference it.
                    spanMap.spans.insert({start, end, type, Type::BlockPointer, ancestors, i, count});
                    continue;
                }
                if (blockElement.valid()) {
                    spanMap.spans.insert({start, end, type, Type::BlockPointer, ancestors, i, count});
                    fillSpanMap(blockElement, spanMap, startAddress, stopAddress, depth + 1, cancel, ancestors, i, count);
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
