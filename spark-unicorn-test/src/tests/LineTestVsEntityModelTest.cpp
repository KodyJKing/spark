#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>

#include "TestRunner.hpp"
#include "UnicornEngine.hpp"
#include "Win64Call.hpp"
#include "MinidumpLoader.hpp"
#include "SharedDumpFixture.hpp"

// First test to actually *execute* real halo1.dll code against globals
// (entity pool, tag array, map/BSP data) rather than just pointer-chasing
// dump-backed memory -- see reversing/notes/LineTestSystem.md for the full
// analysis this is based on. Casts a ray straight through a known live
// entity's own position and checks that _lineTestVsEntityModel reports a
// hit against that same entity.
//
// Every global this call chain touches (entity pool/record pool, tag array,
// DAT_MapBase/DAT_RelocatedMapBase-relative BSP/collision data) was
// confirmed via Ghidra to be reachable purely through those same globals
// EntityListDumpTest.cpp already reads successfully -- so installing
// demand paging (rather than manually loading specific ranges) should be
// sufficient here.
namespace {

constexpr uintptr_t kLineTestVsEntityModelOffset = 0xB91C0C;

// The entity to test against: the player's own cyborg biped, confirmed
// present in approved/EntityList.approved.txt at this handle/position.
constexpr uint32_t kPlayerEntityHandle = 0xE64503D6;
constexpr float kPlayerX = 23.066f;
constexpr float kPlayerY = -17.000f;
constexpr float kPlayerZ = 89.216f;

struct Vec3f {
    float x, y, z;
};

// Only offsets 0x0 (query type) and 0x38 (entity handle) are actually read
// by _lineTestVsEntityModel per the current decompile -- the rest of this
// buffer is zero-filled padding, sized with margin past the last known
// field in case something else nearby gets touched.
#pragma pack(push, 1)
struct RawCollisionQuery {
    int16_t queryType;         // 0x00 -- must be 3 (entity model)
    uint8_t pad_0x02[0x36];    // 0x02..0x38
    uint32_t entityHandle;     // 0x38
    uint8_t pad_0x3c[8];       // trailing safety margin
};
#pragma pack(pop)
static_assert(offsetof(RawCollisionQuery, entityHandle) == 0x38);

// Layout per reversing/notes/LineTestSystem.md's "CollisionResult Output
// Layout" table -- several fields there are marked [UNVERIFIED]. This test
// is itself the first dynamic check of that table: a plausible hit point
// close to the known entity position is good evidence the guessed offsets
// are right.
#pragma pack(push, 1)
struct RawCollisionResult {
    int16_t resultType;        // 0x00 -- 0xFFFF = miss, 3 = entity model hit
    uint8_t pad_0x02[0x12];    // 0x02..0x14
    float fraction;            // 0x14
    float hitX, hitY, hitZ;    // 0x18, 0x1c, 0x20
    float planeX, planeY, planeZ, planeW;  // 0x24..0x34 (Vec4)
    int16_t materialType;      // 0x34
    uint8_t pad_0x36[2];       // 0x36..0x38
    uint32_t entityHandle;     // 0x38
    int16_t unk_0x3c;          // 0x3c -- [UNVERIFIED]
    int16_t boneIndex;         // 0x3e
    int16_t unk_0x40;          // 0x40 -- [UNVERIFIED]
    uint8_t pad_0x42[2];       // 0x42..0x44
    uint32_t unk_0x44;         // 0x44 -- [UNVERIFIED]
    int32_t signFlip;          // 0x48 -- local_46c, [UNVERIFIED]
    uint8_t flags_0x4c;        // 0x4c
    uint8_t flags_0x4d;        // 0x4d
    int16_t surfaceIndex;      // 0x4e
};
#pragma pack(pop)
static_assert(offsetof(RawCollisionResult, fraction) == 0x14);
static_assert(offsetof(RawCollisionResult, planeX) == 0x24);
static_assert(offsetof(RawCollisionResult, materialType) == 0x34);
static_assert(offsetof(RawCollisionResult, entityHandle) == 0x38);
static_assert(offsetof(RawCollisionResult, surfaceIndex) == 0x4e);
static_assert(sizeof(RawCollisionResult) == 0x50);

}  // namespace

