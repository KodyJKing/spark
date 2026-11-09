#include <catch_amalgamated.hpp>
#include <cstring>
#include <limits>

#include "engine/map/spark/static_tag_injection/inject_tags.hpp"
#include "engine/map/bitmap.hpp"
#include "engine/map/model.hpp"
#include "engine/map/schema/functions.hpp"
#include "engine/map/spark/static_tag_injection/configured_injection.hpp"
#include "engine/map/spark/static_tag_injection/injection_files.hpp"
#include <fstream>
#include <future>

using namespace Engine;
using namespace Engine::Map;
using namespace Engine::Map::Spark;

namespace {
    using Bytes = std::vector<uint8_t>;
    constexpr uint32_t base = 0x50000000;
    constexpr uint32_t invalid = 0xFFFFFFFF;

    template<class T> T get(const Bytes& bytes, size_t offset) {
        REQUIRE(offset + sizeof(T) <= bytes.size());
        T value;
        std::memcpy(&value, bytes.data() + offset, sizeof(T));
        return value;
    }

    TEST_CASE("Injection manifests combine wildcard and map selections without duplicates", "[configured_injection]") {
        const auto manifest = std::filesystem::path(L"C:\\tags\\mytags.json");
        const auto package = parseInjectionPackage(R"({
            "cache": "mytags.map",
            "maps": {
                "*": {"weap": ["weapons\\fuel rod gun\\fuel rod"]},
                "b30": {"mod2": ["weapons\\shotgun\\shotgun"], "weap": ["weapons\\fuel rod gun\\fuel rod"]}
            }
        })", manifest, "B30");
        REQUIRE(package.cache == manifest.parent_path() / "mytags.map");
        REQUIRE(package.tags.size() == 2);
        REQUIRE(std::count_if(package.tags.begin(), package.tags.end(), [](const auto& tag) {
            return tag.groupId == GroupId_Model && tag.path == R"(weapons\shotgun\shotgun)";
        }) == 1);
        REQUIRE(std::count_if(package.tags.begin(), package.tags.end(), [](const auto& tag) {
            return tag.groupId == GroupId_Weapon && tag.path == R"(weapons\fuel rod gun\fuel rod)";
        }) == 1);
        const auto wildcard = parseInjectionPackage(
            R"({"cache":"mytags.map","maps":{"*":{"weap":["guest"]},"b30":{"mod2":["model"]}}})", manifest, "a10");
        REQUIRE(wildcard.tags.size() == 1);
        REQUIRE(wildcard.tags.front().groupId == GroupId_Weapon);
    }

    TEST_CASE("Injection manifests reject malformed selections and escaping cache paths", "[configured_injection]") {
        std::string json;
        SECTION("invalid JSON") { json = "{"; }
        SECTION("missing maps") { json = R"({"cache":"guest.map"})"; }
        SECTION("unknown property") { json = R"({"cache":"guest.map","maps":{},"extra":0})"; }
        SECTION("non-string cache") { json = R"({"cache":0,"maps":{}})"; }
        SECTION("escaping cache") { json = R"({"cache":"..\\guest.map","maps":{}})"; }
        SECTION("absolute cache") { json = R"({"cache":"C:\\guest.map","maps":{}})"; }
        SECTION("incorrect extension") { json = R"({"cache":"guest.json","maps":{}})"; }
        SECTION("map selector path") { json = R"({"cache":"guest.map","maps":{"..\\b30":{}}})"; }
        SECTION("non-object selection") { json = R"({"cache":"guest.map","maps":{"b30":[]}})"; }
        SECTION("short group") { json = R"({"cache":"guest.map","maps":{"b30":{"mod":[]}}})"; }
        SECTION("non-array paths") { json = R"({"cache":"guest.map","maps":{"b30":{"mod2":"model"}}})"; }
        SECTION("non-string path") { json = R"({"cache":"guest.map","maps":{"b30":{"mod2":[0]}}})"; }
        SECTION("empty path") { json = R"({"cache":"guest.map","maps":{"b30":{"mod2":[""]}}})"; }
        SECTION("embedded null path") { json = R"({"cache":"guest.map","maps":{"b30":{"mod2":["a\u0000b"]}}})"; }
        REQUIRE_THROWS(parseInjectionPackage(json, L"C:\\tags\\package.json", "b30"));
    }

    TEST_CASE("Configured injection skips protected maps and non-read opens before any IO", "[configured_injection]") {
        ConfiguredInjection service(L"C:\\missing\\halo1\\maps", L"C:\\missing\\tags", L"C:\\missing\\schema.json");
        for (auto name : {"bitmaps.map", "sounds.map", "loc.map", "ui.map", "MAINMENU.MAP",
            "shared.map", "single_player_shared.map", "campaign.map"}) {
            INFO(name);
            REQUIRE(excludedInjectionMap(name));
            REQUIRE_FALSE(service.prepare(std::filesystem::path(L"C:\\missing\\halo1\\maps") / name, 1));
        }
        REQUIRE_FALSE(excludedInjectionMap("b30.map"));
        for (uint32_t flags : {0u, 2u, 3u, 9u, 17u, 0xFFFFFFFFu}) {
            CAPTURE(flags);
            REQUIRE_FALSE(injectionReadOpen(flags));
            REQUIRE_FALSE(service.prepare(L"C:\\missing\\halo1\\maps\\b30.map", flags));
        }
        REQUIRE(injectionReadOpen(1));
        REQUIRE(injectionReadOpen(5));
        REQUIRE_FALSE(service.prepare(L"C:\\missing\\halo1\\maps\\spark_cache\\b30.map", 1));
        REQUIRE_FALSE(service.prepare(L"C:\\missing\\halo2\\maps\\b30.map", 1));
        REQUIRE_FALSE(service.prepare(L"C:\\missing\\halo1\\maps\\b30.txt", 1));
    }

    template<class T> void put(Bytes& bytes, size_t offset, const T& value) {
        REQUIRE(offset + sizeof(T) <= bytes.size());
        std::memcpy(bytes.data() + offset, &value, sizeof(T));
    }

    struct Fixture {
        Bytes bytes = Bytes(0x2000, 0);
        static constexpr uint32_t tagsAt = 0x9E0;
        uint32_t cursor = 0xC00;
        uint32_t nameCursor = 0xB00;
        uint32_t tagCount = 0;

        Fixture() {
            CacheHeader cache{};
            std::memcpy(cache.magic, "daeh", 4);
            std::memcpy(cache.footerMagic, "toof", 4);
            cache.cacheVersion = 13;
            cache.fileSize = static_cast<uint32_t>(bytes.size());
            cache.tagData.offset = tagsAt;
            cache.tagDataSize = static_cast<uint32_t>(bytes.size()) - tagsAt;
            cache.checksum = 0x12345678;
            put(bytes, 0, cache);
            TagDataHeader header{};
            std::memcpy(header.magic, "sgat", 4);
            header.tagArray.offset = base + sizeof(TagDataHeader);
            header.scenarioId = invalid;
            header.modelData.offset = 0x980;
            header.vertexDataSize = 0x40;
            header.modelDataSize = 0x60;
            header.checksum = 0x87654321;
            put(bytes, tagsAt, header);
            for (uint32_t i = 0x800; i < tagsAt; ++i) bytes[i] = static_cast<uint8_t>(i);
        }

        uint32_t allocate(size_t size) {
            auto at = cursor;
            cursor += static_cast<uint32_t>((size + 3) & ~size_t(3));
            REQUIRE(cursor <= bytes.size());
            return at;
        }

        uint32_t pointer(uint32_t file) const { return base + file - tagsAt; }

        uint32_t name(const char* text) {
            auto at = nameCursor;
            auto size = std::strlen(text) + 1;
            REQUIRE(at + size < 0xC00);
            std::memcpy(bytes.data() + at, text, size);
            nameCursor += static_cast<uint32_t>(size);
            return pointer(at);
        }

        uint32_t add(uint32_t group, const char* path, uint32_t data, uint32_t salt = 0xE1170000) {
            REQUIRE(tagCount < 6);
            Tag tag{};
            tag.groupID = group;
            tag.parentGroupID = tag.grandparentGroupID = invalid;
            tag.tagHandle = salt | tagCount;
            tag.path.offset = name(path);
            tag.data.offset = data ? pointer(data) : 0;
            put(bytes, tagsAt + sizeof(TagDataHeader) + tagCount * sizeof(Tag), tag);
            ++tagCount;
            auto header = get<TagDataHeader>(bytes, tagsAt);
            header.tagCount = tagCount;
            put(bytes, tagsAt, header);
            return tag.tagHandle;
        }
    };

    struct Schemas {
        Schema schema;

        StructureNode& node(const char* id, size_t size, TagGroupId group = GroupId_Invalid) {
            auto& result = schema.structures[id];
            result.id = id;
            result.size = size;
            result.groupId = group;
            result.complete = group != GroupId_Invalid;
            return result;
        }

        void field(StructureNode& node, size_t offset, Type type, const char* ref = "") {
            auto id = node.id + ":" + std::to_string(offset);
            FieldNode field;
            field.id = id;
            field.offset = offset;
            field.type = {type, ref};
            node.fields[offset] = id;
            schema.fields[id] = field;
        }

        void padding(StructureNode& node, size_t begin, size_t end) {
            for (size_t at = begin; at < end; ++at) field(node, at, Type::U8);
        }

        void scalar(TagGroupId group = GroupId_Weapon) {
            auto& root = node("scalar", 4, group);
            field(root, 0, Type::U32);
        }

        void bitmap() {
            auto& root = node("bitmap", sizeof(Bitmap), GroupId_Bitmap);
            padding(root, 0, 0x54);
            field(root, 0x54, Type::BlockPointer, "sequence");
            field(root, 0x60, Type::BlockPointer, "bitmap-data");
            auto& sequence = node("sequence", sizeof(GroupSequence));
            padding(sequence, 0, 0x34);
            field(sequence, 0x34, Type::BlockPointer, "sprite");
            auto& sprite = node("sprite", sizeof(Sprite));
            padding(sprite, 0, sprite.size);
            auto& data = node("bitmap-data", sizeof(BitmapData));
            padding(data, 0, data.size);
        }

        void model() {
            auto& root = node("model", sizeof(Model), GroupId_Model);
            padding(root, 0, 0xAC);
            field(root, 0xAC, Type::BlockPointer, "marker");
            field(root, 0xB8, Type::BlockPointer, "node");
            field(root, 0xC4, Type::BlockPointer, "region");
            field(root, 0xD0, Type::BlockPointer, "geometry");
            field(root, 0xDC, Type::BlockPointer, "shader");
            auto& geometry = node("geometry", sizeof(Geometry));
            padding(geometry, 0, 0x24);
            field(geometry, 0x24, Type::BlockPointer, "part");
            auto& part = node("part", sizeof(Part));
            padding(part, 0, part.size);
            auto& shader = node("shader", sizeof(Shader));
            field(shader, 0, Type::TagReference);
            padding(shader, 16, shader.size);
        }
    };

    TagDataHeader header(const Bytes& bytes) {
        return get<TagDataHeader>(bytes, get<CacheHeader>(bytes, 0).tagData.offset);
    }

    uint32_t file(const Bytes& bytes, uint32_t pointer) {
        REQUIRE(pointer >= base);
        return get<CacheHeader>(bytes, 0).tagData.offset + pointer - base;
    }

    Tag tag(const Bytes& bytes, uint32_t index) {
        return get<Tag>(bytes, file(bytes, header(bytes).tagArray.offset) + index * sizeof(Tag));
    }

    std::string path(const Bytes& bytes, uint32_t pointer) {
        auto at = file(bytes, pointer);
        REQUIRE(at < bytes.size());
        auto start = reinterpret_cast<const char*>(bytes.data() + at);
        auto end = static_cast<const char*>(std::memchr(start, 0, bytes.size() - at));
        REQUIRE(end != nullptr);
        return std::string(start, end);
    }
}

