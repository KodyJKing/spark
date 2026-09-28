#pragma once

#include <cstdint>
#include <map>

#include "engine/map/schema/schema_file.hpp"
#include "engine/map/schema/reference.hpp"
#include "engine/map/schema/schema.hpp"

#include "Hints.hpp"

#include "utils/Utils.hpp"

namespace Mod::DevTools::DissectTagNew {

    using namespace Engine::Map;

    struct WindowState {
        bool isOpen = false;
        uint32_t tagId = 0;
        
        // This should *probably* be a smart-pointer.
        MapFile* map = nullptr;

        Hints::HintCache hintCache;

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
        Schema schema;
        inline void loadSchema() {
            static bool schemaLoaded = false;
            if (schemaLoaded) return;
            schema = Engine::Map::loadSchemaFromFile(Utils::getTagSchemaPath());
            schemaLoaded = true;
        }
        inline void saveSchema() {
            auto path = Utils::getTagSchemaPath();
            Engine::Map::saveSchemaToFile(schema, path);
        }

        float hintThreshold = 0.25f;

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
