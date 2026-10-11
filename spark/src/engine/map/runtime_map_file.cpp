#include "engine/rendering/model_data.hpp"
#include "engine/tags/tag_group_id.hpp"
#include "engine/common.hpp"
#include "engine/map.hpp"
#include "schema/schema_file.hpp"
#include "map_file.hpp"
#include "spark/copy_tag.hpp"
#include "spark/spark_cache_header.hpp"
#include "spark/relocate_path_strings.hpp"

#include "utils/Strings.hpp"

#define DEBUG_RUNTIME_MAP_FILE

#ifdef DEBUG_RUNTIME_MAP_FILE
#include <iostream>
#define LOG(x) std::cout << "[Engine::RuntimeMapFile] " << x << std::endl;
#else
#define LOG(x)
#endif

namespace Engine::Map {

    Tag* allocateTag(RuntimeMapFile* map) {
        // Make room for more tags by moving path strings to a new location.
        Spark::relocatePathStrings(map);

        Spark::SparkCacheHeader* sparkHeader = Spark::getOrCreateSparkCacheHeader(map);
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

    Tag *RuntimeMapFile::copyTag(MapFile *sourceMap, uint32_t sourceTagHandle) {
        Schema* schema = getMainSchema();
        return Spark::copyTag(
            schema,
            sourceMap,
            this,
            sourceTagHandle
        );
    }

    void *RuntimeMapFile::allocate(size_t size) {
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
