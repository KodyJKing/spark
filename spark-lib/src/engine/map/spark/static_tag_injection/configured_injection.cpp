#include "configured_injection.hpp"
#include "injection_files.hpp"
#include <Windows.h>
#include <bcrypt.h>
#include <json.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <set>
#include <stdexcept>

namespace Engine::Map::Spark {
    namespace {
        using Json = nlohmann::json;

        std::string lower(std::string value) {
            for (auto& c : value)
                if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
            return value;
        }

        bool samePath(const std::filesystem::path& a, const std::filesystem::path& b) {
            return _wcsicmp(a.c_str(), b.c_str()) == 0;
        }

        std::string hash(std::span<const uint8_t> bytes) {
            if (bytes.size() > (std::numeric_limits<ULONG>::max)())
                throw std::runtime_error("Injection hash input exceeds 32-bit size");
            std::array<uint8_t, 32> digest{};
            auto status = BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0,
                const_cast<PUCHAR>(bytes.data()), static_cast<ULONG>(bytes.size()),
                digest.data(), static_cast<ULONG>(digest.size()));
            if (status < 0)
                throw std::runtime_error("Cannot hash injection input: NTSTATUS " + std::to_string(status));
            constexpr char hex[] = "0123456789abcdef";
            std::string result;
            for (auto b : digest) {
                result += hex[b >> 4];
                result += hex[b & 15];
            }
            return result;
        }

        std::string text(const std::vector<uint8_t>& bytes) {
            return std::string(bytes.begin(), bytes.end());
        }

        std::filesystem::path localCache(const std::filesystem::path& manifest, const std::string& name) {
            const std::filesystem::path relative(name);
            if (name.empty() || relative.is_absolute() || relative.has_root_path() ||
                lower(relative.extension().string()) != ".map")
                throw std::runtime_error("Manifest cache must be a relative .map path: " + manifest.string());
            for (const auto& part : relative)
                if (part == "..")
                    throw std::runtime_error("Manifest cache cannot escape its directory: " + manifest.string());
            return manifest.parent_path() / relative;
        }

