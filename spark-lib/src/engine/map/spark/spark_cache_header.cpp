#include "spark_cache_header.hpp"

namespace Engine::Map::Spark {

    SparkCacheHeader* getOrCreateSparkCacheHeader(RuntimeMapFile* map) {
        if (!map) return nullptr;
        CacheHeader* header = (CacheHeader*) map->getCacheHeader();
        if (!header) return nullptr;
        SparkCacheHeader* sparkHeader = (SparkCacheHeader*) &header->unused[0];
        if (sparkHeader->magic != FOUR_CC_SPARK) {
            *sparkHeader = SparkCacheHeader();
        }
        return sparkHeader;
    }

}
