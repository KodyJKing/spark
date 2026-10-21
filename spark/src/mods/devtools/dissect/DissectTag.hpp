#pragma once

#include <cstdint>
#include "engine/map/map_file.hpp"

namespace Mod::DevTools::DissectTag {
    void openWindow(Engine::Map::MapFile* mapFile, uint32_t tagId);
    void render();

    // Closes every window that is inspecting a tag from this map. Call before freeing the map.
    void closeWindowsForMap(Engine::Map::MapFile* mapFile);
}
