// Reverse engineered Halo 1 structures and access functions go here.

#pragma once

#include <stdint.h>
#include <string>
#include "map.hpp"
#include "memory/Memory.hpp"
#include "utils/Strings.hpp"
#include "spark/SparkAPI.h"
#include "engine/tags/tag_group_id.hpp"

namespace Engine {

    // Thanks to Kavawuvi for documentation on the map format and tag structure.
    class SPARK_API Tag {
        public:
        uint32_t groupID; // Group ID's are fourcc's
        uint32_t parentGroupID; 
        uint32_t grandparentGroupID;
        uint32_t tagID; 
        uint32_t resourcePathAddress; 
        uint32_t dataAddress; 
        char pad_0018[8]; 

        char* getResourcePath();
        void* getData();
        std::string groupIDStr();
        std::string classIdStr();

        template<typename T>
        T* getDataAs() {
            return (T*) getData();
        }
    };

    template<typename T>
    T* getTagDataAs(Tag* tag) {
        if (!tag) return nullptr;
        return (T*) tag->getData();
    }

    // Represents a block of structs in a tag's data.
    template <typename T = void>
    struct BlockPointerTyped {
        uint32_t count;
        uint32_t offset;   // Use translateMapAddress to get the actual pointer
        uint32_t bullshit; // Not sure what this does.

        template<typename U = T>
        U* get(size_t index, bool safe = true) {
            if (index >= count) return nullptr;
            uint64_t baseAddress = Engine::translateMapAddress(offset);
            if (!baseAddress) return nullptr;
            if (safe && !Memory::isAllocatedFast(baseAddress + index * sizeof(U))) return nullptr;
            return (U*)(baseAddress + index * sizeof(U));
        }
    };

    using BlockPointer = BlockPointerTyped<void>;
    
    SPARK_API uint32_t getTagArraySize();
    SPARK_API Tag *getTag(uint32_t tagID);
    SPARK_API Tag * findTag(const char * path, const char * fourCC);
    SPARK_API Tag * findTag(const char * path, uint32_t fourCC);
    SPARK_API bool validTagPath(const char * path);
    SPARK_API bool tagExists(Tag * tag);
}
