#include <catch_amalgamated.hpp>
#include <filesystem>

#include "engine/map/spark/static_tag_injection/inject_tags.hpp"

using namespace Engine::Map;
using namespace Engine::Map::Spark;

namespace {

    std::filesystem::path getMCCHome() {
        const char* mccHome = std::getenv("MCC_HOME");
        return mccHome
            ? std::filesystem::path(mccHome)
            : R"(C:\Program Files (x86)\Steam\steamapps\common\Halo The Master Chief Collection)";
    }

    std::filesystem::path getMapsDirectory() {
        std::filesystem::path mccPath = getMCCHome();
        return mccPath / "halo1" / "maps";
    }

    std::filesystem::path getBackupDirectory() {
        std::filesystem::path mccPath = getMCCHome();
        return mccPath / "halo1" / "maps_backup";
    }

    std::filesystem::path getOutputDirectory() {
        auto dir = std::filesystem::path(SPARK_TEST_DIR) / "out";
        std::filesystem::create_directories(dir);
        return dir;
    }

    std::vector<uint8_t> readFile(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        REQUIRE(file);
        auto size = static_cast<size_t>(file.tellg());
        file.seekg(0);
        std::vector<uint8_t> buffer(size);
        REQUIRE(file.read(reinterpret_cast<char*>(buffer.data()), size));
        return buffer;
    }

    void writeFile(const std::filesystem::path& path, const void* data, size_t size) {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        REQUIRE(file);
        REQUIRE(file.write(reinterpret_cast<const char*>(data), size));
    }

}

TEST_CASE("Inject nothing", "[inject_tags]") {
    auto mapsBackupDir = getBackupDirectory();
    auto baseBytes = readFile(mapsBackupDir / "b30.map");
    auto guestBytes = readFile(mapsBackupDir / "a30.map");

    RawMapFile baseMap(reinterpret_cast<CacheHeader*>(baseBytes.data()));
    RawMapFile guestMap(reinterpret_cast<CacheHeader*>(guestBytes.data()));
    TagList guestTags;
    RawMapFile hostMap;

    injectTags(baseMap, guestMap, guestTags, hostMap);

    REQUIRE(hostMap.cache != nullptr);
    // writeFile(getOutputDirectory() / "inject_nothing.map", hostMap.cache, hostMap.cache->fileSize);

    auto mapsDir = getMapsDirectory();
    writeFile(mapsDir / "b30.map", hostMap.cache, hostMap.cache->fileSize);

    free(hostMap.cache);
}
