#include "mods/devtools/map_file/MapFileViewer.hpp"
#include <Windows.h>
#include <commdlg.h>
#include <cstdio>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include "imgui.h"
#include "engine/map_file/map_file.hpp"
#include "utils/ImGuiUtils.hpp"

#pragma comment(lib, "comdlg32.lib")

// Standalone map file viewer. Uses Engine::MapFile directly and is not wired
// into TagBrowser or any other existing tag/live-memory system.
namespace Mod::DevTools {

    bool showMapFileViewer = false;

    namespace {
        Engine::MapFile::MapFile* loadedMap = nullptr;
        std::string loadedMapPath;
        std::string loadError;
        int selectedTagIndex = -1;

        bool openMapFileDialog(std::string& outPath) {
            char fileBuffer[MAX_PATH] = {0};

            OPENFILENAMEA ofn = {0};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = GetForegroundWindow();
            ofn.lpstrFilter = "Halo Map Files (*.map)\0*.map\0All Files (*.*)\0*.*\0";
            ofn.lpstrFile = fileBuffer;
            ofn.nMaxFile = sizeof(fileBuffer);
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
            ofn.lpstrTitle = "Open Halo Map File";

            if (!GetOpenFileNameA(&ofn))
                return false;

            outPath = fileBuffer;
            return true;
        }

        // GetOpenFileNameA pumps its own modal message loop and blocks the calling thread,
        // so it must not run on the render thread. Guarded by dialogMutex.
        std::mutex dialogMutex;
        bool dialogInProgress = false;
        std::optional<std::string> pendingDialogResult;

        bool isDialogInProgress() {
            std::lock_guard<std::mutex> lock(dialogMutex);
            return dialogInProgress;
        }

        void openMapFileDialogAsync() {
            std::lock_guard<std::mutex> lock(dialogMutex);
            if (dialogInProgress) return;
            dialogInProgress = true;

            std::thread([]() {
                std::string path;
                bool picked = openMapFileDialog(path);

                std::lock_guard<std::mutex> lock(dialogMutex);
                dialogInProgress = false;
                if (picked)
                    pendingDialogResult = path;
            }).detach();
        }

        void closeMap() {
            delete loadedMap;
            loadedMap = nullptr;
            loadedMapPath.clear();
            loadError.clear();
            selectedTagIndex = -1;
        }

        void openMap(const std::string& path) {
            closeMap();

            auto* map = new Engine::MapFile::MapFile();
            if (!map->loadFromFile(path)) {
                delete map;
                loadError = "Failed to load map file: " + path;
                return;
            }
            loadedMap = map;
            loadedMapPath = path;
        }

        // groupId is not null-terminated in the file; copy it into a scratch buffer for display.
        const char* fourCcLabel(const char groupId[4], char (&scratch)[5]) {
            scratch[0] = groupId[0];
            scratch[1] = groupId[1];
            scratch[2] = groupId[2];
            scratch[3] = groupId[3];
            scratch[4] = 0;
            return scratch;
        }

        void renderModelInfo(Engine::MapFile::TagData::Model& model) {
            auto* geometryBlock = model.geometries();
            auto* geometries = loadedMap->resolveBlock(*geometryBlock);
            ImGui::Text("Geometries: %u", geometryBlock->count);
            if (!geometries) return;

            for (uint32_t g = 0; g < geometryBlock->count; g++) {
                auto& geometry = geometries[g];
                auto* partBlock = geometry.parts();
                auto* parts = loadedMap->resolveBlock(*partBlock);
                ImGui::Text("Geometry %u: %u parts", g, partBlock->count);
                if (!parts) continue;

                ImGui::Indent();
                for (uint32_t p = 0; p < partBlock->count; p++) {
                    auto& part = parts[p];
                    Engine::MapFile::Vec3* centroid = part.centroid();
                    auto* vertexData = part.vertexData();
                    auto [vertexPtr, vertexSize] = loadedMap->getVertexData(*vertexData);
                    ImGui::Text(
                        "Part %u: centroid (%.2f, %.2f, %.2f), vertex type %u, count %u (%zu bytes)",
                        p, centroid->x, centroid->y, centroid->z,
                        (unsigned)vertexData->type, vertexData->count, vertexSize
                    );
                    ImGuiUtils::renderCopyableTextf("Vertex Data Address:", "%p", (void*)vertexPtr);
                }
                ImGui::Unindent();
            }
        }

        void renderBitmapInfo(Engine::MapFile::TagData::Bitmap& bitmap) {
            auto* dataBlock = bitmap.bitmapData();
            auto* entries = loadedMap->resolveBlock(*dataBlock);
            ImGui::Text("Bitmap entries: %u", dataBlock->count);
            if (!entries) return;

            ImGui::Indent();
            for (uint32_t i = 0; i < dataBlock->count; i++) {
                auto& entry = entries[i];
                void* texturePtr = loadedMap->getTextureDataPointer(entry);
                ImGui::Text(
                    "%u: %ux%ux%u fmt=%u size=0x%X",
                    i, (unsigned)entry.width, (unsigned)entry.height, (unsigned)entry.depth,
                    (unsigned)entry.format, entry.dataSize
                );
                ImGuiUtils::renderCopyableTextf("Texture Data Address:", "%p", texturePtr);
            }
            ImGui::Unindent();
        }

