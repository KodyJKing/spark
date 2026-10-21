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
        explicit App(HWND window);
        ~App() override { closeFile(); }

        // Draws the main menu bar and all open tool windows. Call once per frame.
        void render();

        bool openMap(const std::filesystem::path& path);

        // Applies the saved window position, size and maximized state. Returns false if none was usable.
        // Call before the window is first shown.
        static bool restoreWindowPlacement(HWND window);

        bool wantsExit() const { return wantsExit_; }

        // Reopens the map and tag windows from the previous run. Pass a map path to override the saved map.
        void restoreSession(const std::filesystem::path& mapOverride = {});

        // TagBrowserHost
        Engine::Map::MapFile* getMap() override { return map_.get(); }
        bool isFileOpen() override { return map_ != nullptr; }
        std::string getFilePath() override { return map_ ? map_->getFilePath().string() : ""; }
        void openFile() override;
        void closeFile() override;

        private:
        void menuBar();
        static std::filesystem::path defaultSessionPath();
        void saveSessionIfChanged();

        std::filesystem::path sessionPath_;
        std::string lastSessionJson_;

        HWND window_;
        Engine::Map::Spark::ManagedMapFilePtr map_;
        bool wantsExit_ = false;
    };

}
