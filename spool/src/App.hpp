#pragma once

#include <filesystem>
#include <windows.h>
#include "imgui.h"

#include "engine/map/spark/managed_map_file.hpp"
#include "engine/map/schema/audit.hpp"
#include "ui/tag_browser/TagBrowserHost.hpp"

namespace Spool {

    // Application-level state. Owns the open map and hands it to tool windows through
    // the TagBrowserHost interface.
    class App : public SparkLib::UI::TagBrowserHost {
        public:
        explicit App(HWND window);
        ~App() override;

        // Draws the main menu bar and all open tool windows. Call once per frame.
        void render();

        bool openMap(const std::filesystem::path& path);

        // Applies the saved window position, size and maximized state. Returns false if none was usable.
        // Call before the window is first shown.
        static bool restoreWindowPlacement(HWND window);

        bool wantsExit() const { return wantsExit_; }

        // Restores the map, tag windows and audit selection. Pass a map path to override the saved map.
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
        void openGuestMap();
        void injectionWindow();
        void injectAndSave();
        void auditWindow();
        void startAudit();
        void openAuditTag(size_t mapIndex, uint32_t tagId);

        struct GuestTagChoice {
            uint32_t groupId;
            std::string path;
            std::string label;
            bool selected = false;
        };

        std::filesystem::path sessionPath_;
        std::string lastSessionJson_;
        std::filesystem::path guestPath_;
        std::vector<uint8_t> guestBytes_;
        std::vector<GuestTagChoice> guestTags_;
        ImGuiTextFilter guestFilter_;
        std::string injectionMessage_;
        bool showInjection_ = false;
        bool showAudit_ = false;
        std::optional<uint32_t> auditGroup_;
        bool auditExcludeBsp_ = false;
        Engine::Map::Analysis::ProblemOptions auditOptions_;
        bool auditExcludeMissingSchema_ = false;
        bool auditDiskMaps_ = false;
        std::vector<std::filesystem::path> auditPaths_;
        std::unique_ptr<Engine::Map::Analysis::AuditBatchJob> audit_;
        std::map<size_t, std::weak_ptr<Engine::Map::Spark::ManagedMapFile>> auditViews_;
        std::string auditError_;

        HWND window_;
        Engine::Map::Spark::ManagedMapFilePtr map_;
        bool wantsExit_ = false;
    };

}
