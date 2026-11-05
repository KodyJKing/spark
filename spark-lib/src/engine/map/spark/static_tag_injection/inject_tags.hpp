#pragma once

#include <vector>
#include <span>
#include "engine/map/schema/schema.hpp"
#include "engine/map/tag_data_types.hpp"

namespace Engine::Map::Spark {

    struct PortableTagId {
        uint32_t groupId;
        const char* path;
    };

    using TagList = std::vector<PortableTagId>;

    struct InjectionTag {
        uint32_t groupId;
        std::string path;
    };

    // Validates the raw map before exposing owned tag identities to a picker.
    std::vector<InjectionTag> listInjectionTags(std::span<const uint8_t> bytes);

    // Throws std::runtime_error for invalid or unsupported maps/schemas.
    // Inputs are immutable; the returned buffer owns the complete serialized map.
    // Warnings are appended on success, or logged when no collector is supplied.
    std::vector<uint8_t> injectTags(
        std::span<const uint8_t> hostBytes,
        std::span<const uint8_t> guestBytes,
        const TagList& guestTags,
        const Schema& schema,
        std::vector<std::string>* warnings = nullptr
    );

}
