#include <catch_amalgamated.hpp>

#include "engine/map/schema/audit.hpp"
#include "engine/map/schema/hints.hpp"
#include "engine/map/spark/managed_map_file.hpp"

#include <chrono>
#include <cstring>
#include <fstream>
#include <limits>

using namespace Engine;
using namespace Engine::Map;
using namespace Engine::Map::Analysis;

namespace {
    struct TestMap : RawMapFile {
        std::vector<uint8_t> bytes;
        uint32_t cursor = 0x1000;

        explicit TestMap(size_t size = 0x2000) : bytes(size) {
            cache = reinterpret_cast<CacheHeader*>(bytes.data());
            cache->fileSize = static_cast<uint32_t>(size);
            cache->tagData.offset = 0x800;
            cache->tagDataSize = static_cast<uint32_t>(size - 0x800);
            getTagDataHeader()->tagArray.offset = pointer(0x840);
        }

        uint32_t pointer(uint32_t offset) const { return 0x50000000 + offset - 0x800; }

        uint32_t addTag(uint32_t group, const char* name, size_t size) {
            auto index = getTagCount();
            REQUIRE(index < 16);
            auto at = cursor;
            cursor += static_cast<uint32_t>(size);
            REQUIRE(cursor <= bytes.size());
            auto& tag = reinterpret_cast<Tag*>(bytes.data() + 0x840)[index];
            tag.groupID = group;
            tag.tagHandle = 0x12340000 | index;
            tag.path.offset = pointer(0xC00 + index * 32);
            snprintf(reinterpret_cast<char*>(bytes.data() + 0xC00 + index * 32), 32, "%s", name);
            tag.data.offset = pointer(at);
            ++getTagDataHeader()->tagCount;
            return at;
        }

        void word(size_t offset, uint32_t value) {
            REQUIRE(offset + sizeof(value) <= bytes.size());
            std::memcpy(bytes.data() + offset, &value, sizeof(value));
        }

        void trim() {
            // Keep allocation stable; the header bounds the map's logical data.
            cache->fileSize = cursor;
            cache->tagDataSize = cursor - cache->tagData.offset;
        }
    };

    void field(Schema& schema, StructureNode& node, size_t offset, Type type) {
        auto id = node.id + ":" + std::to_string(offset);
        auto& entry = schema.fields[id];
        entry.id = id;
        entry.offset = offset;
        entry.type.type = type;
        node.fields[offset] = id;
    }

    Schema scalarSchema() {
        Schema schema;
        auto& root = schema.structures["root"];
        root.id = "root";
        root.groupId = GroupId_Weapon;
        root.size = 4;
        field(schema, root, 0, Type::U32);
        return schema;
    }

    AuditResults wait(AuditJob& job) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (std::chrono::steady_clock::now() < deadline) {
            auto result = job.snapshot();
            if (result.status != AuditStatus::Running) return result;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        job.cancel();
        FAIL("Audit did not finish within 10 seconds");
        return {};
    }
}

TEST_CASE("Audit bins incremental counts and caps stored details globally", "[audit]") {
    AuditResults results;
    results.totalTags = 3;
    AuditTagResult first{1, GroupId_Weapon, "first"};
    first.problemCount = auditProblemLimit + 10;
    first.problems.resize(first.problemCount, {ProblemKind::Gap, 4, 8});
    results.addTag(std::move(first));
    REQUIRE(results.completedTags == 1);
    REQUIRE(results.storedProblemCount == 0x1000);
    REQUIRE(results.problemCount == 0x100A);

    AuditTagResult second{2, GroupId_Effect, "second"};
    second.problemCount = 20;
    second.problems.resize(20, {ProblemKind::Overlap, 4, 8});
    results.addTag(std::move(second));
    REQUIRE(results.tags[1].problems.empty());
    REQUIRE(results.groups.at(GroupId_Effect).problemCount == 20);
    REQUIRE(results.groups.at(GroupId_Effect).storedProblemCount == 0);
    REQUIRE(results.groups.at(GroupId_Effect).tags == std::vector<size_t>{1});
    REQUIRE(results.storedProblemCount == 0x1000);
    REQUIRE(results.problemCount == 0x101E);

    AuditTagResult error{3, GroupId_Weapon, "missing"};
    error.error = "No mapped schema";
    results.addTag(std::move(error));
    REQUIRE(results.completedTags == 3);
    REQUIRE(results.errorCount == 1);
    REQUIRE(results.groups.at(GroupId_Weapon).errorCount == 1);
    REQUIRE(results.groups.at(GroupId_Weapon).problemCount == 0x100A);
}

