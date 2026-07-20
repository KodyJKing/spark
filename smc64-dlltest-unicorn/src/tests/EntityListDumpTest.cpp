#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>

#include "TestRunner.hpp"
#include "UnicornEngine.hpp"
#include "MinidumpLoader.hpp"
#include "SharedDumpFixture.hpp"

#include "engine/entity/entity_list.hpp"

// Approval test: walks the *entire* real entity list as captured in the
// dump and writes handle/tag name/position for every live entity to a
// deterministic text file, then compares it against a checked-in "known
// good" reference (approved/EntityList.approved.txt).
//
// This is pure pointer-chasing against dump-backed emulated memory (demand-
// paged in on the fly via MinidumpLoader::ensureMapped) -- no halo1.dll code
// is executed. Reads use the *actual* Engine::EntityList/EntityRecord/
// Entity/Tag types from engine/entity/types.hpp and engine/tag.hpp directly
// (raw memcpy'd instances, never constructed/linked against their member
// function bodies) rather than duplicating their field offsets by hand, so
// this test automatically stays in sync with those already-validated
// layouts (see their static_assert offset checks). The overall traversal
// mirrors already-validated live engine code (never called directly, just
// replicated as raw reads):
//   Engine::getEntityListPointer()/getEntityArrayBase() -- entity_list.cpp
//   Engine::getTag()                                    -- tag.cpp
//   Engine::translateMapAddress()                       -- map.cpp
//
// Since the dump is a fixed, static capture, this output is expected to be
// perfectly reproducible across runs -- exactly what makes it suited to
// approval testing rather than hand-written expected values.
namespace {

// dllBase()-relative addresses of global pointers/values -- not struct
// fields, so there's no existing type to read these via (see
// entity_list.cpp / tag.cpp / map.cpp):
constexpr uint64_t kEntityListPtrOffset = 0x1C42248;
constexpr uint64_t kEntityArrayBaseOffset = 0x2D9CDF8;
constexpr uint64_t kTagArrayPtrOffset = 0x1C34FB0;
constexpr uint64_t kRelocatedMapBaseOffset = 0x2D9CE10;
constexpr uint64_t kMapBaseOffset = 0x2EA3410;

// getEntityPointer()'s "+ 0x34" adjustment (entity_list.cpp) -- an
// algorithm constant baked into that function, not a member offset of any
// struct, so it isn't something Engine::EntityList/EntityRecord's layout
// can express either.
constexpr uint64_t kEntityArrayBaseAdjust = 0x34;

// Reads a POD value from dump-backed emulated memory, paging in whatever
// captured range covers it first (direct engine.memRead() calls bypass
// Unicorn's CPU-hook path entirely, so they won't demand-page themselves).
template <typename T>
T read(UnicornEngine& engine, MinidumpLoader& dump, uint64_t address) {
    dump.ensureMapped(engine, address, sizeof(T));
    T value{};
    engine.memRead(address, &value, sizeof(T));
    return value;
}

std::string readCString(UnicornEngine& engine, MinidumpLoader& dump, uint64_t address, size_t maxLen = 256) {
    // One ensureMapped() call for the whole potential span rather than one
    // per byte -- much cheaper, and correctness is unaffected since
    // ensureMapped() is a no-op past whatever's already mapped.
    dump.ensureMapped(engine, address, maxLen);
    std::string result;
    for (size_t i = 0; i < maxLen; ++i) {
        char c = 0;
        engine.memRead(address + i, &c, sizeof(c));
        if (c == 0) break;
        result.push_back(c);
    }
    return result;
}

struct EntityRow {
    uint32_t handle;
    uint16_t index;
    std::string tagName;
    Vec3 pos;
};

void writeFile(const std::filesystem::path& path, const std::string& contents) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << contents;
}

std::string readFileIfExists(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

}  // namespace

