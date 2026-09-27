#pragma once

#include "engine/map/map_file.hpp"
#include "schema.hpp"

#include <vector>
#include <string>

namespace Engine::Map {

    struct Context {
        MapFile* mapFile;
        Schema* schema;
    };

    struct FieldRef;

    struct StructureRef {
        Context* context;
        StructureNode* node;
        void* address;

        FieldRef getFieldRef(const char* name);
        std::vector<FieldRef> getFieldRefs();

        bool valid() const;

        void createFieldAt(void* address);
        void insertField(Id id);
    };

    struct FieldRef {
        Context* context;
        FieldNode* node;
        void* address;
        StructureRef parent;

        std::string readAsString();
        void writeFromString(const std::string& value);

        bool valid() const;

        size_t size() const;

        void deleteField();
    };

}
