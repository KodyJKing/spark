#include "App.hpp"

#include <fstream>

#include "FileDialog.hpp"
#include "imgui.h"
#include "ui/dissect/DissectTag.hpp"
#include "ui/tag_browser/TagBrowser.hpp"

namespace Spool {

    App::App(HWND window) : window_(window) {
        sessionPath_ = defaultSessionPath();
    }

    std::filesystem::path App::defaultSessionPath() {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        return std::filesystem::path(exePath).parent_path() / "spool_session.json";
    }

    bool App::restoreWindowPlacement(HWND window) {
        std::ifstream in(defaultSessionPath());
        if (!in) return false;
        auto session = nlohmann::json::parse(in, nullptr, false);
        if (!session.is_object() || !session.contains("window")) return false;
        auto& w = session["window"];
        if (!w.is_object()) return false;

        WINDOWPLACEMENT wp = { sizeof(wp) };
        wp.rcNormalPosition = {
            w.value("left", 0), w.value("top", 0), w.value("right", 0), w.value("bottom", 0)
        };
        if (wp.rcNormalPosition.right - wp.rcNormalPosition.left < 200 ||
            wp.rcNormalPosition.bottom - wp.rcNormalPosition.top < 150)
            return false;
        // Ignore placements left over from a monitor that is no longer attached.
        if (!MonitorFromRect(&wp.rcNormalPosition, MONITOR_DEFAULTTONULL))
            return false;

        wp.showCmd = w.value("maximized", false) ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
        return SetWindowPlacement(window, &wp) != 0;
    }

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

    void App::restoreSession(const std::filesystem::path& mapOverride) {
        nlohmann::json session;
        std::ifstream in(sessionPath_);
        if (in) session = nlohmann::json::parse(in, nullptr, false);
        if (!session.is_object()) session = nlohmann::json::object();

        std::filesystem::path savedMap = session.value("map", std::string());
        auto path = mapOverride.empty() ? savedMap : mapOverride;
        if (!path.empty() && std::filesystem::exists(path) && openMap(path)) {
            // Tag ids only mean something for the map they were saved with.
            if (path == savedMap && session.contains("dissect")) {
                for (auto& id : session["dissect"]) {
                    uint32_t tagId = id.get<uint32_t>();
                    if (map_->getTag(tagId))
                        SparkLib::UI::DissectTag::openWindow(map_.get(), tagId);
                }
            }
        }

        if (!mapOverride.empty() || session.value("tagBrowser", false))
            SparkLib::UI::showTagBrowserWindow();

        lastSessionJson_ = session.dump();
    }

    // Writes on every change rather than at exit, so a build killing the process loses nothing.
    void App::saveSessionIfChanged() {
        nlohmann::json session;
        session["map"] = map_ ? map_->getFilePath().string() : "";
        session["tagBrowser"] = SparkLib::UI::isTagBrowserWindowOpen();
        session["dissect"] = SparkLib::UI::DissectTag::openTagIds();
        WINDOWPLACEMENT wp = { sizeof(wp) };
        if (GetWindowPlacement(window_, &wp) && wp.showCmd != SW_SHOWMINIMIZED) {
            session["window"] = {
                { "left", wp.rcNormalPosition.left }, { "top", wp.rcNormalPosition.top },
                { "right", wp.rcNormalPosition.right }, { "bottom", wp.rcNormalPosition.bottom },
                { "maximized", wp.showCmd == SW_SHOWMAXIMIZED },
            };
        } else if (lastSessionJson_.size()) {
            auto prev = nlohmann::json::parse(lastSessionJson_, nullptr, false);
            if (prev.is_object() && prev.contains("window")) session["window"] = prev["window"];
        }
        auto text = session.dump();
        if (text == lastSessionJson_) return;
        lastSessionJson_ = text;
        std::ofstream(sessionPath_) << text;
    }

    void App::render() {
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_O))
            openFile();

        menuBar();
        SparkLib::UI::tagBrowser(*this);
        saveSessionIfChanged();
    }

}
