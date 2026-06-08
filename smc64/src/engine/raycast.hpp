#pragma once

#include "math/Vectors.hpp"
#include <cstdint>

#define ENGINE_RAYCAST_PROJECTILE_FLAGS 0x1000e9

namespace Engine {
    
    typedef unsigned char undefined;

    typedef enum HitType: uint16_t {
        HitType_Map=0x002,
        HitType_Entity=0x003,
        HitType_Nothing=0xFFFF
    } HitType;
    static_assert(sizeof(HitType) == 2, "HitType size mismatch");

    struct RaycastResult {
        enum HitType hitType;
        char padding_0x02[10];
        uint16_t field11_0xc;
        char padding_0x0e[10];
        struct Vec3 pos;
        struct Vec3 normal;
        char padding_0x30[4];
        uint16_t field28_0x34;
        char padding_0x36[2];
        uint32_t entityHandle;
        char padding_0x3c[20]; // This has been confirmed to exist via Ghidra auto-fill.

        char safetyPadding[0x10]; // This has not been confirmed to exist. For safety, we allocate a little extra.
    };
    static_assert(sizeof(RaycastResult) == 0x50 + 0x10, "RaycastResult size mismatch");

    void raycast(uint64_t flags, Vec3 *origin, Vec3 *displacement, uint32_t sourceEntityHandle, RaycastResult *raycastResultOut);

    void raycastPlayerCrosshair(RaycastResult *raycastResultOut, float maxDistance = 100.0f, uint64_t flags = ENGINE_RAYCAST_PROJECTILE_FLAGS);
}