TEST_CASE("Configured injection publishes immutable cache files and invalidates changed inputs", "[configured_injection]") {
    struct Directory {
        std::filesystem::path path = std::filesystem::temp_directory_path() /
            ("spark-config-test-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
        Directory() { REQUIRE(std::filesystem::create_directory(path)); }
        ~Directory() {
            std::error_code error;
            std::filesystem::remove_all(path, error);
            if (error) std::cerr << "Cannot clean configured injection test: " << error.message() << '\n';
        }
    } temporary;
    const auto maps = temporary.path / "halo1" / "maps", tags = temporary.path / "tags";
    std::filesystem::create_directories(maps);
    std::filesystem::create_directory(tags);
    auto write = [](const std::filesystem::path& path, std::span<const uint8_t> bytes) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        REQUIRE(out.is_open());
        out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        REQUIRE(out.good());
    };
    auto writeText = [&](const std::filesystem::path& path, const std::string& text) {
        write(path, {reinterpret_cast<const uint8_t*>(text.data()), text.size()});
    };
    Fixture host, guest;
    Schemas schemas;
    auto& scenario = schemas.node("scenario", 4, GroupId_Scenario);
    schemas.field(scenario, 0, Type::U32);
    schemas.scalar();
    auto scenarioHandle = host.add(GroupId_Scenario, "scenario", host.allocate(4));
    auto th = header(host.bytes);
    th.scenarioId = scenarioHandle;
    put(host.bytes, Fixture::tagsAt, th);
    auto g = guest.allocate(4);
    guest.add(GroupId_Weapon, "guest", g);
    put(guest.bytes, g, uint32_t(123));
    const auto hostPath = maps / "b30.map", guestPath = tags / "guest.map", schemaPath = tags / "schema.txt";
    write(hostPath, host.bytes);
    write(guestPath, guest.bytes);
    writeText(schemaPath, schemas.schema.toJsonString());
    const auto manifest = tags / "package.json";
    writeText(manifest, R"({"cache":"guest.map","maps":{"*":{"weap":["guest"]}}})");
    ConfiguredInjection service(maps, tags, schemaPath);
    const auto first = service.prepare(hostPath, 1);
    REQUIRE(first.has_value());
    REQUIRE(first->path.parent_path() == maps / "spark_cache");
    const auto firstBytes = readInjectionMap(first->path);
    REQUIRE(listInjectionTags(firstBytes).size() == 2);
    REQUIRE(get<uint32_t>(firstBytes, file(firstBytes, tag(firstBytes, 1).data.offset)) == 123);
    REQUIRE(readInjectionMap(hostPath) == host.bytes);
    REQUIRE(readInjectionMap(guestPath) == guest.bytes);
    REQUIRE(service.prepare(hostPath, 1)->path == first->path);
    auto parallel = std::async(std::launch::async, [&] { return service.prepare(hostPath, 1); });
    REQUIRE(service.prepare(hostPath, 1)->path == parallel.get()->path);

    SECTION("changed guest content") {
        put(guest.bytes, g, uint32_t(456));
        write(guestPath, guest.bytes);
        const auto changed = service.prepare(hostPath, 1);
        REQUIRE(changed->path != first->path);
        const auto bytes = readInjectionMap(changed->path);
        REQUIRE(get<uint32_t>(bytes, file(bytes, tag(bytes, 1).data.offset)) == 456);
        REQUIRE(readInjectionMap(first->path) == firstBytes);
    }
    SECTION("changed host content") {
        host.bytes.back() ^= 1;
        write(hostPath, host.bytes);
        REQUIRE(service.prepare(hostPath, 1)->path != first->path);
    }
    SECTION("changed schema content") {
        schemas.schema.structures.at("scalar").complete = false;
        writeText(schemaPath, schemas.schema.toJsonString());
        REQUIRE(service.prepare(hostPath, 1)->path != first->path);
    }
    SECTION("changed manifest content") {
        writeText(manifest, R"({"cache":"guest.map","maps":{"b30":{"weap":["guest"]}}})");
        REQUIRE(service.prepare(hostPath, 1)->path != first->path);
    }
    SECTION("missing dependency is not hidden by a cached output") {
        std::filesystem::remove(guestPath);
        REQUIRE_THROWS(service.prepare(hostPath, 1));
    }
    SECTION("corrupted cached output is rebuilt") {
        writeText(first->path, "corrupt");
        REQUIRE(service.prepare(hostPath, 1)->path == first->path);
        REQUIRE(readInjectionMap(first->path) == firstBytes);
    }
    SECTION("UI scenario type is rejected even with a level filename") {
        auto cacheHeader = get<CacheHeader>(host.bytes, 0);
        cacheHeader.scenarioType = 2;
        put(host.bytes, 0, cacheHeader);
        write(hostPath, host.bytes);
        REQUIRE_THROWS(service.prepare(hostPath, 1));
    }
    SECTION("multiple manifests reuse an already imported tag") {
        writeText(tags / "second.json", R"({"cache":"guest.map","maps":{"b30":{"weap":["guest"]}}})");
        auto result = service.prepare(hostPath, 1);
        REQUIRE(listInjectionTags(readInjectionMap(result->path)).size() == 2);
    }
}

TEST_CASE("Shared relocation sensitivity includes pointers references and embedded structures", "[inject_tags]") {
    const std::array sensitive{Type::DataPointer, Type::BlockPointer, Type::Structure, Type::TagReference};
    Schemas schema;
    auto& node = schema.node("node", 0);
    REQUIRE_FALSE(needsProcessing(schema.schema, node));
    for (size_t i = 0; i < TypeCount; ++i) {
        const auto type = static_cast<Type>(i);
        const auto expected = std::find(sensitive.begin(), sensitive.end(), type) != sensitive.end();
        CAPTURE(type);
        REQUIRE(relocationSensitive(type) == expected);
        schema.field(node, 0, type);
        REQUIRE(needsProcessing(schema.schema, node) == expected);
        Context context{nullptr, &schema.schema};
        REQUIRE(needsProcessing(StructureRef{&context, &node, nullptr}) == expected);
    }
    REQUIRE_FALSE(needsProcessing(StructureRef{}));
    Context noSchema{};
    REQUIRE_FALSE(needsProcessing(StructureRef{&noSchema, &node, nullptr}));
    node.fields[0] = "missing";
    REQUIRE_THROWS_AS(needsProcessing(schema.schema, node), std::out_of_range);
}

