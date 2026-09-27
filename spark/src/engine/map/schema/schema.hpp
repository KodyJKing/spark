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
        Id id = NullId;
        char name[256] = "";

        // NullId is reserved for read-only placeholder nodes.
        inline bool readonly() const { return id == NullId; }

        bool assertWritable(const char* errorMsg);
    };

    struct StructureNode : public SchemaNode {
        size_t size = 0;
        // Pair of field offset and field ID, used to keep fields sorted by offset
        std::map<size_t, Id> fields;
        TagGroupId groupId = GroupId_Invalid;
    };

    static inline StructureNode NullStructure = StructureNode{};

    struct FieldNode : public SchemaNode {
        TypeRef type = { Type::U32, NullId };
        size_t offset = 0;
    };

    struct Schema {
        std::map<Id, StructureNode> structures;
        std::map<Id, FieldNode> fields;

        StructureNode* structureForGroupId(uint32_t groupId);
        StructureNode* createStructureForGroupId(uint32_t groupId);

        StructureNode* structureByName(const std::string& name);

        Id createField(std::string name, FieldNode** nodeOut = nullptr, Type type = Type::U32);
        Id createStructure(std::string name, StructureNode **nodeOut);
    };
}
