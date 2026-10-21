#include "App.hpp"

#include "FileDialog.hpp"
#include "imgui.h"
#include "ui/tag_browser/TagBrowser.hpp"

namespace Spool {

    bool App::openMap(const std::filesystem::path& path) {
        auto newMap = Engine::Map::Spark::ManagedMapFile::create(path);
        if (!newMap) {
            auto message = L"Failed to open map file:\n" + path.wstring();
            MessageBoxW(window_, message.c_str(), L"Spool", MB_OK | MB_ICONERROR);
            return false;
        }
        closeFile();
        map_ = newMap;
        return true;
    }

    void App::openFile() {
        auto path = pickMapFile(window_);
        if (!path.empty())
            openMap(path);
    }

    void App::closeFile() {
        if (!map_) return;
        SparkLib::UI::releaseMap(map_.get());
        map_.reset();
    }

    void App::menuBar() {
        if (!ImGui::BeginMainMenuBar()) return;

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open Map...", "Ctrl+O"))
                openFile();
            if (ImGui::MenuItem("Close Map", nullptr, false, isFileOpen()))
                closeFile();
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4"))
                wantsExit_ = true;
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools")) {
            if (ImGui::MenuItem("Tag Browser"))
                SparkLib::UI::showTagBrowserWindow();
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    void App::render() {
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_O))
            openFile();

        menuBar();
        SparkLib::UI::tagBrowser(*this);
    }

}