TEST_CASE("Process C relocates scalar blocks without changing their payloads", "[inject_tags]") {
    Fixture host, guest;
    Schemas schema;
    auto& root = schema.node("root", sizeof(BlockPointer), GroupId_Weapon);
    schema.field(root, 0, Type::BlockPointer, "child");
    auto& child = schema.node("child", 4);
    schema.field(child, 0, Type::U32);
    SECTION("scalar elements") {}
    SECTION("embedded scalar elements") {
        schema.field(child, 0, Type::Structure, "scalar");
        auto& scalar = schema.node("scalar", 4);
        schema.field(scalar, 0, Type::U32);
    }
    auto h = host.allocate(sizeof(BlockPointer)), hp = host.allocate(8);
    auto g = guest.allocate(sizeof(BlockPointer)), gp = guest.allocate(8);
    host.add(GroupId_Weapon, "host", h);
    guest.add(GroupId_Weapon, "guest", g);
    put(host.bytes, h, BlockPointer{2, {host.pointer(hp)}, 0});
    put(guest.bytes, g, BlockPointer{2, {guest.pointer(gp)}, 0});
    const std::array<uint32_t, 2> hostData{0x11223344, 0x55667788}, guestData{0xAABBCCDD, 0xEEFF0011};
    put(host.bytes, hp, hostData);
    put(guest.bytes, gp, guestData);
    const auto originalHost = host.bytes, originalGuest = guest.bytes;
    auto output = injectTags(host.bytes, guest.bytes, {{GroupId_Weapon, "guest"}}, schema.schema);
    for (uint32_t i = 0; i < 2; ++i) {
        auto block = get<BlockPointer>(output, file(output, tag(output, i).data.offset));
        REQUIRE(block.count == 2);
        REQUIRE(get<std::array<uint32_t, 2>>(output, file(output, block.data.offset)) == (i ? guestData : hostData));
    }
    REQUIRE(host.bytes == originalHost);
    REQUIRE(guest.bytes == originalGuest);
}

TEST_CASE("Process C warns once per uncertified type without changing output", "[inject_tags]") {
    Fixture host, guest;
    Schemas schema;
    schema.scalar();
    host.add(GroupId_Weapon, "host", host.allocate(4));
    guest.add(GroupId_Weapon, "guest", guest.allocate(4));
    TagList requested;
    SECTION("host only") {}
    SECTION("host and guest of the same type") { requested.push_back({GroupId_Weapon, "guest"}); }

    std::vector<std::string> certifiedWarnings;
    auto certified = injectTags(host.bytes, guest.bytes, requested, schema.schema, &certifiedWarnings);
    REQUIRE(certifiedWarnings.empty());

    schema.schema.structures.at("scalar").complete = false;
    std::vector<std::string> warnings = {"existing warning"};
    auto uncertified = injectTags(host.bytes, guest.bytes, requested, schema.schema, &warnings);
    REQUIRE(uncertified == certified);
    REQUIRE(warnings.size() == 2);
    REQUIRE(warnings[0] == "existing warning");
    REQUIRE_THAT(warnings[1], Catch::Matchers::ContainsSubstring("[weap]"));
    REQUIRE_THAT(warnings[1], Catch::Matchers::ContainsSubstring("not certified complete"));
}

TEST_CASE("Process C checks certification of imported dependencies but not unused types or nested nodes", "[inject_tags]") {
    Fixture host, guest;
    Schemas schema;
    auto& root = schema.node("root", 12, GroupId_Weapon);
    schema.field(root, 0, Type::BlockPointer, "child");
    auto& child = schema.node("child", 16);
    schema.field(child, 0, Type::TagReference);
    auto& dependency = schema.node("dependency", 4, GroupId_Effect);
    schema.field(dependency, 0, Type::U32);
    dependency.complete = false;
    auto& unused = schema.node("unused", 4, GroupId_Sound);
    schema.field(unused, 0, Type::U32);
    unused.complete = false;

    auto at = guest.allocate(12), childAt = guest.allocate(16);
    guest.add(GroupId_Weapon, "root", at);
    auto handle = guest.add(GroupId_Effect, "dependency", guest.allocate(4));
    guest.add(GroupId_Sound, "unused", guest.allocate(4));
    put(guest.bytes, at, BlockPointer{1, {guest.pointer(childAt)}, 0});
    put(guest.bytes, childAt, TagReference{GroupId_Effect, {}, 0, handle});
    REQUIRE(root.complete);
    REQUIRE_FALSE(child.complete);

    std::vector<std::string> warnings;
    SECTION("uncertified dependency") {
        auto output = injectTags(host.bytes, guest.bytes, {{GroupId_Weapon, "root"}}, schema.schema, &warnings);
        REQUIRE(header(output).tagCount == 2);
        REQUIRE(warnings.size() == 1);
        REQUIRE_THAT(warnings[0], Catch::Matchers::ContainsSubstring("[effe]"));
        REQUIRE_THAT(warnings[0], Catch::Matchers::ContainsSubstring("not certified complete"));
    }
    SECTION("certified dependency") {
        dependency.complete = true;
        auto output = injectTags(host.bytes, guest.bytes, {{GroupId_Weapon, "root"}}, schema.schema, &warnings);
        REQUIRE(header(output).tagCount == 2);
        REQUIRE(warnings.empty());
    }
    SECTION("failed injection leaves warnings unchanged") {
        schema.field(root, host.bytes.size(), Type::U32);
        warnings.push_back("existing warning");
        REQUIRE_THROWS_AS(
            injectTags(host.bytes, guest.bytes, {{GroupId_Weapon, "root"}}, schema.schema, &warnings),
            std::runtime_error);
        REQUIRE(warnings == std::vector<std::string>{"existing warning"});
    }
}

TEST_CASE("Process C rebuilds a zero-injection map without changing its inputs", "[inject_tags]") {
    Fixture host, guest;
    Schemas schema;
    schema.scalar();
    auto at = host.allocate(4);
    put(host.bytes, at, uint32_t(0xDEADBEEF));
    host.add(GroupId_Weapon, "host", at);
    auto original = host.bytes;
    auto originalGuest = guest.bytes;

    auto output = injectTags(host.bytes, guest.bytes, {}, schema.schema);
    REQUIRE(host.bytes == original);
    REQUIRE(guest.bytes == originalGuest);
    REQUIRE(output != original);
    REQUIRE(header(output).tagCount == 1);
    REQUIRE(header(output).tagArray.offset == base + sizeof(TagDataHeader));
    REQUIRE(path(output, tag(output, 0).path.offset) == "host");
    REQUIRE(get<uint32_t>(output, file(output, tag(output, 0).data.offset)) == 0xDEADBEEF);
    auto cache = get<CacheHeader>(output, 0);
    REQUIRE(cache.fileSize == output.size());
    REQUIRE(cache.tagData.offset + cache.tagDataSize == output.size());
    REQUIRE(cache.checksum == 0x12345678);
    REQUIRE(header(output).checksum == 0x87654321);
    REQUIRE(std::equal(original.begin() + 0x800, original.begin() + 0x980, output.begin() + 0x800));
    auto again = injectTags(host.bytes, guest.bytes, {}, schema.schema);
    REQUIRE(again == output);
    auto listed = listInjectionTags(output);
    REQUIRE(listed.size() == 1);
    REQUIRE(listed[0].groupId == GroupId_Weapon);
    REQUIRE(listed[0].path == "host");
}

TEST_CASE("Process C closes cyclic dependencies and relocates blocks names and inline data", "[inject_tags]") {
    Fixture host, guest;
    Schemas schema;
    auto& root = schema.node("root", 48, GroupId_Weapon);
    schema.field(root, 0, Type::TagReference);
    schema.field(root, 16, Type::BlockPointer, "child");
    schema.field(root, 28, Type::DataPointer);
    auto& child = schema.node("child", 16);
    schema.field(child, 0, Type::TagReference);
    auto h = host.allocate(48);
    put(host.bytes, h, TagReference{GroupId_Weapon, {}, 0, invalid});
    host.add(GroupId_Weapon, "host", h);
    auto a = guest.allocate(48), b = guest.allocate(48);
    auto childAt = guest.allocate(16), payload = guest.allocate(4);
    auto ah = guest.add(GroupId_Weapon, "a", a, 0x12340000);
    auto bh = guest.add(GroupId_Weapon, "b", b, 0x56780000);
    put(guest.bytes, a, TagReference{GroupId_Weapon, {tag(guest.bytes, 1).path.offset}, 1, bh});
    put(guest.bytes, b, TagReference{GroupId_Weapon, {tag(guest.bytes, 0).path.offset}, 1, ah});
    put(guest.bytes, a + 16, BlockPointer{1, {guest.pointer(childAt)}, 0});
    put(guest.bytes, childAt, TagReference{GroupId_Weapon, {tag(guest.bytes, 1).path.offset}, 1, bh});
    put(guest.bytes, a + 28, uint32_t(4));
    put(guest.bytes, a + 40, guest.pointer(payload));
    put(guest.bytes, payload, uint32_t(0xABCDEF12));
    auto original = guest.bytes;

    auto output = injectTags(host.bytes, guest.bytes, {{GroupId_Weapon, "a"}}, schema.schema);
    REQUIRE(guest.bytes == original);
    REQUIRE(header(output).tagCount == 3);
    REQUIRE(tag(output, 1).tagHandle == 0x12340001);
    REQUIRE(tag(output, 2).tagHandle == 0x56780002);
    auto destA = file(output, tag(output, 1).data.offset);
    auto destB = file(output, tag(output, 2).data.offset);
    auto ref = get<TagReference>(output, destA);
    REQUIRE(ref.tagHandle == tag(output, 2).tagHandle);
    REQUIRE(path(output, ref.path.offset) == "b");
    REQUIRE(get<TagReference>(output, destB).tagHandle == tag(output, 1).tagHandle);
    auto block = get<BlockPointer>(output, destA + 16);
    REQUIRE(get<TagReference>(output, file(output, block.data.offset)).tagHandle == tag(output, 2).tagHandle);
    REQUIRE(path(output, get<TagReference>(output, file(output, block.data.offset)).path.offset) == "b");
    auto data = get<uint32_t>(output, destA + 40);
    REQUIRE(get<uint32_t>(output, file(output, data)) == 0xABCDEF12);
    auto rebuilt = injectTags(output, guest.bytes, {}, schema.schema);
    auto rebuiltA = file(rebuilt, tag(rebuilt, 1).data.offset);
    REQUIRE(get<TagReference>(rebuilt, rebuiltA).tagHandle == tag(rebuilt, 2).tagHandle);
    REQUIRE(path(rebuilt, get<TagReference>(rebuilt, rebuiltA).path.offset) == "b");
    REQUIRE(get<uint32_t>(rebuilt, file(rebuilt, get<uint32_t>(rebuilt, rebuiltA + 40))) == 0xABCDEF12);
}