// SHARED-SAFE: confirmed via Ghidra (2026-07-19) -- the entire call chain
// (getEntityCollisionData -> getTagDataPointer/getBoneTransforms ->
// _lineTestVsEntityCollisionRegions -> invertTransform/transformPoint/
// transformVec -> _lineTestVsBSP -> the recursive BSP walk ->
// transformVec4AsPlane) only reads the entity pool/tag array/BSP data and
// writes exclusively to param_4 (this call's own output buffer) -- never
// to global/pool memory. See reversing/notes/LineTestSystem.md "Dynamic
// Validation" for the full analysis.
UNICORN_TEST(Dump_LineTestVsEntityModel_RayThroughPlayer_HitsPlayerEntity) {
    auto& fixture = SharedDumpFixture::instance();
    MinidumpLoader& dump = fixture.dump();
    UnicornEngine& engine = fixture.engine();

    uint64_t halo1Base = dump.moduleBase("halo1.dll");
    uint64_t funcAddr = halo1Base + kLineTestVsEntityModelOffset;

    uint64_t scratchBase = fixture.allocateScratchSlot();
    uint64_t stackTop = scratchBase + 0x10000;
    uint64_t sentinel = scratchBase + 0x11000;
    uint64_t argsBase = scratchBase + 0x12000;

    uint64_t queryAddr = argsBase;
    uint64_t originAddr = argsBase + 0x100;
    uint64_t dirAddr = argsBase + 0x200;
    uint64_t resultAddr = argsBase + 0x300;

    RawCollisionQuery query{};
    query.queryType = 3;
    query.entityHandle = kPlayerEntityHandle;
    engine.memWrite(queryAddr, &query, sizeof(query));

    // A vertical ray straddling the entity's recorded position by 3 units
    // in each direction -- generous enough to intersect a biped-sized
    // collision model regardless of whether the position convention is
    // feet or center-of-mass.
    Vec3f origin{ kPlayerX, kPlayerY, kPlayerZ + 3.0f };
    Vec3f direction{ 0.0f, 0.0f, -6.0f };
    engine.memWrite(originAddr, &origin, sizeof(origin));
    engine.memWrite(dirAddr, &direction, sizeof(direction));

    RawCollisionResult result{};
    engine.memWrite(resultAddr, &result, sizeof(result));

    Win64Call call{ engine, stackTop, sentinel };
    call.call(funcAddr, queryAddr, originAddr, dirAddr, resultAddr);

    engine.memRead(resultAddr, &result, sizeof(result));

    std::cout << "  halo1.dll base in dump: 0x" << std::hex << halo1Base << std::dec << "\n";
    std::cout << "  ray origin (" << origin.x << ", " << origin.y << ", " << origin.z
              << ") dir (" << direction.x << ", " << direction.y << ", " << direction.z << ")\n";
    std::cout << "  resultType = " << result.resultType
               << " (0xFFFF = miss, 3 = entity model hit)\n";

    if (result.resultType != 3) {
        std::cout << "  no hit -- ray likely didn't intersect the entity's collision model\n";
        return false;
    }

    std::cout << "  fraction = " << result.fraction << "\n";
    std::cout << "  hit point (" << result.hitX << ", " << result.hitY << ", " << result.hitZ << ")\n";
    std::cout << "  plane (" << result.planeX << ", " << result.planeY << ", " << result.planeZ
               << ", " << result.planeW << ")\n";
    std::cout << "  materialType = " << result.materialType << "\n";
    std::cout << "  entityHandle = 0x" << std::hex << result.entityHandle << std::dec << "\n";
    std::cout << "  boneIndex = " << result.boneIndex << "\n";
    std::cout << "  surfaceIndex = " << result.surfaceIndex << "\n";
    std::cout << "  unk_0x3c=" << result.unk_0x3c << " unk_0x40=" << result.unk_0x40
               << " unk_0x44=" << result.unk_0x44 << " signFlip=" << result.signFlip
               << " flags=(" << (int)result.flags_0x4c << "," << (int)result.flags_0x4d << ")\n";

    return result.entityHandle == kPlayerEntityHandle;
}
