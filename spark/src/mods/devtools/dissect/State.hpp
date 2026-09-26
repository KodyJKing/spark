#pragma once

#include "engine/tags/schema/schema.hpp"
#include "engine/halo1.hpp"
#include <fstream>
#include <sstream>

#include "Types.hpp"

namespace Mod::DevTools::DissectTag {

    using namespace Engine::TagSchema;

    static const char* SCHEMA_FILE_PATH = "./tag_schema.json";

    struct State {

        bool initialized = false;
        TagSchemaCollection tagSchemas;
        std::map<uint32_t, WindowState> windowStates;

        void saveSchemas() {
            std::ofstream file(SCHEMA_FILE_PATH);
            if (!file.is_open())
                return;
            file << tagSchemas.toJsonString();
        }
        bool loadSchemas() {
            std::ifstream file(SCHEMA_FILE_PATH);
            if (!file.is_open())
                return false;
            std::stringstream buffer;
            buffer << file.rdbuf();
            tagSchemas = TagSchemaCollection::fromJsonString(buffer.str());
            return true;
        }

        void initialize() {
            if (initialized)
                return;
            initialized = true;
            loadSchemas();
        }

        void openWindow(uint32_t tagId) {
            if (windowStates.find(tagId) == windowStates.end()) {
                windowStates[tagId] = WindowState{tagId};
            }
        }

        void clearClosedWindows() {
            for (auto it = windowStates.begin(); it != windowStates.end(); ) {
                if (!it->second.open) {
                    it = windowStates.erase(it);
                } else {
                    ++it;
                }
            }
        }
            
    };

    inline static State state;
    
}
