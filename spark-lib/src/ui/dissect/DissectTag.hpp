#pragma once

#include <cstdint>
#include <vector>
#include "engine/map/map_file.hpp"

namespace SparkLib::UI::DissectTag {
    void openWindow(Engine::Map::MapFile* mapFile, uint32_t tagId);
    void render();

    // Tag ids of all currently open windows.
    std::vector<uint32_t> openTagIds();

    // Closes every window that is inspecting a tag from this map. Call before freeing the map.
    void closeWindowsForMap(Engine::Map::MapFile* mapFile);
}
