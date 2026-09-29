#include "schema/schema.hpp"
#include "schema/reference.hpp"
#include "engine/map.hpp"

namespace Engine::Map {

    size_t getTagDataSize(MapFile* map, uint32_t tagHandle) {
        // Todo: Replace with a more robust approach.
        //   For RawMapFile, needs to work for final tag by looking at CacheHeader.tagDataSize
        //   For RuntimeMapFile, needs Spark to actively track the size of pre-existing and injected tags.
        //   Once our schemas are fully mapped out, we will be able to do this exactly assuming contiguous tagdata blocks.
        
        return map->guessTagDataSize(tagHandle);
    }

}
