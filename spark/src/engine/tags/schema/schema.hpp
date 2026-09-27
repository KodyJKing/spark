#pragma once

#include <map>
#include <vector>
#include <string>

namespace Engine::TagSchema {

    struct Context {
        uint64_t relocationOffset;
        uintptr_t structureBase;
    };

    enum class PrimitiveTypeRef {
        #define SCHEMA_FIELD_TYPE(name, size) name,
        #include "type_defs.hpp"
        #undef SCHEMA_FIELD_TYPE
    };
    
    static const char* PrimitiveTypeRefNames[] = {
        #define SCHEMA_FIELD_TYPE(name, size) #name,
        #include "type_defs.hpp"
        #undef SCHEMA_FIELD_TYPE
    };

    static const size_t PrimitiveTypeRefSizes[] = {
        #define SCHEMA_FIELD_TYPE(name, size) size,
        #include "type_defs.hpp"
        #undef SCHEMA_FIELD_TYPE
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
        void writeString(const Context& context, const std::string& value);
    };

    struct TagSchema;
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

        /*
         * Renames a structure, but does not update any references to it within other structures.
         */
        void renameStructure(const std::string& oldName, const std::string& newName);
    };

    struct TagSchemaCollection {
        // Maps a group ID to its corresponding tag schema.
        std::map<std::string, TagSchema> schemas;

        std::string toJsonString() const;
        static TagSchemaCollection fromJsonString(const std::string& jsonString);
    };
}
