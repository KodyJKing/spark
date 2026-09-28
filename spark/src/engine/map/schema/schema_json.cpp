#include "schema.hpp"

#include <json.hpp>
#include <cstring>

namespace Engine::Map {

    static void to_json(nlohmann::json& j, const Type& t) {
        switch (t) {
            #define SCHEMA_FIELD_TYPE(name, size) case Type::name: j = #name; return;
            #include "type_defs.hpp"
            #undef SCHEMA_FIELD_TYPE
        }
        j = nullptr;
    }

    static void from_json(const nlohmann::json& j, Type& t) {
        std::string name = j.get<std::string>();
        #define SCHEMA_FIELD_TYPE(name_, size) if (name == #name_) { t = Type::name_; return; }
        #include "type_defs.hpp"
        #undef SCHEMA_FIELD_TYPE
    }

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TypeRef, type, ref)

    // SchemaNode::name is a fixed char buffer rather than std::string, so it needs manual (de)serialization.
    static void to_json(nlohmann::json& j, const StructureNode& node) {
        j = nlohmann::json{
            {"id", node.id},
            {"name", std::string(node.name)},
            {"size", node.size},
            {"fields", node.fields},
            {"groupId", (uint32_t)node.groupId}
        };
    }

    static void from_json(const nlohmann::json& j, StructureNode& node) {
        j.at("id").get_to(node.id);
        std::string name = j.at("name").get<std::string>();
        std::strncpy(node.name, name.c_str(), sizeof(node.name) - 1);
        node.name[sizeof(node.name) - 1] = '\0';
        j.at("size").get_to(node.size);
        j.at("fields").get_to(node.fields);
        node.groupId = (TagGroupId)j.at("groupId").get<uint32_t>();
    }

    static void to_json(nlohmann::json& j, const FieldNode& node) {
        j = nlohmann::json{
            {"id", node.id},
            {"name", std::string(node.name)},
            {"type", node.type},
            {"offset", node.offset}
        };
    }

    static void from_json(const nlohmann::json& j, FieldNode& node) {
        j.at("id").get_to(node.id);
        std::string name = j.at("name").get<std::string>();
        std::strncpy(node.name, name.c_str(), sizeof(node.name) - 1);
        node.name[sizeof(node.name) - 1] = '\0';
        j.at("type").get_to(node.type);
        j.at("offset").get_to(node.offset);
    }

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Schema, structures, fields)

    std::string Schema::toJsonString() const {
        nlohmann::json j = *this;
        return j.dump();
    }

    Schema Schema::fromJsonString(const std::string& json) {
        return nlohmann::json::parse(json).get<Schema>();
    }

}
