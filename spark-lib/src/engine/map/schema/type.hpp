#pragma once

namespace Engine::Map {

    enum class Type {
        #define SCHEMA_FIELD_TYPE(name, size) name,
        #include "type_defs.hpp"
        #undef SCHEMA_FIELD_TYPE
    };

    inline constexpr bool relocationSensitive(Type type) {
        return type == Type::DataPointer || type == Type::BlockPointer ||
            type == Type::Structure || type == Type::TagReference;
    }

    inline const char* TypeNames[] = {
        #define SCHEMA_FIELD_TYPE(name, size) #name,
        #include "type_defs.hpp"
        #undef SCHEMA_FIELD_TYPE
    };

    inline const size_t TypeSizes[] = {
        #define SCHEMA_FIELD_TYPE(name, size) size,
        #include "type_defs.hpp"
        #undef SCHEMA_FIELD_TYPE
    };

    inline constexpr size_t TypeCount = sizeof(TypeNames) / sizeof(TypeNames[0]);

}
