#pragma once

#include "schema.hpp"
#include "reference.hpp"
#include <string>
#include <random>

#include <iostream>

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

}