TEST_CASE("Process C reuses host identities and deduplicates requests", "[inject_tags]") {
    Fixture host, guest;
    Schemas schema;
    schema.scalar();
    auto h = host.allocate(4), g = guest.allocate(4);
    put(host.bytes, h, uint32_t(10));
    put(guest.bytes, g, uint32_t(20));
    host.add(GroupId_Weapon, "shared", h, 0x12340000);
    guest.add(GroupId_Weapon, "shared", g);
    auto output = injectTags(host.bytes, guest.bytes,
        {{GroupId_Weapon, "shared"}, {GroupId_Weapon, "shared"}}, schema.schema);
    REQUIRE(header(output).tagCount == 1);
    REQUIRE(tag(output, 0).tagHandle == 0x12340000);
    REQUIRE(get<uint32_t>(output, file(output, tag(output, 0).data.offset)) == 10);
}

TEST_CASE("Process C copies bitmap bytes outside the host fixed prefix", "[inject_tags]") {
    Fixture host, guest;
    Schemas schema;
    schema.bitmap();
    auto root = guest.allocate(sizeof(Bitmap)), dataAt = guest.allocate(sizeof(BitmapData));
    Bitmap bitmap{};
    bitmap.bitmapData = {1, {guest.pointer(dataAt)}, 0};
    put(guest.bytes, root, bitmap);
    BitmapData data{};
    data.data.offset = 0x840;
    data.dataSize = 7;
    data.bitmapTagHandle = 0xEEEE0000;
    put(guest.bytes, dataAt, data);
    guest.add(GroupId_Bitmap, "bitmap", root);
    std::fill_n(guest.bytes.begin() + 0x840, 7, uint8_t(0xBC));

    auto output = injectTags(host.bytes, guest.bytes, {{GroupId_Bitmap, "bitmap"}}, schema.schema);
    auto copied = get<Bitmap>(output, file(output, tag(output, 0).data.offset));
    auto copiedData = get<BitmapData>(output, file(output, copied.bitmapData.data.offset));
    REQUIRE(copiedData.data.offset == 0x980);
    REQUIRE(copiedData.bitmapTagHandle == tag(output, 0).tagHandle);
    REQUIRE(std::equal(guest.bytes.begin() + 0x840, guest.bytes.begin() + 0x847,
        output.begin() + copiedData.data.offset));
    REQUIRE(header(output).modelData.offset == 0x988);
    REQUIRE(std::equal(host.bytes.begin() + 0x980, host.bytes.begin() + 0x9E0,
        output.begin() + header(output).modelData.offset));
}

TEST_CASE("Process C relocates guest model offsets in separate vertex and index buffers", "[inject_tags]") {
    Fixture host, guest;
    Schemas schema;
    schema.model();
    auto root = guest.allocate(sizeof(Model));
    auto geomAt = guest.allocate(sizeof(Geometry)), partAt = guest.allocate(sizeof(Part));
    Model model{};
    model.geometries = {1, {guest.pointer(geomAt)}, 0};
    put(guest.bytes, root, model);
    Geometry geometry{};
    geometry.parts = {1, {guest.pointer(partAt)}, 0};
    put(guest.bytes, geomAt, geometry);
    Part part{};
    part.vertexFormat = 3;
    part.vertexCount = 2;
    part.union0.vertices.offset = 8;
    part.indices.offset = 4;
    part.alsoIndices.offset = 4;
    part.indexCount = 3;
    SECTION("mirrored index offsets") {}
    SECTION("distinct index offsets are relocated independently") { part.alsoIndices.offset = 8; }
    put(guest.bytes, partAt, part);
    guest.add(GroupId_Model, "model", root);
    std::fill_n(guest.bytes.begin() + 0x980, 0x60, uint8_t(0xBD));

    auto output = injectTags(host.bytes, guest.bytes, {{GroupId_Model, "model"}}, schema.schema);
    auto mh = header(output);
    REQUIRE(mh.vertexDataSize == 0x80);
    REQUIRE(mh.modelDataSize == 0xC0);
    REQUIRE(mh.modelPartCount == 1);
    REQUIRE(mh.modelPartCount2 == 1);
    auto copied = get<Model>(output, file(output, tag(output, 0).data.offset));
    auto geom = get<Geometry>(output, file(output, copied.geometries.data.offset));
    auto dest = get<Part>(output, file(output, geom.parts.data.offset));
    REQUIRE(dest.union0.vertices.offset == 0x48);
    REQUIRE(dest.indices.offset == 0x24);
    REQUIRE(dest.alsoIndices.offset == part.alsoIndices.offset + 0x20);
    REQUIRE(output[mh.modelData.offset + dest.union0.vertices.offset] == 0xBD);
    REQUIRE(output[mh.modelData.offset + mh.vertexDataSize + dest.indices.offset] == 0xBD);
    REQUIRE(output[mh.modelData.offset + mh.vertexDataSize + dest.alsoIndices.offset] == 0xBD);
    REQUIRE(std::equal(host.bytes.begin() + 0x9C0, host.bytes.begin() + 0x9E0,
        output.begin() + mh.modelData.offset + mh.vertexDataSize));
}

TEST_CASE("Process C preserves BSP-local pointers while relinking BSP names", "[inject_tags]") {
    Fixture host, guest;
    Schemas schema;
    auto& scenarioSchema = schema.node("scenario", sizeof(Scenario), GroupId_Scenario);
    schema.padding(scenarioSchema, 0, 0x5A4);
    schema.field(scenarioSchema, 0x5A4, Type::BlockPointer, "bsp-ref");
    auto& refSchema = schema.node("bsp-ref", sizeof(StructureBspReference));
    schema.padding(refSchema, 0, 16);
    schema.field(refSchema, 16, Type::TagReference);
    auto schemaPath = std::filesystem::path(SPARK_TEST_DIR).parent_path()
        / "spark" / "shipfiles" / "tags.schema2.json";
    std::ifstream schemaFile(schemaPath);
    REQUIRE(schemaFile.is_open());
    auto bundled = Schema::fromJsonString(std::string(std::istreambuf_iterator<char>(schemaFile), {}));
    auto* bundledBsp = bundled.structureForGroupId(GroupId_BSP);
    REQUIRE(bundledBsp != nullptr);
    const auto& decalsField = bundled.fields.at(bundledBsp->fields.at(0x258));
    REQUIRE(decalsField.type.type == Type::BlockPointer);
    const auto& decals = bundled.structures.at(decalsField.type.ref);
    REQUIRE(decals.size == 16);
    schema.schema.structures.emplace(decals.id, decals);
    for (const auto& [offset, id] : decals.fields)
        schema.schema.fields.emplace(id, bundled.fields.at(id));

    auto& bspSchema = schema.node("bsp", 40, GroupId_BSP);
    schema.field(bspSchema, 0, Type::TagReference);
    schema.field(bspSchema, 16, Type::BlockPointer, "bsp-child");
    schema.field(bspSchema, 28, Type::BlockPointer, decals.id.c_str());
    auto& child = schema.node("bsp-child", 4);
    schema.field(child, 0, Type::U32);
    auto scenarioAt = host.allocate(sizeof(Scenario));
    auto refAt = host.allocate(sizeof(StructureBspReference));
    auto scenarioHandle = host.add(GroupId_Scenario, "scenario", scenarioAt);
    auto bspHandle = host.add(GroupId_BSP, "bsp", 0);
    auto th = header(host.bytes);
    th.scenarioId = scenarioHandle;
    put(host.bytes, Fixture::tagsAt, th);
    Scenario scenario{};
    scenario.structureBsps = {1, {host.pointer(refAt)}, 0};
    put(host.bytes, scenarioAt, scenario);
    StructureBspReference ref{};
    ref.structureBspHeader.offset = 0x800;
    ref.runtimeAddress.offset = 0x51000000;
    ref.size = 0x180;
    ref.tagReference = {GroupId_BSP, {tag(host.bytes, 1).path.offset}, 3, bspHandle};
    put(host.bytes, refAt, ref);
    StructureBspHeader bh{};
    bh.structureBsp.offset = 0x51000000 + sizeof(StructureBspHeader);
    std::memcpy(bh.magic, "psbs", 4);
    put(host.bytes, 0x800, bh);
    put(host.bytes, 0x818, TagReference{GroupId_Scenario, {tag(host.bytes, 0).path.offset}, 8, scenarioHandle});
    put(host.bytes, 0x828, BlockPointer{1, {0x51000060}, 0});
    put(host.bytes, 0x860, uint32_t(123));
    put(host.bytes, 0x834, BlockPointer{8, {0x51000100}, 0});
    const auto original = host.bytes;

    SECTION("decals end exactly at the BSP section boundary") {}
    SECTION("decals extending past the BSP section are rejected") {
        put(host.bytes, 0x834, BlockPointer{9, {0x51000100}, 0});
        const auto invalidHost = host.bytes;
        REQUIRE_THROWS_WITH(injectTags(host.bytes, guest.bytes, {}, schema.schema),
            Catch::Matchers::ContainsSubstring("range outside its section"));
        REQUIRE(host.bytes == invalidHost);
        return;
    }

    auto output = injectTags(host.bytes, guest.bytes, {}, schema.schema);
    REQUIRE(tag(output, 1).data.offset == 0);
    REQUIRE(get<StructureBspHeader>(output, 0x800).structureBsp.offset == bh.structureBsp.offset);
    REQUIRE(get<BlockPointer>(output, 0x828).data.offset == 0x51000060);
    REQUIRE(get<uint32_t>(output, 0x860) == 123);
    REQUIRE(get<BlockPointer>(output, 0x834).data.offset == 0x51000100);
    REQUIRE(std::equal(original.begin() + 0x900, original.begin() + 0x980, output.begin() + 0x900));
    REQUIRE(host.bytes == original);
    auto copiedRef = get<TagReference>(output, 0x818);
    REQUIRE(path(output, copiedRef.path.offset) == "scenario");
    REQUIRE(copiedRef.path.offset != tag(host.bytes, 0).path.offset);
}

