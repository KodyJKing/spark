#include "InGameHost.hpp"

#include "engine/map/map_file.hpp"
#include "engine/map/spark/managed_map_file.hpp"
#include "utils/Utils.hpp"

namespace Mod::DevTools {

    namespace {

        class InGameHost : public TagBrowserHost {
            public:
            Engine::Map::MapFile* getMap() override {
                if (disk) return disk.get();
                return &run;
            }

            bool isFileOpen() override { return disk != nullptr; }

            std::string getFilePath() override {
                return disk ? disk->getFilePath().string() : "";
            }

            void openFile() override {
                // The native file picker causes a hang in-game. Use a hard coded file for the moment.
                auto path = Utils::getHalo1Directory() / "maps" / "d40.map";
                auto newDisk = Engine::Map::Spark::ManagedMapFile::create(path);
                if (!newDisk) return;
                closeFile();
                disk = newDisk;
            }

            void closeFile() override {
                if (!disk) return;
                releaseMap(disk.get());
                disk.reset();
            }

            bool canLoadTagToRuntime() override { return disk != nullptr; }

            void loadTagToRuntime(uint32_t tagHandle) override {
                if (disk) run.copyTag(disk.get(), tagHandle);
            }

            private:
            Engine::Map::RuntimeMapFile run;
            Engine::Map::Spark::ManagedMapFilePtr disk;
        };

    }

    TagBrowserHost& inGameTagBrowserHost() {
        static InGameHost host;
        return host;
    }

}
