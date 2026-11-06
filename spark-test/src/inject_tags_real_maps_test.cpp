#include <catch_amalgamated.hpp>
#include <fstream>

#include "InjectionFiles.hpp"
#include "engine/map/bitmap.hpp"
#include "engine/map/spark/static_tag_injection/inject_tags.hpp"

using namespace Engine;
using namespace Engine::Map;
using namespace Engine::Map::Spark;

namespace {
    std::filesystem::path backupMaps() {
        auto length = GetEnvironmentVariableW(L"MCC_HOME", nullptr, 0);
        REQUIRE(length > 0);
        std::wstring home(length, L'\0');
        auto written = GetEnvironmentVariableW(L"MCC_HOME", home.data(), length);
        REQUIRE(written > 0);
        REQUIRE(written < length);
        home.resize(written);
        auto directory = std::filesystem::path(home) / "halo1" / "maps_backup";
        REQUIRE(std::filesystem::is_directory(directory));
        return directory;
    }

    Schema injectionSchema() {
        auto path = std::filesystem::path(SPARK_TEST_DIR).parent_path() / "spark" / "shipfiles" / "tags.schema2.json";
        std::ifstream file(path);
        REQUIRE(file.is_open());
        return Schema::fromJsonString(std::string(std::istreambuf_iterator<char>(file), {}));
    }

    uint32_t word(std::span<const uint8_t> bytes, size_t offset) {
        REQUIRE(offset <= bytes.size());
        REQUIRE(sizeof(uint32_t) <= bytes.size() - offset);
        uint32_t value;
        std::memcpy(&value, bytes.data() + offset, sizeof(value));
        return value;
    }
}

TEST_CASE("Backup maps expose valid scenario handles and guest tag lists", "[.real_maps]") {
    std::vector<std::filesystem::path> maps;
    for (const auto& entry : std::filesystem::directory_iterator(backupMaps()))
        if (entry.is_regular_file() && entry.path().extension() == ".map" && entry.path().filename() != "bitmaps.map")
            maps.push_back(entry.path());
    std::sort(maps.begin(), maps.end());
    REQUIRE_FALSE(maps.empty());
    for (const auto& path : maps) {
        INFO(path.string());
        auto bytes = Spool::readInjectionMap(path);
        const auto offset = word(bytes, 0x10);
        TagDataHeader header;
        REQUIRE(size_t(offset) + sizeof(header) <= bytes.size());
        std::memcpy(&header, bytes.data() + offset, sizeof(header));
        REQUIRE(header.scenarioId == word(bytes, size_t(offset) + 4));
        REQUIRE(header.checksum == word(bytes, size_t(offset) + 8));
        auto tags = listInjectionTags(bytes);
        REQUIRE(tags.size() == header.tagCount);
        REQUIRE_FALSE(tags.empty());
        REQUIRE(tags.front().groupId == GroupId_Scenario);
    }
}

TEST_CASE("Inject the d40 shotgun model into b30 using backup maps", "[.real_maps]") {
    const auto directory = backupMaps();
    auto host = Spool::readInjectionMap(directory / "b30.map");
    auto guest = Spool::readInjectionMap(directory / "d40.map");
    auto schema = injectionSchema();
    const std::string modelPath = R"(weapons\shotgun\shotgun)";
    auto guestTags = listInjectionTags(guest);
    auto hostTags = listInjectionTags(host);
    auto isModel = [&](const InjectionTag& tag) {
        return tag.groupId == GroupId_Model && tag.path == modelPath;
    };
    REQUIRE(std::count_if(guestTags.begin(), guestTags.end(), isModel) == 1);
    REQUIRE(std::none_of(hostTags.begin(), hostTags.end(), isModel));

    // The output exists only in memory. No existing map or output file is written.
    std::vector<uint8_t> output;
    try {
        output = injectTags(host, guest, {{GroupId_Model, modelPath.c_str()}}, schema);
    } catch (const std::runtime_error&) {
        REQUIRE(Spool::readInjectionMap(directory / "b30.map") == host);
        REQUIRE(Spool::readInjectionMap(directory / "d40.map") == guest);
        throw;
    }
    auto outputTags = listInjectionTags(output);
    REQUIRE(std::count_if(outputTags.begin(), outputTags.end(), isModel) == 1);
    REQUIRE(outputTags.size() > hostTags.size());
    REQUIRE(std::equal(hostTags.begin(), hostTags.end(), outputTags.begin(),
        [](const auto& a, const auto& b) { return a.groupId == b.groupId && a.path == b.path; }));
    REQUIRE(Spool::readInjectionMap(directory / "b30.map") == host);
    REQUIRE(Spool::readInjectionMap(directory / "d40.map") == guest);
}

