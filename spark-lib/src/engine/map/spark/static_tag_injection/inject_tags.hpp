#pragma once

#include <vector>
#include "engine/map/map_file.hpp"

namespace Engine::Map::Spark {

    struct PortableTagId {
        uint32_t groupId;
        const char* path;
    };

    using TagList = std::vector<PortableTagId>;

    void injectTags(
        RawMapFile& hostMap, 
        RawMapFile& guestMap,
        TagList& guestTags, 
        RawMapFile& newMap
    );

}
