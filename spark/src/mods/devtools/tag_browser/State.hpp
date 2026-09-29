#pragma once

#include <vector>
#include <mutex>
#include "engine/halo1.hpp"
#include "engine/map/map_file.hpp"
#include "engine/map/managed_map_file.hpp"
#include "utils/FileUtils.hpp"
#include "utils/Utils.hpp"

#include "Constants.hpp"

namespace Mod::DevTools {

    inline static Engine::Map::RuntimeMapFile run;

    inline static Engine::Map::ManagedMapFilePtr disk;

    using Tag = Engine::Map::Tag;

    class State {
        public:
        bool show = false;

        std::string currentFilePath;

        // Map
        Engine::Map::MapFile* getMap() {
            if (disk) return disk.get();
            return &run;
        }

        void openFileDialog() {
            // The file picker is causing the weird hang. Use a hard coded file for the moment.
            auto path = Utils::getHalo1Directory() / "maps" / "d40.map";
            disk = Engine::Map::ManagedMapFile::create(path);
            currentFilePath = path.string();
        }

        void closeFile() {
            disk.reset();
        }

        bool isFileOpen() {
            return disk != nullptr;
        }

        const std::string getFileName() {
            if (!disk) return "";
            std::filesystem::path filePath = disk->getFilePath().filename();
            return filePath.string();
        }

        void copyTagToRuntime(uint32_t handle) {
            if (!isFileOpen()) return;
            run.copyTag(disk.get(), handle);
        }

        // Interaction
        int selection = -1;
        int hovered = -1;
        
        // Pagination
        int tagsPerPage = 50;
        int page = 0;
        uint32_t numPages() { return (totalTags() + tagsPerPage - 1) / tagsPerPage; }
        uint32_t totalTags() { return getMap()->getTagCount(); }
        void clampPage() {
            if (page < 0) page = 0;
            if (page >= numPages()) page = numPages() - 1;
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
            std::string pathStr = getMap()->getTagPath(tag);
            return pathStr.find(search) != std::string::npos;
        }

        ///////////////////////////////////////////////

        bool tagExists(uint32_t handle) {
            uint32_t i = handle & 0xFFFF;
            return i < getMap()->getTagCount();
        }

        Tag* getTag(uint32_t handle) {
            return getMap()->getTag(handle);
        }

        bool validTagPath(const char* path) {
            return Engine::validTagPath(path);
        }
    };

    inline static State state;

};
