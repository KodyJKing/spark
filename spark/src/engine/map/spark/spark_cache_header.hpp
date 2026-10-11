#pragma once

#include "../map_file.hpp"

#include <cstdint>
#include "utils/FourCC.hpp"

#define FOUR_CC_SPARK FOUR_CC('s', 'p', 'r', 'k')
#define FOUR_CC_GUILTY FOUR_CC('g', 'l', 't', 'y')

namespace Engine::Map::Spark {

    struct SparkCacheHeader {
        uint32_t magic = FOUR_CC_SPARK;
        void* relocatedPathStrings = nullptr;
        uint32_t tagCapacity = 0;
        uint32_t magicFooter = FOUR_CC_GUILTY;
    };

    SparkCacheHeader* getOrCreateSparkCacheHeader(RuntimeMapFile* map);

}
