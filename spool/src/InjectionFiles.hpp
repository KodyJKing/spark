#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace Spool {
    std::vector<uint8_t> readInjectionMap(const std::filesystem::path& path);

    // Publishes a fully written temporary file, never either input (including aliases).
    void saveInjectedMap(const std::filesystem::path& destination, std::span<const uint8_t> bytes,
        const std::filesystem::path& host, const std::filesystem::path& guest);
}