TEST_CASE("Audit scans all tags including the last and reports missing schemas", "[audit]") {
    auto map = std::make_shared<TestMap>();
    map->addTag(GroupId_Weapon, "first", 16);
    map->addTag(GroupId_Effect, "missing", 16);
    map->addTag(GroupId_Weapon, "last", 16);
    map->trim();
    AuditJob job(map, scalarSchema());
    auto results = wait(job);
    REQUIRE(results.status == AuditStatus::Completed);
    REQUIRE(results.totalTags == 3);
    REQUIRE(results.completedTags == 3);
    REQUIRE(results.problemCount == 2);
    REQUIRE(results.storedProblemCount == 2);
    REQUIRE(results.errorCount == 1);
    REQUIRE(results.tags[0].path == "first");
    REQUIRE(results.tags[2].path == "last");
    REQUIRE(results.tags[2].tagId == 0x12340002);
    REQUIRE(results.tags[2].problemCount == 1);
    REQUIRE(results.tags[2].problems[0].startOffset == 0x1024);
    REQUIRE(results.tags[2].problems[0].endOffset == 0x1030);
    REQUIRE_FALSE(results.tags[1].error.empty());
    REQUIRE(results.groups.at(GroupId_Weapon).problemCount == 2);
}

TEST_CASE("Audit filters groups before scanning and handles an empty selection", "[audit]") {
    auto map = std::make_shared<TestMap>();
    map->addTag(GroupId_Weapon, "weapon", 16);
    map->addTag(GroupId_Effect, "effect", 16);
    map->trim();
    auto group = GENERATE(GroupId_Weapon, GroupId_Sound);
    AuditJob job(map, scalarSchema(), group);
    auto results = wait(job);
    REQUIRE(results.status == AuditStatus::Completed);
    REQUIRE(results.errorCount == 0);
    REQUIRE(results.totalTags == (group == GroupId_Weapon ? 1 : 0));
    REQUIRE(results.completedTags == results.totalTags);
    REQUIRE(results.problemCount == results.totalTags);
    if (group == GroupId_Weapon) REQUIRE(results.groups.size() == 1);
    else REQUIRE(results.groups.empty());
}

TEST_CASE("Audit counts beyond both the minimap and global storage limits", "[audit]") {
    constexpr size_t count = auditProblemLimit + 20;
    auto map = std::make_shared<TestMap>(0x1000 + count * 28);
    Schema schema;
    auto& root = schema.structures["data"];
    root.id = "data";
    root.groupId = GroupId_Weapon;
    root.size = count * 20;
    auto at = map->addTag(GroupId_Weapon, "many-gaps", count * 28);
    for (size_t i = 0; i < count; ++i) {
        field(schema, root, i * 20, Type::DataPointer);
        map->word(at + i * 20, 4);
        map->word(at + i * 20 + 12, map->pointer(static_cast<uint32_t>(at + root.size + i * 8)));
    }
    map->trim();
    ProblemOptions options;
    options.ignoreGapsAfterDataPointer = false;
    Context ctx{map.get(), &schema};
    StructureRef ref{&ctx, &root, map->bytes.data() + at};
    auto minimap = analyzeStructure(ref, ref.address, map->bytes.data() + map->cursor, options);
    REQUIRE(minimap.problems.size() == defaultMaxProblems);

    AuditJob job(map, schema, {}, options);
    auto results = wait(job);
    REQUIRE(results.status == AuditStatus::Completed);
    REQUIRE(results.errorCount == 0);
    REQUIRE(results.problemCount == count);
    REQUIRE(results.tags[0].problemCount == count);
    REQUIRE(results.storedProblemCount == 0x1000);
    REQUIRE(results.tags[0].problems.size() == 0x1000);
    REQUIRE(results.groups.at(GroupId_Weapon).problemCount == count);
}

