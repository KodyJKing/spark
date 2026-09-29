#pragma once
#include "map_file.hpp"

namespace Engine::Map {
    size_t getTagDataSize(MapFile* map, uint32_t tagHandle);
}