        void renderEffectInfo(Engine::MapFile::TagData::Effect& effect) {
            auto* locations = loadedMap->resolveBlock(effect.locations);
            ImGui::Text("Effect locations: %u", effect.locations.count);
            if (!locations) return;

            ImGui::Indent();
            for (uint32_t i = 0; i < effect.locations.count; i++)
                ImGui::Text("%u: %.32s", i, locations[i].name);
            ImGui::Unindent();
        }

        void renderSelectedTagDetails() {
            if (selectedTagIndex < 0) return;

            Engine::MapFile::Tag* tag = loadedMap->getTag((uint32_t)selectedTagIndex);
            if (!tag) return;

            ImGui::Separator();
            ImGui::Text("Selected Tag #%d", selectedTagIndex);

            char groupIdStr[5];
            fourCcLabel(tag->groupId, groupIdStr);
            ImGui::Text("Group ID: %s", groupIdStr);
            ImGui::Text("Tag ID: 0x%X", tag->tagID);

            const char* path = loadedMap->getTagPath(*tag);
            ImGui::Text("Path: %s", path ? path : "<invalid path>");

            void* data = loadedMap->getTagData(*tag);
            ImGuiUtils::renderCopyableTextf("Data Address:", "%p", data);

            ImGui::Separator();

            if (auto* model = loadedMap->getModelData(*tag))
                renderModelInfo(*model);
            else if (auto* bitmap = loadedMap->getBitmapData(*tag))
                renderBitmapInfo(*bitmap);
            else if (auto* effect = loadedMap->getEffectData(*tag))
                renderEffectInfo(*effect);
            else
                ImGui::Text("No known layout for this tag type.");
        }

        void renderTagRow(uint32_t index) {
            Engine::MapFile::Tag* tag = loadedMap->getTag(index);
            if (!tag) {
                ImGui::Text("%05u NULL", index);
                return;
            }

            ImGui::PushID((int)index);

            char groupIdStr[5];
            fourCcLabel(tag->groupId, groupIdStr);

            const char* path = loadedMap->getTagPath(*tag);
            void* data = loadedMap->getTagData(*tag);

            char label[600];
            snprintf(label, sizeof(label), "%05u  %-4s  %s", index, groupIdStr, path ? path : "<invalid path>");

            float addressColumnWidth = 140.0f;
            if (ImGui::Selectable(label, selectedTagIndex == (int)index, 0, ImVec2(ImGui::GetContentRegionAvail().x - addressColumnWidth, 0)))
                selectedTagIndex = (int)index;

            ImGui::SameLine();
            ImGuiUtils::renderCopyableTextf("", "%p", data);

            ImGui::PopID();
        }
    }

    void mapFileViewer() {
        ImGui::Begin("Map File Viewer", &showMapFileViewer);

        {
            std::optional<std::string> result;
            {
                std::lock_guard<std::mutex> lock(dialogMutex);
                result.swap(pendingDialogResult);
            }
            if (result)
                openMap(*result);
        }

        bool dialogBusy = isDialogInProgress();
        ImGui::BeginDisabled(dialogBusy);
        if (ImGui::Button(dialogBusy ? "Opening..." : "Open Map File..."))
            openMapFileDialogAsync();
        ImGui::EndDisabled();

        if (loadedMap) {
            ImGui::SameLine();
            if (ImGui::Button("Close Map"))
                closeMap();
        }

        if (!loadError.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", loadError.c_str());

        if (!loadedMap) {
            ImGui::End();
            return;
        }

        ImGui::Text("Loaded: %s", loadedMapPath.c_str());
        ImGui::Separator();

        uint32_t totalTags = loadedMap->getTagCount();
        static int tagsPerPage = 50;
        static int page = 0;
        int numPages = totalTags == 0 ? 1 : (int)((totalTags + tagsPerPage - 1) / tagsPerPage);

        ImGui::InputInt("Page", &page);
        if (ImGui::IsWindowHovered())
            page -= (int)ImGui::GetIO().MouseWheel;
        if (page < 0) page = 0;
        if (page >= numPages) page = numPages - 1;

        ImGui::SameLine();
        ImGui::InputInt("Page size", &tagsPerPage);
        if (tagsPerPage < 1) tagsPerPage = 1;

        ImGui::Text("%u tags, %d pages", totalTags, numPages);
        ImGui::Separator();

        ImGui::BeginChild("##TagList", ImVec2(0, 300), ImGuiChildFlags_Borders);
        uint32_t pageBase = (uint32_t)page * (uint32_t)tagsPerPage;
        for (int i = 0; i < tagsPerPage; i++) {
            uint32_t index = pageBase + (uint32_t)i;
            if (index >= totalTags) break;
            renderTagRow(index);
        }
        ImGui::EndChild();

        renderSelectedTagDetails();

        ImGui::End();
    }

}