        void validateLevel(std::span<const uint8_t> bytes) {
            if (bytes.size() < sizeof(CacheHeader)) throw std::runtime_error("Truncated level cache header");
            CacheHeader cache;
            std::memcpy(&cache, bytes.data(), sizeof(cache));
            if (cache.scenarioType > 1) throw std::runtime_error("Only campaign/multiplayer maps may be injected");
            auto tags = listInjectionTags(bytes);
            if (tags.empty() || tags.front().groupId != GroupId_Scenario)
                throw std::runtime_error("Injection host has no scenario tag");
            TagDataHeader header;
            std::memcpy(&header, bytes.data() + cache.tagData.offset, sizeof(header));
            if (header.scenarioId == 0xFFFFFFFF)
                throw std::runtime_error("Injection host has no scenario handle");
        }
    }

    bool injectionReadOpen(uint32_t flags) {
        return (flags & 1) && !(flags & (2 | 8 | 16));
    }

    bool excludedInjectionMap(const std::filesystem::path& path) {
        const auto name = lower(path.filename().string());
        return name == "bitmaps.map" || name == "sounds.map" || name == "loc.map" ||
            name == "ui.map" || name == "mainmenu.map" || name == "shared.map" ||
            name == "single_player_shared.map" || name == "campaign.map";
    }

    InjectionPackage parseInjectionPackage(const std::string& json,
        const std::filesystem::path& manifest, const std::string& mapName) {
        const auto document = Json::parse(json);
        if (!document.is_object() || document.size() != 2 || !document.contains("cache") ||
            !document["cache"].is_string() || !document.contains("maps") || !document["maps"].is_object())
            throw std::runtime_error("Manifest requires cache (string) and maps (object): " + manifest.string());
        InjectionPackage result{localCache(manifest, document["cache"].get<std::string>()), {}};
        std::set<std::pair<uint32_t, std::string>> selected;
        for (auto maps = document["maps"].begin(); maps != document["maps"].end(); ++maps) {
            const auto& selector = maps.key();
            if (selector != "*" && (selector.empty() ||
                selector.find_first_of("\\/:.") != std::string::npos))
                throw std::runtime_error("Map selectors must be map stems or *: " + selector);
            if (!maps.value().is_object()) throw std::runtime_error("Map selection must contain tag groups");
            for (auto groups = maps.value().begin(); groups != maps.value().end(); ++groups) {
                if (groups.key().size() != 4 || !groups.value().is_array())
                    throw std::runtime_error("Tag selections require four-character groups and arrays");
                uint32_t group = 0;
                for (auto c : groups.key()) {
                    if (c < 0x20 || c > 0x7E) throw std::runtime_error("Invalid tag group");
                    group = (group << 8) | static_cast<uint8_t>(c);
                }
                for (const auto& path : groups.value()) {
                    if (!path.is_string()) throw std::runtime_error("Tag paths must be strings");
                    auto value = path.get<std::string>();
                    if (value.empty() || value.find('\0') != std::string::npos)
                        throw std::runtime_error("Empty or embedded-null tag path");
                    if (selector == "*" || lower(selector) == lower(mapName))
                        selected.emplace(group, std::move(value));
                }
            }
        }
        for (const auto& [group, path] : selected) result.tags.push_back({group, path});
        return result;
    }

    ConfiguredInjection::ConfiguredInjection(std::filesystem::path mapsDirectory,
        std::filesystem::path tagsDirectory, std::filesystem::path schemaPath)
        : mapsDirectory_(std::filesystem::absolute(mapsDirectory)),
          tagsDirectory_(std::filesystem::absolute(tagsDirectory)), schemaPath_(std::move(schemaPath)) {}

    std::optional<PreparedInjection> ConfiguredInjection::prepare(const std::filesystem::path& host, uint32_t flags) {
        if (!injectionReadOpen(flags) || excludedInjectionMap(host) || lower(host.extension().string()) != ".map")
            return {};
        auto absolute = std::filesystem::absolute(host).lexically_normal();
        if (!samePath(absolute.parent_path(), mapsDirectory_.lexically_normal())) return {};
        std::lock_guard lock(mutex_);
        if (!std::filesystem::exists(tagsDirectory_)) return {};
        std::vector<std::filesystem::path> manifests;
        for (const auto& entry : std::filesystem::directory_iterator(tagsDirectory_))
            if (entry.is_regular_file() && lower(entry.path().extension().string()) == ".json")
                manifests.push_back(entry.path());
        std::sort(manifests.begin(), manifests.end());
        std::vector<InjectionPackage> packages;
        std::string key = "spark-injection-v1\n" + lower(absolute.stem().string()) + "\n";
        for (const auto& manifest : manifests) {
            auto bytes = readInjectionMap(manifest);
            auto package = parseInjectionPackage(text(bytes), manifest, absolute.stem().string());
            key += hash(bytes);
            if (!package.tags.empty()) packages.push_back(std::move(package));
        }
        if (packages.empty()) return {};
        if (!samePath(std::filesystem::canonical(absolute).parent_path(), std::filesystem::canonical(mapsDirectory_)))
            throw std::runtime_error("Injection host resolves outside the maps directory");
        auto source = readInjectionMap(absolute);
        validateLevel(source);
        const auto schemaBytes = readInjectionMap(schemaPath_);
        key += hash(source);
        key += hash(schemaBytes);
        std::vector<std::vector<uint8_t>> guests;
        for (const auto& package : packages) {
            auto resolved = std::filesystem::canonical(package.cache);
            auto directory = std::filesystem::canonical(tagsDirectory_);
            auto relative = resolved.lexically_relative(directory);
            if (relative.empty() || *relative.begin() == "..")
                throw std::runtime_error("Guest cache resolves outside the tags directory");
            guests.push_back(readInjectionMap(resolved));
            key += hash(guests.back());
        }
        const auto keyBytes = std::span(reinterpret_cast<const uint8_t*>(key.data()), key.size());
        const auto inputHash = hash(keyBytes);
        if (auto existing = cached_.find(inputHash); existing != cached_.end()) {
            if (std::filesystem::exists(existing->second.path) &&
                hash(readInjectionMap(existing->second.path)) == existing->second.outputHash)
                return existing->second;
            cached_.erase(existing);
        }
        const auto schema = Schema::fromJsonString(text(schemaBytes));
        PreparedInjection prepared;
        auto output = source;
        for (size_t i = 0; i < packages.size(); ++i) {
            TagList selected;
            for (const auto& tag : packages[i].tags) selected.push_back({tag.groupId, tag.path.c_str()});
            output = injectTags(output, guests[i], selected, schema, &prepared.warnings);
        }
        prepared.outputHash = hash(output);
        const auto directory = mapsDirectory_ / "spark_cache";
        std::filesystem::create_directories(directory);
        if (!samePath(std::filesystem::canonical(directory).parent_path(), std::filesystem::canonical(mapsDirectory_)))
            throw std::runtime_error("spark_cache resolves outside the maps directory");
        prepared.path = directory / (absolute.stem().string() + "-" + inputHash + "-" + prepared.outputHash + ".map");
        if (!std::filesystem::exists(prepared.path) || hash(readInjectionMap(prepared.path)) != prepared.outputHash)
            saveInjectedMap(prepared.path, output, absolute, packages.front().cache);
        cached_.emplace(inputHash, prepared);
        return prepared;
    }
}
