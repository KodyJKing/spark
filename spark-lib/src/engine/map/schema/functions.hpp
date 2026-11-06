#pragma once

#include "schema.hpp"
#include "reference.hpp"
#include <iostream>
#include <string>
#include <random>

namespace Engine::Map {
    
    inline char randomHexChar() {
        static const char hexChars[] = "0123456789abcdef";
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        return hexChars[dis(gen)];
    }

    inline Id createId() {
        char id[16];
        for (int i = 0; i < 16; ++i) {
            id[i] = randomHexChar();
        }
        return std::string(id, 16);
    }

    inline size_t guessSizeFromFirstBlockElement(StructureRef& structure) {
        auto findFirstChild = [](auto&& self, StructureRef& current, size_t depth) -> void* {
            if (!current.valid() || !current.node) return nullptr;
            if (depth > 64) {
                std::cerr << "[Schema] First-child size heuristic: inline structure nesting exceeds 64 levels\n";
                return nullptr;
            }
            auto fieldRefs = current.getFieldRefs();
            for (auto& fieldRef : fieldRefs) {
                if (fieldRef.is(Type::Structure)) {
                    auto embedded = fieldRef.getStructure();
                    if (auto child = self(self, embedded, depth + 1)) return child;
                } else if (fieldRef.is(Type::BlockPointer) && fieldRef.getBlockElementCount() > 0) {
                    auto blockElement = fieldRef.getBlockElement(0);
                    if (blockElement.valid()) return blockElement.address;
                }
            }
            return nullptr;
        };
        // Inline offsets must not change the origin of the size estimate.
        if (auto child = findFirstChild(findFirstChild, structure, 0))
            return static_cast<char*>(child) - static_cast<char*>(structure.address);
        return 0;
    }

    // Simple heuristic to guess the size of a block pointer.
    // Looks at next sibling block pointer field (if it exists).
    // Assumes the siblings next block marks the end of available space for this block pointer.
    inline size_t guessBlockPointerSizeSimple(FieldRef& fieldRef) {
        if (!fieldRef.is(Type::BlockPointer) || (fieldRef.getBlockElementCount() == 0)) return 0;
        StructureRef parent = fieldRef.parent;
        if (!parent.valid()) return 0;
        auto blockElement = fieldRef.getBlockElement(0);
        if (!blockElement.valid()) return 0;
        auto fieldRefs = parent.getFieldRefs();
        bool found = false;
        for (auto& sibling : fieldRefs) {
            if (sibling.address == fieldRef.address) {
                found = true;
                continue;
            }
            if (!found) continue;
            if (sibling.is(Type::BlockPointer) && (sibling.getBlockElementCount() > 0)) {
                auto siblingBlockElement = sibling.getBlockElement(0);
                if (!siblingBlockElement.valid()) continue;
                size_t space = (char*)siblingBlockElement.address - (char*)blockElement.address;
                return space / fieldRef.getBlockElementCount();
            }
        }
        return 0;
    }

}
