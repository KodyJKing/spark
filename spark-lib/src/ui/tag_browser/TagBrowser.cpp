#include "imgui.h"
#include <cmath>
#include "ui/dissect/DissectTag.hpp"

#include "State.hpp"
#include "Constants.hpp"
#include "Search.hpp"
#include "TagRow.hpp"

namespace SparkLib::UI {

    void showTagBrowserWindow() {
        state.show = true;
    }

    bool isTagBrowserWindowOpen() {
        return state.show;
    }

    void releaseMap(Engine::Map::MapFile* map) {
        // Waits for any in-flight search, then drops everything that points into the map.
        state.resetForNewMap();
        DissectTag::closeWindowsForMap(map);
    }

    void menuBar() {
        ImGui::BeginMenuBar();
        if (ImGui::BeginMenu("File")) {
            if (state.isFileOpen()) {
                if (ImGui::MenuItem("Close")) {
                    state.closeFile();
                }
            } else {
                if (ImGui::MenuItem("Open")) {
                    state.openFile();
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Debug")) {
            
            ImGui::EndMenu();
        }

        if (state.isFileOpen()) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 64));
            std::string tail = state.getFilePath();
            constexpr size_t maxTailLength = 40;
            if (tail.size() > maxTailLength) tail = "..." + tail.substr(tail.size() - maxTailLength);
            ImGui::Text(tail.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::EndMenuBar();
    }

    void tagBrowser(TagBrowserHost& host) {
        state.host = &host;
        SparkLib::UI::DissectTag::render();
        
        if (!state.show) return;

        ImGui::Begin("Tag Browser", &state.show, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_MenuBar);
        menuBar();

        auto map = state.getMap();
        if (map != state.lastMap) {
            state.lastMap = map;
            state.resetForNewMap();
        }
        if (!map) {
            ImGui::Text("No map open.");
            ImGui::End();
            return;
        }

        //////////////////////////////////////////////////////////////////////////
        // Pagination
        
        ImGui::InputInt("Page", &state.page);
        if (ImGui::IsWindowHovered()) state.page -= (int) ImGui::GetIO().MouseWheel;
        state.clampPage();
        ImGui::SameLine();
        ImGui::InputInt("Page size", &state.tagsPerPage);
        if (state.tagsPerPage < 1) state.tagsPerPage = 1;

        //////////////////////////////////////////////////////////////////////////
        // Search

        ImGui::Separator();
        if (ImGui::BeginCombo("##GroupID", ids[state.groupIdFilter].name)) {
            for (int i = 0; i < NUM_GROUP_IDS; i++) {
                bool isSelected = state.groupIdFilter == i;
                if (ImGui::Selectable(ids[i].name, isSelected)) state.groupIdFilter = i;
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        ImGui::InputText("Search", state.search, sizeof(state.search));
        bool focused = ImGui::IsItemActive();
        
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            state.search[0] = 0;
            state.groupIdFilter = 0;
        }

        bool hasSearch = state.hasSearch();

        tickSearch(focused);

        if (hasSearch)  {
            int numPages = (int) ceil( state.searchResults.size() / (float) state.tagsPerPage );
            ImGui::Text("%d results, %d pages", state.searchResults.size(), numPages);
            if (state.page >= numPages) state.page = numPages - 1;
            if (state.page < 0) state.page = 0;
        }

        //////////////////////////////////////////////////////////////////////////
        // Results

        ImGui::Separator();

        if (hasSearch) {
            // Render search results
            if (state.searchMutex.try_lock()) {
                int pageBase = state.page * state.tagsPerPage;
                for (int i = 0; i < state.tagsPerPage; i++) {
                    int j = pageBase + i;
                    int index = -1;
                    if (j < state.searchResults.size())
                        index = state.searchResults[j];
                    renderTagRow(index);
                }
                state.searchMutex.unlock();
            } else {
                ImGui::Text("Searching...");
            }
        } else {
            // Render all tags
            for (int i = 0; i < state.tagsPerPage; i++) {
                int index = state.page * state.tagsPerPage + i;
                renderTagRow(index);
            }
        }

        ImGui::End();
    }

}
