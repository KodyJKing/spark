#pragma once

#include "engine/tags/schema/schema.hpp"
#include "engine/halo1.hpp"
#include <string>
#include <cstdio>
#include <algorithm>

namespace Mod::DevTools::DissectTag {

    using namespace Engine::TagSchema;

    inline std::string toHex(uint32_t value) {
        char buffer[9];
        snprintf(buffer, sizeof(buffer), "%08X", value);
        return std::string(buffer);
    }

    inline std::string toHex(uint64_t value) {
        char buffer[17];
        snprintf(buffer, sizeof(buffer), "%016llX", value);
        return std::string(buffer);
    }

    inline size_t guessDataSize(Engine::Tag* tag) {
        auto nextTag = tag + 1;
        if (!Engine::tagExists(nextTag))
            return 0;
        void* dataPointer = tag->getData();
        void* nextDataPointer = nextTag->getData();
        return reinterpret_cast<uint8_t*>(nextDataPointer) - reinterpret_cast<uint8_t*>(dataPointer);
    }

    inline void deleteFieldFromStructure(Structure& structure, Field* field) {
        auto it = std::find_if(structure.fields.begin(), structure.fields.end(),
                               [&](const Field& f) { return strcmp(f.name, field->name) == 0 && f.offset == field->offset; });
        if (it != structure.fields.end()) {
            structure.fields.erase(it);
        }
    }

    inline void sortFields(Structure& structure) {
        std::sort(structure.fields.begin(), structure.fields.end(), [](const Field& a, const Field& b) {
            return a.offset < b.offset;
        });
    }

}