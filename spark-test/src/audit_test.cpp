#include <catch_amalgamated.hpp>

#include "engine/map/schema/audit.hpp"
#include "engine/map/schema/functions.hpp"
#include "engine/map/schema/hints.hpp"
#include "engine/map/spark/managed_map_file.hpp"
#include "ui/dissect/State.hpp"

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
            std::memcpy(cache->magic, "daeh", 4);
            std::memcpy(cache->footerMagic, "toof", 4);
            cache->cacheVersion = 13;
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

    struct TemporaryMap {
        std::filesystem::path path = std::filesystem::temp_directory_path()
            / ("spark-audit-" + std::to_string(GetCurrentProcessId()) + "-"
                + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".map");
        ~TemporaryMap() { std::filesystem::remove(path); }

        void write(const TestMap& map) const {
            std::ofstream out(path, std::ios::binary);
            out.write(reinterpret_cast<const char*>(map.bytes.data()), map.cursor);
            REQUIRE(out.good());
        }
    };

    template<typename Job>
    auto wait(Job& job) -> decltype(job.snapshot()) {
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

TEST_CASE("First-child size heuristic searches inline structures in field order", "[size-heuristic]") {
    auto map = std::make_shared<TestMap>();
    auto at = map->addTag(GroupId_Weapon, "inline-size", 128);
    map->trim();
    auto schema = scalarSchema();
    auto& root = schema.structures.at("root");
    root.size = 0;
    field(schema, root, 4, Type::Structure);
    schema.fields.at("root:4").type.ref = "embedded";
    auto& embedded = schema.structures["embedded"];
    embedded.id = "embedded";
    embedded.size = 16;
    field(schema, embedded, 4, Type::Structure);
    schema.fields.at("embedded:4").type.ref = "leaf";
    auto& leaf = schema.structures["leaf"];
    leaf.id = "leaf";
    leaf.size = 12;
    field(schema, leaf, 0, Type::BlockPointer);
    schema.fields.at("leaf:0").type.ref = "payload";
    auto& payload = schema.structures["payload"];
    payload.id = "payload";
    payload.size = 4;
    field(schema, root, 24, Type::BlockPointer);
    schema.fields.at("root:24").type.ref = "payload";
    map->word(at + 8, 1);
    map->word(at + 12, map->pointer(at + 80));
    map->word(at + 24, 1);
    map->word(at + 28, map->pointer(at + 64));
    Context ctx{map.get(), &schema};
    StructureRef ref{&ctx, &root, map->bytes.data() + at};

    SECTION("Nested child is relative to the root, not the inline structure") {
        REQUIRE(guessSizeFromFirstBlockElement(ref) == 80);
        StructureRef inlineRef{&ctx, &embedded, map->bytes.data() + at + 4};
        REQUIRE(guessSizeFromFirstBlockElement(inlineRef) == 76);
    }
    SECTION("Empty nested blocks allow later root fields to be considered") {
        map->word(at + 8, 0);
        REQUIRE(guessSizeFromFirstBlockElement(ref) == 64);
    }
    SECTION("No populated blocks means no estimate") {
        map->word(at + 8, 0);
        map->word(at + 24, 0);
        REQUIRE(guessSizeFromFirstBlockElement(ref) == 0);
    }
    SECTION("Missing inline schemas allow later root fields to be considered") {
        schema.fields.at("root:4").type.ref = "missing";
        REQUIRE(guessSizeFromFirstBlockElement(ref) == 64);
    }
    SECTION("Earlier direct blocks retain priority") {
        field(schema, root, 0, Type::BlockPointer);
        schema.fields.at("root:0").type.ref = "payload";
        map->word(at, 1);
        map->word(at + 4, map->pointer(at + 48));
        REQUIRE(guessSizeFromFirstBlockElement(ref) == 48);
    }
    SECTION("Cyclic inline schemas are bounded and later siblings still work") {
        embedded.fields.clear();
        field(schema, embedded, 0, Type::Structure);
        schema.fields.at("embedded:0").type.ref = "embedded";
        REQUIRE(guessSizeFromFirstBlockElement(ref) == 64);
    }
}

TEST_CASE("Spans retain instance ancestry through nested blocks and data payloads", "[spans]") {
    auto map = std::make_shared<TestMap>();
    auto at = map->addTag(GroupId_Weapon, "nested", 128);
    map->trim();
    auto schema = scalarSchema();
    auto& root = schema.structures.at("root");
    root.size = GENERATE(0, 12);
    root.fields.clear();
    field(schema, root, 0, Type::BlockPointer);
    schema.fields.at("root:0").type.ref = "child";
    auto& child = schema.structures["child"];
    child.id = "child";
    child.size = 12;
    field(schema, child, 0, Type::BlockPointer);
    schema.fields.at("child:0").type.ref = "leaf";
    auto& leaf = schema.structures["leaf"];
    leaf.id = "leaf";
    leaf.size = 20;
    field(schema, leaf, 0, Type::DataPointer);

    map->word(at, 2);
    map->word(at + 4, map->pointer(at + 64));
    for (uint32_t i = 0; i < 2; ++i) {
        auto childAt = at + 64 + i * 12;
        auto leafAt = at + 16 + i * 20;
        map->word(childAt, 1);
        map->word(childAt + 4, map->pointer(leafAt));
        map->word(leafAt, 4);
        map->word(leafAt + 12, map->pointer(at + 96 + i * 4));
    }
    Context ctx{map.get(), &schema};
    StructureRef ref{&ctx, &root, map->bytes.data() + at};
    SpanMap spans;
    fillSpanMap(ref, spans, ref.address, map->bytes.data() + map->cursor);
    auto flattened = analyzeOverlaps(spans);
    spans.spans.clear();
    REQUIRE(flattened.size() == 6);
    for (uint32_t i = 0; i < 2; ++i) {
        const auto& leafSpan = flattened[i].span;
        REQUIRE(leafSpan.type == &leaf);
        REQUIRE(leafSpan.ancestors.size() == 2);
        REQUIRE(leafSpan.ancestors[0].type == &root);
        REQUIRE(leafSpan.ancestors[0].address == ref.address);
        REQUIRE(leafSpan.ancestors[1].type == &child);
        REQUIRE(static_cast<void*>(leafSpan.ancestors[1].address) == map->bytes.data() + at + 64 + i * 12);
        REQUIRE(flattened[2 + i].span.ancestors.size() == 1);
        const auto& payload = flattened[4 + i].span;
        REQUIRE(payload.fieldType == Type::DataPointer);
        REQUIRE(payload.ancestors.size() == 3);
        REQUIRE(payload.ancestors[2].type == &leaf);
        REQUIRE(payload.ancestors[2].address == leafSpan.start);
    }
    for (const auto& info : flattened) REQUIRE_FALSE(info.overlaps);

    map->word(at, 1025);
    fillSpanMap(ref, spans, ref.address, map->bytes.data() + map->cursor);
    REQUIRE(spans.spans.size() == 1);
    const auto& collapsed = *spans.spans.begin();
    REQUIRE(collapsed.ancestors.size() == 1);
    REQUIRE(collapsed.ancestors[0].type == &root);
    REQUIRE(collapsed.end - collapsed.start == 1025 * child.size);
}

TEST_CASE("Spans traverse inline structures without counting their storage twice", "[spans]") {
    auto map = std::make_shared<TestMap>();
    auto at = map->addTag(GroupId_Weapon, "inline", 80);
    map->trim();
    auto schema = scalarSchema();
    auto& root = schema.structures.at("root");
    root.size = 16;
    field(schema, root, 4, Type::Structure);
    schema.fields.at("root:4").type.ref = "inline";
    auto& embedded = schema.structures["inline"];
    embedded.id = "inline";
    embedded.size = 12;
    field(schema, embedded, 0, Type::BlockPointer);
    schema.fields.at("inline:0").type.ref = "child";
    auto& child = schema.structures["child"];
    child.id = "child";
    child.size = 24;
    field(schema, child, 4, Type::Structure);
    schema.fields.at("child:4").type.ref = "leaf";
    auto& leaf = schema.structures["leaf"];
    leaf.id = "leaf";
    leaf.size = 20;
    field(schema, leaf, 0, Type::DataPointer);

    map->word(at + 4, 2);
    map->word(at + 8, map->pointer(at + 16));
    for (uint32_t i = 0; i < 2; ++i) {
        auto leafAt = at + 20 + i * 24;
        map->word(leafAt, 8);
        map->word(leafAt + 12, map->pointer(at + 64 + i * 8));
    }

    Context ctx{map.get(), &schema};
    StructureRef ref{&ctx, &root, map->bytes.data() + at};
    auto analysis = analyzeStructure(ref, ref.address, map->bytes.data() + map->cursor);
    REQUIRE(analysis.overlaps.size() == 5);
    REQUIRE(analysis.problems.empty());
    for (const auto& info : analysis.overlaps) REQUIRE_FALSE(info.overlaps);
    for (size_t i = 0; i < 2; ++i) {
        const auto& block = analysis.overlaps[1 + i].span;
        REQUIRE(block.type == &child);
        REQUIRE(block.index == i);
        REQUIRE(block.count == 2);
        REQUIRE(block.ancestors.size() == 2);
        REQUIRE(block.ancestors[1].type == &embedded);
        REQUIRE(block.ancestors[1].address == reinterpret_cast<char*>(map->bytes.data() + at + 4));
        REQUIRE(block.ancestors[1].index == -1);
        const auto& payload = analysis.overlaps[3 + i].span;
        REQUIRE(payload.fieldType == Type::DataPointer);
        REQUIRE(payload.start == reinterpret_cast<char*>(map->bytes.data() + at + 64 + i * 8));
        REQUIRE(payload.ancestors.size() == 4);
        REQUIRE(payload.ancestors[2].type == &child);
        REQUIRE(payload.ancestors[2].index == i);
        REQUIRE(payload.ancestors[2].count == 2);
        REQUIRE(payload.ancestors[3].type == &leaf);
        REQUIRE(payload.ancestors[3].address == block.start + 4);
        REQUIRE(payload.ancestors[3].index == -1);
        REQUIRE(payload.ancestors[3].count == 0);
    }
}

TEST_CASE("Inline span traversal rejects out-of-range and recursive structures", "[spans]") {
    auto map = std::make_shared<TestMap>();
    auto at = map->addTag(GroupId_Weapon, "inline", 16);
    map->trim();
    auto schema = scalarSchema();
    auto& root = schema.structures.at("root");
    field(schema, root, 0, Type::Structure);
    schema.fields.at("root:0").type.ref = "root";
    Context ctx{map.get(), &schema};
    StructureRef ref{&ctx, &root, map->bytes.data() + at};
    SpanMap spans;

    SECTION("Recursive inline references remain bounded") {
        REQUIRE_THROWS_WITH(fillSpanMap(ref, spans, ref.address, map->bytes.data() + map->cursor),
            Catch::Matchers::ContainsSubstring("nesting"));
    }
    SECTION("Inline storage must fit within the tag") {
        root.size = 20;
        REQUIRE_THROWS_WITH(fillSpanMap(ref, spans, ref.address, map->bytes.data() + map->cursor),
            "Inline structure extends outside the tag data range");
    }
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

TEST_CASE("Audit optionally excludes absent and empty schemas from results and progress", "[audit]") {
    auto map = std::make_shared<TestMap>();
    map->addTag(GroupId_Weapon, "first", 16);
    map->addTag(GroupId_Effect, "missing", 16);
    map->addTag(GroupId_Sound, "empty", 16);
    map->addTag(GroupId_Weapon, "last", 16);
    map->trim();
    auto schema = scalarSchema();
    auto& empty = schema.structures["empty"];
    empty.id = "empty";
    empty.groupId = GroupId_Sound;
    bool exclude = GENERATE(false, true);
    AuditJob job(map, schema, {}, {}, exclude);
    auto results = wait(job);
    REQUIRE(results.status == AuditStatus::Completed);
    REQUIRE(results.totalTags == (exclude ? 2 : 4));
    REQUIRE(results.completedTags == results.totalTags);
    REQUIRE(results.excludedTags == (exclude ? 2 : 0));
    REQUIRE(results.errorCount == (exclude ? 0 : 2));
    REQUIRE(results.groups.size() == (exclude ? 1 : 3));
    REQUIRE(results.problemCount == 2);
    REQUIRE(results.tags.front().problems[0].endOffset == 0x1010);
    REQUIRE(results.tags.back().path == "last");

    AuditJob filtered(map, schema, GroupId_Effect, {}, exclude);
    auto filteredResults = wait(filtered);
    REQUIRE(filteredResults.status == AuditStatus::Completed);
    REQUIRE(filteredResults.totalTags == (exclude ? 0 : 1));
    REQUIRE(filteredResults.excludedTags == (exclude ? 1 : 0));
    REQUIRE(filteredResults.errorCount == (exclude ? 0 : 1));
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
    options.ignoreAlignmentGaps = false;
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
    TemporaryMap file;
    TestMap source;
    auto at = source.addTag(GroupId_Weapon, "snapshot", 16);
    source.trim();
    file.write(source);
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

    TEST_CASE("Batch audits keep map-local identities and reload only unchanged files", "[audit][batch-audit]") {
        TemporaryMap first, second;
        TestMap source;
        source.addTag(GroupId_Weapon, "weapon", 16);
        source.addTag(GroupId_Effect, "effect", 16);
        source.trim();
        first.write(source);
        second.write(source);
        AuditBatchJob job({{first.path}, {second.path}}, scalarSchema());
        auto results = wait(job);
        REQUIRE(results.status == AuditStatus::Completed);
        REQUIRE(results.completedMaps == 2);
        REQUIRE(results.failedMaps == 0);
        REQUIRE(results.completedTags == 4);
        REQUIRE(results.errorCount == 2);
        REQUIRE(results.problemCount == 2);
        REQUIRE(results.storedProblemCount == 2);
        REQUIRE(results.maps[0].path != results.maps[1].path);
        REQUIRE(results.maps[0].analysis.tags[0].tagId == results.maps[1].analysis.tags[0].tagId);
        auto firstView = job.openMap(0);
        auto secondView = job.openMap(1);
        REQUIRE(firstView != secondView);
        REQUIRE(firstView->getFilePath() == first.path);
        REQUIRE(secondView->getFilePath() == second.path);

        // A same-size edit with the original timestamp must still invalidate disk results.
        auto modified = std::filesystem::last_write_time(second.path);
        source.word(0x1000, 42);
        second.write(source);
        std::filesystem::last_write_time(second.path, modified);
        REQUIRE_THROWS_WITH(job.openMap(1), Catch::Matchers::ContainsSubstring("changed"));
        REQUIRE_NOTHROW(job.openMap(0));
        std::filesystem::remove(first.path);
        REQUIRE_THROWS_WITH(job.openMap(0), Catch::Matchers::ContainsSubstring("reopen"));
    }

    TEST_CASE("Batch audits continue after map errors and apply filters to every map", "[audit][batch-audit]") {
        TemporaryMap missing, first, second, invalid;
        TestMap source;
        source.addTag(GroupId_Weapon, "weapon", 16);
        source.addTag(GroupId_Effect, "effect", 16);
        source.trim();
        first.write(source);
        second.write(source);
        source.getTagDataHeader()->tagArray.offset = 0xFFFFFFFF;
        invalid.write(source);
        bool excludeMissing = GENERATE(false, true);
        AuditBatchJob job({{missing.path}, {first.path}, {invalid.path}, {second.path}},
            scalarSchema(), {}, {}, excludeMissing);
        auto results = wait(job);
        REQUIRE(results.completedMaps == 4);
        REQUIRE(results.failedMaps == 2);
        REQUIRE(results.maps[0].analysis.status == AuditStatus::Failed);
        REQUIRE_FALSE(results.maps[0].analysis.error.empty());
        REQUIRE(results.maps[2].analysis.status == AuditStatus::Failed);
        REQUIRE(results.maps[3].analysis.status == AuditStatus::Completed);
        REQUIRE(results.completedTags == (excludeMissing ? 2 : 4));
        REQUIRE(results.excludedTags == (excludeMissing ? 2 : 0));
        REQUIRE(results.errorCount == (excludeMissing ? 0 : 2));
        AuditBatchJob filtered({{first.path}, {second.path}}, scalarSchema(), GroupId_Weapon);
        auto filteredResults = wait(filtered);
        REQUIRE(filteredResults.completedTags == 2);
        REQUIRE(filteredResults.errorCount == 0);
    }

    TEST_CASE("Batch snapshots survive source closure and open independent inspection copies", "[audit][batch-audit]") {
        TemporaryMap file;
        TestMap source;
        source.addTag(GroupId_Weapon, "snapshot", 16);
        source.trim();
        file.write(source);
        auto map = Engine::Map::Spark::ManagedMapFile::create(file.path);
        REQUIRE(map != nullptr);
        auto value = [](const auto& map) { return static_cast<uint32_t*>(map->getTagData(map->getTag(0))); };
        *value(map) = 123;
        AuditBatchJob job({{file.path, map->snapshot()}}, scalarSchema());
        *value(map) = 456;
        map.reset();
        std::filesystem::remove(file.path);
        auto results = wait(job);
        REQUIRE(results.maps[0].fromSnapshot);
        REQUIRE(results.completedTags == 1);
        auto view = job.openMap(0);
        REQUIRE(*value(view) == 123);
        *value(view) = 789;
        REQUIRE(*value(job.openMap(0)) == 123);
    }

    TEST_CASE("Batch detailed-problem budget is shared while counts continue across maps", "[audit][batch-audit]") {
        constexpr size_t count = 0x810;
        TemporaryMap first, second;
        TestMap map(0x1000 + count * 28);
        Schema schema;
        auto& root = schema.structures["data"];
        root.id = "data";
        root.groupId = GroupId_Weapon;
        root.size = count * 20;
        auto at = map.addTag(GroupId_Weapon, "gaps", count * 28);
        for (size_t i = 0; i < count; ++i) {
            field(schema, root, i * 20, Type::DataPointer);
            map.word(at + i * 20, 4);
            map.word(at + i * 20 + 12, map.pointer(static_cast<uint32_t>(at + root.size + i * 8)));
        }
        map.trim();
        first.write(map);
        second.write(map);
        AuditBatchJob job({{first.path}, {second.path}}, schema);
        auto results = wait(job);
        REQUIRE(results.completedMaps == 2);
        REQUIRE(results.problemCount == count * 2);
        REQUIRE(results.storedProblemCount == 0x1000);
        REQUIRE(results.maps[0].analysis.storedProblemCount == count);
        REQUIRE(results.maps[1].analysis.storedProblemCount == 0x1000 - count);
        REQUIRE(results.maps[1].analysis.groups.at(GroupId_Weapon).problemCount == count);
    }

    TEST_CASE("Batch cancellation preserves consistent partial results", "[audit][batch-audit]") {
        TemporaryMap file;
        TestMap source;
        source.addTag(GroupId_Weapon, "cancel", 16);
        source.trim();
        file.write(source);
        AuditBatchJob job(std::vector<AuditSource>(64, {file.path}), scalarSchema());
        job.cancel();
        auto results = wait(job);
        REQUIRE((results.status == AuditStatus::Cancelled || results.status == AuditStatus::Completed));
        REQUIRE(results.completedMaps <= results.maps.size());
        REQUIRE(results.completedTags == results.problemCount);
        REQUIRE(results.problemCount == results.storedProblemCount);
        for (const auto& map : results.maps) {
            REQUIRE(map.analysis.status != AuditStatus::Running);
            REQUIRE(map.analysis.status != AuditStatus::Pending);
        }
    }

    TEST_CASE("Dissector identity and ownership isolate identical tags and tabs across maps", "[audit][batch-audit]") {
        using namespace SparkLib::UI::DissectTag;
        State state;
        auto first = std::make_shared<TestMap>();
        auto second = std::make_shared<TestMap>();
        std::weak_ptr<TestMap> lifetime = first;
        auto* firstMap = first.get();
        state.openOwnedWindow(first, 1, "first");
        state.openOwnedWindow(second, 1, "second");
        state.openWindow(first.get(), 1, 16, "child", 0);
        state.openWindow(second.get(), 1, 16, "child", 0);
        REQUIRE(state.windows.size() == 4);
        state.openOwnedWindow(first, 1, "first");
        REQUIRE(state.windows.size() == 4);
        first.reset();
        REQUIRE_FALSE(lifetime.expired());
        auto job = std::make_shared<MinimapJob>();
        auto& child = state.windows.at(windowKey(firstMap, 1, 16));
        REQUIRE(child.mapOwner != nullptr);
        child.minimapJob = job;
        std::atomic<bool> aliveUntilCancelled = false;
        std::thread worker([&] {
            while (!job->cancel) std::this_thread::yield();
            aliveUntilCancelled = !lifetime.expired();
            job->done = true;
        });
        state.windows.at(windowKey(firstMap, 1)).isOpen = false;
        state.cleanupWindows();
        worker.join();
        REQUIRE(aliveUntilCancelled);
        REQUIRE(lifetime.expired());
        REQUIRE(state.windows.size() == 2);
        REQUIRE(state.windows.at(windowKey(second.get(), 1)).isOpen);
        REQUIRE(state.windows.at(windowKey(second.get(), 1, 16)).isOpen);
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
