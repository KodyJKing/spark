#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <thread>

#include "engine/map/schema/schema_file.hpp"
#include "engine/map/schema/reference.hpp"
#include "engine/map/schema/schema.hpp"
#include "engine/map/schema/span_map.hpp"
#include "engine/map/schema/problems.hpp"

#include "Hints.hpp"

#include "utils/Utils.hpp"

namespace SparkLib::UI::DissectTag {

    using namespace Engine::Map;

    // Output of a background minimap analysis. Span types point into `schema`,
    // a snapshot taken when the job started, so the UI thread can keep editing
    // the live schema while the job runs.
    struct MinimapAnalysis {
        std::shared_ptr<Schema> schema;
        std::vector<Analysis::SpanInfo> overlaps;
        std::vector<Analysis::Problem> problems;
        std::vector<uint8_t> stripeParity; // Parallel to overlaps.
    };

    struct MinimapJob {
        std::atomic<bool> cancel = false;
        std::atomic<bool> done = false;
        std::shared_ptr<MinimapAnalysis> result; // Only read once done is set.
    };

    struct WindowState {
        bool isOpen = false;
        uint32_t tagId = 0;
        
        // This should *probably* be a smart-pointer.
        MapFile* map = nullptr;

        Hints::HintCache hintCache;

        // Parse-minimap analysis. Rebuilt on a background thread on a time
        // throttle (and on manual refresh), since fillSpanMap walks the entire
        // block tree. Set minimapRefreshRequested to force a rebuild.
        std::shared_ptr<MinimapAnalysis> minimap;
        std::shared_ptr<MinimapJob> minimapJob; // In-flight job, if any.
        double minimapBuiltTime = -1.0;
        bool minimapRefreshRequested = false;

        // Blocks until any in-flight minimap job has stopped touching map memory.
        inline void waitForMinimapJob() {
            if (!minimapJob) return;
            minimapJob->cancel = true;
            while (!minimapJob->done) std::this_thread::yield();
            minimapJob.reset();
        }

        // Minimap view: zoom >= 1 (1 = whole tag visible), viewStart is the
        // fraction of the tag range at the left edge of the minimap.
        double minimapZoom = 1.0;
        double minimapViewStart = 0.0;

        inline Tag* getTag() const {
            return map->getTag(tagId);
        }

        inline StructureRef getStructureRef(Context& ctx) const {
            auto tag = getTag();
            if (!tag) return StructureRef{nullptr, nullptr};
            auto structureNode = ctx.schema->structureForGroupId(tag->groupID);
            if (!structureNode) {
                // Create new structure
                structureNode = ctx.schema->createStructureForGroupId(tag->groupID);
            }
            void* data = map->getTagData(tag);
            return StructureRef{&ctx, structureNode, data};
        }
    };

    struct RenderContext {
        WindowState* windowState;
        void* tagStart;
        void* tagEnd;
    };

    struct State {

        ///////////////////////////
        // Schema
        Schema* schema;
        inline void loadSchema() {
            static bool schemaLoaded = false;
            if (schemaLoaded) return;
            schema = Engine::Map::getMainSchema();
            schemaLoaded = true;
        }
        inline void saveSchema() {
            Engine::Map::saveMainSchema();
        }

        float hintThreshold = 0.25f;
        Analysis::ProblemOptions problemOptions;

        ///////////////////////////
        // Windows

        std::map<uint32_t, WindowState> windows;
        inline void openWindow(MapFile* map, uint32_t tagId) {
            if (windows.find(tagId) == windows.end()) {
                windows[tagId] = WindowState{true, tagId, map};
            }
        }
        inline void cleanupWindows() {
            for (auto it = windows.begin(); it != windows.end(); ) {
                if (!it->second.isOpen) {
                    // The job reads map memory, so it can't outlive the window
                    // (closeWindowsForMap relies on this before the map is freed).
                    it->second.waitForMinimapJob();
                    it = windows.erase(it);
                } else {
                    ++it;
                }
            }
        }

        inline void tick() {
            loadSchema();
            cleanupWindows();
        }
        
    };

    inline static State state;
    
}
