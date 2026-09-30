#pragma once
#include "../schema/schema.hpp"
#include "../map_file.hpp"

namespace Engine::Map::Spark {

    Tag* copyTag(
        Schema* schema,
        MapFile* fromMap,
        RuntimeMapFile* toMap,
        uint32_t fromTagHandle
    );

    bool copyTagData(
        Schema* schema,
        MapFile* fromMap,
        RuntimeMapFile* toMap,
        uint32_t fromTagHandle,
        // Optional, patched if possible. Will allocate new data block if not.
        void* toTagData = nullptr,
        size_t toTagDataSize = 0
    );

}
