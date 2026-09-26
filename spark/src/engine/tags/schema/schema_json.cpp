#include "schema.hpp"
#include <json.hpp>
#include <cstring>

namespace Engine::TagSchema {

    static void to_json(nlohmann::json& j, const PrimitiveTypeRef& e) {
        switch (e) {
            #define SCHEMA_FIELD_TYPE(name, size) case PrimitiveTypeRef::name: j = #name; return;
            #include "type_defs.hpp"
            #undef SCHEMA_FIELD_TYPE
        }
        j = nullptr;
    }

    static void from_json(const nlohmann::json& j, PrimitiveTypeRef& e) {
        std::string name = j.get<std::string>();
        #define SCHEMA_FIELD_TYPE(name_, size) if (name == #name_) { e = PrimitiveTypeRef::name_; return; }
        #include "type_defs.hpp"
        #undef SCHEMA_FIELD_TYPE
    }

    // TypeRef::name is a fixed char buffer rather than std::string, so it needs manual (de)serialization.
    static void to_json(nlohmann::json& j, const TypeRef& type) {
        j = nlohmann::json{
            {"primitive", type.primitive},
            {"name", std::string(type.name)}
        };
    }

    static void from_json(const nlohmann::json& j, TypeRef& type) {
        j.at("primitive").get_to(type.primitive);
        std::string name = j.at("name").get<std::string>();
        std::strncpy(type.name, name.c_str(), sizeof(type.name) - 1);
        type.name[sizeof(type.name) - 1] = '\0';
    }

    // Field::name is a fixed char buffer rather than std::string, so it needs manual (de)serialization.
    static void to_json(nlohmann::json& j, const Field& field) {
        j = nlohmann::json{
            {"name", std::string(field.name)},
            {"type", field.type},
            {"offset", field.offset},
            {"bitOffset", field.bitOffset}
        };
    }

    static void from_json(const nlohmann::json& j, Field& field) {
        std::string name = j.at("name").get<std::string>();
        std::strncpy(field.name, name.c_str(), sizeof(field.name) - 1);
        field.name[sizeof(field.name) - 1] = '\0';
        j.at("type").get_to(field.type);
        j.at("offset").get_to(field.offset);
        j.at("bitOffset").get_to(field.bitOffset);
    }

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Structure, name, size, fields)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(EnumerationEntry, name, value)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Enumeration, name, entries)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TagSchema, name, groupId, mainStructure, enumerations, structures)
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TagSchemaCollection, schemas)

    std::string TagSchemaCollection::toJsonString() const {
        nlohmann::json j = *this;
        return j.dump(2);
    }

    TagSchemaCollection TagSchemaCollection::fromJsonString(const std::string& jsonString) {
        return nlohmann::json::parse(jsonString).get<TagSchemaCollection>();
    }

}
