#pragma once

#include <vector>
#include <mutex>
#include <string>
#include <cstring>
#include "engine/map/map_file.hpp"
#include "engine/map/tag_path.hpp"
#include "TagBrowserHost.hpp"

#include "Constants.hpp"

namespace Mod::DevTools {

    using Tag = Engine::Map::Tag;

    class State {
        public:
        bool show = false;

        // Injected by the hosting application each frame.
        TagBrowserHost* host = nullptr;

        // Map
        Engine::Map::MapFile* getMap() { return host ? host->getMap() : nullptr; }
        Engine::Map::MapFile* lastMap = nullptr;

        void openFile() { if (host) host->openFile(); }
        void closeFile() { if (host) host->closeFile(); }
        bool isFileOpen() { return host && host->isFileOpen(); }
        std::string getFilePath() { return host ? host->getFilePath() : ""; }
        bool canLoadTagToRuntime() { return host && host->canLoadTagToRuntime(); }
        void loadTagToRuntime(uint32_t handle) { if (host) host->loadTagToRuntime(handle); }

        // Interaction
        int selection = -1;
        int hovered = -1;
        
        // Pagination
        int tagsPerPage = 50;
        int page = 0;
        int numPages() { return (int)((totalTags() + tagsPerPage - 1) / tagsPerPage); }
        uint32_t totalTags() { auto map = getMap(); return map ? map->getTagCount() : 0; }
        void clampPage() {
            int n = numPages();
            if (page >= n) page = n - 1;
            if (page < 0) page = 0;
        }
        
        // Search
        char search[512] = {0};
        char lastSearch[512] = {0};
        int groupIdFilter = 0;
        int lastGroupIdFilter = 0;
        std::vector<int> searchResults;
        uint64_t lastSearchTick = 0;
        std::mutex searchMutex;
        bool hasSearch() { return search[0] != 0 || groupIdFilter != 0; }
        bool searchChanged() { return strcmp(search, lastSearch) != 0 || groupIdFilter != lastGroupIdFilter; }
        bool filterTag(Tag* tag) {
            if (!tag) return false;
            auto filterGroupID = ids[groupIdFilter].groupID;
            if (
                filterGroupID != GROUP_ID_ALL && 
                filterGroupID != tag->groupID &&
                filterGroupID != tag->parentGroupID &&
                filterGroupID != tag->grandparentGroupID
            ) 
                return false;
            if (search[0] == 0)
                return true;
            auto map = getMap();
            if (!map) return false;
            std::string pathStr = map->getTagPath(tag);
            return pathStr.find(search) != std::string::npos;
        }

        // Called when the host swaps or closes its map.
        void resetForNewMap() {
            std::lock_guard<std::mutex> lock(searchMutex);
            searchResults.clear();
            lastSearch[0] = 0;
            lastGroupIdFilter = 0;
            selection = -1;
            hovered = -1;
            page = 0;
        }

        ///////////////////////////////////////////////

        bool tagExists(uint32_t handle) {
            uint32_t i = handle & 0xFFFF;
            return i < totalTags();
        }

        Tag* getTag(uint32_t handle) {
            auto map = getMap();
            return map ? map->getTag(handle) : nullptr;
        }

        bool validTagPath(const char* path) {
            return Engine::Map::isValidTagPath(path);
        }
    };

    inline static State state;

};
