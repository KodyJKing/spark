#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include "engine/map/map_file.hpp"

namespace SparkLib::UI::DissectTag {
    void openWindow(Engine::Map::MapFile* mapFile, uint32_t tagId);
    // Standalone inspection retains the map until its windows and analysis jobs close.
    void openWindow(std::shared_ptr<Engine::Map::MapFile> mapFile, uint32_t tagId, const std::string& mapName);
    // Opens (or focuses) a tab for the structure instance at a byte offset from the tag data start.
    // elementIndex is its block element index, or -1.
    void openStructureWindow(Engine::Map::MapFile* mapFile, uint32_t tagId, size_t offset, const std::string& structureId, int64_t elementIndex = -1);
    void render();

    // Root tag ids, optionally restricted to one map (for host session persistence).
    std::vector<uint32_t> openTagIds(Engine::Map::MapFile* mapFile = nullptr);

    // Closes every window that is inspecting a tag from this map. Call before freeing the map.
    void closeWindowsForMap(Engine::Map::MapFile* mapFile);
    void closeAllWindows();
}
