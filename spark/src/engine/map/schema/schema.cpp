#include "schema.hpp"

#include "functions.hpp"
#include "utils/Strings.hpp"

namespace Engine::Map {

    StructureNode* Schema::structureForGroupId(uint32_t groupId) {
        for (auto& [id, structNode] : structures) {
            if (structNode.groupId == groupId) {
                return &structNode;
            }
        }
        return nullptr;
    }

    StructureNode* Schema::createStructureForGroupId(uint32_t groupId) {
        std::string groupIdStr = Strings::fourccToString(groupId);
        Id structureId = createId();
        StructureNode node;
        node.id = structureId;
        snprintf(node.name, sizeof(node.name), "Struct_%s", groupIdStr.c_str());
        node.size = 0;
        node.groupId = (TagGroupId)groupId;
        structures[structureId] = node;
        return &structures[structureId];
    }

    StructureNode* Schema::structureByName(const std::string& name) {
        for (auto& [id, structNode] : structures) {
            if (std::string(structNode.name) == name) {
                return &structNode;
            }
        }
        return nullptr;
    }

    Id Schema::createField(std::string name, FieldNode** nodeOut) {
        Id fieldId = createId();
        FieldNode node = {};
        node.type.type = Type::U32;
        node.id = fieldId;
        if (name.empty()) {
            snprintf(node.name, sizeof(node.name), "Field_%s", fieldId.c_str());
        } else {
            snprintf(node.name, sizeof(node.name), "%s", name.c_str());
        }
        node.offset = 0;
        fields[fieldId] = node;
        if (nodeOut) {
            *nodeOut = &fields[fieldId];
        }
        return fieldId;
    }

}