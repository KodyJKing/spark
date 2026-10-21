#include "math/Vectors.hpp"
#include "engine/math.hpp"
#include "engine/common.hpp"
#include "engine/map.hpp"
#include "types/CollisionQueryObject.hpp"
#include "types/CollisionData.hpp"
#include "types/EntityCollisionData.hpp"

// Ghidra: _lineTestVsEntityModel (offset 0xB91C0C). Handler for collision-query type 3
// (line test against an entity's collision model, dispatched by an as-yet-unidentified
// function pointer table - see reversing/notes/LineTestSystem.md open questions).
//
// getEntityCollisionData (0xC47798) and _lineTestVsEntityCollisionRegions (0xC47940) are
// understood well enough to call directly (see decomp.md's scope-selection rule) but have
// not been hand-decompiled themselves yet, so they're called here as raw function pointers
// into the real binary rather than reimplemented.
//
// CONFIDENCE NOTE: everything up to and including the CollisionData output fields is
// confirmed - see LineTestSystem.md's "Dynamic Validation" section (a real end-to-end call
// via smc64-dlltest-unicorn). The `RegionHitOutput` layout below (the buffer passed to
// _lineTestVsEntityCollisionRegions) is a fresh reconstruction from Ghidra's stack offsets
// that has NOT been independently dynamically validated - in particular `planeLocal` (the
// hit-plane pointer) is inferred rather than confirmed. Before installing this as a live
// hook, consider extending LineTestVsEntityModelTest.cpp to call this decomp function
// alongside the real one (same synthetic inputs) and diff every CollisionData field.
namespace {

    using GetEntityCollisionData_t = bool(*)(Engine::Decomp::EntityCollisionData* out, Engine::EntityHandle entityHandle);
    using LineTestVsEntityCollisionRegions_t = bool(*)(
        Engine::Decomp::EntityCollisionData* collisionData, uint32_t contextFlag,
        Vec3* rayEndWorld, Vec3* negDirectionWorld, int16_t* outRegionHit);

    constexpr uintptr_t kGetEntityCollisionDataOffset = 0xC47798;
    constexpr uintptr_t kLineTestVsEntityCollisionRegionsOffset = 0xC47940;

    GetEntityCollisionData_t getEntityCollisionData() {
        return (GetEntityCollisionData_t)(Engine::dllBase() + kGetEntityCollisionDataOffset);
    }

    LineTestVsEntityCollisionRegions_t lineTestVsEntityCollisionRegions() {
        return (LineTestVsEntityCollisionRegions_t)(Engine::dllBase() + kLineTestVsEntityCollisionRegionsOffset);
    }

    // Ghidra: rotateVec (offset 0xBA3004). Rotates a direction vector by tr's basis columns -
    // no scale applied. Distinct from transformVec (0xBA2F5C, see ../transformVec.hpp) which
    // *does* apply tr's scale first. Small/simple enough not to need its own HOOK entry (see
    // decomp.md), but reimplemented fresh here (not called via function pointer) since it's
    // only used as a private helper of transformVec4AsPlane below.
    // Takes Transform/Vec3/Vec4 by value (not const&) because Vec3/Vec4's operator+/*
    // overloads aren't const-qualified.
    Vec3 rotateVec(Engine::Transform tr, Vec3 vecIn) {
        return tr.x * vecIn.x + tr.y * vecIn.y + tr.z * vecIn.z;
    }

    // Ghidra: transformVec4AsPlane (offset 0xBA3090). Transforms a plane (normal + distance)
    // from bone-local space to world space: rotate the normal, then recompute the distance
    // term from the transform's translation and scale.
    Vec4 transformVec4AsPlane(Engine::Transform tr, Vec4 planeIn) {
        Vec3 normal = rotateVec(tr, Vec3{ planeIn.x, planeIn.y, planeIn.z });
        float w = tr.pos.x * normal.x + tr.pos.y * normal.y + tr.pos.z * normal.z + planeIn.w * tr.w;
        return Vec4{ normal.x, normal.y, normal.z, w };
    }

