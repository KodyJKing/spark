#include "map_file.hpp"
#include "engine/common.hpp"
#include "engine/rendering/model_data.hpp"
#include "engine/map.hpp"
#include "engine/tags/tag_group_id.hpp"

#include "utils/Strings.hpp"

#define DEBUG_RUNTIME_MAP_FILE

#ifdef DEBUG_RUNTIME_MAP_FILE
#include <iostream>
#define LOG(x) std::cout << "[Engine::RuntimeMapFile] " << x << std::endl;
#else
#define LOG(x)
#endif

namespace Engine::Map {

    // Guest data that lives in the unused portion of the cache header.
    struct SparkCacheHeader {
        uint32_t magic = FOUR_CC('s', 'p', 'r', 'k');
        void* relocatedPathStrings = nullptr;
        uint32_t tagCapacity = 0;
        uint32_t magicFooter = FOUR_CC('g', 'l', 't', 'y');
    };

    SparkCacheHeader* getOrCreateSparkCacheHeader(RuntimeMapFile* map) {
        if (!map) return nullptr;
        CacheHeader* header = (CacheHeader*) map->getCacheHeader();
        if (!header) return nullptr;
        SparkCacheHeader* sparkHeader = (SparkCacheHeader*) &header->unused[0];
        if (sparkHeader->magic != FOUR_CC('s', 'p', 'r', 'k')) {
            *sparkHeader = SparkCacheHeader();
        }
        return sparkHeader;
    }
    
    bool movePathStrings(RuntimeMapFile* map) {
        SparkCacheHeader* sparkHeader = getOrCreateSparkCacheHeader(map);
        if (!sparkHeader) return false;
        if (sparkHeader->relocatedPathStrings) return true;
        
        uint32_t tagCount = map->getTagCount();

        // 1. Measure total length of all path strings
        size_t totalLength = 0;
        for (uint32_t i = 0; i < tagCount; ++i) {
            Tag* tag = map->getTag(i);
            char* path = (char*) map->getTagPath(tag);
            totalLength += strlen(path) + 1;
        }
        
        // 2. Allocate a contiguous block of memory for all resource strings.
        char* newBlock = (char*) map->allocate(totalLength);
        if (!newBlock) return false;

        // 3. Copy each path string into the new block and update the tag to point to it.
        char* current = newBlock;
        for (uint32_t i = 0; i < tagCount; ++i) {
            Tag* tag = map->getTag(i);
            char* path = (char*) map->getTagPath(tag);
            size_t len = strlen(path) + 1;
            memcpy(current, path, len);
            tag->path = map->toRelative<PointerBase_Tags>(current);
            current += len;
        }

        sparkHeader->relocatedPathStrings = newBlock;
        
        TagDataHeader* tagHeader = map->getTagDataHeader();
        uint32_t addedCapacity = totalLength / sizeof(Tag);
        sparkHeader->tagCapacity = tagHeader->tagCount + addedCapacity;

        return true;
    }

    Tag* allocateTag(RuntimeMapFile* map) {
        // Make room for more tags by moving path strings to a new location.
        movePathStrings(map);

        SparkCacheHeader* sparkHeader = getOrCreateSparkCacheHeader(map);
        if (!sparkHeader) return nullptr;
        if (sparkHeader->tagCapacity <= map->getTagCount()) return nullptr;

        TagDataHeader* tagHeader = map->getTagDataHeader();
        int newIndex = tagHeader->tagCount;
        tagHeader->tagCount++;
        
        Tag* newTag = map->getTag(newIndex);
        memset(newTag, 0, sizeof(Tag));

        newTag->groupID = GroupId_Invalid;
        newTag->parentGroupID = GroupId_Invalid;
        newTag->grandparentGroupID = GroupId_Invalid;

        uint32_t newTagHandle = 0xE1170000 | (newIndex & 0xFFFF);
        newTag->tagHandle = newTagHandle;

        return newTag;
    }

    ////////////////////////////////////

    void* RuntimeMapFile::getPointerBase(PointerBase b) {
        switch(b) {
            case PointerBase_Map: return (void*) (dllBase() + 0x2B22744);
            case PointerBase_Tags: return (void*) tagDataBase();
            case PointerBase_Vertices: return getModelDataPointer();
            case PointerBase_Indices: return (char*) getModelDataPointer() + getVertexDataSize();
            default: return nullptr;
        }
    }

    TagDataHeader* RuntimeMapFile::getTagDataHeader() {
        return (TagDataHeader*) tagHeaderBase();
    }

    void* RuntimeMapFile::allocate(size_t size) {
        // Todo: Use a Halo owned allocator so we don't need to free anything on quit-to-menu.
        return Engine::allocateMapMemory(size);
    }

    void RuntimeMapFile::free(void* ptr) {
        Engine::freeMapMemory(ptr);
    }

    Tag* RuntimeMapFile::allocateTag() {
        return Engine::Map::allocateTag(this);
    }

}
