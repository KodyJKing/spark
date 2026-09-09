#include "schema.hpp"
#include <json.hpp>
#include <cstring>

namespace Engine::TagSchema {

    NLOHMANN_JSON_SERIALIZE_ENUM(PrimitiveTypeRef, {
        {PrimitiveTypeRef::Bit, "Bit"},
        {PrimitiveTypeRef::Uint8, "Uint8"},
        {PrimitiveTypeRef::Uint16, "Uint16"},
        {PrimitiveTypeRef::Uint32, "Uint32"},
        {PrimitiveTypeRef::Uint64, "Uint64"},
        {PrimitiveTypeRef::Int, "Int"},
        {PrimitiveTypeRef::Int8, "Int8"},
        {PrimitiveTypeRef::Int16, "Int16"},
        {PrimitiveTypeRef::Int32, "Int32"},
        {PrimitiveTypeRef::Int64, "Int64"},
        {PrimitiveTypeRef::Float, "Float"},
        {PrimitiveTypeRef::Vec3, "Vec3"},
        {PrimitiveTypeRef::Vec4, "Vec4"},
        {PrimitiveTypeRef::Matrix3x3, "Matrix3x3"},
        {PrimitiveTypeRef::TagString, "TagString"},
        {PrimitiveTypeRef::TagReference, "TagReference"},
        {PrimitiveTypeRef::StructureReference, "StructureReference"},
        {PrimitiveTypeRef::Enumeration, "Enumeration"}
    })

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