TEST_CASE("Process C rejects invalid and unsupported inputs atomically", "[inject_tags]") {
    Fixture host, guest;
    Schemas schema;
    schema.scalar();
    auto at = host.allocate(4);
    host.add(GroupId_Weapon, "host", at);
    TagList requested;

    SECTION("truncated input") { host.bytes.resize(20); }
    SECTION("wrong magic") { host.bytes[0] = 0; }
    SECTION("mismatched file size") { host.bytes.pop_back(); }
    SECTION("missing group schema") { schema.schema.structures.clear(); }
    SECTION("empty root schema") { schema.schema.structures.at("scalar").fields.clear(); }
    SECTION("invalid metadata pointer") {
        auto entry = tag(host.bytes, 0);
        entry.data.offset = 1;
        put(host.bytes, Fixture::tagsAt + sizeof(TagDataHeader), entry);
    }
    SECTION("missing requested tag") { requested.push_back({GroupId_Weapon, "absent"}); }
    SECTION("null requested name") { requested.push_back({GroupId_Weapon, nullptr}); }
    SECTION("version mismatch") {
        auto cache = get<CacheHeader>(guest.bytes, 0);
        ++cache.cacheVersion;
        put(guest.bytes, 0, cache);
    }
    SECTION("model size underflow") {
        auto th = header(host.bytes);
        th.vertexDataSize = th.modelDataSize + 1;
        put(host.bytes, Fixture::tagsAt, th);
    }
    SECTION("unexpected data after models") {
        auto th = header(host.bytes);
        th.modelDataSize -= 4;
        put(host.bytes, Fixture::tagsAt, th);
    }
    SECTION("nonzero cache padding") {
        auto cache = get<CacheHeader>(host.bytes, 0);
        cache.paddingLength = 4;
        put(host.bytes, 0, cache);
    }
    SECTION("inline DataPointer outside metadata") {
        schema.schema.structures.clear();
        auto& node = schema.node("data", 20, GroupId_Weapon);
        schema.field(node, 0, Type::DataPointer);
        put(host.bytes, at, uint32_t(4));
        put(host.bytes, at + 8, uint32_t(0x980));
        put(host.bytes, at + 12, uint32_t(0xFFFFFFFF));
    }
    SECTION("nonzero DataPointer padding does not hide an invalid metadata pointer") {
        schema.schema.structures.clear();
        auto& node = schema.node("data", 20, GroupId_Weapon);
        schema.field(node, 0, Type::DataPointer);
        put(host.bytes, at, std::array<uint32_t, 5>{4, 0xE18D0019, 0, 0xDEADBEEF, 0});
    }
    SECTION("negative DataPointer size") {
        schema.schema.structures.clear();
        auto& node = schema.node("data", 20, GroupId_Weapon);
        schema.field(node, 0, Type::DataPointer);
        put(host.bytes, at, int32_t(-1));
    }
    SECTION("oversized block") {
        schema.schema.structures.clear();
        auto& node = schema.node("block", 12, GroupId_Weapon);
        schema.field(node, 0, Type::BlockPointer, "block");
        put(host.bytes, at, BlockPointer{0xFFFFFFFF, {host.pointer(at)}, 0});
    }
    SECTION("stale reference salt") {
        schema.schema.structures.clear();
        auto& node = schema.node("ref", 16, GroupId_Weapon);
        schema.field(node, 0, Type::TagReference);
        auto guestData = guest.allocate(16);
        guest.add(GroupId_Weapon, "bad", guestData);
        put(guest.bytes, guestData, TagReference{GroupId_Weapon, {}, 0, 0x12340000});
        requested.push_back({GroupId_Weapon, "bad"});
    }
    SECTION("unterminated name") {
        auto entry = tag(host.bytes, 0);
        entry.path.offset = base + static_cast<uint32_t>(host.bytes.size()) - Fixture::tagsAt - 1;
        host.bytes.back() = 1;
        put(host.bytes, Fixture::tagsAt + sizeof(TagDataHeader), entry);
    }
    SECTION("guest scenario") {
        guest.add(GroupId_Scenario, "scenario", guest.allocate(4));
        requested.push_back({GroupId_Scenario, "scenario"});
    }
    auto originalHost = host.bytes, originalGuest = guest.bytes;
    REQUIRE_THROWS_MATCHES(injectTags(host.bytes, guest.bytes, requested, schema.schema),
        std::runtime_error, Catch::Matchers::MessageMatches(Catch::Matchers::StartsWith("injectTags:")));
    REQUIRE(host.bytes == originalHost);
    REQUIRE(guest.bytes == originalGuest);
}

TEST_CASE("Injection tag listing rejects malformed raw maps before UI access", "[inject_tags]") {
    Fixture map;
    auto at = map.allocate(4);
    map.add(GroupId_Weapon, "tag", at);
    SECTION("truncated header") { map.bytes.resize(10); }
    SECTION("invalid tag array") {
        auto h = header(map.bytes);
        h.tagArray.offset = 0xFFFFFFFF;
        put(map.bytes, Fixture::tagsAt, h);
    }
    REQUIRE_THROWS_AS(listInjectionTags(map.bytes), std::runtime_error);
}

TEST_CASE("Process C preserves shared blocks and empty-pointer sentinels", "[inject_tags]") {
    Fixture host, guest;
    Schemas schema;
    auto& root = schema.node("blocks", 48, GroupId_Weapon);
    for (size_t at = 0; at < 48; at += 12) schema.field(root, at, Type::BlockPointer, "child");
    auto& child = schema.node("child", 16);
    schema.field(child, 0, Type::TagReference);
    auto h = host.allocate(48), childAt = host.allocate(16);
    auto handle = host.add(GroupId_Weapon, "host", h);
    put(host.bytes, h, BlockPointer{1, {host.pointer(childAt)}, 0});
    put(host.bytes, h + 12, BlockPointer{1, {host.pointer(childAt)}, 0});
    put(host.bytes, h + 24, BlockPointer{0, {0}, 0});
    put(host.bytes, h + 36, BlockPointer{0, {0xFFFFFFFF}, 0});
    put(host.bytes, childAt, TagReference{GroupId_Weapon, {tag(host.bytes, 0).path.offset}, 4, handle});

    auto output = injectTags(host.bytes, guest.bytes, {}, schema.schema);
    auto at = file(output, tag(output, 0).data.offset);
    auto first = get<BlockPointer>(output, at), second = get<BlockPointer>(output, at + 12);
    REQUIRE(first.data.offset == second.data.offset);
    REQUIRE(get<BlockPointer>(output, at + 24).data.offset == 0);
    REQUIRE(get<BlockPointer>(output, at + 36).data.offset == 0xFFFFFFFF);
    auto reference = get<TagReference>(output, file(output, first.data.offset));
    REQUIRE(reference.tagHandle == handle);
    REQUIRE(path(output, reference.path.offset) == "host");
}

TEST_CASE("Process C binds guest dependencies to host handles rather than guest indices", "[inject_tags]") {
    Fixture host, guest;
    Schemas schema;
    auto& root = schema.node("ref", 16, GroupId_Weapon);
    schema.field(root, 0, Type::TagReference);
    auto h = host.allocate(16), a = guest.allocate(16), b = guest.allocate(16);
    auto hostHandle = host.add(GroupId_Weapon, "shared", h, 0x11110000);
    put(host.bytes, h, TagReference{GroupId_Weapon, {}, 0, invalid});
    guest.add(GroupId_Weapon, "new", a);
    auto guestHandle = guest.add(GroupId_Weapon, "shared", b, 0x22220000);
    put(guest.bytes, a, TagReference{GroupId_Weapon, {tag(guest.bytes, 1).path.offset}, 6, guestHandle});
    // The guest copy is not reachable after identity binding and need not be walked.
    put(guest.bytes, b, TagReference{GroupId_Weapon, {}, 0, 0x33330000});
    auto output = injectTags(host.bytes, guest.bytes, {{GroupId_Weapon, "new"}}, schema.schema);
    REQUIRE(header(output).tagCount == 2);
    auto reference = get<TagReference>(output, file(output, tag(output, 1).data.offset));
    REQUIRE(reference.tagHandle == hostHandle);
    REQUIRE(reference.path.offset == tag(output, 0).path.offset);
}

