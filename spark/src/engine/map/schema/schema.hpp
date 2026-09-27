#pragma once

#include "engine/tags/tag_group_id.hpp"
#include "type.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <map>


namespace Engine::Map {

    using Id = std::string;
    const Id NullId = "";

    struct TypeRef {
        Type type;
        Id ref;
    };

    struct SchemaNode {
        Id id;
        char name[256];
    };

    struct StructureNode : public SchemaNode {
        size_t size;
        // Pair of field offset and field ID, used to keep fields sorted by offset
        std::map<size_t, Id> fields;
        TagGroupId groupId = GroupId_Invalid;
    };

    struct FieldNode : public SchemaNode {
        TypeRef type;
        size_t offset;
    };

    struct Schema {
        std::map<Id, StructureNode> structures;
        std::map<Id, FieldNode> fields;

        StructureNode* structureForGroupId(uint32_t groupId);
        StructureNode* createStructureForGroupId(uint32_t groupId);

        Id createField(std::string name, FieldNode** nodeOut = nullptr);
    };

}
