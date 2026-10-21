#pragma once

#include <filesystem>
#include <windows.h>

#include "engine/map/spark/managed_map_file.hpp"
#include "ui/tag_browser/TagBrowserHost.hpp"

namespace Spool {

    // Application-level state. Owns the open map and hands it to tool windows through
    // the TagBrowserHost interface.
    class App : public SparkLib::UI::TagBrowserHost {
        public:
        explicit App(HWND window) : window_(window) {}
        ~App() override { closeFile(); }

        // Draws the main menu bar and all open tool windows. Call once per frame.
        void render();

        bool openMap(const std::filesystem::path& path);

        bool wantsExit() const { return wantsExit_; }

        // TagBrowserHost
        Engine::Map::MapFile* getMap() override { return map_.get(); }
        bool isFileOpen() override { return map_ != nullptr; }
        std::string getFilePath() override { return map_ ? map_->getFilePath().string() : ""; }
        void openFile() override;
        void closeFile() override;

        private:
        void menuBar();

        HWND window_;
        Engine::Map::Spark::ManagedMapFilePtr map_;
        bool wantsExit_ = false;
    };

}
