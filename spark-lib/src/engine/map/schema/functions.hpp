#pragma once

#include "schema.hpp"
#include "reference.hpp"
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
        if (!structure.valid()) return 0;
        auto fieldRefs = structure.getFieldRefs();
        for (auto& fieldRef : fieldRefs) {
            if (fieldRef.is(Type::BlockPointer) && (fieldRef.getBlockElementCount() > 0)) {
                StructureRef blockElement = fieldRef.getBlockElement(0);
                if (blockElement.valid()) {
                    return (char*)blockElement.address - (char*)structure.address;
                }
            }
        }
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
