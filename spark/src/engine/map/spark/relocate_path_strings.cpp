#include "spark_cache_header.hpp"
#include "../tag_data_types.hpp"

namespace Engine::Map::Spark {

    bool relocatePathStrings(RuntimeMapFile* map) {
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

}
