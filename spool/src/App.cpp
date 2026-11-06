#include "App.hpp"

#include <fstream>
#include <stdexcept>

#include "FileDialog.hpp"
#include "InjectionFiles.hpp"
#include "imgui.h"
#include "ui/dissect/DissectTag.hpp"
#include "ui/tag_browser/TagBrowser.hpp"
#include "engine/map/schema/schema_file.hpp"
#include "engine/map/spark/static_tag_injection/inject_tags.hpp"

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

        Engine::Map::Schema* schema = Engine::Map::getMainSchema();
        if (schema) {
            newMap->normalize(schema);
        }
        
        closeFile();
        map_ = newMap;
        return true;
    }

    void App::openFile() {
        try {
            auto path = pickMapFile(window_);
            if (!path.empty())
                openMap(path);
        } catch (const std::exception& error) {
            MessageBoxA(window_, error.what(), "Spool - Open map", MB_OK | MB_ICONERROR);
        }
    }

    void App::closeFile() {
        guestPath_.clear();
        guestBytes_.clear();
        guestTags_.clear();
        guestFilter_.Clear();
        injectionMessage_.clear();
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
            if (ImGui::MenuItem("Open Guest Map for Injection...", nullptr, false, isFileOpen()))
                openGuestMap();
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4"))
                wantsExit_ = true;
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools")) {
            if (ImGui::MenuItem("Tag Browser"))
                SparkLib::UI::showTagBrowserWindow();
            if (ImGui::MenuItem("Tag Injection"))
                showInjection_ = true;
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    void App::openGuestMap() {
        if (!map_) return;
        try {
            auto path = pickMapFile(window_);
            if (path.empty()) return;
            auto bytes = readInjectionMap(path);
            auto tags = Engine::Map::Spark::listInjectionTags(bytes);
            std::vector<GuestTagChoice> choices;
            choices.reserve(tags.size());
            for (const auto& tag : tags) {
                auto label = "[" + Strings::fourccToString(tag.groupId) + "] " + tag.path;
                choices.push_back({tag.groupId, tag.path, std::move(label)});
            }
            guestPath_ = std::filesystem::absolute(path);
            guestBytes_ = std::move(bytes);
            guestTags_ = std::move(choices);
            guestFilter_.Clear();
            injectionMessage_.clear();
            showInjection_ = true;
        } catch (const std::exception& error) {
            MessageBoxA(window_, error.what(), "Spool - Open guest map", MB_OK | MB_ICONERROR);
        }
    }

    void App::injectAndSave() {
        if (!map_ || guestBytes_.empty()) return;
        try {
            const auto hostPath = std::filesystem::absolute(map_->getFilePath());
            auto suggested = hostPath.parent_path() / (hostPath.stem().wstring() + L".injected.map");
            auto destination = pickMapSaveFile(window_, suggested);
            if (destination.empty()) return;
            Engine::Map::Spark::TagList selected;
            for (const auto& tag : guestTags_)
                if (tag.selected) selected.push_back({tag.groupId, tag.path.c_str()});
            auto hostBytes = readInjectionMap(hostPath);
            auto* schema = Engine::Map::getMainSchema();
            if (!schema) throw std::runtime_error("No tag schema is loaded");
            std::vector<std::string> warnings;
            auto output = Engine::Map::Spark::injectTags(hostBytes, guestBytes_, selected, *schema, &warnings);
            saveInjectedMap(destination, output, hostPath, guestPath_);
            injectionMessage_ = "Saved injected map: " + std::filesystem::path(destination).string();
            for (const auto& warning : warnings) injectionMessage_ += "\nWarning: " + warning;
            if (!warnings.empty())
                MessageBoxA(window_, injectionMessage_.c_str(), "Spool - Injection warning", MB_OK | MB_ICONWARNING);
        } catch (const std::exception& error) {
            injectionMessage_ = std::string("Injection failed: ") + error.what();
            MessageBoxA(window_, injectionMessage_.c_str(), "Spool - Tag injection", MB_OK | MB_ICONERROR);
        }
    }

    void App::injectionWindow() {
        if (!showInjection_) return;
        ImGui::SetNextWindowSize(ImVec2(760, 540), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Tag Injection", &showInjection_)) {
            ImGui::End();
            return;
        }
        if (!map_) {
            ImGui::TextUnformatted("Open a host map first. Injection creates a separate output file.");
            if (ImGui::Button("Open Host Map...")) openFile();
            ImGui::End();
            return;
        }
        ImGui::TextWrapped("Host: %s", map_->getFilePath().string().c_str());
        if (ImGui::Button("Open Guest Map...")) openGuestMap();
        if (guestBytes_.empty()) {
            ImGui::TextUnformatted("Choose a second map to select tags from.");
            ImGui::End();
            return;
        }
        ImGui::TextWrapped("Guest: %s", guestPath_.string().c_str());
        ImGui::TextWrapped("Dependencies are included automatically; matching host tags are reused.");
        ImGui::TextWrapped("Uses raw files from disk, not unsaved dissector edits. MCC acceptance remains experimental.");
        guestFilter_.Draw("Filter (group/path)");
        std::vector<int> filtered;
        size_t selectedCount = 0;
        for (size_t i = 0; i < guestTags_.size(); ++i) {
            const auto& tag = guestTags_[i];
            if (tag.selected) ++selectedCount;
            if (guestFilter_.PassFilter(tag.label.c_str())) filtered.push_back(static_cast<int>(i));
        }
        if (ImGui::Button("Select filtered"))
            for (auto i : filtered) guestTags_[i].selected = true;
        ImGui::SameLine();
        if (ImGui::Button("Clear selection"))
            for (auto& tag : guestTags_) tag.selected = false;
        ImGui::SameLine();
        ImGui::Text("%zu selected / %zu tags (%zu shown)", selectedCount, guestTags_.size(), filtered.size());
        auto* schema = Engine::Map::getMainSchema();
        if (ImGui::BeginChild("Guest tags", ImVec2(0, -75), ImGuiChildFlags_Borders)) {
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(filtered.size()));
            while (clipper.Step()) {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                    auto i = filtered[row];
                    auto& tag = guestTags_[i];
                    ImGui::PushID(i);
                    ImGui::Checkbox("##selected", &tag.selected);
                    ImGui::SameLine();
                    ImGui::TextUnformatted(tag.label.c_str());
                    auto* root = schema->structureForGroupId(tag.groupId);
                    if (!root || !root->complete) {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "[not certified]");
                        ImGui::SetItemTooltip(
                            "This tag type is not certified complete. Unmapped pointers or references "
                            "may cause corruption or load failures after relocation.");
                    }
                    ImGui::PopID();
                }
            }
        }
        ImGui::EndChild();
        ImGui::BeginDisabled(selectedCount == 0);
        if (ImGui::Button("Inject Selected and Save As...")) injectAndSave();
        ImGui::EndDisabled();
        if (!injectionMessage_.empty()) ImGui::TextWrapped("%s", injectionMessage_.c_str());
        ImGui::End();
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
        injectionWindow();
        saveSessionIfChanged();
    }

}