TEST_CASE("Process C rejects conflicting interpretations of shared metadata", "[inject_tags]") {
    Fixture host, guest;
    Schemas schema;
    auto& root = schema.node("root", 24, GroupId_Weapon);
    schema.field(root, 0, Type::BlockPointer, "ref");
    schema.field(root, 12, Type::BlockPointer, "block");
    auto& ref = schema.node("ref", 16);
    schema.field(ref, 0, Type::TagReference);
    auto& block = schema.node("block", 12);
    schema.field(block, 0, Type::BlockPointer, "scalar");
    auto& scalar = schema.node("scalar", 1);
    schema.field(scalar, 0, Type::U8);
    auto at = host.allocate(24), shared = host.allocate(16);
    host.add(GroupId_Weapon, "host", at);
    put(host.bytes, at, BlockPointer{1, {host.pointer(shared)}, 0});
    put(host.bytes, at + 12, BlockPointer{1, {host.pointer(shared)}, 0});
    put(host.bytes, shared, TagReference{1, {tag(host.bytes, 0).path.offset}, 0, invalid});
    REQUIRE_THROWS_WITH(injectTags(host.bytes, guest.bytes, {}, schema.schema),
        Catch::Matchers::StartsWith("injectTags: conflicting aliased fixups"));
}

TEST_CASE("Process C fails closed on out-of-range internal bitmap payloads", "[inject_tags]") {
    Fixture host, guest;
    Schemas schema;
    schema.bitmap();
    auto root = guest.allocate(sizeof(Bitmap)), dataAt = guest.allocate(sizeof(BitmapData));
    Bitmap bitmap{};
    bitmap.bitmapData = {1, {guest.pointer(dataAt)}, 0};
    put(guest.bytes, root, bitmap);
    BitmapData data{};
    data.data.offset = 0x840;
    data.dataSize = 8;
    SECTION("overlaps moved model buffer") { data.data.offset = 0x97F; }
    SECTION("offset overflow") { data.data.offset = 0xFFFFFFFE; }
    SECTION("size overflow") { data.dataSize = 0xFFFFFFFF; }
    put(guest.bytes, dataAt, data);
    guest.add(GroupId_Bitmap, "bitmap", root);
    auto original = guest.bytes;
    REQUIRE_THROWS_AS(injectTags(host.bytes, guest.bytes, {{GroupId_Bitmap, "bitmap"}}, schema.schema),
        std::runtime_error);
    REQUIRE(guest.bytes == original);
}

TEST_CASE("Process C preserves host external bitmaps while injecting internal guest pixels", "[inject_tags]") {
    Fixture host, guest;
    Schemas schema;
    schema.bitmap();
    auto hostRoot = host.allocate(sizeof(Bitmap));
    auto hostData = host.allocate(2 * sizeof(BitmapData));
    Bitmap hostBitmap{};
    hostBitmap.bitmapData = {2, {host.pointer(hostData)}, 0};
    put(host.bytes, hostRoot, hostBitmap);
    host.add(GroupId_Bitmap, "host", hostRoot);
    BitmapData external{};
    external.flags = 0x183;
    external.data.offset = 0x7A5711B;
    external.dataSize = 349552;
    external.bitmapTagHandle = 0xE1170000;
    external.textureCacheHandle = 0xFFFFFFFF;
    external.bufferIndex = 7;
    put(host.bytes, hostData, external);
    BitmapData internal{};
    internal.data.offset = 0x840;
    internal.dataSize = 8;
    put(host.bytes, hostData + sizeof(BitmapData), internal);
    auto guestRoot = guest.allocate(sizeof(Bitmap));
    auto guestData = guest.allocate(sizeof(BitmapData));
    Bitmap guestBitmap{};
    guestBitmap.bitmapData = {1, {guest.pointer(guestData)}, 0};
    put(guest.bytes, guestRoot, guestBitmap);
    put(guest.bytes, guestData, internal);
    guest.add(GroupId_Bitmap, "guest", guestRoot);
    std::fill_n(guest.bytes.begin() + 0x840, 8, uint8_t(0xBC));
    auto original = host.bytes;

    std::vector<std::string> warnings;
    auto output = injectTags(host.bytes, guest.bytes, {{GroupId_Bitmap, "guest"}}, schema.schema, &warnings);
    REQUIRE(warnings.empty());
    REQUIRE(host.bytes == original);
    auto copiedHost = get<Bitmap>(output, file(output, tag(output, 0).data.offset));
    REQUIRE(copiedHost.bitmapData.data.offset != hostBitmap.bitmapData.data.offset);
    auto copiedRecords = file(output, copiedHost.bitmapData.data.offset);
    REQUIRE(std::memcmp(output.data() + copiedRecords, &external, sizeof(external)) == 0);
    REQUIRE(std::memcmp(output.data() + copiedRecords + sizeof(BitmapData), &internal, sizeof(internal)) == 0);
    REQUIRE(external.data.offset > output.size());
    REQUIRE(std::equal(original.begin() + 0x840, original.begin() + 0x848, output.begin() + 0x840));
    auto copiedGuest = get<Bitmap>(output, file(output, tag(output, 1).data.offset));
    auto guestRecord = get<BitmapData>(output, file(output, copiedGuest.bitmapData.data.offset));
    REQUIRE(guestRecord.bitmapTagHandle == tag(output, 1).tagHandle);
    REQUIRE(guestRecord.data.offset == 0x980);
    REQUIRE(std::equal(guest.bytes.begin() + 0x840, guest.bytes.begin() + 0x848,
        output.begin() + guestRecord.data.offset));
}

TEST_CASE("Process C imports external guest bitmaps with one shared-cache warning", "[inject_tags]") {
    Fixture host, guest;
    Schemas schema;
    schema.bitmap();
    host.add(GroupId_Bitmap, "host", host.allocate(sizeof(Bitmap)));
    auto root = guest.allocate(sizeof(Bitmap)), dataAt = guest.allocate(3 * sizeof(BitmapData));
    Bitmap bitmap{};
    bitmap.bitmapData = {3, {guest.pointer(dataAt)}, 0};
    put(guest.bytes, root, bitmap);
    auto guestHandle = guest.add(GroupId_Bitmap, "external", root);
    BitmapData external{};
    external.flags = 0x183;
    external.data.offset = 0x7A5711B;
    external.dataSize = 349552;
    external.bitmapTagHandle = guestHandle;
    external.textureCacheHandle = 0xFFFFFFFF;
    put(guest.bytes, dataAt, external);
    auto empty = external;
    empty.dataSize = 0;
    put(guest.bytes, dataAt + sizeof(BitmapData), empty);
    BitmapData internal{};
    internal.data.offset = 0x840;
    internal.dataSize = 8;
    put(guest.bytes, dataAt + 2 * sizeof(BitmapData), internal);
    std::vector<std::string> warnings;
    auto original = guest.bytes;

    SECTION("successful import") {
        auto output = injectTags(host.bytes, guest.bytes, {{GroupId_Bitmap, "external"}}, schema.schema, &warnings);
        REQUIRE(warnings.size() == 1);
        REQUIRE_THAT(warnings[0], Catch::Matchers::ContainsSubstring("same shared bitmap cache"));
        REQUIRE_THAT(warnings[0], Catch::Matchers::ContainsSubstring("has not been verified"));
        auto imported = tag(output, 1);
        REQUIRE(imported.tagHandle != guestHandle);
        auto dest = get<Bitmap>(output, file(output, imported.data.offset));
        auto records = file(output, dest.bitmapData.data.offset);
        external.bitmapTagHandle = imported.tagHandle;
        empty.bitmapTagHandle = imported.tagHandle;
        REQUIRE(std::memcmp(output.data() + records, &external, sizeof(external)) == 0);
        REQUIRE(std::memcmp(output.data() + records + sizeof(BitmapData), &empty, sizeof(empty)) == 0);
        auto copiedInternal = get<BitmapData>(output, records + 2 * sizeof(BitmapData));
        REQUIRE(copiedInternal.bitmapTagHandle == imported.tagHandle);
        REQUIRE(copiedInternal.data.offset == 0x980);
        REQUIRE(header(output).modelData.offset == 0x988);
        REQUIRE(guest.bytes == original);
    }
    SECTION("failed import does not publish success warnings") {
        internal.data.offset = 0xFFFFFFFF;
        put(guest.bytes, dataAt + 2 * sizeof(BitmapData), internal);
        REQUIRE_THROWS_AS(injectTags(host.bytes, guest.bytes, {{GroupId_Bitmap, "external"}}, schema.schema, &warnings),
            std::runtime_error);
        REQUIRE(warnings.empty());
    }
}

TEST_CASE("Process C handles an empty tag set and rejects an exhausted handle table", "[inject_tags]") {
    Fixture host, guest;
    Schemas schema;
    SECTION("empty map") {
        auto output = injectTags(host.bytes, guest.bytes, {}, schema.schema);
        REQUIRE(header(output).tagCount == 0);
        REQUIRE(get<CacheHeader>(output, 0).fileSize == output.size());
    }

    SECTION("over-capacity count") {
        auto h = header(host.bytes);
        h.tagCount = 0x10000;
        put(host.bytes, Fixture::tagsAt, h);
        REQUIRE_THROWS_WITH(injectTags(host.bytes, guest.bytes, {}, schema.schema),
            "injectTags: tag count exceeds handle index capacity");
    }
}