TEST_CASE("Shared analysis finds probable references as well as gaps", "[audit]") {
    auto map = std::make_shared<TestMap>();
    auto at = map->addTag(GroupId_Weapon, "reference", 20);
    map->trim();
    auto schema = scalarSchema();
    schema.structures.at("root").size = 16;
    auto* tag = map->getTag(0);
    map->word(at, GroupId_Weapon);
    map->word(at + 4, tag->path.offset);
    map->word(at + 12, tag->tagHandle);
    Context ctx{map.get(), &schema};
    StructureRef ref{&ctx, &schema.structures.at("root"), map->bytes.data() + at};
    auto analysis = analyzeStructure(ref, ref.address, map->bytes.data() + map->cursor);
    REQUIRE(analysis.problems.size() == 2);
    REQUIRE(analysis.problems[0].kind == ProblemKind::Gap);
    REQUIRE(analysis.problems[1].kind == ProblemKind::ProbableTagReference);
}

TEST_CASE("Audit cancellation retains consistent partial results and joins on destruction", "[audit]") {
    auto map = std::make_shared<TestMap>();
    for (int i = 0; i < 16; ++i) map->addTag(GroupId_Weapon, "cancel", 16);
    map->trim();
    AuditJob job(map, scalarSchema());
    job.cancel();
    auto results = wait(job);
    REQUIRE((results.status == AuditStatus::Cancelled || results.status == AuditStatus::Completed));
    if (results.status == AuditStatus::Completed) REQUIRE(results.completedTags == results.totalTags);
    REQUIRE(results.completedTags <= results.totalTags);
    REQUIRE(results.completedTags == results.tags.size());
    REQUIRE(results.problemCount == results.storedProblemCount);
}

TEST_CASE("Managed map snapshots retain edits independently of the open map", "[audit]") {
    struct TemporaryMap {
        std::filesystem::path path = std::filesystem::temp_directory_path()
            / ("spark-audit-" + std::to_string(GetCurrentProcessId()) + "-"
                + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".map");
        ~TemporaryMap() { std::filesystem::remove(path); }
    } file;
    TestMap source;
    auto at = source.addTag(GroupId_Weapon, "snapshot", 16);
    source.trim();
    {
        std::ofstream out(file.path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(source.bytes.data()), source.cursor);
        REQUIRE(out.good());
    }
    auto map = Engine::Map::Spark::ManagedMapFile::create(file.path);
    REQUIRE(map != nullptr);
    auto* value = static_cast<uint32_t*>(map->getTagData(map->getTag(0)));
    *value = 123;
    auto snapshot = map->snapshot();
    *value = 456;
    auto* copiedValue = static_cast<uint32_t*>(snapshot->getTagData(snapshot->getTag(0)));
    REQUIRE(*copiedValue == 123);
    REQUIRE(snapshot->getFilePath() == map->getFilePath());
    REQUIRE(reinterpret_cast<uintptr_t>(copiedValue)
        - reinterpret_cast<uintptr_t>(snapshot->getCacheHeader()) == at);
    map.reset();
    REQUIRE(*copiedValue == 123);
    AuditJob job(snapshot, scalarSchema());
    snapshot.reset();
    auto results = wait(job);
    REQUIRE(results.status == AuditStatus::Completed);
    REQUIRE(results.errorCount == 0);
    REQUIRE(results.completedTags == 1);
}

TEST_CASE("Shared analysis rejects recursive block layouts without hanging", "[audit]") {
    auto map = std::make_shared<TestMap>();
    auto at = map->addTag(GroupId_Weapon, "recursive", 12);
    map->trim();
    auto schema = scalarSchema();
    auto& root = schema.structures.at("root");
    root.size = 12;
    root.fields.clear();
    field(schema, root, 0, Type::BlockPointer);
    schema.fields.at("root:0").type.ref = root.id;
    map->word(at, 1);
    map->word(at + 4, map->pointer(at));
    AuditJob job(map, schema);
    auto results = wait(job);
    REQUIRE(results.status == AuditStatus::Completed);
    REQUIRE(results.completedTags == 1);
    REQUIRE(results.errorCount == 1);
    REQUIRE_THAT(results.tags[0].error, Catch::Matchers::ContainsSubstring("nesting"));
}
