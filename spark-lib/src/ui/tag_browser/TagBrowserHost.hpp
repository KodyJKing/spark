#pragma once

#include <cstdint>
#include <string>
#include "engine/map/map_file.hpp"

namespace SparkLib::UI {

    // Dependency injected into the Tag Browser by whatever application hosts it
    // (the in-game devtools mod, or the standalone Spool app). The host owns the open map.
    class TagBrowserHost {
        public:
        virtual ~TagBrowserHost() = default;

        // The map being browsed. Null if there is none.
        virtual Engine::Map::MapFile* getMap() = 0;

        // Whether a map file from disk is open.
        virtual bool isFileOpen() = 0;
        virtual std::string getFilePath() = 0;

        // Open/close are initiated by the UI. Hosts must call releaseMap() on the old map
        // before destroying it.
        virtual void openFile() = 0;
        virtual void closeFile() = 0;

        // Optional: copy a tag from the open file into the running game.
        virtual bool canLoadTagToRuntime() { return false; }
        virtual void loadTagToRuntime(uint32_t tagHandle) {}
    };

    // Hosts must call this before they free a map. Stops background work and closes
    // dissect windows that reference it.
    void releaseMap(Engine::Map::MapFile* map);

}