    // Output buffer shared with _lineTestVsEntityCollisionRegions and (deeper still)
    // _lineTestVsBSP/FUN_7ff9d8064778 - see LineTestSystem.md for the byte-offset reasoning.
    // Only [0]-[2] and the bestT float are documented there; the fields past bestT
    // (planeLocal onward) are inferred here from Ghidra's stack-slot addresses lining up
    // exactly with this struct's natural layout - MEDIUM confidence, not yet dynamically
    // isolated from the rest of the call chain.
    struct RegionHitOutput {
        int16_t boneIndex = -1;         // 0x00
        int16_t permutationIndex = 0;   // 0x02
        int16_t subRegionIndex = 0;     // 0x04
        int16_t _pad06 = 0;             // 0x06
        float bestT = 3.4028235e38f;    // 0x08 - FLT_MAX ("no hit yet"); matches Ghidra's
                                         //        outRegionHit[4]=-1 / outRegionHit[5]=0x7f7f
                                         //        (0x7F7FFFFF bit-reinterpreted as float)
        Vec4* planeLocal = nullptr;     // 0x10 - [UNVERIFIED] hit plane, bone-local space
        int32_t _unk44 = 0;             // 0x18 - [UNVERIFIED] semantics unknown
        int32_t backfaceSign = 0;       // 0x1c - plane is back-facing the ray when < 0
        uint8_t flags0 = 0;             // 0x20 - [UNVERIFIED]
        uint8_t flags1 = 0;             // 0x21 - [UNVERIFIED]
        int16_t surfaceIndex = -1;      // 0x22 - surface index within the BSP surface list
    };

    static_assert(offsetof(RegionHitOutput, bestT) == 0x08);
    static_assert(offsetof(RegionHitOutput, planeLocal) == 0x10);
    static_assert(offsetof(RegionHitOutput, backfaceSign) == 0x1c);
    static_assert(offsetof(RegionHitOutput, surfaceIndex) == 0x22);

}

void _lineTestVsEntityModel(
    Engine::Decomp::CollisionQueryObject* query,
    Vec3* rayOrigin,
    Vec3* rayDirection,
    Engine::Decomp::CollisionData* result) {

    result->resultType = 0xFFFF; // miss, until proven otherwise
    result->fraction = 3.4028235e38f; // FLT_MAX

    if (query->queryType != 3) return; // this handler only understands entity-model queries

    Vec3 rayEndWorld = *rayOrigin + *rayDirection;
    Vec3 negDirectionWorld = *rayDirection * -1.0f;

    Engine::Decomp::EntityCollisionData collisionData;
    if (!getEntityCollisionData()(&collisionData, query->entityHandle)) return;

    RegionHitOutput regionHit{};
    if (!lineTestVsEntityCollisionRegions()(&collisionData, 1, &rayEndWorld, &negDirectionWorld, (int16_t*)&regionHit)) return;

    result->fraction = 1.0f - regionHit.bestT; // _lineTestVsBSP's t is inverse-parametric (see LineTestSystem.md)
    result->resultType = 3;

    // [UNVERIFIED] see RegionHitOutput::planeLocal above.
    Vec4 plane = transformVec4AsPlane(collisionData.boneTransforms[regionHit.boneIndex], *regionHit.planeLocal);
    if (regionHit.backfaceSign < 0) plane = plane * -1.0f;
    result->plane = plane;

    result->materialType = 0xFFFF;
    if (regionHit.surfaceIndex != -1) {
        // Engine::CollisionTagData+0x238: material table offset (BlockPointer-like: 0 = none,
        // else a map-relative offset). Not yet a named field on Engine::CollisionTagData -
        // flag for review before promoting. Ghidra guards the zero case explicitly (leaves the
        // pointer null) rather than relocating offset 0, so that guard is reproduced here
        // instead of calling Engine::translateMapAddress() unconditionally.
        uint32_t materialTableOffset = *(uint32_t*)((char*)collisionData.collisionTagData + 0x238);
        uint64_t materialTablePtr = materialTableOffset != 0 ? Engine::translateMapAddress(materialTableOffset) : 0;
        result->materialType = *(int16_t*)(materialTablePtr + 0x24 + regionHit.surfaceIndex * 0x48);
    }

    result->entityHandle = query->entityHandle;
    result->permutationIndex = regionHit.permutationIndex;
    result->boneIndex = regionHit.boneIndex;
    result->subRegionIndex = regionHit.subRegionIndex;
    result->_unk44 = regionHit._unk44;
    result->flags0 = regionHit.flags0;
    result->flags1 = regionHit.flags1;
    result->_backfaceSign = regionHit.backfaceSign;
    result->surfaceIndex = regionHit.surfaceIndex;

    result->hitPointX = result->fraction * rayDirection->x + rayOrigin->x;
    result->hitPointY = result->fraction * rayDirection->y + rayOrigin->y;
    result->hitPointZ = result->fraction * rayDirection->z + rayOrigin->z;
}
