#pragma once

#include <map>
#include <vector>
#include <string>

namespace Engine::TagSchema {

    struct Context {
        int64_t relocationOffset;
        uintptr_t structureBase;
    };

    enum class PrimitiveTypeRef {
        Bit,
        Uint8, Uint16, Uint32, Uint64,
        Int, Int8, Int16, Int32, Int64,
        Float,
        TagString,
        TagReference,
        StructureReference,
        Enumeration
    };

    static const char* PrimitiveTypeRefNames[] = {
        "Bit",
        "Uint8", "Uint16", "Uint32", "Uint64",
        "Int", "Int8", "Int16", "Int32", "Int64",
        "Float",
        "TagString",
        "TagReference",
        "StructureReference",
        "Enumeration"
    };

    static const size_t PrimitiveTypeRefSizes[] = {
        1, // Bit
        1, 2, 4, 8, // Uint8, Uint16, Uint32, Uint64
        4, 1, 2, 4, 8, // Int, Int8, Int16, Int32, Int64
        4, // Float
        32, // TagString
        4, // TagReference
        12, // StructureReference
        2 // Enumeration
    };

    /**
     * A reference to a type within the tag schema.
     */
    struct TypeRef {
        PrimitiveTypeRef primitive;
        // std::string name;
        char name[256];
    };

    struct Field {
        // std::string name;
        char name[256];
        TypeRef type;
        size_t offset;
        // Only relevant for bit fields.
        uint8_t bitOffset;
        
        std::string readString(const Context& context);
    };

    struct Structure {
        std::string name;
        size_t size;
        std::vector<Field> fields;

        void deleteField(Field* field);
    };

    struct EnumerationEntry {
        std::string name;
        int value;
    };

    struct Enumeration {
        std::string name;
        std::vector<EnumerationEntry> entries;
    };

    struct TagSchema {
        std::string name;
        std::string groupId;
        Structure mainStructure;

        std::map<std::string, Enumeration> enumerations;
        std::map<std::string, Structure> structures;
    };

    struct TagSchemaCollection {
        // Maps a group ID to its corresponding tag schema.
        std::map<std::string, TagSchema> schemas;

        std::string toJsonString() const;
        static TagSchemaCollection fromJsonString(const std::string& jsonString);
    };
}