UNICORN_TEST(Dump_EntityList_HandlesTagNamesAndPositions_MatchesApproved) {
    using Engine::Entity;
    using Engine::EntityList;
    using Engine::EntityRecord;
    using Engine::Tag;

    // SHARED-SAFE: this test never executes emulated code (pure host-side
    // memRead()/ensureMapped() pointer-chasing) and never writes anything,
    // so sharing the fixture's engine is trivially safe -- and lets its
    // demand-paged pages (entity pool, tag array) be reused by other
    // shared tests that touch the same globals.
    auto& fixture = SharedDumpFixture::instance();
    MinidumpLoader& dump = fixture.dump();
    UnicornEngine& engine = fixture.engine();

    uint64_t dllBase = dump.moduleBase("halo1.dll");

    uint64_t entityListPtr = read<uint64_t>(engine, dump, dllBase + kEntityListPtrOffset);
    uint64_t entityArrayBase = read<uint64_t>(engine, dump, dllBase + kEntityArrayBaseOffset);
    uint64_t tagArrayPtr = read<uint64_t>(engine, dump, dllBase + kTagArrayPtrOffset);
    uint64_t relocatedMapBase = read<uint64_t>(engine, dump, dllBase + kRelocatedMapBaseOffset);
    uint64_t mapBase = read<uint64_t>(engine, dump, dllBase + kMapBaseOffset);

    if (entityListPtr == 0) {
        std::cout << "  entity list pointer is null in this capture -- was a map loaded?\n";
        return false;
    }

    EntityList entityList = read<EntityList>(engine, dump, entityListPtr);

    std::vector<EntityRow> rows;
    for (uint16_t i = 0; i < entityList.capacity; ++i) {
        uint64_t recordAddr = entityListPtr + entityList.entityListOffset + i * sizeof(EntityRecord);
        EntityRecord record = read<EntityRecord>(engine, dump, recordAddr);
        // entityArrayOffset == -1 marks a slot that was used and then freed
        // (see entity_list.cpp's getEntityPointer()). But slots beyond the
        // high-water mark that have *never* been allocated at all are just
        // zero-initialized memory: entityArrayOffset reads back as 0 (not
        // -1), which would otherwise alias every such slot onto whatever's
        // sitting at entityArrayBase+0x34+0. id (the salt, see
        // EntityRecord::id's doc comment) starts counting from a nonzero
        // value and is never actually assigned 0 by the live game, so a
        // zero id reliably means "never touched" here too.
        if (record.entityArrayOffset == -1 || record.id == 0) continue;

        uint64_t entityAddr = entityArrayBase + kEntityArrayBaseAdjust + static_cast<int64_t>(record.entityArrayOffset);
        Entity entity = read<Entity>(engine, dump, entityAddr);

        EntityRow row{};
        row.handle = (static_cast<uint32_t>(record.id) << 16) | i;
        row.index = i;
        row.pos = entity.pos;

        if (entity.tagID == NULL_HANDLE) {
            row.tagName = "(none)";
        } else {
            uint64_t tagAddr = tagArrayPtr + (entity.tagID & 0xFFFF) * sizeof(Tag);
            Tag tag = read<Tag>(engine, dump, tagAddr);
            uint64_t resourcePathAbs = tag.resourcePathAddress + (relocatedMapBase - mapBase);
            row.tagName = readCString(engine, dump, resourcePathAbs);
        }

        rows.push_back(std::move(row));
    }

    std::sort(rows.begin(), rows.end(),
        [](const EntityRow& a, const EntityRow& b) { return a.index < b.index; });

    std::ostringstream out;
    out << rows.size() << " entities\n";
    char line[512];
    for (auto& row : rows) {
        std::snprintf(line, sizeof(line), "0x%08X  %-48s  (% .3f, % .3f, % .3f)\n",
            row.handle, row.tagName.c_str(), row.pos.x, row.pos.y, row.pos.z);
        out << line;
    }

    std::filesystem::path approvedDir =
        std::filesystem::path(SharedDumpFixture::kDumpPath).parent_path().parent_path() / "approved";
    std::filesystem::path approvedPath = approvedDir / "EntityList.approved.txt";
    std::filesystem::path receivedPath = approvedDir / "EntityList.received.txt";

    std::string actual = out.str();
    writeFile(receivedPath, actual);

    std::string approved = readFileIfExists(approvedPath);
    if (approved.empty()) {
        std::cout << "  no approved file yet: " << approvedPath.string() << "\n";
        std::cout << "  wrote " << receivedPath.string() << " -- review it, then rename it to .approved.txt\n";
        return false;
    }

    bool ok = (actual == approved);
    if (!ok) {
        std::cout << "  mismatch vs " << approvedPath.string() << "\n";
        std::cout << "  see " << receivedPath.string() << " for actual output\n";
    }
    return ok;
}