TEST_CASE("Process C still bounds-checks internal host bitmap pixels", "[inject_tags]") {
    Fixture host, guest;
    Schemas schema;
    schema.bitmap();
    auto root = host.allocate(sizeof(Bitmap)), dataAt = host.allocate(sizeof(BitmapData));
    Bitmap bitmap{};
    bitmap.bitmapData = {1, {host.pointer(dataAt)}, 0};
    put(host.bytes, root, bitmap);
    BitmapData data{};
    data.data.offset = 0x7A5711B;
    data.dataSize = 349552;
    put(host.bytes, dataAt, data);
    host.add(GroupId_Bitmap, "internal", root);
    REQUIRE_THROWS_WITH(injectTags(host.bytes, guest.bytes, {}, schema.schema),
        Catch::Matchers::StartsWith("injectTags: range outside its section"));
}

TEST_CASE("Process C validates model formats and buffer extents before copying", "[inject_tags]") {
    Fixture host, guest;
    Schemas schema;
    schema.model();
    auto root = guest.allocate(sizeof(Model));
    auto geomAt = guest.allocate(sizeof(Geometry)), partAt = guest.allocate(sizeof(Part));
    Model model{};
    model.geometries = {1, {guest.pointer(geomAt)}, 0};
    put(guest.bytes, root, model);
    Geometry geometry{};
    geometry.parts = {1, {guest.pointer(partAt)}, 0};
    put(guest.bytes, geomAt, geometry);
    Part part{};
    part.vertexFormat = 3;
    part.vertexCount = 1;
    part.indexCount = 1;
    SECTION("invalid vertex format") { part.vertexFormat = 20; }
    SECTION("unknown vertex size") { part.vertexFormat = 18; }
    SECTION("vertex extent overflow") { part.vertexCount = 0xFFFFFFFF; }
    SECTION("index extent overflow") { part.indexCount = 0xFFFFFFFF; }
    SECTION("vertex buffer overflow") { part.union0.vertices.offset = 0xFFFFFFFF; }
    SECTION("index buffer overflow") { part.indices.offset = 0xFFFFFFFF; }
    SECTION("secondary index buffer overflow") { part.alsoIndices.offset = 0xFFFFFFFF; }
    SECTION("runtime vertex pointer") { part.runtimeVertices.offset = 1; }
    put(guest.bytes, partAt, part);
    guest.add(GroupId_Model, "model", root);
    REQUIRE_THROWS_AS(injectTags(host.bytes, guest.bytes, {{GroupId_Model, "model"}}, schema.schema),
        std::runtime_error);
}

TEST_CASE("Process C remaps empty bitmap records without allocating pixel bytes", "[inject_tags]") {
    Fixture host, guest;
    Schemas schema;
    schema.bitmap();
    auto h = host.allocate(sizeof(Bitmap));
    host.add(GroupId_Bitmap, "host", h);
    auto root = guest.allocate(sizeof(Bitmap)), dataAt = guest.allocate(sizeof(BitmapData));
    Bitmap bitmap{};
    bitmap.bitmapData = {1, {guest.pointer(dataAt)}, 0};
    put(guest.bytes, root, bitmap);
    BitmapData data{};
    data.bitmapTagHandle = 0xE1170000;
    put(guest.bytes, dataAt, data);
    guest.add(GroupId_Bitmap, "empty", root);
    auto output = injectTags(host.bytes, guest.bytes, {{GroupId_Bitmap, "empty"}}, schema.schema);
    auto dest = get<Bitmap>(output, file(output, tag(output, 1).data.offset));
    auto record = get<BitmapData>(output, file(output, dest.bitmapData.data.offset));
    REQUIRE(record.bitmapTagHandle == tag(output, 1).tagHandle);
    REQUIRE(record.dataSize == 0);
    REQUIRE(record.data.offset == 0);
    REQUIRE(header(output).modelData.offset == 0x980);
}

TEST_CASE("Process C uses the bundled mapped scenario schema with an unspecified root size", "[inject_tags]") {
    auto schemaPath = std::filesystem::path(SPARK_TEST_DIR).parent_path()
        / "spark" / "shipfiles" / "tags.schema2.json";
    std::ifstream schemaFile(schemaPath);
    REQUIRE(schemaFile.is_open());
    auto schema = Schema::fromJsonString(std::string(std::istreambuf_iterator<char>(schemaFile), {}));
    auto* root = schema.structureForGroupId(GroupId_Scenario);
    REQUIRE(root != nullptr);
    // Exercise unspecified-size roots even when the saved schema has an explicit size.
    root->size = 0;
    REQUIRE_FALSE(root->fields.empty());
    Fixture host, guest;
    auto rootAt = host.allocate(sizeof(Scenario));
    auto payload = host.allocate(4);
    auto handle = host.add(GroupId_Scenario, "scenario", rootAt);
    size_t referenceOffset = 0;
    size_t dataOffset = 0;
    for (const auto& [offset, id] : root->fields) {
        const auto& field = schema.fields.at(id);
        if (field.type.type == Type::TagReference) {
            put(host.bytes, rootAt + offset, TagReference{GroupId_Scenario, {}, 0, invalid});
            referenceOffset = offset;
        }
        if (field.type.type == Type::DataPointer) dataOffset = offset;
    }
    REQUIRE(referenceOffset != 0);
    REQUIRE(dataOffset != 0);
    put(host.bytes, rootAt + referenceOffset,
        TagReference{GroupId_Scenario, {tag(host.bytes, 0).path.offset}, 8, handle});
    put(host.bytes, rootAt + dataOffset, uint32_t(4));
    put(host.bytes, rootAt + dataOffset + 12, host.pointer(payload));
    put(host.bytes, payload, uint32_t(0x1234ABCD));
    auto original = host.bytes;
    auto output = injectTags(host.bytes, guest.bytes, {}, schema);
    REQUIRE(host.bytes == original);
    auto dest = file(output, tag(output, 0).data.offset);
    auto ref = get<TagReference>(output, dest + referenceOffset);
    REQUIRE(ref.tagHandle == handle);
    REQUIRE(path(output, ref.path.offset) == "scenario");
    REQUIRE(get<uint32_t>(output, file(output, get<uint32_t>(output, dest + dataOffset + 12))) == 0x1234ABCD);
}

TEST_CASE("Process C supports sparse root and block schemas without discarding gap bytes", "[inject_tags]") {
    Fixture host, guest;
    Schemas schema;
    auto& root = schema.node("root", 0, GroupId_Weapon);
    schema.field(root, 8, Type::BlockPointer, "child");
    auto& child = schema.node("child", 24);
    schema.field(child, 4, Type::TagReference);
    auto at = guest.allocate(24), children = guest.allocate(48);
    std::fill_n(guest.bytes.begin() + at, 24, uint8_t(0xAB));
    std::fill_n(guest.bytes.begin() + children, 48, uint8_t(0xCD));
    auto handle = guest.add(GroupId_Weapon, "sparse", at);
    put(guest.bytes, at + 8, BlockPointer{2, {guest.pointer(children)}, 0});
    put(guest.bytes, children + 4, TagReference{GroupId_Weapon, {tag(guest.bytes, 0).path.offset}, 6, handle});
    put(guest.bytes, children + 28, TagReference{GroupId_Weapon, {}, 0, invalid});
    auto output = injectTags(host.bytes, guest.bytes, {{GroupId_Weapon, "sparse"}}, schema.schema);
    auto dest = file(output, tag(output, 0).data.offset);
    REQUIRE(output[dest] == 0xAB);
    REQUIRE(output[dest + 23] == 0xAB);
    auto block = get<BlockPointer>(output, dest + 8);
    auto destChildren = file(output, block.data.offset);
    REQUIRE(output[destChildren] == 0xCD);
    REQUIRE(output[destChildren + 23] == 0xCD);
    REQUIRE(output[destChildren + 47] == 0xCD);
    REQUIRE(path(output, get<TagReference>(output, destChildren + 4).path.offset) == "sparse");
    REQUIRE(get<TagReference>(output, destChildren + 28).tagHandle == invalid);
}

TEST_CASE("Process C still rejects unbounded fields overlaps and unspecified block strides", "[inject_tags]") {
    Fixture host, guest;
    Schemas schema;
    auto& root = schema.node("root", 0, GroupId_Weapon);
    schema.field(root, 8, Type::BlockPointer, "child");
    auto& child = schema.node("child", 4);
    schema.field(child, 0, Type::U32);
    auto at = host.allocate(24), data = host.allocate(4);
    host.add(GroupId_Weapon, "host", at);
    put(host.bytes, at + 8, BlockPointer{1, {host.pointer(data)}, 0});
    SECTION("unspecified block stride") { child.size = 0; }
    SECTION("scalar block field exceeds stride") { schema.field(child, 4, Type::U32); }
    SECTION("scalar block field offsets mismatch") { schema.schema.fields.at(child.fields.at(0)).offset = 1; }
    SECTION("scalar block fields overlap") { schema.field(child, 1, Type::U8); }
    SECTION("scalar block field schema is missing") { schema.schema.fields.erase(child.fields.at(0)); }
    SECTION("scalar block field type is invalid") {
        schema.schema.fields.at(child.fields.at(0)).type.type = static_cast<Type>(TypeCount);
    }
    SECTION("scalar block exceeds metadata bounds") {
        put(host.bytes, at + 8, BlockPointer{2, {host.pointer(static_cast<uint32_t>(host.bytes.size()) - 4)}, 0});
    }
    SECTION("overlapping root fields") { schema.field(root, 16, Type::U64); }
    SECTION("field exceeds declared size") { root.size = 16; }
    SECTION("root field exceeds metadata") { schema.field(root, host.bytes.size(), Type::U32); }
    SECTION("field extent overflows") { schema.field(root, std::numeric_limits<size_t>::max(), Type::U32); }
    REQUIRE_THROWS_AS(injectTags(host.bytes, guest.bytes, {}, schema.schema), std::runtime_error);
}