TEST_CASE("Preserve the real b30 external sky bitmap", "[.real_maps][external_bitmap]") {
    const auto path = backupMaps() / "b30.map";
    const auto original = Spool::readInjectionMap(path);
    auto host = original;
    auto tags = listInjectionTags(host);
    auto found = std::find_if(tags.begin(), tags.end(), [](const InjectionTag& tag) {
        return tag.groupId == GroupId_Bitmap && tag.path == R"(sky\planets\threshold clear afternoon)";
    });
    REQUIRE(found != tags.end());
    const auto tagData = word(host, 0x10);
    const auto array = size_t(tagData) + word(host, tagData) - 0x50000000;
    auto index = static_cast<size_t>(found - tags.begin());
    Tag selected;
    REQUIRE(array + (index + 1) * sizeof(Tag) <= host.size());
    std::memcpy(&selected, host.data() + array + index * sizeof(Tag), sizeof(selected));
    const auto root = size_t(tagData) + selected.data.offset - 0x50000000;
    const auto count = word(host, root + 0x60);
    REQUIRE(count > 0);
    const auto records = size_t(tagData) + word(host, root + 0x64) - 0x50000000;
    REQUIRE(records + size_t(count) * sizeof(BitmapData) <= host.size());
    for (uint32_t i = 0; i < count; ++i) {
        BitmapData bitmap;
        std::memcpy(&bitmap, host.data() + records + i * sizeof(bitmap), sizeof(bitmap));
        REQUIRE((bitmap.flags & (1u << 8)) != 0);
        REQUIRE(bitmap.data.offset > host.size());
    }

    // Isolate this real bitmap in an in-memory table; unrelated host schemas
    // must not hide regressions in external-cache preservation.
    selected.tagHandle &= 0xFFFF0000;
    std::memcpy(host.data() + array, &selected, sizeof(selected));
    const uint32_t noScenario = 0xFFFFFFFF, oneTag = 1;
    std::memcpy(host.data() + tagData + 4, &noScenario, sizeof(noScenario));
    std::memcpy(host.data() + tagData + 12, &oneTag, sizeof(oneTag));
    auto output = injectTags(host, host, {}, injectionSchema());
    auto outputTags = listInjectionTags(output);
    REQUIRE(outputTags.size() == 1);
    REQUIRE(outputTags[0].path == found->path);
    const auto outputData = word(output, 0x10);
    const auto outputArray = size_t(outputData) + word(output, outputData) - 0x50000000;
    const auto outputRoot = size_t(outputData) + word(output, outputArray + 20) - 0x50000000;
    REQUIRE(word(output, outputRoot + 0x60) == count);
    const auto outputRecords = size_t(outputData) + word(output, outputRoot + 0x64) - 0x50000000;
    REQUIRE(outputRecords + size_t(count) * sizeof(BitmapData) <= output.size());
    REQUIRE(std::memcmp(output.data() + outputRecords, original.data() + records,
        size_t(count) * sizeof(BitmapData)) == 0);
    REQUIRE(Spool::readInjectionMap(path) == original);
}
