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
#ifndef SPOOL
    // Called each overlay frame, separately from the pause-menu tag browser.
    // Unpaused results remain visible without capturing mouse or keyboard input.
    void renderInstructionAccessWindows(bool interactive);
#endif

    // Root tag ids, optionally restricted to one map (for host session persistence).
    std::vector<uint32_t> openTagIds(Engine::Map::MapFile* mapFile = nullptr);

    // Stops instruction watches and closes every window inspecting this map. Call before freeing it.
    void closeWindowsForMap(Engine::Map::MapFile* mapFile);
    void closeAllWindows();
}
