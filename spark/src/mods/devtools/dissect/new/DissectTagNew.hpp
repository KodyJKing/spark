#pragma once

#include <cstdint>
#include "engine/map/map_file.hpp"

namespace Mod::DevTools::DissectTagNew {
    void openWindow(Engine::Map::MapFile* mapFile, uint32_t tagId);
    void render();
}
