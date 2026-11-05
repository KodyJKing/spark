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

    // Throws std::runtime_error for invalid or unsupported maps/schemas.
    // Inputs are immutable; the returned buffer owns the complete serialized map.
    std::vector<uint8_t> injectTags(
        std::span<const uint8_t> hostBytes,
        std::span<const uint8_t> guestBytes,
        const TagList& guestTags,
        const Schema& schema
    );

}