TEST_CASE("Process C relocates inline DataPointers regardless of padding", "[inject_tags]") {
    Fixture host, guest;
    Schemas schema;
    auto& root = schema.node("data", 20, GroupId_Weapon);
    schema.field(root, 0, Type::DataPointer);
    auto h = host.allocate(20), hp = host.allocate(4);
    auto g = guest.allocate(20), gp = guest.allocate(4);
    host.add(GroupId_Weapon, "host", h);
    guest.add(GroupId_Weapon, "guest", g);
    uint32_t padding = 0;
    SECTION("zero padding") {}
    SECTION("one is not an external-cache marker") { padding = 1; }
    SECTION("observed cyborg animation padding") { padding = 0xE18D0019; }
    SECTION("all padding bits set") { padding = 0xFFFFFFFF; }
    put(host.bytes, h, std::array<uint32_t, 5>{4, padding, 0x1234, host.pointer(hp), 0});
    put(guest.bytes, g, std::array<uint32_t, 5>{4, padding, 0x5678, guest.pointer(gp), 0});
    put(host.bytes, hp, uint32_t(0x11223344));
    put(guest.bytes, gp, uint32_t(0x55667788));
    const auto originalHost = host.bytes, originalGuest = guest.bytes;
    std::vector<std::string> warnings;
    auto output = injectTags(host.bytes, guest.bytes, {{GroupId_Weapon, "guest"}}, schema.schema, &warnings);
    for (uint32_t i = 0; i < 2; ++i) {
        const auto dest = file(output, tag(output, i).data.offset);
        const auto descriptor = get<std::array<uint32_t, 5>>(output, dest);
        REQUIRE(descriptor[0] == 4);
        REQUIRE(descriptor[1] == padding);
        REQUIRE(descriptor[2] == (i ? 0x5678 : 0x1234));
        REQUIRE(descriptor[4] == 0);
        REQUIRE(get<uint32_t>(output, file(output, descriptor[3])) == (i ? 0x55667788 : 0x11223344));
    }
    REQUIRE(warnings.empty());
    REQUIRE(host.bytes == originalHost);
    REQUIRE(guest.bytes == originalGuest);
}

TEST_CASE("Process C preserves host external data with one deferred warning", "[inject_tags]") {
    Fixture host, guest;
    Schemas schema;
    auto& root = schema.node("external", 40, GroupId_Sound);
    schema.field(root, 0, Type::DataPointer);
    schema.field(root, 20, Type::DataPointer);
    auto at = host.allocate(40);
    host.add(GroupId_Sound, "host-sound", at);
    std::array<uint32_t, 5> descriptor{4, 0, 0x820, 0, 0x12345678};
    SECTION("null metadata pointer") {}
    SECTION("nonzero padding with null metadata pointer") {
        descriptor[1] = 1;
    }
    SECTION("payload ends at fixed boundary") { descriptor[2] = 0x97C; }
    SECTION("observed shield deplete external samples") { descriptor = {24480, 1, 46049749, 0, 0}; }
    put(host.bytes, at, descriptor);
    put(host.bytes, at + 20, descriptor);
    auto original = host.bytes;
    std::vector<std::string> warnings{"existing"};
    auto output = injectTags(host.bytes, guest.bytes, {}, schema.schema, &warnings);
    REQUIRE(host.bytes == original);
    auto dest = file(output, tag(output, 0).data.offset);
    REQUIRE(std::memcmp(output.data() + dest, original.data() + at, 40) == 0);
    REQUIRE(std::equal(original.begin() + 0x800, original.begin() + 0x980, output.begin() + 0x800));
    REQUIRE(warnings.size() == 2);
    REQUIRE(warnings[0] == "existing");
    REQUIRE_THAT(warnings[1], Catch::Matchers::ContainsSubstring("preserved unchanged"));
    REQUIRE_THAT(warnings[1], Catch::Matchers::ContainsSubstring("External offsets are not validated against this map"));
}

TEST_CASE("Process C rejects guest external data without publishing host warnings", "[inject_tags]") {
    Fixture host, guest;
    Schemas schema;
    auto& root = schema.node("external", 20, GroupId_Sound);
    schema.field(root, 0, Type::DataPointer);
    std::memcpy(schema.schema.fields.at(root.fields.at(0)).name, "samples", sizeof("samples"));
    auto h = host.allocate(20), g = guest.allocate(20);
    host.add(GroupId_Sound, "host-sound", h);
    guest.add(GroupId_Sound, "guest-sound", g);
    std::array<uint32_t, 5> descriptor{4, 0, 0x820, 0, 0};
    put(host.bytes, h, descriptor);
    SECTION("null metadata pointer") {}
    SECTION("nonzero padding with null metadata pointer") { descriptor[1] = 1; }
    put(guest.bytes, g, descriptor);
    auto originalHost = host.bytes, originalGuest = guest.bytes;
    std::vector<std::string> warnings{"existing"};
    REQUIRE_THROWS_WITH(
        injectTags(host.bytes, guest.bytes, {{GroupId_Sound, "guest-sound"}}, schema.schema, &warnings),
        Catch::Matchers::ContainsSubstring("guest external DataPointer relocation is not implemented: samples") &&
        Catch::Matchers::ContainsSubstring("guest-sound"));
    REQUIRE(warnings == std::vector<std::string>{"existing"});
    REQUIRE(host.bytes == originalHost);
    REQUIRE(guest.bytes == originalGuest);
}

TEST_CASE("Process C treats host external offsets as opaque and ignores empty descriptors", "[inject_tags]") {
    Fixture host, guest;
    Schemas schema;
    auto& root = schema.node("external", 20, GroupId_Sound);
    schema.field(root, 0, Type::DataPointer);
    auto at = host.allocate(20);
    host.add(GroupId_Sound, "host-sound", at);
    std::array<uint32_t, 5> descriptor{4, 0, 0x820, 0, 0};
    bool empty = false;
    SECTION("header overlap") { descriptor[2] = 0x7FF; }
    SECTION("model overlap") { descriptor[2] = 0x97F; }
    SECTION("offset overflow") { descriptor[2] = 0xFFFFFFFE; }
    SECTION("size outside prefix") { descriptor[0] = 0x7FFFFFFF; }
    SECTION("empty host and guest") { descriptor = {0, 0xFFFFFFFF, 0xFFFFFFFF, 0, 0}; empty = true; }
    put(host.bytes, at, descriptor);
    std::vector<std::string> warnings;
    if (empty) {
        auto g = guest.allocate(20);
        guest.add(GroupId_Sound, "guest-sound", g);
        put(guest.bytes, g, descriptor);
        auto output = injectTags(host.bytes, guest.bytes, {{GroupId_Sound, "guest-sound"}}, schema.schema, &warnings);
        for (uint32_t i = 0; i < 2; ++i)
            REQUIRE(get<decltype(descriptor)>(output, file(output, tag(output, i).data.offset)) == descriptor);
        REQUIRE(warnings.empty());
    } else {
        auto output = injectTags(host.bytes, guest.bytes, {}, schema.schema, &warnings);
        REQUIRE(get<decltype(descriptor)>(output, file(output, tag(output, 0).data.offset)) == descriptor);
        REQUIRE(warnings.size() == 1);
    }
}

TEST_CASE("Process C preserves host editor references and inline data source offsets", "[inject_tags]") {
    Fixture host, guest;
    Schemas schema;
    auto& root = schema.node("root", 36, GroupId_Weapon);
    schema.field(root, 0, Type::TagReference);
    schema.field(root, 16, Type::DataPointer);
    auto at = host.allocate(36), payload = host.allocate(4);
    host.add(GroupId_Weapon, "host", at);
    put(host.bytes, at, TagReference{GroupId_Weapon, {tag(host.bytes, 0).path.offset}, 0, 0x12340000});
    put(host.bytes, at + 16, uint32_t(4));
    put(host.bytes, at + 24, uint32_t(0x61D));
    put(host.bytes, at + 28, host.pointer(payload));
    put(host.bytes, payload, uint32_t(0xABCDEF12));
    auto output = injectTags(host.bytes, guest.bytes, {}, schema.schema);
    auto dest = file(output, tag(output, 0).data.offset);
    REQUIRE(get<TagReference>(output, dest).tagHandle == 0x12340000);
    REQUIRE(path(output, get<TagReference>(output, dest).path.offset) == "host");
    REQUIRE(get<uint32_t>(output, dest + 24) == 0x61D);
    REQUIRE(get<uint32_t>(output, file(output, get<uint32_t>(output, dest + 28))) == 0xABCDEF12);
}
