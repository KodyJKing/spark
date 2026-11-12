#pragma once

#include "inject_tags.hpp"
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>

namespace Engine::Map::Spark {
    bool injectionReadOpen(uint32_t flags);
    bool excludedInjectionMap(const std::filesystem::path& path);

    struct InjectionPackage {
        std::filesystem::path cache;
        std::vector<InjectionTag> tags;
    };

    InjectionPackage parseInjectionPackage(const std::string& json,
        const std::filesystem::path& manifest, const std::string& mapName);

    struct PreparedInjection {
        std::filesystem::path path;
        std::vector<std::string> warnings;
        std::string outputHash;
    };

    class ConfiguredInjection {
    public:
        ConfiguredInjection(std::filesystem::path mapsDirectory, std::filesystem::path tagsDirectory,
            std::filesystem::path schemaPath);
        std::optional<PreparedInjection> prepare(const std::filesystem::path& host, uint32_t flags);

    private:
        std::filesystem::path mapsDirectory_, tagsDirectory_, schemaPath_;
        std::map<std::string, PreparedInjection> cached_;
        std::mutex mutex_;
    };
}
