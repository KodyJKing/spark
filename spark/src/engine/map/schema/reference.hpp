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

        bool allocated() const;

        void createFieldAt(void* address, Type type = Type::U32);
        void insertField(Id id);
    };

    struct FieldRef {
        Context* context;
        FieldNode* node;
        void* address;
        StructureRef parent;

        bool valid() const;
        void deleteField();

        std::string readAsString();
        void writeFromString(const std::string& value);
        size_t size();
        
        bool createStructure(size_t size = 0x100);
        StructureNode* getReferencedType();

        BlockPointer* getBlockPointer();
        StructureRef getBlockElement(size_t index);

        StructureRef getStructure();

        bool is(Type type);
    };

}
